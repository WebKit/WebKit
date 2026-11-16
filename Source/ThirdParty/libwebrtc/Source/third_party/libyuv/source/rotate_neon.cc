/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
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

#if !defined(LIBYUV_DISABLE_NEON) && defined(__ARM_NEON__) && \
    !defined(__aarch64__)

void TransposeWx8_NEON(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width) {
  const uint8_t* temp;
  asm volatile(
      // loops are on blocks of 8. loop will stop when
      // counter gets to or below 0. starting the counter
      // at w-8 allow for this
      "sub         %[width], #8                  \n"

      "1:          \n"
      "mov         %[temp], %[src]               \n"
      "vld1.8      {d0}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d1}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d2}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d3}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d4}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d5}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d6}, [%[temp]], %[src_stride] \n"
      "vld1.8      {d7}, [%[temp]]               \n"
      "add         %[src], #8                    \n"

      "vtrn.8      d1, d0                        \n"
      "vtrn.8      d3, d2                        \n"
      "vtrn.8      d5, d4                        \n"
      "vtrn.8      d7, d6                        \n"
      "subs        %[width], #8                  \n"

      "vtrn.16     d1, d3                        \n"
      "vtrn.16     d0, d2                        \n"
      "vtrn.16     d5, d7                        \n"
      "vtrn.16     d4, d6                        \n"

      "vtrn.32     d1, d5                        \n"
      "vtrn.32     d0, d4                        \n"
      "vtrn.32     d3, d7                        \n"
      "vtrn.32     d2, d6                        \n"

      "vrev16.8    q0, q0                        \n"
      "vrev16.8    q1, q1                        \n"
      "vrev16.8    q2, q2                        \n"
      "vrev16.8    q3, q3                        \n"

      "mov         %[temp], %[dst]               \n"
      "vst1.8      {d1}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d0}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d3}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d2}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d5}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d4}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d7}, [%[temp]], %[dst_stride] \n"
      "vst1.8      {d6}, [%[temp]]               \n"
      "add         %[dst], %[dst], %[dst_stride], lsl #3 \n"

      "bge         1b                            \n"
      : [temp] "=&r"(temp),            // %[temp]
        [src] "+r"(src),               // %[src]
        [dst] "+r"(dst),               // %[dst]
        [width] "+r"(width)            // %[width]
      : [src_stride] "r"(src_stride),  // %[src_stride]
        [dst_stride] "r"(dst_stride)   // %[dst_stride]
      : "memory", "cc", "q0", "q1", "q2", "q3");
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
      "sub         %[width], #8                  \n"

      "1:          \n"
      "mov         %[temp], %[src]               \n"
      "vld2.8      {d0,  d1},  [%[temp]], %[src_stride] \n"
      "vld2.8      {d2,  d3},  [%[temp]], %[src_stride] \n"
      "vld2.8      {d4,  d5},  [%[temp]], %[src_stride] \n"
      "vld2.8      {d6,  d7},  [%[temp]], %[src_stride] \n"
      "vld2.8      {d16, d17}, [%[temp]], %[src_stride] \n"
      "vld2.8      {d18, d19}, [%[temp]], %[src_stride] \n"
      "vld2.8      {d20, d21}, [%[temp]], %[src_stride] \n"
      "vld2.8      {d22, d23}, [%[temp]]         \n"
      "add         %[src], #8*2                  \n"

      "vtrn.8      q1, q0                        \n"
      "vtrn.8      q3, q2                        \n"
      "vtrn.8      q9, q8                        \n"
      "vtrn.8      q11, q10                      \n"
      "subs        %[width], #8                  \n"

      "vtrn.16     q1, q3                        \n"
      "vtrn.16     q0, q2                        \n"
      "vtrn.16     q9, q11                       \n"
      "vtrn.16     q8, q10                       \n"

      "vtrn.32     q1, q9                        \n"
      "vtrn.32     q0, q8                        \n"
      "vtrn.32     q3, q11                       \n"
      "vtrn.32     q2, q10                       \n"

      "vrev16.8    q0, q0                        \n"
      "vrev16.8    q1, q1                        \n"
      "vrev16.8    q2, q2                        \n"
      "vrev16.8    q3, q3                        \n"
      "vrev16.8    q8, q8                        \n"
      "vrev16.8    q9, q9                        \n"
      "vrev16.8    q10, q10                      \n"
      "vrev16.8    q11, q11                      \n"

      "mov         %[temp], %[dst_a]             \n"
      "vst1.8      {d2},  [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d0},  [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d6},  [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d4},  [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d18}, [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d16}, [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d22}, [%[temp]], %[dst_stride_a] \n"
      "vst1.8      {d20}, [%[temp]]              \n"
      "add         %[dst_a], %[dst_a], %[dst_stride_a], lsl #3 \n"

      "mov         %[temp], %[dst_b]             \n"
      "vst1.8      {d3},  [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d1},  [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d7},  [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d5},  [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d19}, [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d17}, [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d23}, [%[temp]], %[dst_stride_b] \n"
      "vst1.8      {d21}, [%[temp]]              \n"
      "add         %[dst_b], %[dst_b], %[dst_stride_b], lsl #3 \n"

      "bge         1b                            \n"
      : [temp] "=&r"(temp),                // %[temp]
        [src] "+r"(src),                   // %[src]
        [dst_a] "+r"(dst_a),               // %[dst_a]
        [dst_b] "+r"(dst_b),               // %[dst_b]
        [width] "+r"(width)                // %[width]
      : [src_stride] "r"(src_stride),      // %[src_stride]
        [dst_stride_a] "r"(dst_stride_a),  // %[dst_stride_a]
        [dst_stride_b] "r"(dst_stride_b)   // %[dst_stride_b]
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11");
}

// Transpose 32 bit values (ARGB) in Wx4 tiles using vld4.32 with lane insertion
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
      "subs        %8, %8, #4                    \n"
      "blt         2f                            \n"

      // Main loop transpose 4x4. Read a column, write a row.
      "1:                                        \n"
      "vld4.32     {d0[0], d2[0], d4[0], d6[0]}, [%0], %9 \n"
      "vld4.32     {d0[1], d2[1], d4[1], d6[1]}, [%1], %9 \n"
      "vld4.32     {d1[0], d3[0], d5[0], d7[0]}, [%2], %9 \n"
      "vld4.32     {d1[1], d3[1], d5[1], d7[1]}, [%3], %9 \n"
      "subs        %8, %8, #4                    \n"
      "vst1.8      {q0}, [%4]!                   \n"
      "vst1.8      {q1}, [%5]!                   \n"
      "vst1.8      {q2}, [%6]!                   \n"
      "vst1.8      {q3}, [%7]!                   \n"
      "bge         1b                            \n"

      "2:                                        \n"
      "adds        %8, %8, #4                    \n"
      "beq         5f                            \n"

      // Remainder of 1, 2, or 3 source rows.
      "vld4.32     {d0[0], d2[0], d4[0], d6[0]}, [%0] \n"
      "cmp         %8, #2                        \n"
      "blt         4f                            \n"
      "vld4.32     {d0[1], d2[1], d4[1], d6[1]}, [%1] \n"
      "bgt         3f                            \n"
      "vst1.32     {d0}, [%4]                    \n"
      "vst1.32     {d2}, [%5]                    \n"
      "vst1.32     {d4}, [%6]                    \n"
      "vst1.32     {d6}, [%7]                    \n"
      "b           5f                            \n"

      "3:                                        \n"
      "vld4.32     {d1[0], d3[0], d5[0], d7[0]}, [%2] \n"
      "vst1.32     {d0}, [%4]!                   \n"
      "vst1.32     {d2}, [%5]!                   \n"
      "vst1.32     {d4}, [%6]!                   \n"
      "vst1.32     {d6}, [%7]!                   \n"
      "vst1.32     {d1[0]}, [%4]                 \n"
      "vst1.32     {d3[0]}, [%5]                 \n"
      "vst1.32     {d5[0]}, [%6]                 \n"
      "vst1.32     {d7[0]}, [%7]                 \n"
      "b           5f                            \n"

      "4:                                        \n"
      "vst1.32     {d0[0]}, [%4]                 \n"
      "vst1.32     {d2[0]}, [%5]                 \n"
      "vst1.32     {d4[0]}, [%6]                 \n"
      "vst1.32     {d6[0]}, [%7]                 \n"
      "5:                                        \n"
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
      : "memory", "cc", "q0", "q1", "q2", "q3");
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
        "subs        %7, %7, #4                    \n"
        "blt         2f                            \n"
        "1:                                        \n"
        "vld3.32     {d0[0], d2[0], d4[0]}, [%0], %8 \n"
        "vld3.32     {d0[1], d2[1], d4[1]}, [%1], %8 \n"
        "vld3.32     {d1[0], d3[0], d5[0]}, [%2], %8 \n"
        "vld3.32     {d1[1], d3[1], d5[1]}, [%3], %8 \n"
        "subs        %7, %7, #4                    \n"
        "vst1.8      {q0}, [%4]!                   \n"
        "vst1.8      {q1}, [%5]!                   \n"
        "vst1.8      {q2}, [%6]!                   \n"
        "bge         1b                            \n"
        "2:                                        \n"
        "adds        %7, %7, #4                    \n"
        "beq         5f                            \n"
        "vld3.32     {d0[0], d2[0], d4[0]}, [%0]   \n"
        "cmp         %7, #2                        \n"
        "blt         4f                            \n"
        "vld3.32     {d0[1], d2[1], d4[1]}, [%1]   \n"
        "bgt         3f                            \n"
        "vst1.32     {d0}, [%4]                    \n"
        "vst1.32     {d2}, [%5]                    \n"
        "vst1.32     {d4}, [%6]                    \n"
        "b           5f                            \n"
        "3:                                        \n"
        "vld3.32     {d1[0], d3[0], d5[0]}, [%2]   \n"
        "vst1.32     {d0}, [%4]!                   \n"
        "vst1.32     {d2}, [%5]!                   \n"
        "vst1.32     {d4}, [%6]!                   \n"
        "vst1.32     {d1[0]}, [%4]                 \n"
        "vst1.32     {d3[0]}, [%5]                 \n"
        "vst1.32     {d5[0]}, [%6]                 \n"
        "b           5f                            \n"
        "4:                                        \n"
        "vst1.32     {d0[0]}, [%4]                 \n"
        "vst1.32     {d2[0]}, [%5]                 \n"
        "vst1.32     {d4[0]}, [%6]                 \n"
        "5:                                        \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(dst1),   // %5
          "+r"(dst2),   // %6
          "+r"(height)  // %7
        : "r"(s * 4)    // %8
        : "memory", "cc", "q0", "q1", "q2");
  } else if (width == 2) {
    const ptrdiff_t s = src_stride;
    const uint8_t* src1 = src + s;
    const uint8_t* src2 = src1 + s;
    const uint8_t* src3 = src2 + s;
    uint8_t* dst1 = dst + d;
    asm volatile(
        "subs        %6, %6, #4                    \n"
        "blt         2f                            \n"
        "1:                                        \n"
        "vld2.32     {d0[0], d2[0]}, [%0], %7      \n"
        "vld2.32     {d0[1], d2[1]}, [%1], %7      \n"
        "vld2.32     {d1[0], d3[0]}, [%2], %7      \n"
        "vld2.32     {d1[1], d3[1]}, [%3], %7      \n"
        "subs        %6, %6, #4                    \n"
        "vst1.8      {q0}, [%4]!                   \n"
        "vst1.8      {q1}, [%5]!                   \n"
        "bge         1b                            \n"
        "2:                                        \n"
        "adds        %6, %6, #4                    \n"
        "beq         5f                            \n"
        "vld2.32     {d0[0], d2[0]}, [%0]          \n"
        "cmp         %6, #2                        \n"
        "blt         4f                            \n"
        "vld2.32     {d0[1], d2[1]}, [%1]          \n"
        "bgt         3f                            \n"
        "vst1.32     {d0}, [%4]                    \n"
        "vst1.32     {d2}, [%5]                    \n"
        "b           5f                            \n"
        "3:                                        \n"
        "vld2.32     {d1[0], d3[0]}, [%2]          \n"
        "vst1.32     {d0}, [%4]!                   \n"
        "vst1.32     {d2}, [%5]!                   \n"
        "vst1.32     {d1[0]}, [%4]                 \n"
        "vst1.32     {d3[0]}, [%5]                 \n"
        "b           5f                            \n"
        "4:                                        \n"
        "vst1.32     {d0[0]}, [%4]                 \n"
        "vst1.32     {d2[0]}, [%5]                 \n"
        "5:                                        \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(dst1),   // %5
          "+r"(height)  // %6
        : "r"(s * 4)    // %7
        : "memory", "cc", "q0", "q1");
  } else if (width == 1) {
    const ptrdiff_t s = src_stride;
    const uint8_t* src1 = src + s;
    const uint8_t* src2 = src1 + s;
    const uint8_t* src3 = src2 + s;
    asm volatile(
        "subs        %5, %5, #4                    \n"
        "blt         2f                            \n"
        "1:                                        \n"
        "vld1.32     {d0[0]}, [%0], %6             \n"
        "vld1.32     {d0[1]}, [%1], %6             \n"
        "vld1.32     {d1[0]}, [%2], %6             \n"
        "vld1.32     {d1[1]}, [%3], %6             \n"
        "subs        %5, %5, #4                    \n"
        "vst1.8      {q0}, [%4]!                   \n"
        "bge         1b                            \n"
        "2:                                        \n"
        "adds        %5, %5, #4                    \n"
        "beq         5f                            \n"
        "vld1.32     {d0[0]}, [%0]                 \n"
        "cmp         %5, #2                        \n"
        "blt         4f                            \n"
        "vld1.32     {d0[1]}, [%1]                 \n"
        "bgt         3f                            \n"
        "vst1.32     {d0}, [%4]                    \n"
        "b           5f                            \n"
        "3:                                        \n"
        "vld1.32     {d1[0]}, [%2]                 \n"
        "vst1.32     {d0}, [%4]!                   \n"
        "vst1.32     {d1[0]}, [%4]                 \n"
        "b           5f                            \n"
        "4:                                        \n"
        "vst1.32     {d0[0]}, [%4]                 \n"
        "5:                                        \n"
        : "+r"(src),    // %0
          "+r"(src1),   // %1
          "+r"(src2),   // %2
          "+r"(src3),   // %3
          "+r"(dst),    // %4
          "+r"(height)  // %5
        : "r"(s * 4)    // %6
        : "memory", "cc", "q0");
  }
}

void TransposeWx8_16_NEON(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* temp;
  asm volatile(
      "1:                                          \n"
      "mov         %[temp], %[src]                 \n"
      "vld1.8      {q0}, [%[temp]], %[s]           \n"
      "vld1.8      {q1}, [%[temp]], %[s]           \n"
      "vld1.8      {q2}, [%[temp]], %[s]           \n"
      "vld1.8      {q3}, [%[temp]], %[s]           \n"
      "vld1.8      {q8}, [%[temp]], %[s]           \n"
      "vld1.8      {q9}, [%[temp]], %[s]           \n"
      "vld1.8      {q10}, [%[temp]], %[s]          \n"
      "vld1.8      {q11}, [%[temp]]                \n"
      "add         %[src], %[src], #16             \n"

      // Stage 1: 16-bit transpose
      "vtrn.16     q0, q1                          \n"
      "vtrn.16     q2, q3                          \n"
      "vtrn.16     q8, q9                          \n"
      "vtrn.16     q10, q11                        \n"

      // Stage 2: 32-bit transpose
      "vtrn.32     q0, q2                          \n"
      "vtrn.32     q1, q3                          \n"
      "vtrn.32     q8, q10                         \n"
      "vtrn.32     q9, q11                         \n"

      // Stage 3: 64-bit swap
      "vswp        d1, d16                         \n"
      "vswp        d3, d18                         \n"
      "vswp        d5, d20                         \n"
      "vswp        d7, d22                         \n"

      "subs        %[w], %[w], #8                  \n"
      "vst1.8      {q0}, [%[dst]], %[d]            \n"
      "vst1.8      {q1}, [%[dst]], %[d]            \n"
      "vst1.8      {q2}, [%[dst]], %[d]            \n"
      "vst1.8      {q3}, [%[dst]], %[d]            \n"
      "vst1.8      {q8}, [%[dst]], %[d]            \n"
      "vst1.8      {q9}, [%[dst]], %[d]            \n"
      "vst1.8      {q10}, [%[dst]], %[d]           \n"
      "vst1.8      {q11}, [%[dst]], %[d]           \n"
      "bgt         1b                              \n"
      : [src] "+r"(src),    // %[src]
        [dst] "+r"(dst),    // %[dst]
        [w] "+r"(width),    // %[w]
        [temp] "=&r"(temp)  // %[temp]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "cc", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11");
}

#endif  // defined(__ARM_NEON__) && !defined(__aarch64__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
