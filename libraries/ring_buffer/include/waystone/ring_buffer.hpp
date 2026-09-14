#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>

namespace waystone {

template <class T, std::size_t kCapacity>
class RingBuffer {
  static_assert(kCapacity > 0, "RingBuffer capacity must be greater than zero");

 public:
  using ValueType = T;
  using SizeType = std::size_t;
  using Reference = T&;
  using ConstReference = const T&;

  [[nodiscard]] constexpr bool Empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr bool Full() const noexcept { return size_ == kCapacity; }
  [[nodiscard]] constexpr SizeType Size() const noexcept { return size_; }
  [[nodiscard]] static constexpr SizeType Capacity() noexcept { return kCapacity; }

  [[nodiscard]] bool TryPushBack(const T& value) { return TryEmplaceBack(value); }

  [[nodiscard]] bool TryPushBack(T&& value) { return TryEmplaceBack(std::move(value)); }

  template <class... Args>
  [[nodiscard]] bool TryEmplaceBack(Args&&... args) {
    if (Full()) return false;

    storage_[tail_].emplace(std::forward<Args>(args)...);
    tail_ = Next(tail_);
    ++size_;
    return true;
  }

  constexpr Reference Front() {
    assert(!Empty());
    return *storage_[head_];
  }

  constexpr ConstReference Front() const {
    assert(!Empty());
    return *storage_[head_];
  }

  constexpr Reference Back() {
    assert(!Empty());
    return *storage_[Previous(tail_)];
  }

  constexpr ConstReference Back() const {
    assert(!Empty());
    return *storage_[Previous(tail_)];
  }

  constexpr void PopFront() {
    assert(!Empty());
    storage_[head_].reset();
    head_ = Next(head_);
    --size_;
  }

  constexpr void Clear() {
    while (!Empty()) {
      PopFront();
    }
  }

 private:
  [[nodiscard]] static constexpr SizeType Next(SizeType index) noexcept {
    return index + 1 == kCapacity ? 0 : index + 1;
  }

  [[nodiscard]] static constexpr SizeType Previous(SizeType index) noexcept {
    return index == 0 ? kCapacity - 1 : index - 1;
  }

  std::array<std::optional<T>, kCapacity> storage_{};
  SizeType head_ = 0;
  SizeType tail_ = 0;
  SizeType size_ = 0;
};

}  // namespace waystone
