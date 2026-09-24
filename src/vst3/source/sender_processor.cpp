#include "sender_processor.hpp"

#include "cids.hpp"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace dhrelink::vst3 {
SenderProcessor::SenderProcessor() {
  setControllerClass(kSenderControllerId);
}

Steinberg::tresult PLUGIN_API SenderProcessor::initialize(Steinberg::FUnknown* context) {
  const auto result = AudioEffect::initialize(context);
  if (result != Steinberg::kResultOk) return result;

  if (addAudioInput(STR16("Input"), Steinberg::Vst::SpeakerArr::kStereo) == nullptr) {
    return Steinberg::kResultFalse;
  }
  if (addAudioOutput(STR16("Output"), Steinberg::Vst::SpeakerArr::kStereo) == nullptr) {
    return Steinberg::kResultFalse;
  }
  return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API SenderProcessor::terminate() {
  stream_.close();
  return AudioEffect::terminate();
}

Steinberg::tresult PLUGIN_API SenderProcessor::setActive(Steinberg::TBool state) {
  if (state) {
    const auto opened = stream_.open("main-mix", "Main Mix");
    static_cast<void>(opened);
    runningSamplePosition_ = 0;
  } else {
    stream_.close();
  }
  return AudioEffect::setActive(state);
}

Steinberg::tresult PLUGIN_API SenderProcessor::setBusArrangements(
  Steinberg::Vst::SpeakerArrangement* inputs,
  Steinberg::int32 inputCount,
  Steinberg::Vst::SpeakerArrangement* outputs,
  Steinberg::int32 outputCount) {
  if (inputCount != 1 || outputCount != 1) return Steinberg::kResultFalse;
  const auto inputChannels = Steinberg::Vst::SpeakerArr::getChannelCount(inputs[0]);
  const auto outputChannels = Steinberg::Vst::SpeakerArr::getChannelCount(outputs[0]);
  if (inputChannels != outputChannels || (inputChannels != 1 && inputChannels != 2)) {
    return Steinberg::kResultFalse;
  }

  getAudioInput(0)->setArrangement(inputs[0]);
  getAudioOutput(0)->setArrangement(outputs[0]);
  return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API SenderProcessor::canProcessSampleSize(Steinberg::int32 symbolicSampleSize) {
  return symbolicSampleSize == Steinberg::Vst::kSample32 || symbolicSampleSize == Steinberg::Vst::kSample64
    ? Steinberg::kResultTrue
    : Steinberg::kResultFalse;
}

template <typename Sample>
void SenderProcessor::copyAudio(
  Sample** input,
  Sample** output,
  Steinberg::int32 channels,
  Steinberg::int32 frames) noexcept {
  for (Steinberg::int32 channel = 0; channel < channels; ++channel) {
    if (output[channel] == nullptr) continue;
    if (input[channel] == nullptr) {
      std::fill_n(output[channel], frames, static_cast<Sample>(0));
    } else if (input[channel] != output[channel]) {
      std::memcpy(output[channel], input[channel], static_cast<std::size_t>(frames) * sizeof(Sample));
    }
  }
}

void SenderProcessor::publishFloatAudio(
  float** input,
  Steinberg::int32 channels,
  Steinberg::int32 frames,
  std::uint64_t samplePosition,
  std::uint32_t flags) noexcept {
  const float* buffers[kMaxChannels] = {input[0], channels > 1 ? input[1] : nullptr};
  const auto pushed = stream_.push(
    buffers,
    static_cast<std::uint32_t>(channels),
    static_cast<std::uint32_t>(frames),
    static_cast<std::uint32_t>(processSetup.sampleRate),
    samplePosition,
    flags);
  static_cast<void>(pushed);
}

void SenderProcessor::publishDoubleAudio(
  double** input,
  Steinberg::int32 channels,
  Steinberg::int32 frames,
  std::uint64_t samplePosition,
  std::uint32_t flags) noexcept {
  Steinberg::int32 offset = 0;
  while (offset < frames) {
    const auto blockFrames = std::min<Steinberg::int32>(kMaxFramesPerSlot, frames - offset);
    for (Steinberg::int32 channel = 0; channel < channels; ++channel) {
      if (input[channel] == nullptr) {
        std::fill_n(conversionBuffer_[channel].data(), blockFrames, 0.0F);
      } else {
        for (Steinberg::int32 frame = 0; frame < blockFrames; ++frame) {
          conversionBuffer_[channel][frame] = static_cast<float>(input[channel][offset + frame]);
        }
      }
    }
    const float* buffers[kMaxChannels] = {
      conversionBuffer_[0].data(),
      channels > 1 ? conversionBuffer_[1].data() : nullptr,
    };
    const auto pushed = stream_.push(
      buffers,
      static_cast<std::uint32_t>(channels),
      static_cast<std::uint32_t>(blockFrames),
      static_cast<std::uint32_t>(processSetup.sampleRate),
      samplePosition + static_cast<std::uint64_t>(offset),
      flags);
    static_cast<void>(pushed);
    offset += blockFrames;
  }
}

Steinberg::tresult PLUGIN_API SenderProcessor::process(Steinberg::Vst::ProcessData& data) {
  if (
    data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0 ||
    data.inputs == nullptr || data.outputs == nullptr) {
    return Steinberg::kResultOk;
  }

  const auto channels = std::min({
    data.inputs[0].numChannels,
    data.outputs[0].numChannels,
    static_cast<Steinberg::int32>(kMaxChannels),
  });
  if (channels <= 0) return Steinberg::kResultOk;

  std::uint64_t samplePosition = runningSamplePosition_;
  if (data.processContext != nullptr) {
    samplePosition = static_cast<std::uint64_t>(
      std::max<Steinberg::Vst::TSamples>(0, data.processContext->projectTimeSamples));
  }

  const auto silence = data.inputs[0].silenceFlags != 0;
  const auto flags = silence ? kAudioBlockSilence : kAudioBlockNone;

  if (data.symbolicSampleSize == Steinberg::Vst::kSample32) {
    auto** input = data.inputs[0].channelBuffers32;
    auto** output = data.outputs[0].channelBuffers32;
    if (input == nullptr || output == nullptr) return Steinberg::kResultOk;
    copyAudio(input, output, channels, data.numSamples);
    publishFloatAudio(input, channels, data.numSamples, samplePosition, flags);
  } else {
    auto** input = data.inputs[0].channelBuffers64;
    auto** output = data.outputs[0].channelBuffers64;
    if (input == nullptr || output == nullptr) return Steinberg::kResultOk;
    copyAudio(input, output, channels, data.numSamples);
    publishDoubleAudio(input, channels, data.numSamples, samplePosition, flags);
  }

  data.outputs[0].silenceFlags = data.inputs[0].silenceFlags;
  runningSamplePosition_ = samplePosition + static_cast<std::uint64_t>(data.numSamples);
  return Steinberg::kResultOk;
}

} // namespace dhrelink::vst3
