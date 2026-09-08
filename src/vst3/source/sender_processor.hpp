#pragma once

#include "dhrelink/shared_audio_stream.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <cstdint>

namespace dhrelink::vst3 {

class SenderProcessor final : public Steinberg::Vst::AudioEffect {
public:
  SenderProcessor();
  ~SenderProcessor() SMTG_OVERRIDE = default;

  static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new SenderProcessor()); }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setBusArrangements(
    Steinberg::Vst::SpeakerArrangement* inputs,
    Steinberg::int32 inputCount,
    Steinberg::Vst::SpeakerArrangement* outputs,
    Steinberg::int32 outputCount) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

private:
  template <typename Sample>
  void copyAudio(
    Sample** input,
    Sample** output,
    Steinberg::int32 channels,
    Steinberg::int32 frames) noexcept;

  void publishFloatAudio(
    float** input,
    Steinberg::int32 channels,
    Steinberg::int32 frames,
    std::uint64_t samplePosition,
    std::uint32_t flags) noexcept;
  void publishDoubleAudio(
    double** input,
    Steinberg::int32 channels,
    Steinberg::int32 frames,
    std::uint64_t samplePosition,
    std::uint32_t flags) noexcept;

  StreamWriter stream_;
  std::array<std::array<float, kMaxFramesPerSlot>, kMaxChannels> conversionBuffer_{};
  std::uint64_t runningSamplePosition_ = 0;
};

} // namespace dhrelink::vst3
