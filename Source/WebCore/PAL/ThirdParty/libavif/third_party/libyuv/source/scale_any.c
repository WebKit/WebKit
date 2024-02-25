/*
 *  Copyright 2015 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>  // For memset/memcpy

#include "libyuv/scale.h"
#include "libyuv/scale_row.h"

#include "libyuv/basic_types.h"

// Scale up horizontally 2 times using linear filter.
#define SUH2LANY(NAME, SIMD, C, MASK, PTYPE)                       \
  void NAME(const PTYPE* src_ptr, PTYPE* dst_ptr, int dst_width) { \
    int work_width = (dst_width - 1) & ~1;                         \
    int r = work_width & MASK;                                     \
    int n = work_width & ~MASK;                                    \
    dst_ptr[0] = src_ptr[0];                                       \
    if (work_width > 0) {                                          \
      if (n != 0) {                                                \
        SIMD(src_ptr, dst_ptr + 1, n);                             \
      }                                                            \
      C(src_ptr + (n / 2), dst_ptr + n + 1, r);                    \
    }                                                              \
    dst_ptr[dst_width - 1] = src_ptr[(dst_width - 1) / 2];         \
  }

// Even the C versions need to be wrapped, because boundary pixels have to
// be handled differently

SUH2LANY(ScaleRowUp2_Linear_Any_C,
         ScaleRowUp2_Linear_C,
         ScaleRowUp2_Linear_C,
         0,
         uint8_t)

SUH2LANY(ScaleRowUp2_Linear_16_Any_C,
         ScaleRowUp2_Linear_16_C,
         ScaleRowUp2_Linear_16_C,
         0,
         uint16_t)

// Scale up 2 times using bilinear filter.
// This function produces 2 rows at a time.
#define SU2BLANY(NAME, SIMD, C, MASK, PTYPE)                              \
  void NAME(const PTYPE* src_ptr, ptrdiff_t src_stride, PTYPE* dst_ptr,   \
            ptrdiff_t dst_stride, int dst_width) {                        \
    int work_width = (dst_width - 1) & ~1;                                \
    int r = work_width & MASK;                                            \
    int n = work_width & ~MASK;                                           \
    const PTYPE* sa = src_ptr;                                            \
    const PTYPE* sb = src_ptr + src_stride;                               \
    PTYPE* da = dst_ptr;                                                  \
    PTYPE* db = dst_ptr + dst_stride;                                     \
    da[0] = (3 * sa[0] + sb[0] + 2) >> 2;                                 \
    db[0] = (sa[0] + 3 * sb[0] + 2) >> 2;                                 \
    if (work_width > 0) {                                                 \
      if (n != 0) {                                                       \
        SIMD(sa, sb - sa, da + 1, db - da, n);                            \
      }                                                                   \
      C(sa + (n / 2), sb - sa, da + n + 1, db - da, r);                   \
    }                                                                     \
    da[dst_width - 1] =                                                   \
        (3 * sa[(dst_width - 1) / 2] + sb[(dst_width - 1) / 2] + 2) >> 2; \
    db[dst_width - 1] =                                                   \
        (sa[(dst_width - 1) / 2] + 3 * sb[(dst_width - 1) / 2] + 2) >> 2; \
  }

SU2BLANY(ScaleRowUp2_Bilinear_Any_C,
         ScaleRowUp2_Bilinear_C,
         ScaleRowUp2_Bilinear_C,
         0,
         uint8_t)

SU2BLANY(ScaleRowUp2_Bilinear_16_Any_C,
         ScaleRowUp2_Bilinear_16_C,
         ScaleRowUp2_Bilinear_16_C,
         0,
         uint16_t)
