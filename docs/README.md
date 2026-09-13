# Waystone Documentation

[中文](README.zh-CN.md)

Documentation grows with the code and experiments it explains. Create a category when the first useful document is ready, rather than preserving empty directories.

## Categories

- `notes/` explains reusable concepts independent of one implementation.
- `design/` records goals, constraints, alternatives, and rationale for a Waystone component.
- `decisions/` contains concise architecture decision records for choices with lasting consequences.
- `troubleshooting/` preserves symptoms, failed assumptions, investigation, root causes, fixes, and recognition cues.
- `journeys/` organizes a genuine multi-step study path and is optional.

Usage instructions belong beside the relevant code in its module README. Longer knowledge and rationale belong here. Avoid duplicating material: connect documents through links to related code, notes, designs, decisions, and troubleshooting records.

## When to Create a Document

A separate document is usually worthwhile when the knowledge is likely to be reused, spans multiple experiments, needs more than a few README paragraphs, captures a meaningful trade-off, or preserves a difficult debugging lesson. A small observation can remain in the experiment README.

## Language and Naming

English is the unsuffixed primary version for public entry points, conventions, and documents explicitly maintained as bilingual. Chinese peers use `.zh-CN.md`, and both versions link to each other near the top. Ordinary notes may use the language most natural for the subject. File names and key technical terms remain in English.

See [decisions/README.md](decisions/README.md) for the decision-record format.
