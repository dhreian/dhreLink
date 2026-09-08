#include "cids.hpp"
#include "sender_controller.hpp"
#include "sender_processor.hpp"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/main/pluginfactory.h"

#define DHRELINK_VERSION "1.0.0"

BEGIN_FACTORY_DEF("dhreian", "https://dhre.link", "mailto:support@dhre.link")

DEF_CLASS2(
  INLINE_UID_FROM_FUID(dhrelink::vst3::kSenderProcessorId),
  Steinberg::PClassInfo::kManyInstances,
  kVstAudioEffectClass,
  "dhreLink Sender",
  Steinberg::Vst::kDistributable,
  Steinberg::Vst::PlugType::kFx,
  DHRELINK_VERSION,
  kVstVersionString,
  dhrelink::vst3::SenderProcessor::createInstance)

DEF_CLASS2(
  INLINE_UID_FROM_FUID(dhrelink::vst3::kSenderControllerId),
  Steinberg::PClassInfo::kManyInstances,
  kVstComponentControllerClass,
  "dhreLink Sender Controller",
  0,
  "",
  DHRELINK_VERSION,
  kVstVersionString,
  dhrelink::vst3::SenderController::createInstance)

END_FACTORY
