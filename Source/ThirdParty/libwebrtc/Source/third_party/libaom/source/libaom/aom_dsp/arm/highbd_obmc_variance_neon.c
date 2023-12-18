/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
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

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static INLINE void highbd_obmc_variance_8x1_s16_neon(uint16x8_t pre,
                                                     const int32_t *wsrc,
                                                     const int32_t *mask,
                                                     uint32x4_t *sse,
                                                     int32x4_t *sum) {
  int16x8_t pre_s16 = vreinterpretq_s16_u16(pre);
  int32x4_t wsrc_lo = vld1q_s32(&wsrc[0]);
  int32x4_t wsrc_hi = vld1q_s32(&wsrc[4]);

  int32x4_t mask_lo = vld1q_s32(&mask[0]);
  int32x4_t mask_hi = vld1q_s32(&mask[4]);

  int16x8_t mask_s16 = vcombine_s16(vmovn_s32(mask_lo), vmovn_s32(mask_hi));

  int32x4_t diff_lo = vmull_s16(vget_low_s16(pre_s16), vget_low_s16(mask_s16));
  int32x4_t diff_hi =
      vmull_s16(vget_high_s16(pre_s16), vget_high_s16(mask_s16));

  diff_lo = vsubq_s32(wsrc_lo, diff_lo);
  diff_hi = vsubq_s32(wsrc_hi, diff_hi);

  // ROUND_POWER_OF_TWO_SIGNED(value, 12) rounds to nearest with ties away
  // from zero, however vrshrq_n_s32 rounds to nearest with ties rounded up.
  // This difference only affects the bit patterns at the rounding breakpoints
  // exactly, so we can add -1 to all negative numbers to move the breakpoint
  // one value across and into the correct rounding region.
  diff_lo = vsraq_n_s32(diff_lo, diff_lo, 31);
  diff_hi = vsraq_n_s32(diff_hi, diff_hi, 31);
  int32x4_t round_lo = vrshrq_n_s32(diff_lo, 12);
  int32x4_t round_hi = vrshrq_n_s32(diff_hi, 12);

  *sum = vaddq_s32(*sum, round_lo);
  *sum = vaddq_s32(*sum, round_hi);
  *sse = vmlaq_u32(*sse, vreinterpretq_u32_s32(round_lo),
                   vreinterpretq_u32_s32(round_lo));
  *sse = vmlaq_u32(*sse, vreinterpretq_u32_s32(round_hi),
                   vreinterpretq_u32_s32(round_hi));
}

// For 12-bit data, we can only accumulate up to 256 elements in the unsigned
// 32-bit elements (4095*4095*256 = 4292870400) before we have to accumulate
// into 64-bit elements. Therefore blocks of size 32x64, 64x32, 64x64, 64x128,
// 128x64, 128x128 are processed in a different helper function.
static INLINE void highbd_obmc_variance_xlarge_neon(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int width, int h, int h_limit, uint64_t *sse,
    int64_t *sum) {
  uint16_t *pre_ptr = CONVERT_TO_SHORTPTR(pre);
  int32x4_t sum_s32 = vdupq_n_s32(0);
  uint64x2_t sse_u64 = vdupq_n_u64(0);

  // 'h_limit' is the number of 'w'-width rows we can process before our 32-bit
  // accumulator overflows. After hitting this limit we accumulate into 64-bit
  // elements.
  int h_tmp = h > h_limit ? h_limit : h;

  do {
    uint32x4_t sse_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };
    int j = 0;

    do {
      int i = 0;

      do {
        uint16x8_t pre0 = vld1q_u16(pre_ptr + i);
        highbd_obmc_variance_8x1_s16_neon(pre0, wsrc, mask, &sse_u32[0],
                                          &sum_s32);

        uint16x8_t pre1 = vld1q_u16(pre_ptr + i + 8);
        highbd_obmc_variance_8x1_s16_neon(pre1, wsrc + 8, mask + 8, &sse_u32[1],
                                          &sum_s32);

        i += 16;
        wsrc += 16;
        mask += 16;
      } while (i < width);

      pre_ptr += pre_stride;
      j++;
    } while (j < h_tmp);

    sse_u64 = vpadalq_u32(sse_u64, sse_u32[0]);
    sse_u64 = vpadalq_u32(sse_u64, sse_u32[1]);
    h -= h_tmp;
  } while (h != 0);

  *sse = horizontal_add_u64x2(sse_u64);
  *sum = horizontal_long_add_s32x4(sum_s32);
}

static INLINE void highbd_obmc_variance_xlarge_neon_128xh(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int h, uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_xlarge_neon(pre, pre_stride, wsrc, mask, 128, h, 16, sse,
                                   sum);
}

static INLINE void highbd_obmc_variance_xlarge_neon_64xh(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int h, uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_xlarge_neon(pre, pre_stride, wsrc, mask, 64, h, 32, sse,
                                   sum);
}

static INLINE void highbd_obmc_variance_xlarge_neon_32xh(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int h, uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_xlarge_neon(pre, pre_stride, wsrc, mask, 32, h, 64, sse,
                                   sum);
}

static INLINE void highbd_obmc_variance_large_neon(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int width, int h, uint64_t *sse, int64_t *sum) {
  uint16_t *pre_ptr = CONVERT_TO_SHORTPTR(pre);
  uint32x4_t sse_u32 = vdupq_n_u32(0);
  int32x4_t sum_s32 = vdupq_n_s32(0);

  do {
    int i = 0;
    do {
      uint16x8_t pre0 = vld1q_u16(pre_ptr + i);
      highbd_obmc_variance_8x1_s16_neon(pre0, wsrc, mask, &sse_u32, &sum_s32);

      uint16x8_t pre1 = vld1q_u16(pre_ptr + i + 8);
      highbd_obmc_variance_8x1_s16_neon(pre1, wsrc + 8, mask + 8, &sse_u32,
                                        &sum_s32);

      i += 16;
      wsrc += 16;
      mask += 16;
    } while (i < width);

    pre_ptr += pre_stride;
  } while (--h != 0);

  *sse = horizontal_long_add_u32x4(sse_u32);
  *sum = horizontal_long_add_s32x4(sum_s32);
}

static INLINE void highbd_obmc_variance_neon_128xh(
    const uint8_t *pre, int pre_stride, const int32_t *wsrc,
    const int32_t *mask, int h, uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_large_neon(pre, pre_stride, wsrc, mask, 128, h, sse,
                                  sum);
}

static INLINE void highbd_obmc_variance_neon_64xh(const uint8_t *pre,
                                                  int pre_stride,
                                                  const int32_t *wsrc,
                                                  const int32_t *mask, int h,
                                                  uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_large_neon(pre, pre_stride, wsrc, mask, 64, h, sse, sum);
}

static INLINE void highbd_obmc_variance_neon_32xh(const uint8_t *pre,
                                                  int pre_stride,
                                                  const int32_t *wsrc,
                                                  const int32_t *mask, int h,
                                                  uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_large_neon(pre, pre_stride, wsrc, mask, 32, h, sse, sum);
}

static INLINE void highbd_obmc_variance_neon_16xh(const uint8_t *pre,
                                                  int pre_stride,
                                                  const int32_t *wsrc,
                                                  const int32_t *mask, int h,
                                                  uint64_t *sse, int64_t *sum) {
  highbd_obmc_variance_large_neon(pre, pre_stride, wsrc, mask, 16, h, sse, sum);
}

static INLINE void highbd_obmc_variance_neon_8xh(const uint8_t *pre8,
                                                 int pre_stride,
                                                 const int32_t *wsrc,
                                                 const int32_t *mask, int h,
                                                 uint64_t *sse, int64_t *sum) {
  uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);
  uint32x4_t sse_u32 = vdupq_n_u32(0);
  int32x4_t sum_s32 = vdupq_n_s32(0);

  do {
    uint16x8_t pre_u16 = vld1q_u16(pre);

    highbd_obmc_variance_8x1_s16_neon(pre_u16, wsrc, mask, &sse_u32, &sum_s32);

    pre += pre_stride;
    wsrc += 8;
    mask += 8;
  } while (--h != 0);

  *sse = horizontal_long_add_u32x4(sse_u32);
  *sum = horizontal_long_add_s32x4(sum_s32);
}

static INLINE void highbd_obmc_variance_neon_4xh(const uint8_t *pre8,
                                                 int pre_stride,
                                                 const int32_t *wsrc,
                                                 const int32_t *mask, int h,
                                                 uint64_t *sse, int64_t *sum) {
  assert(h % 2 == 0);
  uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);
  uint32x4_t sse_u32 = vdupq_n_u32(0);
  int32x4_t sum_s32 = vdupq_n_s32(0);

  do {
    uint16x8_t pre_u16 = load_unaligned_u16_4x2(pre, pre_stride);

    highbd_obmc_variance_8x1_s16_neon(pre_u16, wsrc, mask, &sse_u32, &sum_s32);

    pre += 2 * pre_stride;
    wsrc += 8;
    mask += 8;
    h -= 2;
  } while (h != 0);

  *sse = horizontal_long_add_u32x4(sse_u32);
  *sum = horizontal_long_add_s32x4(sum_s32);
}

static INLINE void highbd_8_obmc_variance_cast(int64_t sum64, uint64_t sse64,
                                               int *sum, unsigned int *sse) {
  *sum = (int)sum64;
  *sse = (unsigned int)sse64;
}

static INLINE void highbd_10_obmc_variance_cast(int64_t sum64, uint64_t sse64,
                                                int *sum, unsigned int *sse) {
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 2);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 4);
}

static INLINE void highbd_12_obmc_variance_cast(int64_t sum64, uint64_t sse64,
                                                int *sum, unsigned int *sse) {
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 4);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 8);
}

#define HIGHBD_OBMC_VARIANCE_WXH_NEON(w, h, bitdepth)                         \
  unsigned int aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(         \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,                \
      const int32_t *mask, unsigned int *sse) {                               \
    int sum;                                                                  \
    int64_t sum64;                                                            \
    uint64_t sse64;                                                           \
    highbd_obmc_variance_neon_##w##xh(pre, pre_stride, wsrc, mask, h, &sse64, \
                                      &sum64);                                \
    highbd_##bitdepth##_obmc_variance_cast(sum64, sse64, &sum, sse);          \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (w * h));             \
  }

#define HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(w, h, bitdepth)                 \
  unsigned int aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(        \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,               \
      const int32_t *mask, unsigned int *sse) {                              \
    int sum;                                                                 \
    int64_t sum64;                                                           \
    uint64_t sse64;                                                          \
    highbd_obmc_variance_xlarge_neon_##w##xh(pre, pre_stride, wsrc, mask, h, \
                                             &sse64, &sum64);                \
    highbd_##bitdepth##_obmc_variance_cast(sum64, sse64, &sum, sse);         \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (w * h));            \
  }

// 8-bit
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 4, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 8, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 16, 8)

HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 4, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 8, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 16, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 32, 8)

HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 4, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 8, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 16, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 32, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 64, 8)

HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 8, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 16, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 32, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 64, 8)

HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 16, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 32, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 64, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 128, 8)

HIGHBD_OBMC_VARIANCE_WXH_NEON(128, 64, 8)
HIGHBD_OBMC_VARIANCE_WXH_NEON(128, 128, 8)

// 10-bit
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 4, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 8, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 16, 10)

HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 4, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 8, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 16, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 32, 10)

HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 4, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 8, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 16, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 32, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 64, 10)

HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 8, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 16, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 32, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 64, 10)

HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 16, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 32, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 64, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 128, 10)

HIGHBD_OBMC_VARIANCE_WXH_NEON(128, 64, 10)
HIGHBD_OBMC_VARIANCE_WXH_NEON(128, 128, 10)

// 12-bit
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 4, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 8, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(4, 16, 12)

HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 4, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 8, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 16, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(8, 32, 12)

HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 4, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 8, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 16, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 32, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(16, 64, 12)

HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 8, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 16, 12)
HIGHBD_OBMC_VARIANCE_WXH_NEON(32, 32, 12)
HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(32, 64, 12)

HIGHBD_OBMC_VARIANCE_WXH_NEON(64, 16, 12)
HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(64, 32, 12)
HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(64, 64, 12)
HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(64, 128, 12)

HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(128, 64, 12)
HIGHBD_OBMC_VARIANCE_WXH_XLARGE_NEON(128, 128, 12)
