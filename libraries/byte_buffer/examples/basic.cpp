#include <array>
#include <cstddef>
#include <iostream>
#include <waystone/byte_buffer.hpp>

int main() {
  constexpr std::array chunk = {std::byte{2}, std::byte{'O'}, std::byte{'K'}};

  waystone::ByteBuffer buffer;
  buffer.Append(chunk);

  waystone::ByteCursor cursor(buffer.Bytes());
  const auto length = cursor.Read(1);
  if (!length) return 1;

  const auto payload = cursor.Read(std::to_integer<std::size_t>((*length)[0]));
  if (!payload) return 1;

  for (const auto byte : *payload) std::cout << std::to_integer<char>(byte);
  std::cout << '\n';

  return buffer.DiscardPrefix(cursor.Consumed()) ? 0 : 1;
}
