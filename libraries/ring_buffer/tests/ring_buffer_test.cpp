#include <waystone/ring_buffer.hpp>

#include <cassert>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {

struct non_default {
    explicit non_default(int initial) : value(initial) {}
    int value;
};

struct tracked {
    explicit tracked(int initial) : value(initial) { ++alive; }
    tracked(const tracked& other) : value(other.value) { ++alive; }
    tracked(tracked&& other) noexcept : value(other.value) { ++alive; }
    ~tracked() { --alive; }

    static inline int alive = 0;
    int value;
};

struct throwing_value {
    explicit throwing_value(int initial) : value(initial) {
        if (initial < 0) {
            throw std::runtime_error("construction failed");
        }
    }

    int value;
};

void test_fifo_and_wrap_around() {
    waystone::ring_buffer<int, 3> buffer;

    assert(buffer.empty());
    assert(buffer.capacity() == 3);
    assert(buffer.try_push_back(1));
    assert(buffer.try_push_back(2));
    assert(buffer.try_push_back(3));
    assert(buffer.full());
    assert(!buffer.try_push_back(4));
    assert(buffer.front() == 1);
    assert(buffer.back() == 3);

    buffer.pop_front();
    int* second_address = &buffer.front();
    assert(buffer.try_push_back(4));
    assert(buffer.front() == 2);
    assert(buffer.back() == 4);
    assert(second_address == &buffer.front());

    buffer.pop_front();
    buffer.pop_front();
    assert(buffer.front() == 4);
    buffer.pop_front();
    assert(buffer.empty());

    for (int round = 0; round < 5; ++round) {
        assert(buffer.try_push_back(round));
        assert(buffer.front() == round);
        buffer.pop_front();
    }
}

void test_lifetime_and_element_types() {
    waystone::ring_buffer<non_default, 2> values;
    assert(values.try_emplace_back(7));
    assert(values.front().value == 7);

    waystone::ring_buffer<std::unique_ptr<int>, 1> pointers;
    assert(pointers.try_push_back(std::make_unique<int>(9)));
    assert(*pointers.front() == 9);

    {
        waystone::ring_buffer<tracked, 2> tracked_values;
        assert(tracked_values.try_emplace_back(1));
        assert(tracked_values.try_emplace_back(2));
        assert(tracked::alive == 2);
        tracked_values.pop_front();
        assert(tracked::alive == 1);
        tracked_values.clear();
        assert(tracked::alive == 0);
    }
    assert(tracked::alive == 0);
}

void test_failed_construction_preserves_state() {
    waystone::ring_buffer<throwing_value, 2> buffer;
    assert(buffer.try_emplace_back(5));

    try {
        static_cast<void>(buffer.try_emplace_back(-1));
        assert(false);
    } catch (const std::runtime_error&) {
    }

    assert(buffer.size() == 1);
    assert(buffer.front().value == 5);
    assert(buffer.back().value == 5);
    assert(buffer.try_emplace_back(6));
}

void test_value_semantics_and_const_access() {
    using copyable_buffer = waystone::ring_buffer<int, 2>;
    using move_only_buffer = waystone::ring_buffer<std::unique_ptr<int>, 2>;

    static_assert(std::is_copy_constructible_v<copyable_buffer>);
    static_assert(std::is_move_constructible_v<copyable_buffer>);
    static_assert(!std::is_copy_constructible_v<move_only_buffer>);
    static_assert(std::is_move_constructible_v<move_only_buffer>);

    copyable_buffer original;
    assert(original.try_push_back(11));
    const copyable_buffer copied = original;
    assert(copied.front() == 11);
    assert(copied.back() == 11);

    copyable_buffer moved = std::move(original);
    assert(moved.front() == 11);
}

} // namespace

int main() {
    test_fifo_and_wrap_around();
    test_lifetime_and_element_types();
    test_failed_construction_preserves_state();
    test_value_semantics_and_const_access();
}
