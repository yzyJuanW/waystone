# 现代 C++ 中一次调用如何被接收、保存和执行

一次调用如果要稍后执行，代码需要先接收 callable（可调用对象）和参数，再保存它们，最后恢复一次调用。这几步使用的类型和值类别可能不同。把它们混为一谈，容易误判对象是否被复制、哪个 `operator()` 会运行，以及结果是否仍是引用。

读完后，你应该能沿一段延后调用代码判断：模板如何接收实参，保存的状态究竟拥有还是借用对象，执行时会选中哪个调用形式，以及结果类型是否包含引用。这套检查方法也适用于线程池以外的泛型调用代码。

本文面向熟悉 C++11 lambda、常用 STL 和基础 template 语法的读者，不要求先懂线程池。阅读顺序是：**接收与推导 → 保存与借用 → 实际调用 → 结果类型**。文中的“完整示例”可作为独立 C++20 程序编译运行；“局部片段”只说明当前语义。版本标记表示特性最早进入标准的版本，C++20 是统一的示例验证环境。

```text
调用点的表达式
    ↓ 模板推导：F、Args... 保留值类别与 const
函数形参：F&&、Args&&...
    ↓ std::forward → 构造保存状态
按值保存的 callable + tuple 中的参数（显式借用除外）
    ↓ 以选定的值类别调用
std::apply / std::invoke
    ↓
返回值类型，包括可能存在的引用
```

## 1. 接收：`F&&` 取决于实参表达式

在模板实参推导中，`template<class F> void Call(F&& function)` 的 `F&&` 是 forwarding reference（转发引用，C++11）。它能接收左值和右值，并让 `F` 记录原表达式的值类别。这里的关键是**表达式**，不是变量声明里是否写了 `&`。

| 传入的表达式 | 推导的 `F` | 引用折叠后的形参类型 |
|---|---|---|
| `Widget` 类型的左值 `widget` | `Widget&` | `Widget&` |
| `const Widget` 类型的左值 | `const Widget&` | `const Widget&` |
| `Widget{}` 右值 | `Widget` | `Widget&&` |
| `Widget*` 类型的左值指针变量 `pointer` | `Widget*&` | `Widget*&` |

左值情况下，代入 `F&&` 会出现 `Widget& &&`，reference collapsing（引用折叠）使它成为 `Widget&`。右值情况下，`F` 本身推导为 `Widget`，形参模式里的 `&&` 才使最终形参成为 `Widget&&`。具名的右值引用变量也属于左值表达式：`Widget&& saved = Widget{}; Call(saved);` 中 `F` 是 `Widget&`，而 `Call(std::move(saved))` 中 `F` 是 `Widget`。`std::move` 只转换表达式的值类别，不自行移动对象。

`Args&&...` 对参数包里的每个参数分别做同样的推导。`Args...` 是 template parameter pack，`args...` 是 function parameter pack；参数数量和类型都可变化。

**局部片段：** 下面两种写法可以声明，但都不是转发引用。

```cpp
template <class F>
void TakeRvalue(const F&& value);

template <class F>
void TakeRvalueOtherSpelling(F const&& value);
```

`const F&&` 与 `F const&&` 等价。由于 `F` 受到 `const` 修饰，这类形参不能直接绑定普通左值；不要把它们当成接收任意值类别的接口。相反，`F&&` 接到非 `const` 左值时确实绑定原对象，函数体可以通过形参修改它。接收阶段的引用关系，并不意味着后续保存阶段也会继续借用。

## 2. 保存：先转发，再决定拥有哪一层

函数体里的 `function` 和 `args` 都是**具名左值表达式**。为了按调用者原来的值类别构造保存状态，使用原推导类型做 `std::forward`（C++11）：

```cpp
// 局部片段
std::decay_t<F> stored_function(std::forward<F>(function));
auto stored_arguments = std::make_tuple(std::forward<Args>(args)...);
```

若 `F=Widget&`，`std::forward<F>(function)` 仍是左值，通常选择复制；若 `F=Widget`，它是右值，可选择移动。`std::forward` 和 `std::move` 都只是转换表达式的值类别，真正的复制或移动发生在构造保存值时。`const` 左值即使经过 `std::move`，通常也不能调用要求非 `const` 右值的 move constructor。

把上面写成 `std::forward<std::decay_t<F>>(function)` 会改变语义：当 `F=Widget&` 时，`decay_t<F>` 已经是 `Widget`，该表达式会被转换为 `Widget&&`，后面的构造可能意外地从调用者的左值移动。**转发使用推导所得的 `F`；存储类型再单独决定。**

`std::decay_t`（C++14；`std::decay` 自 C++11 起提供）适合表达默认的值存储：去掉 reference 和顶层 cv 限定，并把 array、function type 分别转成 pointer。作为类型运算，`decay_t<Widget&&>` 仍是 `Widget`。

```cpp
// 局部片段：类型对照
using A = std::decay_t<int&>;         // int
using B = std::decay_t<const int&&>;  // int
using C = std::decay_t<int[3]>;       // int*
using D = std::decay_t<int(double)>;  // int (*)(double)
```

因此，普通自由函数可存为函数指针；函数本身不会因为传入它的局部变量离开作用域而消失。但 callable 不总是自由函数：函数对象、lambda 和保存了外部指针的对象都可能借用别的对象。复制 callable 只复制它**自身持有的成员**，不会递归复制成员所指向的东西。指针变量左值可先推导成 `T*&`，按值存储后却只是一个 `T*`：保存了地址，没有拥有 `T`。

**局部反例（不可运行）：** 返回的 lambda 持有已销毁局部变量的引用，调用会涉及未定义行为。

```cpp
auto BadTask() {
  int local = 42;
  return [&local] { return local; };
}

auto task = BadTask();
// task();  // 不要执行：local 已销毁。
```

如果任务确实需要自己的 `int`，改为 `[local]` 按值捕获即可。若有意共享同一个对象，则要让借用在调用点可见，并保证其生命周期和并发访问的同步。常见借用形式包括 `[&object]`、`[this]`、raw pointer、`std::string_view` 和 `std::ref(object)`；它们都不会自动延长底层对象的生命周期。

`std::ref`（C++11）返回 `std::reference_wrapper<T>`。`std::make_tuple(std::ref(object))` 有特别规则：tuple 元素是 `T&`，因此稍后展开 tuple 仍会访问原对象。普通 `std::make_tuple(object)` 则通常保存值。两者的所有权承诺不同，不能仅凭“传入时用了左值”判断。

```cpp
// 局部片段
int count = 0;
auto owned = std::make_tuple(count);               // tuple<int>，保存副本
auto borrowed = std::make_tuple(std::ref(count));  // tuple<int&>，借用 count
```

lambda init-capture（C++14）能在创建闭包时构造保存成员；`mutable`（C++11）让闭包的 `operator()` 不再是默认的 `const`，从而允许修改或移出按值捕获的状态：

```cpp
// 局部片段
auto task = [state = std::make_unique<int>(42)]() mutable { return std::move(state); };
```

可以把这个 lambda 理解为一个匿名函数对象：capture 是成员，调用体是 `operator()`。手写 class 也能保存同样的成员并定义调用运算符；lambda 省去的是类声明，而不是所有权判断。上面的调用会消耗 `state`，因此这个 task 应按一次性调用使用。

## 3. 执行：保存的对象以什么值类别参与调用

`std::invoke(function, arg1, arg2)`（C++17）接收 callable 和逐个给出的参数，**立即执行**一次调用。对普通函数、lambda 或函数对象，它的效果相当于 `function(arg1, arg2)`；它还统一支持成员函数指针和数据成员指针。成员指针调用需要额外指出作用于哪个对象，例如 `std::invoke(&Counter::Add, counter, 1)`；直接写 `(&Counter::Add)(counter, 1)` 则不是合法的成员函数调用。

`std::apply(function, arguments)`（C++17）也立即调用，但参数已经装在 tuple 中。它按 tuple 中元素的顺序取出它们，再按 `std::invoke` 使用的 `INVOKE` 规则调用。可以先记成下面的关系；`std::get` 取出的元素是左值还是右值，取决于传给 `apply` 的 tuple 表达式和元素自身的类型。

```text
std::invoke(function, a, b)           // 参数逐个给出
std::apply(function, tuple_of_a_b)    // 参数从 tuple 展开，再按 INVOKE 规则调用
```

**完整示例（可独立编译运行）：** 同一次调用既可以逐个传参，也可以从 tuple 展开；成员指针同样适用。

```cpp
#include <cassert>
#include <functional>
#include <tuple>

struct Counter {
  int value = 40;
  int Add(int delta) { return value += delta; }
};

int main() {
  auto add = [](int left, int right) { return left + right; };
  assert(std::invoke(add, 20, 22) == 42);
  assert(std::apply(add, std::tuple{20, 22}) == 42);

  Counter counter;
  assert(std::invoke(&Counter::Add, counter, 1) == 41);
  auto arguments = std::make_tuple(std::ref(counter), 1);
  assert(std::apply(&Counter::Add, arguments) == 42);
  assert(std::invoke(&Counter::value, counter) == 42);
}
```

`std::make_tuple`（C++11）负责保存参数，`std::apply` 负责稍后展开并调用；`std::invoke` 本身不保存调用。对值元素使用右值 tuple，可把其中的 move-only 对象交给 callable；对 `T&` 元素使用右值 tuple，展开后仍是对原对象的引用。

```cpp
// 局部片段：一次性消费保存状态
return std::apply(std::move(function), std::move(arguments));
```

这里 `std::move(function)` 很重要。成员函数的 ref qualifier 可以限制调用对象的值类别：`operator() &` 由左值函数对象调用，`operator() &&` 由右值函数对象调用。它不是参数类型的一部分；普通自由函数不能在参数列表后加这种限定，其他非静态成员函数则可以使用。

**完整示例（可独立编译运行）：** 闭包按值拥有 callable 和 move-only 参数，调用时把两者作为右值交给 `std::apply`。

```cpp
#include <cassert>
#include <memory>
#include <tuple>
#include <utility>

struct Consume {
  int operator()(std::unique_ptr<int>) & { return -1; }
  int operator()(std::unique_ptr<int> value) && { return *value + 2; }
};

int main() {
  auto function = Consume{};
  auto arguments = std::make_tuple(std::make_unique<int>(40));
  auto task = [function = std::move(function), arguments = std::move(arguments)]() mutable {
    return std::apply(std::move(function), std::move(arguments));
  };
  assert(task() == 42);  // 选择 operator() &&。
}
```

这个闭包与手写的“存储 callable 和 tuple、提供一次无参调用”的 class 在作用上相同。示例只展示调用语义；重复调用已经消费状态的闭包没有这里所需的保证。

## 4. 结果：求的是哪次调用的类型

前一节的 `std::invoke` 和 `std::apply` **执行调用**。如果泛型代码需要先声明返回类型，可以用 `std::invoke_result_t<Fn, ArgTypes...>`（C++17）在编译期**查询一次假想调用的结果类型**，不运行 callable。这里的 `Fn` 和 `ArgTypes...` 要描述那次调用中的表达式类型；引用与 `const` 会影响可选重载。

**局部片段（只做类型查询，不执行调用）：** 先看普通函数，再看有两个调用重载的函数对象。

```cpp
int Add(int, int);
using PlainResult = std::invoke_result_t<decltype(&Add), int, int>;  // int

struct Choice {
  int operator()(int&) &;
  long operator()(int&&) &&;
};

int& Identity(int&);
using ReferenceResult = std::invoke_result_t<decltype(&Identity), int&>;  // int&
```

`PlainResult` 是 `int`。对 `Choice`，可沿“接收 → 保存 → 执行”对照两次结果查询：

```cpp
// 局部片段：接收的是 Choice 和 int 左值；沿用上面的 Choice 声明。
using F = Choice&;                    // F&& 接收左值后，F 的推导结果
using Arg = int&;                     // Arg&& 接收左值后，Arg 的推导结果
using StoredF = std::decay_t<F>;      // Choice
using StoredArg = std::decay_t<Arg>;  // int

using AtReception = std::invoke_result_t<F, Arg>;              // int：左值调用
using AtExecution = std::invoke_result_t<StoredF, StoredArg>;  // long：右值调用
using ViaDecltype = decltype(std::invoke(std::declval<StoredF>(),
                                         std::declval<StoredArg>()));  // long
```

第 2 节开头的写法用 `StoredF` 保存 callable，并用 `std::make_tuple` 将普通 `int` 保存为 `tuple<int>`；第 3 节的 `std::apply(std::move(function), std::move(arguments))` 再以右值执行，因此对应 `AtExecution`。直接使用入口类型 `F, Arg` 得到的是 `AtReception`，会选中另一个重载。`decay_t` 只做类型变换，不自行移动对象，也不会抹去 callable 返回类型中的引用。

`ViaDecltype` 是同一次假想调用的另一种写法：`std::declval` 只在不执行的类型查询中提供表达式。直接写 `decltype(function(arg))` 则会把具名变量当作左值，也不能覆盖成员指针的全部 `INVOKE` 形式。若类型不能组成合法调用，`std::invoke_result_t` 就没有 `type`。上述“按值保存、右值执行”的对应关系遇到左值调用或 `std::ref` 借用时，仍须按实际调用重新核对。

上例的 `Identity` 声明返回 `int&`，所以 `ReferenceResult` 仍是 `int&`。这只说明调用表达式的类型，不保证被引用对象活到结果被使用时；指向临时对象或已销毁状态的引用依然不安全。

### 让实际调用选中预期重载

`AtReception` 和 `AtExecution` 只是两种假想调用的结果类型；真正选中哪个重载，取决于最终写下的调用表达式。即使两个重载返回相同类型，检查结果类型也不能证明选中了哪一个。

**局部片段（沿用上面的 `Choice`）：** 两个任务都拥有副本；最终调用表达式决定各自选中的重载：

```cpp
Choice choice;
int value = 1;
auto task = [saved = choice, saved_value = value]() mutable {
  return saved(saved_value);  // 闭包内两个具名成员是左值，选中 operator()(int&) &。
};
auto result = std::move(task)();  // 外层按右值调用 task；内部仍按上面的表达式调用。

auto rvalue_task = [saved = choice, saved_value = value]() mutable {
  // 调用对象和参数均为右值，选中 operator()(int&&) &&。
  return std::move(saved)(std::move(saved_value));
};
auto rvalue_result = std::move(rvalue_task)();
```

第一处的 `saved(saved_value)` 选择左值调用；第二处同时将 callable 和参数转为右值，选择右值调用。外层调用 task 时的值类别不会改变闭包内具名成员的左值类别。若要借用原对象并选择其 `&` 重载，可保存引用后以左值调用，但须保证生命周期及并发访问的同步。若捕获时写 `saved = std::move(choice)`，移动的是保存阶段的对象；执行时仍需写 `std::move(saved)` 才会以右值调用它。`std::invoke_result_t` 只能查询写定的调用形式，不能替代这些表达式来指定重载。

### `std::ref` 与实际参数类型的一处边界

`std::make_tuple(std::ref(value))` 把 `reference_wrapper<T>` 解包为 tuple 中的 `T&`。若计算结果类型时仍以 `reference_wrapper<T>` 为参数，两种调用可能选中不同重载。因此，按 `decay_t<Args>` 计算的结果不一定与稍后 `std::apply` 实际选中的重载返回类型相同。若外层调用显式规定了结果类型，实际结果还可能被转换成该类型；不能把这种转换误说成 trait 预测了实际重载。

**完整示例（可独立编译运行）：** 同时观察引用返回，以及 `reference_wrapper<int>` 和 tuple 中 `int&` 选中的不同重载。

```cpp
#include <cassert>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

int& Identity(int& value) { return value; }

struct Pick {
  long operator()(std::reference_wrapper<int>) && { return 11; }
  int operator()(int&) && { return 22; }
};

int main() {
  static_assert(std::is_same_v<std::invoke_result_t<decltype(&Identity), int&>, int&>);

  int value = 0;
  int& alias = std::invoke(Identity, value);
  assert(&alias == &value);

  using Predicted = std::invoke_result_t<Pick, std::reference_wrapper<int>>;
  static_assert(std::is_same_v<Predicted, long>);
  auto arguments = std::make_tuple(std::ref(value));
  static_assert(std::is_same_v<decltype(arguments), std::tuple<int&>>);
  auto actual = std::apply(Pick{}, std::move(arguments));
  static_assert(std::is_same_v<decltype(actual), int>);
  assert(actual == 22);
}
```

这是一种少见的重载边界，不改变日常规则：先辨认保存后的真实类型，再检查它如何参与最终调用。若在异步结果通道中传递引用，仍须另外保证被引用对象在读取时存活。

## 回查一次延后调用

1. 调用点传入的是左值还是右值？`F`、`Args...` 分别推导成什么？
2. 保存时用的是原推导类型做 `std::forward`，还是意外改变了值类别？最终保存的是值、指针、view，还是显式引用？
3. callable 与保存的参数在执行时分别是左值还是右值？ref 限定和重载会选中哪一个？
4. 结果 trait 描述的是这次实际调用吗？返回引用时，被引用对象能否活到使用结束？

进一步看这些语义如何用于任务提交、结果与异常通道及并发关闭，可读[线程池学习笔记](basic_thread_pool.md)。标准依据可从 C++ 草案的[转发引用推导](https://eel.is/c++draft/temp.deduct.call)、[类型特征](https://eel.is/c++draft/meta.trans.other)、[`make_tuple`](https://eel.is/c++draft/tuple.creation)、[`invoke`](https://eel.is/c++draft/func.invoke)、[`apply`](https://eel.is/c++draft/tuple.apply) 与[成员函数 ref 限定](https://eel.is/c++draft/over.match.funcs)查起。
