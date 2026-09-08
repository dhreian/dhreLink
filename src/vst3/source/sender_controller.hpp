#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace dhrelink::vst3 {

class SenderController final : public Steinberg::Vst::EditController {
public:
  static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IEditController*>(new SenderController()); }
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
};

} // namespace dhrelink::vst3
