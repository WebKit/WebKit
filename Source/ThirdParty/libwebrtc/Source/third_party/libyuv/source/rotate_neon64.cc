/*
 *  Copyright 2014 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for GCC Neon armv8 64 bit.
#if !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)

void TransposeWx16_NEON(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width) {
  const uint8_t* src_temp;
  asm volatile(
      "1:          \n"
      "mov         %[src_temp], %[src]           \n"

      "ld1         {v16.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v17.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v18.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v19.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v20.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v21.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v22.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v23.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v24.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v25.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v26.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v27.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v28.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v29.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v30.16b}, [%[src_temp]], %[src_stride] \n"
      "ld1         {v31.16b}, [%[src_temp]], %[src_stride] \n"

      "add         %[src], %[src], #16           \n"

      // Transpose bytes within each 2x2 block.
      "trn1        v0.16b, v16.16b, v17.16b      \n"
      "trn2        v1.16b, v16.16b, v17.16b      \n"
      "trn1        v2.16b, v18.16b, v19.16b      \n"
      "trn2        v3.16b, v18.16b, v19.16b      \n"
      "trn1        v4.16b, v20.16b, v21.16b      \n"
      "trn2        v5.16b, v20.16b, v21.16b      \n"
      "trn1        v6.16b, v22.16b, v23.16b      \n"
      "trn2        v7.16b, v22.16b, v23.16b      \n"
      "trn1        v8.16b, v24.16b, v25.16b      \n"
      "trn2        v9.16b, v24.16b, v25.16b      \n"
      "trn1        v10.16b, v26.16b, v27.16b     \n"
      "trn2        v11.16b, v26.16b, v27.16b     \n"
      "trn1        v12.16b, v28.16b, v29.16b     \n"
      "trn2        v13.16b, v28.16b, v29.16b     \n"
      "trn1        v14.16b, v30.16b, v31.16b     \n"
      "trn2        v15.16b, v30.16b, v31.16b     \n"

      // Transpose 2x2-byte blocks within each 4x4 block.
      "trn1        v16.8h, v0.8h, v2.8h          \n"
      "trn1        v17.8h, v1.8h, v3.8h          \n"
      "trn2        v18.8h, v0.8h, v2.8h          \n"
      "trn2        v19.8h, v1.8h, v3.8h          \n"
      "trn1        v20.8h, v4.8h, v6.8h          \n"
      "trn1        v21.8h, v5.8h, v7.8h          \n"
      "trn2        v22.8h, v4.8h, v6.8h          \n"
      "trn2        v23.8h, v5.8h, v7.8h          \n"
      "trn1        v24.8h, v8.8h, v10.8h         \n"
      "trn1        v25.8h, v9.8h, v11.8h         \n"
      "trn2        v26.8h, v8.8h, v10.8h         \n"
      "trn2        v27.8h, v9.8h, v11.8h         \n"
      "trn1        v28.8h, v12.8h, v14.8h        \n"
      "trn1        v29.8h, v13.8h, v15.8h        \n"
      "trn2        v30.8h, v12.8h, v14.8h        \n"
      "trn2        v31.8h, v13.8h, v15.8h        \n"

      "subs        %w[width], %w[width], #16     \n"

      // Transpose 4x4-byte blocks within each 8x8 block.
      "trn1        v0.4s, v16.4s, v20.4s         \n"
      "trn1        v2.4s, v17.4s, v21.4s         \n"
      "trn1        v4.4s, v18.4s, v22.4s         \n"
      "trn1        v6.4s, v19.4s, v23.4s         \n"
      "trn2        v8.4s, v16.4s, v20.4s         \n"
      "trn2        v10.4s, v17.4s, v21.4s        \n"
      "trn2        v12.4s, v18.4s, v22.4s        \n"
      "trn2        v14.4s, v19.4s, v23.4s        \n"
      "trn1        v1.4s, v24.4s, v28.4s         \n"
      "trn1        v3.4s, v25.4s, v29.4s         \n"
      "trn1        v5.4s, v26.4s, v30.4s         \n"
      "trn1        v7.4s, v27.4s, v31.4s         \n"
      "trn2        v9.4s, v24.4s, v28.4s         \n"
      "trn2        v11.4s, v25.4s, v29.4s        \n"
      "trn2        v13.4s, v26.4s, v30.4s        \n"
      "trn2        v15.4s, v27.4s, v31.4s        \n"

      // Transpose 8x8-byte blocks and store.
      "st2         {v0.d, v1.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v2.d, v3.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v4.d, v5.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v6.d, v7.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v8.d, v9.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v10.d, v11.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v12.d, v13.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v14.d, v15.d}[0], [%[dst]], %[dst_stride] \n"
      "st2         {v0.d, v1.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v2.d, v3.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v4.d, v5.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v6.d, v7.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v8.d, v9.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v10.d, v11.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v12.d, v13.d}[1], [%[dst]], %[dst_stride] \n"
      "st2         {v14.d, v15.d}[1], [%[dst]], %[dst_stride] \n"

      "b.gt        1b                            \n"
      : [src] "+r"(src),                          // %[src]
        [src_temp] "=&r"(src_temp),               // %[src_temp]
        [dst] "+r"(dst),                          // %[dst]
        [width] "+r"(width)                       // %[width]
      : [src_stride] "r"((ptrdiff_t)src_stride),  // %[src_stride]
        [dst_stride] "r"((ptrdiff_t)dst_stride)   // %[dst_stride]
      : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8",
        "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18",
        "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28",
        "v29", "v30", "v31");
}

void TransposeUVWx8_NEON(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst_a,
                         int dst_stride_a,
                         uint8_t* dst_b,
                         int dst_stride_b,
                         int width) {
  const uint8_t* temp;
  asm volatile(
      // loops are on blocks of 8. loop will stop when
      // counter gets to or below 0. starting the counter
      // at w-8 allow for this
      "sub         %w[width], %w[width], #8      \n"

      "1:          \n"
      "mov         %[temp], %[src]               \n"
      "ld1         {v0.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v1.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v2.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v3.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v4.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v5.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v6.16b}, [%[temp]], %[src_stride] \n"
      "ld1         {v7.16b}, [%[temp]]           \n"
      "add         %[src], %[src], #16           \n"

      "trn1        v16.16b, v0.16b, v1.16b       \n"
      "trn2        v17.16b, v0.16b, v1.16b       \n"
      "trn1        v18.16b, v2.16b, v3.16b       \n"
      "trn2        v19.16b, v2.16b, v3.16b       \n"
      "trn1        v20.16b, v4.16b, v5.16b       \n"
      "trn2        v21.16b, v4.16b, v5.16b       \n"
      "trn1        v22.16b, v6.16b, v7.16b       \n"
      "trn2        v23.16b, v6.16b, v7.16b       \n"

      "subs        %w[width], %w[width],  #8     \n"

      "trn1        v0.8h, v16.8h, v18.8h         \n"
      "trn2        v1.8h, v16.8h, v18.8h         \n"
      "trn1        v2.8h, v20.8h, v22.8h         \n"
      "trn2        v3.8h, v20.8h, v22.8h         \n"
      "trn1        v4.8h, v17.8h, v19.8h         \n"
      "trn2        v5.8h, v17.8h, v19.8h         \n"
      "trn1        v6.8h, v21.8h, v23.8h         \n"
      "trn2        v7.8h, v21.8h, v23.8h         \n"

      "trn1        v16.4s, v0.4s, v2.4s          \n"
      "trn2        v17.4s, v0.4s, v2.4s          \n"
      "trn1        v18.4s, v1.4s, v3.4s          \n"
      "trn2        v19.4s, v1.4s, v3.4s          \n"
      "trn1        v20.4s, v4.4s, v6.4s          \n"
      "trn2        v21.4s, v4.4s, v6.4s          \n"
      "trn1        v22.4s, v5.4s, v7.4s          \n"
      "trn2        v23.4s, v5.4s, v7.4s          \n"

      "mov         %[temp], %[dst_a]             \n"
      "st1         {v16.d}[0], [%[temp]], %[dst_stride_a] \n"
      "st1         {v18.d}[0], [%[temp]], %[dst_stride_a] \n"
      "st1         {v17.d}[0], [%[temp]], %[dst_stride_a] \n"
      "st1         {v19.d}[0], [%[temp]], %[dst_stride_a] \n"
      "st1         {v16.d}[1], [%[temp]], %[dst_stride_a] \n"
      "st1         {v18.d}[1], [%[temp]], %[dst_stride_a] \n"
      "st1         {v17.d}[1], [%[temp]], %[dst_stride_a] \n"
      "st1         {v19.d}[1], [%[temp]]         \n"
      "add         %[dst_a], %[dst_a], %[dst_stride_a], lsl #3 \n"

      "mov         %[temp], %[dst_b]             \n"
      "st1         {v20.d}[0], [%[temp]], %[dst_stride_b] \n"
      "st1         {v22.d}[0], [%[temp]], %[dst_stride_b] \n"
      "st1         {v21.d}[0], [%[temp]], %[dst_stride_b] \n"
      "st1         {v23.d}[0], [%[temp]], %[dst_stride_b] \n"
      "st1         {v20.d}[1], [%[temp]], %[dst_stride_b] \n"
      "st1         {v22.d}[1], [%[temp]], %[dst_stride_b] \n"
      "st1         {v21.d}[1], [%[temp]], %[dst_stride_b] \n"
      "st1         {v23.d}[1], [%[temp]]         \n"
      "add         %[dst_b], %[dst_b], %[dst_stride_b], lsl #3 \n"

      "b.ge        1b                            \n"
      : [temp] "=&r"(temp),                           // %[temp]
        [src] "+r"(src),                              // %[src]
        [dst_a] "+r"(dst_a),                          // %[dst_a]
        [dst_b] "+r"(dst_b),                          // %[dst_b]
        [width] "+r"(width)                           // %[width]
      : [src_stride] "r"((ptrdiff_t)src_stride),      // %[src_stride]
        [dst_stride_a] "r"((ptrdiff_t)dst_stride_a),  // %[dst_stride_a]
        [dst_stride_b] "r"((ptrdiff_t)dst_stride_b)   // %[dst_stride_b]
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v30", "v31");
}

// Transpose 32 bit values (ARGB) in Wx4 tiles using ld4 with lane insertion
// and lane stores for 1..3 remainder rows.
void TransposeWx4_32_NEON(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = src_stride;
  const uint8_t* src1 = src + s;
  const uint8_t* src2 = src1 + s;
  const uint8_t* src3 = src2 + s;
  const ptrdiff_t d = dst_stride;
  uint8_t* dst1 = dst + d;
  uint8_t* dst2 = dst1 + d;
  uint8_t* dst3 = dst2 + d;
  asm volatile(
      "subs        %w8, %w8, #4                    \n"
      "b.lt        2f                              \n"

      // Main loop transpose 4x4. Read a column, write a row.
      "1:                                          \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[0], [%0], %9 \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[1], [%1], %9 \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[2], [%2], %9 \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[3], [%3], %9 \n"
      "subs        %w8, %w8, #4                    \n"
      "st1         {v0.4s}, [%4], 16               \n"
      "st1         {v1.4s}, [%5], 16               \n"
      "st1         {v2.4s}, [%6], 16               \n"
      "st1         {v3.4s}, [%7], 16               \n"
      "b.ge        1b                              \n"

      "2:                                          \n"
      "adds        %w8, %w8, #4                    \n"
      "b.eq        5f                              \n"

      // Remainder of 1, 2, or 3 source rows.
      "ld4         {v0.s, v1.s, v2.s, v3.s}[0], [%0] \n"
      "cmp         %w8, #2                         \n"
      "b.lt        4f                              \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[1], [%1] \n"
      "b.gt        3f                              \n"
      "st1         {v0.d}[0], [%4]                 \n"
      "st1         {v1.d}[0], [%5]                 \n"
      "st1         {v2.d}[0], [%6]                 \n"
      "st1         {v3.d}[0], [%7]                 \n"
      "b           5f                              \n"

      "3:                                          \n"
      "ld4         {v0.s, v1.s, v2.s, v3.s}[2], [%2] \n"
      "st1         {v0.d}[0], [%4], 8              \n"
      "st1         {v1.d}[0], [%5], 8              \n"
      "st1         {v2.d}[0], [%6], 8              \n"
      "st1         {v3.d}[0], [%7], 8              \n"
      "st1         {v0.s}[2], [%4]                 \n"
      "st1         {v1.s}[2], [%5]                 \n"
      "st1         {v2.s}[2], [%6]                 \n"
      "st1         {v3.s}[2], [%7]                 \n"
      "b           5f                              \n"

      "4:                                          \n"
      "st1         {v0.s}[0], [%4]                 \n"
      "st1         {v1.s}[0], [%5]                 \n"
      "st1         {v2.s}[0], [%6]                 \n"
      "st1         {v3.s}[0], [%7]                 \n"
      "5:                                          \n"
      : "+r"(src),   // %0
        "+r"(src1),  // %1
        "+r"(src2),  // %2
        "+r"(src3),  // %3
        "+r"(dst),   // %4
        "+r"(dst1),  // %5
        "+r"(dst2),  // %6
        "+r"(dst3),  // %7
        "+r"(width)  // %8
      : "r"(s * 4)   // %9
      : "memory", "cc", "v0", "v1", "v2", "v3");
}

void TransposeWxH_32_NEON(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width,
                          int height) {
  const ptrdiff_t d = dst_stride;
  while (width >= 4) {
    TransposeWx4_32_NEON(src, src_stride, dst, dst_stride, height);
    src += 16;
    dst += 4 * d;
    width -= 4;
  }
  if (width == 3) {
    const ptrdiff_t s = src_stride;
    const uint8_t* src1 = src + s;
    const uint8_t* src2 = src1 + s;
    const uint8_t* src3 = src2 + s;
    uint8_t* dst1 = dst + d;
    uint8_t* dst2 = dst1 + d;
    asm volatile(
        "subs        %w7, %w7, #4                    \n"
        "b.lt        2f                              \n"
        "1:                                          \n"
        "ld3         {v0.s, v1.s, v2.s}[0], [%0], %8 \n"
        "ld3         {v0.s, v1.s, v2.s}[1], [%1], %8 \n"
        "ld3         {v0.s, v1.s, v2.s}[2], [%2], %8 \n"
        "ld3         {v0.s, v1.s, v2.s}[3], [%3], %8 \n"
        "subs        %w7, %w7, #4                    \n"
        "st1         {v0.4s}, [%4], 16               \n"
        "st1         {v1.4s}, [%5], 16               \n"
        "st1         {v2.4s}, [%6], 16               \n"
        "b.ge        1b                              \n"
        "2:                                          \n"
        "adds        %w7, %w7, #4                    \n"
        "b.eq        5f                              \n"
        "ld3         {v0.s, v1.s, v2.s}[0], [%0]     \n"
        "cmp         %w7, #2                         \n"
        "b.lt        4f                              \n"
        "ld3         {v0.s, v1.s, v2.s}[1], [%1]     \n"
        "b.gt        3f                              \n"
        "st1         {v0.d}[0], [%4]                 \n"
        "st1         {v1.d}[0], [%5]                 \n"
        "st1         {v2.d}[0], [%6]                 \n"
        "b           5f                              \n"
        "3:                                          \n"
        "ld3         {v0.s, v1.s, v2.s}[2], [%2]     \n"
        "st1         {v0.d}[0], [%4], 8              \n"
        "st1         {v1.d}[0], [%5], 8              \n"
        "st1         {v2.d}[0], [%6], 8              \n"
        "st1         {v0.s}[2], [%4]                 \n"
        "st1         {v1.s}[2], [%5]                 \n"
        "st1         {v2.s}[2], [%6]                 \n"
        "b           5f                              \n"
        "4:                                          \n"
        "st1         {v0.s}[0], [%4]                 \n"
        "st1         {v1.s}[0], [%5]                 \n"
        "st1         {v2.s}[0], [%6]                 \n"
        "5:                                          \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(dst1),   // %5
          "+r"(dst2),   // %6
          "+r"(height)  // %7
        : "r"(s * 4)    // %8
        : "memory", "cc", "v0", "v1", "v2");
  } else if (width == 2) {
    const ptrdiff_t s = src_stride;
    const uint8_t* src1 = src + s;
    const uint8_t* src2 = src1 + s;
    const uint8_t* src3 = src2 + s;
    uint8_t* dst1 = dst + d;
    asm volatile(
        "subs        %w6, %w6, #4                    \n"
        "b.lt        2f                              \n"
        "1:                                          \n"
        "ld2         {v0.s, v1.s}[0], [%0], %7       \n"
        "ld2         {v0.s, v1.s}[1], [%1], %7       \n"
        "ld2         {v0.s, v1.s}[2], [%2], %7       \n"
        "ld2         {v0.s, v1.s}[3], [%3], %7       \n"
        "subs        %w6, %w6, #4                    \n"
        "st1         {v0.4s}, [%4], 16               \n"
        "st1         {v1.4s}, [%5], 16               \n"
        "b.ge        1b                              \n"
        "2:                                          \n"
        "adds        %w6, %w6, #4                    \n"
        "b.eq        5f                              \n"
        "ld2         {v0.s, v1.s}[0], [%0]           \n"
        "cmp         %w6, #2                         \n"
        "b.lt        4f                              \n"
        "ld2         {v0.s, v1.s}[1], [%1]           \n"
        "b.gt        3f                              \n"
        "st1         {v0.d}[0], [%4]                 \n"
        "st1         {v1.d}[0], [%5]                 \n"
        "b           5f                              \n"
        "3:                                          \n"
        "ld2         {v0.s, v1.s}[2], [%2]           \n"
        "st1         {v0.d}[0], [%4], 8              \n"
        "st1         {v1.d}[0], [%5], 8              \n"
        "st1         {v0.s}[2], [%4]                 \n"
        "st1         {v1.s}[2], [%5]                 \n"
        "b           5f                              \n"
        "4:                                          \n"
        "st1         {v0.s}[0], [%4]                 \n"
        "st1         {v1.s}[0], [%5]                 \n"
        "5:                                          \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(dst1),   // %5
          "+r"(height)  // %6
        : "r"(s * 4)    // %7
        : "memory", "cc", "v0", "v1");
  } else if (width == 1) {
    const ptrdiff_t s = src_stride;
    const uint8_t* src1 = src + s;
    const uint8_t* src2 = src1 + s;
    const uint8_t* src3 = src2 + s;
    asm volatile(
        "subs        %w5, %w5, #4                    \n"
        "b.lt        2f                              \n"
        "1:                                          \n"
        "ld1         {v0.s}[0], [%0], %6             \n"
        "ld1         {v0.s}[1], [%1], %6             \n"
        "ld1         {v0.s}[2], [%2], %6             \n"
        "ld1         {v0.s}[3], [%3], %6             \n"
        "subs        %w5, %w5, #4                    \n"
        "st1         {v0.4s}, [%4], 16               \n"
        "b.ge        1b                              \n"
        "2:                                          \n"
        "adds        %w5, %w5, #4                    \n"
        "b.eq        5f                              \n"
        "ld1         {v0.s}[0], [%0]                 \n"
        "cmp         %w5, #2                         \n"
        "b.lt        4f                              \n"
        "ld1         {v0.s}[1], [%1]                 \n"
        "b.gt        3f                              \n"
        "st1         {v0.d}[0], [%4]                 \n"
        "b           5f                              \n"
        "3:                                          \n"
        "ld1         {v0.s}[2], [%2]                 \n"
        "st1         {v0.d}[0], [%4], 8              \n"
        "st1         {v0.s}[2], [%4]                 \n"
        "b           5f                              \n"
        "4:                                          \n"
        "st1         {v0.s}[0], [%4]                 \n"
        "5:                                          \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(height)  // %5
        : "r"(s * 4)    // %6
        : "memory", "cc", "v0");
  }
}

void TransposeWx8_16_NEON(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint16_t* temp;
  asm volatile(
      "1:                                          \n"
      "mov         %[temp], %[src]                 \n"
      "ld1         {v0.8h}, [%[temp]], %[s]        \n"
      "ld1         {v1.8h}, [%[temp]], %[s]        \n"
      "ld1         {v2.8h}, [%[temp]], %[s]        \n"
      "ld1         {v3.8h}, [%[temp]], %[s]        \n"
      "ld1         {v4.8h}, [%[temp]], %[s]        \n"
      "ld1         {v5.8h}, [%[temp]], %[s]        \n"
      "ld1         {v6.8h}, [%[temp]], %[s]        \n"
      "ld1         {v7.8h}, [%[temp]]             \n"
      "add         %[src], %[src], #16             \n"

      // Stage 1: 16-bit transpose
      "trn1        v16.8h, v0.8h, v1.8h            \n"
      "trn2        v17.8h, v0.8h, v1.8h            \n"
      "trn1        v18.8h, v2.8h, v3.8h            \n"
      "trn2        v19.8h, v2.8h, v3.8h            \n"
      "trn1        v20.8h, v4.8h, v5.8h            \n"
      "trn2        v21.8h, v4.8h, v5.8h            \n"
      "trn1        v22.8h, v6.8h, v7.8h            \n"
      "trn2        v23.8h, v6.8h, v7.8h            \n"

      // Stage 2: 32-bit transpose
      "trn1        v0.4s, v16.4s, v18.4s           \n"
      "trn1        v1.4s, v17.4s, v19.4s           \n"
      "trn2        v2.4s, v16.4s, v18.4s           \n"
      "trn2        v3.4s, v17.4s, v19.4s           \n"
      "trn1        v4.4s, v20.4s, v22.4s           \n"
      "trn1        v5.4s, v21.4s, v23.4s           \n"
      "trn2        v6.4s, v20.4s, v22.4s           \n"
      "trn2        v7.4s, v21.4s, v23.4s           \n"

      // Stage 3: 64-bit transpose
      "trn1        v16.2d, v0.2d, v4.2d            \n"
      "trn1        v17.2d, v1.2d, v5.2d            \n"
      "trn1        v18.2d, v2.2d, v6.2d            \n"
      "trn1        v19.2d, v3.2d, v7.2d            \n"
      "trn2        v20.2d, v0.2d, v4.2d            \n"
      "trn2        v21.2d, v1.2d, v5.2d            \n"
      "trn2        v22.2d, v2.2d, v6.2d            \n"
      "trn2        v23.2d, v3.2d, v7.2d            \n"

      "subs        %w[w], %w[w], #8                \n"
      "st1         {v16.8h}, [%[dst]], %[d]        \n"
      "st1         {v17.8h}, [%[dst]], %[d]        \n"
      "st1         {v18.8h}, [%[dst]], %[d]        \n"
      "st1         {v19.8h}, [%[dst]], %[d]        \n"
      "st1         {v20.8h}, [%[dst]], %[d]        \n"
      "st1         {v21.8h}, [%[dst]], %[d]        \n"
      "st1         {v22.8h}, [%[dst]], %[d]        \n"
      "st1         {v23.8h}, [%[dst]], %[d]        \n"
      "b.gt        1b                              \n"
      : [src] "+r"(src),    // %[src]
        [dst] "+r"(dst),    // %[dst]
        [w] "+r"(width),    // %[w]
        [temp] "=&r"(temp)  // %[temp]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "cc", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16",
        "v17", "v18", "v19", "v20", "v21", "v22", "v23");
}

#endif  // !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
