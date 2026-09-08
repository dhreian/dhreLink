#include "sender_controller.hpp"

#include "sender_editor.hpp"

#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <cstring>

namespace dhrelink::vst3 {

Steinberg::tresult PLUGIN_API SenderController::initialize(Steinberg::FUnknown* context) {
  const auto result = EditController::initialize(context);
  if (result != Steinberg::kResultOk) return result;

  // dhreLink uses one fixed local stream; the editor is informational only.
  return Steinberg::kResultOk;
}

Steinberg::IPlugView* PLUGIN_API SenderController::createView(Steinberg::FIDString name) {
  if (name != nullptr && std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
    return createSenderEditor();
  }
  return nullptr;
}

} // namespace dhrelink::vst3
