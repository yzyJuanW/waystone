# Byte Buffer

[中文](README.zh-CN.md)

`waystone::ByteBuffer` owns contiguous bytes for incremental input. `waystone::ByteCursor` reads a
non-owning `std::span` sequentially. The module deliberately adds only the append, prefix-discard,
and checked-read contracts needed by a future length-prefixed protocol framer; `std::vector` and
`std::span` provide the underlying storage and view mechanics.

## Quick Start

```cpp
#include <waystone/byte_buffer.hpp>

waystone::ByteBuffer buffer;
buffer.Append(input_bytes);

waystone::ByteCursor cursor(buffer.Bytes());
if (auto header = cursor.Read(4)) {
  // Interpret the bytes according to the caller's protocol.
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
add_subdirectory(path/to/byte_buffer)
target_link_libraries(my_app PRIVATE waystone::byte_buffer)
```

## API

### `ByteBuffer`

- `Append(bytes)` copies an external `std::span<const std::byte>` to the end. The input span must
  not refer to this buffer's own storage.
- `DiscardPrefix(count)` removes `count` leading bytes and returns `true`. If `count` exceeds the
  current size, it returns `false` without changing the buffer.
- `Bytes()` returns a read-only span over all stored bytes. Use `Bytes().size()` and
  `Bytes().empty()` for state queries.

### `ByteCursor`

- Construct it from `std::span<const std::byte>`; the cursor does not own or extend the source
  lifetime.
- `Read(count)` returns the next span and advances on success. It returns an empty `std::optional`
  without advancing when fewer than `count` bytes remain.
- `Read(0)` succeeds with an engaged empty span and does not advance.
- `Consumed()` and `Remaining()` report byte counts.

## Ownership, Guarantees, and Complexity

- `ByteBuffer` has the copy and move behavior of its `std::vector<std::byte>` member. Allocation
  failures from `Append` propagate and leave existing bytes unchanged.
- Any non-const operation on a buffer conservatively invalidates its existing spans and cursors.
  Destruction, move, and assignment can also invalidate them. A cursor and spans returned by
  `Read` must not outlive or be used after invalidation of their source storage.
- `Append` is linear in the appended byte count, plus existing bytes if reallocation occurs.
  `Bytes`, cursor construction, state queries, and `Read` are constant time.
  `DiscardPrefix` is linear in the bytes that remain because `std::vector` moves them forward.
- Neither type is thread-safe for concurrent access involving mutation. External synchronization
  is the caller's responsibility.

## Dependencies and Portability

- Engineering profile: `foundational-component`
- Library role: `standalone`
- Lifecycle: `experimental`
- Portability: `portable`
- Required: C++20 and its standard library
- Optional dependencies: none
- Internal Waystone dependencies: none
- Last verified: macOS 15.3.2, AppleClang 16.0.0, CMake 4.0.2

The module is header-only. When built as the top-level CMake project, it also builds the example and
framework-free test program. Source compatibility may change while the module is experimental; ABI
stability is not promised.

## Non-goals

This module does not provide custom allocators, small-buffer optimization, copy-on-write, a general
serialization framework, protocol framing, endian conversion, typed numeric reads, writable
cursors, SIMD, benchmarks, asynchronous I/O, installation, or package configuration.
