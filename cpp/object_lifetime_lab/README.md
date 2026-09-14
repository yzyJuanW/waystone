# Object Lifetime Lab

[中文](README.zh-CN.md)

A small, self-checking C++20 program that makes object lifetime events visible without relying on compiler elision or container implementation details.

## What It Demonstrates

- Local objects are destroyed in reverse construction order.
- Explicit copy/move construction and copy/move assignment are distinct operations.
- Successfully constructed local objects are destroyed during stack unwinding.
- When a constructor throws, already-constructed members are destroyed, but the incomplete enclosing object is not.

The program records semantic events in memory and verifies their type, object identity, and order. Console headings and formatting are only for readers and are not part of the test contract.

## Build, Run, and Test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/object_lifetime_lab
```

Multi-config generators may require `--config Debug` and place the executable under `build/Debug/`.

## Contract

- Profile: `learning-example`
- Portability: `portable`
- Language: C++20
- Dependencies: C++ standard library only
- Build dependency: CMake 3.20 or newer
- Compatibility: the example may change freely; it does not expose a reusable API

The `moved-from` label is behavior deliberately implemented by this teaching type. It is not a general guarantee about the state or value of every moved-from C++ object.

## Limits

This focused example does not cover allocators, placement new, unions, inheritance destruction, coroutines, threads, or container invalidation. It is not a reusable tracing library, test framework, or complete object-lifetime tutorial.

## Last Verified

- macOS 15.3.2 (Apple Silicon)
- AppleClang 16.0.0
- CMake 4.0.2
