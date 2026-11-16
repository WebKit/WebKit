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
#include "libyuv/scale_row.h"

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for RVV (RISC-V Vector extension)
#if !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
#include <assert.h>
#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

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

#ifdef HAS_SCALEARGBFILTERCOLS_RVV
void ScaleARGBFilterCols_RVV(uint8_t* dst_argb,
                             const uint8_t* src_argb,
                             int dst_width,
                             int x,
                             int dx) {
  assert(x >= 0);
  int vl, vl_dx;
  asm volatile(
      "vsetvli     %[vl], %[w], e32, m2, ta, ma   \n"
      "vmv.v.x     v0, %[x]                       \n"
      "vid.v       v2                             \n"
      "vmacc.vx    v0, %[dx], v2                  \n"

      "1:          \n"
      // idx is x >> 16, byte index is (x >> 14) & ~3u
      "vsrl.vi     v2, v0, 14                     \n"
      "vand.vi     v2, v2, -4                     \n"
      // Read Packed ARGB w/ byte index.
      "vluxseg2ei32.v v4, (%[src_argb]), v2        \n"
      // xf = (x >> 9) & 0x7f;
      "vsrl.vi     v8, v0, 9                      \n"
      "vand.vx     v8, v8, %[c_7f]                \n"
      "mul         %[vl_dx], %[vl], %[dx]         \n"
      "vadd.vx     v0, v0, %[vl_dx]               \n"
      // duplicate v_xf0_u32[i] from {0,0,0,f[i]} to {f[i],f[i],f[i],f[i]}
      "vmul.vx     v8, v8, %[c_01010101]          \n"
      // TODO(fbarchard): Replace 0x7f ^ f with 128-f.  bug=607.
      "vxor.vx     v10, v8, %[c_7f7f7f7f]         \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 2                \n"
      "vsetvli     zero, %[vl], e8, m2, ta, ma    \n"
      // ((a) * (0x7f ^ f) + (b)*f) >> 7
      "vwmulu.vv   v12, v4, v10                   \n"
      "vwmaccu.vv  v12, v6, v8                    \n"
      "vnsrl.wi    v4, v12, 7                     \n"
      "vse8.v      v4, (%[dst_argb])              \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "vsetvli     %[vl], %[w], e32, m2, ta, ma   \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),            // %[w]
        [dst_argb] "+r"(dst_argb),      // %[dst_argb]
        [vl] "=&r"(vl),                 // %[vl]
        [vl_dx] "=&r"(vl_dx)            // %[vl_dx]
      : [src_argb] "r"(src_argb),       // %[src_argb]
        [x] "r"(x),                     // %[x]
        [dx] "r"(dx),                   // %[dx]
        [c_7f] "r"(0x7f),               // %[c_7f]
        [c_01010101] "r"(0x01010101u),  // %[c_01010101]
        [c_7f7f7f7f] "r"(0x7f7f7f7fu)   // %[c_7f7f7f7f]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15");
}
#endif

#ifdef HAS_SCALEADDROW_RVV
void ScaleAddRow_RVV(const uint8_t* src_ptr, uint16_t* dst_ptr, int src_width) {
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vle16.v     v8, (%[dst_ptr])               \n"
      "vle8.v      v16, (%[src_ptr])              \n"
      // Use widening multiply-add instead of widening + add
      "vwmaccu.vx  v8, %[one], v16                \n"
      "vse16.v     v8, (%[dst_ptr])               \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(src_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),  // %[dst_ptr]
        [vl] "=&r"(vl)            // %[vl]
      : [one] "r"(1)              // %[one]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_SCALEARGBROWDOWN2_RVV
void ScaleARGBRowDown2_RVV(const uint8_t* src_argb,
                           ptrdiff_t src_stride,
                           uint8_t* dst_argb,
                           int dst_width) {
  (void)src_stride;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma   \n"
      "vlseg2e32.v v8, (%[src])                   \n"
      "vse32.v     v12, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src], %[src], %[vl]          \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),   // %[w]
        [src] "+r"(src_argb),  // %[src]
        [dst] "+r"(dst_argb),  // %[dst]
        [vl] "=&r"(vl)         // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEARGBROWDOWN2LINEAR_RVV
void ScaleARGBRowDown2Linear_RVV(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  (void)src_stride;
  int vl;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile(
      "csrwi       vxrm, 0                        \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma   \n"
      "vlseg2e32.v v8, (%[src])                   \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 2                \n"
      "vsetvli     zero, %[vl], e8, m4, ta, ma    \n"
      "vaaddu.vv   v8, v8, v12                    \n"
      "vse8.v      v8, (%[dst_argb])              \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src], %[src], %[vl]          \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),        // %[w]
        [src] "+r"(src_argb),       // %[src]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [vl] "=&r"(vl)              // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEARGBROWDOWN2BOX_RVV
void ScaleARGBRowDown2Box_RVV(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              uint8_t* dst_argb,
                              int dst_width) {
  int vl;
  asm volatile(
      "add         %[src1], %[src0], %[src1]      \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m2, ta, ma   \n"
      "vlseg2e32.v v16, (%[src0])                 \n"
      "vlseg2e32.v v20, (%[src1])                 \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 2                \n"
      "vsetvli     zero, %[vl], e8, m2, ta, ma    \n"
      "vwaddu.vx   v8, v16, %[c2]                 \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v20                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vnsrl.wi    v16, v8, 2                     \n"
      "vse8.v      v16, (%[dst_argb])             \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src0], %[src0], %[vl]        \n"
      "add         %[src1], %[src1], %[vl]        \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),        // %[w]
        [src0] "+r"(src_argb),      // %[src0]
        [src1] "+r"(src_stride),    // %[src1]
        [dst_argb] "+r"(dst_argb),  // %[dst_argb]
        [vl] "=&r"(vl)              // %[vl]
      : [c2] "r"(2)                 // %[c2]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_SCALEARGBROWDOWNEVEN_RVV
void ScaleARGBRowDownEven_RVV(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              int src_stepx,
                              uint8_t* dst_argb,
                              int dst_width) {
  (void)src_stride;
  int vl;
  ptrdiff_t src_step;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m4, ta, ma   \n"
      "vlse32.v    v8, (%[src]), %[stride_byte]   \n"
      "vse32.v     v8, (%[dst])                   \n"
      "sub         %[w], %[w], %[vl]              \n"
      "mul         %[src_step], %[vl], %[stride_byte]\n"
      "add         %[src], %[src], %[src_step]    \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),                         // %[w]
        [src] "+r"(src_argb),                        // %[src]
        [dst] "+r"(dst_argb),                        // %[dst]
        [vl] "=&r"(vl),                              // %[vl]
        [src_step] "=&r"(src_step)                   // %[src_step]
      : [stride_byte] "r"((ptrdiff_t)src_stepx * 4)  // %[stride_byte]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11");
}
#endif

#ifdef HAS_SCALEARGBROWDOWNEVENBOX_RVV
void ScaleARGBRowDownEvenBox_RVV(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 int src_stepx,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  int vl;
  ptrdiff_t src_step;
  asm volatile(
      "add         %[src1], %[src0], %[src1]      \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m2, ta, ma   \n"
      "vlsseg2e32.v v16, (%[src0]), %[stride_byte] \n"
      "vlsseg2e32.v v20, (%[src1]), %[stride_byte] \n"
      "sub         %[w], %[w], %[vl]              \n"
      "mul         %[src_step], %[vl], %[stride_byte]\n"
      "slli        %[vl], %[vl], 2                \n"
      "vsetvli     zero, %[vl], e8, m2, ta, ma    \n"
      "vwaddu.vx   v8, v16, %[c2]                 \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v20                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vnsrl.wi    v16, v8, 2                     \n"
      "vse8.v      v16, (%[dst_argb])             \n"
      "add         %[src0], %[src0], %[src_step]  \n"
      "add         %[src1], %[src1], %[src_step]  \n"
      "add         %[dst_argb], %[dst_argb], %[vl]\n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),                          // %[w]
        [src0] "+r"(src_argb),                        // %[src0]
        [src1] "+r"(src_stride),                      // %[src1]
        [dst_argb] "+r"(dst_argb),                    // %[dst_argb]
        [vl] "=&r"(vl),                               // %[vl]
        [src_step] "=&r"(src_step)                    // %[src_step]
      : [stride_byte] "r"((ptrdiff_t)src_stepx * 4),  // %[stride_byte]
        [c2] "r"(2)                                   // %[c2]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_SCALEROWDOWN2_RVV
void ScaleRowDown2_RVV(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       int dst_width) {
  (void)src_stride;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vle16.v     v8, (%[src_ptr])               \n"
      "vnsrl.wi    v16, v8, 8                     \n"
      "vse8.v      v16, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst] "+r"(dst),          // %[dst]
        [vl] "=&r"(vl)            // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_SCALEROWDOWN2LINEAR_RVV
void ScaleRowDown2Linear_RVV(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst,
                             int dst_width) {
  (void)src_stride;
  int vl;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile(
      "csrwi       vxrm, 0                        \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vle16.v     v8, (%[src_ptr])               \n"
      "vnsrl.wi    v16, v8, 0                     \n"
      "vnsrl.wi    v18, v8, 8                     \n"
      "vaaddu.vv   v16, v16, v18                  \n"
      "vse8.v      v16, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst] "+r"(dst),          // %[dst]
        [vl] "=&r"(vl)            // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19");
}
#endif

#ifdef HAS_SCALEROWDOWN2BOX_RVV
void ScaleRowDown2Box_RVV(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          int dst_width) {
  int vl;
  asm volatile(
      "add         %[t], %[s], %[t]               \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg2e8.v  v16, (%[s])                    \n"
      "vlseg2e8.v  v20, (%[t])                    \n"
      "vwaddu.vx   v8, v16, %[c2]                 \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v20                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vnsrl.wi    v16, v8, 2                     \n"
      "vse8.v      v16, (%[dst])                  \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst], %[dst], %[vl]          \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[s], %[s], %[vl]              \n"
      "add         %[t], %[t], %[vl]              \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),   // %[w]
        [s] "+r"(src_ptr),     // %[s]
        [t] "+r"(src_stride),  // %[t]
        [dst] "+r"(dst),       // %[dst]
        [vl] "=&r"(vl)         // %[vl]
      : [c2] "r"(2)            // %[c2]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23");
}
#endif

#ifdef HAS_SCALEROWDOWN4_RVV
void ScaleRowDown4_RVV(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst_ptr,
                       int dst_width) {
  (void)src_stride;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v8, (%[src_ptr])               \n"
      "vse8.v      v12, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),  // %[dst_ptr]
        [vl] "=&r"(vl)            // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEROWDOWN4BOX_RVV
void ScaleRowDown4Box_RVV(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst_ptr,
                          int dst_width) {
  const uint8_t* src_ptr1;
  const uint8_t* src_ptr2;
  int vl;
  asm volatile(
      "add         %[src_ptr1], %[src_ptr], %[src_ptr3]\n"
      "add         %[src_ptr2], %[src_ptr1], %[src_ptr3]\n"
      "add         %[src_ptr3], %[src_ptr2], %[src_ptr3]\n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v16, (%[src_ptr])              \n"
      "vlseg4e8.v  v24, (%[src_ptr1])             \n"
      "vwaddu.vx   v8, v16, %[c8]                 \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v20                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vwaddu.wv   v8, v8, v24                    \n"
      "vwaddu.wv   v8, v8, v26                    \n"
      "vwaddu.wv   v8, v8, v28                    \n"
      "vwaddu.wv   v8, v8, v30                    \n"
      "vlseg4e8.v  v16, (%[src_ptr2])             \n"
      "vlseg4e8.v  v24, (%[src_ptr3])             \n"
      "vwaddu.wv   v8, v8, v16                    \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v20                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vwaddu.wv   v8, v8, v24                    \n"
      "vwaddu.wv   v8, v8, v26                    \n"
      "vwaddu.wv   v8, v8, v28                    \n"
      "vwaddu.wv   v8, v8, v30                    \n"
      "vnsrl.wi    v16, v8, 4                     \n"
      "vse8.v      v16, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "add         %[src_ptr1], %[src_ptr1], %[vl]\n"
      "add         %[src_ptr2], %[src_ptr2], %[vl]\n"
      "add         %[src_ptr3], %[src_ptr3], %[vl]\n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),          // %[w]
        [src_ptr] "+r"(src_ptr),      // %[src_ptr]
        [src_ptr1] "=&r"(src_ptr1),   // %[src_ptr1]
        [src_ptr2] "=&r"(src_ptr2),   // %[src_ptr2]
        [src_ptr3] "+r"(src_stride),  // %[src_ptr3]
        [dst_ptr] "+r"(dst_ptr),      // %[dst_ptr]
        [vl] "=&r"(vl)                // %[vl]
      : [c8] "r"(8)                   // %[c8]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28",
        "v29", "v30", "v31");
}
#endif

#ifdef HAS_SCALEROWDOWN34_RVV
void ScaleRowDown34_RVV(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  dst_width /= 3;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v8, (%[src_ptr])               \n"
      "vmv2r.v     v12, v14                       \n"
      "vsseg3e8.v  v8, (%[dst_ptr])               \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),  // %[dst_ptr]
        [vl] "=&r"(vl)            // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEROWDOWN34_0_BOX_RVV
void ScaleRowDown34_0_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  dst_width /= 3;
  const uint8_t* t;
  int vl;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile(
      "add         %[t], %[s], %[src_stride]      \n"
      "csrwi       vxrm, 0                        \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v24, (%[s])                    \n"
      "bnez        %[src_stride], 2f              \n"
      "vwaddu.vx   v20, v24, %[c2]                \n"
      "vwaddu.vx   v16, v26, %[c2]                \n"
      "vwaddu.vx   v12, v28, %[c2]                \n"
      "vwaddu.vx   v8, v30, %[c2]                 \n"
      "j           3f                             \n"

      "2:          \n"
      "vlseg4e8.v  v0, (%[t])                     \n"
      "vwaddu.vx   v20, v0, %[c2]                 \n"
      "vwaddu.vx   v16, v2, %[c2]                 \n"
      "vwaddu.vx   v12, v4, %[c2]                 \n"
      "vwaddu.vx   v8, v6, %[c2]                  \n"

      "3:          \n"
      "vwmaccu.vx  v20, %[c3], v24                \n"
      "vnsrl.wi    v24, v20, 2                    \n"
      "vwmaccu.vx  v16, %[c3], v26                \n"
      "vnsrl.wi    v20, v16, 2                    \n"
      "vwmaccu.vx  v12, %[c3], v28                \n"
      "vnsrl.wi    v16, v12, 2                    \n"
      "vwmaccu.vx  v8, %[c3], v30                 \n"
      "vnsrl.wi    v12, v8, 2                     \n"
      // a0 = (src[0] * 3 + s[1] * 1 + 2) >> 2
      "vwaddu.vx   v8, v20, %[c2]                 \n"
      "vwmaccu.vx  v8, %[c3], v24                 \n"
      "vnsrl.wi    v14, v8, 2                     \n"
      // a2 = (src[2] * 1 + s[3] * 3 + 2) >> 2
      "vwaddu.vx   v8, v16, %[c2]                 \n"
      "vwmaccu.vx  v8, %[c3], v12                 \n"
      // a1 = (src[1] * 1 + s[2] * 1 + 1) >> 1
      "vaaddu.vv   v16, v20, v16                  \n"
      "vnsrl.wi    v18, v8, 2                     \n"
      "vsseg3e8.v  v14, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[s], %[s], %[vl]              \n"
      "add         %[t], %[t], %[vl]              \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),           // %[w]
        [s] "+r"(src_ptr),             // %[s]
        [t] "=&r"(t),                  // %[t]
        [dst_ptr] "+r"(dst_ptr),       // %[dst_ptr]
        [vl] "=&r"(vl)                 // %[vl]
      : [src_stride] "r"(src_stride),  // %[src_stride]
        [c2] "r"(2),                   // %[c2]
        [c3] "r"(3)                    // %[c3]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17",
        "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27",
        "v28", "v29", "v30", "v31");
}
#endif

#ifdef HAS_SCALEROWDOWN34_1_BOX_RVV
void ScaleRowDown34_1_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  dst_width /= 3;
  const uint8_t* t;
  int vl;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile(
      "add         %[t], %[s], %[src_stride]      \n"
      "csrwi       vxrm, 0                        \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v14, (%[s])                    \n"
      "bnez        %[src_stride], 2f              \n"
      "vaaddu.vv   v14, v14, v14                  \n"
      "vaaddu.vv   v8, v16, v16                   \n"
      "vaaddu.vv   v10, v18, v18                  \n"
      "vaaddu.vv   v12, v20, v20                  \n"
      "j           3f                             \n"

      "2:          \n"
      "vlseg4e8.v  v22, (%[t])                    \n"
      "vaaddu.vv   v14, v14, v22                  \n"
      "vaaddu.vv   v8, v16, v24                   \n"
      "vaaddu.vv   v10, v18, v26                  \n"
      "vaaddu.vv   v12, v20, v28                  \n"

      "3:          \n"
      // a0 = (src[0] * 3 + s[1] * 1 + 2) >> 2
      "vwaddu.vx   v16, v8, %[c2]                 \n"
      "vwmaccu.vx  v16, %[c3], v14                \n"
      "vnsrl.wi    v20, v16, 2                    \n"
      // a2 = (src[2] * 1 + s[3] * 3 + 2) >> 2
      "vwaddu.vx   v16, v10, %[c2]                \n"
      "vwmaccu.vx  v16, %[c3], v12                \n"
      // a1 = (src[1] * 1 + s[2] * 1 + 1) >> 1
      "vaaddu.vv   v22, v8, v10                   \n"
      "vnsrl.wi    v24, v16, 2                    \n"
      "vsseg3e8.v  v20, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[s], %[s], %[vl]              \n"
      "add         %[t], %[t], %[vl]              \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),           // %[w]
        [s] "+r"(src_ptr),             // %[s]
        [t] "=&r"(t),                  // %[t]
        [dst_ptr] "+r"(dst_ptr),       // %[dst_ptr]
        [vl] "=&r"(vl)                 // %[vl]
      : [src_stride] "r"(src_stride),  // %[src_stride]
        [c2] "r"(2),                   // %[c2]
        [c3] "r"(3)                    // %[c3]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29");
}
#endif

#ifdef HAS_SCALEROWDOWN38_RVV
void ScaleRowDown38_RVV(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  (void)src_stride;
  assert(dst_width % 3 == 0);
  dst_width /= 3;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma    \n"
      "vlseg8e8.v  v8, (%[src_ptr])               \n"
      "vmv1r.v     v9, v11                        \n"
      "vmv1r.v     v10, v14                       \n"
      "vsseg3e8.v  v8, (%[dst_ptr])               \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),      // %[w]
        [src_ptr] "+r"(src_ptr),  // %[src_ptr]
        [dst_ptr] "+r"(dst_ptr),  // %[dst_ptr]
        [vl] "=&r"(vl)            // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEROWDOWN38_2_BOX_RVV
void ScaleRowDown38_2_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  dst_width /= 3;
  int vl;
  asm volatile(
      "add         %[src_ptr1], %[src_ptr], %[src_ptr1]\n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma    \n"
      // s: e00, e10, e20, f00, f10, f20, g00, g10
      "vlseg8e8.v  v14, (%[src_ptr])              \n"
      // t: e01, e11, e21, f01, f11, f21, g01, g11
      "vlseg8e8.v  v22, (%[src_ptr1])             \n"
      "vwaddu.vv   v8, v14, v22                   \n"
      "vwaddu.vv   v10, v15, v23                  \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v8, v8, v10                    \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v10, v17, v25                  \n"
      "vwaddu.vv   v12, v18, v26                  \n"
      "vwaddu.vv   v14, v16, v24                  \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v10, v10, v12                  \n"
      "vadd.vv     v8, v8, v14                    \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v12, v19, v27                  \n"
      "vwaddu.vv   v14, v20, v28                  \n"
      "vwaddu.vv   v16, v21, v29                  \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v10, v10, v12                  \n"
      "vadd.vv     v12, v14, v16                  \n"
      // Average in 16-bit fixed-point
      "vmulhu.vx   v8, v8, %[coeff_a]             \n"
      "vmulhu.vx   v10, v10, %[coeff_a]           \n"
      "vmulhu.vx   v12, v12, %[coeff_b]           \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vnsrl.wi    v14, v8, 0                     \n"
      "vnsrl.wi    v15, v10, 0                    \n"
      "vnsrl.wi    v16, v12, 0                    \n"
      "vsseg3e8.v  v14, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "add         %[src_ptr1], %[src_ptr1], %[vl]\n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),          // %[w]
        [src_ptr] "+r"(src_ptr),      // %[src_ptr]
        [src_ptr1] "+r"(src_stride),  // %[src_ptr1]
        [dst_ptr] "+r"(dst_ptr),      // %[dst_ptr]
        [vl] "=&r"(vl)                // %[vl]
      : [coeff_a] "r"(65536u / 6u),   // %[coeff_a]
        [coeff_b] "r"(65536u / 4u)    // %[coeff_b]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29");
}
#endif

#ifdef HAS_SCALEROWDOWN38_3_BOX_RVV
void ScaleRowDown38_3_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  dst_width /= 3;
  const uint8_t* src_ptr1;
  int vl;
  asm volatile(
      "add         %[src_ptr1], %[src_ptr], %[src_ptr2]\n"
      "add         %[src_ptr2], %[src_ptr1], %[src_ptr2]\n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m1, ta, ma    \n"
      // s: e00, e10, e20, f00, f10, f20, g00, g10
      "vlseg8e8.v  v8, (%[src_ptr])               \n"
      // t: e01, e11, e21, f01, f11, f21, g01, g11
      "vlseg8e8.v  v19, (%[src_ptr1])             \n"
      // u: e02, e12, e22, f02, f12, f22, g02, g12
      "vlseg8e8.v  v0, (%[src_ptr2])              \n"
      // Calculate sum of [e00, e22]
      "vwaddu.vv   v16, v8, v19                   \n"
      "vwaddu.vv   v18, v9, v20                   \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v8, v16, v18                   \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v16, v10, v21                  \n"
      "vwaddu.vv   v18, v0, v1                    \n"
      "vwcvtu.x.x.v v20, v2                        \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v16, v16, v18                  \n"
      "vadd.vv     v8, v8, v20                    \n"
      "vadd.vv     v8, v8, v16                    \n"
      // Calculate sum of [f00, f22]
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v16, v11, v22                  \n"
      "vwaddu.vv   v10, v12, v23                  \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v10, v16, v10                  \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v16, v13, v24                  \n"
      "vwaddu.vv   v12, v3, v4                    \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v12, v16, v12                  \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwcvtu.x.x.v v16, v5                        \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v10, v10, v16                  \n"
      "vadd.vv     v10, v10, v12                  \n"
      // Calculate sum of [g00, g12]
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vwaddu.vv   v12, v14, v25                  \n"
      "vwaddu.vv   v16, v15, v26                  \n"
      "vwaddu.vv   v14, v6, v7                    \n"
      "vsetvli     zero, zero, e16, m2, ta, ma    \n"
      "vadd.vv     v12, v12, v16                  \n"
      "vadd.vv     v12, v12, v14                  \n"
      // Average in 16-bit fixed-point
      "vmulhu.vx   v8, v8, %[coeff_a]             \n"
      "vmulhu.vx   v10, v10, %[coeff_a]           \n"
      "vmulhu.vx   v12, v12, %[coeff_b]           \n"
      "vsetvli     zero, zero, e8, m1, ta, ma     \n"
      "vnsrl.wi    v14, v8, 0                     \n"
      "vnsrl.wi    v15, v10, 0                    \n"
      "vnsrl.wi    v16, v12, 0                    \n"
      "vsseg3e8.v  v14, (%[dst_ptr])              \n"
      "sub         %[w], %[w], %[vl]              \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_ptr], %[dst_ptr], %[vl]  \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_ptr], %[src_ptr], %[vl]  \n"
      "add         %[src_ptr1], %[src_ptr1], %[vl]\n"
      "add         %[src_ptr2], %[src_ptr2], %[vl]\n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),          // %[w]
        [src_ptr] "+r"(src_ptr),      // %[src_ptr]
        [src_ptr1] "=&r"(src_ptr1),   // %[src_ptr1]
        [src_ptr2] "+r"(src_stride),  // %[src_ptr2]
        [dst_ptr] "+r"(dst_ptr),      // %[dst_ptr]
        [vl] "=&r"(vl)                // %[vl]
      : [coeff_a] "r"(65536u / 9u),   // %[coeff_a]
        [coeff_b] "r"(65536u / 6u)    // %[coeff_b]
      : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17",
        "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26");
}
#endif

// ScaleUVRowUp2_(Bi)linear_RVV function is equal to other platforms'
// ScaleRowUp2_(Bi)linear_Any_XXX. We process entire row in this function. Other
// platforms only implement non-edge part of image and process edge with scalar.

#ifdef HAS_SCALEROWUP2_LINEAR_RVV
void ScaleRowUp2_Linear_RVV(const uint8_t* src_ptr,
                            uint8_t* dst_ptr,
                            int dst_width) {
  int src_width = (dst_width - 1) >> 1;
  dst_ptr[0] = src_ptr[0];
  if (src_width > 0) {
    int vl;
    const uint8_t* work_src_next;
    uint8_t* work_dst_ptr = dst_ptr + 1;
    asm volatile(
        "1:          \n"
        "vsetvli     %[vl], %[src_width], e8, m2, ta, ma\n"
        "vle8.v      v12, (%[src_ptr])              \n"
        "addi        %[work_src_next], %[src_ptr], 1\n"
        "vle8.v      v24, (%[work_src_next])        \n"
        "vwaddu.vx   v16, v12, %[c2]                \n"
        "vwmaccu.vx  v16, %[c3], v24                \n"
        "vnsrl.wi    v26, v16, 2                    \n"
        "vwaddu.vx   v16, v24, %[c2]                \n"
        "vwmaccu.vx  v16, %[c3], v12                \n"
        "vnsrl.wi    v24, v16, 2                    \n"
        "vsseg2e8.v  v24, (%[work_dst_ptr])         \n"
        "sub         %[src_width], %[src_width], %[vl]\n"
        "add         %[src_ptr], %[src_ptr], %[vl]  \n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[work_dst_ptr], %[work_dst_ptr], %[vl]\n"
        "bgtz        %[src_width], 1b               \n"
        : [src_width] "+r"(src_width),          // %[src_width]
          [src_ptr] "+r"(src_ptr),              // %[src_ptr]
          [work_dst_ptr] "+r"(work_dst_ptr),    // %[work_dst_ptr]
          [vl] "=&r"(vl),                       // %[vl]
          [work_src_next] "=&r"(work_src_next)  // %[work_src_next]
        : [c2] "r"(2),                          // %[c2]
          [c3] "r"(3)                           // %[c3]
        : RVV_VL_CLOBBER "memory", "v12", "v13", "v16", "v17", "v18", "v19",
          "v24", "v25", "v26", "v27");
  }
  dst_ptr[dst_width - 1] = src_ptr[0];
}
#endif

#ifdef HAS_SCALEROWUP2_BILINEAR_RVV
void ScaleRowUp2_Bilinear_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              ptrdiff_t dst_stride,
                              int dst_width) {
  int src_width = (dst_width - 1) >> 1;
  const uint8_t* t = src_ptr + src_stride;
  uint8_t* e = dst_ptr + dst_stride;
  dst_ptr[0] = (3 * src_ptr[0] + t[0] + 2) >> 2;
  e[0] = (src_ptr[0] + 3 * t[0] + 2) >> 2;
  if (src_width > 0) {
    int vl;
    const uint8_t* tmp_ptr;
    uint8_t* work_d = dst_ptr + 1;
    uint8_t* work_e = e + 1;
    asm volatile(
        "1:          \n"
        "vsetvli     %[vl], %[src_width], e8, m2, ta, ma\n"
        "vle8.v      v0, (%[work_s])                \n"
        "addi        %[tmp_ptr], %[work_s], 1       \n"
        "vle8.v      v2, (%[tmp_ptr])               \n"
        "vwaddu.vx   v8, v0, %[c2]                  \n"
        "vwaddu.vx   v12, v2, %[c2]                 \n"
        "vwmaccu.vx  v8, %[c3], v2                  \n"
        "vwmaccu.vx  v12, %[c3], v0                 \n"
        "vle8.v      v4, (%[work_t])                \n"
        "addi        %[tmp_ptr], %[work_t], 1       \n"
        "vle8.v      v6, (%[tmp_ptr])               \n"
        "vwaddu.vx   v16, v4, %[c2]                 \n"
        "vwaddu.vx   v20, v6, %[c2]                 \n"
        "vwmaccu.vx  v16, %[c3], v6                 \n"
        "vwmaccu.vx  v20, %[c3], v4                 \n"
        "vmv4r.v     v24, v16                       \n"
        "vmv4r.v     v28, v20                       \n"
        "vsetvli     zero, zero, e16, m4, ta, ma    \n"
        "vmacc.vx    v16, %[c3], v8                 \n"
        "vmacc.vx    v20, %[c3], v12                \n"
        "vmacc.vx    v8, %[c3], v24                 \n"
        "vmacc.vx    v12, %[c3], v28                \n"
        "vsetvli     zero, zero, e8, m2, ta, ma     \n"
        "vnsrl.wi    v0, v20, 4                     \n"
        "vnsrl.wi    v2, v16, 4                     \n"
        "vsseg2e8.v  v0, (%[work_d])                \n"
        "vnsrl.wi    v4, v12, 4                     \n"
        "vnsrl.wi    v6, v8, 4                      \n"
        "vsseg2e8.v  v4, (%[work_e])                \n"
        "sub         %[src_width], %[src_width], %[vl]\n"
        "add         %[work_s], %[work_s], %[vl]    \n"
        "add         %[work_t], %[work_t], %[vl]    \n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[work_d], %[work_d], %[vl]    \n"
        "add         %[work_e], %[work_e], %[vl]    \n"
        "bgtz        %[src_width], 1b               \n"
        : [src_width] "+r"(src_width),  // %[src_width]
          [work_s] "+r"(src_ptr),       // %[work_s]
          [work_t] "+r"(t),             // %[work_t]
          [work_d] "+r"(work_d),        // %[work_d]
          [work_e] "+r"(work_e),        // %[work_e]
          [vl] "=&r"(vl),               // %[vl]
          [tmp_ptr] "=&r"(tmp_ptr)      // %[tmp_ptr]
        : [c2] "r"(2),                  // %[c2]
          [c3] "r"(3)                   // %[c3]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6",
          "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16",
          "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26",
          "v27", "v28", "v29", "v30", "v31");
  }
  dst_ptr[dst_width - 1] = (3 * src_ptr[0] + t[0] + 2) >> 2;
  e[dst_width - 1] = (src_ptr[0] + 3 * t[0] + 2) >> 2;
}
#endif

#ifdef HAS_SCALEUVROWDOWN2_RVV
void ScaleUVRowDown2_RVV(const uint8_t* src_uv,
                         ptrdiff_t src_stride,
                         uint8_t* dst_uv,
                         int dst_width) {
  (void)src_stride;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m2, ta, ma   \n"
      "vle32.v     v8, (%[src_uv])                \n"
      "vnsrl.wi    v16, v8, 16                    \n"
      "vse16.v     v16, (%[dst_uv])               \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_uv], %[dst_uv], %[vl]    \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_uv], %[src_uv], %[vl]    \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),    // %[w]
        [src_uv] "+r"(src_uv),  // %[src_uv]
        [dst_uv] "+r"(dst_uv),  // %[dst_uv]
        [vl] "=&r"(vl)          // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_SCALEUVROWDOWN2LINEAR_RVV
void ScaleUVRowDown2Linear_RVV(const uint8_t* src_uv,
                               ptrdiff_t src_stride,
                               uint8_t* dst_uv,
                               int dst_width) {
  (void)src_stride;
  int vl;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile(
      "csrwi       vxrm, 0                        \n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v8, (%[src_uv])                \n"
      "vaaddu.vv   v8, v8, v12                    \n"
      "vaaddu.vv   v10, v10, v14                  \n"
      "vsseg2e8.v  v8, (%[dst_uv])                \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_uv], %[dst_uv], %[vl]    \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_uv], %[src_uv], %[vl]    \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),    // %[w]
        [src_uv] "+r"(src_uv),  // %[src_uv]
        [dst_uv] "+r"(dst_uv),  // %[dst_uv]
        [vl] "=&r"(vl)          // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15");
}
#endif

#ifdef HAS_SCALEUVROWDOWN2BOX_RVV
void ScaleUVRowDown2Box_RVV(const uint8_t* src_uv,
                            ptrdiff_t src_stride,
                            uint8_t* dst_uv,
                            int dst_width) {
  int vl;
  asm volatile(
      "add         %[src_uv_row1], %[src_uv], %[src_uv_row1]\n"

      "1:          \n"
      "vsetvli     %[vl], %[w], e8, m2, ta, ma    \n"
      "vlseg4e8.v  v14, (%[src_uv])               \n"
      "vlseg4e8.v  v22, (%[src_uv_row1])          \n"
      "vwaddu.vx   v8, v14, %[c2]                 \n"
      "vwaddu.wv   v8, v8, v18                    \n"
      "vwaddu.wv   v8, v8, v22                    \n"
      "vwaddu.wv   v8, v8, v26                    \n"
      "vwaddu.vx   v12, v16, %[c2]                \n"
      "vwaddu.wv   v12, v12, v20                  \n"
      "vwaddu.wv   v12, v12, v24                  \n"
      "vwaddu.wv   v12, v12, v28                  \n"
      "vnsrl.wi    v16, v8, 2                     \n"
      "vnsrl.wi    v18, v12, 2                    \n"
      "vsseg2e8.v  v16, (%[dst_uv])               \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_uv], %[dst_uv], %[vl]    \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[src_uv], %[src_uv], %[vl]    \n"
      "add         %[src_uv_row1], %[src_uv_row1], %[vl]\n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),             // %[w]
        [src_uv] "+r"(src_uv),           // %[src_uv]
        [src_uv_row1] "+r"(src_stride),  // %[src_uv_row1]
        [dst_uv] "+r"(dst_uv),           // %[dst_uv]
        [vl] "=&r"(vl)                   // %[vl]
      : [c2] "r"(2)                      // %[c2]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
        "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24",
        "v25", "v26", "v27", "v28", "v29");
}
#endif

#ifdef HAS_SCALEUVROWDOWN4_RVV
void ScaleUVRowDown4_RVV(const uint8_t* src_uv,
                         ptrdiff_t src_stride,
                         int src_stepx,
                         uint8_t* dst_uv,
                         int dst_width) {
  (void)src_stride;
  (void)src_stepx;
  int vl;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e32, m2, ta, ma   \n"
      "vle64.v     v8, (%[src_uv])                \n"
      "vnsrl.wi    v16, v8, 0                     \n"
      "vsetvli     zero, zero, e16, m1, ta, ma    \n"
      "vnsrl.wi    v8, v16, 0                     \n"
      "vse16.v     v8, (%[dst_uv])                \n"
      "sub         %[w], %[w], %[vl]              \n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_uv], %[dst_uv], %[vl]    \n"
      "slli        %[vl], %[vl], 2                \n"
      "add         %[src_uv], %[src_uv], %[vl]    \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),    // %[w]
        [src_uv] "+r"(src_uv),  // %[src_uv]
        [dst_uv] "+r"(dst_uv),  // %[dst_uv]
        [vl] "=&r"(vl)          // %[vl]
      :
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11", "v16", "v17");
}
#endif

#ifdef HAS_SCALEUVROWDOWNEVEN_RVV
void ScaleUVRowDownEven_RVV(const uint8_t* src_uv,
                            ptrdiff_t src_stride,
                            int src_stepx,
                            uint8_t* dst_uv,
                            int dst_width) {
  (void)src_stride;
  int vl;
  ptrdiff_t src_step;
  asm volatile(
      "1:          \n"
      "vsetvli     %[vl], %[w], e16, m4, ta, ma   \n"
      "vlse16.v    v8, (%[src_uv]), %[stride_byte]\n"
      "vse16.v     v8, (%[dst_uv])                \n"
      "sub         %[w], %[w], %[vl]              \n"
      "mul         %[src_step], %[vl], %[stride_byte]\n"
      "add         %[src_uv], %[src_uv], %[src_step]\n"
      "slli        %[vl], %[vl], 1                \n"
      "add         %[dst_uv], %[dst_uv], %[vl]    \n"
      "bgtz        %[w], 1b                       \n"
      : [w] "+r"(dst_width),                         // %[w]
        [src_uv] "+r"(src_uv),                       // %[src_uv]
        [dst_uv] "+r"(dst_uv),                       // %[dst_uv]
        [vl] "=&r"(vl),                              // %[vl]
        [src_step] "=&r"(src_step)                   // %[src_step]
      : [stride_byte] "r"((ptrdiff_t)src_stepx * 2)  // %[stride_byte]
      : RVV_VL_CLOBBER "memory", "v8", "v9", "v10", "v11");
}
#endif

// ScaleUVRowUp2_(Bi)linear_RVV function is equal to other platforms'
// ScaleUVRowUp2_(Bi)linear_Any_XXX. We process entire row in this function.
// Other platforms only implement non-edge part of image and process edge with
// scalar.

#ifdef HAS_SCALEUVROWUP2_LINEAR_RVV
void ScaleUVRowUp2_Linear_RVV(const uint8_t* src_ptr,
                              uint8_t* dst_ptr,
                              int dst_width) {
  int src_pairs = (dst_width - 1) >> 1;
  dst_ptr[0] = src_ptr[0];
  dst_ptr[1] = src_ptr[1];
  if (src_pairs > 0) {
    int vl;
    const uint8_t* work_src_next;
    uint8_t* work_dst_ptr = dst_ptr + 2;
    asm volatile(
        "1:          \n"
        "vsetvli     %[vl], %[src_pairs], e8, m2, ta, ma\n"
        "addi        %[work_src_next], %[src_ptr], 2\n"
        "vlseg2e8.v  v16, (%[work_src_next])        \n"
        "vlseg2e8.v  v20, (%[src_ptr])              \n"
        "vwaddu.vx   v12, v16, %[c2]                \n"
        "vwmaccu.vx  v12, %[c3], v20                \n"
        "vnsrl.wi    v24, v12, 2                    \n"
        "vwaddu.vx   v12, v20, %[c2]                \n"
        "vwmaccu.vx  v12, %[c3], v16                \n"
        "vnsrl.wi    v28, v12, 2                    \n"
        "vwaddu.vx   v12, v18, %[c2]                \n"
        "vwmaccu.vx  v12, %[c3], v22                \n"
        "vnsrl.wi    v26, v12, 2                    \n"
        "vwaddu.vx   v12, v22, %[c2]                \n"
        "vwmaccu.vx  v12, %[c3], v18                \n"
        "vnsrl.wi    v30, v12, 2                    \n"
        "vsseg4e8.v  v24, (%[work_dst_ptr])         \n"
        "sub         %[src_pairs], %[src_pairs], %[vl]\n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[src_ptr], %[src_ptr], %[vl]  \n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[work_dst_ptr], %[work_dst_ptr], %[vl]\n"
        "bgtz        %[src_pairs], 1b               \n"
        : [src_pairs] "+r"(src_pairs),          // %[src_pairs]
          [src_ptr] "+r"(src_ptr),              // %[src_ptr]
          [work_dst_ptr] "+r"(work_dst_ptr),    // %[work_dst_ptr]
          [vl] "=&r"(vl),                       // %[vl]
          [work_src_next] "=&r"(work_src_next)  // %[work_src_next]
        : [c2] "r"(2),                          // %[c2]
          [c3] "r"(3)                           // %[c3]
        : RVV_VL_CLOBBER "memory", "v12", "v13", "v14", "v15", "v16", "v17",
          "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27",
          "v28", "v29", "v30", "v31");
  }
  dst_ptr[2 * dst_width - 2] = src_ptr[0];
  dst_ptr[2 * dst_width - 1] = src_ptr[1];
}
#endif

#ifdef HAS_SCALEUVROWUP2_BILINEAR_RVV
void ScaleUVRowUp2_Bilinear_RVV(const uint8_t* src_ptr,
                                ptrdiff_t src_stride,
                                uint8_t* dst_ptr,
                                ptrdiff_t dst_stride,
                                int dst_width) {
  int src_pairs = (dst_width - 1) >> 1;
  const uint8_t* t = src_ptr + src_stride;
  uint8_t* e = dst_ptr + dst_stride;
  dst_ptr[0] = (3 * src_ptr[0] + t[0] + 2) >> 2;
  e[0] = (src_ptr[0] + 3 * t[0] + 2) >> 2;
  dst_ptr[1] = (3 * src_ptr[1] + t[1] + 2) >> 2;
  e[1] = (src_ptr[1] + 3 * t[1] + 2) >> 2;
  if (src_pairs > 0) {
    int vl;
    const uint8_t* tmp_ptr;
    uint8_t* work_d = dst_ptr + 2;
    uint8_t* work_e = e + 2;
    asm volatile(
        "1:          \n"
        "vsetvli     %[vl], %[src_pairs], e8, m2, ta, ma\n"
        // Load s_u0 (v0), s_v0 (v2) and s_u1 (v4), s_v1 (v6)
        "vlseg2e8.v  v0, (%[work_s])                \n"
        "addi        %[tmp_ptr], %[work_s], 2       \n"
        "vlseg2e8.v  v4, (%[tmp_ptr])               \n"
        // Load t_u0 (v8), t_v0 (v10) and t_u1 (v12), t_v1 (v14)
        "vlseg2e8.v  v8, (%[work_t])                \n"
        "addi        %[tmp_ptr], %[work_t], 2       \n"
        "vlseg2e8.v  v12, (%[tmp_ptr])              \n"
        // Compute U channel horizontal interpolation into v16..v31
        "vwaddu.vx   v16, v0, %[c2]                 \n"
        "vwaddu.vx   v20, v4, %[c2]                 \n"
        "vwmaccu.vx  v16, %[c3], v4                 \n"
        "vwmaccu.vx  v20, %[c3], v0                 \n"
        "vwaddu.vx   v24, v8, %[c2]                 \n"
        "vwaddu.vx   v28, v12, %[c2]                \n"
        "vwmaccu.vx  v24, %[c3], v12                \n"
        "vwmaccu.vx  v28, %[c3], v8                 \n"
        // Move t_v0 (v10) -> v0, t_v1 (v14) -> v4 so v8..v15 are free
        "vmv2r.v     v0, v10                        \n"
        "vmv2r.v     v4, v14                        \n"
        "vmv4r.v     v8, v24                        \n"
        "vmv4r.v     v12, v28                       \n"
        "vsetvli     zero, zero, e16, m4, ta, ma    \n"
        "vmacc.vx    v24, %[c3], v16                \n"
        "vmacc.vx    v28, %[c3], v20                \n"
        "vmacc.vx    v16, %[c3], v8                 \n"
        "vmacc.vx    v20, %[c3], v12                \n"
        "vsetvli     zero, zero, e8, m2, ta, ma     \n"
        // Move s_v0 (v2) -> v8, s_v1 (v6) -> v10, t_v0 (v0) -> v12, t_v1 (v4)
        // -> v14
        "vmv2r.v     v8, v2                         \n"
        "vmv2r.v     v10, v6                        \n"
        "vmv2r.v     v12, v0                        \n"
        "vmv2r.v     v14, v4                        \n"
        // Narrow U results into v0 (u_d_even), v2 (u_d_odd), v4 (u_e_even), v6
        // (u_e_odd)
        "vnsrl.wi    v0, v28, 4                     \n"
        "vnsrl.wi    v2, v24, 4                     \n"
        "vnsrl.wi    v4, v20, 4                     \n"
        "vnsrl.wi    v6, v16, 4                     \n"
        // Compute V channel horizontal interpolation from v8, v10, v12, v14
        // into v16..v31
        "vwaddu.vx   v16, v8, %[c2]                 \n"
        "vwaddu.vx   v20, v10, %[c2]                \n"
        "vwmaccu.vx  v16, %[c3], v10                \n"
        "vwmaccu.vx  v20, %[c3], v8                 \n"
        "vwaddu.vx   v24, v12, %[c2]                \n"
        "vwaddu.vx   v28, v14, %[c2]                \n"
        "vwmaccu.vx  v24, %[c3], v14                \n"
        "vwmaccu.vx  v28, %[c3], v12                \n"
        "vmv4r.v     v8, v24                        \n"
        "vmv4r.v     v12, v28                       \n"
        "vsetvli     zero, zero, e16, m4, ta, ma    \n"
        "vmacc.vx    v24, %[c3], v16                \n"
        "vmacc.vx    v28, %[c3], v20                \n"
        "vmacc.vx    v16, %[c3], v8                 \n"
        "vmacc.vx    v20, %[c3], v12                \n"
        "vsetvli     zero, zero, e8, m2, ta, ma     \n"
        // Assemble and store row 0: (u_d_even, v_d_even, u_d_odd, v_d_odd) in
        // v8..v15
        "vmv2r.v     v8, v0                         \n"
        "vnsrl.wi    v10, v28, 4                    \n"
        "vmv2r.v     v12, v2                        \n"
        "vnsrl.wi    v14, v24, 4                    \n"
        "vsseg4e8.v  v8, (%[work_d])                \n"
        // Assemble and store row 1: (u_e_even, v_e_even, u_e_odd, v_e_odd) in
        // v8..v15
        "vmv2r.v     v8, v4                         \n"
        "vnsrl.wi    v10, v20, 4                    \n"
        "vmv2r.v     v12, v6                        \n"
        "vnsrl.wi    v14, v16, 4                    \n"
        "vsseg4e8.v  v8, (%[work_e])                \n"
        "sub         %[src_pairs], %[src_pairs], %[vl]\n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[work_s], %[work_s], %[vl]    \n"
        "add         %[work_t], %[work_t], %[vl]    \n"
        "slli        %[vl], %[vl], 1                \n"
        "add         %[work_d], %[work_d], %[vl]    \n"
        "add         %[work_e], %[work_e], %[vl]    \n"
        "bgtz        %[src_pairs], 1b               \n"
        : [src_pairs] "+r"(src_pairs),  // %[src_pairs]
          [work_s] "+r"(src_ptr),       // %[work_s]
          [work_t] "+r"(t),             // %[work_t]
          [work_d] "+r"(work_d),        // %[work_d]
          [work_e] "+r"(work_e),        // %[work_e]
          [vl] "=&r"(vl),               // %[vl]
          [tmp_ptr] "=&r"(tmp_ptr)      // %[tmp_ptr]
        : [c2] "r"(2),                  // %[c2]
          [c3] "r"(3)                   // %[c3]
        : RVV_VL_CLOBBER "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6",
          "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16",
          "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26",
          "v27", "v28", "v29", "v30", "v31");
  }
  dst_ptr[2 * dst_width - 2] = (3 * src_ptr[0] + t[0] + 2) >> 2;
  e[2 * dst_width - 2] = (src_ptr[0] + 3 * t[0] + 2) >> 2;
  dst_ptr[2 * dst_width - 1] = (3 * src_ptr[1] + t[1] + 2) >> 2;
  e[2 * dst_width - 1] = (src_ptr[1] + 3 * t[1] + 2) >> 2;
}
#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
