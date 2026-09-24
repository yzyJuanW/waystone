# 字节缓冲区

[English](README.md)

`waystone::ByteBuffer` 为增量输入拥有连续字节，`waystone::ByteCursor` 顺序读取 non-owning
`std::span`。该模块只增加未来 length-prefixed protocol framer 确实需要的追加、丢弃前缀和
检查型读取契约；底层存储与视图机制直接复用 `std::vector` 和 `std::span`。

## 快速开始

```cpp
#include <waystone/byte_buffer.hpp>

waystone::ByteBuffer buffer;
buffer.Append(input_bytes);

waystone::ByteCursor cursor(buffer.Bytes());
if (auto header = cursor.Read(4)) {
  // 按调用者自己的协议解释这些字节。
}
```

独立构建并测试模块：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

从另一个 CMake 项目消费：

```cmake
add_subdirectory(path/to/byte_buffer)
target_link_libraries(my_app PRIVATE waystone::byte_buffer)
```

## API

### `ByteBuffer`

- `Append(bytes)` 将外部 `std::span<const std::byte>` 复制到末尾。输入 span 不得指向该
  buffer 自身的 storage。
- `DiscardPrefix(count)` 移除开头 `count` 个字节并返回 `true`；若 `count` 超出当前 size，
  则返回 `false` 且 buffer 保持不变。
- `Bytes()` 返回覆盖全部已存字节的只读 span；使用 `Bytes().size()` 和 `Bytes().empty()`
  查询状态。

### `ByteCursor`

- 由 `std::span<const std::byte>` 构造；cursor 不拥有来源数据，也不会延长其生命周期。
- `Read(count)` 成功时返回下一段 span 并推进；剩余字节不足时返回空 `std::optional`，且
  不推进。
- `Read(0)` 成功返回 engaged empty span，且不推进。
- `Consumed()` 与 `Remaining()` 报告字节数。

## 所有权、保证与复杂度

- `ByteBuffer` 的复制和移动行为来自其 `std::vector<std::byte>` 成员。`Append` 的分配失败
  会继续向上传播，原有字节保持不变。
- 对 buffer 的任何 non-const 操作都会保守地使已有 spans 与 cursors 失效；析构、移动和赋值
  也可能使其失效。cursor 及 `Read` 返回的 spans 不得比来源 storage 活得更久，也不得在来源
  失效后继续使用。
- `Append` 对追加字节数为线性复杂度；若发生重新分配，还需搬移原有字节。`Bytes`、cursor
  构造、状态查询和 `Read` 为常数复杂度。`DiscardPrefix` 因 `std::vector` 向前搬移剩余字节，
  对剩余字节数为线性复杂度。
- 两个类型都不支持包含 mutation 的无同步并发访问；调用者负责外部同步。

## 依赖与可移植性

- 工程 profile：`foundational-component`
- Library 角色：`standalone`
- 生命周期：`experimental`
- 可移植性：`portable`
- 必需依赖：C++20 及其标准库
- 可选依赖：无
- Waystone 内部依赖：无
- 最近验证：macOS 15.3.2、AppleClang 16.0.0、CMake 4.0.2

该模块为 header-only。作为顶层 CMake 项目构建时，还会构建示例和无测试框架的测试程序。
模块处于 experimental 阶段，源码兼容性仍可能变化；不承诺 ABI 稳定性。

## 非目标

本模块不提供自定义 allocator、small-buffer optimization、copy-on-write、通用 serialization
framework、protocol framing、endian conversion、typed numeric reads、writable cursor、SIMD、
benchmark、异步 I/O、安装或 package configuration。
