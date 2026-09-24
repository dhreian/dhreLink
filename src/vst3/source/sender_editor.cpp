#include "sender_editor.hpp"

#include "dhrelink/shared_audio_stream.hpp"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "public.sdk/source/common/pluginview.h"
#include "../resources/resource.h"
#include "../../../assets/ui/brand_theme.hpp"

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

constexpr int kViewWidth = 420;
constexpr int kViewHeight = 180;
constexpr UINT_PTR kRefreshTimer = 1;

using namespace dhre::ui;

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

class SenderEditor final :
  public Steinberg::CPluginView,
  public Steinberg::IPlugViewContentScaleSupport {
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

  Steinberg::tresult PLUGIN_API queryInterface(
    const Steinberg::TUID iid,
    void** object) override {
    QUERY_INTERFACE(
      iid,
      object,
      Steinberg::IPlugViewContentScaleSupport::iid,
      Steinberg::IPlugViewContentScaleSupport)
    return Steinberg::CPluginView::queryInterface(iid, object);
  }

  DELEGATE_REFCOUNT(Steinberg::CPluginView)

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
      static_cast<int>(getRect().getWidth()),
      static_cast<int>(getRect().getHeight()),
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

  Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override {
    if (!std::isfinite(factor) || factor <= 0.0F) {
      return Steinberg::kInvalidArgument;
    }
    if (std::abs(contentScaleFactor_ - factor) < 0.001F) {
      return Steinberg::kResultTrue;
    }

    contentScaleFactor_ = factor;
    Steinberg::ViewRect scaledRect(
      0,
      0,
      static_cast<Steinberg::int32>(std::lround(kViewWidth * factor)),
      static_cast<Steinberg::int32>(std::lround(kViewHeight * factor)));
    setRect(scaledRect);

    if (plugFrame != nullptr) {
      const auto resizeResult = plugFrame->resizeView(this, &scaledRect);
      static_cast<void>(resizeResult);
    } else if (window_ != nullptr) {
      MoveWindow(
        window_,
        0,
        0,
        static_cast<int>(scaledRect.getWidth()),
        static_cast<int>(scaledRect.getHeight()),
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
    windowClass.hIcon = LoadIconW(moduleInstance(), MAKEINTRESOURCEW(IDI_DHRELINK_ICON));
    windowClass.hIconSm = windowClass.hIcon;
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
      case WM_PRINTCLIENT:
        if (wParam != 0) {
          editor->paintToContext(reinterpret_cast<HDC>(wParam));
        }
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
    montserratBoldResource_ = addEmbeddedFont(IDR_FONT_MONTSERRAT_BOLD);
    titleFont_ = CreateFontW(
      -kTitlePixels, 0, 0, 0, kRegularWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, kTitleFamily);
    bodyFont_ = CreateFontW(
      -kBodyPixels, 0, 0, 0, kStrongWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, kBodyFamily);
    smallFont_ = CreateFontW(
      -kSmallPixels, 0, 0, 0, kRegularWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, kBodyFamily);
    creditFont_ = CreateFontW(
      -kCreditPixels, 0, 0, 0, kRegularWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, kBodyFamily);
  }

  void releaseFonts() noexcept {
    for (const auto font : {titleFont_, bodyFont_, smallFont_, creditFont_}) {
      if (font != nullptr) DeleteObject(font);
    }
    titleFont_ = nullptr;
    bodyFont_ = nullptr;
    smallFont_ = nullptr;
    creditFont_ = nullptr;
    if (quintessentialResource_ != nullptr) {
      RemoveFontMemResourceEx(quintessentialResource_);
      quintessentialResource_ = nullptr;
    }
    if (montserratResource_ != nullptr) {
      RemoveFontMemResourceEx(montserratResource_);
      montserratResource_ = nullptr;
    }
    if (montserratBoldResource_ != nullptr) {
      RemoveFontMemResourceEx(montserratBoldResource_);
      montserratBoldResource_ = nullptr;
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
    const auto icon = LoadIconW(moduleInstance(), MAKEINTRESOURCEW(IDI_DHRELINK_ICON));
    if (icon != nullptr) {
      DrawIconEx(context, kSpace, kHeaderInset, icon, kHeaderIconSize, kHeaderIconSize, 0, nullptr, DI_NORMAL);
    }
  }

  void paintMeter(HDC context, int y, wchar_t channel, float peak, bool active) const {
    wchar_t label[] = {channel, L'\0'};
    drawText(context, smallFont_, kMuted, label, RECT{kSpace, y - 8, 30, y + 8});
    const RECT track{36, y - 3, kViewWidth - kSpace, y + 3};
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
    fill(context, RECT{0, 0, bounds.right, kHeaderHeight}, kSurface);
    line(context, 0, kHeaderHeight - kHairline, bounds.right, kHeaderHeight - kHairline, kLine);
    paintLogo(context);
    drawText(context, titleFont_, kWhite, L"dhreLink",
      RECT{kSpace + kHeaderIconSize + kCompactSpace, 0, 190, kHeaderHeight - kHairline});

    const auto connected = status_.receiverConnected;
    const auto statusColor = connected ? kConnected : kPurple;
    const auto statusText = connected ? L"Connected" : L"Waiting for OBS";
    SIZE statusSize{};
    const auto previousStatusFont = SelectObject(context, smallFont_);
    GetTextExtentPoint32W(
      context, statusText, static_cast<int>(std::wcslen(statusText)), &statusSize);
    SelectObject(context, previousStatusFont);
    const auto statusTextRight = bounds.right - kSpace;
    const auto statusTextLeft = statusTextRight - statusSize.cx;
    const auto statusLeft = statusTextLeft - 13;
    const auto statusBrush = CreateSolidBrush(statusColor);
    const auto previousBrush = SelectObject(context, statusBrush);
    const auto previousPen = SelectObject(context, GetStockObject(NULL_PEN));
    Ellipse(context, statusLeft, 19, statusLeft + 7, 26);
    SelectObject(context, previousPen);
    SelectObject(context, previousBrush);
    DeleteObject(statusBrush);
    drawText(
      context, smallFont_, connected ? kWhite : kMuted, statusText,
      RECT{statusTextLeft, 0, statusTextRight, kHeaderHeight - kHairline});

    const RECT dawBox{kSpace, kHeaderHeight + kMediumSpace, 74, 86};
    const RECT obsBox{346, kHeaderHeight + kMediumSpace, kViewWidth - kSpace, 86};
    fill(context, dawBox, kSurfaceRaised);
    fill(context, obsBox, kSurfaceRaised);
    drawText(context, bodyFont_, kWhite, L"DAW", dawBox, DT_CENTER | DT_VCENTER);
    drawText(context, bodyFont_, kWhite, L"OBS", obsBox, DT_CENTER | DT_VCENTER);
    line(context, 84, 73, 336, 73, connected ? kPurple : kLine, 2);
    line(context, 329, 67, 336, 73, connected ? kPurple : kLine, 2);
    line(context, 329, 79, 336, 73, connected ? kPurple : kLine, 2);

    paintMeter(context, 104, L'L', status_.peakLeft, status_.senderConnected);
    paintMeter(context, 124, L'R', status_.peakRight, status_.senderConnected);

    constexpr auto footerTop = kViewHeight - kFooterHeight;
    fill(context, RECT{0, footerTop, bounds.right, kViewHeight}, kSurface);
    line(context, 0, footerTop, bounds.right, footerTop, kLine);
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
    drawText(context, creditFont_, kMuted, format,
      RECT{kSpace, footerTop + kHairline, 180, kViewHeight});

    drawText(
      context,
      creditFont_,
      kPurple,
      L"DEVELOPED BY DHREIAN",
      RECT{180, footerTop + kHairline, bounds.right - kSpace, kViewHeight},
      DT_RIGHT | DT_VCENTER);
  }

  void paintToContext(HDC context) const {
    RECT bounds{};
    GetClientRect(window_, &bounds);
    if (context == nullptr || bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
      return;
    }
    const auto buffer = CreateCompatibleDC(context);
    const auto bitmap = CreateCompatibleBitmap(
      context, bounds.right - bounds.left, bounds.bottom - bounds.top);
    const auto previousBitmap = SelectObject(buffer, bitmap);
    SetMapMode(buffer, MM_ANISOTROPIC);
    SetWindowExtEx(buffer, kViewWidth, kViewHeight, nullptr);
    SetViewportExtEx(
      buffer,
      bounds.right - bounds.left,
      bounds.bottom - bounds.top,
      nullptr);
    paintContent(buffer, RECT{0, 0, kViewWidth, kViewHeight});
    SetMapMode(buffer, MM_TEXT);
    BitBlt(
      context, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
      buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
  }

  void paint() const {
    PAINTSTRUCT paintState{};
    const auto context = BeginPaint(window_, &paintState);
    paintToContext(context);
    EndPaint(window_, &paintState);
  }

  HWND window_ = nullptr;
  HANDLE quintessentialResource_ = nullptr;
  HANDLE montserratResource_ = nullptr;
  HANDLE montserratBoldResource_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT bodyFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  HFONT creditFont_ = nullptr;
  ScaleFactor contentScaleFactor_ = 1.0F;
  StreamStatusReader statusReader_;
  StreamStatus status_{};
};

} // namespace

Steinberg::IPlugView* createSenderEditor() {
  return new SenderEditor();
}

} // namespace dhrelink::vst3
