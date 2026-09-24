#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/hosting/module.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

constexpr int kExpectedWidth = 420;
constexpr int kExpectedHeight = 180;

bool succeeded(Steinberg::tresult result) noexcept {
  return result == Steinberg::kResultTrue || result == Steinberg::kResultOk;
}

bool saveWindowBitmap(HWND window, const std::filesystem::path& outputPath) {
  RECT bounds{};
  if (!GetClientRect(window, &bounds)) return false;
  const auto width = bounds.right - bounds.left;
  const auto height = bounds.bottom - bounds.top;
  if (width <= 0 || height <= 0) return false;

  BITMAPINFO bitmapInfo{};
  bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmapInfo.bmiHeader.biWidth = width;
  bitmapInfo.bmiHeader.biHeight = -height;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;
  bitmapInfo.bmiHeader.biSizeImage =
    static_cast<DWORD>(width * height * 4);

  void* pixels = nullptr;
  const auto bitmap = CreateDIBSection(
    nullptr, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
  const auto context = CreateCompatibleDC(nullptr);
  if (bitmap == nullptr || context == nullptr || pixels == nullptr) {
    if (bitmap != nullptr) DeleteObject(bitmap);
    if (context != nullptr) DeleteDC(context);
    return false;
  }

  const auto previousBitmap = SelectObject(context, bitmap);
  SendMessageW(
    window,
    WM_PRINTCLIENT,
    reinterpret_cast<WPARAM>(context),
    PRF_CLIENT);

  BITMAPFILEHEADER fileHeader{};
  fileHeader.bfType = 0x4D42;
  fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fileHeader.bfSize = fileHeader.bfOffBits + bitmapInfo.bmiHeader.biSizeImage;

  std::ofstream output(outputPath, std::ios::binary);
  if (output) {
    output.write(
      reinterpret_cast<const char*>(&fileHeader),
      sizeof(fileHeader));
    output.write(
      reinterpret_cast<const char*>(&bitmapInfo.bmiHeader),
      sizeof(bitmapInfo.bmiHeader));
    output.write(
      static_cast<const char*>(pixels),
      bitmapInfo.bmiHeader.biSizeImage);
  }

  SelectObject(context, previousBitmap);
  DeleteObject(bitmap);
  DeleteDC(context);
  return output.good();
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Expected the VST3 bundle and an optional BMP output path.\n";
    return 1;
  }

  std::string error;
  const auto bundlePath = std::filesystem::weakly_canonical(argv[1]).string();
  auto module = VST3::Hosting::Module::create(bundlePath, error);
  if (module == nullptr) {
    std::cerr << "Could not load the VST3 bundle: " << error << '\n';
    return 2;
  }

  Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
  for (const auto& classInfo : module->getFactory().classInfos()) {
    if (classInfo.category() == kVstComponentControllerClass) {
      controller =
        module->getFactory().createInstance<Steinberg::Vst::IEditController>(classInfo.ID());
      break;
    }
  }
  if (controller == nullptr || !succeeded(controller->initialize(nullptr))) {
    std::cerr << "Could not initialize the dhreLink editor controller.\n";
    return 3;
  }

  auto* view = controller->createView(Steinberg::Vst::ViewType::kEditor);
  if (view == nullptr) {
    controller->terminate();
    std::cerr << "The controller did not create an editor view.\n";
    return 4;
  }

  Steinberg::ViewRect size{};
  if (
    !succeeded(view->getSize(&size)) || size.getWidth() != kExpectedWidth ||
    size.getHeight() != kExpectedHeight ||
    !succeeded(view->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND))) {
    view->release();
    controller->terminate();
    std::cerr << "The editor reported an unexpected size or platform.\n";
    return 5;
  }

  Steinberg::IPlugViewContentScaleSupport* scaleSupport = nullptr;
  if (
    !succeeded(view->queryInterface(
      Steinberg::IPlugViewContentScaleSupport::iid,
      reinterpret_cast<void**>(&scaleSupport))) ||
    scaleSupport == nullptr ||
    !succeeded(scaleSupport->setContentScaleFactor(1.5F)) ||
    !succeeded(view->getSize(&size)) ||
    size.getWidth() != 630 ||
    size.getHeight() != 270) {
    if (scaleSupport != nullptr) scaleSupport->release();
    view->release();
    controller->terminate();
    std::cerr << "The editor did not apply the requested content scale factor.\n";
    return 6;
  }
  if (
    !succeeded(scaleSupport->setContentScaleFactor(1.0F)) ||
    !succeeded(view->getSize(&size)) ||
    size.getWidth() != kExpectedWidth ||
    size.getHeight() != kExpectedHeight) {
    scaleSupport->release();
    view->release();
    controller->terminate();
    std::cerr << "The editor did not return to its native dimensions.\n";
    return 7;
  }

  const auto parent = CreateWindowExW(
    0, L"STATIC", L"dhreLink editor test", WS_OVERLAPPED,
    0, 0, static_cast<int>(size.getWidth()), static_cast<int>(size.getHeight()),
    nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (parent == nullptr) {
    scaleSupport->release();
    view->release();
    controller->terminate();
    std::cerr << "Could not create the test host window.\n";
    return 8;
  }

  if (!succeeded(view->attached(parent, Steinberg::kPlatformTypeHWND))) {
    DestroyWindow(parent);
    scaleSupport->release();
    view->release();
    controller->terminate();
    std::cerr << "The editor could not attach to a Win32 host window.\n";
    return 9;
  }

  const auto child = GetWindow(parent, GW_CHILD);
  wchar_t className[64]{};
  const auto classLength = child != nullptr
    ? GetClassNameW(child, className, static_cast<int>(std::size(className)))
    : 0;
  const auto correctClass =
    classLength > 0 && std::wstring_view(className) == L"dhreLink.VST3.Editor.v1";
  if (child != nullptr) {
    RedrawWindow(child, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
  }
  const auto bitmapSaved =
    argc != 3 || (child != nullptr && saveWindowBitmap(child, argv[2]));

  const auto removed = view->removed();
  const auto childDestroyed = GetWindow(parent, GW_CHILD) == nullptr;
  DestroyWindow(parent);
  scaleSupport->release();
  view->release();
  controller->terminate();
  controller = nullptr;
  module.reset();

  if (!correctClass || !bitmapSaved || !childDestroyed || !succeeded(removed)) {
    std::cerr << "The editor window lifecycle check failed.\n";
    return 10;
  }

  std::cout << "dhreLink VST3 editor scaled and rendered at 420x180 successfully.\n";
  return 0;
}
