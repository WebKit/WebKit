---
name: checkout-webkit-commit
description: Use when the user wants to checkout a branch, checkout a PR, checkout a commit or move HEAD to a specific commit in the WebKit repository. The user can specify a pull requests by number or URL.
user-invocable: true
allowed-tools: Bash(git-webkit checkout:*), Bash(git webkit checkout:*), Bash(Tools/Scripts/git-webkit checkout:*), Bash(git-webkit find:*), Bash(git webkit find:*), Bash(Tools/Scripts/git-webkit find:*), Bash(git checkout:*),
---

Use `git-webkit checkout` instead of `git checkout` to check out branches, commits, or pull requests when the string provided does not match a hash, branch or tag in the repository.

## How to use

### Check out a branch or commit
```
git-webkit checkout <branch-or-ref>
```

### Check out a pull request
```
git-webkit checkout pr-<number>
```

### Check out a PR from an alternate remote
When the user provides a PR URL (e.g., `https://github.com/<org>/<repo>/pull/<number>`), extract the remote and PR number, then use `--remote`:
```
git-webkit checkout pr-<number> --remote <remote>
```

## Locating git-webkit

If `git-webkit` is not in PATH, use `Tools/Scripts/git-webkit` instead (relative to the repository root).

## Rules

1. Use `git-webkit checkout` instead of `git checkout` or `git switch` in the WebKit repository.
2. When the user provides a PR URL, parse the remote from the URL and pass it with `--remote`.
3. Convert any commit hashes in the output to WebKit identifiers using `git-webkit find` before presenting them to the user. Always display identifiers in backticks (e.g., `285301@main`).
4. Never prefix `git-webkit` or `Tools/Scripts/git-webkit` with `python3`. The script is directly executable.
