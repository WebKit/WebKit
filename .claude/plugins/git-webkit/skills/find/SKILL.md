---
name: find-webkit-commit
description: Auto-invoke (1) to convert git commit hashes to WebKit identifiers (NNNNNN@main) and (2) to convert identifiers to commit hashes using git-webkit. Use whenever a commit hash appears in a tool result that will be shared with the user in the WebKit repository.
user-invocable: false
allowed-tools: Bash(git-webkit find:*), Bash(git webkit find:*), Bash(Tools/Scripts/git-webkit find:*), Bash(git-webkit log:*), Bash(git webkit log:*), Bash(Tools/Scripts/git-webkit log:*), Bash(git-webkit blame:*), Bash(git webkit blame:*), Bash(Tools/Scripts/git-webkit blame:*), Bash(git-webkit show:*), Bash(git webkit show:*), Bash(Tools/Scripts/git-webkit show:*)
---

When working in the WebKit repository, ALWAYS reference commits by their WebKit "identifier" rather than by hash.

## What is a commit identifier?

A commit identifier is a human-readable reference of the form:

- `<number>@main` for commits on the main branch (e.g., `285301@main`)
- `<branch-point>.<number>@<branch>` for commits on other branches (e.g., `278024.26@safari-7621-branch`)

These are more stable and readable than raw commit hashes.

## How to convert between hashes and identifiers

Use `git-webkit find` to convert in either direction:

### Hash to identifier
```
git-webkit find <hash>
```

### Identifier to hash
```
git-webkit find <identifier>
```

For example:
```
git-webkit find c8a159ededf1
git-webkit find 285301@main
```

## Prefer git-webkit over git for log, blame, and show

`git-webkit` provides wrappers that automatically display identifiers instead of hashes:

- Use `git-webkit log` instead of `git log`
- Use `git-webkit blame` instead of `git blame`
- Use `git-webkit show` instead of `git show`

These commands accept the same arguments as their git counterparts but output identifiers directly, avoiding the need to manually convert hashes.

## Locating git-webkit

If `git-webkit` is not in PATH, use `/path/to/OpenSource/Tools/Scripts/git-webkit` (only for `find` subcommand), or use `Tools/Scripts/git-webkit` from within the OpenSource checkout instead.

## Rules

1. Use `git-webkit log`, `git-webkit blame`, and `git-webkit show` instead of `git log`, `git blame`, and `git show`.
2. When you encounter a commit hash from other git commands (e.g., `git bisect`), convert it to an identifier using `git-webkit find` before presenting it to the user.
3. When the user provides a commit hash, convert it to an identifier for display.
4. When the user provides an identifier, you can use it directly with `git-webkit find` to get the hash if needed for git operations.
5. Always display identifiers in backticks (e.g., `285301@main`).
6. Never prefix `git-webkit` or `Tools/Scripts/git-webkit` with `python3`. The script is directly executable.
7. Always invoke as `git-webkit` (hyphen), never `git webkit` (space). The space form depends on git's subcommand resolution, which requires `git-webkit` to be in PATH.
8. Never use `git log --grep` to resolve WebKit identifiers to hashes. Cherry-picks and backports reference identifiers in their messages, producing wrong matches.
