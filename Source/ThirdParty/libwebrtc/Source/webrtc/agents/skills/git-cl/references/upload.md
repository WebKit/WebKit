# Non-Interactive git cl upload

This skill provides instructions for uploading CLs to Gerrit without interactive
prompts.

## Core Workflows

### Discover Associated Issue

To discover what (if any) issue (often referred to as a CL or Gerrit Change) is
associated with the current branch:

1. Run `git cl issue`:
   ```bash
   git cl issue
   ```
   This will print the issue number and URL if associated, or "No issue
   associated" otherwise.

### Handle Chained Branches

When uploading a branch that is part of a chain of local branches,
`git cl upload` may prompt to update all branches in the chain.

**Note**: A branch based directly on `origin/main` is not considered part of a
chain.

**Action**: Ask the user if they want to upload the entire chain or only the
current branch. Uploading the entire chain should be the default option
presented to the user.

#### Option 1: Upload Entire Chain (Default)

If the user prefers to upload the entire chain:

1. Run `git cl upload` without cherry-pick flags.
2. When prompted to update branches, use `send_input` to send a newline (`\n`)
   to the task to confirm.

#### Option 2: Upload Only Current Branch (Cherry-Pick Approach)

If the user prefers to upload only the current branch:

1. Use the `--cherry-pick-stacked` (or `--cp`) flag:
   ```bash
   git cl upload --cherry-pick-stacked
   ```
   This uploads only the current branch cherry-picked on its parent's last
   uploaded commit, avoiding interactive prompts for other branches.

### Avoid Editor Prompt

To avoid the editor opening for the CL description:

1. Use `--commit-description=+` to use the local commit message:
   ```bash
   git cl upload --commit-description=+
   ```
2. Or use `-T` or `--skip-title` to use the most recent commit message as the
   title.

### Upload Incremental Updates

When an issue is already associated with the branch (verify with
`git cl issue`), and you want to upload an incremental update, **strongly
prefer** using the `-m` flag. This is the most reliable way to avoid interactive
prompts for descriptions.

1. Use the `-m` flag to specify a one-line description of the update:
   ```bash
   git cl upload -m "One-line description of the update"
   ```
   This avoids opening the editor and adds the description as a message for the
   new patchset.

### Handle Presubmit Warnings

Presubmit warnings will prompt for `y/N`. In an automated environment:

1. If you are confident, answer `y` using `send_input` tool if running in
   background.
2. Or fix the warnings before uploading (e.g., add issue numbers to TODOs).

## Recommended Commands

### For Incremental Updates (Issue Already Associated)

```bash
git cl upload -d -m "One-line description of the update"
```

### For Initial Upload or Chained Branches

```bash
git cl upload --cherry-pick-stacked -d --commit-description=+
```

*Note*: Drop `--cherry-pick-stacked` if the branch is not part of a chain.

If presubmit warnings occur, be prepared to send `y` to stdin.
