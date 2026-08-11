---
name: classify-webkit-commit
description: Use when the user wants to classify a commit or understand what kind of change a commit represents in the WebKit repository.
user-invocable: true
allowed-tools: Bash(git-webkit classify:*), Bash(git webkit classify:*), Bash(Tools/Scripts/git-webkit classify:*), Bash(git-webkit find:*), Bash(git webkit find:*), Bash(Tools/Scripts/git-webkit find:*)
---

Use `git-webkit classify` to compute the classification of a given commit.

## How to use

```
git-webkit classify <commit-ref>
```

The commit ref can be a hash, identifier, branch name, or any valid git ref.

## Locating git-webkit

If `git-webkit` is not in PATH, use `Tools/Scripts/git-webkit` instead (relative to the repository root).

## Rules

1. Convert any commit hashes in the output to WebKit identifiers using `git-webkit find` before presenting them to the user. Always display identifiers in backticks (e.g., `285301@main`).
2. Never prefix `git-webkit` or `Tools/Scripts/git-webkit` with `python3`. The script is directly executable.
