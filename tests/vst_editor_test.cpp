#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/hosting/module.h"

#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

constexpr int kExpectedWidth = 360;
constexpr int kExpectedHeight = 152;

bool succeeded(Steinberg::tresult result) noexcept {
  return result == Steinberg::kResultTrue || result == Steinberg::kResultOk;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Expected the path to the dhreLink VST3 bundle.\n";
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

  const auto parent = CreateWindowExW(
    0, L"STATIC", L"dhreLink editor test", WS_OVERLAPPED,
    0, 0, kExpectedWidth, kExpectedHeight,
    nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (parent == nullptr) {
    view->release();
    controller->terminate();
    std::cerr << "Could not create the test host window.\n";
    return 6;
  }

  if (!succeeded(view->attached(parent, Steinberg::kPlatformTypeHWND))) {
    DestroyWindow(parent);
    view->release();
    controller->terminate();
    std::cerr << "The editor could not attach to a Win32 host window.\n";
    return 7;
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

  const auto removed = view->removed();
  const auto childDestroyed = GetWindow(parent, GW_CHILD) == nullptr;
  DestroyWindow(parent);
  view->release();
  controller->terminate();
  controller = nullptr;
  module.reset();

  if (!correctClass || !childDestroyed || !succeeded(removed)) {
    std::cerr << "The editor window lifecycle check failed.\n";
    return 8;
  }

  std::cout << "dhreLink VST3 editor created at 360x152 and closed successfully.\n";
  return 0;
}
