# Ring Buffer

[English](README.md)

`waystone::ring_buffer<T, Capacity>` 是固定容量、非线程安全的 FIFO 容器。它不会覆盖已有元素：缓冲区已满时，插入返回 `false`。

## 快速开始

```cpp
#include <waystone/ring_buffer.hpp>

waystone::ring_buffer<int, 4> buffer;
if (buffer.try_push_back(42)) {
    int value = buffer.front();
    buffer.pop_front();
}
```

独立构建并测试模块：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

在其他 CMake 项目中使用：

```cmake
add_subdirectory(path/to/ring_buffer)
target_link_libraries(my_app PRIVATE waystone::ring_buffer)
```

## API

- `try_push_back(value)` 和 `try_emplace_back(args...)` 在队尾插入。缓冲区已满时返回 `false`，并且不改变容器。元素构造异常会正常传播，容器保持原状。
- `front()` 和 `back()` 返回首尾元素的引用。
- `pop_front()` 销毁并移除首个元素。
- `clear()` 销毁全部元素。
- `empty()`、`full()`、`size()` 和 `capacity()` 查询容器状态。

对空缓冲区调用 `front()`、`back()` 或 `pop_front()` 会违反前置条件。调用前应先检查 `empty()`。

## 保证与失效规则

- 插入、访问、移除和状态查询均为常数时间；`clear()` 与当前元素数量呈线性关系。
- 插入不会使已有元素的引用失效。
- 元素被移除、容器被清空或销毁时，指向相应元素的引用失效。
- copy/move 能力由 `T` 和标准库成员自然决定。移动后的源缓冲区仍有效，但状态未指定，不保证为空。

## 依赖与可移植性

- Engineering profile：`foundational-component`
- Library role：`standalone`
- Lifecycle：`experimental`
- Portability：`portable`
- 必需：C++20 及其标准库
- 可选依赖：无
- Waystone 内部依赖：无
- 最近验证环境：macOS 15.3.2、AppleClang 16.0.0、CMake 4.0.2

模块采用 header-only 形式。仅当它作为顶层 CMake 项目构建时，才同时构建示例和无测试框架的测试程序。

## 非目标

本实现不提供线程安全或 lock-free 保证，也不支持动态容量、满容量覆盖、allocator、iterator、随机访问、安装规则或 package 配置。未来变体应按 `spsc_ring_buffer` 等真实语义命名，而不是笼统称为“高级版”。
