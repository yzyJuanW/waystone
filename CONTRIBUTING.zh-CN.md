# 贡献指南

[English](CONTRIBUTING.md)

Waystone 从具体的学习与工程工作中生长。相比推测性的框架或空目录树，应优先提交小而可运行的内容。

## 选择合适的位置

- 快速、可丢弃的探索放入 `playground/`。
- 聚焦的学习示例放入相应主题目录，例如 `cpp/` 或 `systems/`。
- 只有代码已经体现复用价值，值得拥有明确 API、测试和示例时，才移入 `libraries/`。
- 首个真实文件已经准备好时，才创建对应目录。

一个有意义的实验通常应说明目的、构建命令、运行命令、预期结果、结论、依赖和平台支持。非简单的 C 或 C++ 模块应优先使用 CMake，并在可行时支持独立构建。

## 工程成熟度

选择能够诚实反映模块当前承诺的最低 profile；每一档都包含此前各档的要求：

| Profile | 最低承诺 |
|---|---|
| `experiment` | 可复现地运行，并记录目的、结果与限制。 |
| `learning-example` | 聚焦讲清一个知识点，不让生产级抽象掩盖学习重点。 |
| `reusable-library` | 明确 API、ownership、错误模型、测试、依赖、兼容性和平台支持。 |
| `foundational-component` | 进一步检查适用的生命周期和值语义、失败保证、失效规则、复杂度、边界条件和扩展约束。 |

设计或审查模块前，应识别相关组件特征，而不是机械套用全部关注点。容器和值类型可能需要分析生命周期、复制/移动、allocator、失效和复杂度；并发代码可能需要分析线程安全边界、shutdown、竞态、阻塞、取消和 memory order；系统资源、协议和平台适配层则各有 ownership、部分失败、输入、状态、超时、错误映射和原生语义问题。

应分析可信的未来变化，但不要为没有明确依据的可能性预先实现抽象。优先使用已有方案、标准库或直接设计。只有当前约束或具体的预期变化能够证明其价值时，design pattern 才是候选工具。

## Library 边界

默认将每个 `libraries/<name>` 模块视为 `standalone`：它应能脱离仓库根路径独立 configure、build 和 test。当一个库的职责就是组合底层库时，将其标记为 `composed`。依赖必须显式、浅层、单向；demo 可以依赖库，库不得反向依赖 demo。

相比过早引入 `common` 或 `utils` 依赖，应允许少量局部重复。可行时不要让实现细节和第三方类型泄漏到 public interface；构建依赖能使用 `PRIVATE` 时不要扩大为 `PUBLIC`。PImpl 可以解决已经确认的接口泄漏，但不是默认要求。

Library README 应说明 required、optional 和 internal Waystone dependencies、portability，以及它属于 `standalone` 还是 `composed`。Namespaced build target、独立复制测试、依赖层级、package manager 支持和依赖图等能力，应在出现真实使用者或依赖确实增长时再加入。

## 模块契约与维护

按需说明 guarantees 和 non-goals。兼容性必须是显式承诺：实验和学习示例可以自由变化；可复用库应说明其 source/API compatibility 预期；默认不暗示 ABI stability。

有帮助时，记录模块处于 `experimental`、`maintained`、`stable` 或 `archived`，并注明最后验证的平台和 toolchain。归档内容可以保留学习价值，但不承诺持续支持。

第三方代码应记录来源、license 和本地修改。处理不可信输入或持久化用户数据时，根据实际风险考虑校验、大小和资源限制、溢出、部分失败、取消与损坏。Release policy、SemVer、CI matrix、ABI policy、打包等机制只在真实模块需要时引入。

## 可移植性

每个非简单模块都应声明一种支持等级：

| 等级 | 含义 |
|---|---|
| `portable` | 仅使用可移植的语言或标准库能力。 |
| `portable-with-adapters` | 具有平台无关核心，并将平台实现隔离在适配层。 |
| `platform-specific` | 有意面向某个明确的平台或系统能力。 |
| `experimental-port` | 已有用于探索的移植版本，但尚不是正式支持目标。 |

涉及多个平台时，在模块 README 中加入平台矩阵：

```markdown
## Platform Support

| Platform | Status | Requirements |
|---|---|---|
| Linux | Supported | ... |
| macOS | Experimental | ... |
| Windows | Not supported | ... |
```

算法、状态机、解析和业务规则应与操作系统 API 解耦。Socket、timer、event、shared memory、file mapping 等差异应集中在平台适配层和构建配置中。避免在上层代码中散布条件编译；除非模块本身就是平台专用模块，否则公共 API 不应暴露原生 handle。

## 项目所有权命名

Waystone 自有的可复用代码必须采用相应生态中惯用的项目所有权机制：

- 可复用 C++ 代码使用顶层 `namespace waystone`，并可继续细分为 `waystone::network` 等领域 namespace。
- 可复用 Python 库使用顶层 `waystone` package，并可继续细分为 `waystone.network` 等 module。
- 对于其他语言，在首个真实的可复用模块出现时再确定具体映射。当生态自然支持时，以 `waystone` 为根或名称的一部分；不要把 C++ namespace 约定机械套用到不同的 module 或 package 系统。

独立入口、一次性实验、只消费库的示例和测试、第三方代码不需要人为包装。真实代码出现前，不创建对应语言的空脚手架。

## 文档职责

| 位置 | 主要回答的问题 |
|---|---|
| 模块 `README.md` | 这是什么、如何构建、如何使用？ |
| `docs/notes/` | 可复用的概念或知识是什么？ |
| `docs/design/` | 这个组件为什么这样设计？ |
| `docs/decisions/` | 做了什么重要选择，会产生什么后果？ |
| `docs/troubleshooting/` | 什么出了问题、根因是什么、以后如何识别？ |
| `docs/journeys/` | 哪条有序路线适合一段完整的专题学习？ |

不要在多个文件中重复同一段解释。应通过链接关联代码、笔记、设计记录、决策和排错记录。只有出现真实的多阶段学习路线时，才创建 `journeys/`。

## 语言

公共入口、项目规范和明确标记为双语的文档，以无后缀英文文件为主版本，以 `.zh-CN.md` 作为中文配对文件。配对文件应在顶部互相链接，并尽量同步更新。普通知识笔记可以采用最适合主题的语言，不要求提供翻译。文件名、代码标识和关键技术术语保持英文。

## 代码风格

C++ 格式遵循仓库根目录的 `.clang-format`：以 Google 风格为基础，使用两个空格缩进、100 列限制，并允许较短的 `if`/`else` 写在单行。现有 `.cpp` 扩展名保持不变。

现有和新增的 Waystone C++ 代码都采用 Google naming：类型、类型别名和函数使用 PascalCase，变量和参数使用 snake_case，常量和枚举值使用 `kPascalCase`，namespace 使用小写。语言规定的 `main` 和重载运算符等名称除外。

Python 每级使用两个空格缩进，不使用 Tab 字符。根目录 `.editorconfig` 记录该规则；只有真实 Python 代码出现并需要时，才增加 formatter 或 linter 配置。

## 提交前检查

- 保持改动聚焦，删除未使用的脚手架。
- 在适用时验证文档中的构建和运行命令。
- 声明平台支持与依赖。
- 只更新职责与本次改动匹配的文档。
- 保持要求配对的中英文内容一致。
