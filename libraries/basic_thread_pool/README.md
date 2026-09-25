# Basic Thread Pool

[中文](README.zh-CN.md)

`waystone::BasicThreadPool` reuses a fixed number of worker threads to execute independent,
finite tasks. Submit a callable, receive a `std::future`, and explicitly drain the pool with
`Shutdown()` or let destruction do the same work.

## Quick Start

```cpp
#include <waystone/basic_thread_pool.hpp>

waystone::BasicThreadPool pool(4);
auto answer = pool.Submit([](int value) { return value * 2; }, 21);
int result = answer.get();  // 42
pool.Shutdown();
```

Build and test the standalone module:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Consume it from another CMake project:

```cmake
add_subdirectory(path/to/basic_thread_pool)
target_link_libraries(my_app PRIVATE waystone::basic_thread_pool)
```

## API and Ownership

- `BasicThreadPool(worker_count)` starts a fixed number of workers. Zero workers throws
  `std::invalid_argument`. The pool is neither copyable nor movable.
- `Submit(F&&, Args&&...)` stores the callable and ordinary arguments by value (copying lvalues and
  moving rvalues when possible) and returns a future. On execution, the stored callable and
  ordinary value arguments are passed as rvalues:
  `Submit(c, ...)` can select `operator() &&` on a copy of the lvalue `c`. Move-only callables and
  arguments are supported. Use `std::ref(c)` to borrow and invoke the original `c` as an lvalue;
  keep borrowed objects alive through task completion and synchronize concurrent access.
- To choose a ref-qualified overload, decide which object the task should call. For `operator() &`
  on an owned copy, submit a `mutable` lambda that calls its named value capture. For
  `operator() &&`, submit `c` to copy it or `std::move(c)` to move it into the task; the stored
  object is invoked as an rvalue. See the [call-site examples](../../docs/notes/basic_thread_pool.md).
- A task return value or exception is delivered by its future. A throwing task does not stop its
  worker.
- `Shutdown()` stops accepting work, drains every accepted task, and joins all workers. It is
  idempotent and safe for concurrent external callers. Submission after shutdown begins throws
  `std::runtime_error` immediately.
- Destruction performs the same drain shutdown if it has not already completed.

`Shutdown()` must not be called by a task running in the same pool because the join leader would
wait for itself. Destroying the pool while another thread is accessing it is also invalid; object
lifetime still belongs to the caller.

## Concurrency and Blocking

`Submit()` and `Shutdown()` may be called concurrently. Their shared lock determines whether a
task is accepted or rejected. Accepted tasks execute at most once. The shared FIFO queue determines
which task a worker takes next, but worker scheduling means completion order and fairness are not
guaranteed.

The queue is logically unbounded. `Submit()` may briefly block for the internal mutex or memory
allocation, but the pool provides no backpressure. Callers must control submission rate so queued
work does not grow without limit. `Shutdown()` and destruction block until accepted work finishes;
tasks therefore need to terminate on their own.

If worker creation fails, construction stops and joins workers already created before rethrowing the
original exception. No background worker is left behind.

## Dependencies and Portability

- Engineering profile: `reusable-library`
- Library role: `standalone`
- Lifecycle: `experimental`
- Portability: `portable`
- Required: C++20, its standard library, and CMake's `Threads` package
- Optional dependencies: none
- Internal Waystone dependencies: none
- Last verified: macOS 15.3.2, AppleClang 16.0.0, CMake 4.0.2

The module is header-only. When built as the top-level CMake project, it also builds the example and
framework-free test program. Source compatibility may change while the module is experimental; ABI
stability is not promised.

## Non-goals

This pool does not provide cancellation, bounded queues, priorities, delayed work, work stealing,
dynamic worker counts, coroutines, affinity, a general executor interface, benchmarks, installation,
or package configuration. For one isolated background operation, prefer `std::async` or a direct
thread.

## Further Reading

- [Task submission, thread safety, and concurrent shutdown](../../docs/notes/basic_thread_pool.md)
