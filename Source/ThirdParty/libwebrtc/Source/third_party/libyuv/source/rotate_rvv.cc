/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

// This module is for RVV (RISC-V Vector extension)
#if !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#ifndef RVV_VL_CLOBBER
#ifdef __clang__
#define RVV_VL_CLOBBER
#else
#define RVV_VL_CLOBBER "vl",
#endif
#endif

#ifdef HAS_TRANSPOSEWX4_32_RVV
// Transpose 32 bit values (ARGB) in Wx4 tiles, moving down W rows of source
// (VL at a time) and across W columns of destination (4 rows).
// Tuned for 512-bit VL (SiFive X280).
void TransposeWx4_32_RVV(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width) {
  int vl;
  ptrdiff_t src_step;
  ptrdiff_t dst_step;
  uint8_t* dst1 = dst + (ptrdiff_t)dst_stride;
  uint8_t* dst2 = dst1 + (ptrdiff_t)dst_stride;
  uint8_t* dst3 = dst2 + (ptrdiff_t)dst_stride;
  asm volatile(
      "vsetvli      %[vl], %[w], e32, m2, ta, ma       \n"
      "slli         %[dst_step], %[vl], 2              \n"
      "mul          %[src_step], %[vl], %[src_stride]  \n"
      "sub          %[w], %[w], %[vl]                  \n"
      "bltz         %[w], 2f                           \n"
      "1:                                              \n"
      "vlsseg4e32.v v8, (%[src]), %[src_stride]        \n"
      "add          %[src], %[src], %[src_step]        \n"
      "vse32.v      v8, (%[dst0])                      \n"
      "vse32.v      v10, (%[dst1])                     \n"
      "vse32.v      v12, (%[dst2])                     \n"
      "vse32.v      v14, (%[dst3])                     \n"
      "add          %[dst0], %[dst0], %[dst_step]      \n"
      "add          %[dst1], %[dst1], %[dst_step]      \n"
      "add          %[dst2], %[dst2], %[dst_step]      \n"
      "add          %[dst3], %[dst3], %[dst_step]      \n"
      "sub          %[w], %[w], %[vl]                  \n"
      "bgez         %[w], 1b                           \n"
      "2:                                              \n"
      "add          %[w], %[w], %[vl]                  \n"
      "beqz         %[w], 3f                           \n"
      "vsetvli      %[vl], %[w], e32, m2, ta, ma       \n"
      "vlsseg4e32.v v8, (%[src]), %[src_stride]        \n"
      "vse32.v      v8, (%[dst0])                      \n"
      "vse32.v      v10, (%[dst1])                     \n"
      "vse32.v      v12, (%[dst2])                     \n"
      "vse32.v      v14, (%[dst3])                     \n"
      "3:                                              \n"
      : [w] "+r"(width),                         // %[w]
        [src] "+r"(src),                         // %[src]
        [dst0] "+r"(dst),                        // %[dst0]
        [dst1] "+r"(dst1),                       // %[dst1]
        [dst2] "+r"(dst2),                       // %[dst2]
        [dst3] "+r"(dst3),                       // %[dst3]
        [vl] "=&r"(vl),                          // %[vl]
        [src_step] "=&r"(src_step),              // %[src_step]
        [dst_step] "=&r"(dst_step)               // %[dst_step]
      : [src_stride] "r"((ptrdiff_t)src_stride)  // %[src_stride]
      : RVV_VL_CLOBBER "vtype", "memory", "v8", "v9", "v10", "v11", "v12",
        "v13", "v14", "v15");
}
#endif  // HAS_TRANSPOSEWX4_32_RVV

#ifdef HAS_TRANSPOSEWXH_32_RVV
// Transpose 32 bit plane of any width x height using RVV without scalar
// fallback.
void TransposeWxH_32_RVV(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width,
                         int height) {
  while (width >= 4) {
    TransposeWx4_32_RVV(src, src_stride, dst, dst_stride, height);
    src += 16;
    dst += 4 * (ptrdiff_t)dst_stride;
    width -= 4;
  }
  while (width > 0) {
    const uint8_t* s_col = src;
    uint8_t* d_row = dst;
    int h = height;
    int vl;
    ptrdiff_t src_step;
    ptrdiff_t dst_step;
    asm volatile(
        "vsetvli  %[vl], %[h], e32, m8, ta, ma       \n"
        "slli     %[d_step], %[vl], 2                \n"
        "mul      %[s_step], %[vl], %[src_stride]    \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bltz     %[h], 2f                           \n"
        "1:                                          \n"
        "vlse32.v v8, (%[s]), %[src_stride]          \n"
        "add      %[s], %[s], %[s_step]              \n"
        "vse32.v  v8, (%[d])                         \n"
        "add      %[d], %[d], %[d_step]              \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bgez     %[h], 1b                           \n"
        "2:                                          \n"
        "add      %[h], %[h], %[vl]                  \n"
        "beqz     %[h], 3f                           \n"
        "vsetvli  %[vl], %[h], e32, m8, ta, ma       \n"
        "vlse32.v v8, (%[s]), %[src_stride]          \n"
        "vse32.v  v8, (%[d])                         \n"
        "3:                                          \n"
        : [h] "+r"(h),                             // %[h]
          [s] "+r"(s_col),                         // %[s]
          [d] "+r"(d_row),                         // %[d]
          [vl] "=&r"(vl),                          // %[vl]
          [s_step] "=&r"(src_step),                // %[s_step]
          [d_step] "=&r"(dst_step)                 // %[d_step]
        : [src_stride] "r"((ptrdiff_t)src_stride)  // %[src_stride]
        : RVV_VL_CLOBBER "vtype", "memory", "v8", "v9", "v10", "v11", "v12",
          "v13", "v14", "v15");
    src += 4;
    dst += (ptrdiff_t)dst_stride;
    width -= 1;
  }
}
#endif  // HAS_TRANSPOSEWXH_32_RVV

#ifdef HAS_TRANSPOSEWXH_16_RVV
void TransposeWxH_16_RVV(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width,
                         int height) {
  const ptrdiff_t s_stride = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d_stride = (ptrdiff_t)dst_stride * 2;
  while (width >= 4) {
    const uint16_t* s0 = src;
    const uint16_t* s1 = src + 1;
    const uint16_t* s2 = src + 2;
    const uint16_t* s3 = src + 3;
    uint8_t* d0 = (uint8_t*)dst;
    uint8_t* d1 = d0 + d_stride;
    uint8_t* d2 = d1 + d_stride;
    uint8_t* d3 = d2 + d_stride;
    int h = height;
    int vl;
    ptrdiff_t src_step;
    ptrdiff_t dst_step;
    asm volatile(
        "vsetvli  %[vl], %[h], e16, m8, ta, ma       \n"
        "slli     %[d_step], %[vl], 1                \n"
        "mul      %[s_step], %[vl], %[s_stride]      \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bltz     %[h], 2f                           \n"
        "1:                                          \n"
        "vlse16.v v0, (%[s0]), %[s_stride]           \n"
        "vlse16.v v8, (%[s1]), %[s_stride]           \n"
        "vlse16.v v16, (%[s2]), %[s_stride]          \n"
        "vlse16.v v24, (%[s3]), %[s_stride]          \n"
        "add      %[s0], %[s0], %[s_step]            \n"
        "add      %[s1], %[s1], %[s_step]            \n"
        "add      %[s2], %[s2], %[s_step]            \n"
        "add      %[s3], %[s3], %[s_step]            \n"
        "vse16.v  v0, (%[d0])                        \n"
        "vse16.v  v8, (%[d1])                        \n"
        "vse16.v  v16, (%[d2])                       \n"
        "vse16.v  v24, (%[d3])                       \n"
        "add      %[d0], %[d0], %[d_step]            \n"
        "add      %[d1], %[d1], %[d_step]            \n"
        "add      %[d2], %[d2], %[d_step]            \n"
        "add      %[d3], %[d3], %[d_step]            \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bgez     %[h], 1b                           \n"
        "2:                                          \n"
        "add      %[h], %[h], %[vl]                  \n"
        "beqz     %[h], 3f                           \n"
        "vsetvli  %[vl], %[h], e16, m8, ta, ma       \n"
        "vlse16.v v0, (%[s0]), %[s_stride]           \n"
        "vlse16.v v8, (%[s1]), %[s_stride]           \n"
        "vlse16.v v16, (%[s2]), %[s_stride]          \n"
        "vlse16.v v24, (%[s3]), %[s_stride]          \n"
        "vse16.v  v0, (%[d0])                        \n"
        "vse16.v  v8, (%[d1])                        \n"
        "vse16.v  v16, (%[d2])                       \n"
        "vse16.v  v24, (%[d3])                       \n"
        "3:                                          \n"
        : [h] "+r"(h),               // %[h]
          [s0] "+r"(s0),             // %[s0]
          [s1] "+r"(s1),             // %[s1]
          [s2] "+r"(s2),             // %[s2]
          [s3] "+r"(s3),             // %[s3]
          [d0] "+r"(d0),             // %[d0]
          [d1] "+r"(d1),             // %[d1]
          [d2] "+r"(d2),             // %[d2]
          [d3] "+r"(d3),             // %[d3]
          [vl] "=&r"(vl),            // %[vl]
          [s_step] "=&r"(src_step),  // %[s_step]
          [d_step] "=&r"(dst_step)   // %[d_step]
        : [s_stride] "r"(s_stride)   // %[s_stride]
        : RVV_VL_CLOBBER "vtype", "memory", "v0", "v1", "v2", "v3", "v4", "v5",
          "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
          "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25",
          "v26", "v27", "v28", "v29", "v30", "v31");
    src += 4;
    dst += 4 * (ptrdiff_t)dst_stride;
    width -= 4;
  }
  while (width > 0) {
    const uint16_t* s_col = src;
    uint16_t* d_row = dst;
    int h = height;
    int vl;
    ptrdiff_t src_step;
    ptrdiff_t dst_step;
    asm volatile(
        "vsetvli  %[vl], %[h], e16, m8, ta, ma       \n"
        "slli     %[d_step], %[vl], 1                \n"
        "mul      %[s_step], %[vl], %[s_stride]      \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bltz     %[h], 2f                           \n"
        "1:                                          \n"
        "vlse16.v v8, (%[s]), %[s_stride]            \n"
        "add      %[s], %[s], %[s_step]              \n"
        "vse16.v  v8, (%[d])                         \n"
        "add      %[d], %[d], %[d_step]              \n"
        "sub      %[h], %[h], %[vl]                  \n"
        "bgez     %[h], 1b                           \n"
        "2:                                          \n"
        "add      %[h], %[h], %[vl]                  \n"
        "beqz     %[h], 3f                           \n"
        "vsetvli  %[vl], %[h], e16, m8, ta, ma       \n"
        "vlse16.v v8, (%[s]), %[s_stride]            \n"
        "vse16.v  v8, (%[d])                         \n"
        "3:                                          \n"
        : [h] "+r"(h),               // %[h]
          [s] "+r"(s_col),           // %[s]
          [d] "+r"(d_row),           // %[d]
          [vl] "=&r"(vl),            // %[vl]
          [s_step] "=&r"(src_step),  // %[s_step]
          [d_step] "=&r"(dst_step)   // %[d_step]
        : [s_stride] "r"(s_stride)   // %[s_stride]
        : RVV_VL_CLOBBER "vtype", "memory", "v8", "v9", "v10", "v11", "v12",
          "v13", "v14", "v15");
    src += 1;
    dst += (ptrdiff_t)dst_stride;
    width -= 1;
  }
}
#endif  // HAS_TRANSPOSEWXH_16_RVV

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
