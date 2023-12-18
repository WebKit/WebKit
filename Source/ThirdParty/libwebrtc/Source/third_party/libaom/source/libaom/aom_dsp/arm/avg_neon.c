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
#include <assert.h>
#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

unsigned int aom_avg_4x4_neon(const uint8_t *p, int stride) {
  const uint8x8_t s0 = load_unaligned_u8(p, stride);
  const uint8x8_t s1 = load_unaligned_u8(p + 2 * stride, stride);

  const uint32_t sum = horizontal_add_u16x8(vaddl_u8(s0, s1));
  return (sum + (1 << 3)) >> 4;
}

unsigned int aom_avg_8x8_neon(const uint8_t *p, int stride) {
  uint8x8_t s0 = vld1_u8(p);
  p += stride;
  uint8x8_t s1 = vld1_u8(p);
  p += stride;
  uint16x8_t acc = vaddl_u8(s0, s1);

  int i = 0;
  do {
    const uint8x8_t si = vld1_u8(p);
    p += stride;
    acc = vaddw_u8(acc, si);
  } while (++i < 6);

  const uint32_t sum = horizontal_add_u16x8(acc);
  return (sum + (1 << 5)) >> 6;
}

void aom_avg_8x8_quad_neon(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  avg[0] = aom_avg_8x8_neon(s + y16_idx * p + x16_idx, p);
  avg[1] = aom_avg_8x8_neon(s + y16_idx * p + (x16_idx + 8), p);
  avg[2] = aom_avg_8x8_neon(s + (y16_idx + 8) * p + x16_idx, p);
  avg[3] = aom_avg_8x8_neon(s + (y16_idx + 8) * p + (x16_idx + 8), p);
}

int aom_satd_lp_neon(const int16_t *coeff, int length) {
  int16x8_t s0 = vld1q_s16(coeff);
  int16x8_t s1 = vld1q_s16(coeff + 8);

  int16x8_t abs0 = vabsq_s16(s0);
  int16x8_t abs1 = vabsq_s16(s1);

  int32x4_t acc0 = vpaddlq_s16(abs0);
  int32x4_t acc1 = vpaddlq_s16(abs1);

  length -= 16;
  coeff += 16;

  while (length != 0) {
    s0 = vld1q_s16(coeff);
    s1 = vld1q_s16(coeff + 8);

    abs0 = vabsq_s16(s0);
    abs1 = vabsq_s16(s1);

    acc0 = vpadalq_s16(acc0, abs0);
    acc1 = vpadalq_s16(acc1, abs1);

    length -= 16;
    coeff += 16;
  }

  int32x4_t accum = vaddq_s32(acc0, acc1);
  return horizontal_add_s32x4(accum);
}

void aom_int_pro_row_neon(int16_t *hbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  assert(width % 16 == 0);
  assert(height % 4 == 0);

  const int16x8_t neg_norm_factor = vdupq_n_s16(-norm_factor);
  uint16x8_t sum_lo[2], sum_hi[2];

  int w = 0;
  do {
    const uint8_t *r = ref + w;
    uint8x16_t r0 = vld1q_u8(r + 0 * ref_stride);
    uint8x16_t r1 = vld1q_u8(r + 1 * ref_stride);
    uint8x16_t r2 = vld1q_u8(r + 2 * ref_stride);
    uint8x16_t r3 = vld1q_u8(r + 3 * ref_stride);

    sum_lo[0] = vaddl_u8(vget_low_u8(r0), vget_low_u8(r1));
    sum_hi[0] = vaddl_u8(vget_high_u8(r0), vget_high_u8(r1));
    sum_lo[1] = vaddl_u8(vget_low_u8(r2), vget_low_u8(r3));
    sum_hi[1] = vaddl_u8(vget_high_u8(r2), vget_high_u8(r3));

    r += 4 * ref_stride;

    for (int h = height - 4; h != 0; h -= 4) {
      r0 = vld1q_u8(r + 0 * ref_stride);
      r1 = vld1q_u8(r + 1 * ref_stride);
      r2 = vld1q_u8(r + 2 * ref_stride);
      r3 = vld1q_u8(r + 3 * ref_stride);

      uint16x8_t tmp0_lo = vaddl_u8(vget_low_u8(r0), vget_low_u8(r1));
      uint16x8_t tmp0_hi = vaddl_u8(vget_high_u8(r0), vget_high_u8(r1));
      uint16x8_t tmp1_lo = vaddl_u8(vget_low_u8(r2), vget_low_u8(r3));
      uint16x8_t tmp1_hi = vaddl_u8(vget_high_u8(r2), vget_high_u8(r3));

      sum_lo[0] = vaddq_u16(sum_lo[0], tmp0_lo);
      sum_hi[0] = vaddq_u16(sum_hi[0], tmp0_hi);
      sum_lo[1] = vaddq_u16(sum_lo[1], tmp1_lo);
      sum_hi[1] = vaddq_u16(sum_hi[1], tmp1_hi);

      r += 4 * ref_stride;
    }

    sum_lo[0] = vaddq_u16(sum_lo[0], sum_lo[1]);
    sum_hi[0] = vaddq_u16(sum_hi[0], sum_hi[1]);

    const int16x8_t avg0 =
        vshlq_s16(vreinterpretq_s16_u16(sum_lo[0]), neg_norm_factor);
    const int16x8_t avg1 =
        vshlq_s16(vreinterpretq_s16_u16(sum_hi[0]), neg_norm_factor);

    vst1q_s16(hbuf + w, avg0);
    vst1q_s16(hbuf + w + 8, avg1);
    w += 16;
  } while (w < width);
}

void aom_int_pro_col_neon(int16_t *vbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  assert(width % 16 == 0);
  assert(height % 4 == 0);

  const int16x4_t neg_norm_factor = vdup_n_s16(-norm_factor);
  uint16x8_t sum[4];

  int h = 0;
  do {
    sum[0] = vpaddlq_u8(vld1q_u8(ref + 0 * ref_stride));
    sum[1] = vpaddlq_u8(vld1q_u8(ref + 1 * ref_stride));
    sum[2] = vpaddlq_u8(vld1q_u8(ref + 2 * ref_stride));
    sum[3] = vpaddlq_u8(vld1q_u8(ref + 3 * ref_stride));

    for (int w = 16; w < width; w += 16) {
      sum[0] = vpadalq_u8(sum[0], vld1q_u8(ref + 0 * ref_stride + w));
      sum[1] = vpadalq_u8(sum[1], vld1q_u8(ref + 1 * ref_stride + w));
      sum[2] = vpadalq_u8(sum[2], vld1q_u8(ref + 2 * ref_stride + w));
      sum[3] = vpadalq_u8(sum[3], vld1q_u8(ref + 3 * ref_stride + w));
    }

    uint16x4_t sum_4d = vmovn_u32(horizontal_add_4d_u16x8(sum));
    int16x4_t avg = vshl_s16(vreinterpret_s16_u16(sum_4d), neg_norm_factor);
    vst1_s16(vbuf + h, avg);

    ref += 4 * ref_stride;
    h += 4;
  } while (h < height);
}

// coeff: 20 bits, dynamic range [-524287, 524287].
// length: value range {16, 32, 64, 128, 256, 512, 1024}.
int aom_satd_neon(const tran_low_t *coeff, int length) {
  const int32x4_t zero = vdupq_n_s32(0);

  int32x4_t s0 = vld1q_s32(&coeff[0]);
  int32x4_t s1 = vld1q_s32(&coeff[4]);
  int32x4_t s2 = vld1q_s32(&coeff[8]);
  int32x4_t s3 = vld1q_s32(&coeff[12]);

  int32x4_t accum0 = vabsq_s32(s0);
  int32x4_t accum1 = vabsq_s32(s2);
  accum0 = vabaq_s32(accum0, s1, zero);
  accum1 = vabaq_s32(accum1, s3, zero);

  length -= 16;
  coeff += 16;

  while (length != 0) {
    s0 = vld1q_s32(&coeff[0]);
    s1 = vld1q_s32(&coeff[4]);
    s2 = vld1q_s32(&coeff[8]);
    s3 = vld1q_s32(&coeff[12]);

    accum0 = vabaq_s32(accum0, s0, zero);
    accum1 = vabaq_s32(accum1, s1, zero);
    accum0 = vabaq_s32(accum0, s2, zero);
    accum1 = vabaq_s32(accum1, s3, zero);

    length -= 16;
    coeff += 16;
  }

  // satd: 30 bits, dynamic range [-524287 * 1024, 524287 * 1024]
  return horizontal_add_s32x4(vaddq_s32(accum0, accum1));
}

int aom_vector_var_neon(const int16_t *ref, const int16_t *src, int bwl) {
  assert(bwl >= 2 && bwl <= 5);
  int width = 4 << bwl;

  int16x8_t r = vld1q_s16(ref);
  int16x8_t s = vld1q_s16(src);

  // diff: dynamic range [-510, 510] 10 (signed) bits.
  int16x8_t diff = vsubq_s16(r, s);
  // v_mean: dynamic range 16 * diff -> [-8160, 8160], 14 (signed) bits.
  int16x8_t v_mean = diff;
  // v_sse: dynamic range 2 * 16 * diff^2 -> [0, 8,323,200], 24 (signed) bits.
  int32x4_t v_sse[2];
  v_sse[0] = vmull_s16(vget_low_s16(diff), vget_low_s16(diff));
  v_sse[1] = vmull_s16(vget_high_s16(diff), vget_high_s16(diff));

  ref += 8;
  src += 8;
  width -= 8;

  do {
    r = vld1q_s16(ref);
    s = vld1q_s16(src);

    diff = vsubq_s16(r, s);
    v_mean = vaddq_s16(v_mean, diff);

    v_sse[0] = vmlal_s16(v_sse[0], vget_low_s16(diff), vget_low_s16(diff));
    v_sse[1] = vmlal_s16(v_sse[1], vget_high_s16(diff), vget_high_s16(diff));

    ref += 8;
    src += 8;
    width -= 8;
  } while (width != 0);

  // Dynamic range [0, 65280], 16 (unsigned) bits.
  const uint32_t mean_abs = abs(horizontal_add_s16x8(v_mean));
  const int32_t sse = horizontal_add_s32x4(vaddq_s32(v_sse[0], v_sse[1]));

  // (mean_abs * mean_abs): dynamic range 32 (unsigned) bits.
  return sse - ((mean_abs * mean_abs) >> (bwl + 2));
}

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

#if AOM_ARCH_AARCH64
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
