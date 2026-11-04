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

#if !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)
#include "libyuv/loongson_intrinsics.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Transpose 32 bit values (ARGB) in 2x 4x4 tiles per step (4 cols x 8 rows ->
// 8 cols x 4 rows). Width is a multiple of 8.
void TransposeWx4_32_LASX(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width) {
  ptrdiff_t s = src_stride;
  ptrdiff_t s2 = s * 2;
  ptrdiff_t s3 = s + s2;
  ptrdiff_t s4 = s * 4;
  uint8_t* dst0 = dst;
  uint8_t* dst1 = dst0 + dst_stride;
  uint8_t* dst2 = dst1 + dst_stride;
  uint8_t* dst3 = dst2 + dst_stride;
  const uint8_t* src4;

  asm volatile(
      "srli.w     %[w], %[w], 3                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "vld        $vr0, %[src], 0                      \n"
      "vldx       $vr1, %[src], %[s]                   \n"
      "vldx       $vr2, %[src], %[s2]                  \n"
      "vldx       $vr3, %[src], %[s3]                  \n"
      "add.d      %[src4], %[src], %[s4]               \n"
      "vld        $vr4, %[src4], 0                     \n"
      "vldx       $vr5, %[src4], %[s]                  \n"
      "vldx       $vr6, %[src4], %[s2]                 \n"
      "vldx       $vr7, %[src4], %[s3]                 \n"
      "add.d      %[src], %[src4], %[s4]               \n"

      // Combine rows (0,4), (1,5), (2,6), (3,7) into 256-bit xr0..xr3
      "xvpermi.q  $xr0, $xr4, 0x02                     \n"
      "xvpermi.q  $xr1, $xr5, 0x02                     \n"
      "xvpermi.q  $xr2, $xr6, 0x02                     \n"
      "xvpermi.q  $xr3, $xr7, 0x02                     \n"

      // Transpose 2x2 across both 128-bit lanes
      "xvilvl.w   $xr4, $xr1, $xr0                     \n"
      "xvilvh.w   $xr5, $xr1, $xr0                     \n"
      "xvilvl.w   $xr6, $xr3, $xr2                     \n"
      "xvilvh.w   $xr7, $xr3, $xr2                     \n"

      // Transpose 4x4 across both 128-bit lanes
      "xvilvl.d   $xr0, $xr6, $xr4                     \n"
      "xvilvh.d   $xr1, $xr6, $xr4                     \n"
      "xvilvl.d   $xr2, $xr7, $xr5                     \n"
      "xvilvh.d   $xr3, $xr7, $xr5                     \n"

      // Store 4 full 256-bit (32-byte) destination rows
      "xvst       $xr0, %[dst0], 0                     \n"
      "xvst       $xr1, %[dst1], 0                     \n"
      "xvst       $xr2, %[dst2], 0                     \n"
      "xvst       $xr3, %[dst3], 0                     \n"
      "addi.d     %[dst0], %[dst0], 32                 \n"
      "addi.d     %[dst1], %[dst1], 32                 \n"
      "addi.d     %[dst2], %[dst2], 32                 \n"
      "addi.d     %[dst3], %[dst3], 32                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src),     // %[src]
        [src4] "=&r"(src4),  // %[src4]
        [dst0] "+r"(dst0),   // %[dst0]
        [dst1] "+r"(dst1),   // %[dst1]
        [dst2] "+r"(dst2),   // %[dst2]
        [dst3] "+r"(dst3),   // %[dst3]
        [w] "+r"(width)      // %[w]
      : [s] "r"(s),          // %[s]
        [s2] "r"(s2),        // %[s2]
        [s3] "r"(s3),        // %[s3]
        [s4] "r"(s4)         // %[s4]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7");
}

void TransposeWx16_16_LASX(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width) {
  const ptrdiff_t s = (ptrdiff_t)src_stride * 2;
  const ptrdiff_t d = (ptrdiff_t)dst_stride * 2;
  const uint8_t* src_b = (const uint8_t*)src;
  uint8_t* dst_b = (uint8_t*)dst;
  const uint8_t* t;
  asm volatile(
      "srli.w     %[w], %[w], 3                        \n"
      "beqz       %[w], 2f                             \n"
      "1:                                              \n"
      "move       %[t], %[src]                         \n"
      "vld        $vr0, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr1, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr2, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr3, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr4, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr5, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr6, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr7, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"

      "vld        $vr8, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr9, %[t], 0                        \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr10, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr11, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr12, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr13, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr14, %[t], 0                       \n"
      "add.d      %[t], %[t], %[s]                     \n"
      "vld        $vr15, %[t], 0                       \n"
      "addi.d     %[src], %[src], 16                   \n"

      "xvpermi.q  $xr0, $xr8, 0x02                     \n"
      "xvpermi.q  $xr1, $xr9, 0x02                     \n"
      "xvpermi.q  $xr2, $xr10, 0x02                    \n"
      "xvpermi.q  $xr3, $xr11, 0x02                    \n"
      "xvpermi.q  $xr4, $xr12, 0x02                    \n"
      "xvpermi.q  $xr5, $xr13, 0x02                    \n"
      "xvpermi.q  $xr6, $xr14, 0x02                    \n"
      "xvpermi.q  $xr7, $xr15, 0x02                    \n"

      "xvilvl.h   $xr8, $xr1, $xr0                     \n"
      "xvilvh.h   $xr9, $xr1, $xr0                     \n"
      "xvilvl.h   $xr10, $xr3, $xr2                    \n"
      "xvilvh.h   $xr11, $xr3, $xr2                    \n"
      "xvilvl.h   $xr12, $xr5, $xr4                    \n"
      "xvilvh.h   $xr13, $xr5, $xr4                    \n"
      "xvilvl.h   $xr14, $xr7, $xr6                    \n"
      "xvilvh.h   $xr15, $xr7, $xr6                    \n"

      "xvilvl.w   $xr0, $xr10, $xr8                    \n"
      "xvilvh.w   $xr1, $xr10, $xr8                    \n"
      "xvilvl.w   $xr2, $xr11, $xr9                    \n"
      "xvilvh.w   $xr3, $xr11, $xr9                    \n"
      "xvilvl.w   $xr4, $xr14, $xr12                   \n"
      "xvilvh.w   $xr5, $xr14, $xr12                   \n"
      "xvilvl.w   $xr6, $xr15, $xr13                   \n"
      "xvilvh.w   $xr7, $xr15, $xr13                   \n"

      "xvilvl.d   $xr8, $xr4, $xr0                     \n"
      "xvilvh.d   $xr9, $xr4, $xr0                     \n"
      "xvilvl.d   $xr10, $xr5, $xr1                    \n"
      "xvilvh.d   $xr11, $xr5, $xr1                    \n"
      "xvilvl.d   $xr12, $xr6, $xr2                    \n"
      "xvilvh.d   $xr13, $xr6, $xr2                    \n"
      "xvilvl.d   $xr14, $xr7, $xr3                    \n"
      "xvilvh.d   $xr15, $xr7, $xr3                    \n"

      "xvst       $xr8, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr9, %[dst], 0                      \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr10, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr11, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr12, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr13, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr14, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "xvst       $xr15, %[dst], 0                     \n"
      "add.d      %[dst], %[dst], %[d]                 \n"
      "addi.w     %[w], %[w], -1                       \n"
      "bnez       %[w], 1b                             \n"
      "2:                                              \n"
      : [src] "+r"(src_b),  // %[src]
        [dst] "+r"(dst_b),  // %[dst]
        [w] "+r"(width),    // %[w]
        [t] "=&r"(t)        // %[t]
      : [s] "r"(s),         // %[s]
        [d] "r"(d)          // %[d]
      : "memory", "$xr0", "$xr1", "$xr2", "$xr3", "$xr4", "$xr5", "$xr6",
        "$xr7", "$xr8", "$xr9", "$xr10", "$xr11", "$xr12", "$xr13", "$xr14",
        "$xr15");
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)
