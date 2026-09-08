#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dhrelink {

inline constexpr std::uint32_t kProtocolMagic = 0x4B4E4C44; // "DLNK"
inline constexpr std::uint32_t kProtocolVersion = 2;
inline constexpr std::uint32_t kSlotCount = 128;
inline constexpr std::uint32_t kMaxFramesPerSlot = 1'024;
inline constexpr std::uint32_t kMaxChannels = 2;
inline constexpr std::size_t kStreamIdBytes = 40;
inline constexpr std::size_t kDisplayNameBytes = 64;

enum AudioBlockFlags : std::uint32_t {
  kAudioBlockNone = 0,
  kAudioBlockDiscontinuity = 1U << 0U,
  kAudioBlockSilence = 1U << 1U,
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

// LONG64-compatible fields are intentionally represented as signed 64-bit
// integers. The Windows implementation accesses them only through Interlocked
// operations when they cross a process boundary.
struct alignas(64) StreamHeader final {
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t headerBytes = 0;
  std::uint32_t slotCount = 0;
  std::uint32_t maxFramesPerSlot = 0;
  std::uint32_t maxChannels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t channels = 0;
  alignas(8) volatile std::int64_t writeSequence = 0;
  alignas(8) volatile std::int64_t senderHeartbeatMs = 0;
  alignas(8) volatile std::int64_t receiverHeartbeatMs = 0;
  alignas(4) volatile std::int32_t peakLeftBits = 0;
  alignas(4) volatile std::int32_t peakRightBits = 0;
  char streamId[kStreamIdBytes]{};
  char displayName[kDisplayNameBytes]{};
  std::array<std::byte, 304> reserved{};
};

struct alignas(64) AudioSlot final {
  alignas(8) volatile std::int64_t sequence = 0;
  std::uint64_t samplePosition = 0;
  std::uint32_t frames = 0;
  std::uint32_t channels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t flags = 0;
  std::array<std::array<float, kMaxFramesPerSlot>, kMaxChannels> samples{};
};

struct alignas(64) SharedAudioRegion final {
  StreamHeader header{};
  std::array<AudioSlot, kSlotCount> slots{};
};

static_assert(alignof(StreamHeader) == 64);
static_assert(alignof(AudioSlot) == 64);
static_assert(sizeof(std::int64_t) == 8);

struct AudioBlock final {
  std::uint64_t sequence = 0;
  std::uint64_t samplePosition = 0;
  std::uint32_t frames = 0;
  std::uint32_t channels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t flags = 0;
  std::array<std::array<float, kMaxFramesPerSlot>, kMaxChannels> samples{};
};

struct StreamStatus final {
  bool senderConnected = false;
  bool receiverConnected = false;
  std::uint32_t sampleRate = 0;
  std::uint32_t channels = 0;
  float peakLeft = 0.0F;
  float peakRight = 0.0F;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace dhrelink
