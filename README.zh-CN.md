# Waystone

[English](README.md)

> 工程旅途中积累的笔记、实验与工具。

Waystone 是一个长期维护的个人工程实验室，用于学习、验证想法、沉淀来之不易的知识，并逐步构建可复用的软件组件。

C++ 是主要语言，但仓库也可以涵盖 Linux、操作系统、网络、并发、IPC、嵌入式系统、算法、性能、构建系统、调试、Python 与 Shell 工具等主题。

## 原则

- 从最小但有用的实验开始。
- 在可行时让示例可复现、可独立构建。
- 只有经过真实使用证明有必要时，才提取可复用库。
- 用法贴近代码，可复用知识放入 `docs/`，设计理由保存在设计记录中。
- 明确声明平台支持，不含糊暗示可移植性。
- 有真实内容需要时再增加目录和索引，不维护空分类树。

## 仓库的生长方式

`cpp/`、`systems/`、`embedded/`、`algorithms/`、`libraries/`、`tools/`、`playground/` 等目录会在相应内容出现时创建。只有当仓库内容足以从目录或路线图中获益时，才会引入目录索引或路线图。

文档结构见 [docs/README.zh-CN.md](docs/README.zh-CN.md)，贡献和模块约定见 [CONTRIBUTING.zh-CN.md](CONTRIBUTING.zh-CN.md)。

## 当前状态

仓库现已包含可移植、固定容量的 [ring buffer](libraries/ring_buffer/README.zh-CN.md)，以及聚焦的 [C++ 对象生命周期实验](cpp/object_lifetime_lab/README.zh-CN.md)。其他目录和索引仍只在真实内容需要时建立。
