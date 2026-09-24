# 从 `BasicThreadPool` 学习任务提交与并发关闭

这篇笔记以 [`waystone::BasicThreadPool`](../../libraries/basic_thread_pool/include/waystone/basic_thread_pool.hpp)
为证据，讨论一组可以迁移到其他工业 C++ 代码中的问题：怎样拥有一次延后执行的调用，怎样把返回值和异常交还给调用者，怎样保护共享状态，以及怎样让多个线程安全地关闭同一个服务。

阅读本文只需要熟悉 C++11 的 lambda、常用 STL 和基础 template 语法。文中会解释用到的现代 C++ 工具，但不会从变量、函数等基础语法开始，也不会展开完整的 C++ memory model、lock-free 或生产者—消费者模型。

代码块分为两类：

- “完整示例”可以作为独立 C++20 程序编译运行；
- “局部片段”只强调当前关系，不能脱离上下文直接编译。

文中的 C++ 版本标记表示该语言特性或标准库工具最早进入标准的版本。“示例以 C++20 编译”只是本文的统一验证环境，不表示其中每项工具都是 C++20 新增的。

已经读过本文、只想回查关键约束时，可以直接查看第 8 节“核心不变量速查”。

## 1. 先看完整数据流

线程池没有直接把调用者传入的 callable（可调用对象，例如普通函数、lambda 或重载了调用运算符的对象）放进 worker。`Submit()` 先把一次“可调用对象 + 参数 + 未来结果”转换为统一的无参任务：

```text
caller thread
  callable + arguments
      │ decay-copy / move：线程池取得默认所有权
      ▼
  bound_task: Result()
      │
      ▼
  packaged_task<Result()> ── shared state（共享状态） ── future<Result>
      │ 再包装，隐藏 Result
      ▼
  packaged_task<void()>
      │ 在 mutex 保护下进入 FIFO queue
      ▼
  worker: wait → pop → unlock → execute
      │
      └──────── value / exception ───────────────► future.get()
```

这条链上有四个关键转换：

1. function 和 arguments 默认变成线程池拥有的值，避免调用者离开作用域后悬空；
2. `std::packaged_task<Result()>` 把执行结果或异常写入 shared state；
3. `std::future<Result>` 是调用者读取同一个 shared state 的句柄；
4. 外层 `std::packaged_task<void()>` 隐藏不同的 `Result`，使 queue 只存一种类型。

最后一步不是丢掉结果。外层任务执行时会调用内层任务，内层任务仍然把 `Result` 写入与 future 相连的 shared state。

### 1.1 提交与执行之间的生命周期断层

第 1 点针对的是异步执行天然可能跨越作用域：`Submit()` 在 caller thread 中同步返回，task 却可能过一段时间才被 worker 取出。

```text
caller:  创建局部对象 → Submit() 返回 → 局部对象离开作用域并销毁
worker:                                  → 取出 task → 使用对象？
```

因此 task 执行时需要的对象必须仍然存在。最简单的默认规则是让 task 拷贝或移动这些对象并取得所有权；另一种选择是只借用 reference 或 pointer，但借用者必须另外保证对象生命周期覆盖整个 task。

先看线程池内部设计错误的反例。假如 `Submit()` 只把 callable 和参数的 reference 捕获进 queue：

```cpp
// 局部片段：错误示例，仓库中的 BasicThreadPool 没有这样实现
template <class F, class... Args>
void BadSubmit(F&& function, Args&&... args) {
  tasks_.emplace([&] { std::invoke(function, args...); });
}

pool.BadSubmit([](std::string text) { /* ... */ }, std::string{"work"});
// 若 worker 在这一完整表达式结束后才执行，两个临时对象已被销毁，引用悬空。
```

即使传入的不是临时对象，只要 caller 的局部对象先于 task 结束生命周期，同样会悬空。这个 bug 还可能表现得断断续续：worker 抢先执行时看似正常，调度稍慢时才访问已销毁对象。

当前实现按值保存 callable 和 arguments，修复的是上面这类“线程池偷偷借用”的问题。但 value ownership 不是递归的：一个值本身仍可能只是指向其他对象的非拥有句柄。例如，线程池拥有 lambda，不代表它拥有 lambda 引用捕获的对象：

```cpp
// 局部片段：线程池正确拥有 lambda，但 lambda 内部只保存 value 的 reference
std::future<int> result;
{
  int value = 42;
  result = pool.Submit([&value] { return value; });
}  // value 已销毁
result.get();  // 若 task 未在 value 销毁前完成全部访问，task 会涉及未定义行为
```

同样需要警惕的非拥有值还有 `std::reference_wrapper`、`std::string_view`、raw pointer 和 `[this]` 捕获。把它们按值复制进 task，只复制了“怎样找到原对象”，不会延长原对象的生命周期：

| 提交方式 | task 实际拥有的内容 | caller 仍须保证 |
|---|---|---|
| 不含外部借用的值，如 `int` 或 `std::string` | 对象的副本或被移动后的对象 | 无额外 lifetime 责任 |
| `std::ref(object)` | `arguments` tuple 中的 `object` reference | `object` 活到 task 结束；存在并发访问时正确同步 |
| `[&object]` | 借用 `object` 的 lambda | `object` 活到 task 结束；存在并发访问时正确同步 |
| `[this]` | 内含 `this` pointer 的 lambda | 当前对象活到 task 结束；存在并发访问时正确同步 |
| `std::string_view` 或 raw pointer | view 或 pointer 本身 | 底层存储保持有效；存在并发访问时正确同步 |

所以边界不是“异步代码绝不能使用 reference”，而是：默认由 task 拥有执行所需状态；确实需要借用时，让借用在调用点显式可见，并由 caller 承担 lifetime 和 synchronization 责任。后面的 `std::decay_t`、lambda init-capture 和 `std::ref` 小节会分别解释这两种语义怎样实现。

## 2. `Submit()` 的类型与所有权工具

### 2.1 先读懂函数声明

真实声明可以先拆成“模板参数、函数参数、返回类型”三部分：

```cpp
// 局部片段
class BasicThreadPool {
 public:
  template <class F, class... Args>
  [[nodiscard]] auto Submit(F&& function, Args&&... args)
      -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    // ...
  }
};
```

`[[nodiscard]]`（C++17）表示调用者不应随意丢弃返回值。如果忽略 `Submit()` 返回的 future，编译器通常会警告，因为调用者将无法取得结果或观察任务异常：

```cpp
// 局部片段
pool.Submit([] { return 42; });                // 可能触发 nodiscard 警告
auto result = pool.Submit([] { return 42; });  // 保留 future
```

`auto Submit(...) -> ReturnType` 是 trailing return type（尾置返回类型，C++11）写法。这里并非只能使用尾置返回类型；它只是把很长的依赖类型放到函数名和参数之后，便于先看到 `Submit()` 接收什么。

### 2.2 `F&&` 不总是 rvalue reference（右值引用，C++11）

这里的 `F&&` 和 `Args&&...` 会参与 template argument deduction（模板实参推导），因此是 forwarding references（转发引用）：

假设 callable 的具体类型是 `Callable`：

- 传入 `Callable` 类型的 lvalue（左值）`f` 时，`F` 推导为 `Callable&`。把它代回 `F&&`
  得到 `Callable& &&`，根据 reference collapsing（引用折叠）规则最终是 `Callable&`；
- 传入 `Callable` 类型的 rvalue（右值）时，`F` 推导为 `Callable`，代回后的参数类型是
  `Callable&&`。

`Callable& &&` 中同时出现了两层引用，引用折叠规则负责把它合并成一个最终引用类型。`Args&&...` 中的每个参数也分别遵循同样的推导与引用折叠规则。

这让同一个接口既能接收需要复制的 lvalue，也能接收只能移动的 rvalue。普通的、没有 template deduction 的 `Widget&&` 只是 rvalue reference，不是 forwarding reference。

`Args...` 是 template parameter pack（模板形参包），`args...` 是对应的 function parameter pack（函数形参包）。省略号允许参数数量和类型都变化：

```cpp
// 局部片段
pool.Submit([] { return 1; });
pool.Submit([](int x) { return x * 2; }, 21);
pool.Submit([](int x, std::string text) { /* ... */ }, 7, "work");
```

把推导结果直接写在调用旁边，会更容易建立直觉：

```cpp
// 局部片段
Callable callable;
int value = 42;

pool.Submit(callable, value);  // F = Callable&, Args... = int&
pool.Submit(Callable{}, 42);   // F = Callable,  Args... = int
```

### 2.3 `std::forward`（C++11）保留调用者的 value category（值类别）

forwarding reference 只负责“接住”参数。继续向下传递时，需要 `std::forward<T>`：

```cpp
// 局部片段
template <class T>
void Relay(T&& value) {
  Consume(std::forward<T>(value));
}

Widget widget;
Relay(widget);    // T = Widget&，向下继续传递 lvalue
Relay(Widget{});  // T = Widget，向下继续传递 rvalue
```

- 原参数是 lvalue，`std::forward<T>(value)` 仍是 lvalue；
- 原参数是 rvalue，它仍是 rvalue，可以选择 move constructor。

不要把 `std::forward` 理解成“更聪明的 move”。`std::move` 无条件把表达式转成 rvalue；`std::forward` 只在 forwarding template 中恢复调用者原本的 value category。

### 2.4 为什么存储时需要 `std::decay_t`（C++14）

异步任务不能只借用 `Submit()` 的局部参数。`Submit()` 返回后，参数变量 `function` 和 `args...` 就不存在了。实现使用：

```cpp
// 局部片段
std::decay_t<F>(std::forward<F>(function))
```

`std::decay` 在 C++11 就已提供；C++14 新增的 `std::decay_t<T>` 只是 `typename std::decay<T>::type` 的便捷别名。它产生适合按值存储的类型，主要效果包括：

- 去掉 reference；
- 去掉顶层 `const`/`volatile`；
- array 转成 pointer；
- function type 转成 function pointer。

几个直接的类型对照：

```cpp
// 局部片段
using A = std::decay_t<int&>;         // int
using B = std::decay_t<const int&>;   // int
using C = std::decay_t<int[3]>;       // int*
using D = std::decay_t<int(double)>;  // int (*)(double)
```

于是默认规则很明确：lvalue 被复制，rvalue 被移动，线程池拥有存入任务的对象。这个默认值语义比默默保存 reference 更安全，因为任务可能在原作用域结束后才运行。

这里的“拥有”只到 decay 后的存储值这一层，不能递归改变对象内部的 ownership。若存入的 lambda 使用 `[&value]` 捕获，线程池拥有 lambda，仍不拥有 `value`；调用者必须保证 `value` 的 lifetime，并在存在并发访问时提供同步。第 1.1 节给出了完整反例。

### 2.5 `std::ref` 是显式借用（C++11）

如果 callable 的参数必须引用原对象，可以使用 `std::ref`：

```cpp
// 局部片段
int total = 0;
auto result = pool.Submit([](int& value) { ++value; }, std::ref(total));
result.get();
```

`std::ref(total)` 返回 `std::reference_wrapper<int>`。`std::make_tuple` 会识别它并保留 reference 语义，而不是复制 `total`。

代价也必须显式承担：

- `total` 必须活到任务结束；
- 若多个线程访问 `total`，调用者必须提供同步；
- reference 不表达 ownership，不能延长 lifetime。

因此经验规则是：异步边界默认传值；只有明确需要共享同一对象，并且 lifetime 与同步都已经设计好时，才用 `std::ref` 或 reference capture。

### 2.6 `std::make_tuple`（C++11）与 `std::apply`（C++17）

参数数量不固定，不能为每个数量编写一种 task type。`std::make_tuple` 把参数集合存成一个对象，`std::apply` 再把 tuple 展开为一次调用。

先看最小用法：

```cpp
// 局部片段
auto add = [](int left, int right) { return left + right; };
auto arguments = std::make_tuple(20, 22);
int answer = std::apply(add, arguments);  // 42
```

完整示例（可独立编译运行）：

```cpp
#include <cassert>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

int main() {
  auto arguments = std::make_tuple(std::make_unique<int>(40), 2);
  auto add = [](std::unique_ptr<int> left, int right) { return *left + right; };
  assert(std::apply(std::move(add), std::move(arguments)) == 42);

  int value = 1;
  auto references = std::make_tuple(std::ref(value));
  std::apply([](int& item) { ++item; }, references);
  assert(value == 2);
}
```

第一个调用把 tuple 作为 rvalue 展开，因此其中的 `std::unique_ptr` 可以移动给 callable。第二个调用展示 `std::ref` 保留的借用语义。示例中的 `std::make_unique` 来自 C++14。

`std::apply` 最终按 `std::invoke`（C++17）的规则执行 callable，所以不仅支持普通 function 和 lambda，也支持 function object（函数对象）、member function pointer 和 data member pointer。

### 2.7 lambda init-capture（初始化捕获，C++14）与 `mutable`（C++11）

`Submit()` 使用 C++14 init-capture，在创建 lambda 的同时构造它的成员：

```cpp
// 局部片段
int value = 41;
auto read = [saved = value + 1] { return saved; };
int answer = read();  // 42
```

```cpp
// 局部片段
auto bound_task = [function = std::decay_t<F>(std::forward<F>(function)),
                   arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Result {
  return std::apply(std::move(function), std::move(arguments));
};
```

可以把 capture 想成 compiler 生成的匿名 class 的 data members。默认情况下，lambda 的 `operator()` 是 `const`，因此在函数体内只能把按值捕获的成员当作 `const` 对象访问，无法按这里需要的方式移出 move-only（只能移动、不能复制）状态。`mutable` 使 `operator()` 不再是 `const`，从而让 task 执行时可以消费保存的 function 和 arguments。

一个更直观的 `mutable` 用法是修改 lambda 自己按值捕获的状态：

```cpp
// 局部片段
auto counter = [count = 0]() mutable { return ++count; };
int first = counter();   // 1
int second = counter();  // 2
```

这也说明该 bound task 是一次性任务：它会移动并消费保存的状态，不应该被重复调用。

### 2.8 `std::invoke_result_t` 在编译期计算结果类型（C++17）

`Submit()` 的 `Result` 来自：

```cpp
// 局部片段
using Result = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
```

先看一个不带参数包的最小例子：

```cpp
// 局部片段
auto convert = [](int value) { return value + 0.5; };
using Result = std::invoke_result_t<decltype(convert), int>;  // double
```

它不是执行 function，而是询问 compiler：“按 `std::invoke` 的规则，用这些 argument types 调用这个 callable，会得到什么类型？”

- callable 返回 `int`，`Result` 是 `int`；
- callable 返回 `void`，`Result` 是 `void`；
- 调用在类型层面不成立，template substitution（模板替换）会失败，错误在编译期暴露。

工业代码中的意义是让 API 的返回类型与 callable 保持一致，而不要求调用者手写 `Result`。代价是 template error 可能较长，排查时应先单独验证“这个 callable 能否用这些 argument types 调用”。

### 2.9 `std::packaged_task` 与 `std::future`（C++11）

`std::packaged_task<R(Args...)>` 同时拥有两样东西：一个 callable，以及一个用于保存返回值或异常的 shared state。`get_future()` 返回读取这个 state 的 `std::future<R>`。

同一个 shared state 只能通过 `get_future()` 取得一次 future。当前实现先取 future，再把 `result_task` 移入外层 lambda；移动后，原变量已经不再拥有该 shared state。

完整示例（可独立编译运行）：

```cpp
#include <cassert>
#include <future>
#include <stdexcept>

int main() {
  std::packaged_task<int(int)> success([](int value) { return value * 2; });
  auto answer = success.get_future();
  success(21);
  assert(answer.get() == 42);

  std::packaged_task<int()> failure([]() -> int { throw std::runtime_error("task failed"); });
  auto error = failure.get_future();
  failure();

  try {
    static_cast<void>(error.get());
    assert(false);
  } catch (const std::runtime_error&) {
  }
}
```

重点不是“future 会开线程”——它不会。这里是调用 `packaged_task` 的线程执行 callable；future 只代表结果通道。

callable 抛出的异常由 `packaged_task` 捕获并保存，随后由 `future::get()` 重新抛出。因此 worker 不会因为普通任务异常而退出。`get()` 还会等待结果 ready；成功调用一次后，该 `std::future` 不再关联 shared state，不能再次 `get()`。需要多次读取同一结果时，应使用 `std::shared_future`。

### 2.10 为什么还要包装成 `std::packaged_task<void()>`

每次 `Submit()` 的 `Result` 可能不同：

```text
packaged_task<int()>
packaged_task<std::string()>
packaged_task<void()>
```

这些是不同类型，不能直接进入同一个 `std::queue<T>`。实现保留各自的内层 task，再用统一的外层 task 调用它：

```cpp
// 局部片段
std::packaged_task<Result()> result_task(std::move(bound_task));
auto result = result_task.get_future();

std::packaged_task<void()> queued_task([task = std::move(result_task)]() mutable { task(); });
```

queue 只关心“执行一个无参、无返回值的工作单元”，调用者仍通过先前取得的 future 得到真正结果。这是一种有目的的 type erasure（类型擦除）：隐藏 queue 不需要知道的 result type，但保留结果通道。

为什么不直接使用 `std::function<void()>`（C++11）？因为 `std::function` 要求其 target 可复制，而捕获 `std::packaged_task` 或 `std::unique_ptr` 的 lambda 是 move-only。`std::packaged_task<void()>` 自身支持 move-only ownership，正好符合 queue 的需要。C++23 的 `std::move_only_function` 是另一种可能，但本模块承诺 C++20。

## 3. 线程安全不是“加了一把锁”

本章主要使用的 `std::thread`、`std::mutex`、`std::condition_variable` 和 `std::atomic` 都在 C++11 进入标准库。后面单独标出的工具来自更新标准。

### 3.1 先区分 data race（数据竞争）与逻辑竞态

data race 指不同线程未同步地并发访问同一 memory location，至少一个访问是写入，且不存在合适的 happens-before（先行发生，即前一操作的效果保证对后一操作可见）关系。在 C++ 中，data race 会导致 undefined behavior（未定义行为）。

逻辑竞态不一定有 data race。例如每次访问都单独加锁，但“检查后执行”跨了两个临界区：

```cpp
// 局部片段：没有 data race，仍有 check-then-act race
if (!queue.empty()) {  // lock, check, unlock
  queue.pop();         // lock again; queue 可能已经被另一个线程清空
}
```

正确性需要保护一个完整的不变量，而不只是让每一行“看起来线程安全”。当前线程池把“检查 `accepting_` 并 enqueue”放在同一个临界区，就是在保护接受任务这一整体决定。

### 3.2 哪些状态由同一把 mutex 保护

`mutex_` 保护：

- `tasks_` 的 push、front 和 pop；
- `accepting_` 是否还接收任务；
- `joining_` 是否已经选出 join leader（负责执行 join 的主导调用者）；
- `joined_` 是否已经完成关闭。

`task_available_` 和 `shutdown_complete_` 不是状态；它们只负责等待和通知状态可能变化。判断“现在能不能继续”的依据必须仍然来自 mutex 保护的 predicate（条件谓词，即返回布尔结果的条件）。

### 3.3 `std::lock_guard` 与 `std::unique_lock`

`std::lock_guard` 使用最小的 RAII（资源获取即初始化，由对象 lifetime 自动释放资源）方式管理锁：构造时 lock，析构时 unlock。适合不需要中途 unlock、relock 或交给 condition variable（条件变量）的短临界区。

`std::unique_lock` 多保存一些 lock ownership（锁所有权，即它当前是否持有 mutex）状态，可以 unlock/relock，也能传给 `condition_variable::wait()`。wait 必须在睡眠时原子地释放 mutex，并在返回前重新取得 mutex，因此 worker 和 shutdown follower（等待关闭完成的跟随调用者）使用 `unique_lock`。

本文代码中的 `std::unique_lock lock(mutex)` 使用了 C++17 的 class template argument deduction（CTAD，类模板实参推导）。在 C++11/14 中需写成 `std::unique_lock<std::mutex> lock(mutex)`；`std::lock_guard` 的省略模板实参写法也同理。

经验规则不是“并发代码都用 `unique_lock`”，而是默认用更小的 `lock_guard`，只有 wait 或显式改变 lock ownership 时使用 `unique_lock`。

### 3.4 condition variable 等待的是 predicate

worker 的等待条件是：池已经关闭，或者 queue 不为空。

```cpp
// 局部片段
task_available_.wait(lock, [this] { return !accepting_ || !tasks_.empty(); });
```

带 predicate 的 wait 可以理解为：

```cpp
// 局部片段：语义示意
while (accepting_ && tasks_.empty()) {
  task_available_.wait(lock);
}
```

这个循环同时处理：

- spurious wakeup（虚假唤醒）：没有对应通知也可能醒来；
- 其他线程先消费了任务：醒来后条件可能已经再次为 false；
- 通知发生在真正进入睡眠前：先检查 mutex 保护的状态，不依赖“记住一次通知”。

完整示例（可独立编译运行）：

```cpp
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

int main() {
  std::mutex mutex;
  std::condition_variable changed;
  bool ready = false;
  int value = 0;

  std::thread consumer([&] {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return ready; });
    assert(value == 42);
  });

  {
    std::lock_guard lock(mutex);
    value = 42;
    ready = true;
  }
  changed.notify_one();
  consumer.join();
}
```

`ready` 才是状态，notification 只是提示等待者重新检查。写 `value` 和 `ready` 的线程先 unlock，consumer 随后成功 lock 同一 mutex，这个同步关系使 consumer 能看见之前的写入。

### 3.5 为什么通常在 unlock 后 notify

状态必须在持锁时修改；notification 可以在 unlock 后发送：

```cpp
// 局部片段
{
  std::lock_guard lock(mutex_);
  tasks_.push(std::move(task));
}
task_available_.notify_one();
```

这样被唤醒的 worker 更有机会直接取得 mutex，而不是立即在通知线程仍持有的 mutex 上阻塞。先 unlock 再 notify 不是正确性的唯一写法；真正的正确性来自“状态在同一 mutex 下修改和检查”。

### 3.6 为什么 task 必须在锁外执行

worker 只在锁内完成 wait、move 和 pop，然后释放锁再调用 task：

```cpp
// 局部片段
{
  std::unique_lock lock(mutex_);
  task_available_.wait(lock, predicate);
  task = std::move(tasks_.front());
  tasks_.pop();
}
task();
```

如果在锁内执行：

- 多个 workers 会被同一把锁串行化，线程池失去并行价值；
- 长任务会阻塞所有 `Submit()` 和 `Shutdown()`；
- task 若再次调用同一 pool 的 `Submit()`，会尝试获取自己尚未释放的 non-recursive mutex（不允许同一线程重复加锁的互斥量），形成死锁。

“把用户代码放在内部锁外执行”是可迁移的 library design 原则。library 无法控制 callback 做什么，所以不应在持有内部 lock 时调用它。

### 3.7 `Submit()` 与 `Shutdown()` 的接受边界

两者在同一把 mutex 下竞争：

- `Submit()` 先取得 lock，并成功 `tasks_.push()`：任务已经被接受，后续 shutdown 必须 drain（排空，即执行完已接受任务）它；
- `Shutdown()` 先取得 lock，并将 `accepting_ = false`：后续 `Submit()` 立即抛 `std::runtime_error`。

这就是 acceptance linearization point（接收操作的线性化点）：并发操作可以被视为在这一瞬间生效。真实时间上两个调用可能重叠，但共享 lock 给出一个明确的逻辑先后，不会出现“返回成功却没有入队”或“关闭后偷偷接收”的中间状态。

### 3.8 常见死锁检查

分析 concurrent component 时，可以逐项问：

1. 是否存在两把 lock 的相反获取顺序？当前 pool 只有一把内部 mutex，没有内部 lock-order cycle（锁获取顺序环）。
2. 是否持锁等待另一个可能需要同一把 lock 的线程？`Shutdown()` 在 join workers 前释放 `mutex_`，否则 worker 无法重新取锁并退出。
3. 是否持锁调用未知 user code？worker 在锁外执行 task。
4. 是否可能等待自己？pool worker 调用 `Shutdown()` 会让 join leader 尝试 join 自己，形成 self-join（线程等待自身结束），因此这种调用是禁止的。
5. object lifetime 是否与并发访问协调？另一个线程还在访问时析构 pool 仍然无效，幂等不能修复 use-after-destruction（对象销毁后继续访问）。

### 3.9 为什么不能把 bool 换成 atomic 就结束

`accepting_`、queue push 和 shutdown 决定属于同一个不变量。把 `accepting_` 单独改成 `std::atomic<bool>`，并不能让“检查 accepting + push task”成为一个原子事务，也不能保护 `std::queue`。

atomic 适合一个值本身就是完整状态、并且 memory ordering（内存序，即原子操作间的可见性与重排序约束）已经清楚的场景。mutex 更适合保护多个字段和 container operation 组成的不变量。`std::counting_semaphore`/`std::binary_semaphore` 与 `std::latch`（均为 C++20）或 bounded queue 也各自解决特定问题，不能替代这里的 lifecycle state machine（生命周期状态机）。

## 4. `Shutdown()`：从重复调用到并发幂等

### 4.1 单线程幂等还不够

最简单的幂等可能只是：

```cpp
// 局部片段：只能说明串行重复调用
if (stopped_) return;
stopped_ = true;
JoinWorkers();
```

这能处理同一线程连续调用两次，却不能处理两个线程同时通过 `if (!stopped_)`。若两者同时调用同一个 `std::thread` 对象的 `join()`，程序没有得到线程安全保证。

并发幂等的要求更强：多个外部线程可以同时调用，只有一个线程执行一次性清理，其余调用者等待或观察同一个最终状态，然后全部安全返回。

### 4.2 三个 lifecycle flags

当前实现使用三个互补状态：

| Flag | 含义 |
|---|---|
| `accepting_` | 是否还允许新任务进入 queue |
| `joining_` | 是否已经有一个调用者负责 join workers |
| `joined_` | workers 是否已经全部 join，关闭是否完成 |

它们不是三个互不相关的 bool，而是一个小型 state machine：

```text
running
  accepting=true, joining=false, joined=false
      │ 第一个 Shutdown()
      ▼
joining
  accepting=false, joining=true, joined=false
      │ workers drain + leader joins
      ▼
joined
  accepting=false, joining=true, joined=true
```

`joining_` 表达“有人正在完成”，`joined_` 表达“已经完成”。只用一个 `stopped_` 很难同时区分这两个阶段。

### 4.3 leader/follower 协议

持有 `mutex_` 时：

1. 若 `joined_`，直接返回；
2. 所有调用者都发布 `accepting_ = false`；
3. 若还没有 `joining_`，当前调用者设置它并成为唯一 join leader；
4. 否则当前调用者是 follower，在 `shutdown_complete_` 上等待 `joined_`。

leader 随后在锁外：

1. `notify_all()` 唤醒等待任务的 workers；
2. join 每个 worker；
3. 重新持锁设置 `joined_ = true`；
4. 通知所有 followers。

这里的优雅之处在于把“谁执行清理”和“谁需要知道清理完成”分开。leader 拥有一次性动作，followers 共享最终状态。

### 4.4 drain 的 worker 退出条件

shutdown 把 `accepting_` 设为 false，但 worker 不是立即退出。worker 醒来后：

- queue 非空：继续取出并执行已经接受的 task；
- queue 为空：退出 loop。

所以退出条件实质上是“已经关闭接收，并且没有剩余任务”。代码只检查 `tasks_.empty()` 也成立，是因为 worker 的 wait predicate 保证它只有在 `!accepting_ || !tasks_.empty()` 时走到该检查：运行期间 queue 空会继续等待；关闭后 queue 空才返回。

### 4.5 为什么 join 必须在 mutex 外

worker 要取得 `mutex_` 才能检查 queue 已空并返回。若 leader 持有 `mutex_` 调用 `join()`：

```text
leader: holds mutex_ → waits for worker to exit
worker: waits for mutex_ → cannot exit
```

这就是确定性的死锁。正确顺序是先在 lock 内发布 shutdown state，再 unlock、notify、join，最后重新 lock 发布完成状态。

### 4.6 析构复用与边界

destructor 调用同一个 `Shutdown()`，避免维护两套关闭逻辑。已经显式 shutdown 的对象再次析构时，会因 `joined_` 直接返回。

但以下结论不成立：

- 幂等不等于 reentrant（可重入）：pool 中的 task 调用本 pool 的 `Shutdown()` 会产生 self-join；
- 并发 `Shutdown()` 安全不等于并发 destruction 安全：对象 lifetime 结束时不能还有线程访问它；
- `noexcept` 不等于所有上下文都安全：违反 self-join 前置条件时，`std::thread::join()` 可能抛出，而异常不能逃出 `noexcept`。

### 4.7 迁移到其他关闭流程

后台服务 `Stop()`、writer `Close()` 或 connection shutdown 常能复用同一骨架：

```text
open
  │ publish: reject new work
  ▼
stopping
  │ one leader: flush / close / join
  │ followers: wait for terminal state
  ▼
stopped
```

迁移时先回答三件事：

1. 哪个状态变化关闭新工作，它的 linearization point 在哪里？
2. 哪些清理只能执行一次，由谁成为 leader？
3. followers 是立即返回，还是必须等到资源真正关闭？

不能直接照搬的地方也要明确。例如 writer 可能要求 flush error 返回给所有 callers；connection 可能有 timeout；后台服务可能支持 cooperative cancellation。模式提供的是不变量，不是固定 API。

## 5. 其他值得迁移的设计

### 5.1 constructor 部分失败：对象没构造完，destructor 不会兜底

假设第 4 个 `std::thread` 创建失败，前三个 workers 已经开始访问 pool。若 constructor 直接重新抛异常：

- `BasicThreadPool` 自身没有完成构造，它的 destructor 不会运行；
- `workers_` member 会析构；
- 其中仍 joinable 的 `std::thread` 析构会调用 `std::terminate()`。

当前实现的 catch block 先在锁下发布 `accepting_ = false`，通知并 join 已创建 workers，再 `throw;` 保留原始异常。这是 constructor rollback（构造失败回滚）：已经获得的资源必须在 constructor 内失败路径上恢复到可安全销毁的状态。

可迁移原则：先列出 constructor 每一步取得的资源，再检查任意一步失败时，已取得资源是否能由成员自动安全释放。仅仅“member destructor 会运行”并不够；`std::thread` 的安全终态要求 not-joinable（不再关联可供 `join()` 的线程）。

### 5.2 task exception isolation

如果 worker 直接调用 user callable，而异常逃出 thread entry function（线程入口函数），进程会 `std::terminate()`。当前实现让 `packaged_task` 把异常写进 shared state：

- 失败归属于对应的 future；
- worker loop 可以继续执行下一个 task；
- 调用者选择何时在 `get()` 处处理异常。

这不是忽略异常，而是把异常送到正确的 ownership boundary（所有权与错误处理的责任边界）。调用者若永远不调用 `get()`，就可能永远不观察任务失败；工业 API 需要决定这是否可接受，或是否还要 logging/monitoring。本基础库不额外增加观测系统。

### 5.3 value ownership 与 borrowed reference（借用引用）

第 1.1、2.4 和 2.5 节已经用当前 `Submit()` 展示了 ownership 与借用的具体语义。把这条原则迁移到其他异步 API 时，可以分两层检查：

1. task 是否拥有它实际存储的值，还是在接口内部悄悄保存了 caller 的 reference；
2. 若存储值本身是 reference、view、pointer、`[&]` 或 `[this]` lambda，它指向的对象是否覆盖整个执行期，并在存在并发访问时正确同步。

正面设计不是“永远不能引用”，而是默认拥有执行所需状态，并让危险的借用在调用点显式可见。

### 5.4 drain、cancel 与 force-stop 不是同义词

| 语义 | 已运行任务 | queue 中任务 | 调用者如何得知 |
|---|---|---|---|
| drain | 允许完成 | 全部执行 | future 正常得到值或任务异常 |
| cancel pending | 通常允许完成 | 不再执行 | future 需要明确 cancellation error |
| cooperative stop | task 自己检查请求 | 取决于契约 | task 返回或抛 cancellation |
| force-stop | 试图从外部中断 | 可能丢弃 | C++ `std::thread` 没有通用安全方案 |

当前 pool 选择 drain，所以 shutdown 时间没有上界：任何 task 不返回，`Shutdown()` 就一直等待。若以后需要 cancellation，必须同时设计 task 是否配合、pending future 得到什么、资源是否保持一致，不能只清空 queue。

### 5.5 FIFO acquisition 不等于 FIFO completion

共享 queue 按成功 enqueue 的顺序让 workers 取任务，但多个 producers 的“调用开始时间”不等于它们取得 mutex 并 enqueue 的顺序。即使 acquisition 顺序确定，完成顺序仍受 task 时长和 scheduler 影响：

```text
worker A gets task 1: runs 100 ms
worker B gets task 2: runs   1 ms
task 2 finishes first
```

因此 FIFO 只描述 queue 层的 acquisition order，不承诺 completion order、公平性或回调顺序。需要有序输出时，应在更高层按 sequence number 重排，或明确使用单 worker；不要从 FIFO queue 推导不存在的保证。

### 5.6 unbounded queue 与 backpressure（背压）

逻辑无界 queue 让 `Submit()` API 很简单，但 producer 长期快于 workers 时，等待任务和它们拥有的参数会持续占用内存。线程池复用了 thread，不代表资源使用自动有界。

bounded queue 必须新增一组真实语义：

- 满时阻塞、立即拒绝还是等待 timeout？
- shutdown 如何唤醒被阻塞的 producers？
- capacity 以 task 数量还是估算 memory 衡量？
- 调用者如何得到 overload signal？

这就是 backpressure：系统把消费能力不足反馈给 producer。当前实现把限流责任留给调用者；没有真实 consumer 前，不预先加入一套任意的满队列策略。

## 6. 怎样验证并发代码

### 6.1 functional test 证明契约

最有价值的测试围绕可观察状态：

- future 返回正确值；
- task exception 在 `get()` 处出现且 worker 继续工作；
- move-only callable 和 argument 可执行；
- 并发 `Shutdown()` 全部返回，accepted tasks 完成；
- shutdown 后 `Submit()` 同步拒绝；
- destructor 执行 drain。

当前模块的 [测试](../../libraries/basic_thread_pool/tests/basic_thread_pool_test.cpp) 使用 `std::promise`/`std::future`（C++11）建立确定性事件顺序，而不是猜测“sleep 多久应该够了”。

### 6.2 不用 sleep 猜并发顺序

`sleep_for(100ms)` 只能表示当前 thread 暂停过，不能证明另一个 thread 已经到达某个状态；慢机器或 scheduler 抖动会制造 flaky test（结果不稳定的测试）。

更好的方法是显式事件：

```text
task_started promise  ──► test 知道 task 已进入
release_task promise  ──► test 决定 task 何时继续
future.wait()/get()    ──► test 等待确定的完成条件
```

测试需要验证的是 happens-before 和状态转换，而不是 wall-clock（实际经过时间）运气。

### 6.3 重复运行与 ThreadSanitizer 的边界

- 重复运行可以提高偶发 race 暴露概率，但“跑一万次没失败”不是线程安全证明；
- ThreadSanitizer 能发现许多实际执行路径上的 data race，但不能证明没有未覆盖路径，也通常不能判断业务层逻辑竞态；
- 普通 functional test 能证明输入输出，却可能完全没有制造目标 interleaving（交错执行顺序）；
- invariant review（不变量审查）用来检查 lock boundary、lifetime 和 state transition，是运行测试不能替代的一层。

可靠证据来自组合：明确 contract、可审查的不变量、确定性测试、压力重复和可用时的 race detector。不要把其中任何一项描述成万能证明。

## 7. 一套可复用的阅读方法

以后阅读并发 library，可以按这个顺序：

1. 列出拥有的资源：threads、queue、callbacks、shared state；
2. 标出 borrowed references 及其 lifetime 责任；
3. 列出所有共享字段，以及各自由哪把 mutex 或 atomic 保护；
4. 找到 state transition 和 linearization points；
5. 检查 user code、blocking operation 和 join 是否发生在内部 lock 外；
6. 写出 normal、failure、shutdown 和 partial construction 路径；
7. 区分 queue order、execution order、completion order 与 fairness；
8. 为每个 claim 找到对应测试，并说明测试不能证明什么。

## 8. 核心不变量速查

前文可以压缩成下面这张查阅表。它记录当前设计依赖的不变量和常见误区，不能替代各章节对语义与边界的说明。

| 场景 | 核心不变量 | 常见错误 | 详见 |
|---|---|---|---|
| 提交异步任务 | task 默认拥有按值保存的 callable 与 arguments，但不自动拥有其中借用的对象 | 隐式保存 reference，或让 `[this]`、view、pointer 指向短命对象 | 1.1、2.4～2.7 |
| 返回结果与异常 | `packaged_task` 与 future 共享结果状态；`get()` 读取一次结果 | 丢弃 future、重复 `get()`，或让 task exception 逃出 thread entry | 2.9、2.10、5.2 |
| 保护共享状态 | 同一 mutex 保护相关状态；wait 始终重新检查 predicate | 把 notification 当成状态，或只换成 atomic 就认为安全 | 3.2～3.4、3.9 |
| 执行 user code | queue 操作在锁内，task 执行在锁外 | 持有内部 lock 执行未知代码 | 3.5、3.6 |
| 接受任务与关闭 | 检查 `accepting_` 与 enqueue 共用一个 linearization point | shutdown 开始后仍让 task 进入 queue | 3.7 |
| 并发关闭 | 唯一 leader 执行 join，followers 等待完成，join 发生在锁外 | 多个线程同时 join、持锁 join 或 self-join | 4 |
| 顺序与容量 | queue 只保证锁内的 FIFO 取出，不保证开始、完成或公平性；无界 queue 不提供 backpressure | 从 FIFO 推导完成顺序，或认为线程池天然限制内存 | 5.5、5.6 |
| 并发验证 | 用确定性事件控制 interleaving，并结合 invariant review 与 race detector | 用 sleep 或重复运行充当正确性证明 | 6 |

## 9. 自检问题

1. 为什么 `F&&` 在 `Submit()` 中是 forwarding reference，而 `Widget&&` 通常不是？
2. `std::decay_t` 让任务默认拥有什么？它为什么不能修复 lambda 的 reference capture？
3. 为什么 `std::ref` 既有用又危险？
4. `std::packaged_task<void()>` 隐藏了什么，又保留了什么？
5. condition variable 的 notification 与 predicate 分别承担什么职责？
6. 为什么 worker 必须在锁外执行 task？
7. `Submit()` 与 `Shutdown()` 并发时，任务在哪个时刻算 accepted？
8. `joining_` 和 `joined_` 为什么不能简单合并成一个 `stopped_`？
9. 为什么 join workers 时持有 `mutex_` 会死锁？
10. drain、cancel pending 和 cooperative stop 会怎样改变 future 的契约？
11. FIFO queue 为什么不保证 FIFO completion？
12. 重复运行和 ThreadSanitizer 各自能提供什么证据，又不能证明什么？

如果能够结合当前实现回答这些问题，就不只是记住了线程池 API，而是掌握了一组能迁移到 service、writer、connection 和其他异步组件的设计方法。
