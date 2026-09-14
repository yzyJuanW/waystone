# Ring Buffer

[中文](README.zh-CN.md)

`waystone::ring_buffer<T, Capacity>` is a fixed-capacity, non-thread-safe FIFO container. It never overwrites existing elements: insertion returns `false` when the buffer is full.

## Quick Start

```cpp
#include <waystone/ring_buffer.hpp>

waystone::ring_buffer<int, 4> buffer;
if (buffer.try_push_back(42)) {
    int value = buffer.front();
    buffer.pop_front();
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

- `try_push_back(value)` and `try_emplace_back(args...)` insert at the back. They return `false` without modifying the buffer when it is full. Exceptions from element construction propagate and leave the buffer unchanged.
- `front()` and `back()` return references to the first and last elements.
- `pop_front()` destroys and removes the first element.
- `clear()` destroys all elements.
- `empty()`, `full()`, `size()`, and `capacity()` report container state.

Calling `front()`, `back()`, or `pop_front()` on an empty buffer violates a precondition. Check `empty()` first.

## Guarantees and Invalidation

- Insertion, access, removal, and state queries are constant time. `clear()` is linear in the current size.
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

This implementation is not thread-safe or lock-free. It does not provide dynamic capacity, overwrite-on-full behavior, allocators, iterators, random access, installation rules, or package configuration. Future variants should be named for their real semantics, such as `spsc_ring_buffer`, rather than called “advanced.”
