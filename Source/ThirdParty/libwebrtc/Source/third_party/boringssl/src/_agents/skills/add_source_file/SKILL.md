---
name: add-source-file
description: How to add a new source file to the BoringSSL build.
---

# Adding a Source File to the Build

To add a new source file (C/C++, header, or assembly) to BoringSSL, you must update the build configuration and regenerate the build files.

## Steps

1.  **Update `build.json`**:
    *   Add the new file to the correct section (e.g., `srcs`, `hdrs`, `internal_hdrs`) under the appropriate target in [build.json](../../../build.json).
    *   For the schema of `build.json`, refer to the Go structs in [util/pregenerate/build.go](../../../util/pregenerate/build.go).

2.  **Update Pregenerated Files**:
    *   After modifying `build.json`, you must regenerate the build files.
    *   Refer to [gen/README.md](../../../gen/README.md) or use the [update_pregenerated_files](../update_pregenerated_files/SKILL.md) skill to update the pre-generated files.
