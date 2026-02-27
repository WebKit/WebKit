# Claude Agent Skills

This directory contains **Agent Skills** for Claude Code — reusable, invocable prompts that automate common WebKit engineering tasks.

For the full specification, see: <https://agentskills.io/specification>

## What Is a Skill?

A skill is a named prompt that users can invoke with a slash command (e.g. `/add-new-MessageReceiver-class`). When invoked, Claude loads the skill's prompt text and executes it as if the user had typed out the full instructions. Skills can include reference material, multi-step procedures, and links to supporting documents.

## Directory Structure

Each skill lives in its own subdirectory under `.claude/skills/`:

```
.claude/skills/
├── README.md                          ← this file
└── <skill-name>/
    ├── SKILL.md                       ← required: the skill definition
    └── <any supporting files>         ← optional: docs, templates, etc.
```

### `SKILL.md` Format

Every skill must have a `SKILL.md` file with a YAML frontmatter block followed by the prompt body:

```markdown
---
name: skill-name
description: One-sentence description shown in the skill picker
---

The full prompt text that Claude will execute when this skill is invoked.
You can use markdown, include code blocks, reference supporting files
in the same directory, and link to documentation elsewhere in the repo.
```

**Frontmatter fields:**

| Field | Required | Description |
|---|---|---|
| `name` | Yes | The slash-command name (e.g. `my-skill` → `/my-skill`) |
| `description` | Yes | Brief description displayed to the user |

The body of `SKILL.md` is the prompt Claude receives verbatim when the skill is invoked. Write it as you would write a detailed task description: include background, step-by-step instructions, and links to reference material.

## Creating a New Skill

1. Create a subdirectory: `.claude/skills/<skill-name>/`
2. Create `SKILL.md` with the frontmatter and prompt body.
3. Add any reference documents, templates, or other supporting files to the same subdirectory.
4. Invoke it with `/<skill-name>` in a Claude Code session.

## Existing Skills

| Skill | Description |
|---|---|
| [`/add-new-MessageReceiver-class`](add-new-MessageReceiver-class/SKILL.md) | Create a new IPC MessageReceiver subclass and wire it into the WebKit build system |
