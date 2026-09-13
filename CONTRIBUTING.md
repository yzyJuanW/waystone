# Contributing

[中文](CONTRIBUTING.zh-CN.md)

Waystone grows from concrete learning and engineering work. Prefer a small, runnable contribution over a speculative framework or an empty directory tree.

## Choose the Right Home

- Use `playground/` for quick, disposable investigations.
- Place focused learning examples under the relevant topic area, such as `cpp/` or `systems/`.
- Move code into `libraries/` only when it has demonstrated reuse value and deserves an explicit API, tests, and examples.
- Create a directory only when the first real file is ready to live there.

A meaningful experiment should normally explain its purpose, build command, run command, expected result, conclusion, dependencies, and platform support. Non-trivial C or C++ modules should prefer CMake and should be independently buildable when practical.

## Portability

Every non-trivial module should declare one support level:

| Level | Meaning |
|---|---|
| `portable` | Uses portable language or standard-library facilities. |
| `portable-with-adapters` | Has a platform-independent core with isolated platform adapters. |
| `platform-specific` | Intentionally targets one named platform or facility. |
| `experimental-port` | A port exists for exploration but is not yet a supported target. |

When more than one platform matters, include a platform matrix in the module README:

```markdown
## Platform Support

| Platform | Status | Requirements |
|---|---|---|
| Linux | Supported | ... |
| macOS | Experimental | ... |
| Windows | Not supported | ... |
```

Keep algorithms, state machines, parsing, and business rules independent of operating-system APIs. Concentrate sockets, timers, events, shared memory, file mapping, and similar differences in platform adapters and build configuration. Avoid scattering conditional compilation throughout higher-level code, and do not expose native handles in public APIs unless the module is intentionally platform-specific.

## Project Ownership Names

Waystone-owned reusable code must use the ecosystem's idiomatic project ownership mechanism:

- Reusable C++ code uses the top-level `namespace waystone` and may add domain namespaces such as `waystone::network`.
- Reusable Python libraries use the top-level `waystone` package and may add modules such as `waystone.network`.
- For another language, define its concrete mapping when the first real reusable module is introduced. Use `waystone` as a root or part of the name when the ecosystem supports that naturally; do not force C++ namespace conventions onto a different module or package system.

Standalone entry points, disposable experiments, examples and tests that only consume a library, and third-party code do not need artificial wrappers. Do not create language scaffolding before real code needs it.

## Documentation Responsibilities

| Location | Primary question |
|---|---|
| Module `README.md` | What is this, how is it built, and how is it used? |
| `docs/notes/` | What is the reusable concept or knowledge? |
| `docs/design/` | Why is this component designed this way? |
| `docs/decisions/` | What important choice was made, and with what consequences? |
| `docs/troubleshooting/` | What failed, why, and how can it be recognized next time? |
| `docs/journeys/` | What ordered path supports a substantial course of study? |

Do not repeat the same explanation across several files. Link related code, notes, design records, decisions, and troubleshooting entries instead. Create `journeys/` only for a real multi-step study path.

## Language

Public entry points, project conventions, and explicitly bilingual documents use English as the unsuffixed primary file and Chinese as a `.zh-CN.md` peer. Paired files must link to each other near the top and should be updated together. Ordinary knowledge notes may use the language that best supports the subject and are not required to have translations. File names, code identifiers, and key technical terms remain in English.

## Before Submitting

- Keep the change focused and remove unused scaffolding.
- Verify documented build and run commands when applicable.
- State platform support and dependencies.
- Add or update only the documentation whose responsibility matches the change.
- Keep required English and Chinese pairs aligned.
