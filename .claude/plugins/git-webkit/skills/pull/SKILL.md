---
name: update-webkit
description: Use when the user wants to update, pull, or sync their WebKit repository or branch.
user-invocable: true
allowed-tools: Bash(git-webkit pull:*), Bash(git webkit pull:*), Bash(Tools/Scripts/git-webkit pull:*), Bash(git-webkit find:*), Bash(git webkit find:*), Bash(Tools/Scripts/git-webkit find:*)
---

Use `git-webkit pull` to update the current repository.

## What it does

- Updates the current production branch (e.g., `main`) with the latest upstream changes
- If on a development branch, rebases it on the updated production branch
- Works safely even when there are local uncommitted edits

## How to use

```
git-webkit pull
```

No additional arguments are needed.

## Locating git-webkit

If `git-webkit` is not in PATH, use `Tools/Scripts/git-webkit` instead (relative to the repository root).

## Rules

1. Use `git-webkit pull` instead of `git pull` or `git fetch` + `git rebase` in the WebKit repository.
2. Do not pass `--force` or other flags unless the user explicitly requests them.
3. After pulling, convert any commit hashes in the output to WebKit identifiers using `git-webkit find` before presenting them to the user. Always display identifiers in backticks (e.g., `285301@main`).
4. Never prefix `git-webkit` or `Tools/Scripts/git-webkit` with `python3`. The script is directly executable.
