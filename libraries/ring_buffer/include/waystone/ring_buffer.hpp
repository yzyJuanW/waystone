#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>

namespace waystone {

template <class T, std::size_t Capacity>
class ring_buffer {
    static_assert(Capacity > 0, "ring_buffer capacity must be greater than zero");

public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr bool full() const noexcept { return size_ == Capacity; }
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] static constexpr size_type capacity() noexcept { return Capacity; }

    [[nodiscard]] bool try_push_back(const T& value) {
        return try_emplace_back(value);
    }

    [[nodiscard]] bool try_push_back(T&& value) {
        return try_emplace_back(std::move(value));
    }

    template <class... Args>
    [[nodiscard]] bool try_emplace_back(Args&&... args) {
        if (full()) {
            return false;
        }

        storage_[tail_].emplace(std::forward<Args>(args)...);
        tail_ = next(tail_);
        ++size_;
        return true;
    }

    constexpr reference front() {
        assert(!empty());
        return *storage_[head_];
    }

    constexpr const_reference front() const {
        assert(!empty());
        return *storage_[head_];
    }

    constexpr reference back() {
        assert(!empty());
        return *storage_[previous(tail_)];
    }

    constexpr const_reference back() const {
        assert(!empty());
        return *storage_[previous(tail_)];
    }

    constexpr void pop_front() {
        assert(!empty());
        storage_[head_].reset();
        head_ = next(head_);
        --size_;
    }

    constexpr void clear() {
        while (!empty()) {
            pop_front();
        }
    }

private:
    [[nodiscard]] static constexpr size_type next(size_type index) noexcept {
        return index + 1 == Capacity ? 0 : index + 1;
    }

    [[nodiscard]] static constexpr size_type previous(size_type index) noexcept {
        return index == 0 ? Capacity - 1 : index - 1;
    }

    std::array<std::optional<T>, Capacity> storage_{};
    size_type head_ = 0;
    size_type tail_ = 0;
    size_type size_ = 0;
};

} // namespace waystone
