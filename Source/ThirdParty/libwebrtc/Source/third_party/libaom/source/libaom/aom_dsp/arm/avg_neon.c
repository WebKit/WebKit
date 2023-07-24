/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

#if !defined(__aarch64__)
static INLINE uint32x2_t horizontal_add_u16x8_v(const uint16x8_t a) {
  const uint32x4_t b = vpaddlq_u16(a);
  const uint64x2_t c = vpaddlq_u32(b);
  return vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                  vreinterpret_u32_u64(vget_high_u64(c)));
}
#endif

unsigned int aom_avg_4x4_neon(const uint8_t *a, int a_stride) {
  const uint8x16_t b = load_unaligned_u8q(a, a_stride);
  const uint16x8_t c = vaddl_u8(vget_low_u8(b), vget_high_u8(b));
#if defined(__aarch64__)
  const uint32_t d = vaddlvq_u16(c);
  return (d + 8) >> 4;
#else
  const uint32x2_t d = horizontal_add_u16x8_v(c);
  return vget_lane_u32(vrshr_n_u32(d, 4), 0);
#endif
}

unsigned int aom_avg_8x8_neon(const uint8_t *a, int a_stride) {
  uint16x8_t sum;
  uint8x8_t b = vld1_u8(a);
  a += a_stride;
  uint8x8_t c = vld1_u8(a);
  a += a_stride;
  sum = vaddl_u8(b, c);

  for (int i = 0; i < 6; ++i) {
    const uint8x8_t e = vld1_u8(a);
    a += a_stride;
    sum = vaddw_u8(sum, e);
  }

#if defined(__aarch64__)
  const uint32_t d = vaddlvq_u16(sum);
  return (d + 32) >> 6;
#else
  const uint32x2_t d = horizontal_add_u16x8_v(sum);
  return vget_lane_u32(vrshr_n_u32(d, 6), 0);
#endif
}

void aom_avg_8x8_quad_neon(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  for (int k = 0; k < 4; k++) {
    const int x8_idx = x16_idx + ((k & 1) << 3);
    const int y8_idx = y16_idx + ((k >> 1) << 3);
    const uint8_t *s_tmp = s + y8_idx * p + x8_idx;
    avg[k] = aom_avg_8x8_neon(s_tmp, p);
  }
}

int aom_satd_lp_neon(const int16_t *coeff, int length) {
  const int16x4_t zero = vdup_n_s16(0);
  int32x4_t accum = vdupq_n_s32(0);

  do {
    const int16x8_t src0 = vld1q_s16(coeff);
    const int16x8_t src8 = vld1q_s16(coeff + 8);
    accum = vabal_s16(accum, vget_low_s16(src0), zero);
    accum = vabal_s16(accum, vget_high_s16(src0), zero);
    accum = vabal_s16(accum, vget_low_s16(src8), zero);
    accum = vabal_s16(accum, vget_high_s16(src8), zero);
    length -= 16;
    coeff += 16;
  } while (length != 0);

  return horizontal_add_s32x4(accum);
}

void aom_int_pro_row_neon(int16_t *hbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  const uint8_t *idx = ref;
  const uint16x8_t zero = vdupq_n_u16(0);
  const int16x8_t neg_norm_factor = vdupq_n_s16(-norm_factor);

  for (int wd = 0; wd < width; wd += 16) {
    uint16x8_t vec0 = zero;
    uint16x8_t vec1 = zero;
    idx = ref + wd;
    for (int ht = 0; ht < height; ++ht) {
      const uint8x16_t tmp = vld1q_u8(idx);
      idx += ref_stride;
      vec0 = vaddw_u8(vec0, vget_low_u8(tmp));
      vec1 = vaddw_u8(vec1, vget_high_u8(tmp));
    }

    const int16x8_t result0 =
        vshlq_s16(vreinterpretq_s16_u16(vec0), neg_norm_factor);
    const int16x8_t result1 =
        vshlq_s16(vreinterpretq_s16_u16(vec1), neg_norm_factor);

    vst1q_s16(hbuf + wd, result0);
    vst1q_s16(hbuf + wd + 8, result1);
  }
}

void aom_int_pro_col_neon(int16_t *vbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  for (int ht = 0; ht < height; ++ht) {
    uint16x8_t sum = vdupq_n_u16(0);
    for (int wd = 0; wd < width; wd += 16) {
      const uint8x16_t vec = vld1q_u8(ref + wd);
      sum = vaddq_u16(sum, vpaddlq_u8(vec));
    }

#if defined(__aarch64__)
    vbuf[ht] = ((int16_t)vaddvq_u16(sum)) >> norm_factor;
#else
    const uint32x4_t a = vpaddlq_u16(sum);
    const uint64x2_t b = vpaddlq_u32(a);
    const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                  vreinterpret_u32_u64(vget_high_u64(b)));
    vbuf[ht] = ((int16_t)vget_lane_u32(c, 0)) >> norm_factor;
#endif
    ref += ref_stride;
  }
}

// coeff: 16 bits, dynamic range [-32640, 32640].
// length: value range {16, 64, 256, 1024}.
int aom_satd_neon(const tran_low_t *coeff, int length) {
  const int32x4_t zero = vdupq_n_s32(0);
  int32x4_t accum = zero;
  do {
    const int32x4_t src0 = vld1q_s32(&coeff[0]);
    const int32x4_t src8 = vld1q_s32(&coeff[4]);
    const int32x4_t src16 = vld1q_s32(&coeff[8]);
    const int32x4_t src24 = vld1q_s32(&coeff[12]);
    accum = vabaq_s32(accum, src0, zero);
    accum = vabaq_s32(accum, src8, zero);
    accum = vabaq_s32(accum, src16, zero);
    accum = vabaq_s32(accum, src24, zero);
    length -= 16;
    coeff += 16;
  } while (length != 0);

  // satd: 26 bits, dynamic range [-32640 * 1024, 32640 * 1024]
  return horizontal_add_s32x4(accum);
}

int aom_vector_var_neon(const int16_t *ref, const int16_t *src, int bwl) {
  int32x4_t v_mean = vdupq_n_s32(0);
  int32x4_t v_sse = v_mean;
  int16x8_t v_ref, v_src;
  int16x4_t v_low;

  int i, width = 4 << bwl;
  for (i = 0; i < width; i += 8) {
    v_ref = vld1q_s16(&ref[i]);
    v_src = vld1q_s16(&src[i]);
    const int16x8_t diff = vsubq_s16(v_ref, v_src);
    // diff: dynamic range [-510, 510], 10 bits.
    v_mean = vpadalq_s16(v_mean, diff);
    v_low = vget_low_s16(diff);
    v_sse = vmlal_s16(v_sse, v_low, v_low);
#if defined(__aarch64__)
    v_sse = vmlal_high_s16(v_sse, diff, diff);
#else
    const int16x4_t v_high = vget_high_s16(diff);
    v_sse = vmlal_s16(v_sse, v_high, v_high);
#endif
  }
  const int mean = horizontal_add_s32x4(v_mean);
  const int sse = horizontal_add_s32x4(v_sse);
  const unsigned int mean_abs = mean >= 0 ? mean : -mean;
  // (mean * mean): dynamic range 31 bits.
  const int var = sse - ((mean_abs * mean_abs) >> (bwl + 2));
  return var;
}

#if CONFIG_AV1_HIGHBITDEPTH
unsigned int aom_highbd_avg_4x4_neon(const uint8_t *s, int p) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(s);
  const uint16x4_t r0 = vld1_u16(src);
  src += p;
  uint16x4_t r1, r2, r3;
  r1 = vld1_u16(src);
  src += p;
  r2 = vld1_u16(src);
  src += p;
  r3 = vld1_u16(src);
  const uint16x4_t s1 = vadd_u16(r0, r1);
  const uint16x4_t s2 = vadd_u16(r2, r3);
  const uint16x4_t s3 = vadd_u16(s1, s2);
#if defined(__aarch64__)
  return (vaddv_u16(s3) + 8) >> 4;
#else
  const uint16x4_t h1 = vpadd_u16(s3, s3);
  const uint16x4_t h2 = vpadd_u16(h1, h1);
  const uint16x4_t res = vrshr_n_u16(h2, 4);
  return vget_lane_u16(res, 0);
#endif
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_minmax_8x8_neon(const uint8_t *a, int a_stride, const uint8_t *b,
                         int b_stride, int *min, int *max) {
  // Load and concatenate.
  const uint8x16_t a01 = load_u8_8x2(a + 0 * a_stride, a_stride);
  const uint8x16_t a23 = load_u8_8x2(a + 2 * a_stride, a_stride);
  const uint8x16_t a45 = load_u8_8x2(a + 4 * a_stride, a_stride);
  const uint8x16_t a67 = load_u8_8x2(a + 6 * a_stride, a_stride);

  const uint8x16_t b01 = load_u8_8x2(b + 0 * b_stride, b_stride);
  const uint8x16_t b23 = load_u8_8x2(b + 2 * b_stride, b_stride);
  const uint8x16_t b45 = load_u8_8x2(b + 4 * b_stride, b_stride);
  const uint8x16_t b67 = load_u8_8x2(b + 6 * b_stride, b_stride);

  // Absolute difference.
  const uint8x16_t ab01_diff = vabdq_u8(a01, b01);
  const uint8x16_t ab23_diff = vabdq_u8(a23, b23);
  const uint8x16_t ab45_diff = vabdq_u8(a45, b45);
  const uint8x16_t ab67_diff = vabdq_u8(a67, b67);

  // Max values between the Q vectors.
  const uint8x16_t ab0123_max = vmaxq_u8(ab01_diff, ab23_diff);
  const uint8x16_t ab4567_max = vmaxq_u8(ab45_diff, ab67_diff);
  const uint8x16_t ab0123_min = vminq_u8(ab01_diff, ab23_diff);
  const uint8x16_t ab4567_min = vminq_u8(ab45_diff, ab67_diff);

  const uint8x16_t ab07_max = vmaxq_u8(ab0123_max, ab4567_max);
  const uint8x16_t ab07_min = vminq_u8(ab0123_min, ab4567_min);

#if defined(__aarch64__)
  *min = *max = 0;  // Clear high bits
  *((uint8_t *)max) = vmaxvq_u8(ab07_max);
  *((uint8_t *)min) = vminvq_u8(ab07_min);
#else
  // Split into 64-bit vectors and execute pairwise min/max.
  uint8x8_t ab_max = vmax_u8(vget_high_u8(ab07_max), vget_low_u8(ab07_max));
  uint8x8_t ab_min = vmin_u8(vget_high_u8(ab07_min), vget_low_u8(ab07_min));

  // Enough runs of vpmax/min propagate the max/min values to every position.
  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  *min = *max = 0;  // Clear high bits
  // Store directly to avoid costly neon->gpr transfer.
  vst1_lane_u8((uint8_t *)max, ab_max, 0);
  vst1_lane_u8((uint8_t *)min, ab_min, 0);
#endif
}
