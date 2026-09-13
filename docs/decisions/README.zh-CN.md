# 架构决策记录

[English](README.md)

当一个选择会影响多个模块、形成长期约束，或可能让未来的读者追问“为什么项目要这样做”时，使用架构决策记录（ADR）。

文件名由序号和简短英文 slug 组成：

```text
0001-use-cmake-for-non-trivial-cpp-modules.md
```

采用足以保留设计理由的最小结构：

```markdown
# 决策标题

## Status

Accepted | Superseded | Deprecated

## Context

什么问题、约束或压力要求作出决定？

## Decision

Waystone 将采用什么做法？

## Consequences

哪些事情会变得更容易、更困难、成为必要条件，或明确不受支持？
```

适用时链接相关代码、设计说明或替代本决策的新 ADR。对于可轻易撤销、并且代码已经能清楚解释的局部细节，不要创建 ADR。

只有明确标记为双语维护的 ADR 才必须提供中文配对文件。配对时，英文文件使用普通名称，中文文件在 `.md` 前插入 `.zh-CN`，双方在顶部互相链接。
