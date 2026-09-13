# Architecture Decision Records

[中文](README.zh-CN.md)

Use an architecture decision record (ADR) for a choice that affects multiple modules, establishes a lasting constraint, or would otherwise make a future reader ask why the project works this way.

Name records with a sequence number and a short English slug:

```text
0001-use-cmake-for-non-trivial-cpp-modules.md
```

Use the smallest structure that preserves the reasoning:

```markdown
# Decision title

## Status

Accepted | Superseded | Deprecated

## Context

What problem, constraint, or pressure requires a decision?

## Decision

What will Waystone do?

## Consequences

What becomes easier, harder, required, or intentionally unsupported?
```

Link the relevant code, design note, or replacement ADR when applicable. Do not create an ADR for a reversible local detail that the code already explains clearly.

An ADR needs a Chinese peer only when it is explicitly marked for bilingual maintenance. When paired, the English file uses the ordinary name, the Chinese file inserts `.zh-CN` before `.md`, and both link to each other near the top.
