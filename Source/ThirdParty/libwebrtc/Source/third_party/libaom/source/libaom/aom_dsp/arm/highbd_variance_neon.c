/*
 * Copyright (c) 2023 The WebM project authors. All rights reserved.
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/variance.h"

// Process a block of width 4 two rows at a time.
static INLINE void highbd_variance_4xh_neon(const uint16_t *src_ptr,
                                            int src_stride,
                                            const uint16_t *ref_ptr,
                                            int ref_stride, int h,
                                            uint64_t *sse, int64_t *sum) {
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_s32 = vdupq_n_s32(0);

  int i = h;
  do {
    const uint16x8_t s = load_unaligned_u16_4x2(src_ptr, src_stride);
    const uint16x8_t r = load_unaligned_u16_4x2(ref_ptr, ref_stride);

    int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(s, r));
    sum_s16 = vaddq_s16(sum_s16, diff);

    sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff), vget_low_s16(diff));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff), vget_high_s16(diff));

    src_ptr += 2 * src_stride;
    ref_ptr += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = horizontal_add_s32x4(sse_s32);
}

// For 8-bit and 10-bit data, since we're using two int32x4 accumulators, all
// block sizes can be processed in 32-bit elements (1023*1023*128*32 =
// 4286582784 for a 128x128 block).
static INLINE void highbd_variance_large_neon(const uint16_t *src_ptr,
                                              int src_stride,
                                              const uint16_t *ref_ptr,
                                              int ref_stride, int w, int h,
                                              uint64_t *sse, int64_t *sum) {
  int32x4_t sum_s32 = vdupq_n_s32(0);
  int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      const uint16x8_t s = vld1q_u16(src_ptr + j);
      const uint16x8_t r = vld1q_u16(ref_ptr + j);

      const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(s, r));
      sum_s32 = vpadalq_s16(sum_s32, diff);

      sse_s32[0] =
          vmlal_s16(sse_s32[0], vget_low_s16(diff), vget_low_s16(diff));
      sse_s32[1] =
          vmlal_s16(sse_s32[1], vget_high_s16(diff), vget_high_s16(diff));

      j += 8;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  *sum = horizontal_add_s32x4(sum_s32);
  *sse = horizontal_long_add_u32x4(vaddq_u32(
      vreinterpretq_u32_s32(sse_s32[0]), vreinterpretq_u32_s32(sse_s32[1])));
}

static INLINE void highbd_variance_8xh_neon(const uint16_t *src, int src_stride,
                                            const uint16_t *ref, int ref_stride,
                                            int h, uint64_t *sse,
                                            int64_t *sum) {
  highbd_variance_large_neon(src, src_stride, ref, ref_stride, 8, h, sse, sum);
}

static INLINE void highbd_variance_16xh_neon(const uint16_t *src,
                                             int src_stride,
                                             const uint16_t *ref,
                                             int ref_stride, int h,
                                             uint64_t *sse, int64_t *sum) {
  highbd_variance_large_neon(src, src_stride, ref, ref_stride, 16, h, sse, sum);
}

static INLINE void highbd_variance_32xh_neon(const uint16_t *src,
                                             int src_stride,
                                             const uint16_t *ref,
                                             int ref_stride, int h,
                                             uint64_t *sse, int64_t *sum) {
  highbd_variance_large_neon(src, src_stride, ref, ref_stride, 32, h, sse, sum);
}

static INLINE void highbd_variance_64xh_neon(const uint16_t *src,
                                             int src_stride,
                                             const uint16_t *ref,
                                             int ref_stride, int h,
                                             uint64_t *sse, int64_t *sum) {
  highbd_variance_large_neon(src, src_stride, ref, ref_stride, 64, h, sse, sum);
}

static INLINE void highbd_variance_128xh_neon(const uint16_t *src,
                                              int src_stride,
                                              const uint16_t *ref,
                                              int ref_stride, int h,
                                              uint64_t *sse, int64_t *sum) {
  highbd_variance_large_neon(src, src_stride, ref, ref_stride, 128, h, sse,
                             sum);
}

// For 12-bit data, we can only accumulate up to 128 elements in the sum of
// squares (4095*4095*128 = 2146435200), and because we're using two int32x4
// accumulators, we can only process up to 32 32-element rows (32*32/8 = 128)
// or 16 64-element rows before we have to accumulate into 64-bit elements.
// Therefore blocks of size 32x64, 64x32, 64x64, 64x128, 128x64, 128x128 are
// processed in a different helper function.

// Process a block of any size where the width is divisible by 8, with
// accumulation into 64-bit elements.
static INLINE void highbd_variance_xlarge_neon(
    const uint16_t *src_ptr, int src_stride, const uint16_t *ref_ptr,
    int ref_stride, int w, int h, int h_limit, uint64_t *sse, int64_t *sum) {
  int32x4_t sum_s32 = vdupq_n_s32(0);
  int64x2_t sse_s64 = vdupq_n_s64(0);

  // 'h_limit' is the number of 'w'-width rows we can process before our 32-bit
  // accumulator overflows. After hitting this limit we accumulate into 64-bit
  // elements.
  int h_tmp = h > h_limit ? h_limit : h;

  int i = 0;
  do {
    int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
    do {
      int j = 0;
      do {
        const uint16x8_t s0 = vld1q_u16(src_ptr + j);
        const uint16x8_t r0 = vld1q_u16(ref_ptr + j);

        const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(s0, r0));
        sum_s32 = vpadalq_s16(sum_s32, diff);

        sse_s32[0] =
            vmlal_s16(sse_s32[0], vget_low_s16(diff), vget_low_s16(diff));
        sse_s32[1] =
            vmlal_s16(sse_s32[1], vget_high_s16(diff), vget_high_s16(diff));

        j += 8;
      } while (j < w);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      i++;
    } while (i < h_tmp);

    sse_s64 = vpadalq_s32(sse_s64, sse_s32[0]);
    sse_s64 = vpadalq_s32(sse_s64, sse_s32[1]);
    h_tmp += h_limit;
  } while (i < h);

  *sum = horizontal_add_s32x4(sum_s32);
  *sse = (uint64_t)horizontal_add_s64x2(sse_s64);
}

static INLINE void highbd_variance_32xh_xlarge_neon(
    const uint16_t *src, int src_stride, const uint16_t *ref, int ref_stride,
    int h, uint64_t *sse, int64_t *sum) {
  highbd_variance_xlarge_neon(src, src_stride, ref, ref_stride, 32, h, 32, sse,
                              sum);
}

static INLINE void highbd_variance_64xh_xlarge_neon(
    const uint16_t *src, int src_stride, const uint16_t *ref, int ref_stride,
    int h, uint64_t *sse, int64_t *sum) {
  highbd_variance_xlarge_neon(src, src_stride, ref, ref_stride, 64, h, 16, sse,
                              sum);
}

static INLINE void highbd_variance_128xh_xlarge_neon(
    const uint16_t *src, int src_stride, const uint16_t *ref, int ref_stride,
    int h, uint64_t *sse, int64_t *sum) {
  highbd_variance_xlarge_neon(src, src_stride, ref, ref_stride, 128, h, 8, sse,
                              sum);
}

#define HBD_VARIANCE_WXH_8_NEON(w, h)                                 \
  uint32_t aom_highbd_8_variance##w##x##h##_neon(                     \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_neon(src, src_stride, ref, ref_stride, h, \
                                 &sse_long, &sum_long);               \
    *sse = (uint32_t)sse_long;                                        \
    sum = (int)sum_long;                                              \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (w * h));         \
  }

#define HBD_VARIANCE_WXH_10_NEON(w, h)                                \
  uint32_t aom_highbd_10_variance##w##x##h##_neon(                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    int64_t var;                                                      \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_neon(src, src_stride, ref, ref_stride, h, \
                                 &sse_long, &sum_long);               \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);                 \
    sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (w * h));         \
    return (var >= 0) ? (uint32_t)var : 0;                            \
  }

#define HBD_VARIANCE_WXH_12_NEON(w, h)                                \
  uint32_t aom_highbd_12_variance##w##x##h##_neon(                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    int64_t var;                                                      \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_neon(src, src_stride, ref, ref_stride, h, \
                                 &sse_long, &sum_long);               \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);                 \
    sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (w * h));         \
    return (var >= 0) ? (uint32_t)var : 0;                            \
  }

#define HBD_VARIANCE_WXH_12_XLARGE_NEON(w, h)                                \
  uint32_t aom_highbd_12_variance##w##x##h##_neon(                           \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride, uint32_t *sse) {                                       \
    int sum;                                                                 \
    int64_t var;                                                             \
    uint64_t sse_long = 0;                                                   \
    int64_t sum_long = 0;                                                    \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                            \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                            \
    highbd_variance_##w##xh_xlarge_neon(src, src_stride, ref, ref_stride, h, \
                                        &sse_long, &sum_long);               \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);                        \
    sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);                              \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (w * h));                \
    return (var >= 0) ? (uint32_t)var : 0;                                   \
  }

// 8-bit
HBD_VARIANCE_WXH_8_NEON(4, 4)
HBD_VARIANCE_WXH_8_NEON(4, 8)

HBD_VARIANCE_WXH_8_NEON(8, 4)
HBD_VARIANCE_WXH_8_NEON(8, 8)
HBD_VARIANCE_WXH_8_NEON(8, 16)

HBD_VARIANCE_WXH_8_NEON(16, 8)
HBD_VARIANCE_WXH_8_NEON(16, 16)
HBD_VARIANCE_WXH_8_NEON(16, 32)

HBD_VARIANCE_WXH_8_NEON(32, 16)
HBD_VARIANCE_WXH_8_NEON(32, 32)
HBD_VARIANCE_WXH_8_NEON(32, 64)

HBD_VARIANCE_WXH_8_NEON(64, 32)
HBD_VARIANCE_WXH_8_NEON(64, 64)
HBD_VARIANCE_WXH_8_NEON(64, 128)

HBD_VARIANCE_WXH_8_NEON(128, 64)
HBD_VARIANCE_WXH_8_NEON(128, 128)

// 10-bit
HBD_VARIANCE_WXH_10_NEON(4, 4)
HBD_VARIANCE_WXH_10_NEON(4, 8)

HBD_VARIANCE_WXH_10_NEON(8, 4)
HBD_VARIANCE_WXH_10_NEON(8, 8)
HBD_VARIANCE_WXH_10_NEON(8, 16)

HBD_VARIANCE_WXH_10_NEON(16, 8)
HBD_VARIANCE_WXH_10_NEON(16, 16)
HBD_VARIANCE_WXH_10_NEON(16, 32)

HBD_VARIANCE_WXH_10_NEON(32, 16)
HBD_VARIANCE_WXH_10_NEON(32, 32)
HBD_VARIANCE_WXH_10_NEON(32, 64)

HBD_VARIANCE_WXH_10_NEON(64, 32)
HBD_VARIANCE_WXH_10_NEON(64, 64)
HBD_VARIANCE_WXH_10_NEON(64, 128)

HBD_VARIANCE_WXH_10_NEON(128, 64)
HBD_VARIANCE_WXH_10_NEON(128, 128)

// 12-bit
HBD_VARIANCE_WXH_12_NEON(4, 4)
HBD_VARIANCE_WXH_12_NEON(4, 8)

HBD_VARIANCE_WXH_12_NEON(8, 4)
HBD_VARIANCE_WXH_12_NEON(8, 8)
HBD_VARIANCE_WXH_12_NEON(8, 16)

HBD_VARIANCE_WXH_12_NEON(16, 8)
HBD_VARIANCE_WXH_12_NEON(16, 16)
HBD_VARIANCE_WXH_12_NEON(16, 32)

HBD_VARIANCE_WXH_12_NEON(32, 16)
HBD_VARIANCE_WXH_12_NEON(32, 32)
HBD_VARIANCE_WXH_12_XLARGE_NEON(32, 64)

HBD_VARIANCE_WXH_12_XLARGE_NEON(64, 32)
HBD_VARIANCE_WXH_12_XLARGE_NEON(64, 64)
HBD_VARIANCE_WXH_12_XLARGE_NEON(64, 128)

HBD_VARIANCE_WXH_12_XLARGE_NEON(128, 64)
HBD_VARIANCE_WXH_12_XLARGE_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
// 8-bit
HBD_VARIANCE_WXH_8_NEON(4, 16)

HBD_VARIANCE_WXH_8_NEON(8, 32)

HBD_VARIANCE_WXH_8_NEON(16, 4)
HBD_VARIANCE_WXH_8_NEON(16, 64)

HBD_VARIANCE_WXH_8_NEON(32, 8)

HBD_VARIANCE_WXH_8_NEON(64, 16)

// 10-bit
HBD_VARIANCE_WXH_10_NEON(4, 16)

HBD_VARIANCE_WXH_10_NEON(8, 32)

HBD_VARIANCE_WXH_10_NEON(16, 4)
HBD_VARIANCE_WXH_10_NEON(16, 64)

HBD_VARIANCE_WXH_10_NEON(32, 8)

HBD_VARIANCE_WXH_10_NEON(64, 16)

// 12-bit
HBD_VARIANCE_WXH_12_NEON(4, 16)

HBD_VARIANCE_WXH_12_NEON(8, 32)

HBD_VARIANCE_WXH_12_NEON(16, 4)
HBD_VARIANCE_WXH_12_NEON(16, 64)

HBD_VARIANCE_WXH_12_NEON(32, 8)

HBD_VARIANCE_WXH_12_NEON(64, 16)

#endif  // !CONFIG_REALTIME_ONLY

static INLINE uint32_t highbd_mse_wxh_neon(const uint16_t *src_ptr,
                                           int src_stride,
                                           const uint16_t *ref_ptr,
                                           int ref_stride, int w, int h,
                                           unsigned int *sse) {
  uint32x4_t sse_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      uint16x8_t s = vld1q_u16(src_ptr + j);
      uint16x8_t r = vld1q_u16(ref_ptr + j);

      uint16x8_t diff = vabdq_u16(s, r);

      sse_u32[0] =
          vmlal_u16(sse_u32[0], vget_low_u16(diff), vget_low_u16(diff));
      sse_u32[1] =
          vmlal_u16(sse_u32[1], vget_high_u16(diff), vget_high_u16(diff));

      j += 8;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  *sse = horizontal_add_u32x4(vaddq_u32(sse_u32[0], sse_u32[1]));
  return *sse;
}

#define HIGHBD_MSE_WXH_NEON(w, h)                                     \
  uint32_t aom_highbd_8_mse##w##x##h##_neon(                          \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_mse_wxh_neon(src, src_stride, ref, ref_stride, w, h, sse); \
    return *sse;                                                      \
  }                                                                   \
                                                                      \
  uint32_t aom_highbd_10_mse##w##x##h##_neon(                         \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_mse_wxh_neon(src, src_stride, ref, ref_stride, w, h, sse); \
    *sse = ROUND_POWER_OF_TWO(*sse, 4);                               \
    return *sse;                                                      \
  }                                                                   \
                                                                      \
  uint32_t aom_highbd_12_mse##w##x##h##_neon(                         \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_mse_wxh_neon(src, src_stride, ref, ref_stride, w, h, sse); \
    *sse = ROUND_POWER_OF_TWO(*sse, 8);                               \
    return *sse;                                                      \
  }

HIGHBD_MSE_WXH_NEON(16, 16)
HIGHBD_MSE_WXH_NEON(16, 8)
HIGHBD_MSE_WXH_NEON(8, 16)
HIGHBD_MSE_WXH_NEON(8, 8)

#undef HIGHBD_MSE_WXH_NEON

static INLINE uint64x2_t mse_accumulate_u16_8x2(uint64x2_t sum, uint16x8_t s0,
                                                uint16x8_t s1, uint16x8_t d0,
                                                uint16x8_t d1) {
  uint16x8_t e0 = vabdq_u16(s0, d0);
  uint16x8_t e1 = vabdq_u16(s1, d1);

  uint32x4_t mse = vmull_u16(vget_low_u16(e0), vget_low_u16(e0));
  mse = vmlal_u16(mse, vget_high_u16(e0), vget_high_u16(e0));
  mse = vmlal_u16(mse, vget_low_u16(e1), vget_low_u16(e1));
  mse = vmlal_u16(mse, vget_high_u16(e1), vget_high_u16(e1));

  return vpadalq_u32(sum, mse);
}

uint64_t aom_mse_wxh_16bit_highbd_neon(uint16_t *dst, int dstride,
                                       uint16_t *src, int sstride, int w,
                                       int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4));

  uint64x2_t sum = vdupq_n_u64(0);

  if (w == 8) {
    do {
      uint16x8_t d0 = vld1q_u16(dst + 0 * dstride);
      uint16x8_t d1 = vld1q_u16(dst + 1 * dstride);
      uint16x8_t s0 = vld1q_u16(src + 0 * sstride);
      uint16x8_t s1 = vld1q_u16(src + 1 * sstride);

      sum = mse_accumulate_u16_8x2(sum, s0, s1, d0, d1);

      dst += 2 * dstride;
      src += 2 * sstride;
      h -= 2;
    } while (h != 0);
  } else {  // w == 4
    do {
      uint16x8_t d0 = load_unaligned_u16_4x2(dst + 0 * dstride, dstride);
      uint16x8_t d1 = load_unaligned_u16_4x2(dst + 2 * dstride, dstride);
      uint16x8_t s0 = load_unaligned_u16_4x2(src + 0 * sstride, sstride);
      uint16x8_t s1 = load_unaligned_u16_4x2(src + 2 * sstride, sstride);

      sum = mse_accumulate_u16_8x2(sum, s0, s1, d0, d1);

      dst += 4 * dstride;
      src += 4 * sstride;
      h -= 4;
    } while (h != 0);
  }

  return horizontal_add_u64x2(sum);
}
