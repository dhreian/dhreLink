#include "dhrelink/shared_audio_stream.hpp"

#include <Windows.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace dhrelink {
namespace {

constexpr std::uint64_t kSenderTimeoutMs = 2'000;

std::string normalizeStreamId(std::string_view value) {
  std::string result;
  result.reserve(std::min<std::size_t>(value.size(), 32));
  for (const unsigned char character : value) {
    if (result.size() == 32) break;
    if (std::isalnum(character) != 0 || character == '-' || character == '_') {
      result.push_back(static_cast<char>(std::tolower(character)));
    } else {
      result.push_back('-');
    }
  }
  return result.empty() ? "main-mix" : result;
}

std::wstring widenAscii(std::string_view value) {
  return {value.begin(), value.end()};
}

std::wstring mappingName(std::string_view streamId) {
  return L"Local\\dhreLink-audio-v1-" + widenAscii(normalizeStreamId(streamId));
}

std::wstring writerMutexName(std::string_view streamId) {
  return L"Local\\dhreLink-writer-v1-" + widenAscii(normalizeStreamId(streamId));
}

volatile LONG64* interlocked(std::int64_t& value) noexcept {
  return reinterpret_cast<volatile LONG64*>(&value);
}

volatile LONG64* interlocked(volatile std::int64_t& value) noexcept {
  return reinterpret_cast<volatile LONG64*>(&value);
}

std::int64_t atomicLoad(volatile std::int64_t& value) noexcept {
  return InterlockedCompareExchange64(interlocked(value), 0, 0);
}

void atomicStore(volatile std::int64_t& value, std::int64_t desired) noexcept {
  InterlockedExchange64(interlocked(value), static_cast<LONG64>(desired));
}

std::int64_t atomicIncrement(volatile std::int64_t& value) noexcept {
  return InterlockedIncrement64(interlocked(value));
}

std::int32_t atomicLoad(volatile std::int32_t& value) noexcept {
  return InterlockedCompareExchange(
    reinterpret_cast<volatile LONG*>(&value), 0, 0);
}

void atomicStore(volatile std::int32_t& value, std::int32_t desired) noexcept {
  InterlockedExchange(
    reinterpret_cast<volatile LONG*>(&value), static_cast<LONG>(desired));
}

void copyText(char* destination, std::size_t capacity, std::string_view source) noexcept {
  if (capacity == 0) return;
  const auto bytes = std::min(capacity - 1, source.size());
  std::memcpy(destination, source.data(), bytes);
  destination[bytes] = '\0';
}

} // namespace

class StreamWriter::Impl final {
public:
  HANDLE mapping = nullptr;
  HANDLE writerMutex = nullptr;
  SharedAudioRegion* region = nullptr;

  void close() noexcept {
    if (region != nullptr) {
      atomicStore(region->header.senderHeartbeatMs, 0);
      UnmapViewOfFile(region);
      region = nullptr;
    }
    if (mapping != nullptr) {
      CloseHandle(mapping);
      mapping = nullptr;
    }
    if (writerMutex != nullptr) {
      ReleaseMutex(writerMutex);
      CloseHandle(writerMutex);
      writerMutex = nullptr;
    }
  }
};

StreamWriter::StreamWriter() : impl_(std::make_unique<Impl>()) {}
StreamWriter::~StreamWriter() { close(); }
StreamWriter::StreamWriter(StreamWriter&&) noexcept = default;
StreamWriter& StreamWriter::operator=(StreamWriter&&) noexcept = default;

bool StreamWriter::open(std::string_view streamId, std::string_view displayName) {
  close();
  const auto normalizedId = normalizeStreamId(streamId);
  const auto mutexName = writerMutexName(normalizedId);

  impl_->writerMutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
  if (impl_->writerMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (impl_->writerMutex != nullptr) CloseHandle(impl_->writerMutex);
    impl_->writerMutex = nullptr;
    return false;
  }

  const auto name = mappingName(normalizedId);
  impl_->mapping = CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    0,
    static_cast<DWORD>(sizeof(SharedAudioRegion)),
    name.c_str());

  if (impl_->mapping == nullptr) {
    close();
    return false;
  }

  impl_->region = static_cast<SharedAudioRegion*>(
    MapViewOfFile(impl_->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedAudioRegion)));
  if (impl_->region == nullptr) {
    close();
    return false;
  }

  std::memset(impl_->region, 0, sizeof(SharedAudioRegion));
  auto& header = impl_->region->header;
  header.version = kProtocolVersion;
  header.headerBytes = sizeof(StreamHeader);
  header.slotCount = kSlotCount;
  header.maxFramesPerSlot = kMaxFramesPerSlot;
  header.maxChannels = kMaxChannels;
  copyText(header.streamId, sizeof(header.streamId), normalizedId);
  copyText(header.displayName, sizeof(header.displayName), displayName);
  atomicStore(header.senderHeartbeatMs, static_cast<std::int64_t>(GetTickCount64()));
  MemoryBarrier();
  header.magic = kProtocolMagic;
  MemoryBarrier();
  return true;
}

void StreamWriter::close() noexcept { impl_->close(); }
bool StreamWriter::isOpen() const noexcept { return impl_->region != nullptr; }

bool StreamWriter::push(
  const float* const* buffers,
  std::uint32_t channels,
  std::uint32_t frames,
  std::uint32_t sampleRate,
  std::uint64_t samplePosition,
  std::uint32_t flags) noexcept {
  if (impl_->region == nullptr || buffers == nullptr || channels == 0 || sampleRate == 0) return false;
  if (frames == 0) return true;

  auto& header = impl_->region->header;
  const auto outputChannels = std::min(channels, kMaxChannels);
  header.sampleRate = sampleRate;
  header.channels = outputChannels;

  float peaks[kMaxChannels]{};
  for (std::uint32_t channel = 0; channel < outputChannels; ++channel) {
    if (buffers[channel] == nullptr) continue;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
      peaks[channel] = std::max(peaks[channel], std::abs(buffers[channel][frame]));
    }
  }
  atomicStore(header.peakLeftBits, std::bit_cast<std::int32_t>(peaks[0]));
  atomicStore(
    header.peakRightBits,
    std::bit_cast<std::int32_t>(outputChannels > 1 ? peaks[1] : peaks[0]));

  std::uint32_t offset = 0;
  while (offset < frames) {
    const auto blockFrames = std::min(kMaxFramesPerSlot, frames - offset);
    const auto sequence = atomicIncrement(header.writeSequence);
    auto& slot = impl_->region->slots[(static_cast<std::uint64_t>(sequence) - 1U) % kSlotCount];

    atomicStore(slot.sequence, 0);
    slot.samplePosition = samplePosition + offset;
    slot.frames = blockFrames;
    slot.channels = outputChannels;
    slot.sampleRate = sampleRate;
    slot.flags = flags;

    for (std::uint32_t channel = 0; channel < kMaxChannels; ++channel) {
      auto* destination = slot.samples[channel].data();
      if (channel < outputChannels && buffers[channel] != nullptr) {
        std::memcpy(destination, buffers[channel] + offset, blockFrames * sizeof(float));
      } else {
        std::memset(destination, 0, blockFrames * sizeof(float));
      }
    }

    MemoryBarrier();
    atomicStore(slot.sequence, sequence);
    offset += blockFrames;
  }

  atomicStore(header.senderHeartbeatMs, static_cast<std::int64_t>(GetTickCount64()));
  return true;
}

class StreamReader::Impl final {
public:
  HANDLE mapping = nullptr;
  SharedAudioRegion* region = nullptr;
  std::uint64_t nextSequence = 0;
  std::uint64_t dropped = 0;

  void close() noexcept {
    if (region != nullptr) {
      atomicStore(region->header.receiverHeartbeatMs, 0);
      UnmapViewOfFile(region);
      region = nullptr;
    }
    if (mapping != nullptr) {
      CloseHandle(mapping);
      mapping = nullptr;
    }
    nextSequence = 0;
    dropped = 0;
  }

  bool disconnected() const noexcept {
    if (region == nullptr) return true;
    const auto heartbeat = atomicLoad(region->header.senderHeartbeatMs);
    const auto now = static_cast<std::int64_t>(GetTickCount64());
    return heartbeat <= 0 || now - heartbeat > static_cast<std::int64_t>(kSenderTimeoutMs);
  }
};

StreamReader::StreamReader() : impl_(std::make_unique<Impl>()) {}
StreamReader::~StreamReader() { close(); }
StreamReader::StreamReader(StreamReader&&) noexcept = default;
StreamReader& StreamReader::operator=(StreamReader&&) noexcept = default;

bool StreamReader::open(std::string_view streamId) {
  close();
  const auto name = mappingName(streamId);
  impl_->mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
  if (impl_->mapping == nullptr) return false;

  impl_->region = static_cast<SharedAudioRegion*>(
    MapViewOfFile(impl_->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedAudioRegion)));
  if (impl_->region == nullptr) {
    close();
    return false;
  }

  MemoryBarrier();
  const auto& header = impl_->region->header;
  if (
    header.magic != kProtocolMagic ||
    header.version != kProtocolVersion ||
    header.slotCount != kSlotCount ||
    header.maxFramesPerSlot != kMaxFramesPerSlot ||
    header.maxChannels != kMaxChannels) {
    close();
    return false;
  }

  // A status observer can keep the mapping object alive briefly after the DAW
  // removes the sender. Never reopen that stale region as a live stream.
  if (impl_->disconnected()) {
    close();
    return false;
  }

  atomicStore(
    impl_->region->header.receiverHeartbeatMs,
    static_cast<std::int64_t>(GetTickCount64()));

  return true;
}

void StreamReader::close() noexcept { impl_->close(); }
bool StreamReader::isOpen() const noexcept { return impl_->region != nullptr; }
std::uint64_t StreamReader::droppedBlocks() const noexcept { return impl_->dropped; }

ReadStatus StreamReader::read(AudioBlock& destination) noexcept {
  if (impl_->region == nullptr) return ReadStatus::senderDisconnected;

  auto& header = impl_->region->header;
  atomicStore(
    header.receiverHeartbeatMs,
    static_cast<std::int64_t>(GetTickCount64()));
  if (impl_->disconnected()) return ReadStatus::senderDisconnected;

  const auto latestSigned = atomicLoad(header.writeSequence);
  if (latestSigned <= 0) {
    return impl_->disconnected() ? ReadStatus::senderDisconnected : ReadStatus::noData;
  }

  const auto latest = static_cast<std::uint64_t>(latestSigned);
  bool overrun = false;
  if (impl_->nextSequence == 0) impl_->nextSequence = latest;

  const auto earliest = latest >= kSlotCount ? latest - kSlotCount + 1 : 1;
  if (impl_->nextSequence < earliest) {
    impl_->dropped += earliest - impl_->nextSequence;
    impl_->nextSequence = earliest;
    overrun = true;
  }

  if (impl_->nextSequence > latest) {
    return impl_->disconnected() ? ReadStatus::senderDisconnected : ReadStatus::noData;
  }

  auto expected = impl_->nextSequence;
  auto* slot = &impl_->region->slots[(expected - 1U) % kSlotCount];
  auto published = atomicLoad(slot->sequence);
  if (published <= 0) return ReadStatus::noData;

  if (static_cast<std::uint64_t>(published) > expected) {
    impl_->dropped += static_cast<std::uint64_t>(published) - expected;
    expected = static_cast<std::uint64_t>(published);
    impl_->nextSequence = expected;
    slot = &impl_->region->slots[(expected - 1U) % kSlotCount];
    published = atomicLoad(slot->sequence);
    overrun = true;
  }

  if (static_cast<std::uint64_t>(published) != expected) return ReadStatus::noData;
  MemoryBarrier();

  const auto frames = slot->frames;
  const auto channels = slot->channels;
  if (frames > kMaxFramesPerSlot || channels == 0 || channels > kMaxChannels || slot->sampleRate == 0) {
    impl_->nextSequence = expected + 1;
    return ReadStatus::noData;
  }

  destination.sequence = expected;
  destination.samplePosition = slot->samplePosition;
  destination.frames = frames;
  destination.channels = channels;
  destination.sampleRate = slot->sampleRate;
  destination.flags = slot->flags;
  for (std::uint32_t channel = 0; channel < channels; ++channel) {
    std::memcpy(destination.samples[channel].data(), slot->samples[channel].data(), frames * sizeof(float));
  }

  MemoryBarrier();
  if (atomicLoad(slot->sequence) != static_cast<std::int64_t>(expected)) {
    return ReadStatus::noData;
  }

  impl_->nextSequence = expected + 1;
  return overrun ? ReadStatus::overrun : ReadStatus::audio;
}

class StreamStatusReader::Impl final {
public:
  HANDLE mapping = nullptr;
  SharedAudioRegion* region = nullptr;

  void close() noexcept {
    if (region != nullptr) {
      UnmapViewOfFile(region);
      region = nullptr;
    }
    if (mapping != nullptr) {
      CloseHandle(mapping);
      mapping = nullptr;
    }
  }
};

StreamStatusReader::StreamStatusReader() : impl_(std::make_unique<Impl>()) {}
StreamStatusReader::~StreamStatusReader() { close(); }
StreamStatusReader::StreamStatusReader(StreamStatusReader&&) noexcept = default;
StreamStatusReader& StreamStatusReader::operator=(StreamStatusReader&&) noexcept = default;

bool StreamStatusReader::open(std::string_view streamId) {
  close();
  const auto name = mappingName(streamId);
  impl_->mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
  if (impl_->mapping == nullptr) return false;

  impl_->region = static_cast<SharedAudioRegion*>(
    MapViewOfFile(impl_->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedAudioRegion)));
  if (impl_->region == nullptr) {
    close();
    return false;
  }

  MemoryBarrier();
  const auto& header = impl_->region->header;
  if (
    header.magic != kProtocolMagic || header.version != kProtocolVersion ||
    header.slotCount != kSlotCount || header.maxFramesPerSlot != kMaxFramesPerSlot ||
    header.maxChannels != kMaxChannels) {
    close();
    return false;
  }
  return true;
}

void StreamStatusReader::close() noexcept { impl_->close(); }
bool StreamStatusReader::isOpen() const noexcept { return impl_->region != nullptr; }

bool StreamStatusReader::snapshot(StreamStatus& destination) noexcept {
  if (impl_->region == nullptr) return false;
  auto& header = impl_->region->header;
  MemoryBarrier();
  if (header.magic != kProtocolMagic || header.version != kProtocolVersion) return false;

  const auto now = static_cast<std::int64_t>(GetTickCount64());
  const auto senderHeartbeat = atomicLoad(header.senderHeartbeatMs);
  const auto receiverHeartbeat = atomicLoad(header.receiverHeartbeatMs);
  destination.senderConnected =
    senderHeartbeat > 0 && now - senderHeartbeat <= static_cast<std::int64_t>(kSenderTimeoutMs);
  destination.receiverConnected =
    receiverHeartbeat > 0 && now - receiverHeartbeat <= static_cast<std::int64_t>(kSenderTimeoutMs);
  destination.sampleRate = header.sampleRate;
  destination.channels = header.channels;
  destination.peakLeft = std::bit_cast<float>(atomicLoad(header.peakLeftBits));
  destination.peakRight = std::bit_cast<float>(atomicLoad(header.peakRightBits));
  return true;
}

} // namespace dhrelink
