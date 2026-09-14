#include <iostream>
#include <waystone/ring_buffer.hpp>

int main() {
  waystone::RingBuffer<int, 3> buffer;
  if (!buffer.TryPushBack(10) || !buffer.TryPushBack(20) || !buffer.TryPushBack(30)) {
    return 1;
  }

  while (!buffer.Empty()) {
    std::cout << buffer.Front() << '\n';
    buffer.PopFront();
  }
}
