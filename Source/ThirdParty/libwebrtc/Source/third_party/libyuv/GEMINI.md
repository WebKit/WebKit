# Gemini Project Context: libyuv Row Functions

This file provides context for the core row-processing architecture of libyuv. Use these guidelines when refactoring, reviewing, or generating code within the `row_*.cc` files.

## Architectural Overview

Libyuv uses a dispatch system where high-level conversion functions call optimized "Row" functions. These functions are categorized by SIMD architecture and compiler compatibility.

## Source File Map

### x86 Architectures (32-bit and 64-bit)

*   **row_gcc.cc**: **Master copy.** Contains inline assembly in GCC syntax for GCC and Clang. Supports AVX, and AVX512. AVX512 implementations are strictly for 64-bit targets.
*   **row_win.cc**: Derivative of `row_gcc.cc`. Contains C++ intrinsics specifically for Visual C++ (MSVC). Can be tested with Clang using `-DLIBYUV_ENABLE_ROWWIN`.
*   **Note**: Use either `row_gcc` or `row_win`, never both.

### ARM Architectures

*   **row_neon.cc**: 32-bit ARM. Written entirely in inline assembly for GCC/Clang.
*   **row_neon64.cc**: 64-bit ARM (AArch64). Written entirely in inline assembly for GCC/Clang.
*   **row_sve.cc**: ARMv9 Scalable Vector Extensions (SVE).
*   **row_sme.cc**: ARMv9 Scalable Matrix Extension (SME) and Streaming SVE (SSVE).

### Other Architectures

*   **row_rvv.cc**: RISC-V Vector (RVV). Implemented using intrinsics. Optimized for SiFive X280.
*   **row_lsx.cc / row_lasx.cc**: Loongarch MIPS-like extensions.

### Utility and Fallbacks

*   **row_common.cc**: Portable C/C++ versions. This is the reference implementation.
*   **row_any.cc**: Handles "remainder" pixels for widths not multiples of SIMD register size. Used for x86, NEON, and MIPS. Not required for SVE, SME, or RVV due to hardware-level masking.

## Coding Guidelines

1.  **AVX512 Logic**: AVX512 row functions are strictly enabled for **64-bit x86 only**.
2.  **Feature Macros**: Use the `HAS_` macros in `include/libyuv/row.h` to enable or disable specific AVX512 versions.

## Changelist (CL) & Commit Guidelines

When generating descriptions, follow the Chromium/Google standard format. Wrap commit message text at 72 characters

### Format Example:

\[libyuv] Optimized ARGBToRGB24 for AVX2

Detailed technical explanation of the change. Describe the SIMD
optimization logic and any platform-specific constraints. Mention
if this impacts specific row files like row_gcc.cc.

Test: libyuv_unittest --gunit_filter=*ARGBToRGB24*
Bug: libyuv:12345, b/67890
