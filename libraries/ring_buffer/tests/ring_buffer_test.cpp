#include <waystone/ring_buffer.hpp>

#include <cassert>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {

struct NonDefault {
  explicit NonDefault(int initial) : value(initial) {}
  int value;
};

struct Tracked {
  explicit Tracked(int initial) : value(initial) { ++alive; }
  Tracked(const Tracked& other) : value(other.value) { ++alive; }
  Tracked(Tracked&& other) noexcept : value(other.value) { ++alive; }
  ~Tracked() { --alive; }

  static inline int alive = 0;
  int value;
};

struct ThrowingValue {
  explicit ThrowingValue(int initial) : value(initial) {
    if (initial < 0) throw std::runtime_error("construction failed");
  }

  int value;
};

void TestFifoAndWrapAround() {
  waystone::RingBuffer<int, 3> buffer;

  assert(buffer.Empty());
  assert(buffer.Capacity() == 3);
  assert(buffer.TryPushBack(1));
  assert(buffer.TryPushBack(2));
  assert(buffer.TryPushBack(3));
  assert(buffer.Full());
  assert(!buffer.TryPushBack(4));
  assert(buffer.Front() == 1);
  assert(buffer.Back() == 3);

  buffer.PopFront();
  int* second_address = &buffer.Front();
  assert(buffer.TryPushBack(4));
  assert(buffer.Front() == 2);
  assert(buffer.Back() == 4);
  assert(second_address == &buffer.Front());

  buffer.PopFront();
  buffer.PopFront();
  assert(buffer.Front() == 4);
  buffer.PopFront();
  assert(buffer.Empty());

  for (int round = 0; round < 5; ++round) {
    assert(buffer.TryPushBack(round));
    assert(buffer.Front() == round);
    buffer.PopFront();
  }
}

void TestLifetimeAndElementTypes() {
  waystone::RingBuffer<NonDefault, 2> values;
  assert(values.TryEmplaceBack(7));
  assert(values.Front().value == 7);

  waystone::RingBuffer<std::unique_ptr<int>, 1> pointers;
  assert(pointers.TryPushBack(std::make_unique<int>(9)));
  assert(*pointers.Front() == 9);

  {
    waystone::RingBuffer<Tracked, 2> tracked_values;
    assert(tracked_values.TryEmplaceBack(1));
    assert(tracked_values.TryEmplaceBack(2));
    assert(Tracked::alive == 2);
    tracked_values.PopFront();
    assert(Tracked::alive == 1);
    tracked_values.Clear();
    assert(Tracked::alive == 0);
  }
  assert(Tracked::alive == 0);
}

void TestFailedConstructionPreservesState() {
  waystone::RingBuffer<ThrowingValue, 2> buffer;
  assert(buffer.TryEmplaceBack(5));

  try {
    static_cast<void>(buffer.TryEmplaceBack(-1));
    assert(false);
  } catch (const std::runtime_error&) {
  }

  assert(buffer.Size() == 1);
  assert(buffer.Front().value == 5);
  assert(buffer.Back().value == 5);
  assert(buffer.TryEmplaceBack(6));
}

void TestValueSemanticsAndConstAccess() {
  using CopyableBuffer = waystone::RingBuffer<int, 2>;
  using MoveOnlyBuffer = waystone::RingBuffer<std::unique_ptr<int>, 2>;

  static_assert(std::is_copy_constructible_v<CopyableBuffer>);
  static_assert(std::is_move_constructible_v<CopyableBuffer>);
  static_assert(!std::is_copy_constructible_v<MoveOnlyBuffer>);
  static_assert(std::is_move_constructible_v<MoveOnlyBuffer>);

  CopyableBuffer original;
  assert(original.TryPushBack(11));
  const CopyableBuffer copied = original;
  assert(copied.Front() == 11);
  assert(copied.Back() == 11);

  CopyableBuffer moved = std::move(original);
  assert(moved.Front() == 11);
}

}  // namespace

int main() {
  TestFifoAndWrapAround();
  TestLifetimeAndElementTypes();
  TestFailedConstructionPreservesState();
  TestValueSemanticsAndConstAccess();
}
