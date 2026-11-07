---
name: update-pregenerated-files
description: How to regenerate/update pre-generated files (e.g., perlasm, build files) in BoringSSL.
---

# Regenerating Pre-generated Files

BoringSSL checks in a number of pre-generated build artifacts. If you modify any inputs to these files, they must be regenerated.

To regenerate these files, refer to the following documentation:

*   [gen/README.md](../../../gen/README.md) - Instructions on how to run the pregenerate tool, check if files are up-to-date, and filter the generation.
*   [BUILDING.md](../../../BUILDING.md) (specifically the "Pre-generated Files" section) - Information on required dependencies (Go, Perl, Clang) and platform-specific setup.
