#include "dhrelink/shared_audio_stream.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {

bool nearlyEqual(float left, float right) {
  return std::abs(left - right) < 0.00001F;
}

} // namespace

int main() {
#if !defined(_WIN32)
  std::cout << "Shared-memory transport is currently implemented for Windows only.\n";
  return 0;
#else
  const auto streamId = "transport-test-" + std::to_string(GetCurrentProcessId());
  dhrelink::StreamWriter writer;
  if (!writer.open(streamId, "Transport Test")) {
    std::cerr << "Could not open writer.\n";
    return 1;
  }

  dhrelink::StreamStatusReader statusReader;
  if (!statusReader.open(streamId)) {
    std::cerr << "Could not open status reader.\n";
    return 11;
  }
  dhrelink::StreamStatus streamStatus;
  if (!statusReader.snapshot(streamStatus) || !streamStatus.senderConnected || streamStatus.receiverConnected) {
    std::cerr << "Initial stream status is invalid.\n";
    return 12;
  }

  dhrelink::StreamWriter duplicateWriter;
  if (duplicateWriter.open(streamId, "Duplicate")) {
    std::cerr << "A second producer claimed the same stream.\n";
    return 2;
  }

  dhrelink::StreamReader reader;
  if (!reader.open(streamId)) {
    std::cerr << "Could not open reader.\n";
    return 3;
  }
  if (!statusReader.snapshot(streamStatus) || !streamStatus.receiverConnected) {
    std::cerr << "Receiver heartbeat was not published.\n";
    return 13;
  }

  constexpr std::uint32_t frameCount = 256;
  std::array<float, frameCount> left{};
  std::array<float, frameCount> right{};
  for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
    left[frame] = static_cast<float>(frame) / static_cast<float>(frameCount);
    right[frame] = -left[frame];
  }

  const float* buffers[] = {left.data(), right.data()};
  if (!writer.push(buffers, 2, frameCount, 48'000, 12'345)) {
    std::cerr << "Writer rejected a valid block.\n";
    return 4;
  }

  dhrelink::AudioBlock block;
  const auto status = reader.read(block);
  if (status != dhrelink::ReadStatus::audio) {
    std::cerr << "Reader did not receive the block.\n";
    return 5;
  }
  if (block.frames != frameCount || block.channels != 2 || block.sampleRate != 48'000) return 6;
  if (block.samplePosition != 12'345) return 7;
  if (!nearlyEqual(block.samples[0][127], left[127])) return 8;
  if (!nearlyEqual(block.samples[1][255], right[255])) return 9;
  if (reader.read(block) != dhrelink::ReadStatus::noData) return 10;
  if (!statusReader.snapshot(streamStatus)) return 14;
  if (streamStatus.sampleRate != 48'000 || streamStatus.channels != 2) return 15;
  if (!nearlyEqual(streamStatus.peakLeft, left.back())) return 16;
  if (!nearlyEqual(streamStatus.peakRight, -right.back())) return 17;

  writer.close();
  if (reader.read(block) != dhrelink::ReadStatus::senderDisconnected) {
    std::cerr << "Reader did not detect sender removal.\n";
    return 18;
  }
  reader.close();
  if (reader.open(streamId)) {
    std::cerr << "Reader reopened a stale stream after sender removal.\n";
    return 19;
  }
  if (!statusReader.snapshot(streamStatus) || streamStatus.senderConnected) {
    std::cerr << "Status reader did not report sender removal.\n";
    return 20;
  }

  if (!writer.open(streamId, "Transport Test Reconnected")) {
    std::cerr << "Writer could not reconnect.\n";
    return 21;
  }
  if (!reader.open(streamId)) {
    std::cerr << "Reader could not reconnect.\n";
    return 22;
  }
  if (!writer.push(buffers, 2, frameCount, 48'000, 22'222)) {
    std::cerr << "Reconnected writer rejected a valid block.\n";
    return 23;
  }
  if (reader.read(block) != dhrelink::ReadStatus::audio || block.samplePosition != 22'222) {
    std::cerr << "Reader did not receive audio after reconnection.\n";
    return 24;
  }

  std::cout << "dhreLink shared-memory transport passed.\n";
  return 0;
#endif
}
