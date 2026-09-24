#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace waystone {

class ByteBuffer {
 public:
  void Append(std::span<const std::byte> bytes) {
    if (bytes.empty()) return;
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  }

  [[nodiscard]] bool DiscardPrefix(std::size_t count) noexcept {
    if (count > bytes_.size()) return false;
    bytes_.erase(bytes_.begin(), bytes_.begin() + count);
    return true;
  }

  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
    return {bytes_.data(), bytes_.size()};
  }

 private:
  std::vector<std::byte> bytes_;
};

class ByteCursor {
 public:
  explicit ByteCursor(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

  [[nodiscard]] std::size_t Consumed() const noexcept { return position_; }

  [[nodiscard]] std::size_t Remaining() const noexcept { return bytes_.size() - position_; }

  [[nodiscard]] std::optional<std::span<const std::byte>> Read(std::size_t count) noexcept {
    if (count > Remaining()) return std::nullopt;

    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
  }

 private:
  std::span<const std::byte> bytes_;
  std::size_t position_ = 0;
};

}  // namespace waystone
