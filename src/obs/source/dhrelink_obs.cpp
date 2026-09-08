// SPDX-License-Identifier: GPL-2.0-or-later

#include "dhrelink/shared_audio_stream.hpp"

#include <obs-module.h>
#include <obs-properties.h>
#include <util/platform.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <thread>

OBS_DECLARE_MODULE()

namespace {

constexpr auto kReconnectDelay = std::chrono::milliseconds(250);
constexpr auto kIdleDelay = std::chrono::milliseconds(1);
constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000ULL;
constexpr std::uint64_t kClockToleranceNs = 100'000'000ULL;

struct Receiver final {
  explicit Receiver(obs_source_t* sourceValue) : source(sourceValue) {}

  obs_source_t* source = nullptr;
  dhrelink::StreamReader reader;
  std::atomic<bool> running{true};
  std::thread worker;
  std::atomic<bool> connected{false};
  std::atomic<std::uint32_t> currentSampleRate{0};
  std::atomic<std::uint32_t> currentChannels{0};
  std::uint64_t nextTimestampNs = 0;
  std::uint32_t clockSampleRate = 0;

  bool start() noexcept {
    try {
      worker = std::thread([this] { run(); });
      return true;
    } catch (...) {
      running.store(false, std::memory_order_release);
      return false;
    }
  }

  void stop() {
    running.store(false, std::memory_order_release);
    if (worker.joinable()) worker.join();
    reader.close();
  }

  void resetClock() noexcept {
    nextTimestampNs = 0;
    clockSampleRate = 0;
  }

  void updateState(
    bool newConnected,
    std::uint32_t newSampleRate,
    std::uint32_t newChannels) noexcept {
    const auto connectionChanged =
      connected.exchange(newConnected, std::memory_order_acq_rel) != newConnected;
    const auto sampleRateChanged =
      currentSampleRate.exchange(newSampleRate, std::memory_order_acq_rel) != newSampleRate;
    const auto channelsChanged =
      currentChannels.exchange(newChannels, std::memory_order_acq_rel) != newChannels;
    if (connectionChanged || sampleRateChanged || channelsChanged) {
      obs_source_update_properties(source);
    }
  }

  std::uint64_t timestampFor(
    const dhrelink::AudioBlock& block,
    dhrelink::ReadStatus status) noexcept {
    const auto now = os_gettime_ns();
    const auto tooLate =
      now > nextTimestampNs && now - nextTimestampNs > kClockToleranceNs;
    const auto tooEarly =
      nextTimestampNs > now && nextTimestampNs - now > kClockToleranceNs;

    if (
      nextTimestampNs == 0 || clockSampleRate != block.sampleRate ||
      status == dhrelink::ReadStatus::overrun || tooLate || tooEarly) {
      nextTimestampNs = now;
      clockSampleRate = block.sampleRate;
    }

    const auto timestamp = nextTimestampNs;
    nextTimestampNs +=
      static_cast<std::uint64_t>(block.frames) * kNanosecondsPerSecond /
      block.sampleRate;
    return timestamp;
  }

  void run() {
    dhrelink::AudioBlock block;
    while (running.load(std::memory_order_acquire)) {
      if (!reader.isOpen()) {
        if (!reader.open("main-mix")) {
          std::this_thread::sleep_for(kReconnectDelay);
          continue;
        }
      }

      const auto status = reader.read(block);
      if (status == dhrelink::ReadStatus::senderDisconnected) {
        reader.close();
        resetClock();
        updateState(false, 0, 0);
        std::this_thread::sleep_for(kReconnectDelay);
        continue;
      }
      if (status == dhrelink::ReadStatus::noData) {
        std::this_thread::sleep_for(kIdleDelay);
        continue;
      }

      updateState(true, block.sampleRate, block.channels);

      obs_source_audio audio{};
      audio.data[0] = reinterpret_cast<std::uint8_t*>(block.samples[0].data());
      if (block.channels > 1) {
        audio.data[1] = reinterpret_cast<std::uint8_t*>(block.samples[1].data());
      }
      audio.frames = block.frames;
      audio.speakers = block.channels == 1 ? SPEAKERS_MONO : SPEAKERS_STEREO;
      audio.samples_per_sec = block.sampleRate;
      audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
      audio.timestamp = timestampFor(block, status);
      obs_source_output_audio(source, &audio);
    }
  }
};

const char* sourceName(void*) {
  return "dhreLink Receiver";
}

obs_properties_t* sourceProperties(void* data) {
  const auto* receiver = static_cast<const Receiver*>(data);
  const auto isConnected =
    receiver != nullptr && receiver->connected.load(std::memory_order_acquire);
  const auto sampleRate = receiver != nullptr
    ? receiver->currentSampleRate.load(std::memory_order_acquire)
    : 0;
  const auto channels = receiver != nullptr
    ? receiver->currentChannels.load(std::memory_order_acquire)
    : 0;

  auto* properties = obs_properties_create();
  auto* status = obs_properties_add_text(
    properties,
    "dhrelink_status",
    isConnected
      ? "<span style=\"color:#5FE39B; font-weight:600;\">● Conectado</span>"
        " | Recibiendo audio desde el DAW."
      : "○  Esperando la señal del DAW…",
    OBS_TEXT_INFO);
  obs_property_text_set_info_type(
    status, isConnected ? OBS_TEXT_INFO_NORMAL : OBS_TEXT_INFO_WARNING);

  obs_properties_add_text(
    properties,
    "dhrelink_route",
    "DAW   →   dhreLink   →   OBS",
    OBS_TEXT_INFO);

  char format[96] = "Formato: esperando audio";
  if (sampleRate > 0 && channels > 0) {
    std::snprintf(
      format,
      sizeof(format),
      "Formato: %.1f kHz | %s",
      static_cast<double>(sampleRate) / 1'000.0,
      channels == 1 ? "Mono" : "Estéreo");
  }
  obs_properties_add_text(
    properties,
    "dhrelink_format",
    format,
    OBS_TEXT_INFO);
  obs_properties_add_text(
    properties,
    "dhrelink_help",
    "La conexión es automática. Mantén una sola instancia de dhreLink Sender activa.",
    OBS_TEXT_INFO);
  obs_properties_add_text(
    properties,
    "dhrelink_credit",
    "Developed by dhreian",
    OBS_TEXT_INFO);
  return properties;
}

const char* darkIcon(void*) {
  return obs_module_file("dhreLink-dark.svg");
}

const char* lightIcon(void*) {
  return obs_module_file("dhreLink-light.svg");
}

void* sourceCreate(obs_data_t*, obs_source_t* source) {
  try {
    auto* receiver = new Receiver(source);
    if (!receiver->start()) {
      delete receiver;
      return nullptr;
    }
    return receiver;
  } catch (...) {
    return nullptr;
  }
}

void sourceDestroy(void* data) {
  auto* receiver = static_cast<Receiver*>(data);
  if (receiver == nullptr) return;
  receiver->stop();
  delete receiver;
}

obs_source_info sourceInfo = {
  .id = "dhrelink_audio_source",
  .type = OBS_SOURCE_TYPE_INPUT,
  .output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
  .get_name = sourceName,
  .create = sourceCreate,
  .destroy = sourceDestroy,
  .get_properties = sourceProperties,
  .icon_type = OBS_ICON_TYPE_CUSTOM,
  .get_dark_icon = darkIcon,
  .get_light_icon = lightIcon,
};

} // namespace

const char* obs_module_description(void) {
  return "Receives post-effects DAW audio from dhreLink Sender.";
}

bool obs_module_load(void) {
  obs_register_source(&sourceInfo);
  return true;
}
