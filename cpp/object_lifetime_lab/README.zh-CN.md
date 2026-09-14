# 对象生命周期实验

[English](README.md)

一个小型、自校验的 C++20 程序，用于观察对象生命周期事件，同时避免依赖编译器消除或容器实现细节。

## 演示内容

- 局部对象按照构造顺序的逆序析构。
- 显式 copy/move construction 与 copy/move assignment 是不同操作。
- 已成功构造的局部对象会在栈展开期间析构。
- 构造函数抛异常时，已构造的成员会析构，但未完成构造的外围对象不会析构。

程序在内存中记录语义事件，并验证事件类型、对象身份与发生顺序。终端标题和排版只服务阅读，不属于测试契约。

## 构建、运行与测试

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/object_lifetime_lab
```

多配置生成器可能需要添加 `--config Debug`，并从 `build/Debug/` 运行可执行文件。

## 契约

- Profile：`learning-example`
- 可移植性：`portable`
- 语言：C++20
- 依赖：仅 C++ 标准库
- 构建依赖：CMake 3.20 或更高版本
- 兼容性：示例可以自由演进，不提供可复用 API

`moved-from` 标签是这个教学类型刻意实现的行为，不代表所有被 move 的 C++ 对象都具有相同状态或值保证。

## 限制

这个聚焦示例不涵盖 allocator、placement new、union、继承析构、协程、线程或容器失效规则；它也不是可复用追踪库、测试框架或完整的对象生命周期教程。

## 最后验证

- macOS 15.3.2（Apple Silicon）
- AppleClang 16.0.0
- CMake 4.0.2
