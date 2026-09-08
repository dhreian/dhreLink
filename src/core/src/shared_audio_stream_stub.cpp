#include "dhrelink/shared_audio_stream.hpp"

#include <utility>

namespace dhrelink {

class StreamWriter::Impl final {};
StreamWriter::StreamWriter() : impl_(std::make_unique<Impl>()) {}
StreamWriter::~StreamWriter() = default;
StreamWriter::StreamWriter(StreamWriter&&) noexcept = default;
StreamWriter& StreamWriter::operator=(StreamWriter&&) noexcept = default;
bool StreamWriter::open(std::string_view, std::string_view) { return false; }
void StreamWriter::close() noexcept {}
bool StreamWriter::isOpen() const noexcept { return false; }
bool StreamWriter::push(const float* const*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t) noexcept { return false; }

class StreamReader::Impl final {};
StreamReader::StreamReader() : impl_(std::make_unique<Impl>()) {}
StreamReader::~StreamReader() = default;
StreamReader::StreamReader(StreamReader&&) noexcept = default;
StreamReader& StreamReader::operator=(StreamReader&&) noexcept = default;
bool StreamReader::open(std::string_view) { return false; }
void StreamReader::close() noexcept {}
bool StreamReader::isOpen() const noexcept { return false; }
ReadStatus StreamReader::read(AudioBlock&) noexcept { return ReadStatus::senderDisconnected; }
std::uint64_t StreamReader::droppedBlocks() const noexcept { return 0; }

class StreamStatusReader::Impl final {};
StreamStatusReader::StreamStatusReader() : impl_(std::make_unique<Impl>()) {}
StreamStatusReader::~StreamStatusReader() = default;
StreamStatusReader::StreamStatusReader(StreamStatusReader&&) noexcept = default;
StreamStatusReader& StreamStatusReader::operator=(StreamStatusReader&&) noexcept = default;
bool StreamStatusReader::open(std::string_view) { return false; }
void StreamStatusReader::close() noexcept {}
bool StreamStatusReader::isOpen() const noexcept { return false; }
bool StreamStatusReader::snapshot(StreamStatus&) noexcept { return false; }

} // namespace dhrelink
