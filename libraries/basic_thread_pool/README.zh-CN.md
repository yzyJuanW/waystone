# 基础线程池

[English](README.md)

`waystone::BasicThreadPool` 用固定数量的 worker 线程执行相互独立、能够自行结束的任务，避免反复创建和销毁线程。提交 callable 后会得到 `std::future`；可显式调用 `Shutdown()` 排空任务，也可由析构完成同样的关闭过程。

## 快速开始

```cpp
#include <waystone/basic_thread_pool.hpp>

waystone::BasicThreadPool pool(4);
auto answer = pool.Submit([](int value) { return value * 2; }, 21);
int result = answer.get();  // 42
pool.Shutdown();
```

独立构建和测试模块：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

从另一个 CMake 项目消费：

```cmake
add_subdirectory(path/to/basic_thread_pool)
target_link_libraries(my_app PRIVATE waystone::basic_thread_pool)
```

## API 与所有权

- `BasicThreadPool(worker_count)` 启动固定数量的 workers；数量为零时抛出 `std::invalid_argument`。线程池不可复制、不可移动。
- `Submit(F&&, Args&&...)` 保存 callable 和参数，并返回结果 future。支持 move-only callable 与参数；需要保留引用语义时使用 `std::ref`。
- 任务返回值或异常由 future 交付；任务抛出异常不会终止 worker。
- `Shutdown()` 停止接收新任务、执行完所有已接受任务并 join workers。多个外部线程可安全地并发调用，且重复调用无副作用。shutdown 开始后提交会立即抛出 `std::runtime_error`。
- 若尚未完成 shutdown，析构会执行同样的 drain shutdown。

同一线程池中的任务不得调用 `Shutdown()`，否则负责 join 的调用者会等待自身。其他线程仍在访问对象时也不得销毁线程池；对象生命周期仍由调用者负责协调。

## 并发与阻塞

`Submit()` 与 `Shutdown()` 可以并发调用；两者通过同一把锁决定任务被接受还是拒绝。已接受任务最多执行一次。共享 FIFO 队列只决定 worker 取得任务的顺序；受线程调度影响，不保证完成顺序或公平性。

任务队列在逻辑上无界。`Submit()` 可能因内部互斥锁或内存分配短暂阻塞，但线程池不提供 backpressure；调用者必须控制提交速率，避免排队任务无限增长。`Shutdown()` 和析构会等待已接受任务结束，因此任务必须能够自行终止。

若创建 worker 失败，构造函数会先停止并 join 已创建的 workers，再重新抛出原异常，不遗留后台线程。

## 依赖与可移植性

- Engineering profile：`reusable-library`
- Library role：`standalone`
- Lifecycle：`experimental`
- Portability：`portable`
- 必需：C++20、对应标准库及 CMake `Threads` package
- 可选依赖：无
- Waystone 内部依赖：无
- 最后验证环境：macOS 15.3.2、AppleClang 16.0.0、CMake 4.0.2

模块采用 header-only 交付。作为顶层 CMake 项目构建时，还会生成示例和不依赖测试框架的测试程序。模块处于 experimental 阶段，source compatibility 可能变化，不承诺 ABI 稳定性。

## 非目标

本线程池不提供取消、有界队列、优先级、延迟任务、work stealing、动态 worker 数量、协程、affinity、通用 executor interface、benchmark、安装或 package 配置。若只需执行一个独立后台操作，应优先使用 `std::async` 或直接创建线程。

## 延伸阅读

- [任务提交、线程安全与并发关闭](../../docs/notes/basic_thread_pool.md)
