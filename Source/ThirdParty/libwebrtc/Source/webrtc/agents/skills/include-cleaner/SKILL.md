---
name: include-cleaner
description: Runs the WebRTC include-cleaner tool (IWYU replacement) to fix headers in C++ files. Use when preparing a CL for upload, after modifying .cc or .h files, or when instructed to fix include regressions.
---

# WebRTC Include Cleaner

This skill provides instructions for using
`tools_webrtc/iwyu/apply-include-cleaner`, a tool that automatically manages C++
`#include` directives in the WebRTC codebase. It ensures that every header used
is explicitly included and that unused headers are removed.

## When to Use

- **Pre-upload**: Run this tool before uploading a CL to ensure clean includes.
- **After refactoring**: When moving code or changing dependencies, use this to
  update `#include` blocks.
- **Fixing regressions**: Use this if a presubmit or bot identifies
  include-related issues.

## Basic Usage

To run the include cleaner on specific files:

```bash
tools_webrtc/iwyu/apply-include-cleaner path/to/file.cc path/to/file.h
```

To run it on all modified files relative to the upstream branch (ideal for CL
preparation):

```bash
tools_webrtc/iwyu/apply-include-cleaner
```

Note: This is as expensive as a build for each file, so use it sparingly.

## Options

- `-p`, `--print`: Don't modify the files, just print the proposed changes.
- `-w WORK_DIR`, `--work-dir WORK_DIR`: Specify the GN work directory (default:
  `out/Default`).

## Post-Execution Steps

After running the include cleaner, it is recommended to perform the following
steps to ensure build and style consistency:

1. **Check for build errors**: The tool might occasionally make mistakes. Run a
   build to verify.
2. **Fix GN dependencies**: Use `tools_webrtc/gn_check_autofix.py` to fix any
   `deps` issues caused by include changes.
   ```bash
   tools_webrtc/gn_check_autofix.py -C out/Default
   ```
3. **Format code**: Run `git cl format` to fix any formatting issues in the
   `#include` blocks.
   ```bash
   git cl format
   ```

## Prerequisites

- The tool automatically generates `compile_commands.json` in the output
  directory if `out/Default` exists.
- `clangd` must be checked out in your `.gclient` file
  (`"checkout_clangd": True`).
