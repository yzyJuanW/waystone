#include <waystone/ring_buffer.hpp>

#include <iostream>

int main() {
    waystone::ring_buffer<int, 3> buffer;
    if (!buffer.try_push_back(10) || !buffer.try_push_back(20) ||
        !buffer.try_push_back(30)) {
        return 1;
    }

    while (!buffer.empty()) {
        std::cout << buffer.front() << '\n';
        buffer.pop_front();
    }
}
