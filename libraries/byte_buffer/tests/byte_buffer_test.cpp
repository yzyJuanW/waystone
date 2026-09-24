#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>
#include <waystone/byte_buffer.hpp>

namespace {

constexpr std::array kFirstChunk = {std::byte{1}, std::byte{2}};
constexpr std::array kSecondChunk = {std::byte{3}, std::byte{4}, std::byte{5}};

void TestBufferAppendAndDiscard() {
  waystone::ByteBuffer buffer;
  assert(buffer.Bytes().empty());

  buffer.Append(kFirstChunk);
  buffer.Append({});
  buffer.Append(kSecondChunk);
  assert(buffer.Bytes().size() == 5);
  assert(buffer.Bytes()[0] == std::byte{1});
  assert(buffer.Bytes()[4] == std::byte{5});

  assert(buffer.DiscardPrefix(0));
  const auto before_failure = buffer.Bytes();
  assert(!buffer.DiscardPrefix(6));
  assert(buffer.Bytes().data() == before_failure.data());
  assert(buffer.Bytes().size() == before_failure.size());

  assert(buffer.DiscardPrefix(2));
  assert(buffer.Bytes().size() == 3);
  assert(buffer.Bytes()[0] == std::byte{3});
  assert(buffer.DiscardPrefix(3));
  assert(buffer.Bytes().empty());
}

void TestBufferValueSemantics() {
  static_assert(std::is_copy_constructible_v<waystone::ByteBuffer>);
  static_assert(std::is_move_constructible_v<waystone::ByteBuffer>);

  waystone::ByteBuffer original;
  original.Append(kFirstChunk);

  auto copy = original;
  assert(copy.DiscardPrefix(1));
  assert(original.Bytes().size() == 2);
  assert(copy.Bytes().size() == 1);

  auto moved = std::move(copy);
  assert(moved.Bytes().size() == 1);
  assert(moved.Bytes()[0] == std::byte{2});
}

void TestCursorReadsAndReportsState() {
  constexpr std::array bytes = {std::byte{10}, std::byte{20}, std::byte{30}};
  waystone::ByteCursor cursor(bytes);

  assert(cursor.Consumed() == 0);
  assert(cursor.Remaining() == 3);

  const auto empty = cursor.Read(0);
  assert(empty.has_value());
  assert(empty->empty());
  assert(cursor.Consumed() == 0);

  const auto first = cursor.Read(2);
  assert(first.has_value());
  assert(first->size() == 2);
  assert((*first)[0] == std::byte{10});
  assert((*first)[1] == std::byte{20});
  assert(cursor.Consumed() == 2);
  assert(cursor.Remaining() == 1);

  const auto before_failure = cursor.Consumed();
  assert(!cursor.Read(2).has_value());
  assert(cursor.Consumed() == before_failure);
  assert(cursor.Remaining() == 1);

  const auto last = cursor.Read(1);
  assert(last.has_value());
  assert((*last)[0] == std::byte{30});
  assert(cursor.Consumed() == 3);
  assert(cursor.Remaining() == 0);

  assert(!cursor.Read(1).has_value());
  assert(cursor.Consumed() == 3);
}

}  // namespace

int main() {
  TestBufferAppendAndDiscard();
  TestBufferValueSemantics();
  TestCursorReadsAndReportsState();
}
