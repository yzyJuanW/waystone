# Ring Buffer

[中文](README.zh-CN.md)

`waystone::RingBuffer<T, kCapacity>` is a fixed-capacity, non-thread-safe FIFO container. It never overwrites existing elements: insertion returns `false` when the buffer is full.

## Quick Start

```cpp
#include <waystone/ring_buffer.hpp>

waystone::RingBuffer<int, 4> buffer;
if (buffer.TryPushBack(42)) {
  int value = buffer.Front();
  buffer.PopFront();
}
```

Build and test the standalone module:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Consume it from another CMake project:

```cmake
add_subdirectory(path/to/ring_buffer)
target_link_libraries(my_app PRIVATE waystone::ring_buffer)
```

## API

- `TryPushBack(value)` and `TryEmplaceBack(args...)` insert at the back. They return `false` without modifying the buffer when it is full. Exceptions from element construction propagate and leave the buffer unchanged.
- `Front()` and `Back()` return references to the first and last elements.
- `PopFront()` destroys and removes the first element.
- `Clear()` destroys all elements.
- `Empty()`, `Full()`, `Size()`, and `Capacity()` report container state.

Calling `Front()`, `Back()`, or `PopFront()` on an empty buffer violates a precondition. Check `Empty()` first.

## Guarantees and Invalidation

- Insertion, access, removal, and state queries are constant time. `Clear()` is linear in the current size.
- Insertion does not invalidate references to existing elements.
- A reference is invalidated when its element is removed, when the buffer is cleared, or when the buffer is destroyed.
- Copy and move availability follow `T` and the standard-library members. A moved-from buffer remains valid but has an unspecified state; it is not guaranteed to be empty.

## Dependencies and Portability

- Engineering profile: `foundational-component`
- Library role: `standalone`
- Lifecycle: `experimental`
- Portability: `portable`
- Required: C++20 and its standard library
- Optional dependencies: none
- Internal Waystone dependencies: none
- Last verified: macOS 15.3.2, AppleClang 16.0.0, CMake 4.0.2

The module is header-only. When built as the top-level CMake project, it also builds the example and the framework-free test program.

## Non-goals

This implementation is not thread-safe or lock-free. It does not provide dynamic capacity, overwrite-on-full behavior, allocators, iterators, random access, installation rules, or package configuration.
