---
name: gn-check-autofix
description: Automatically fix GN check errors in WebRTC BUILD.gn files. Use when encountering "Include not allowed", "rtc_source_set shall not contain cc files", or when needing to clean up non-absolute dependencies.
---

# GN Check Autofix

This skill provides instructions for using `tools_webrtc/gn_check_autofix.py` to
automatically resolve common GN build configuration errors in WebRTC.

## Core Workflows

### Fix Missing Dependencies

When you see "Include not allowed" errors from `gn gen --check`, use this
workflow:

1. Run the autofix tool on your output directory:
   ```bash
   tools_webrtc/gn_check_autofix.py -C <dir>
   ```
2. The tool will:
   - Identify targets missing dependencies.
   - Add the missing `deps` to the appropriate `BUILD.gn` files.
   - Automatically run `gn format` on modified files.

### Fix rtc_source_set Violations

If you see "rtc_source_set shall not contain cc files", the tool will
automatically convert them to `rtc_library`:

1. Run the tool:
   ```bash
   tools_webrtc/gn_check_autofix.py -C <dir>
   ```

### Clean Up Dependencies

To remove all non-absolute dependencies (those not starting with `//`) from
specific `BUILD.gn` files:

```bash
tools_webrtc/gn_check_autofix.py -r path/to/BUILD.gn
```

*Note: This preserves absolute dependencies (starting with `//`) and targets
ending in `_test`, `_tests`, `_unittest`, or `_unittests`.*

## Integration with Include Cleaner

This tool is the recommended second step after running `webrtc-include-cleaner`.
While the include cleaner updates your C++ source files, `gn-check-autofix`
synchronizes your `BUILD.gn` files to match.

## Parameters

- `-C <dir>`: Path to a local build directory (e.g., `out/Default`). The tool
  internally runs `gn gen --check --error-limit=20000`.
- `-r <files>`: Remove all non-absolute dependencies from the specified files.
- `--error-limit`: Can be used to override the default error cap.

## Post-Fix Steps

After the tool runs, always verify the changes:

1. **Deduplicate**: The tool may occasionally add a dependency that is already
   present. Check the `deps` list for duplicates.
2. **Regenerate GN**: Run `gn gen <dir>` to confirm errors are resolved.
3. **Format**: The tool runs `gn format`, but a final `git cl format` is
   recommended for consistency.
4. **Review**: Check the diff to ensure dependencies were added to the correct
   targets.
