/*
 *  Copyright 2023 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2023 SiFive, Inc. All rights reserved.
 *
 * Contributed by Darren Hsieh <darren.hsieh@sifive.com>
 * Contributed by Bruce Lai <bruce.lai@sifive.com>
 */

#include "libyuv/row.h"

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for RVV (RISC-V Vector extension)
#if !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
#include <assert.h>

// vsetvli/vsetivli write the vl and vtype CSRs. Only "vl" is declared, and
// only for GCC; clang historically rejected it. The "vtype" register is
// intentionally not declared, and whether it should be is unresolved.
// TODO(fbarchard): consider adding "vtype" (and "vl" for clang) to the
// clobber list once confirmed with compiler maintainers.
#ifndef RVV_VL_CLOBBER
#ifdef __clang__
#define RVV_VL_CLOBBER
#else
#define RVV_VL_CLOBBER "vl",
#endif
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Fill YUV -> RGB conversion constants into registers
// NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
// register) is set to round-to-nearest-up mode(0).
#define YUVTORGB_SETUP                   \
  "csrwi       vxrm, 0               \n" \
  "lbu         %[ub], 0(%[yuvconst]) \n" \
  "lbu         %[vr], 1(%[yuvconst]) \n" \
  "lbu         %[ug], 2(%[yuvconst]) \n" \
  "lbu         %[vg], 3(%[yuvconst]) \n" \
  "lhu         %[yg], 16(%[yuvconst])\n" \
  "lh          %[bb], 18(%[yuvconst])\n" \
  "lh          %[bg], 20(%[yuvconst])\n" \
  "lh          %[br], 22(%[yuvconst])\n" \
  "addi        %[bb], %[bb], 32      \n" \
  "addi        %[bg], %[bg], -32     \n" \
  "addi        %[br], %[br], 32      \n" \
  "li          %[k0101], 0x0101      \n"

// Fill YUV -> AR30 conversion constants into registers
#define YUVTORGB_SETUP_AR30              \
  "csrwi       vxrm, 0               \n" \
  "lbu         %[ub], 0(%[yuvconst]) \n" \
  "lbu         %[vr], 1(%[yuvconst]) \n" \
  "lbu         %[ug], 2(%[yuvconst]) \n" \
  "lbu         %[vg], 3(%[yuvconst]) \n" \
  "lhu         %[yg], 16(%[yuvconst])\n" \
  "lh          %[bb], 18(%[yuvconst])\n" \
  "lh          %[bg], 20(%[yuvconst])\n" \
  "lh          %[br], 22(%[yuvconst])\n" \
  "addi        %[bb], %[bb], 24      \n" \
  "addi        %[bg], %[bg], -24     \n" \
  "addi        %[br], %[br], 24      \n" \
  "li          %[k0101], 0x0101      \n"

// Read [2*VLEN/8] Y, [VLEN/8] U and [VLEN/8] V from 422
#define READYUV422                               \
  "addi        %[vl], %[w], 1                \n" \
  "srli        %[vl], %[vl], 1               \n" \
  "vsetvli     %[vl], %[vl], e8, m1, ta, ma  \n" \
  "vle8.v      v0, (%[src_u])                \n" \
  "vle8.v      v1, (%[src_v])                \n" \
  "vwcvtu.x.x.v v2, v0                       \n" \
  "vwcvtu.x.x.v v4, v1                       \n" \
  "vsetvli     zero, zero, e16, m2, ta, ma   \n" \
  "vmul.vx     v2, v2, %[k0101]              \n" \
  "vmul.vx     v4, v4, %[k0101]              \n" \
  "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n" \
  "vle8.v      v0, (%[src_y])                \n" \
  "vwcvtu.x.x.v v8, v0                       \n"

// Read [2*VLEN/8] Y, [2*VLEN/8] U, and [2*VLEN/8] V from 444
#define READYUV444                               \
  "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n" \
  "vle8.v      v0, (%[src_y])                \n" \
  "vle8.v      v2, (%[src_u])                \n" \
  "vle8.v      v4, (%[src_v])                \n" \
  "vwcvtu.x.x.v v8, v0                       \n"

// Read [2*VLEN/8] Y from src_y; Read [VLEN/8] U and [VLEN/8] V from src_uv
#define READNV12                                 \
  "addi        %[vl], %[w], 1                \n" \
  "srli        %[vl], %[vl], 1               \n" \
  "vsetvli     %[vl], %[vl], e8, m1, ta, ma  \n" \
  "vlseg2e8.v  v0, (%[src_uv])               \n" \
  "vwcvtu.x.x.v v2, v0                       \n" \
  "vwcvtu.x.x.v v4, v1                       \n" \
  "vsetvli     zero, zero, e16, m2, ta, ma   \n" \
  "vmul.vx     v2, v2, %[k0101]              \n" \
  "vmul.vx     v4, v4, %[k0101]              \n" \
  "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n" \
  "vle8.v      v0, (%[src_y])                \n" \
  "vwcvtu.x.x.v v8, v0                       \n"

// Read 2*[VLEN/8] Y from src_y; Read [VLEN/8] U and [VLEN/8] V from src_vu
#define READNV21                                 \
  "addi        %[vl], %[w], 1                \n" \
  "srli        %[vl], %[vl], 1               \n" \
  "vsetvli     %[vl], %[vl], e8, m1, ta, ma  \n" \
  "vlseg2e8.v  v0, (%[src_vu])               \n" \
  "vwcvtu.x.x.v v4, v0                       \n" \
  "vwcvtu.x.x.v v2, v1                       \n" \
  "vsetvli     zero, zero, e16, m2, ta, ma   \n" \
  "vmul.vx     v2, v2, %[k0101]              \n" \
  "vmul.vx     v4, v4, %[k0101]              \n" \
  "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n" \
  "vle8.v      v0, (%[src_y])                \n" \
  "vwcvtu.x.x.v v8, v0                       \n"

// Convert from YUV to fixed point RGB
#define YUVTORGB                                 \
  "vwmulu.vx   v12, v2, %[ug]                \n" \
  "vwmaccu.vx  v12, %[vg], v4                \n" \
  "vwmulu.vx   v0, v2, %[ub]                 \n" \
  "vsetvli     zero, zero, e16, m4, ta, ma   \n" \
  "vmul.vx     v8, v8, %[k0101]              \n" \
  "vmulhu.vx   v8, v8, %[yg]                 \n" \
  "vadd.vx     v16, v8, %[bg]                \n" \
  "vadd.vv     v0, v8, v0                    \n" \
  "vsetvli     zero, zero, e8, m2, ta, ma    \n" \
  "vwmaccu.vx  v8, %[vr], v4                 \n" \
  "vsetvli     zero, zero, e16, m4, ta, ma   \n" \
  "vssubu.vv   v12, v16, v12                 \n" \
  "vssubu.vx   v0, v0, %[bb]                 \n" \
  "vssubu.vx   v8, v8, %[br]                 \n"

// Convert from fixed point RGB To 8 bit RGB
#define RGBTORGB8                                \
  "vsetvli     zero, zero, e8, m2, ta, ma    \n" \
  "vnclipu.wi  v26, v12, 6                   \n" \
  "vnclipu.wi  v24, v0, 6                    \n" \
  "vnclipu.wi  v28, v8, 6                    \n"

#define YUVTORGB_REGS                                                         \
  "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9", "v10", "v11", "v12", "v13", \
      "v14", "v15", "v16", "v17", "v18", "v19", "v24", "v25", "v26", "v27",   \
      "v28", "v29"

#ifdef HAS_ARGBTOAR64ROW_RVV
void ARGBToAR64Row_RVV(const uint8_t* src_argb, uint16_t* dst_ar64, int width) {
  int vl;
  asm volatile(
      "slli        %[w], %[w], 2                 \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle8.v      v16, (%[src_argb])            \n"
      "vwcvtu.x.x.v v8, v16                      \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vmul.vx     v8, v8, %[k0101]              \n"
      "vse16.v     v8, (%[dst_ar64])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_ar64], %[dst_ar64], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_ar64] "+r"(dst_ar64),  // %[dst_ar64]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      : [k0101] "r"(0x0101)
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_ARGBTOAB64ROW_RVV
void ARGBToAB64Row_RVV(const uint8_t* src_argb, uint16_t* dst_ab64, int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma   \n"
      "vlseg4e8.v  v4, (%[src_argb])             \n"
      "vwcvtu.x.x.v v12, v4                      \n"
      "vwcvtu.x.x.v v10, v5                      \n"
      "vwcvtu.x.x.v v8, v6                       \n"
      "vwcvtu.x.x.v v14, v7                      \n"
      "vsetvli     zero, zero, e16, m2, ta, ma   \n"
      "vmul.vx     v8, v8, %[k0101]              \n"
      "vmul.vx     v10, v10, %[k0101]            \n"
      "vmul.vx     v12, v12, %[k0101]            \n"
      "vmul.vx     v14, v14, %[k0101]            \n"
      "vsseg4e16.v v8, (%[dst_ab64])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_ab64], %[dst_ab64], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_ab64] "+r"(dst_ab64),  // %[dst_ab64]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      : [k0101] "r"(0x0101)
      : RVV_VL_CLOBBER "memory", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15");
}
#endif

#ifdef HAS_AR64TOARGBROW_RVV
void AR64ToARGBRow_RVV(const uint16_t* src_ar64, uint8_t* dst_argb, int width) {
  int vl;
  asm volatile(
      "slli        %[w], %[w], 2                 \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle16.v     v8, (%[src_ar64])             \n"
      "vnsrl.wi    v16, v8, 8                    \n"
      "vse8.v      v16, (%[dst_argb])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_ar64], %[src_ar64], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_ar64] "+r"(src_ar64),  // %[src_ar64]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_AR64TOAB64ROW_RVV
void AR64ToAB64Row_RVV(const uint16_t* src_ar64,
                       uint16_t* dst_ab64,
                       int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m2, ta, ma  \n"
      "vlseg4e16.v v8, (%[src_ar64])             \n"
      "vmv2r.v     v16, v12                      \n"
      "vmv2r.v     v18, v10                      \n"
      "vmv2r.v     v20, v8                       \n"
      "vmv2r.v     v22, v14                      \n"
      "vsseg4e16.v v16, (%[dst_ab64])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 3               \n"
      "add         %[src_ar64], %[src_ar64], %[vl]\n"
      "add         %[dst_ab64], %[dst_ab64], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_ar64] "+r"(src_ar64),  // %[src_ar64]
        [dst_ab64] "+r"(dst_ab64),  // %[dst_ab64]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_AB64TOARGBROW_RVV
void AB64ToARGBRow_RVV(const uint16_t* src_ab64, uint8_t* dst_argb, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma   \n"
      "vlseg4e16.v v8, (%[src_ab64])             \n"
      "vnsrl.wi    v4, v12, 8                    \n"
      "vnsrl.wi    v5, v10, 8                    \n"
      "vnsrl.wi    v6, v8, 8                     \n"
      "vnsrl.wi    v7, v14, 8                    \n"
      "vsseg4e8.v  v4, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_ab64], %[src_ab64], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_ab64] "+r"(src_ab64),  // %[src_ab64]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15");
}
#endif

#ifdef HAS_RAWTOARGBROW_RVV
void RAWToARGBRow_RVV(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  size_t vl, tmp;
  asm volatile(
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vmv.v.i     v14, -1                       \n"

      "1:          \n"
      "vlseg3e8.v  v16, (%[src_raw])             \n"
      "vmv2r.v     v8, v20                       \n"
      "vmv2r.v     v10, v18                      \n"
      "vmv2r.v     v12, v16                      \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_raw], %[src_raw], %[vl] \n"
      "add         %[src_raw], %[src_raw], %[tmp]\n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_raw] "+r"(src_raw),    // %[src_raw]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl),             // %[vl]
        [tmp] "=&r"(tmp)            // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21");
}
#endif

#ifdef HAS_RAWTORGBAROW_RVV
void RAWToRGBARow_RVV(const uint8_t* src_raw, uint8_t* dst_rgba, int width) {
  size_t vl, tmp;
  asm volatile(
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vmv.v.i     v8, -1                        \n"

      "1:          \n"
      "vlseg3e8.v  v16, (%[src_raw])             \n"
      "vmv2r.v     v10, v20                      \n"
      "vmv2r.v     v12, v18                      \n"
      "vmv2r.v     v14, v16                      \n"
      "vsseg4e8.v  v8, (%[dst_rgba])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_raw], %[src_raw], %[vl] \n"
      "add         %[src_raw], %[src_raw], %[tmp]\n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_rgba], %[dst_rgba], %[vl]\n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_raw] "+r"(src_raw),    // %[src_raw]
        [dst_rgba] "+r"(dst_rgba),  // %[dst_rgba]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl),             // %[vl]
        [tmp] "=&r"(tmp)            // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21");
}
#endif

#ifdef HAS_RAWTORGB24ROW_RVV
void RAWToRGB24Row_RVV(const uint8_t* src_raw, uint8_t* dst_rgb24, int width) {
  size_t vl, tmp;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg3e8.v  v16, (%[src_raw])             \n"
      "vmv2r.v     v8, v20                       \n"
      "vmv2r.v     v10, v18                      \n"
      "vmv2r.v     v12, v16                      \n"
      "vsseg3e8.v  v8, (%[dst_rgb24])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[tmp], %[tmp], %[vl]         \n"
      "add         %[src_raw], %[src_raw], %[tmp]\n"
      "add         %[dst_rgb24], %[dst_rgb24], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_raw] "+r"(src_raw),      // %[src_raw]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [tmp] "=&r"(tmp)              // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v16",
        "v17", "v18", "v19", "v20", "v21");
}
#endif

#ifdef HAS_ARGBTORAWROW_RVV
void ARGBToRAWRow_RVV(const uint8_t* src_argb, uint8_t* dst_raw, int width) {
  size_t vl, tmp;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vmv2r.v     v16, v12                      \n"
      "vmv2r.v     v18, v10                      \n"
      "vmv2r.v     v20, v8                       \n"
      "vsseg3e8.v  v16, (%[dst_raw])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 2              \n"
      "add         %[src_argb], %[src_argb], %[tmp]\n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[dst_raw], %[dst_raw], %[vl] \n"
      "add         %[dst_raw], %[dst_raw], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_raw] "+r"(dst_raw),    // %[dst_raw]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl),             // %[vl]
        [tmp] "=&r"(tmp)            // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21");
}
#endif

#ifdef HAS_ARGBTORGB24ROW_RVV
void ARGBToRGB24Row_RVV(const uint8_t* src_argb,
                        uint8_t* dst_rgb24,
                        int width) {
  size_t vl, tmp;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vsseg3e8.v  v8, (%[dst_rgb24])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 2              \n"
      "add         %[src_argb], %[src_argb], %[tmp]\n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "add         %[dst_rgb24], %[dst_rgb24], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),    // %[src_argb]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [tmp] "=&r"(tmp)              // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_ARGBTOABGRROW_RVV
void ARGBToABGRRow_RVV(const uint8_t* src_argb, uint8_t* dst_abgr, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vmv2r.v     v16, v12                      \n"
      "vmv2r.v     v18, v10                      \n"
      "vmv2r.v     v20, v8                       \n"
      "vmv2r.v     v22, v14                      \n"
      "vsseg4e8.v  v16, (%[dst_abgr])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "add         %[dst_abgr], %[dst_abgr], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_abgr] "+r"(dst_abgr),  // %[dst_abgr]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_ARGBTOBGRAROW_RVV
void ARGBToBGRARow_RVV(const uint8_t* src_argb, uint8_t* dst_bgra, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma  \n"
      "vle32.v     v8, (%[src_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "vsrl.vi     v12, v8, 16                   \n"
      "vsll.vi     v8, v8, 16                    \n"
      "vor.vv      v8, v8, v12                   \n"
      "slli        %[vl], %[vl], 1               \n"
      "vsetvli     zero, %[vl], e16, m4, ta, ma  \n"
      "vsrl.vi     v12, v8, 8                    \n"
      "vsll.vi     v8, v8, 8                     \n"
      "vor.vv      v8, v8, v12                   \n"
      "vse16.v     v8, (%[dst_bgra])             \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "add         %[dst_bgra], %[dst_bgra], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_bgra] "+r"(dst_bgra),  // %[dst_bgra]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_ARGBTORGBAROW_RVV
void ARGBToRGBARow_RVV(const uint8_t* src_argb, uint8_t* dst_rgba, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma  \n"
      "vle32.v     v8, (%[src_argb])             \n"
      "vsrl.vi     v12, v8, 24                   \n"
      "vsll.vi     v8, v8, 8                     \n"
      "vor.vv      v8, v8, v12                   \n"
      "vse32.v     v8, (%[dst_rgba])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "add         %[dst_rgba], %[dst_rgba], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_rgba] "+r"(dst_rgba),  // %[dst_rgba]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_RGBATOARGBROW_RVV
void RGBAToARGBRow_RVV(const uint8_t* src_rgba, uint8_t* dst_argb, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma  \n"
      "vle32.v     v8, (%[src_rgba])             \n"
      "vsrl.vi     v12, v8, 8                    \n"
      "vsll.vi     v8, v8, 24                    \n"
      "vor.vv      v8, v8, v12                   \n"
      "vse32.v     v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_rgba], %[src_rgba], %[vl]\n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_rgba] "+r"(src_rgba),  // %[src_rgba]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_RGB24TOARGBROW_RVV
void RGB24ToARGBRow_RVV(const uint8_t* src_rgb24,
                        uint8_t* dst_argb,
                        int width) {
  size_t vl, tmp;
  asm volatile(
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vmv.v.i     v14, -1                       \n"

      "1:          \n"
      "vlseg3e8.v  v8, (%[src_rgb24])            \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_rgb24], %[src_rgb24], %[vl]\n"
      "add         %[src_rgb24], %[src_rgb24], %[tmp]\n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_rgb24] "+r"(src_rgb24),  // %[src_rgb24]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [tmp] "=&r"(tmp)              // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_I444TOARGBROW_RVV
void I444ToARGBRow_RVV(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //
      "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
      "vmv.v.i     v30, -1                       \n"

      "1:          \n"                                //
      READYUV444                                      //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_u], %[src_u], %[vl]     \n"
      "add         %[src_v], %[src_v], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_I444ALPHATOARGBROW_RVV
void I444AlphaToARGBRow_RVV(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            const uint8_t* src_a,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READYUV444                                      //
      "vle8.v      v30, (%[src_a])               \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_u], %[src_u], %[vl]     \n"
      "add         %[src_v], %[src_v], %[vl]     \n"
      "add         %[src_a], %[src_a], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [src_a] "+r"(src_a),          // %[src_a]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_I444TORGB24ROW_RVV
void I444ToRGB24Row_RVV(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_rgb24,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READYUV444                                      //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg3e8.v  v24, (%[dst_rgb24])           \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_u], %[src_u], %[vl]     \n"
      "add         %[src_v], %[src_v], %[vl]     \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS);
}
#endif

#ifdef HAS_I422TOARGBROW_RVV
void I422ToARGBRow_RVV(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //
      "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
      "vmv.v.i     v30, -1                       \n"

      "1:          \n"                                //
      READYUV422                                      //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "srli        %[k0101], %[vl], 1            \n"
      "add         %[src_u], %[src_u], %[k0101]  \n"
      "add         %[src_v], %[src_v], %[k0101]  \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "li          %[k0101], 0x0101              \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_I422ALPHATOARGBROW_RVV
void I422AlphaToARGBRow_RVV(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            const uint8_t* src_a,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READYUV422                                      //
      "vle8.v      v30, (%[src_a])               \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_a], %[src_a], %[vl]     \n"
      "srli        %[k0101], %[vl], 1            \n"
      "add         %[src_u], %[src_u], %[k0101]  \n"
      "add         %[src_v], %[src_v], %[k0101]  \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "li          %[k0101], 0x0101              \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [src_a] "+r"(src_a),          // %[src_a]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_I422TORGBAROW_RVV
void I422ToRGBARow_RVV(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_rgba,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READYUV422                                      //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vmv.v.i     v22, -1                       \n"
      "vsseg4e8.v  v22, (%[dst_rgba])            \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "srli        %[k0101], %[vl], 1            \n"
      "add         %[src_u], %[src_u], %[k0101]  \n"
      "add         %[src_v], %[src_v], %[k0101]  \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_rgba], %[dst_rgba], %[vl]\n"
      "li          %[k0101], 0x0101              \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_rgba] "+r"(dst_rgba),    // %[dst_rgba]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v22", "v23");
}
#endif

#ifdef HAS_I422TORGB24ROW_RVV
void I422ToRGB24Row_RVV(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_rgb24,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READYUV422                                      //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg3e8.v  v24, (%[dst_rgb24])           \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "srli        %[k0101], %[vl], 1            \n"
      "add         %[src_u], %[src_u], %[k0101]  \n"
      "add         %[src_v], %[src_v], %[k0101]  \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "li          %[k0101], 0x0101              \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS);
}
#endif

#ifdef HAS_I422TOAR30ROW_RVV
void I422ToAR30Row_RVV(const uint8_t* src_y,
                       const uint8_t* src_u,
                       const uint8_t* src_v,
                       uint8_t* dst_ar30,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(            //
      YUVTORGB_SETUP_AR30  //

      "1:          \n"
      "addi        %[vl], %[w], 1                \n"
      "srli        %[vl], %[vl], 1               \n"
      "vsetvli     %[vl], %[vl], e8, mf2, ta, ma \n"
      "vle8.v      v0, (%[src_u])                \n"
      "vle8.v      v1, (%[src_v])                \n"
      "vwcvtu.x.x.v v2, v0                       \n"
      "vwcvtu.x.x.v v4, v1                       \n"
      "vsetvli     zero, zero, e16, m1, ta, ma   \n"
      "vmul.vx     v2, v2, %[k0101]              \n"
      "vmul.vx     v4, v4, %[k0101]              \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma   \n"
      "vle8.v      v0, (%[src_y])                \n"
      "vwcvtu.x.x.v v8, v0                       \n"
      "vwmulu.vx   v12, v2, %[ug]                \n"
      "vwmaccu.vx  v12, %[vg], v4                \n"
      "vwmulu.vx   v0, v2, %[ub]                 \n"
      "vsetvli     zero, zero, e16, m2, ta, ma   \n"
      "vmul.vx     v8, v8, %[k0101]              \n"
      "vmulhu.vx   v8, v8, %[yg]                 \n"
      "vadd.vx     v16, v8, %[bg]                \n"
      "vadd.vv     v0, v8, v0                    \n"
      "vsetvli     zero, zero, e8, m1, ta, ma    \n"
      "vwmaccu.vx  v8, %[vr], v4                 \n"
      "vsetvli     zero, zero, e16, m2, ta, ma   \n"
      "vssubu.vv   v12, v16, v12                 \n"
      "vssubu.vx   v0, v0, %[bb]                 \n"
      "vssubu.vx   v8, v8, %[br]                 \n"
      "vsrl.vi     v0, v0, 4                     \n"
      "vsrl.vi     v12, v12, 4                   \n"
      "vsrl.vi     v8, v8, 4                     \n"
      "li          %[k0101], 1023                \n"
      "vminu.vx    v0, v0, %[k0101]              \n"
      "vminu.vx    v12, v12, %[k0101]            \n"
      "vminu.vx    v8, v8, %[k0101]              \n"
      "vwcvtu.x.x.v v16, v0                      \n"
      "li          %[k0101], 1024                \n"
      "vwmaccu.vx  v16, %[k0101], v12            \n"
      "li          %[k0101], 0x0c00              \n"
      "vor.vx      v8, v8, %[k0101]              \n"
      "vwcvtu.x.x.v v20, v8                      \n"
      "vsetvli     zero, zero, e32, m4, ta, ma   \n"
      "vsll.vi     v20, v20, 20                  \n"
      "vor.vv      v16, v16, v20                 \n"
      "vse32.v     v16, (%[dst_ar30])            \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "srli        %[k0101], %[vl], 1            \n"
      "add         %[src_u], %[src_u], %[k0101]  \n"
      "add         %[src_v], %[src_v], %[k0101]  \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_ar30], %[dst_ar30], %[vl]\n"
      "li          %[k0101], 0x0101              \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_u] "+r"(src_u),          // %[src_u]
        [src_v] "+r"(src_v),          // %[src_v]
        [dst_ar30] "+r"(dst_ar30),    // %[dst_ar30]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v4", "v8", "v9", "v12",
        "v13", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_I400TOARGBROW_RVV
void I400ToARGBRow_RVV(const uint8_t* src_y,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  const bool is_yb_positive = (yuvconstants->kRGBCoeffBias[4] >= 0);
  uint16_t yg = yuvconstants->kRGBCoeffBias[0];
  uint16_t yb = is_yb_positive
                    ? (uint16_t)(yuvconstants->kRGBCoeffBias[4] - 32)
                    : (uint16_t)(-yuvconstants->kRGBCoeffBias[4] + 32);
  size_t k0101;
  if (is_yb_positive) {
    asm volatile(
        "csrwi       vxrm, 0                       \n"
        "li          %[k0101], 0x0101              \n"
        "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
        "vmv.v.x     v20, %[yg]                    \n"
        "vmv.v.x     v24, %[yb]                    \n"
        "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
        "vmv.v.i     v14, -1                       \n"

        "1:          \n"
        "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
        "vle8.v      v0, (%[src_y])                \n"
        "vwcvtu.x.x.v v16, v0                      \n"
        "vsetvli     zero, zero, e16, m4, ta, ma   \n"
        "vmul.vx     v16, v16, %[k0101]            \n"
        "vmulhu.vv   v16, v16, v20                 \n"
        "vsaddu.vv   v16, v16, v24                 \n"
        "vsetvli     zero, zero, e8, m2, ta, ma    \n"
        "vnclipu.wi  v8, v16, 6                    \n"
        "vmv2r.v     v10, v8                       \n"
        "vmv2r.v     v12, v8                       \n"
        "vsseg4e8.v  v8, (%[dst_argb])             \n"
        "sub         %[w], %[w], %[vl]             \n"
        "add         %[src_y], %[src_y], %[vl]     \n"
        "slli        %[vl], %[vl], 2               \n"
        "add         %[dst_argb], %[dst_argb], %[vl]\n"
        "bgtz        %[w], 1b                      \n"
        : [src_y] "+r"(src_y),        // %[src_y]
          [dst_argb] "+r"(dst_argb),  // %[dst_argb]
          [w] "+r"(width),            // %[w]
          [vl] "=&r"(vl),             // %[vl]
          [k0101] "=&r"(k0101)        // %[k0101]
        : [yg] "r"(yg),               // %[yg]
          [yb] "r"(yb)                // %[yb]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v8", "v9", "v10", "v11", "v12",
          "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22",
          "v23", "v24", "v25", "v26", "v27");
  } else {
    asm volatile(
        "csrwi       vxrm, 0                       \n"
        "li          %[k0101], 0x0101              \n"
        "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
        "vmv.v.x     v20, %[yg]                    \n"
        "vmv.v.x     v24, %[yb]                    \n"
        "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
        "vmv.v.i     v14, -1                       \n"

        "1:          \n"
        "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
        "vle8.v      v0, (%[src_y])                \n"
        "vwcvtu.x.x.v v16, v0                      \n"
        "vsetvli     zero, zero, e16, m4, ta, ma   \n"
        "vmul.vx     v16, v16, %[k0101]            \n"
        "vmulhu.vv   v16, v16, v20                 \n"
        "vssubu.vv   v16, v16, v24                 \n"
        "vsetvli     zero, zero, e8, m2, ta, ma    \n"
        "vnclipu.wi  v8, v16, 6                    \n"
        "vmv2r.v     v10, v8                       \n"
        "vmv2r.v     v12, v8                       \n"
        "vsseg4e8.v  v8, (%[dst_argb])             \n"
        "sub         %[w], %[w], %[vl]             \n"
        "add         %[src_y], %[src_y], %[vl]     \n"
        "slli        %[vl], %[vl], 2               \n"
        "add         %[dst_argb], %[dst_argb], %[vl]\n"
        "bgtz        %[w], 1b                      \n"
        : [src_y] "+r"(src_y),        // %[src_y]
          [dst_argb] "+r"(dst_argb),  // %[dst_argb]
          [w] "+r"(width),            // %[w]
          [vl] "=&r"(vl),             // %[vl]
          [k0101] "=&r"(k0101)        // %[k0101]
        : [yg] "r"(yg),               // %[yg]
          [yb] "r"(yb)                // %[yb]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v8", "v9", "v10", "v11", "v12",
          "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22",
          "v23", "v24", "v25", "v26", "v27");
  }
}
#endif

#ifdef HAS_J400TOARGBROW_RVV
void J400ToARGBRow_RVV(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  size_t vl;
  asm volatile(
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vmv.v.i     v14, -1                       \n"

      "1:          \n"
      "vle8.v      v8, (%[src_y])                \n"
      "vmv2r.v     v10, v8                       \n"
      "vmv2r.v     v12, v8                       \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),        // %[src_y]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_COPYROW_RVV
void CopyRow_RVV(const uint8_t* src, uint8_t* dst, int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m4, ta, ma   \n"
      "vle8.v      v8, (%[src])                  \n"
      "vse8.v      v8, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src], %[src], %[vl]         \n"
      "add         %[dst], %[dst], %[vl]         \n"
      "bgtz        %[w], 1b                      \n"
      : [src] "+r"(src),  // %[src]
        [dst] "+r"(dst),  // %[dst]
        [w] "+r"(width),  // %[w]
        [vl] "=&r"(vl)    // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11");
}
#endif

#ifdef HAS_NV12TOARGBROW_RVV
void NV12ToARGBRow_RVV(const uint8_t* src_y,
                       const uint8_t* src_uv,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //
      "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
      "vmv.v.i     v30, -1                       \n"

      "1:          \n"                                //
      READNV12                                        //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_uv], %[src_uv], %[vl]   \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_uv] "+r"(src_uv),        // %[src_uv]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_NV12TORGB24ROW_RVV
void NV12ToRGB24Row_RVV(const uint8_t* src_y,
                        const uint8_t* src_uv,
                        uint8_t* dst_rgb24,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READNV12                                        //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg3e8.v  v24, (%[dst_rgb24])           \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_uv], %[src_uv], %[vl]   \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_uv] "+r"(src_uv),        // %[src_uv]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS);
}
#endif

#ifdef HAS_NV21TOARGBROW_RVV
void NV21ToARGBRow_RVV(const uint8_t* src_y,
                       const uint8_t* src_vu,
                       uint8_t* dst_argb,
                       const struct YuvConstants* yuvconstants,
                       int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //
      "vsetvli     zero, %[w], e8, m2, ta, ma    \n"
      "vmv.v.i     v30, -1                       \n"

      "1:          \n"                                //
      READNV21                                        //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg4e8.v  v24, (%[dst_argb])            \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_vu], %[src_vu], %[vl]   \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_vu] "+r"(src_vu),        // %[src_vu]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS, "v30", "v31");
}
#endif

#ifdef HAS_NV21TORGB24ROW_RVV
void NV21ToRGB24Row_RVV(const uint8_t* src_y,
                        const uint8_t* src_vu,
                        uint8_t* dst_rgb24,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  size_t vl;
  size_t ub, vr, ug, vg;
  size_t yg, bb, bg, br;
  size_t k0101;
  asm volatile(       //
      YUVTORGB_SETUP  //

      "1:          \n"                                //
      READNV21                                        //
      "sub         %[w], %[w], %[vl]             \n"  //
      YUVTORGB                                        //
      RGBTORGB8                                       //
      "vsseg3e8.v  v24, (%[dst_rgb24])           \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[src_vu], %[src_vu], %[vl]   \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_rgb24], %[dst_rgb24], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),          // %[src_y]
        [src_vu] "+r"(src_vu),        // %[src_vu]
        [dst_rgb24] "+r"(dst_rgb24),  // %[dst_rgb24]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl),               // %[vl]
        [ub] "=&r"(ub),               // %[ub]
        [vr] "=&r"(vr),               // %[vr]
        [ug] "=&r"(ug),               // %[ug]
        [vg] "=&r"(vg),               // %[vg]
        [yg] "=&r"(yg),               // %[yg]
        [bb] "=&r"(bb),               // %[bb]
        [bg] "=&r"(bg),               // %[bg]
        [br] "=&r"(br),               // %[br]
        [k0101] "=&r"(k0101)          // %[k0101]
      : [yuvconst] "r"(yuvconstants)  // %[yuvconst]
      : RVV_VL_CLOBBER "memory", YUVTORGB_REGS);
}
#endif

// Bilinear filter [VLEN/8]x2 -> [VLEN/8]x1
#ifdef HAS_INTERPOLATEROW_RVV
void InterpolateRow_RVV(uint8_t* dst_ptr,
                        const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        int dst_width,
                        int source_y_fraction) {
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  size_t dst_w = (size_t)dst_width;
  size_t vl;
  assert(source_y_fraction >= 0);
  assert(source_y_fraction < 256);
  // Blend 100 / 0 - Copy row unchanged.
  if (y1_fraction == 0) {
    asm volatile(
        "1:          \n"
        "vsetvli     %[vl], %[dst_w], e8, m4, ta, ma\n"
        "vle8.v      v8, (%[src_ptr])              \n"
        "vse8.v      v8, (%[dst_ptr])              \n"
        "sub         %[dst_w], %[dst_w], %[vl]     \n"
        "add         %[src_ptr], %[src_ptr], %[vl] \n"
        "add         %[dst_ptr], %[dst_ptr], %[vl] \n"
        "bgtz        %[dst_w], 1b                  \n"
        : [dst_ptr] "+r"(dst_ptr),  // %[dst_ptr]
          [src_ptr] "+r"(src_ptr),  // %[src_ptr]
          [dst_w] "+r"(dst_w),      // %[dst_w]
          [vl] "=&r"(vl)            // %[vl]
        :
        : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11");
    return;
  }
  // Blend 50 / 50.
  if (y1_fraction == 128) {
    asm volatile(
        "csrwi       vxrm, 0                       \n"

        "1:          \n"
        "vsetvli     %[vl], %[dst_w], e8, m4, ta, ma\n"
        "vle8.v      v8, (%[src_ptr])              \n"
        "vle8.v      v12, (%[src_ptr1])            \n"
        "vaaddu.vv   v8, v8, v12                   \n"
        "vse8.v      v8, (%[dst_ptr])              \n"
        "sub         %[dst_w], %[dst_w], %[vl]     \n"
        "add         %[src_ptr], %[src_ptr], %[vl] \n"
        "add         %[src_ptr1], %[src_ptr1], %[vl]\n"
        "add         %[dst_ptr], %[dst_ptr], %[vl] \n"
        "bgtz        %[dst_w], 1b                  \n"
        : [dst_ptr] "+r"(dst_ptr),    // %[dst_ptr]
          [src_ptr] "+r"(src_ptr),    // %[src_ptr]
          [src_ptr1] "+r"(src_ptr1),  // %[src_ptr1]
          [dst_w] "+r"(dst_w),        // %[dst_w]
          [vl] "=&r"(vl)              // %[vl]
        :
        : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13",
          "v14", "v15");
    return;
  }
  // General purpose row blend.
  asm volatile(
      "vsetvli     zero, %[dst_w], e16, m4, ta, ma\n"
      "vmv.v.x     v12, %[c128]                  \n"

      "1:          \n"
      "vsetvli     %[vl], %[dst_w], e8, m2, ta, ma\n"
      "vle8.v      v16, (%[src_ptr])             \n"
      "vmv4r.v     v8, v12                       \n"
      "vwmaccu.vx  v8, %[y0_fraction], v16       \n"
      "vle8.v      v16, (%[src_ptr1])            \n"
      "vwmaccu.vx  v8, %[y1_fraction], v16       \n"
      "vnsrl.wi    v16, v8, 8                    \n"
      "vse8.v      v16, (%[dst_ptr])             \n"
      "sub         %[dst_w], %[dst_w], %[vl]     \n"
      "add         %[src_ptr], %[src_ptr], %[vl] \n"
      "add         %[src_ptr1], %[src_ptr1], %[vl]\n"
      "add         %[dst_ptr], %[dst_ptr], %[vl] \n"
      "bgtz        %[dst_w], 1b                  \n"
      : [dst_ptr] "+r"(dst_ptr),         // %[dst_ptr]
        [src_ptr] "+r"(src_ptr),         // %[src_ptr]
        [src_ptr1] "+r"(src_ptr1),       // %[src_ptr1]
        [dst_w] "+r"(dst_w),             // %[dst_w]
        [vl] "=&r"(vl)                   // %[vl]
      : [y0_fraction] "r"(y0_fraction),  // %[y0_fraction]
        [y1_fraction] "r"(y1_fraction),  // %[y1_fraction]
        [c128] "r"(128)                  // %[c128]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17");
}
#endif

#ifdef HAS_SPLITRGBROW_RVV
void SplitRGBRow_RVV(const uint8_t* src_rgb,
                     uint8_t* dst_r,
                     uint8_t* dst_g,
                     uint8_t* dst_b,
                     int width) {
  size_t vl, tmp;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg3e8.v  v8, (%[src_rgb])              \n"
      "vse8.v      v8, (%[dst_r])                \n"
      "vse8.v      v10, (%[dst_g])               \n"
      "vse8.v      v12, (%[dst_b])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_r], %[dst_r], %[vl]     \n"
      "add         %[dst_g], %[dst_g], %[vl]     \n"
      "add         %[dst_b], %[dst_b], %[vl]     \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_rgb], %[src_rgb], %[vl] \n"
      "add         %[src_rgb], %[src_rgb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_rgb] "+r"(src_rgb),  // %[src_rgb]
        [dst_r] "+r"(dst_r),      // %[dst_r]
        [dst_g] "+r"(dst_g),      // %[dst_g]
        [dst_b] "+r"(dst_b),      // %[dst_b]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [tmp] "=&r"(tmp)          // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13");
}
#endif

#ifdef HAS_MERGERGBROW_RVV
void MergeRGBRow_RVV(const uint8_t* src_r,
                     const uint8_t* src_g,
                     const uint8_t* src_b,
                     uint8_t* dst_rgb,
                     int width) {
  size_t vl, tmp;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle8.v      v8, (%[src_r])                \n"
      "vle8.v      v10, (%[src_g])               \n"
      "vle8.v      v12, (%[src_b])               \n"
      "vsseg3e8.v  v8, (%[dst_rgb])              \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_r], %[src_r], %[vl]     \n"
      "add         %[src_g], %[src_g], %[vl]     \n"
      "add         %[src_b], %[src_b], %[vl]     \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[dst_rgb], %[dst_rgb], %[vl] \n"
      "add         %[dst_rgb], %[dst_rgb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_r] "+r"(src_r),      // %[src_r]
        [src_g] "+r"(src_g),      // %[src_g]
        [src_b] "+r"(src_b),      // %[src_b]
        [dst_rgb] "+r"(dst_rgb),  // %[dst_rgb]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [tmp] "=&r"(tmp)          // %[tmp]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13");
}
#endif

#ifdef HAS_SPLITARGBROW_RVV
void SplitARGBRow_RVV(const uint8_t* src_argb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      uint8_t* dst_a,
                      int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vse8.v      v14, (%[dst_a])               \n"
      "vse8.v      v12, (%[dst_r])               \n"
      "vse8.v      v10, (%[dst_g])               \n"
      "vse8.v      v8, (%[dst_b])                \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_a], %[dst_a], %[vl]     \n"
      "add         %[dst_r], %[dst_r], %[vl]     \n"
      "add         %[dst_g], %[dst_g], %[vl]     \n"
      "add         %[dst_b], %[dst_b], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_r] "+r"(dst_r),        // %[dst_r]
        [dst_g] "+r"(dst_g),        // %[dst_g]
        [dst_b] "+r"(dst_b),        // %[dst_b]
        [dst_a] "+r"(dst_a),        // %[dst_a]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_MERGEARGBROW_RVV
void MergeARGBRow_RVV(const uint8_t* src_r,
                      const uint8_t* src_g,
                      const uint8_t* src_b,
                      const uint8_t* src_a,
                      uint8_t* dst_argb,
                      int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle8.v      v12, (%[src_r])               \n"
      "vle8.v      v10, (%[src_g])               \n"
      "vle8.v      v8, (%[src_b])                \n"
      "vle8.v      v14, (%[src_a])               \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_r], %[src_r], %[vl]     \n"
      "add         %[src_g], %[src_g], %[vl]     \n"
      "add         %[src_b], %[src_b], %[vl]     \n"
      "add         %[src_a], %[src_a], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_r] "+r"(src_r),        // %[src_r]
        [src_g] "+r"(src_g),        // %[src_g]
        [src_b] "+r"(src_b),        // %[src_b]
        [src_a] "+r"(src_a),        // %[src_a]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SPLITXRGBROW_RVV
void SplitXRGBRow_RVV(const uint8_t* src_argb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vse8.v      v12, (%[dst_r])               \n"
      "vse8.v      v10, (%[dst_g])               \n"
      "vse8.v      v8, (%[dst_b])                \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_r], %[dst_r], %[vl]     \n"
      "add         %[dst_g], %[dst_g], %[vl]     \n"
      "add         %[dst_b], %[dst_b], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_r] "+r"(dst_r),        // %[dst_r]
        [dst_g] "+r"(dst_g),        // %[dst_g]
        [dst_b] "+r"(dst_b),        // %[dst_b]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_MERGEXRGBROW_RVV
void MergeXRGBRow_RVV(const uint8_t* src_r,
                      const uint8_t* src_g,
                      const uint8_t* src_b,
                      uint8_t* dst_argb,
                      int width) {
  size_t vl;
  asm volatile(
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vmv.v.i     v14, -1                       \n"

      "1:          \n"
      "vle8.v      v12, (%[src_r])               \n"
      "vle8.v      v10, (%[src_g])               \n"
      "vle8.v      v8, (%[src_b])                \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_r], %[src_r], %[vl]     \n"
      "add         %[src_g], %[src_g], %[vl]     \n"
      "add         %[src_b], %[src_b], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_r] "+r"(src_r),        // %[src_r]
        [src_g] "+r"(src_g),        // %[src_g]
        [src_b] "+r"(src_b),        // %[src_b]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SPLITUVROW_RVV
void SplitUVRow_RVV(const uint8_t* src_uv,
                    uint8_t* dst_u,
                    uint8_t* dst_v,
                    int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m4, ta, ma   \n"
      "vlseg2e8.v  v8, (%[src_uv])               \n"
      "vse8.v      v8, (%[dst_u])                \n"
      "vse8.v      v12, (%[dst_v])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_u], %[dst_u], %[vl]     \n"
      "add         %[dst_v], %[dst_v], %[vl]     \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_uv], %[src_uv], %[vl]   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_uv] "+r"(src_uv),  // %[src_uv]
        [dst_u] "+r"(dst_u),    // %[dst_u]
        [dst_v] "+r"(dst_v),    // %[dst_v]
        [w] "+r"(width),        // %[w]
        [vl] "=&r"(vl)          // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_MERGEUVROW_RVV
void MergeUVRow_RVV(const uint8_t* src_u,
                    const uint8_t* src_v,
                    uint8_t* dst_uv,
                    int width) {
  size_t vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m4, ta, ma   \n"
      "vle8.v      v8, (%[src_u])                \n"
      "vle8.v      v12, (%[src_v])               \n"
      "vsseg2e8.v  v8, (%[dst_uv])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_u], %[src_u], %[vl]     \n"
      "add         %[src_v], %[src_v], %[vl]     \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_uv], %[dst_uv], %[vl]   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_u] "+r"(src_u),    // %[src_u]
        [src_v] "+r"(src_v),    // %[src_v]
        [dst_uv] "+r"(dst_uv),  // %[dst_uv]
        [w] "+r"(width),        // %[w]
        [vl] "=&r"(vl)          // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SWAPUVROW_RVV
void SwapUVRow_RVV(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  assert(width != 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
  size_t vl;
  asm("1:          \n"
      "vsetvli     %[vl], %[w], e16, m4, ta, ma  \n"
      "vle16.v     v8, (%[src_uv])               \n"
      "vsrl.vi     v12, v8, 8                    \n"
      "vsll.vi     v8, v8, 8                     \n"
      "vor.vv      v8, v8, v12                   \n"
      "vse16.v     v8, (%[dst_vu])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_uv], %[src_uv], %[vl]   \n"
      "add         %[dst_vu], %[dst_vu], %[vl]   \n"
      "bgtz        %[w], 1b                      \n"
      : [src_uv] "+r"(src_uv),  // %[src_uv]
        [dst_vu] "+r"(dst_vu),  // %[dst_vu]
        [w] "+r"(width),        // %[w]
        [vl] "=&r"(vl),         // %[vl]
        "=m"(*(uint8_t (*)[width * 2]) dst_vu)
      : "m"(*(const uint8_t (*)[width * 2]) src_uv)
      : RVV_VL_CLOBBER "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15");
#pragma GCC diagnostic pop
}
#endif

// ARGB expects first 3 values to contain RGB and 4th value is ignored
#ifdef HAS_ARGBTOYMATRIXROW_RVV
void ARGBToYMatrixRow_RVV(const uint8_t* src_argb,
                          uint8_t* dst_y,
                          int width,
                          const struct ArgbConstants* c) {
  assert(width != 0);
  size_t vl, tmp;
  asm volatile(
      "lhu         %[tmp], 48(%[c])              \n"
      "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
      "vmv.v.x     v8, %[tmp]                    \n"
      "lbu         %[vl], 0(%[c])                \n"
      "lbu         %[tmp], 1(%[c])               \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vmv.v.x     v12, %[vl]                    \n"
      "vmv.v.x     v14, %[tmp]                   \n"
      "lbu         %[vl], 2(%[c])                \n"
      "lbu         %[tmp], 3(%[c])               \n"
      "vmv.v.x     v16, %[vl]                    \n"
      "vmv.v.x     v18, %[tmp]                   \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v24, (%[src_argb])            \n"
      "vwmulu.vv   v20, v24, v12                 \n"
      "vwmaccu.vv  v20, v14, v26                 \n"
      "vwmaccu.vv  v20, v16, v28                 \n"
      "vwmaccu.vv  v20, v18, v30                 \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vadd.vv     v20, v20, v8                  \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v24, v20, 8                   \n"
      "vse8.v      v24, (%[dst_y])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_y], %[dst_y], %[vl]     \n"
      "slli        %[tmp], %[vl], 2              \n"
      "add         %[src_argb], %[src_argb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_y] "+r"(dst_y),        // %[dst_y]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl),             // %[vl]
        [tmp] "=&r"(tmp)            // %[tmp]
      : [c] "r"(c)                  // %[c]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29", "v30", "v31");
}
#endif

#ifdef HAS_RGBTOYMATRIXROW_RVV
void RGBToYMatrixRow_RVV(const uint8_t* src_rgb,
                         uint8_t* dst_y,
                         int width,
                         const struct ArgbConstants* c) {
  assert(width != 0);
  size_t vl, tmp;
  asm volatile(
      "lhu         %[tmp], 48(%[c])              \n"
      "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
      "vmv.v.x     v8, %[tmp]                    \n"
      "lbu         %[vl], 0(%[c])                \n"
      "lbu         %[tmp], 1(%[c])               \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vmv.v.x     v12, %[vl]                    \n"
      "vmv.v.x     v14, %[tmp]                   \n"
      "lbu         %[vl], 2(%[c])                \n"
      "vmv.v.x     v16, %[vl]                    \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg3e8.v  v24, (%[src_rgb])             \n"
      "vwmulu.vv   v20, v28, v16                 \n"
      "vwmaccu.vv  v20, v14, v26                 \n"
      "vwmaccu.vv  v20, v12, v24                 \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vadd.vv     v20, v20, v8                  \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v18, v20, 8                   \n"
      "vse8.v      v18, (%[dst_y])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_y], %[dst_y], %[vl]     \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_rgb], %[src_rgb], %[vl] \n"
      "add         %[src_rgb], %[src_rgb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_rgb] "+r"(src_rgb),  // %[src_rgb]
        [dst_y] "+r"(dst_y),      // %[dst_y]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [tmp] "=&r"(tmp)          // %[tmp]
      : [c] "r"(c)                // %[c]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29");
}
#endif

#ifdef HAS_ARGBTOUV444MATRIXROW_RVV
void ARGBToUV444MatrixRow_RVV(const uint8_t* src_argb,
                              uint8_t* dst_u,
                              uint8_t* dst_v,
                              int width,
                              const struct ArgbConstants* c) {
  assert(width != 0);
  size_t vl, tmp;
  asm volatile(
      "lh          %[tmp], 64(%[c])              \n"
      "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
      "vmv.v.x     v16, %[tmp]                   \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "lb          %[vl], 16(%[c])               \n"
      "lb          %[tmp], 17(%[c])              \n"
      "vmv.v.x     v0, %[vl]                     \n"
      "vmv.v.x     v2, %[tmp]                    \n"
      "lb          %[vl], 18(%[c])               \n"
      "lb          %[tmp], 19(%[c])              \n"
      "vmv.v.x     v4, %[vl]                     \n"
      "vmv.v.x     v6, %[tmp]                    \n"
      "lb          %[vl], 32(%[c])               \n"
      "lb          %[tmp], 33(%[c])              \n"
      "vmv.v.x     v8, %[vl]                     \n"
      "vmv.v.x     v10, %[tmp]                   \n"
      "lb          %[vl], 34(%[c])               \n"
      "lb          %[tmp], 35(%[c])              \n"
      "vmv.v.x     v12, %[vl]                    \n"
      "vmv.v.x     v14, %[tmp]                   \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v24, (%[src_argb])            \n"
      "vwmulsu.vv  v20, v0, v24                  \n"
      "vwmaccsu.vv v20, v2, v26                  \n"
      "vwmaccsu.vv v20, v4, v28                  \n"
      "vwmaccsu.vv v20, v6, v30                  \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vsub.vv     v20, v16, v20                 \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v20, v20, 8                   \n"
      "vse8.v      v20, (%[dst_u])               \n"
      "vwmulsu.vv  v20, v8, v24                  \n"
      "vwmaccsu.vv v20, v10, v26                 \n"
      "vwmaccsu.vv v20, v12, v28                 \n"
      "vwmaccsu.vv v20, v14, v30                 \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vsub.vv     v20, v16, v20                 \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v20, v20, 8                   \n"
      "vse8.v      v20, (%[dst_v])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_u], %[dst_u], %[vl]     \n"
      "add         %[dst_v], %[dst_v], %[vl]     \n"
      "slli        %[tmp], %[vl], 2              \n"
      "add         %[src_argb], %[src_argb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_u] "+r"(dst_u),        // %[dst_u]
        [dst_v] "+r"(dst_v),        // %[dst_v]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl),             // %[vl]
        [tmp] "=&r"(tmp)            // %[tmp]
      : [c] "r"(c)                  // %[c]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17",
        "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27",
        "v28", "v29", "v30", "v31");
}
#endif

#ifdef HAS_ARGBTOUVMATRIXROW_RVV
void ARGBToUVMatrixRow_RVV(const uint8_t* src_argb,
                           int src_stride_argb,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width,
                           const struct ArgbConstants* c) {
  assert(width != 0);
  const uint8_t* src_argb_0 = src_argb;
  const uint8_t* src_argb_1 = src_argb + src_stride_argb;
  int w = width >> 1;
  if (w > 0) {
    size_t w_len = (size_t)w;
    size_t vl, tmp;
    asm volatile(
        "lh          %[tmp], 64(%[c])              \n"
        "vsetvli     zero, %[w_len], e16, m2, ta, ma\n"
        "vmv.v.x     v8, %[tmp]                    \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "lb          %[vl], 16(%[c])               \n"
        "lb          %[tmp], 17(%[c])              \n"
        "vmv.v.x     v10, %[vl]                    \n"
        "vmv.v.x     v11, %[tmp]                   \n"
        "lb          %[vl], 18(%[c])               \n"
        "lb          %[tmp], 19(%[c])              \n"
        "vmv.v.x     v12, %[vl]                    \n"
        "vmv.v.x     v13, %[tmp]                   \n"
        "lb          %[vl], 32(%[c])               \n"
        "lb          %[tmp], 33(%[c])              \n"
        "vmv.v.x     v14, %[vl]                    \n"
        "vmv.v.x     v15, %[tmp]                   \n"
        "lb          %[vl], 34(%[c])               \n"
        "lb          %[tmp], 35(%[c])              \n"
        "vmv.v.x     v16, %[vl]                    \n"
        "vmv.v.x     v17, %[tmp]                   \n"

        "1:          \n"
        "vsetvli     %[vl], %[w_len], e8, m1, ta, ma\n"
        "vlseg8e8.v  v20, (%[src_argb_0])          \n"
        "vlseg8e8.v  v0, (%[src_argb_1])           \n"
        "vwaddu.vv   v18, v20, v24                 \n"
        "vwaddu.wv   v18, v18, v0                  \n"
        "vwaddu.wv   v18, v18, v4                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v20, v18, 2                   \n"
        "vwaddu.vv   v18, v21, v25                 \n"
        "vwaddu.wv   v18, v18, v1                  \n"
        "vwaddu.wv   v18, v18, v5                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v21, v18, 2                   \n"
        "vwaddu.vv   v18, v22, v26                 \n"
        "vwaddu.wv   v18, v18, v2                  \n"
        "vwaddu.wv   v18, v18, v6                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v22, v18, 2                   \n"
        "vwaddu.vv   v18, v23, v27                 \n"
        "vwaddu.wv   v18, v18, v3                  \n"
        "vwaddu.wv   v18, v18, v7                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v23, v18, 2                   \n"
        "vwmulsu.vv  v18, v10, v20                 \n"
        "vwmaccsu.vv v18, v11, v21                 \n"
        "vwmaccsu.vv v18, v12, v22                 \n"
        "vwmaccsu.vv v18, v13, v23                 \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vsub.vv     v18, v8, v18                  \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v24, v18, 8                   \n"
        "vse8.v      v24, (%[dst_u])               \n"
        "vwmulsu.vv  v18, v14, v20                 \n"
        "vwmaccsu.vv v18, v15, v21                 \n"
        "vwmaccsu.vv v18, v16, v22                 \n"
        "vwmaccsu.vv v18, v17, v23                 \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vsub.vv     v18, v8, v18                  \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v20, v18, 8                   \n"
        "vse8.v      v20, (%[dst_v])               \n"
        "sub         %[w_len], %[w_len], %[vl]     \n"
        "add         %[dst_u], %[dst_u], %[vl]     \n"
        "add         %[dst_v], %[dst_v], %[vl]     \n"
        "slli        %[tmp], %[vl], 3              \n"
        "add         %[src_argb_0], %[src_argb_0], %[tmp]\n"
        "add         %[src_argb_1], %[src_argb_1], %[tmp]\n"
        "bgtz        %[w_len], 1b                  \n"
        : [src_argb_0] "+r"(src_argb_0),  // %[src_argb_0]
          [src_argb_1] "+r"(src_argb_1),  // %[src_argb_1]
          [dst_u] "+r"(dst_u),            // %[dst_u]
          [dst_v] "+r"(dst_v),            // %[dst_v]
          [w_len] "+r"(w_len),            // %[w_len]
          [vl] "=&r"(vl),                 // %[vl]
          [tmp] "=&r"(tmp)                // %[tmp]
        : [c] "r"(c)                      // %[c]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6",
          "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16",
          "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26",
          "v27");
  }
  if (width & 1) {
    uint8_t b = (src_argb_0[0] + src_argb_1[0] + 1) >> 1;
    uint8_t g = (src_argb_0[1] + src_argb_1[1] + 1) >> 1;
    uint8_t r = (src_argb_0[2] + src_argb_1[2] + 1) >> 1;
    uint8_t a = (src_argb_0[3] + src_argb_1[3] + 1) >> 1;
    dst_u[0] = (c->kAddUV[0] - (c->kRGBToU[0] * b + c->kRGBToU[1] * g +
                                c->kRGBToU[2] * r + c->kRGBToU[3] * a)) >>
               8;
    dst_v[0] = (c->kAddUV[0] - (c->kRGBToV[0] * b + c->kRGBToV[1] * g +
                                c->kRGBToV[2] * r + c->kRGBToV[3] * a)) >>
               8;
  }
}
#endif

#ifdef HAS_RGBTOUV444MATRIXROW_RVV
void RGBToUV444MatrixRow_RVV(const uint8_t* src_rgb,
                             uint8_t* dst_u,
                             uint8_t* dst_v,
                             int width,
                             const struct ArgbConstants* c) {
  assert(width != 0);
  size_t vl, tmp;
  asm volatile(
      "lh          %[tmp], 64(%[c])              \n"
      "vsetvli     zero, %[w], e16, m4, ta, ma   \n"
      "vmv.v.x     v16, %[tmp]                   \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "lb          %[vl], 16(%[c])               \n"
      "lb          %[tmp], 17(%[c])              \n"
      "vmv.v.x     v0, %[vl]                     \n"
      "vmv.v.x     v2, %[tmp]                    \n"
      "lb          %[vl], 18(%[c])               \n"
      "vmv.v.x     v4, %[vl]                     \n"
      "lb          %[vl], 32(%[c])               \n"
      "lb          %[tmp], 33(%[c])              \n"
      "vmv.v.x     v8, %[vl]                     \n"
      "vmv.v.x     v10, %[tmp]                   \n"
      "lb          %[vl], 34(%[c])               \n"
      "vmv.v.x     v12, %[vl]                    \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg3e8.v  v24, (%[src_rgb])             \n"
      "vwmulsu.vv  v20, v4, v28                  \n"
      "vwmaccsu.vv v20, v2, v26                  \n"
      "vwmaccsu.vv v20, v0, v24                  \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vsub.vv     v20, v16, v20                 \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v20, v20, 8                   \n"
      "vse8.v      v20, (%[dst_u])               \n"
      "vwmulsu.vv  v20, v12, v28                 \n"
      "vwmaccsu.vv v20, v10, v26                 \n"
      "vwmaccsu.vv v20, v8, v24                  \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vsub.vv     v20, v16, v20                 \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v20, v20, 8                   \n"
      "vse8.v      v20, (%[dst_v])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_u], %[dst_u], %[vl]     \n"
      "add         %[dst_v], %[dst_v], %[vl]     \n"
      "slli        %[tmp], %[vl], 1              \n"
      "add         %[src_rgb], %[src_rgb], %[vl] \n"
      "add         %[src_rgb], %[src_rgb], %[tmp]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_rgb] "+r"(src_rgb),  // %[src_rgb]
        [dst_u] "+r"(dst_u),      // %[dst_u]
        [dst_v] "+r"(dst_v),      // %[dst_v]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [tmp] "=&r"(tmp)          // %[tmp]
      : [c] "r"(c)                // %[c]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v8", "v9",
        "v10", "v11", "v12", "v13", "v16", "v17", "v18", "v19", "v20", "v21",
        "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29");
}
#endif

#ifdef HAS_RGBTOUVMATRIXROW_RVV
void RGBToUVMatrixRow_RVV(const uint8_t* src_rgb,
                          int src_stride_rgb,
                          uint8_t* dst_u,
                          uint8_t* dst_v,
                          int width,
                          const struct ArgbConstants* c) {
  assert(width != 0);
  const uint8_t* src_rgb_0 = src_rgb;
  const uint8_t* src_rgb_1 = src_rgb + src_stride_rgb;
  int w = width >> 1;
  if (w > 0) {
    size_t w_len = (size_t)w;
    size_t vl, tmp;
    asm volatile(
        "lh          %[tmp], 64(%[c])              \n"
        "vsetvli     zero, %[w_len], e16, m2, ta, ma\n"
        "vmv.v.x     v8, %[tmp]                    \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "lb          %[vl], 16(%[c])               \n"
        "lb          %[tmp], 17(%[c])              \n"
        "vmv.v.x     v10, %[vl]                    \n"
        "vmv.v.x     v11, %[tmp]                   \n"
        "lb          %[vl], 18(%[c])               \n"
        "vmv.v.x     v12, %[vl]                    \n"
        "lb          %[vl], 32(%[c])               \n"
        "lb          %[tmp], 33(%[c])              \n"
        "vmv.v.x     v14, %[vl]                    \n"
        "vmv.v.x     v15, %[tmp]                   \n"
        "lb          %[vl], 34(%[c])               \n"
        "vmv.v.x     v16, %[vl]                    \n"

        "1:          \n"
        "vsetvli     %[vl], %[w_len], e8, m1, ta, ma\n"
        "vlseg6e8.v  v20, (%[src_rgb_0])           \n"
        "vlseg6e8.v  v0, (%[src_rgb_1])            \n"
        "vwaddu.vv   v18, v20, v23                 \n"
        "vwaddu.wv   v18, v18, v0                  \n"
        "vwaddu.wv   v18, v18, v3                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v20, v18, 2                   \n"
        "vwaddu.vv   v18, v21, v24                 \n"
        "vwaddu.wv   v18, v18, v1                  \n"
        "vwaddu.wv   v18, v18, v4                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v21, v18, 2                   \n"
        "vwaddu.vv   v18, v22, v25                 \n"
        "vwaddu.wv   v18, v18, v2                  \n"
        "vwaddu.wv   v18, v18, v5                  \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vadd.vi     v18, v18, 2                   \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v22, v18, 2                   \n"
        "vwmulsu.vv  v18, v12, v22                 \n"
        "vwmaccsu.vv v18, v11, v21                 \n"
        "vwmaccsu.vv v18, v10, v20                 \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vsub.vv     v18, v8, v18                  \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v24, v18, 8                   \n"
        "vse8.v      v24, (%[dst_u])               \n"
        "vwmulsu.vv  v18, v16, v22                 \n"
        "vwmaccsu.vv v18, v15, v21                 \n"
        "vwmaccsu.vv v18, v14, v20                 \n"
        "vsetvli     zero, zero, e16, m2, ta, ma   \n"
        "vsub.vv     v18, v8, v18                  \n"
        "vsetvli     zero, zero, e8, m1, ta, ma    \n"
        "vnsrl.wi    v20, v18, 8                   \n"
        "vse8.v      v20, (%[dst_v])               \n"
        "sub         %[w_len], %[w_len], %[vl]     \n"
        "add         %[dst_u], %[dst_u], %[vl]     \n"
        "add         %[dst_v], %[dst_v], %[vl]     \n"
        "slli        %[tmp], %[vl], 1              \n"
        "add         %[tmp], %[tmp], %[vl]         \n"
        "slli        %[tmp], %[tmp], 1              \n"
        "add         %[src_rgb_0], %[src_rgb_0], %[tmp]\n"
        "add         %[src_rgb_1], %[src_rgb_1], %[tmp]\n"
        "bgtz        %[w_len], 1b                  \n"
        : [src_rgb_0] "+r"(src_rgb_0),  // %[src_rgb_0]
          [src_rgb_1] "+r"(src_rgb_1),  // %[src_rgb_1]
          [dst_u] "+r"(dst_u),          // %[dst_u]
          [dst_v] "+r"(dst_v),          // %[dst_v]
          [w_len] "+r"(w_len),          // %[w_len]
          [vl] "=&r"(vl),               // %[vl]
          [tmp] "=&r"(tmp)              // %[tmp]
        : [c] "r"(c)                    // %[c]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v8",
          "v9", "v10", "v11", "v12", "v14", "v15", "v16", "v18", "v19", "v20",
          "v21", "v22", "v23", "v24", "v25");
  }
  if (width & 1) {
    uint8_t b = (src_rgb_0[0] + src_rgb_1[0] + 1) >> 1;
    uint8_t g = (src_rgb_0[1] + src_rgb_1[1] + 1) >> 1;
    uint8_t r = (src_rgb_0[2] + src_rgb_1[2] + 1) >> 1;
    dst_u[0] = (c->kAddUV[0] -
                (c->kRGBToU[0] * b + c->kRGBToU[1] * g + c->kRGBToU[2] * r)) >>
               8;
    dst_v[0] = (c->kAddUV[0] -
                (c->kRGBToV[0] * b + c->kRGBToV[1] * g + c->kRGBToV[2] * r)) >>
               8;
  }
}
#endif

// Blend src_argb over src_argb1 and store to dst_argb.
// dst_argb may be src_argb or src_argb1.
// src_argb: RGB values have already been pre-multiplied by the a.
#ifdef HAS_ARGBBLENDROW_RVV
void ARGBBlendRow_RVV(const uint8_t* src_argb,
                      const uint8_t* src_argb1,
                      uint8_t* dst_argb,
                      int width) {
  size_t vl;
  asm volatile(
      "vsetvli     %[vl], zero, e8, m2, ta, ma   \n"
      "vmv.v.i     v14, -1                       \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v16, (%[src_argb])            \n"
      "vlseg4e8.v  v24, (%[src_argb1])           \n"
      "vmulhu.vv   v8, v24, v22                  \n"
      "vmulhu.vv   v10, v26, v22                 \n"
      "vmulhu.vv   v12, v28, v22                 \n"
      "vsub.vv     v8, v24, v8                   \n"
      "vsub.vv     v10, v26, v10                 \n"
      "vsub.vv     v12, v28, v12                 \n"
      "vsaddu.vv   v8, v8, v16                   \n"
      "vsaddu.vv   v10, v10, v18                 \n"
      "vsaddu.vv   v12, v12, v20                 \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "add         %[src_argb1], %[src_argb1], %[vl]\n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),    // %[src_argb]
        [src_argb1] "+r"(src_argb1),  // %[src_argb1]
        [dst_argb] "+r"(dst_argb),    // %[dst_argb]
        [w] "+r"(width),              // %[w]
        [vl] "=&r"(vl)                // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29", "v30", "v31");
}
#endif

#ifdef HAS_BLENDPLANEROW_RVV
void BlendPlaneRow_RVV(const uint8_t* src0,
                       const uint8_t* src1,
                       const uint8_t* alpha,
                       uint8_t* dst,
                       int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle8.v      v16, (%[src0])                \n"
      "vle8.v      v18, (%[src1])                \n"
      "vle8.v      v20, (%[alpha])               \n"
      "vrsub.vx    v22, v20, %[k255]             \n"
      "vwmulu.vv   v8, v20, v16                  \n"
      "vwmaccu.vv  v8, v22, v18                  \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vadd.vx     v8, v8, %[k255]               \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v16, v8, 8                    \n"
      "vse8.v      v16, (%[dst])                 \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src0], %[src0], %[vl]       \n"
      "add         %[src1], %[src1], %[vl]       \n"
      "add         %[alpha], %[alpha], %[vl]     \n"
      "add         %[dst], %[dst], %[vl]         \n"
      "bgtz        %[w], 1b                      \n"
      : [src0] "+r"(src0),    // %[src0]
        [src1] "+r"(src1),    // %[src1]
        [alpha] "+r"(alpha),  // %[alpha]
        [dst] "+r"(dst),      // %[dst]
        [w] "+r"(width),      // %[w]
        [vl] "=&r"(vl)        // %[vl]
      : [k255] "r"(255)
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23");
}
#endif

// Attenuate: (f * a + 255) >> 8
#ifdef HAS_ARGBATTENUATEROW_RVV
void ARGBAttenuateRow_RVV(const uint8_t* src_argb,
                          uint8_t* dst_argb,
                          int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vlseg4e8.v  v8, (%[src_argb])             \n"
      "vwmulu.vv   v16, v8, v14                  \n"
      "vwmulu.vv   v20, v10, v14                 \n"
      "vwmulu.vv   v24, v12, v14                 \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vadd.vx     v16, v16, %[k255]             \n"
      "vadd.vx     v20, v20, %[k255]             \n"
      "vadd.vx     v24, v24, %[k255]             \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v8, v16, 8                    \n"
      "vnsrl.wi    v10, v20, 8                   \n"
      "vnsrl.wi    v12, v24, 8                   \n"
      "vsseg4e8.v  v8, (%[dst_argb])             \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      : [k255] "r"(255)
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27");
}
#endif

#ifdef HAS_ARGBMULTIPLYROW_RVV
void ARGBMultiplyRow_RVV(const uint8_t* src_argb,
                         const uint8_t* src_argb1,
                         uint8_t* dst_argb,
                         int width) {
  assert(width != 0);
  size_t w = (size_t)width * 4;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
  size_t vl;
  asm("1:                                         \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vle8.v      v8, (%[src0])                  \n"
      "vle8.v      v10, (%[src1])                 \n"
      "vwmulu.vv   v12, v8, v10                   \n"
      "vwaddu.wx   v12, v12, %[k128]              \n"
      "vnsrl.wi    v8, v12, 8                     \n"
      "vse8.v      v8, (%[dst])                   \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[src0], %[src0], %[vl]        \n"
      "add         %[src1], %[src1], %[vl]        \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "bgtz        %[w], 1b                       \n"
      : [src0] "+r"(src_argb),   // %[src0]
        [src1] "+r"(src_argb1),  // %[src1]
        [dst] "+r"(dst_argb),    // %[dst]
        [w] "+r"(w),             // %[w]
        [vl] "=&r"(vl),          // %[vl]
        "=m"(*(uint8_t (*)[w])dst_argb)
      : [k128] "r"(128),  // %[k128]
        "m"(*(const uint8_t (*)[w])src_argb),
        "m"(*(const uint8_t (*)[w])src_argb1)
      : RVV_VL_CLOBBER "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15");
#pragma GCC diagnostic pop
}
#endif

#ifdef HAS_ARGBEXTRACTALPHAROW_RVV
void ARGBExtractAlphaRow_RVV(const uint8_t* src_argb,
                             uint8_t* dst_a,
                             int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m4, ta, ma  \n"
      "vle32.v     v8, (%[src_argb])             \n"
      "vnsrl.wi    v16, v8, 16                   \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wi    v8, v16, 8                    \n"
      "vse8.v      v8, (%[dst_a])                \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_a], %[dst_a], %[vl]     \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[src_argb], %[src_argb], %[vl]\n"
      "bgtz        %[w], 1b                      \n"
      : [src_argb] "+r"(src_argb),  // %[src_argb]
        [dst_a] "+r"(dst_a),        // %[dst_a]
        [w] "+r"(width),            // %[w]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19");
}
#endif

#ifdef HAS_ARGBCOPYYTOALPHAROW_RVV
void ARGBCopyYToAlphaRow_RVV(const uint8_t* src, uint8_t* dst, int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m8, ta, ma  \n"
      "vle8.v      v16, (%[src])                 \n"
      "vle32.v     v8, (%[dst])                  \n"
      "vzext.vf4   v24, v16                      \n"
      "vand.vx     v8, v8, %[mask]               \n"
      "vsll.vi     v24, v24, 24                  \n"
      "vor.vv      v8, v8, v24                   \n"
      "vse32.v     v8, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src], %[src], %[vl]         \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[dst], %[dst], %[vl]         \n"
      "bgtz        %[w], 1b                      \n"
      : [src] "+r"(src),         // %[src]
        [dst] "+r"(dst),         // %[dst]
        [w] "+r"(width),         // %[w]
        [vl] "=&r"(vl)           // %[vl]
      : [mask] "r"(0x00ffffffu)  // %[mask]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
        "v31");
}
#endif

#ifdef HAS_CONVERT16TO8ROW_RVV
void Convert16To8Row_RVV(const uint16_t* src_y,
                         uint8_t* dst_y,
                         int scale,
                         int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle16.v     v8, (%[src_y])                \n"
      "vnsrl.wx    v16, v8, %[shift]             \n"
      "vse8.v      v16, (%[dst_y])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_y], %[dst_y], %[vl]     \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),                             // %[src_y]
        [dst_y] "+r"(dst_y),                             // %[dst_y]
        [w] "+r"(width),                                 // %[w]
        [vl] "=&r"(vl)                                   // %[vl]
      : [shift] "r"(__builtin_clz((int32_t)scale) - 15)  // %[shift]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_CONVERT8TO16ROW_RVV
// Use scale to convert lsb formats to msb, depending how many bits there are:
// 512 = 9 bits
// 1024 = 10 bits
// 4096 = 12 bits
// 65536 = 16 bits
void Convert8To16Row_RVV(const uint8_t* src_y,
                         uint16_t* dst_y,
                         int bits,
                         int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma   \n"
      "vle8.v      v16, (%[src_y])               \n"
      "vwcvtu.x.x.v v8, v16                      \n"
      "vsetvli     zero, zero, e16, m4, ta, ma   \n"
      "vmul.vx     v8, v8, %[k0101]              \n"
      "vsrl.vx     v8, v8, %[shift]              \n"
      "vse16.v     v8, (%[dst_y])                \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[dst_y], %[dst_y], %[vl]     \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),  // %[src_y]
        [dst_y] "+r"(dst_y),  // %[dst_y]
        [w] "+r"(width),      // %[w]
        [vl] "=&r"(vl)        // %[vl]
      : [shift] "r"(16 - bits), [k0101] "r"(0x0101)
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_MULTIPLYROW_16_RVV
void MultiplyRow_16_RVV(const uint16_t* src_y,
                        uint16_t* dst_y,
                        int scale,
                        int width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m4, ta, ma  \n"
      "vle16.v     v8, (%[src_y])                \n"
      "vmul.vx     v8, v8, %[scale]              \n"
      "vse16.v     v8, (%[dst_y])                \n"
      "sub         %[w], %[w], %[vl]             \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_y], %[src_y], %[vl]     \n"
      "add         %[dst_y], %[dst_y], %[vl]     \n"
      "bgtz        %[w], 1b                      \n"
      : [src_y] "+r"(src_y),  // %[src_y]
        [dst_y] "+r"(dst_y),  // %[dst_y]
        [w] "+r"(width),      // %[w]
        [vl] "=&r"(vl)        // %[vl]
      : [scale] "r"(scale)    // %[scale]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11");
}
#endif

#ifdef HAS_HALFROW_16TO8_RVV
void HalfRow_16To8_RVV(const uint16_t* src_uv,
                       ptrdiff_t src_uv_stride,
                       uint8_t* dst_uv,
                       int scale,
                       int width) {
  int vl;
  asm volatile(
      "slli        %[src_uv1], %[src_uv1], 1     \n"
      "add         %[src_uv1], %[src_uv], %[src_uv1]\n"
      "csrwi       vxrm, 0                       \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m4, ta, ma  \n"
      "vle16.v     v8, (%[src_uv])               \n"
      "vle16.v     v12, (%[src_uv1])             \n"
      "vaaddu.vv   v8, v8, v12                   \n"
      "vsetvli     zero, zero, e8, m2, ta, ma    \n"
      "vnsrl.wx    v16, v8, %[shift]             \n"
      "vse8.v      v16, (%[dst_uv])              \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_uv], %[dst_uv], %[vl]   \n"
      "slli        %[vl], %[vl], 1               \n"
      "add         %[src_uv], %[src_uv], %[vl]   \n"
      "add         %[src_uv1], %[src_uv1], %[vl] \n"
      "bgtz        %[w], 1b                      \n"
      : [src_uv] "+r"(src_uv),                           // %[src_uv]
        [src_uv1] "+r"(src_uv_stride),                   // %[src_uv1]
        [dst_uv] "+r"(dst_uv),                           // %[dst_uv]
        [w] "+r"(width),                                 // %[w]
        [vl] "=&r"(vl)                                   // %[vl]
      : [shift] "r"(__builtin_clz((int32_t)scale) - 15)  // %[shift]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17");
}
#endif

#ifdef HAS_HALFWIDTHROW_16TO8_RVV
void HalfWidthRow_16To8_RVV(const uint16_t* src_uv,
                            ptrdiff_t src_uv_stride,
                            uint8_t* dst_uv,
                            int scale,
                            int width) {
  int vl;
  asm volatile(
      "slli        %[t], %[t], 1                 \n"
      "add         %[t], %[s], %[t]              \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m2, ta, ma  \n"
      "vlseg2e16.v v16, (%[s])                   \n"
      "vlseg2e16.v v20, (%[t])                   \n"
      "vwaddu.vx   v8, v16, %[c2]                \n"
      "vwaddu.wv   v8, v8, v18                   \n"
      "vwaddu.wv   v8, v8, v20                   \n"
      "vwaddu.wv   v8, v8, v22                   \n"
      "vnsrl.wi    v16, v8, 2                    \n"
      "vsetvli     zero, zero, e8, m1, ta, ma    \n"
      "vnsrl.wx    v8, v16, %[shift]             \n"
      "vse8.v      v8, (%[dst_uv])               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "add         %[dst_uv], %[dst_uv], %[vl]   \n"
      "slli        %[vl], %[vl], 2               \n"
      "add         %[s], %[s], %[vl]             \n"
      "add         %[t], %[t], %[vl]             \n"
      "bgtz        %[w], 1b                      \n"
      : [s] "+r"(src_uv),                                 // %[s]
        [t] "+r"(src_uv_stride),                          // %[t]
        [dst_uv] "+r"(dst_uv),                            // %[dst_uv]
        [w] "+r"(width),                                  // %[w]
        [vl] "=&r"(vl)                                    // %[vl]
      : [shift] "r"(__builtin_clz((int32_t)scale) - 15),  // %[shift]
        [c2] "r"(2)                                       // %[c2]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_ARGBMIRRORROW_RVV
void ARGBMirrorRow_RVV(const uint8_t* src, uint8_t* dst, int width) {
  int vl;
  ptrdiff_t byte_vl;
  src += (ptrdiff_t)width * 4;
  asm volatile(
      "vsetvli     %[vl], %[w], e32, m8, ta, ma  \n"
      "slli        %[byte_vl], %[vl], 2          \n"
      "vid.v       v16                           \n"
      "addi        %[vl], %[vl], -1              \n"
      "vrsub.vx    v16, v16, %[vl]               \n"
      "addi        %[vl], %[vl], 1               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "bltz        %[w], 2f                      \n"
      "1:                                        \n"
      "sub         %[src], %[src], %[byte_vl]    \n"
      "vle32.v     v8, (%[src])                  \n"
      "vrgather.vv v0, v8, v16                   \n"
      "vse32.v     v0, (%[dst])                  \n"
      "add         %[dst], %[dst], %[byte_vl]    \n"
      "sub         %[w], %[w], %[vl]             \n"
      "bgez        %[w], 1b                      \n"
      "2:                                        \n"
      "add         %[w], %[w], %[vl]             \n"
      "beqz        %[w], 3f                      \n"
      "vsetvli     %[vl], %[w], e32, m8, ta, ma  \n"
      "slli        %[byte_vl], %[vl], 2          \n"
      "vid.v       v16                           \n"
      "addi        %[vl], %[vl], -1              \n"
      "vrsub.vx    v16, v16, %[vl]               \n"
      "sub         %[src], %[src], %[byte_vl]    \n"
      "vle32.v     v8, (%[src])                  \n"
      "vrgather.vv v0, v8, v16                   \n"
      "vse32.v     v0, (%[dst])                  \n"
      "3:                                        \n"
      : [src] "+r"(src),          // %[src]
        [dst] "+r"(dst),          // %[dst]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [byte_vl] "=&r"(byte_vl)  // %[byte_vl]
      :
      : RVV_VL_CLOBBER "vtype", "memory", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#endif  // HAS_ARGBMIRRORROW_RVV

#ifdef HAS_MIRRORUVROW_RVV
void MirrorUVRow_RVV(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  int vl;
  ptrdiff_t byte_vl;
  src_uv += (ptrdiff_t)width * 2;
  asm volatile(
      "vsetvli     %[vl], %[w], e16, m8, ta, ma  \n"
      "slli        %[byte_vl], %[vl], 1          \n"
      "vid.v       v16                           \n"
      "addi        %[vl], %[vl], -1              \n"
      "vrsub.vx    v16, v16, %[vl]               \n"
      "addi        %[vl], %[vl], 1               \n"
      "sub         %[w], %[w], %[vl]             \n"
      "bltz        %[w], 2f                      \n"
      "1:                                        \n"
      "sub         %[src], %[src], %[byte_vl]    \n"
      "vle16.v     v8, (%[src])                  \n"
      "vrgather.vv v0, v8, v16                   \n"
      "vse16.v     v0, (%[dst])                  \n"
      "add         %[dst], %[dst], %[byte_vl]    \n"
      "sub         %[w], %[w], %[vl]             \n"
      "bgez        %[w], 1b                      \n"
      "2:                                        \n"
      "add         %[w], %[w], %[vl]             \n"
      "beqz        %[w], 3f                      \n"
      "vsetvli     %[vl], %[w], e16, m8, ta, ma  \n"
      "slli        %[byte_vl], %[vl], 1          \n"
      "vid.v       v16                           \n"
      "addi        %[vl], %[vl], -1              \n"
      "vrsub.vx    v16, v16, %[vl]               \n"
      "sub         %[src], %[src], %[byte_vl]    \n"
      "vle16.v     v8, (%[src])                  \n"
      "vrgather.vv v0, v8, v16                   \n"
      "vse16.v     v0, (%[dst])                  \n"
      "3:                                        \n"
      : [src] "+r"(src_uv),       // %[src]
        [dst] "+r"(dst_uv),       // %[dst]
        [w] "+r"(width),          // %[w]
        [vl] "=&r"(vl),           // %[vl]
        [byte_vl] "=&r"(byte_vl)  // %[byte_vl]
      :
      : RVV_VL_CLOBBER "vtype", "memory", "v0", "v1", "v2", "v3", "v4", "v5",
        "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}
#endif  // HAS_MIRRORUVROW_RVV

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
