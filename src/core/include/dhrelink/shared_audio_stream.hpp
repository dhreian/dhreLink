#pragma once

#include "dhrelink/audio_protocol.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace dhrelink {

class StreamWriter final {
public:
  StreamWriter();
  ~StreamWriter();

  StreamWriter(const StreamWriter&) = delete;
  StreamWriter& operator=(const StreamWriter&) = delete;
  StreamWriter(StreamWriter&&) noexcept;
  StreamWriter& operator=(StreamWriter&&) noexcept;

  [[nodiscard]] bool open(std::string_view streamId, std::string_view displayName);
  void close() noexcept;
  [[nodiscard]] bool isOpen() const noexcept;

  // Real-time safe after open(): no locks, heap allocation, I/O, or waiting.
  // Buffers are planar and may be null for a silent/missing channel.
  [[nodiscard]] bool push(
    const float* const* buffers,
    std::uint32_t channels,
    std::uint32_t frames,
    std::uint32_t sampleRate,
    std::uint64_t samplePosition,
    std::uint32_t flags = kAudioBlockNone) noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

enum class ReadStatus {
  noData,
  audio,
  overrun,
  senderDisconnected,
};

class StreamReader final {
public:
  StreamReader();
  ~StreamReader();

  StreamReader(const StreamReader&) = delete;
  StreamReader& operator=(const StreamReader&) = delete;
  StreamReader(StreamReader&&) noexcept;
  StreamReader& operator=(StreamReader&&) noexcept;

  [[nodiscard]] bool open(std::string_view streamId);
  void close() noexcept;
  [[nodiscard]] bool isOpen() const noexcept;
  [[nodiscard]] ReadStatus read(AudioBlock& destination) noexcept;
  [[nodiscard]] std::uint64_t droppedBlocks() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class StreamStatusReader final {
public:
  StreamStatusReader();
  ~StreamStatusReader();

  StreamStatusReader(const StreamStatusReader&) = delete;
  StreamStatusReader& operator=(const StreamStatusReader&) = delete;
  StreamStatusReader(StreamStatusReader&&) noexcept;
  StreamStatusReader& operator=(StreamStatusReader&&) noexcept;

  [[nodiscard]] bool open(std::string_view streamId);
  void close() noexcept;
  [[nodiscard]] bool isOpen() const noexcept;
  [[nodiscard]] bool snapshot(StreamStatus& destination) noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace dhrelink
