/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/variance.h"

// Process a block of width 4 two rows at a time.
static inline void highbd_variance_4xh_sve(const uint16_t *src_ptr,
                                           int src_stride,
                                           const uint16_t *ref_ptr,
                                           int ref_stride, int h, uint64_t *sse,
                                           int64_t *sum) {
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int64x2_t sse_s64 = vdupq_n_s64(0);

  do {
    const uint16x8_t s = load_unaligned_u16_4x2(src_ptr, src_stride);
    const uint16x8_t r = load_unaligned_u16_4x2(ref_ptr, ref_stride);

    int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(s, r));
    sum_s16 = vaddq_s16(sum_s16, diff);

    sse_s64 = aom_sdotq_s16(sse_s64, diff, diff);

    src_ptr += 2 * src_stride;
    ref_ptr += 2 * ref_stride;
    h -= 2;
  } while (h != 0);

  *sum = vaddlvq_s16(sum_s16);
  *sse = vaddvq_s64(sse_s64);
}

static inline void variance_8x1_sve(const uint16_t *src, const uint16_t *ref,
                                    int32x4_t *sum, int64x2_t *sse) {
  const uint16x8_t s = vld1q_u16(src);
  const uint16x8_t r = vld1q_u16(ref);

  const int16x8_t diff = vreinterpretq_s16_u16(vsubq_u16(s, r));
  *sum = vpadalq_s16(*sum, diff);

  *sse = aom_sdotq_s16(*sse, diff, diff);
}

static inline void highbd_variance_8xh_sve(const uint16_t *src_ptr,
                                           int src_stride,
                                           const uint16_t *ref_ptr,
                                           int ref_stride, int h, uint64_t *sse,
                                           int64_t *sum) {
  int32x4_t sum_s32 = vdupq_n_s32(0);
  int64x2_t sse_s64 = vdupq_n_s64(0);

  do {
    variance_8x1_sve(src_ptr, ref_ptr, &sum_s32, &sse_s64);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--h != 0);

  *sum = vaddlvq_s32(sum_s32);
  *sse = vaddvq_s64(sse_s64);
}

static inline void highbd_variance_16xh_sve(const uint16_t *src_ptr,
                                            int src_stride,
                                            const uint16_t *ref_ptr,
                                            int ref_stride, int h,
                                            uint64_t *sse, int64_t *sum) {
  int32x4_t sum_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
  int64x2_t sse_s64[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  do {
    variance_8x1_sve(src_ptr, ref_ptr, &sum_s32[0], &sse_s64[0]);
    variance_8x1_sve(src_ptr + 8, ref_ptr + 8, &sum_s32[1], &sse_s64[1]);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--h != 0);

  *sum = vaddlvq_s32(vaddq_s32(sum_s32[0], sum_s32[1]));
  *sse = vaddvq_s64(vaddq_s64(sse_s64[0], sse_s64[1]));
}

static inline void highbd_variance_large_sve(const uint16_t *src_ptr,
                                             int src_stride,
                                             const uint16_t *ref_ptr,
                                             int ref_stride, int w, int h,
                                             uint64_t *sse, int64_t *sum) {
  int32x4_t sum_s32[4] = { vdupq_n_s32(0), vdupq_n_s32(0), vdupq_n_s32(0),
                           vdupq_n_s32(0) };
  int64x2_t sse_s64[4] = { vdupq_n_s64(0), vdupq_n_s64(0), vdupq_n_s64(0),
                           vdupq_n_s64(0) };

  do {
    int j = 0;
    do {
      variance_8x1_sve(src_ptr + j, ref_ptr + j, &sum_s32[0], &sse_s64[0]);
      variance_8x1_sve(src_ptr + j + 8, ref_ptr + j + 8, &sum_s32[1],
                       &sse_s64[1]);
      variance_8x1_sve(src_ptr + j + 16, ref_ptr + j + 16, &sum_s32[2],
                       &sse_s64[2]);
      variance_8x1_sve(src_ptr + j + 24, ref_ptr + j + 24, &sum_s32[3],
                       &sse_s64[3]);

      j += 32;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--h != 0);

  sum_s32[0] = vaddq_s32(sum_s32[0], sum_s32[1]);
  sum_s32[2] = vaddq_s32(sum_s32[2], sum_s32[3]);
  *sum = vaddlvq_s32(vaddq_s32(sum_s32[0], sum_s32[2]));
  sse_s64[0] = vaddq_s64(sse_s64[0], sse_s64[1]);
  sse_s64[2] = vaddq_s64(sse_s64[2], sse_s64[3]);
  *sse = vaddvq_s64(vaddq_s64(sse_s64[0], sse_s64[2]));
}

static inline void highbd_variance_32xh_sve(const uint16_t *src, int src_stride,
                                            const uint16_t *ref, int ref_stride,
                                            int h, uint64_t *sse,
                                            int64_t *sum) {
  highbd_variance_large_sve(src, src_stride, ref, ref_stride, 32, h, sse, sum);
}

static inline void highbd_variance_64xh_sve(const uint16_t *src, int src_stride,
                                            const uint16_t *ref, int ref_stride,
                                            int h, uint64_t *sse,
                                            int64_t *sum) {
  highbd_variance_large_sve(src, src_stride, ref, ref_stride, 64, h, sse, sum);
}

static inline void highbd_variance_128xh_sve(const uint16_t *src,
                                             int src_stride,
                                             const uint16_t *ref,
                                             int ref_stride, int h,
                                             uint64_t *sse, int64_t *sum) {
  highbd_variance_large_sve(src, src_stride, ref, ref_stride, 128, h, sse, sum);
}

#define HBD_VARIANCE_WXH_8_SVE(w, h)                                  \
  uint32_t aom_highbd_8_variance##w##x##h##_sve(                      \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_sve(src, src_stride, ref, ref_stride, h,  \
                                &sse_long, &sum_long);                \
    *sse = (uint32_t)sse_long;                                        \
    sum = (int)sum_long;                                              \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (w * h));         \
  }

#define HBD_VARIANCE_WXH_10_SVE(w, h)                                 \
  uint32_t aom_highbd_10_variance##w##x##h##_sve(                     \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    int64_t var;                                                      \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_sve(src, src_stride, ref, ref_stride, h,  \
                                &sse_long, &sum_long);                \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);                 \
    sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (w * h));         \
    return (var >= 0) ? (uint32_t)var : 0;                            \
  }

#define HBD_VARIANCE_WXH_12_SVE(w, h)                                 \
  uint32_t aom_highbd_12_variance##w##x##h##_sve(                     \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    int sum;                                                          \
    int64_t var;                                                      \
    uint64_t sse_long = 0;                                            \
    int64_t sum_long = 0;                                             \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_variance_##w##xh_sve(src, src_stride, ref, ref_stride, h,  \
                                &sse_long, &sum_long);                \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);                 \
    sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (w * h));         \
    return (var >= 0) ? (uint32_t)var : 0;                            \
  }

// 8-bit
HBD_VARIANCE_WXH_8_SVE(4, 4)
HBD_VARIANCE_WXH_8_SVE(4, 8)

HBD_VARIANCE_WXH_8_SVE(8, 4)
HBD_VARIANCE_WXH_8_SVE(8, 8)
HBD_VARIANCE_WXH_8_SVE(8, 16)

HBD_VARIANCE_WXH_8_SVE(16, 8)
HBD_VARIANCE_WXH_8_SVE(16, 16)
HBD_VARIANCE_WXH_8_SVE(16, 32)

HBD_VARIANCE_WXH_8_SVE(32, 16)
HBD_VARIANCE_WXH_8_SVE(32, 32)
HBD_VARIANCE_WXH_8_SVE(32, 64)

HBD_VARIANCE_WXH_8_SVE(64, 32)
HBD_VARIANCE_WXH_8_SVE(64, 64)
HBD_VARIANCE_WXH_8_SVE(64, 128)

HBD_VARIANCE_WXH_8_SVE(128, 64)
HBD_VARIANCE_WXH_8_SVE(128, 128)

// 10-bit
HBD_VARIANCE_WXH_10_SVE(4, 4)
HBD_VARIANCE_WXH_10_SVE(4, 8)

HBD_VARIANCE_WXH_10_SVE(8, 4)
HBD_VARIANCE_WXH_10_SVE(8, 8)
HBD_VARIANCE_WXH_10_SVE(8, 16)

HBD_VARIANCE_WXH_10_SVE(16, 8)
HBD_VARIANCE_WXH_10_SVE(16, 16)
HBD_VARIANCE_WXH_10_SVE(16, 32)

HBD_VARIANCE_WXH_10_SVE(32, 16)
HBD_VARIANCE_WXH_10_SVE(32, 32)
HBD_VARIANCE_WXH_10_SVE(32, 64)

HBD_VARIANCE_WXH_10_SVE(64, 32)
HBD_VARIANCE_WXH_10_SVE(64, 64)
HBD_VARIANCE_WXH_10_SVE(64, 128)

HBD_VARIANCE_WXH_10_SVE(128, 64)
HBD_VARIANCE_WXH_10_SVE(128, 128)

// 12-bit
HBD_VARIANCE_WXH_12_SVE(4, 4)
HBD_VARIANCE_WXH_12_SVE(4, 8)

HBD_VARIANCE_WXH_12_SVE(8, 4)
HBD_VARIANCE_WXH_12_SVE(8, 8)
HBD_VARIANCE_WXH_12_SVE(8, 16)

HBD_VARIANCE_WXH_12_SVE(16, 8)
HBD_VARIANCE_WXH_12_SVE(16, 16)
HBD_VARIANCE_WXH_12_SVE(16, 32)

HBD_VARIANCE_WXH_12_SVE(32, 16)
HBD_VARIANCE_WXH_12_SVE(32, 32)
HBD_VARIANCE_WXH_12_SVE(32, 64)

HBD_VARIANCE_WXH_12_SVE(64, 32)
HBD_VARIANCE_WXH_12_SVE(64, 64)
HBD_VARIANCE_WXH_12_SVE(64, 128)

HBD_VARIANCE_WXH_12_SVE(128, 64)
HBD_VARIANCE_WXH_12_SVE(128, 128)

#if !CONFIG_REALTIME_ONLY
// 8-bit
HBD_VARIANCE_WXH_8_SVE(4, 16)

HBD_VARIANCE_WXH_8_SVE(8, 32)

HBD_VARIANCE_WXH_8_SVE(16, 4)
HBD_VARIANCE_WXH_8_SVE(16, 64)

HBD_VARIANCE_WXH_8_SVE(32, 8)

HBD_VARIANCE_WXH_8_SVE(64, 16)

// 10-bit
HBD_VARIANCE_WXH_10_SVE(4, 16)

HBD_VARIANCE_WXH_10_SVE(8, 32)

HBD_VARIANCE_WXH_10_SVE(16, 4)
HBD_VARIANCE_WXH_10_SVE(16, 64)

HBD_VARIANCE_WXH_10_SVE(32, 8)

HBD_VARIANCE_WXH_10_SVE(64, 16)

// 12-bit
HBD_VARIANCE_WXH_12_SVE(4, 16)

HBD_VARIANCE_WXH_12_SVE(8, 32)

HBD_VARIANCE_WXH_12_SVE(16, 4)
HBD_VARIANCE_WXH_12_SVE(16, 64)

HBD_VARIANCE_WXH_12_SVE(32, 8)

HBD_VARIANCE_WXH_12_SVE(64, 16)

#endif  // !CONFIG_REALTIME_ONLY

#undef HBD_VARIANCE_WXH_8_SVE
#undef HBD_VARIANCE_WXH_10_SVE
#undef HBD_VARIANCE_WXH_12_SVE

static inline uint32_t highbd_mse_wxh_sve(const uint16_t *src_ptr,
                                          int src_stride,
                                          const uint16_t *ref_ptr,
                                          int ref_stride, int w, int h,
                                          unsigned int *sse) {
  uint64x2_t sse_u64 = vdupq_n_u64(0);

  do {
    int j = 0;
    do {
      uint16x8_t s = vld1q_u16(src_ptr + j);
      uint16x8_t r = vld1q_u16(ref_ptr + j);

      uint16x8_t diff = vabdq_u16(s, r);

      sse_u64 = aom_udotq_u16(sse_u64, diff, diff);

      j += 8;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--h != 0);

  *sse = (uint32_t)vaddvq_u64(sse_u64);
  return *sse;
}

#define HIGHBD_MSE_WXH_SVE(w, h)                                      \
  uint32_t aom_highbd_10_mse##w##x##h##_sve(                          \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_mse_wxh_sve(src, src_stride, ref, ref_stride, w, h, sse);  \
    *sse = ROUND_POWER_OF_TWO(*sse, 4);                               \
    return *sse;                                                      \
  }                                                                   \
                                                                      \
  uint32_t aom_highbd_12_mse##w##x##h##_sve(                          \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, \
      int ref_stride, uint32_t *sse) {                                \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                     \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                     \
    highbd_mse_wxh_sve(src, src_stride, ref, ref_stride, w, h, sse);  \
    *sse = ROUND_POWER_OF_TWO(*sse, 8);                               \
    return *sse;                                                      \
  }

HIGHBD_MSE_WXH_SVE(16, 16)
HIGHBD_MSE_WXH_SVE(16, 8)
HIGHBD_MSE_WXH_SVE(8, 16)
HIGHBD_MSE_WXH_SVE(8, 8)

#undef HIGHBD_MSE_WXH_SVE

uint64_t aom_mse_wxh_16bit_highbd_sve(uint16_t *dst, int dstride, uint16_t *src,
                                      int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4));

  uint64x2_t sum = vdupq_n_u64(0);

  if (w == 8) {
    do {
      uint16x8_t d0 = vld1q_u16(dst + 0 * dstride);
      uint16x8_t d1 = vld1q_u16(dst + 1 * dstride);
      uint16x8_t s0 = vld1q_u16(src + 0 * sstride);
      uint16x8_t s1 = vld1q_u16(src + 1 * sstride);

      uint16x8_t abs_diff0 = vabdq_u16(s0, d0);
      uint16x8_t abs_diff1 = vabdq_u16(s1, d1);

      sum = aom_udotq_u16(sum, abs_diff0, abs_diff0);
      sum = aom_udotq_u16(sum, abs_diff1, abs_diff1);

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

      uint16x8_t abs_diff0 = vabdq_u16(s0, d0);
      uint16x8_t abs_diff1 = vabdq_u16(s1, d1);

      sum = aom_udotq_u16(sum, abs_diff0, abs_diff0);
      sum = aom_udotq_u16(sum, abs_diff1, abs_diff1);

      dst += 4 * dstride;
      src += 4 * sstride;
      h -= 4;
    } while (h != 0);
  }

  return vaddvq_u64(sum);
}
