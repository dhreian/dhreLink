#include "sender_editor.hpp"

#include "dhrelink/shared_audio_stream.hpp"
#include "public.sdk/source/common/pluginview.h"
#include "../resources/resource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <iterator>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace dhrelink::vst3 {
namespace {

constexpr int kViewWidth = 360;
constexpr int kViewHeight = 152;
constexpr UINT_PTR kRefreshTimer = 1;

constexpr COLORREF kBlack = RGB(10, 10, 12);
constexpr COLORREF kSurface = RGB(22, 22, 25);
constexpr COLORREF kSurfaceRaised = RGB(34, 34, 39);
constexpr COLORREF kLine = RGB(52, 52, 58);
constexpr COLORREF kWhite = RGB(255, 255, 255);
constexpr COLORREF kMuted = RGB(159, 159, 169);
constexpr COLORREF kPurple = RGB(138, 108, 255);
constexpr COLORREF kConnected = RGB(95, 227, 155);

HINSTANCE moduleInstance() noexcept {
  return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

void fill(HDC context, const RECT& rectangle, COLORREF color) {
  const auto brush = CreateSolidBrush(color);
  FillRect(context, &rectangle, brush);
  DeleteObject(brush);
}

void drawText(
  HDC context,
  HFONT font,
  COLORREF color,
  const wchar_t* text,
  RECT rectangle,
  UINT alignment = DT_LEFT | DT_VCENTER) {
  const auto previousFont = SelectObject(context, font);
  SetTextColor(context, color);
  SetBkMode(context, TRANSPARENT);
  DrawTextW(context, text, -1, &rectangle, alignment | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(context, previousFont);
}

void line(HDC context, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
  const auto pen = CreatePen(PS_SOLID, width, color);
  const auto previousPen = SelectObject(context, pen);
  MoveToEx(context, x1, y1, nullptr);
  LineTo(context, x2, y2);
  SelectObject(context, previousPen);
  DeleteObject(pen);
}

class SenderEditor final : public Steinberg::CPluginView {
public:
  SenderEditor() {
    setRect(Steinberg::ViewRect(0, 0, kViewWidth, kViewHeight));
  }

  ~SenderEditor() override {
    destroyWindow();
  }

  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override {
    return type != nullptr && std::strcmp(type, Steinberg::kPlatformTypeHWND) == 0
      ? Steinberg::kResultTrue
      : Steinberg::kResultFalse;
  }

  Steinberg::tresult PLUGIN_API attached(
    void* parent,
    Steinberg::FIDString type) override {
    if (
      parent == nullptr || window_ != nullptr ||
      isPlatformTypeSupported(type) != Steinberg::kResultTrue) {
      return Steinberg::kInvalidArgument;
    }

    if (!registerWindowClass()) return Steinberg::kResultFalse;
    systemWindow = parent;
    loadFonts();
    window_ = CreateWindowExW(
      0,
      windowClassName(),
      L"dhreLink",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
      0,
      0,
      kViewWidth,
      kViewHeight,
      static_cast<HWND>(parent),
      nullptr,
      moduleInstance(),
      this);
    if (window_ == nullptr) {
      systemWindow = nullptr;
      releaseFonts();
      return Steinberg::kResultFalse;
    }

    SetTimer(window_, kRefreshTimer, 50, nullptr);
    refresh();
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API removed() override {
    destroyWindow();
    systemWindow = nullptr;
    statusReader_.close();
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override {
    if (newSize == nullptr) return Steinberg::kInvalidArgument;
    setRect(*newSize);
    if (window_ != nullptr) {
      MoveWindow(
        window_, 0, 0,
        static_cast<int>(newSize->getWidth()),
        static_cast<int>(newSize->getHeight()),
        TRUE);
    }
    return Steinberg::kResultTrue;
  }

private:
  static const wchar_t* windowClassName() noexcept {
    return L"dhreLink.VST3.Editor.v1";
  }

  static bool registerWindowClass() noexcept {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = moduleInstance();
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = windowClassName();
    if (RegisterClassExW(&windowClass) != 0) return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }

  static LRESULT CALLBACK windowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    auto* editor = reinterpret_cast<SenderEditor*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
      editor = static_cast<SenderEditor*>(create->lpCreateParams);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor));
    }

    if (editor == nullptr) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
      case WM_ERASEBKGND:
        return 1;
      case WM_TIMER:
        if (wParam == kRefreshTimer) editor->refresh();
        return 0;
      case WM_PAINT:
        editor->paint();
        return 0;
      case WM_DESTROY:
        KillTimer(window, kRefreshTimer);
        return 0;
      default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
  }

  HANDLE addEmbeddedFont(int resourceId) noexcept {
    const auto resource = FindResourceW(
      moduleInstance(), MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (resource == nullptr) return nullptr;
    const auto loaded = LoadResource(moduleInstance(), resource);
    const auto* data = LockResource(loaded);
    const auto bytes = SizeofResource(moduleInstance(), resource);
    if (data == nullptr || bytes == 0) return nullptr;
    DWORD fontCount = 0;
    return AddFontMemResourceEx(
      const_cast<void*>(data), bytes, nullptr, &fontCount);
  }

  void loadFonts() noexcept {
    quintessentialResource_ = addEmbeddedFont(IDR_FONT_QUINTESSENTIAL);
    montserratResource_ = addEmbeddedFont(IDR_FONT_MONTSERRAT);
    titleFont_ = CreateFontW(
      -24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Quintessential");
    bodyFont_ = CreateFontW(
      -13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Montserrat");
    smallFont_ = CreateFontW(
      -11, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Montserrat");
    signatureFont_ = CreateFontW(
      -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Quintessential");
  }

  void releaseFonts() noexcept {
    for (const auto font : {titleFont_, bodyFont_, smallFont_, signatureFont_}) {
      if (font != nullptr) DeleteObject(font);
    }
    titleFont_ = nullptr;
    bodyFont_ = nullptr;
    smallFont_ = nullptr;
    signatureFont_ = nullptr;
    if (quintessentialResource_ != nullptr) {
      RemoveFontMemResourceEx(quintessentialResource_);
      quintessentialResource_ = nullptr;
    }
    if (montserratResource_ != nullptr) {
      RemoveFontMemResourceEx(montserratResource_);
      montserratResource_ = nullptr;
    }
  }

  void destroyWindow() noexcept {
    if (window_ != nullptr) {
      DestroyWindow(window_);
      window_ = nullptr;
    }
    releaseFonts();
  }

  void refresh() noexcept {
    StreamStatus latest;
    if (!statusReader_.isOpen()) {
      const auto opened = statusReader_.open("main-mix");
      static_cast<void>(opened);
    }
    if (statusReader_.isOpen() && !statusReader_.snapshot(latest)) {
      statusReader_.close();
    }
    status_ = latest;
    if (window_ != nullptr) InvalidateRect(window_, nullptr, FALSE);
  }

  void paintLogo(HDC context) const {
    const auto linkPen = CreatePen(PS_SOLID, 3, kPurple);
    const auto previousPen = SelectObject(context, linkPen);
    const auto previousBrush = SelectObject(context, GetStockObject(NULL_BRUSH));
    POINT leftLink[] = {
      {25, 11}, {22, 11}, {20, 11}, {18, 11},
      {8, 11}, {8, 29}, {18, 29},
      {20, 29}, {22, 29}, {25, 29}};
    POINT rightLink[] = {
      {28, 11}, {31, 11}, {33, 11}, {35, 11},
      {45, 11}, {45, 29}, {35, 29},
      {33, 29}, {31, 29}, {28, 29}};
    PolyBezier(context, leftLink, static_cast<DWORD>(std::size(leftLink)));
    PolyBezier(context, rightLink, static_cast<DWORD>(std::size(rightLink)));

    const auto wavePen = CreatePen(PS_SOLID, 2, kWhite);
    SelectObject(context, wavePen);
    POINT waveform[] = {
      {17, 20}, {22, 20}, {24, 16}, {28, 25}, {31, 20}, {36, 20}};
    Polyline(context, waveform, static_cast<int>(std::size(waveform)));

    SelectObject(context, previousPen);
    SelectObject(context, previousBrush);
    DeleteObject(wavePen);
    DeleteObject(linkPen);
  }

  void paintMeter(HDC context, int y, wchar_t channel, float peak, bool active) const {
    wchar_t label[] = {channel, L'\0'};
    drawText(context, smallFont_, kMuted, label, RECT{14, y - 5, 28, y + 6});
    const RECT track{32, y - 3, 346, y + 3};
    fill(context, track, kSurfaceRaised);
    const auto normalized = std::sqrt(std::clamp(peak, 0.0F, 1.0F));
    const auto width = active
      ? static_cast<int>(normalized * static_cast<float>(track.right - track.left))
      : 0;
    if (width > 0) {
      fill(context, RECT{track.left, track.top, track.left + width, track.bottom}, kPurple);
    }
  }

  void paintContent(HDC context, const RECT& bounds) const {
    fill(context, bounds, kBlack);
    fill(context, RECT{0, 0, bounds.right, 39}, kSurface);
    line(context, 0, 38, bounds.right, 38, kLine);
    paintLogo(context);
    drawText(context, titleFont_, kWhite, L"dhreLink", RECT{47, 0, 165, 38});

    const auto connected = status_.receiverConnected;
    const auto statusColor = connected ? kConnected : kPurple;
    const auto statusText = connected ? L"Connected" : L"Waiting for OBS";
    const auto statusLeft = connected ? 277 : 249;
    const auto statusBrush = CreateSolidBrush(statusColor);
    const auto previousBrush = SelectObject(context, statusBrush);
    const auto previousPen = SelectObject(context, GetStockObject(NULL_PEN));
    Ellipse(context, statusLeft, 17, statusLeft + 7, 24);
    SelectObject(context, previousPen);
    SelectObject(context, previousBrush);
    DeleteObject(statusBrush);
    drawText(
      context, smallFont_, connected ? kWhite : kMuted, statusText,
      RECT{statusLeft + 12, 0, 352, 39});

    const RECT dawBox{14, 49, 64, 73};
    const RECT obsBox{296, 49, 346, 73};
    fill(context, dawBox, kSurfaceRaised);
    fill(context, obsBox, kSurfaceRaised);
    drawText(context, bodyFont_, kWhite, L"DAW", dawBox, DT_CENTER | DT_VCENTER);
    drawText(context, bodyFont_, kWhite, L"OBS", obsBox, DT_CENTER | DT_VCENTER);
    line(context, 72, 61, 288, 61, connected ? kPurple : kLine, 2);
    line(context, 282, 56, 288, 61, connected ? kPurple : kLine, 2);
    line(context, 282, 66, 288, 61, connected ? kPurple : kLine, 2);

    paintMeter(context, 87, L'L', status_.peakLeft, status_.senderConnected);
    paintMeter(context, 104, L'R', status_.peakRight, status_.senderConnected);

    line(context, 0, 120, bounds.right, 120, kLine);
    wchar_t format[48] = L"-- kHz  |  -- ch";
    if (status_.sampleRate > 0 && status_.channels > 0) {
      if (status_.sampleRate % 1'000U == 0) {
        swprintf_s(
          format, L"%u kHz  |  %u ch",
          status_.sampleRate / 1'000U, status_.channels);
      } else {
        swprintf_s(
          format, L"%.1f kHz  |  %u ch",
          static_cast<double>(status_.sampleRate) / 1'000.0, status_.channels);
      }
    }
    drawText(context, smallFont_, kMuted, format, RECT{14, 121, 150, 152});

    constexpr auto credit = L"Developed by";
    SIZE creditSize{};
    const auto previousFont = SelectObject(context, smallFont_);
    GetTextExtentPoint32W(context, credit, static_cast<int>(std::wcslen(credit)), &creditSize);
    SelectObject(context, previousFont);
    constexpr int signatureWidth = 48;
    const auto creditLeft = bounds.right - 14 - signatureWidth - 7 - creditSize.cx;
    drawText(
      context, smallFont_, kMuted, credit,
      RECT{creditLeft, 121, creditLeft + creditSize.cx, 152});
    drawText(
      context, signatureFont_, kWhite, L"dhreian",
      RECT{bounds.right - 14 - signatureWidth, 121, bounds.right - 14, 152});
  }

  void paint() const {
    PAINTSTRUCT paintState{};
    const auto context = BeginPaint(window_, &paintState);
    RECT bounds{};
    GetClientRect(window_, &bounds);
    const auto buffer = CreateCompatibleDC(context);
    const auto bitmap = CreateCompatibleBitmap(
      context, bounds.right - bounds.left, bounds.bottom - bounds.top);
    const auto previousBitmap = SelectObject(buffer, bitmap);
    paintContent(buffer, bounds);
    BitBlt(
      context, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
      buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(window_, &paintState);
  }

  HWND window_ = nullptr;
  HANDLE quintessentialResource_ = nullptr;
  HANDLE montserratResource_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT bodyFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  HFONT signatureFont_ = nullptr;
  StreamStatusReader statusReader_;
  StreamStatus status_{};
};

} // namespace

Steinberg::IPlugView* createSenderEditor() {
  return new SenderEditor();
}

} // namespace dhrelink::vst3
