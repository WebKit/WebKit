# WebRTC Agent Infrastructure

This directory contains configuration and assets for AI agents used in the
WebRTC project.

## Directory Structure

- `prompts/`: System prompts and common prompt fragments.
- `skills/`: Tracked skills available to all users.

## Adding Skills

### Shared Skills

To add a skill that should be shared with all developers:

1. Create a new directory under `agents/skills/`.
2. Add a `SKILL.md` file in that directory with appropriate frontmatter (name
   and description).

These skills can be copied or symlinked to a skills folder your agent can use.

### Using `gemini` CLI

Alternatively, you can link skills using the `gemini` CLI:

```bash
gemini skills link --workspace agents/skills/include-cleaner
```
