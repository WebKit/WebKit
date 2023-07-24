/*
 * Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/variance.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/sum_neon.h"

typedef void (*high_variance_fn_t)(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride,
                                   uint32_t *sse, int *sum);

void aom_highbd_calc16x16var_neon(const uint16_t *src, int src_stride,
                                  const uint16_t *ref, int ref_stride,
                                  uint32_t *sse, int *sum) {
  int i, j;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < 16; ++i) {
    for (j = 0; j < 16; j += 8) {
      const uint16x8_t v_a = vld1q_u16(&src[j]);
      const uint16x8_t v_b = vld1q_u16(&ref[j]);
      const int16x8_t sv_diff = vreinterpretq_s16_u16(vsubq_u16(v_a, v_b));
      v_sum = vaddq_s16(v_sum, sv_diff);
      v_sse_lo =
          vmlal_s16(v_sse_lo, vget_low_s16(sv_diff), vget_low_s16(sv_diff));
      v_sse_hi =
          vmlal_s16(v_sse_hi, vget_high_s16(sv_diff), vget_high_s16(sv_diff));
    }
    src += src_stride;
    ref += ref_stride;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

void aom_highbd_calc8x8var_neon(const uint16_t *src, int src_stride,
                                const uint16_t *ref, int ref_stride,
                                uint32_t *sse, int *sum) {
  int i;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < 8; ++i) {
    const uint16x8_t v_a = vld1q_u16(&src[0]);
    const uint16x8_t v_b = vld1q_u16(&ref[0]);
    const int16x8_t sv_diff = vreinterpretq_s16_u16(vsubq_u16(v_a, v_b));
    v_sum = vaddq_s16(v_sum, sv_diff);
    v_sse_lo =
        vmlal_s16(v_sse_lo, vget_low_s16(sv_diff), vget_low_s16(sv_diff));
    v_sse_hi =
        vmlal_s16(v_sse_hi, vget_high_s16(sv_diff), vget_high_s16(sv_diff));
    src += src_stride;
    ref += ref_stride;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

void aom_highbd_calc4x4var_neon(const uint16_t *src, int src_stride,
                                const uint16_t *ref, int ref_stride,
                                uint32_t *sse, int *sum) {
  int i;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < 4; i += 2) {
    const uint16x4_t v_a_r0 = vld1_u16(&src[0]);
    const uint16x4_t v_b_r0 = vld1_u16(&ref[0]);
    const uint16x4_t v_a_r1 = vld1_u16(&src[src_stride]);
    const uint16x4_t v_b_r1 = vld1_u16(&ref[ref_stride]);
    const uint16x8_t v_a = vcombine_u16(v_a_r0, v_a_r1);
    const uint16x8_t v_b = vcombine_u16(v_b_r0, v_b_r1);
    const int16x8_t sv_diff = vreinterpretq_s16_u16(vsubq_u16(v_a, v_b));
    v_sum = vaddq_s16(v_sum, sv_diff);
    v_sse_lo =
        vmlal_s16(v_sse_lo, vget_low_s16(sv_diff), vget_low_s16(sv_diff));
    v_sse_hi =
        vmlal_s16(v_sse_hi, vget_high_s16(sv_diff), vget_high_s16(sv_diff));
    src += src_stride << 1;
    ref += ref_stride << 1;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

static void highbd_10_variance_neon(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 2);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
}

#define VAR_FN(w, h, block_size, shift)                                    \
  uint32_t aom_highbd_10_variance##w##x##h##_neon(                         \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,            \
      int ref_stride, uint32_t *sse) {                                     \
    int sum;                                                               \
    int64_t var;                                                           \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                             \
    highbd_10_variance_neon(                                               \
        src, src_stride, ref, ref_stride, w, h, sse, &sum,                 \
        aom_highbd_calc##block_size##x##block_size##var_neon, block_size); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);               \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

VAR_FN(128, 128, 16, 14)
VAR_FN(128, 64, 16, 13)
VAR_FN(64, 128, 16, 13)
VAR_FN(64, 64, 16, 12)
VAR_FN(64, 32, 16, 11)
VAR_FN(32, 64, 16, 11)
VAR_FN(32, 32, 16, 10)
VAR_FN(32, 16, 16, 9)
VAR_FN(16, 32, 16, 9)
VAR_FN(16, 16, 16, 8)
VAR_FN(16, 8, 8, 7)
VAR_FN(8, 16, 8, 7)
VAR_FN(8, 8, 8, 6)

VAR_FN(16, 4, 4, 6)
VAR_FN(4, 16, 4, 6)

VAR_FN(8, 4, 4, 5)
VAR_FN(4, 8, 4, 5)
VAR_FN(4, 4, 4, 4)

#if !CONFIG_REALTIME_ONLY
VAR_FN(64, 16, 16, 10)
VAR_FN(16, 64, 16, 10)
VAR_FN(8, 32, 8, 8)
VAR_FN(32, 8, 8, 8)
#endif  // !CONFIG_REALTIME_ONLY

#undef VAR_FN

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

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE uint32_t highbd_mse8_8xh_neon(const uint16_t *src_ptr,
                                            int src_stride,
                                            const uint16_t *ref_ptr,
                                            int ref_stride, int h,
                                            unsigned int *sse) {
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h / 2;
  do {
    uint16x8_t s0 = vld1q_u16(src_ptr);
    src_ptr += src_stride;
    uint16x8_t s1 = vld1q_u16(src_ptr);
    src_ptr += src_stride;
    uint16x8_t r0 = vld1q_u16(ref_ptr);
    ref_ptr += ref_stride;
    uint16x8_t r1 = vld1q_u16(ref_ptr);
    ref_ptr += ref_stride;

    uint8x16_t s = vcombine_u8(vmovn_u16(s0), vmovn_u16(s1));
    uint8x16_t r = vcombine_u8(vmovn_u16(r0), vmovn_u16(r1));

    uint8x16_t diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, diff, diff);
  } while (--i != 0);

  *sse = horizontal_add_u32x4(sse_u32);
  return *sse;
}

static INLINE uint32_t highbd_mse8_16xh_neon(const uint16_t *src_ptr,
                                             int src_stride,
                                             const uint16_t *ref_ptr,
                                             int ref_stride, int h,
                                             unsigned int *sse) {
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint16x8_t s0 = vld1q_u16(src_ptr);
    uint16x8_t s1 = vld1q_u16(src_ptr + 8);
    uint16x8_t r0 = vld1q_u16(ref_ptr);
    uint16x8_t r1 = vld1q_u16(ref_ptr + 8);

    uint8x16_t s = vcombine_u8(vmovn_u16(s0), vmovn_u16(s1));
    uint8x16_t r = vcombine_u8(vmovn_u16(r0), vmovn_u16(r1));

    uint8x16_t diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, diff, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  *sse = horizontal_add_u32x4(sse_u32);
  return *sse;
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE uint32_t highbd_mse8_8xh_neon(const uint16_t *src_ptr,
                                            int src_stride,
                                            const uint16_t *ref_ptr,
                                            int ref_stride, int h,
                                            unsigned int *sse) {
  return highbd_mse_wxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 8, h,
                             sse);
}

static INLINE uint32_t highbd_mse8_16xh_neon(const uint16_t *src_ptr,
                                             int src_stride,
                                             const uint16_t *ref_ptr,
                                             int ref_stride, int h,
                                             unsigned int *sse) {
  return highbd_mse_wxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 16, h,
                             sse);
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

#define HIGHBD_MSE_WXH_NEON(w, h)                                       \
  uint32_t aom_highbd_8_mse##w##x##h##_neon(                            \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,   \
      int ref_stride, uint32_t *sse) {                                  \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                       \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                       \
    highbd_mse8_##w##xh_neon(src, src_stride, ref, ref_stride, h, sse); \
    return *sse;                                                        \
  }                                                                     \
                                                                        \
  uint32_t aom_highbd_10_mse##w##x##h##_neon(                           \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,   \
      int ref_stride, uint32_t *sse) {                                  \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                       \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                       \
    highbd_mse_wxh_neon(src, src_stride, ref, ref_stride, w, h, sse);   \
    *sse = ROUND_POWER_OF_TWO(*sse, 4);                                 \
    return *sse;                                                        \
  }                                                                     \
                                                                        \
  uint32_t aom_highbd_12_mse##w##x##h##_neon(                           \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,   \
      int ref_stride, uint32_t *sse) {                                  \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                       \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                       \
    highbd_mse_wxh_neon(src, src_stride, ref, ref_stride, w, h, sse);   \
    *sse = ROUND_POWER_OF_TWO(*sse, 8);                                 \
    return *sse;                                                        \
  }

HIGHBD_MSE_WXH_NEON(16, 16)
HIGHBD_MSE_WXH_NEON(16, 8)
HIGHBD_MSE_WXH_NEON(8, 16)
HIGHBD_MSE_WXH_NEON(8, 8)

#undef HIGHBD_MSE_WXH_NEON
