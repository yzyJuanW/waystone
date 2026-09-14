# Waystone

[中文](README.zh-CN.md)

> Notes, experiments, and tools collected along the engineering journey.

Waystone is a long-lived personal engineering laboratory for learning, testing ideas, documenting hard-won knowledge, and gradually building reusable software components.

C++ is the primary language, but the repository may also cover Linux, operating systems, networking, concurrency, IPC, embedded systems, algorithms, performance, build systems, debugging, Python, and shell tooling.

## Principles

- Start with the smallest useful experiment.
- Keep examples reproducible and independently buildable when practical.
- Extract reusable libraries only after real use demonstrates the need.
- Keep usage close to code, reusable knowledge in `docs/`, and rationale in design records.
- State platform support explicitly instead of implying portability.
- Add directories and indexes when real content needs them; do not maintain empty taxonomies.

## Repository Growth

Directories such as `cpp/`, `systems/`, `embedded/`, `algorithms/`, `libraries/`, `tools/`, and `playground/` will appear as the corresponding work is added. A catalog or roadmap will be introduced only when the repository has enough material to benefit from one.

For documentation structure, see [docs/README.md](docs/README.md). For contribution and module conventions, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Current Status

The repository now contains its first reusable component: the portable, fixed-capacity [ring buffer](libraries/ring_buffer/README.md). Additional directories and indexes will appear only when real content needs them.
