/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/convolve_neon.h"

static INLINE int16x4_t convolve8_4x4(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  return sum;
}

#if !defined(__aarch64__)
static INLINE uint8x8_t convolve8_x_4x1(const int16x4_t s0, const int16x4_t s1,
                                        const int16x4_t s2, const int16x4_t s3,
                                        const int16x4_t s4, const int16x4_t s5,
                                        const int16x4_t s6, const int16x4_t s7,
                                        const int16x8_t filter,
                                        const int16x4_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x4_t sum = horiz_const;

  sum = vmla_lane_s16(sum, s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(vcombine_s16(sum, vdup_n_s16(0)), FILTER_BITS - 1);
}
#endif  // !defined(__arch64__)

#if defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int32x4_t convolve12_4_usdot(uint8x16_t samples,
                                           const int8x16_t filters,
                                           const uint8x16x3_t permute_tbl,
                                           const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum;

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  /* First 4 output values. */
  sum = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum = vusdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vusdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  return sum;
}

static INLINE int16x8_t convolve12_8_usdot(uint8x16_t samples0,
                                           uint8x16_t samples1,
                                           const int8x16_t filters,
                                           const uint8x16x3_t permute_tbl,
                                           const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[4];
  int32x4_t sum[2];

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples0, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples0, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples0, permute_tbl.val[2]);
  /* {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 } */
  permuted_samples[3] = vqtbl1q_u8(samples1, permute_tbl.val[2]);

  /* First 4 output values. */
  sum[0] = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  /* Second 4 output values. */
  sum[1] = vusdotq_laneq_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  /* Narrow and re-pack. */
  return vcombine_s16(vqrshrn_n_s32(sum[0], FILTER_BITS),
                      vqrshrn_n_s32(sum[1], FILTER_BITS));
}

#elif defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE int16x4_t convolve12_horiz_4_sdot(
    uint8x16_t samples, const int8x16_t filters, const int32x4_t correction,
    const uint8x16_t range_limit, const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum;

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum = vdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  /* Narrow and re-pack. */
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_horiz_8_sdot(
    uint8x16_t samples0, uint8x16_t samples1, const int8x16_t filters,
    const int32x4_t correction, const uint8x16_t range_limit,
    const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples[2], permuted_samples[4];
  int32x4_t sum[2];

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples[0] = vreinterpretq_s8_u8(vsubq_u8(samples0, range_limit));
  clamped_samples[1] = vreinterpretq_s8_u8(vsubq_u8(samples1, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[2]);
  /* {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 } */
  permuted_samples[3] = vqtbl1q_s8(clamped_samples[1], permute_tbl.val[2]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum[0] = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  /* Second 4 output values. */
  sum[1] = vdotq_laneq_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  /* Narrow and re-pack. */
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS),
                      vshrn_n_s32(sum[1], ROUND0_BITS));
}

static INLINE int32x4_t convolve12_4_sdot(uint8x16_t samples,
                                          const int8x16_t filters,
                                          const int32x4_t correction,
                                          const uint8x16_t range_limit,
                                          const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum;

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum = vdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  return sum;
}

static INLINE int16x8_t convolve12_8_sdot(uint8x16_t samples0,
                                          uint8x16_t samples1,
                                          const int8x16_t filters,
                                          const int32x4_t correction,
                                          const uint8x16_t range_limit,
                                          const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples[2], permuted_samples[4];
  int32x4_t sum[2];

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples[0] = vreinterpretq_s8_u8(vsubq_u8(samples0, range_limit));
  clamped_samples[1] = vreinterpretq_s8_u8(vsubq_u8(samples1, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[2]);
  /* {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 } */
  permuted_samples[3] = vqtbl1q_s8(clamped_samples[1], permute_tbl.val[2]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum[0] = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  /* Second 4 output values. */
  sum[1] = vdotq_laneq_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  /* Narrow and re-pack. */
  return vcombine_s16(vqrshrn_n_s32(sum[0], FILTER_BITS),
                      vqrshrn_n_s32(sum[1], FILTER_BITS));
}

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE uint8x8_t convolve8_vert_8x4(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE int16x4_t convolve8_vert_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);
  int32x4_t sum;

  sum = vmull_lane_s16(s0, y_filter_lo, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_hi, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_hi, 3);

  return vqrshrn_n_s32(sum, 2 * FILTER_BITS - ROUND0_BITS);
}

static INLINE uint8x8_t
convolve8_vert_8_s32(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                     const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                     const int16x8_t s6, const int16x8_t s7,
                     const int16x8_t y_filter, const int16x8_t sub_const) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);
  int32x4_t sum0, sum1;
  int16x8_t res;

  sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_hi, 3);

  sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_hi, 3);

  res = vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                     vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

#if defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

void convolve_x_sr_12tap_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const int16_t *x_filter_ptr) {
  const int16x8_t filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int16x8_t filter_8_15 = vcombine_s16(filter_8_11, vdup_n_s16(0));
  const int8x16_t filter =
      vcombine_s8(vmovn_s16(filter_0_7), vmovn_s16(filter_8_15));

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(filter_0_7, 5) == 128) {
    uint8x8_t d0;

    // Undo the horizontal offset in the calling function.
    src += 5;

    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j += 8) {
        d0 = vld1_u8(src + i * src_stride + j);
        if (w == 2) {
          store_u8_2x1(dst + i * dst_stride, d0, 0);
        } else if (w == 4) {
          store_u8_4x1(dst + i * dst_stride, d0, 0);
        } else {
          vst1_u8(dst + i * dst_stride + j, d0);
        }
      }
    }
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // This shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding
    // right shift by FILTER_BITS - instead of a first rounding right shift by
    // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
    // ROUND0_BITS.
    const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

    if (w <= 4) {
      uint8x16_t s0, s1, s2, s3;
      int32x4_t d0, d1, d2, d3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23;

      do {
        load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

        d0 = convolve12_4_usdot(s0, filter, permute_tbl, horiz_const);
        d1 = convolve12_4_usdot(s1, filter, permute_tbl, horiz_const);
        d2 = convolve12_4_usdot(s2, filter, permute_tbl, horiz_const);
        d3 = convolve12_4_usdot(s3, filter, permute_tbl, horiz_const);

        t01 = vcombine_s16(vqrshrn_n_s32(d0, FILTER_BITS),
                           vqrshrn_n_s32(d1, FILTER_BITS));
        t23 = vcombine_s16(vqrshrn_n_s32(d2, FILTER_BITS),
                           vqrshrn_n_s32(d3, FILTER_BITS));

        d01 = vqmovun_s16(t01);
        d23 = vqmovun_s16(t23);

        if (w == 2) {
          store_u8_2x1(dst + 0 * dst_stride, d01, 0);
          store_u8_2x1(dst + 1 * dst_stride, d01, 2);
          if (h != 2) {
            store_u8_2x1(dst + 2 * dst_stride, d23, 0);
            store_u8_2x1(dst + 3 * dst_stride, d23, 2);
          }
        } else {
          store_u8_4x1(dst + 0 * dst_stride, d01, 0);
          store_u8_4x1(dst + 1 * dst_stride, d01, 1);
          if (h != 2) {
            store_u8_4x1(dst + 2 * dst_stride, d23, 0);
            store_u8_4x1(dst + 3 * dst_stride, d23, 1);
          }
        }

        dst += 4 * dst_stride;
        src += 4 * src_stride;
        h -= 4;
      } while (h > 0);
    } else {
      uint8x16_t s0, s1, s2, s3, s4, s5, s6, s7;
      int16x8_t d0, d1, d2, d3;
      uint8x8_t dd0, dd1, dd2, dd3;

      do {
        const uint8_t *s = src;
        uint8_t *d = dst;
        int width = w;

        do {
          load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);
          load_u8_16x4(s + 4, src_stride, &s4, &s5, &s6, &s7);

          d0 = convolve12_8_usdot(s0, s4, filter, permute_tbl, horiz_const);
          d1 = convolve12_8_usdot(s1, s5, filter, permute_tbl, horiz_const);
          d2 = convolve12_8_usdot(s2, s6, filter, permute_tbl, horiz_const);
          d3 = convolve12_8_usdot(s3, s7, filter, permute_tbl, horiz_const);

          dd0 = vqmovun_s16(d0);
          dd1 = vqmovun_s16(d1);
          dd2 = vqmovun_s16(d2);
          dd3 = vqmovun_s16(d3);

          store_u8_8x2(d + 0 * dst_stride, dst_stride, dd0, dd1);
          if (h != 2) {
            store_u8_8x2(d + 2 * dst_stride, dst_stride, dd2, dd3);
          }

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 4 * src_stride;
        dst += 4 * dst_stride;
        h -= 4;
      } while (h > 0);
    }
  }
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  (void)conv_params;
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  src -= horiz_offset;

  if (filter_params_x->taps > 8) {
    convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             x_filter_ptr);
    return;
  }

  // Filter values are even, so downshift by 1 to reduce intermediate precision
  // requirements.
  const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we halved the filter values.
  const int32x4_t horiz_const = vdupq_n_s32(1 << ((ROUND0_BITS - 1) - 1));

  if (w <= 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int32x4_t t0, t1, t2, t3;
    int16x8_t t01, t23;
    uint8x8_t d01, d23;

    do {
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_usdot(s0, x_filter, permute_tbl, horiz_const);
      t1 = convolve8_4_usdot(s1, x_filter, permute_tbl, horiz_const);
      t2 = convolve8_4_usdot(s2, x_filter, permute_tbl, horiz_const);
      t3 = convolve8_4_usdot(s3, x_filter, permute_tbl, horiz_const);

      t01 = vcombine_s16(vmovn_s32(t0), vmovn_s32(t1));
      t23 = vcombine_s16(vmovn_s32(t2), vmovn_s32(t3));

      // We halved the convolution filter values so - 1 from the right shift.
      d01 = vqrshrun_n_s16(t01, FILTER_BITS - 1);
      d23 = vqrshrun_n_s16(t23, FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst + 3 * dst_stride, d23, 1);
        }
      }

      h -= 4;
      src += 4 * src_stride;
      dst += 4 * dst_stride;
    } while (h > 0);

  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int16x8_t t0, t1, t2, t3;
    uint8x8_t d0, d1, d2, d3;

    do {
      int width = w;
      const uint8_t *s = src;
      uint8_t *d = dst;

      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        t0 = convolve8_x_8_usdot(s0, x_filter, permute_tbl, horiz_const);
        t1 = convolve8_x_8_usdot(s1, x_filter, permute_tbl, horiz_const);
        t2 = convolve8_x_8_usdot(s2, x_filter, permute_tbl, horiz_const);
        t3 = convolve8_x_8_usdot(s3, x_filter, permute_tbl, horiz_const);

        // We halved the convolution filter values so - 1 from the right shift.
        d0 = vqrshrun_n_s16(t0, FILTER_BITS - 1);
        d1 = vqrshrun_n_s16(t1, FILTER_BITS - 1);
        d2 = vqrshrun_n_s16(t2, FILTER_BITS - 1);
        d3 = vqrshrun_n_s16(t3, FILTER_BITS - 1);

        vst1_u8(d + 0 * dst_stride, d0);
        vst1_u8(d + 1 * dst_stride, d1);
        if (h != 2) {
          vst1_u8(d + 2 * dst_stride, d2);
          vst1_u8(d + 3 * dst_stride, d3);
        }

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

#elif defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

void convolve_x_sr_12tap_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const int16_t *x_filter_ptr) {
  const int16x8_t filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int16x8_t filter_8_15 = vcombine_s16(filter_8_11, vdup_n_s16(0));
  const int8x16_t filter =
      vcombine_s8(vmovn_s16(filter_0_7), vmovn_s16(filter_8_15));

  const int32x4_t correct_tmp =
      vaddq_s32(vpaddlq_s16(vshlq_n_s16(filter_0_7, 7)),
                vpaddlq_s16(vshlq_n_s16(filter_8_15, 7)));
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding
  // right shift by FILTER_BITS - instead of a first rounding right shift by
  // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
  // ROUND0_BITS.
  int32x4_t correction =
      vdupq_n_s32(vaddvq_s32(correct_tmp) + (1 << (ROUND0_BITS - 1)));
  const uint8x16_t range_limit = vdupq_n_u8(128);
  const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(filter_0_7, 5) == 128) {
    uint8x8_t d0;

    // Undo the horizontal offset in the calling function.
    src += 5;

    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j += 8) {
        d0 = vld1_u8(src + i * src_stride + j);
        if (w == 2) {
          store_u8_2x1(dst + i * dst_stride, d0, 0);
        } else if (w == 4) {
          store_u8_4x1(dst + i * dst_stride, d0, 0);
        } else {
          vst1_u8(dst + i * dst_stride + j, d0);
        }
      }
    }
  } else {
    if (w <= 4) {
      uint8x16_t s0, s1, s2, s3;
      int32x4_t d0, d1, d2, d3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23;

      do {
        load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

        d0 =
            convolve12_4_sdot(s0, filter, correction, range_limit, permute_tbl);
        d1 =
            convolve12_4_sdot(s1, filter, correction, range_limit, permute_tbl);
        d2 =
            convolve12_4_sdot(s2, filter, correction, range_limit, permute_tbl);
        d3 =
            convolve12_4_sdot(s3, filter, correction, range_limit, permute_tbl);

        t01 = vcombine_s16(vqrshrn_n_s32(d0, FILTER_BITS),
                           vqrshrn_n_s32(d1, FILTER_BITS));
        t23 = vcombine_s16(vqrshrn_n_s32(d2, FILTER_BITS),
                           vqrshrn_n_s32(d3, FILTER_BITS));

        d01 = vqmovun_s16(t01);
        d23 = vqmovun_s16(t23);

        if (w == 2) {
          store_u8_2x1(dst + 0 * dst_stride, d01, 0);
          store_u8_2x1(dst + 1 * dst_stride, d01, 2);
          if (h != 2) {
            store_u8_2x1(dst + 2 * dst_stride, d23, 0);
            store_u8_2x1(dst + 3 * dst_stride, d23, 2);
          }
        } else {
          store_u8_4x1(dst + 0 * dst_stride, d01, 0);
          store_u8_4x1(dst + 1 * dst_stride, d01, 1);
          if (h != 2) {
            store_u8_4x1(dst + 2 * dst_stride, d23, 0);
            store_u8_4x1(dst + 3 * dst_stride, d23, 1);
          }
        }

        dst += 4 * dst_stride;
        src += 4 * src_stride;
        h -= 4;
      } while (h > 0);
    } else {
      uint8x16_t s0, s1, s2, s3, s4, s5, s6, s7;
      int16x8_t d0, d1, d2, d3;
      uint8x8_t dd0, dd1, dd2, dd3;

      do {
        const uint8_t *s = src;
        uint8_t *d = dst;
        int width = w;

        do {
          load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);
          load_u8_16x4(s + 4, src_stride, &s4, &s5, &s6, &s7);

          d0 = convolve12_8_sdot(s0, s4, filter, correction, range_limit,
                                 permute_tbl);
          d1 = convolve12_8_sdot(s1, s5, filter, correction, range_limit,
                                 permute_tbl);
          d2 = convolve12_8_sdot(s2, s6, filter, correction, range_limit,
                                 permute_tbl);
          d3 = convolve12_8_sdot(s3, s7, filter, correction, range_limit,
                                 permute_tbl);

          dd0 = vqmovun_s16(d0);
          dd1 = vqmovun_s16(d1);
          dd2 = vqmovun_s16(d2);
          dd3 = vqmovun_s16(d3);

          store_u8_8x2(d + 0 * dst_stride, dst_stride, dd0, dd1);
          if (h != 2) {
            store_u8_8x2(d + 2 * dst_stride, dst_stride, dd2, dd3);
          }

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 4 * src_stride;
        dst += 4 * dst_stride;
        h -= 4;
      } while (h > 0);
    }
  }
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  (void)conv_params;
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  src -= horiz_offset;

  if (filter_params_x->taps > 8) {
    convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             x_filter_ptr);
    return;
  }

  // Filter values are even, so downshift by 1 to reduce intermediate precision
  // requirements.
  const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // Dot product constants.
  const int16x8_t correct_tmp = vshll_n_s8(x_filter, 7);
  // This shim of (1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we halved the filter values.
  const int32x4_t correction =
      vdupq_n_s32(vaddlvq_s16(correct_tmp) + (1 << ((ROUND0_BITS - 1) - 1)));
  const uint8x16_t range_limit = vdupq_n_u8(128);

  if (w <= 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int32x4_t t0, t1, t2, t3;
    int16x8_t t01, t23;
    uint8x8_t d01, d23;

    do {
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_sdot(s0, x_filter, correction, range_limit, permute_tbl);
      t1 = convolve8_4_sdot(s1, x_filter, correction, range_limit, permute_tbl);
      t2 = convolve8_4_sdot(s2, x_filter, correction, range_limit, permute_tbl);
      t3 = convolve8_4_sdot(s3, x_filter, correction, range_limit, permute_tbl);

      t01 = vcombine_s16(vmovn_s32(t0), vmovn_s32(t1));
      t23 = vcombine_s16(vmovn_s32(t2), vmovn_s32(t3));

      // We halved the convolution filter values so - 1 from the right shift.
      d01 = vqrshrun_n_s16(t01, FILTER_BITS - 1);
      d23 = vqrshrun_n_s16(t23, FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst + 3 * dst_stride, d23, 1);
        }
      }

      h -= 4;
      src += 4 * src_stride;
      dst += 4 * dst_stride;
    } while (h > 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int16x8_t t0, t1, t2, t3;
    uint8x8_t d0, d1, d2, d3;

    do {
      int width = w;
      const uint8_t *s = src;
      uint8_t *d = dst;

      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        t0 = convolve8_x_8_sdot(s0, x_filter, correction, range_limit,
                                permute_tbl);
        t1 = convolve8_x_8_sdot(s1, x_filter, correction, range_limit,
                                permute_tbl);
        t2 = convolve8_x_8_sdot(s2, x_filter, correction, range_limit,
                                permute_tbl);
        t3 = convolve8_x_8_sdot(s3, x_filter, correction, range_limit,
                                permute_tbl);

        // We halved the convolution filter values so - 1 from the right shift.
        d0 = vqrshrun_n_s16(t0, FILTER_BITS - 1);
        d1 = vqrshrun_n_s16(t1, FILTER_BITS - 1);
        d2 = vqrshrun_n_s16(t2, FILTER_BITS - 1);
        d3 = vqrshrun_n_s16(t3, FILTER_BITS - 1);

        vst1_u8(d + 0 * dst_stride, d0);
        vst1_u8(d + 1 * dst_stride, d1);
        if (h != 2) {
          vst1_u8(d + 2 * dst_stride, d2);
          vst1_u8(d + 3 * dst_stride, d3);
        }

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

#else  // !(defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD))

static INLINE uint8x8_t
convolve8_horiz_8x8(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                    const int16x8_t s6, const int16x8_t s7,
                    const int16x8_t filter, const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x8_t sum = horiz_const;

  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE int16x4_t convolve12_x_4x4_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
    const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);
  int32x4_t sum = horiz_const;

  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vqrshrn_n_s32(sum, FILTER_BITS);
}

// 4 column per iteration filtering for 12-tap convolve_x_sr.
// Processes one row at a time.
static INLINE void x_filter_12tap_w4_single_row(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    do {
      uint8x8_t dd0;
      uint8x16_t t0;
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, d0;
      int16x8_t tt0, tt1;

      t0 = vld1q_u8(s);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      s0 = vget_low_s16(tt0);
      s4 = vget_high_s16(tt0);
      s8 = vget_low_s16(tt1);
      s12 = vget_high_s16(tt1);

      s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      d0 = convolve12_x_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, x_filter_0_7, x_filter_8_11, horiz_const);

      dd0 = vqmovun_s16(vcombine_s16(d0, vdup_n_s16(0)));

      if (w == 2) {
        store_u8_2x1(d, dd0, 0);
      } else {
        store_u8_4x1(d, dd0, 0);
      }

      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);

    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

static INLINE void convolve_x_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            const int dst_stride, int w, int h,
                                            const int16_t *x_filter_ptr) {
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);

#if defined(__aarch64__)
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

  do {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint8x8_t t0, t1, t2, t3;

    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      int16x4_t s11, s12, s13, s14, d0, d1, d2, d3;
      int16x8_t d01, d23;
      uint8x8_t dd01, dd23;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve12_x_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, x_filter_0_7, x_filter_8_11, horiz_const);
      d1 = convolve12_x_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                s12, x_filter_0_7, x_filter_8_11, horiz_const);
      d2 = convolve12_x_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                s13, x_filter_0_7, x_filter_8_11, horiz_const);
      d3 = convolve12_x_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                                s14, x_filter_0_7, x_filter_8_11, horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      d01 = vcombine_s16(d0, d1);
      d23 = vcombine_s16(d2, d3);

      dd01 = vqmovun_s16(d01);
      dd23 = vqmovun_s16(d23);

      if (w == 2) {
        store_u8_2x1(d + 0 * dst_stride, dd01, 0);
        store_u8_2x1(d + 1 * dst_stride, dd01, 2);
        if (h != 2) {
          store_u8_2x1(d + 2 * dst_stride, dd23, 0);
          store_u8_2x1(d + 3 * dst_stride, dd23, 2);
        }
      } else {
        store_u8_4x1(d + 0 * dst_stride, dd01, 0);
        store_u8_4x1(d + 1 * dst_stride, dd01, 1);
        if (h != 2) {
          store_u8_4x1(d + 2 * dst_stride, dd23, 0);
          store_u8_4x1(d + 3 * dst_stride, dd23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);

    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h >= 4);

  if (h > 0) {
    x_filter_12tap_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w, h,
                                 x_filter_0_7, x_filter_8_11);
  }
#else   // !defined(__aarch64__)
  x_filter_12tap_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w, h,
                               x_filter_0_7, x_filter_8_11);
#endif  // defined(__aarch64__)
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  (void)conv_params;
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  src -= horiz_offset;

  if (filter_params_x->taps > 8) {
    convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             x_filter_ptr);
    return;
  }

  uint8x8_t t0;
#if defined(__aarch64__)
  uint8x8_t t1, t2, t3;
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we halved the filter values.
  const int16x8_t horiz_const = vdupq_n_s16(1 << ((ROUND0_BITS - 1) - 1));
#endif  // defined(__aarch64__)
  // Filter values are even so downshift by 1 to reduce precision requirements.
  const int16x8_t x_filter = vshrq_n_s16(vld1q_s16(x_filter_ptr), 1);

#if defined(__aarch64__)
  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t d01_temp, d23_temp;

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);

    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, x_filter);
      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, x_filter);
      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, x_filter);
      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, x_filter);

      d01_temp = vcombine_s16(d0, d1);
      d23_temp = vcombine_s16(d2, d3);

      d01_temp = vaddq_s16(d01_temp, horiz_const);
      d23_temp = vaddq_s16(d23_temp, horiz_const);

      // We halved the convolution filter values so - 1 from the right shift.
      d01 = vqrshrun_n_s16(d01_temp, FILTER_BITS - 1);
      d23 = vqrshrun_n_s16(d23_temp, FILTER_BITS - 1);

      transpose_u8_4x4(&d01, &d23);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst + 1 * dst_stride, d23, 0);
        store_u8_2x1(dst + 2 * dst_stride, d01, 2);
        store_u8_2x1(dst + 3 * dst_stride, d23, 2);
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst + 1 * dst_stride, d23, 0);
        store_u8_4x1(dst + 2 * dst_stride, d01, 1);
        store_u8_4x1(dst + 3 * dst_stride, d23, 1);
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w > 0);
  } else {
#endif  // defined(__aarch64__)
    int width;
    const uint8_t *s;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;

#if defined(__aarch64__)
    int16x8_t s8, s9, s10;
    uint8x8_t t4, t5, t6, t7;
#endif  // defined(__aarch64__)

    if (w <= 4) {
#if defined(__aarch64__)
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                 horiz_const);
        t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                 horiz_const);
        t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                 horiz_const);
        t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                 horiz_const);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        if (w == 4) {
          store_u8_4x1(dst + 0 * dst_stride, t0, 0);
          store_u8_4x1(dst + 1 * dst_stride, t1, 0);
          if (h > 4) {
            store_u8_4x1(dst + 2 * dst_stride, t2, 0);
            store_u8_4x1(dst + 3 * dst_stride, t3, 0);
            store_u8_4x1(dst + 4 * dst_stride, t0, 1);
            store_u8_4x1(dst + 5 * dst_stride, t1, 1);
            store_u8_4x1(dst + 6 * dst_stride, t2, 1);
            store_u8_4x1(dst + 7 * dst_stride, t3, 1);
          }
        } else if (w == 2) {
          store_u8_2x1(dst + 0 * dst_stride, t0, 0);
          store_u8_2x1(dst + 1 * dst_stride, t1, 0);
          if (h > 4) {
            store_u8_2x1(dst + 2 * dst_stride, t2, 0);
            store_u8_2x1(dst + 3 * dst_stride, t3, 0);
            store_u8_2x1(dst + 4 * dst_stride, t0, 2);
            store_u8_2x1(dst + 5 * dst_stride, t1, 2);
            store_u8_2x1(dst + 6 * dst_stride, t2, 2);
            store_u8_2x1(dst + 7 * dst_stride, t3, 2);
          }
        }

        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
#else   // !defined(__aarch64__)
    // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
    // rounding right shift by FILTER_BITS - instead of a first rounding right
    // shift by ROUND0_BITS, followed by second rounding right shift by
    // FILTER_BITS - ROUND0_BITS.
    // The outermost -1 is needed because we halved the filter values.
    const int16x4_t horiz_const = vdup_n_s16(1 << ((ROUND0_BITS - 1) - 1));
    int16x8_t tt0;
    int16x4_t x0, x1, x2, x3, x4, x5, x6, x7;

    do {
      t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      x0 = vget_low_s16(tt0);   // a0 a1 a2 a3
      x4 = vget_high_s16(tt0);  // a4 a5 a6 a7

      t0 = vld1_u8(src + 8);  // a8 a9 a10 a11 a12 a13 a14 a15
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      x7 = vget_low_s16(tt0);  // a8 a9 a10 a11

      x1 = vext_s16(x0, x4, 1);  // a1 a2 a3 a4
      x2 = vext_s16(x0, x4, 2);  // a2 a3 a4 a5
      x3 = vext_s16(x0, x4, 3);  // a3 a4 a5 a6
      x5 = vext_s16(x4, x7, 1);  // a5 a6 a7 a8
      x6 = vext_s16(x4, x7, 2);  // a6 a7 a8 a9
      x7 = vext_s16(x4, x7, 3);  // a7 a8 a9 a10

      src += src_stride;

      t0 = convolve8_x_4x1(x0, x1, x2, x3, x4, x5, x6, x7, x_filter,
                           horiz_const);

      if (w == 4) {
        store_u8_4x1(dst, t0, 0);
        dst += dst_stride;
      } else if (w == 2) {
        store_u8_2x1(dst, t0, 0);
        dst += dst_stride;
      }
      h -= 1;
    } while (h > 0);
#endif  // defined(__aarch64__)
    } else {
      uint8_t *d;
      int16x8_t s11;
#if defined(__aarch64__)
      int16x8_t s12, s13, s14;
      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        width = w;
        s = src + 7;
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                   horiz_const);
          t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                   horiz_const);
          t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                   horiz_const);
          t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                   horiz_const);
          t4 = convolve8_horiz_8x8(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                   horiz_const);
          t5 = convolve8_horiz_8x8(s5, s6, s7, s8, s9, s10, s11, s12, x_filter,
                                   horiz_const);
          t6 = convolve8_horiz_8x8(s6, s7, s8, s9, s10, s11, s12, s13, x_filter,
                                   horiz_const);
          t7 = convolve8_horiz_8x8(s7, s8, s9, s10, s11, s12, s13, s14,
                                   x_filter, horiz_const);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

          if (h != 2) {
            store_u8_8x8(d, dst_stride, t0, t1, t2, t3, t4, t5, t6, t7);
          } else {
            store_u8_8x2(d, dst_stride, t0, t1);
          }

          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
#else   // !defined(__aarch64__)
    // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
    // rounding right shift by FILTER_BITS - instead of a first rounding right
    // shift by ROUND0_BITS, followed by second rounding right shift by
    // FILTER_BITS - ROUND0_BITS.
    // The outermost -1 is needed because we halved the filter values.
    const int16x8_t horiz_const = vdupq_n_s16(1 << ((ROUND0_BITS - 1) - 1));

    do {
      t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

      width = w;
      s = src + 8;
      d = dst;
      __builtin_prefetch(dst);

      do {
        t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s11 = s0;
        s0 = s7;

        s1 = vextq_s16(s11, s7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        s2 = vextq_s16(s11, s7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        s3 = vextq_s16(s11, s7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        s4 = vextq_s16(s11, s7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        s5 = vextq_s16(s11, s7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        s6 = vextq_s16(s11, s7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        s7 = vextq_s16(s11, s7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        t0 = convolve8_horiz_8x8(s11, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                 horiz_const);

        vst1_u8(d, t0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += src_stride;
      dst += dst_stride;
      h -= 1;
    } while (h > 0);
#endif  // defined(__aarch64__)
    }
#if defined(__aarch64__)
  }
#endif  // defined(__aarch64__)
}

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE void convolve_y_sr_6tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16x8_t y_filter_0_7) {
  if (w <= 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5;
    int16x4_t s0, s1, s2, s3, s4, s5, d0;
    uint8x8_t d01;

#if defined(__aarch64__)
    uint8x8_t t6, t7, t8;
    int16x4_t s6, s7, s8, d1, d2, d3;
    uint8x8_t d23;
#endif  // defined(__aarch64__)

    const uint8_t *s = src_ptr + src_stride;
    uint8_t *d = dst_ptr;

    load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4);
    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));
    s += 5 * src_stride;

    do {
#if defined(__aarch64__)
      load_u8_8x4(s, src_stride, &t5, &t6, &t7, &t8);
      s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
      s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));

      d0 = convolve6_4x4(s0, s1, s2, s3, s4, s5, y_filter_0_7);
      d1 = convolve6_4x4(s1, s2, s3, s4, s5, s6, y_filter_0_7);
      d2 = convolve6_4x4(s2, s3, s4, s5, s6, s7, y_filter_0_7);
      d3 = convolve6_4x4(s3, s4, s5, s6, s7, s8, y_filter_0_7);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(d + 0 * dst_stride, d01, 0);
        store_u8_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(d + 2 * dst_stride, d23, 0);
          store_u8_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(d + 0 * dst_stride, d01, 0);
        store_u8_4x1(d + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(d + 2 * dst_stride, d23, 0);
          store_u8_4x1(d + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
#else   // !defined(__aarch64__)
      t5 = vld1_u8(s);
      s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));

      d0 = convolve6_4x4(s0, s1, s2, s3, s4, s5, y_filter_0_7);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, vdup_n_s16(0)), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(d, d01, 0);
      } else {
        store_u8_4x1(d, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s += src_stride;
      d += dst_stride;
      h--;
#endif  // defined(__aarch64__)
    } while (h > 0);
  } else {
    // if width is a multiple of 8 & height is a multiple of 4
    uint8x8_t t0, t1, t2, t3, t4, t5;
    int16x8_t s0, s1, s2, s3, s4, s5, dd0;
    uint8x8_t d0;
#if defined(__aarch64__)
    uint8x8_t t6, t7, t8;
    int16x8_t s6, s7, s8, dd1, dd2, dd3;
    uint8x8_t d1, d2, d3;
#endif  // defined(__aarch64__)

    do {
      int height = h;
      const uint8_t *s = src_ptr + src_stride;
      uint8_t *d = dst_ptr;

      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s += 5 * src_stride;

      do {
#if defined(__aarch64__)
        load_u8_8x4(s, src_stride, &t5, &t6, &t7, &t8);
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t8));

        dd0 = convolve6_8x4(s0, s1, s2, s3, s4, s5, y_filter_0_7);
        dd1 = convolve6_8x4(s1, s2, s3, s4, s5, s6, y_filter_0_7);
        dd2 = convolve6_8x4(s2, s3, s4, s5, s6, s7, y_filter_0_7);
        dd3 = convolve6_8x4(s3, s4, s5, s6, s7, s8, y_filter_0_7);

        d0 = vqrshrun_n_s16(dd0, FILTER_BITS - 1);
        d1 = vqrshrun_n_s16(dd1, FILTER_BITS - 1);
        d2 = vqrshrun_n_s16(dd2, FILTER_BITS - 1);
        d3 = vqrshrun_n_s16(dd3, FILTER_BITS - 1);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !defined(__aarch64__)
        t5 = vld1_u8(s);
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));

        dd0 = convolve6_8x4(s0, s1, s2, s3, s4, s5, y_filter_0_7);
        d0 = vqrshrun_n_s16(dd0, FILTER_BITS - 1);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // defined(__aarch64__)
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t convolve12_y_4x4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, y_filter_0_3, 0);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmla_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmla_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmla_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmla_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmla_lane_s16(sum, s11, y_filter_8_11, 3);

  // Separate out the two filter values in the middle of the kernel that have
  // the largest magnitude and use saturating addition to prevent overflow. This
  // means we can stay at 16-bit elements, rather than having to widen
  // everything to a 32-bit result, requiring twice the number of instructions.
  sum = vqadd_s16(sum, vmul_lane_s16(s5, y_filter_4_7, 1));
  sum = vqadd_s16(sum, vmul_lane_s16(s6, y_filter_4_7, 2));

  return sum;
}

static INLINE uint8x8_t convolve12_y_8x4_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlaq_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlaq_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlaq_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlaq_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmlaq_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmlaq_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmlaq_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmlaq_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmlaq_lane_s16(sum, s11, y_filter_8_11, 3);

  // Separate out the two filter values in the middle of the kernel that have
  // the largest magnitude and use saturating addition to prevent overflow. This
  // means we can stay at 16-bit elements, rather than having to widen
  // everything to a 32-bit result, requiring twice the number of instructions.
  sum = vqaddq_s16(sum, vmulq_lane_s16(s5, y_filter_4_7, 1));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s6, y_filter_4_7, 2));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve_y_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            int dst_stride, int w, int h,
                                            const int16_t *y_filter_ptr) {
  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (y_filter_ptr[5] == 128) {
    // Undo the horizontal offset in the calling function
    src_ptr += 5 * src_stride;

    if (w <= 4) {
      for (int i = 0; i < h; i += 2) {
        uint8x8_t d0 = load_unaligned_u8(src_ptr + i * src_stride, src_stride);
        if (w == 2) {
          store_u8_2x1(dst_ptr + i * dst_stride, d0, 0);
          store_u8_2x1(dst_ptr + (i + 1) * dst_stride, d0, 1);
        } else if (w == 4) {
          store_u8_4x1(dst_ptr + i * dst_stride, d0, 0);
          store_u8_4x1(dst_ptr + (i + 1) * dst_stride, d0, 1);
        }
      }
    } else {
      for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j += 8) {
          uint8x8_t d0 = vld1_u8(src_ptr + i * src_stride + j);
          vst1_u8(dst_ptr + i * dst_stride + j, d0);
        }
      }
    }
    return;
  }

  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

  if (w <= 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
    int16x4_t d0, d1, d2, d3;
    int16x8_t dd01, dd23;
    uint8x8_t d01, d23;

    load_u8_8x11(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7,
                 &t8, &t9, &t10);
    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));
    s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
    s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
    s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
    s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));
    s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t9)));
    s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t10)));

    src_ptr += 11 * src_stride;

    do {
      load_u8_8x4(src_ptr, src_stride, &t11, &t12, &t13, &t14);
      s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t11)));
      s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t12)));
      s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t13)));
      s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t14)));

      d0 = convolve12_y_4x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, y_filter_0_7, y_filter_8_11);
      d1 = convolve12_y_4x4_s32(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                s12, y_filter_0_7, y_filter_8_11);
      d2 = convolve12_y_4x4_s32(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                s13, y_filter_0_7, y_filter_8_11);
      d3 = convolve12_y_4x4_s32(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                                s14, y_filter_0_7, y_filter_8_11);

      dd01 = vcombine_s16(d0, d1);
      dd23 = vcombine_s16(d2, d3);

      d01 = vqrshrun_n_s16(dd01, FILTER_BITS);
      d23 = vqrshrun_n_s16(dd23, FILTER_BITS);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14;
    uint8x8_t d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;

    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      load_u8_8x11(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8,
                   &t9, &t10);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
      s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      s10 = vreinterpretq_s16_u16(vmovl_u8(t10));

      s += 11 * src_stride;

      do {
        load_u8_8x4(s, src_stride, &t11, &t12, &t13, &t14);
        s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t13));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t14));

        d0 = convolve12_y_8x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                  s11, y_filter_0_7, y_filter_8_11);
        d1 = convolve12_y_8x4_s32(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                  s12, y_filter_0_7, y_filter_8_11);
        d2 = convolve12_y_8x4_s32(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                  s13, y_filter_0_7, y_filter_8_11);
        d3 = convolve12_y_8x4_s32(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                  s13, s14, y_filter_0_7, y_filter_8_11);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s7 = s11;
        s8 = s12;
        s9 = s13;
        s10 = s14;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void av1_convolve_y_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_y,
                            const int subpel_y_qn) {
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int vert_offset = filter_params_y->taps / 2 - 1;

  src -= vert_offset * src_stride;

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (y_filter_taps > 8) {
    convolve_y_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             y_filter_ptr);
    return;
  }

  // Filter values are even so downshift by 1 to reduce precision requirements.
  const int16x8_t y_filter = vshrq_n_s16(vld1q_s16(y_filter_ptr), 1);

  if (y_filter_taps < 8) {
    convolve_y_sr_6tap_neon(src, src_stride, dst, dst_stride, w, h, y_filter);
    return;
  }

  if (w <= 4) {
    uint8x8_t d01;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, d0;
#if defined(__aarch64__)
    uint8x8_t d23;
    int16x4_t s8, s9, s10, d1, d2, d3;
#endif  // defined(__aarch64__)
    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;

    do {
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
#if defined(__aarch64__)
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      dst += 4 * dst_stride;
      h -= 4;
#else   // !defined(__aarch64__)
      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);

      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d0), FILTER_BITS - 1);

      if (w == 4) {
        store_u8_4x1(dst, d01, 0);
      } else if (w == 2) {
        store_u8_2x1(dst, d01, 0);
      }
      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      dst += dst_stride;
      h -= 1;
#endif  // defined(__aarch64__)
    } while (h > 0);
  } else {
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
#if defined(__aarch64__)
    uint8x8_t t1, t2, t3;
    int16x8_t s8, s9, s10;
#endif  // defined(__aarch64__)
    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      s = src;
      s0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      d = dst;
      height = h;

      do {
        s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
#if defined(__aarch64__)
        s8 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s9 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s10 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);
        t0 = convolve8_vert_8x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
        t1 = convolve8_vert_8x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
        t2 = convolve8_vert_8x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
        t3 = convolve8_vert_8x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, t0, t1, t2, t3);
        } else {
          store_u8_8x2(d, dst_stride, t0, t1);
        }
        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        d += 4 * dst_stride;
        height -= 4;
#else   // !defined(__aarch64__)
        __builtin_prefetch(d);
        __builtin_prefetch(s);

        t0 = convolve8_vert_8x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

        vst1_u8(d, t0);
        d += dst_stride;

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        height -= 1;
#endif  // defined(__aarch64__)
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

#if defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int16x4_t convolve12_horiz_4_usdot(uint8x16_t samples,
                                                 const int8x16_t filters,
                                                 const uint8x16x3_t permute_tbl,
                                                 int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum;

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  /* First 4 output values. */
  sum = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum = vusdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vusdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  /* Narrow and re-pack. */
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_horiz_8_usdot(uint8x16_t samples0,
                                                 uint8x16_t samples1,
                                                 const int8x16_t filters,
                                                 const uint8x16x3_t permute_tbl,
                                                 const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[4];
  int32x4_t sum[2];

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples0, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples0, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples0, permute_tbl.val[2]);
  /* {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 } */
  permuted_samples[3] = vqtbl1q_u8(samples1, permute_tbl.val[2]);

  /* First 4 output values. */
  sum[0] = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  /* Second 4 output values. */
  sum[1] = vusdotq_laneq_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  /* Narrow and re-pack. */
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS),
                      vshrn_n_s32(sum[1], ROUND0_BITS));
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(x_filter_0_7, 5) == 128) {
    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd - 1)));
    // Undo the horizontal offset in the calling function.
    src_ptr += 5;

    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j += 8) {
        uint8x8_t s0 = vld1_u8(src_ptr + i * src_stride + j);
        uint16x8_t t0 = vaddw_u8(vreinterpretq_u16_s16(horiz_const), s0);
        int16x8_t d0 =
            vshlq_n_s16(vreinterpretq_s16_u16(t0), FILTER_BITS - ROUND0_BITS);
        if (w == 2) {
          store_s16_2x1(dst_ptr + i * dst_stride, vget_low_s16(d0), 0);
        } else if (w == 4) {
          vst1_s16(dst_ptr + i * dst_stride, vget_low_s16(d0));
        } else {
          vst1q_s16(dst_ptr + i * dst_stride + j, d0);
        }
      }
    }
  } else {
    // Narrow filter values to 8-bit.
    const int16x8x2_t x_filter_s16 = {
      { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
    };
    const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                           vmovn_s16(x_filter_s16.val[1]));
    // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
    // - which are generally faster than rounding shifts on modern CPUs.
    const int32x4_t horiz_const =
        vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);

    if (w <= 4) {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0, s1, s2, s3;
          int16x4_t d0, d1, d2, d3;

          load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

          d0 = convolve12_horiz_4_usdot(s0, x_filter, permute_tbl, horiz_const);
          d1 = convolve12_horiz_4_usdot(s1, x_filter, permute_tbl, horiz_const);
          d2 = convolve12_horiz_4_usdot(s2, x_filter, permute_tbl, horiz_const);
          d3 = convolve12_horiz_4_usdot(s3, x_filter, permute_tbl, horiz_const);

          if (w == 2) {
            store_s16_2x1(d + 0 * dst_stride, d0, 0);
            store_s16_2x1(d + 1 * dst_stride, d1, 0);
            store_s16_2x1(d + 2 * dst_stride, d2, 0);
            store_s16_2x1(d + 3 * dst_stride, d3, 0);
          } else {
            store_s16_4x4(d, dst_stride, d0, d1, d2, d3);
          }

          s += 4;
          d += 4;
          width -= 4;
        } while (width > 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h >= 4);

      for (; h > 0; h--) {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0;
          int16x4_t d0;

          s0 = vld1q_u8(s);

          d0 = convolve12_horiz_4_usdot(s0, x_filter, permute_tbl, horiz_const);

          if (w == 2) {
            store_s16_2x1(d, d0, 0);
          } else {
            vst1_s16(d, d0);
          }

          s += 4;
          d += 4;
          width -= 4;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      }
    } else {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          int16x8_t d0, d1, d2, d3;

          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          d0 = convolve12_horiz_8_usdot(s0[0], s0[1], x_filter, permute_tbl,
                                        horiz_const);
          d1 = convolve12_horiz_8_usdot(s1[0], s1[1], x_filter, permute_tbl,
                                        horiz_const);
          d2 = convolve12_horiz_8_usdot(s2[0], s2[1], x_filter, permute_tbl,
                                        horiz_const);
          d3 = convolve12_horiz_8_usdot(s3[0], s3[1], x_filter, permute_tbl,
                                        horiz_const);

          store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h >= 4);

      for (; h > 0; h--) {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2];
          int16x8_t d0;

          s0[0] = vld1q_u8(s);
          s0[1] = vld1q_u8(s + 4);

          d0 = convolve12_horiz_8_usdot(s0[0], s0[1], x_filter, permute_tbl,
                                        horiz_const);

          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      }
    }
  }
}

#elif defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(x_filter_0_7, 5) == 128) {
    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd - 1)));
    // Undo the horizontal offset in the calling function.
    src_ptr += 5;

    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j += 8) {
        uint8x8_t s0 = vld1_u8(src_ptr + i * src_stride + j);
        uint16x8_t t0 = vaddw_u8(vreinterpretq_u16_s16(horiz_const), s0);
        int16x8_t d0 =
            vshlq_n_s16(vreinterpretq_s16_u16(t0), FILTER_BITS - ROUND0_BITS);
        if (w == 2) {
          store_s16_2x1(dst_ptr + i * dst_stride, vget_low_s16(d0), 0);
        } else if (w == 4) {
          vst1_s16(dst_ptr + i * dst_stride, vget_low_s16(d0));
        } else {
          vst1q_s16(dst_ptr + i * dst_stride + j, d0);
        }
      }
    }
  } else {
    // Narrow filter values to 8-bit.
    const int16x8x2_t x_filter_s16 = {
      { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
    };
    const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                           vmovn_s16(x_filter_s16.val[1]));

    // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
    // - which are generally faster than rounding shifts on modern CPUs.
    const int32_t horiz_const =
        ((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
    // Dot product constants.
    const int32x4_t correct_tmp =
        vaddq_s32(vpaddlq_s16(vshlq_n_s16(x_filter_s16.val[0], 7)),
                  vpaddlq_s16(vshlq_n_s16(x_filter_s16.val[1], 7)));
    const int32x4_t correction =
        vdupq_n_s32(vaddvq_s32(correct_tmp) + horiz_const);
    const uint8x16_t range_limit = vdupq_n_u8(128);
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);

    if (w <= 4) {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0, s1, s2, s3;
          int16x4_t d0, d1, d2, d3;

          load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

          d0 = convolve12_horiz_4_sdot(s0, x_filter, correction, range_limit,
                                       permute_tbl);
          d1 = convolve12_horiz_4_sdot(s1, x_filter, correction, range_limit,
                                       permute_tbl);
          d2 = convolve12_horiz_4_sdot(s2, x_filter, correction, range_limit,
                                       permute_tbl);
          d3 = convolve12_horiz_4_sdot(s3, x_filter, correction, range_limit,
                                       permute_tbl);

          if (w == 2) {
            store_s16_2x1(d + 0 * dst_stride, d0, 0);
            store_s16_2x1(d + 1 * dst_stride, d1, 0);
            store_s16_2x1(d + 2 * dst_stride, d2, 0);
            store_s16_2x1(d + 3 * dst_stride, d3, 0);
          } else {
            store_s16_4x4(d, dst_stride, d0, d1, d2, d3);
          }

          s += 4;
          d += 4;
          width -= 4;
        } while (width > 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h >= 4);

      for (; h > 0; h--) {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0;
          int16x4_t d0;

          s0 = vld1q_u8(s);

          d0 = convolve12_horiz_4_sdot(s0, x_filter, correction, range_limit,
                                       permute_tbl);

          if (w == 2) {
            store_s16_2x1(d, d0, 0);
          } else {
            vst1_s16(d, d0);
          }

          s += 4;
          d += 4;
          width -= 4;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      }
    } else {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          int16x8_t d0, d1, d2, d3;

          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          d0 = convolve12_horiz_8_sdot(s0[0], s0[1], x_filter, correction,
                                       range_limit, permute_tbl);
          d1 = convolve12_horiz_8_sdot(s1[0], s1[1], x_filter, correction,
                                       range_limit, permute_tbl);
          d2 = convolve12_horiz_8_sdot(s2[0], s2[1], x_filter, correction,
                                       range_limit, permute_tbl);
          d3 = convolve12_horiz_8_sdot(s3[0], s3[1], x_filter, correction,
                                       range_limit, permute_tbl);

          store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h >= 4);

      for (; h > 0; h--) {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2];
          int16x8_t d0;

          s0[0] = vld1q_u8(s);
          s0[1] = vld1q_u8(s + 4);

          d0 = convolve12_horiz_8_sdot(s0[0], s0[1], x_filter, correction,
                                       range_limit, permute_tbl);

          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      }
    }
  }
}

#else  // !(defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD))

static INLINE int16x4_t convolve12_horiz_4x4_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
    const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);
  int32x4_t sum;

  sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vshrn_n_s32(sum, ROUND0_BITS);
}

// 4 column per iteration horizontal filtering for 12-tap convolve_2d_sr.
// Processes one row at a time.
static INLINE void horiz_filter_12tap_w4_single_row(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11, const int32x4_t horiz_const) {
  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    do {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, d0;
      uint8x16_t t0;
      int16x8_t tt0, tt1;

      t0 = vld1q_u8(s);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      s0 = vget_low_s16(tt0);
      s4 = vget_high_s16(tt0);
      s8 = vget_low_s16(tt1);
      s12 = vget_high_s16(tt1);

      s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      d0 = convolve12_horiz_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, x_filter_0_7, x_filter_8_11,
                                    horiz_const);

      if (w == 2) {
        store_s16_2x1(d, d0, 0);
      } else {
        vst1_s16(d, d0);
      }

      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);

    src_ptr += src_stride;
    dst_ptr += dst_stride;
    h--;
  } while (h > 0);
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts -
  // which are generally faster than rounding shifts on modern CPUs.
  const int32x4_t horiz_const =
      vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));

#if defined(__aarch64__)
  do {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint8x8_t t0, t1, t2, t3;

    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      int16x4_t s11, s12, s13, s14, d0, d1, d2, d3;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve12_horiz_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, x_filter_0_7, x_filter_8_11,
                                    horiz_const);
      d1 = convolve12_horiz_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, s12, x_filter_0_7, x_filter_8_11,
                                    horiz_const);
      d2 = convolve12_horiz_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                    s12, s13, x_filter_0_7, x_filter_8_11,
                                    horiz_const);
      d3 = convolve12_horiz_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                    s13, s14, x_filter_0_7, x_filter_8_11,
                                    horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      if (w == 2) {
        store_s16_2x1(d + 0 * dst_stride, d0, 0);
        store_s16_2x1(d + 1 * dst_stride, d1, 0);
        store_s16_2x1(d + 2 * dst_stride, d2, 0);
        store_s16_2x1(d + 3 * dst_stride, d3, 0);
      } else {
        store_s16_4x4(d, dst_stride, d0, d1, d2, d3);
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;

      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);

    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h >= 4);

  if (h) {
    horiz_filter_12tap_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride,
                                     w, h, x_filter_0_7, x_filter_8_11,
                                     horiz_const);
  }
#else   // !defined(__aarch64__)
  horiz_filter_12tap_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w,
                                   h, x_filter_0_7, x_filter_8_11, horiz_const);
#endif  // defined(__aarch64__)
}

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

#if defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE void convolve_2d_sr_horiz_8tap_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, int im_stride, int w,
    int im_h, const int16x8_t x_filter_s16) {
  const int bd = 8;

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;

  int height = im_h;

  // Filter values are even, so downshift by 1 to reduce intermediate precision
  // requirements.
  const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);
  // This shim of  1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // The outermost -1 is needed because we halved the filter values.
  const int32x4_t horiz_const = vdupq_n_s32((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));

  if (w <= 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int32x4_t t0, t1, t2, t3;
    int16x4_t d0, d1, d2, d3;

    do {
      assert(height >= 4);

      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_usdot(s0, x_filter, permute_tbl, horiz_const);
      t1 = convolve8_4_usdot(s1, x_filter, permute_tbl, horiz_const);
      t2 = convolve8_4_usdot(s2, x_filter, permute_tbl, horiz_const);
      t3 = convolve8_4_usdot(s3, x_filter, permute_tbl, horiz_const);

      // We halved the convolution filter values so -1 from the right shift.
      d0 = vshrn_n_s32(t0, ROUND0_BITS - 1);
      d1 = vshrn_n_s32(t1, ROUND0_BITS - 1);
      d2 = vshrn_n_s32(t2, ROUND0_BITS - 1);
      d3 = vshrn_n_s32(t3, ROUND0_BITS - 1);

      if (w == 2) {
        store_s16_2x1(dst_ptr + 0 * dst_stride, d0, 0);
        store_s16_2x1(dst_ptr + 1 * dst_stride, d1, 0);
        store_s16_2x1(dst_ptr + 2 * dst_stride, d2, 0);
        store_s16_2x1(dst_ptr + 3 * dst_stride, d3, 0);
      } else {
        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);
      }

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);

      do {
        s0 = vld1q_u8(src_ptr);
        t0 = convolve8_4_usdot(s0, x_filter, permute_tbl, horiz_const);
        // We halved the convolution filter values so -1 from the right shift.
        d0 = vshrn_n_s32(t0, ROUND0_BITS - 1);

        if (w == 2) {
          store_s16_2x1(dst_ptr, d0, 0);
        } else {
          vst1_s16(dst_ptr, d0);
        }

        src_ptr += src_stride;
        dst_ptr += dst_stride;
        height--;
      } while (height > 0);
    }
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int16x8_t d0, d1, d2, d3;

    do {
      assert(height >= 4);

      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 = convolve8_horiz_8_usdot(s0, x_filter, permute_tbl, horiz_const);
        d1 = convolve8_horiz_8_usdot(s1, x_filter, permute_tbl, horiz_const);
        d2 = convolve8_horiz_8_usdot(s2, x_filter, permute_tbl, horiz_const);
        d3 = convolve8_horiz_8_usdot(s3, x_filter, permute_tbl, horiz_const);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);

      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          s0 = vld1q_u8(s);
          d0 = convolve8_horiz_8_usdot(s0, x_filter, permute_tbl, horiz_const);
          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
        height--;
      } while (height > 0);
    }
  }
}

#elif defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE void convolve_2d_sr_horiz_8tap_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, int im_stride, int w,
    int im_h, const int16x8_t x_filter_s16) {
  const int bd = 8;

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;

  int height = im_h;

  // Filter values are even, so downshift by 1 to reduce intermediate precision
  // requirements.
  const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);
  // This shim of  1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // The outermost -1 is needed because we halved the filter values.
  const int32_t horiz_const =
      ((1 << (bd + FILTER_BITS - 2)) + (1 << ((ROUND0_BITS - 1) - 1)));
  // Dot product constants.
  const int16x8_t correct_tmp = vshlq_n_s16(x_filter_s16, 6);
  int32x4_t correction = vdupq_n_s32(vaddlvq_s16(correct_tmp) + horiz_const);
  const uint8x16_t range_limit = vdupq_n_u8(128);

  if (w <= 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int32x4_t t0, t1, t2, t3;
    int16x4_t d0, d1, d2, d3;

    do {
      assert(height >= 4);

      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_sdot(s0, x_filter, correction, range_limit, permute_tbl);
      t1 = convolve8_4_sdot(s1, x_filter, correction, range_limit, permute_tbl);
      t2 = convolve8_4_sdot(s2, x_filter, correction, range_limit, permute_tbl);
      t3 = convolve8_4_sdot(s3, x_filter, correction, range_limit, permute_tbl);

      // We halved the convolution filter values so -1 from the right shift.
      d0 = vshrn_n_s32(t0, ROUND0_BITS - 1);
      d1 = vshrn_n_s32(t1, ROUND0_BITS - 1);
      d2 = vshrn_n_s32(t2, ROUND0_BITS - 1);
      d3 = vshrn_n_s32(t3, ROUND0_BITS - 1);

      if (w == 2) {
        store_s16_2x1(dst_ptr + 0 * dst_stride, d0, 0);
        store_s16_2x1(dst_ptr + 1 * dst_stride, d1, 0);
        store_s16_2x1(dst_ptr + 2 * dst_stride, d2, 0);
        store_s16_2x1(dst_ptr + 3 * dst_stride, d3, 0);
      } else {
        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);
      }

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);

      do {
        s0 = vld1q_u8(src_ptr);
        t0 = convolve8_4_sdot(s0, x_filter, correction, range_limit,
                              permute_tbl);
        // We halved the convolution filter values so -1 from the right shift.
        d0 = vshrn_n_s32(t0, ROUND0_BITS - 1);

        if (w == 2) {
          store_s16_2x1(dst_ptr, d0, 0);
        } else {
          vst1_s16(dst_ptr, d0);
        }

        src_ptr += src_stride;
        dst_ptr += dst_stride;
        height--;
      } while (height > 0);
    }
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    uint8x16_t s0, s1, s2, s3;
    int16x8_t d0, d1, d2, d3;

    do {
      assert(height >= 4);

      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 = convolve8_horiz_8_sdot(s0, x_filter, correction, range_limit,
                                    permute_tbl);
        d1 = convolve8_horiz_8_sdot(s1, x_filter, correction, range_limit,
                                    permute_tbl);
        d2 = convolve8_horiz_8_sdot(s2, x_filter, correction, range_limit,
                                    permute_tbl);
        d3 = convolve8_horiz_8_sdot(s3, x_filter, correction, range_limit,
                                    permute_tbl);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);

      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          s0 = vld1q_u8(s);
          d0 = convolve8_8_sdot(s0, x_filter, correction, range_limit,
                                permute_tbl, vdupq_n_s16(0));
          // We halved the convolution filter values so -1 from the right shift.
          d0 = vshrq_n_s16(d0, ROUND0_BITS - 1);
          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
        height--;
      } while (height > 0);
    }
  }
}

#else  // !(defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD))

// Horizontal filtering for convolve_2d_sr for width multiple of 8
// Processes one row at a time
static INLINE void horiz_filter_w8_single_row(const uint8_t *src_ptr,
                                              int src_stride, int16_t *dst_ptr,
                                              const int dst_stride, int width,
                                              int height,
                                              const int16x8_t x_filter,
                                              const int16x8_t horiz_const) {
  int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
  do {
    uint8x8_t t0 = vld1_u8(src_ptr);
    s0 = vreinterpretq_s16_u16(vmovl_u8(t0));  // a0 a1 a2 a3 a4 a5 a6 a7

    int width_tmp = width;
    const uint8_t *s = src_ptr + 8;
    int16_t *dst_tmp = dst_ptr;

    __builtin_prefetch(dst_ptr);

    do {
      t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
      s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t sum = s0;
      s0 = s7;

      s1 = vextq_s16(sum, s7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
      s2 = vextq_s16(sum, s7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
      s3 = vextq_s16(sum, s7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
      s4 = vextq_s16(sum, s7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
      s5 = vextq_s16(sum, s7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
      s6 = vextq_s16(sum, s7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
      s7 = vextq_s16(sum, s7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

      int16x8_t res0 = convolve8_horiz_8x8_s16(sum, s1, s2, s3, s4, s5, s6, s7,
                                               x_filter, horiz_const);

      vst1q_s16(dst_tmp, res0);

      s += 8;
      dst_tmp += 8;
      width_tmp -= 8;
    } while (width_tmp > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
    height--;
  } while (height > 0);
}

// Horizontal filtering for convolve_2d_sr for width <= 4
// Processes one row at a time
static INLINE void horiz_filter_w4_single_row(const uint8_t *src_ptr,
                                              int src_stride, int16_t *dst_ptr,
                                              const int dst_stride, int width,
                                              int height,
                                              const int16x8_t x_filter,
                                              const int16x4_t horiz_const) {
  int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
  do {
    const uint8_t *s = src_ptr;

    __builtin_prefetch(s);

    uint8x8_t t0 = vld1_u8(s);  // a0 a1 a2 a3 a4 a5 a6 a7
    int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    s0 = vget_low_s16(tt0);
    s4 = vget_high_s16(tt0);

    __builtin_prefetch(dst_ptr);
    s += 8;

    t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
    s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));

    s1 = vext_s16(s0, s4, 1);  // a1 a2 a3 a4
    s2 = vext_s16(s0, s4, 2);  // a2 a3 a4 a5
    s3 = vext_s16(s0, s4, 3);  // a3 a4 a5 a6
    s5 = vext_s16(s4, s7, 1);  // a5 a6 a7 a8
    s6 = vext_s16(s4, s7, 2);  // a6 a7 a8 a9
    s7 = vext_s16(s4, s7, 3);  // a7 a8 a9 a10

    int16x4_t d0 = convolve8_horiz_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7,
                                           x_filter, horiz_const);

    if (width == 2) {
      store_s16_2x1(dst_ptr, d0, 0);
    } else {
      vst1_s16(dst_ptr, d0);
    }

    dst_ptr += dst_stride;
    src_ptr += src_stride;
    height--;
  } while (height > 0);
}

static INLINE void convolve_2d_sr_horiz_8tap_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, int im_stride, int w,
    int im_h, const int16x8_t x_filter_s16) {
  const int bd = 8;

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;

  int height = im_h;

  // Filter values are even, so downshift by 1 to reduce intermediate precision
  // requirements.
  const int16x8_t x_filter = vshrq_n_s16(x_filter_s16, 1);

  if (w <= 4) {
    // This shim of  1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
    // shifts - which are generally faster than rounding shifts on modern CPUs.
    // The outermost -1 is needed because we halved the filter values.
    const int16x4_t horiz_const = vdup_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                             (1 << ((ROUND0_BITS - 1) - 1)));

#if defined(__aarch64__)
    do {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
      uint8x8_t t0, t1, t2, t3;
      const uint8_t *s = src_ptr;

      assert(height >= 4);

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
      s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

      s += 7;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_horiz_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                   horiz_const);
      d1 = convolve8_horiz_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                   horiz_const);
      d2 = convolve8_horiz_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                   horiz_const);
      d3 = convolve8_horiz_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                   horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      if (w == 2) {
        store_s16_2x1(dst_ptr + 0 * dst_stride, d0, 0);
        store_s16_2x1(dst_ptr + 1 * dst_stride, d1, 0);
        store_s16_2x1(dst_ptr + 2 * dst_stride, d2, 0);
        store_s16_2x1(dst_ptr + 3 * dst_stride, d3, 0);
      } else {
        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);
      }

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);
      horiz_filter_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w,
                                 height, x_filter, horiz_const);
    }

#else   // !defined(__aarch64__)
    horiz_filter_w4_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w,
                               height, x_filter, horiz_const);
#endif  // defined(__aarch64__)

  } else {
    // This shim of  1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
    // shifts - which are generally faster than rounding shifts on modern CPUs.
    // The outermost -1 is needed because we halved the filter values.
    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                              (1 << ((ROUND0_BITS - 1) - 1)));

#if defined(__aarch64__)

    for (; height >= 8; height -= 8) {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
          d0, d1, d2, d3, d4, d5, d6, d7;
      uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;

      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s += 7;

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        d0 = convolve8_horiz_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                     horiz_const);
        d1 = convolve8_horiz_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                     horiz_const);
        d2 = convolve8_horiz_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                     horiz_const);
        d3 = convolve8_horiz_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                     horiz_const);
        d4 = convolve8_horiz_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                     horiz_const);
        d5 = convolve8_horiz_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                     x_filter, horiz_const);
        d6 = convolve8_horiz_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                     x_filter, horiz_const);
        d7 = convolve8_horiz_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                     x_filter, horiz_const);

        transpose_s16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        store_s16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += 8 * src_stride;
      dst_ptr += 8 * dst_stride;
    }

    for (; height >= 4; height -= 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
          dd0, dd1, dd2, dd3, dd4, dd5, dd6, dd7;
      int16x8_t d0, d1, d2, d3;
      uint8x8_t t0, t1, t2, t3;

      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      load_u8_8x4(src_ptr, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
      s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

      s += 7;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
        s11 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        s12 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        s13 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        s14 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

        dd0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, x_filter);
        dd1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, x_filter);
        dd2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, x_filter);
        dd3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, x_filter);
        dd4 = convolve8_4x4(s4, s5, s6, s7, s8, s9, s10, s11, x_filter);
        dd5 = convolve8_4x4(s5, s6, s7, s8, s9, s10, s11, s12, x_filter);
        dd6 = convolve8_4x4(s6, s7, s8, s9, s10, s11, s12, s13, x_filter);
        dd7 = convolve8_4x4(s7, s8, s9, s10, s11, s12, s13, s14, x_filter);

        transpose_s16_4x8(&dd0, &dd1, &dd2, &dd3, &dd4, &dd5, &dd6, &dd7, &d0,
                          &d1, &d2, &d3);

        d0 = vaddq_s16(d0, horiz_const);
        d1 = vaddq_s16(d1, horiz_const);
        d2 = vaddq_s16(d2, horiz_const);
        d3 = vaddq_s16(d3, horiz_const);

        // We halved the convolution filter values so -1 from the right shift.
        d0 = vshrq_n_s16(d0, ROUND0_BITS - 1);
        d1 = vshrq_n_s16(d1, ROUND0_BITS - 1);
        d2 = vshrq_n_s16(d2, ROUND0_BITS - 1);
        d3 = vshrq_n_s16(d3, ROUND0_BITS - 1);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
    }

    if (height) {
      assert(height < 4);
      horiz_filter_w8_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w,
                                 height, x_filter, horiz_const);
    }

#else   // !defined(__aarch64__)
    horiz_filter_w8_single_row(src_ptr, src_stride, dst_ptr, dst_stride, w,
                               height, x_filter, horiz_const);
#endif  // defined(__aarch64__)
  }
}

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE int32x4_t convolve12_vert_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int32x4_t sum;

  sum = vmull_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, y_filter_8_11, 3);

  return sum;
}

static INLINE uint8x8_t convolve12_vert_8_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int16x8_t sub_const) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int32x4_t sum0, sum1;
  int16x8_t res;

  sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_4_7, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s8), y_filter_8_11, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s9), y_filter_8_11, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s10), y_filter_8_11, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s11), y_filter_8_11, 3);

  sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_4_7, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s8), y_filter_8_11, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s9), y_filter_8_11, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s10), y_filter_8_11, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s11), y_filter_8_11, 3);

  res = vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                     vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

static INLINE void convolve_2d_sr_vert_12tap_neon(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
    int32x4_t d0, d1, d2, d3;
    int16x8_t dd01, dd23;
    uint8x8_t d01, d23;

    load_s16_4x11(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,
                  &s8, &s9, &s10);
    src_ptr += 11 * src_stride;

    do {
      load_s16_4x4(src_ptr, src_stride, &s11, &s12, &s13, &s14);

      d0 = convolve12_vert_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                 s11, y_filter_0_7, y_filter_8_11);
      d1 = convolve12_vert_4_s32(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                 s12, y_filter_0_7, y_filter_8_11);
      d2 = convolve12_vert_4_s32(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                 s13, y_filter_0_7, y_filter_8_11);
      d3 = convolve12_vert_4_s32(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                                 s14, y_filter_0_7, y_filter_8_11);

      dd01 = vcombine_s16(vqrshrn_n_s32(d0, 2 * FILTER_BITS - ROUND0_BITS),
                          vqrshrn_n_s32(d1, 2 * FILTER_BITS - ROUND0_BITS));
      dd23 = vcombine_s16(vqrshrn_n_s32(d2, 2 * FILTER_BITS - ROUND0_BITS),
                          vqrshrn_n_s32(d3, 2 * FILTER_BITS - ROUND0_BITS));

      dd01 = vsubq_s16(dd01, sub_const);
      dd23 = vsubq_s16(dd23, sub_const);

      d01 = vqmovun_s16(dd01);
      d23 = vqmovun_s16(dd23);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 0);

  } else {
    do {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
      uint8x8_t d0, d1, d2, d3;

      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      int height = h;

      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        d0 = convolve12_vert_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                   s11, y_filter_0_7, y_filter_8_11, sub_const);
        d1 = convolve12_vert_8_s32(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                   s12, y_filter_0_7, y_filter_8_11, sub_const);
        d2 =
            convolve12_vert_8_s32(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                  s13, y_filter_0_7, y_filter_8_11, sub_const);
        d3 = convolve12_vert_8_s32(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                   s13, s14, y_filter_0_7, y_filter_8_11,
                                   sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s7 = s11;
        s8 = s12;
        s9 = s13;
        s10 = s14;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE void convolve_2d_sr_vert_8tap_neon(int16_t *src_ptr,
                                                 int src_stride,
                                                 uint8_t *dst_ptr,
                                                 int dst_stride, int w, int h,
                                                 const int16x8_t y_filter) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, d0;
    uint8x8_t d01;

#if defined(__aarch64__)
    int16x4_t s8, s9, s10, d1, d2, d3;
    uint8x8_t d23;
#endif  // defined(__aarch64__)

    int16_t *s = src_ptr;
    uint8_t *d = dst_ptr;

    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
#if defined(__aarch64__)
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      d0 = convolve8_vert_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      d1 = convolve8_vert_4_s32(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      d2 = convolve8_vert_4_s32(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      d3 = convolve8_vert_4_s32(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, d1), sub_const));
      d23 = vqmovun_s16(vsubq_s16(vcombine_s16(d2, d3), sub_const));

      if (w == 2) {
        store_u8_2x1(d + 0 * dst_stride, d01, 0);
        store_u8_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(d + 2 * dst_stride, d23, 0);
          store_u8_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(d + 0 * dst_stride, d01, 0);
        store_u8_4x1(d + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(d + 2 * dst_stride, d23, 0);
          store_u8_4x1(d + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
#else   // !defined(__aarch64__)
      s7 = vld1_s16(s);
      s += src_stride;

      d0 = convolve8_vert_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

      d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, vdup_n_s16(0)), sub_const));

      if (w == 2) {
        store_u8_2x1(d, d01, 0);
      } else {
        store_u8_4x1(d, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      d += dst_stride;
      h--;
#endif  // defined(__aarch64__)
    } while (h > 0);
  } else {
    // if width is a multiple of 8 & height is a multiple of 4
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint8x8_t d0;
#if defined(__aarch64__)
    int16x8_t s8, s9, s10;
    uint8x8_t d1, d2, d3;
#endif  // defined(__aarch64__)

    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
#if defined(__aarch64__)
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        d0 = convolve8_vert_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                  sub_const);
        d1 = convolve8_vert_8_s32(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                  sub_const);
        d2 = convolve8_vert_8_s32(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                  sub_const);
        d3 = convolve8_vert_8_s32(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                  sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !defined(__aarch64__)
        s7 = vld1q_s16(s);

        d0 = convolve8_vert_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                  sub_const);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // defined(__aarch64__)
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t
convolve6_vert_4_s32(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                     const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                     const int16x8_t y_filter) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);
  int32x4_t sum;

  sum = vmull_lane_s16(s0, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 2);

  return vqrshrn_n_s32(sum, 2 * FILTER_BITS - ROUND0_BITS);
}

static INLINE uint8x8_t
convolve6_vert_8_s32(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                     const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                     const int16x8_t y_filter, const int16x8_t sub_const) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);
  int32x4_t sum0, sum1;
  int16x8_t res;

  sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_hi, 2);

  sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_hi, 2);

  res = vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                     vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

static INLINE void convolve_2d_sr_vert_6tap_neon(int16_t *src_ptr,
                                                 int src_stride,
                                                 uint8_t *dst_ptr,
                                                 int dst_stride, int w, int h,
                                                 const int16x8_t y_filter) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, d0;
    uint8x8_t d01;

#if defined(__aarch64__)
    int16x4_t s6, s7, s8, d1, d2, d3;
    uint8x8_t d23;
#endif  // defined(__aarch64__)

    int16_t *s = src_ptr;
    uint8_t *d = dst_ptr;

    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
#if defined(__aarch64__)
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      d0 = convolve6_vert_4_s32(s0, s1, s2, s3, s4, s5, y_filter);
      d1 = convolve6_vert_4_s32(s1, s2, s3, s4, s5, s6, y_filter);
      d2 = convolve6_vert_4_s32(s2, s3, s4, s5, s6, s7, y_filter);
      d3 = convolve6_vert_4_s32(s3, s4, s5, s6, s7, s8, y_filter);

      d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, d1), sub_const));
      d23 = vqmovun_s16(vsubq_s16(vcombine_s16(d2, d3), sub_const));

      if (w == 2) {
        store_u8_2x1(d + 0 * dst_stride, d01, 0);
        store_u8_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(d + 2 * dst_stride, d23, 0);
          store_u8_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(d + 0 * dst_stride, d01, 0);
        store_u8_4x1(d + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(d + 2 * dst_stride, d23, 0);
          store_u8_4x1(d + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
#else   // !defined(__aarch64__)
      s5 = vld1_s16(s);

      d0 = convolve6_vert_4_s32(s0, s1, s2, s3, s4, s5, y_filter);
      d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, vdup_n_s16(0)), sub_const));

      if (w == 2) {
        store_u8_2x1(d, d01, 0);
      } else {
        store_u8_4x1(d, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s += src_stride;
      d += dst_stride;
      h--;
#endif  // defined(__aarch64__)
    } while (h > 0);
  } else {
    // if width is a multiple of 8 & height is a multiple of 4
    int16x8_t s0, s1, s2, s3, s4, s5;
    uint8x8_t d0;
#if defined(__aarch64__)
    int16x8_t s6, s7, s8;
    uint8x8_t d1, d2, d3;
#endif  // defined(__aarch64__)

    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
#if defined(__aarch64__)
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        d0 = convolve6_vert_8_s32(s0, s1, s2, s3, s4, s5, y_filter, sub_const);
        d1 = convolve6_vert_8_s32(s1, s2, s3, s4, s5, s6, y_filter, sub_const);
        d2 = convolve6_vert_8_s32(s2, s3, s4, s5, s6, s7, y_filter, sub_const);
        d3 = convolve6_vert_8_s32(s3, s4, s5, s6, s7, s8, y_filter, sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !defined(__aarch64__)
        s5 = vld1q_s16(s);

        d0 = convolve6_vert_8_s32(s0, s1, s2, s3, s4, s5, y_filter, sub_const);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // defined(__aarch64__)
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void av1_convolve_2d_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  (void)conv_params;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
    const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
    const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
    const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

    convolve_2d_sr_horiz_12tap_neon(src_ptr, src_stride, im_block, im_stride, w,
                                    im_h, x_filter_0_7, x_filter_8_11);

    convolve_2d_sr_vert_12tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_0_7, y_filter_8_11);
  } else {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

    convolve_2d_sr_horiz_8tap_neon(src_ptr, src_stride, im_block, im_stride, w,
                                   im_h, x_filter);

    if (clamped_y_taps <= 6) {
      convolve_2d_sr_vert_6tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    } else {
      convolve_2d_sr_vert_8tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    }
  }
}

static INLINE void scaledconvolve_horiz_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[4 * 4]);
  int x, y, z;

  src -= SUBPEL_TAPS / 2 - 1;

  y = h;
  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      // process 4 src_x steps
      for (z = 0; z < 4; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          uint8x8_t s[8], d;
          int16x8_t ss[4];
          int16x4_t t[8], tt;

          load_u8_8x4(src_x, src_stride, &s[0], &s[1], &s[2], &s[3]);
          transpose_u8_8x4(&s[0], &s[1], &s[2], &s[3]);

          ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
          ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
          ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
          ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
          t[0] = vget_low_s16(ss[0]);
          t[1] = vget_low_s16(ss[1]);
          t[2] = vget_low_s16(ss[2]);
          t[3] = vget_low_s16(ss[3]);
          t[4] = vget_high_s16(ss[0]);
          t[5] = vget_high_s16(ss[1]);
          t[6] = vget_high_s16(ss[2]);
          t[7] = vget_high_s16(ss[3]);

          tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7],
                           filters);
          d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
          store_u8_4x1(&temp[4 * z], d, 0);
        } else {
          int i;
          for (i = 0; i < 4; ++i) {
            temp[z * 4 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 4x4 filters values back to dst
      {
        const uint8x8x4_t d4 = vld4_u8(temp);
        store_u8_4x1(&dst[x + 0 * dst_stride], d4.val[0], 0);
        store_u8_4x1(&dst[x + 1 * dst_stride], d4.val[1], 0);
        store_u8_4x1(&dst[x + 2 * dst_stride], d4.val[2], 0);
        store_u8_4x1(&dst[x + 3 * dst_stride], d4.val[3], 0);
      }
      x += 4;
    } while (x < w);

    src += src_stride * 4;
    dst += dst_stride * 4;
    y -= 4;
  } while (y > 0);
}

static INLINE void scaledconvolve_horiz_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  // This function processes 8x8 areas. The intermediate height is not always
  // a multiple of 8, so force it to be a multiple of 8 here.
  y = (h + 7) & ~7;

  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      uint8x8_t d[8];
      // process 8 src_x steps
      for (z = 0; z < 8; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];

        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          uint8x8_t s[8];
          load_u8_8x8(src_x, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4],
                      &s[5], &s[6], &s[7]);
          transpose_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                           &s[7]);
          d[0] = scale_filter_8(s, filters);
          vst1_u8(&temp[8 * z], d[0]);
        } else {
          int i;
          for (i = 0; i < 8; ++i) {
            temp[z * 8 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 8x8 filters values back to dst
      load_u8_8x8(temp, 8, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                  &d[7]);
      transpose_u8_8x8(&d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]);
      store_u8_8x8(dst + x, dst_stride, d[0], d[1], d[2], d[3], d[4], d[5],
                   d[6], d[7]);
      x += 8;
    } while (x < w);

    src += src_stride * 8;
    dst += dst_stride * 8;
  } while (y -= 8);
}

static INLINE void scaledconvolve_vert_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];

    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      uint8x8_t s[8], d;
      int16x4_t t[8], tt;

      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      t[0] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[0])));
      t[1] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[1])));
      t[2] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[2])));
      t[3] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[3])));
      t[4] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[4])));
      t[5] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[5])));
      t[6] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[6])));
      t[7] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[7])));

      tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], filters);
      d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
      store_u8_4x1(dst, d, 0);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }

    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      uint8x8_t s[8], d;
      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      d = scale_filter_8(s, filters);
      vst1_u8(dst, d);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w16(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int x, y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      x = 0;
      do {
        const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
        uint8x16_t ss[8];
        uint8x8_t s[8], d[2];
        load_u8_16x8(src_y, src_stride, &ss[0], &ss[1], &ss[2], &ss[3], &ss[4],
                     &ss[5], &ss[6], &ss[7]);
        s[0] = vget_low_u8(ss[0]);
        s[1] = vget_low_u8(ss[1]);
        s[2] = vget_low_u8(ss[2]);
        s[3] = vget_low_u8(ss[3]);
        s[4] = vget_low_u8(ss[4]);
        s[5] = vget_low_u8(ss[5]);
        s[6] = vget_low_u8(ss[6]);
        s[7] = vget_low_u8(ss[7]);
        d[0] = scale_filter_8(s, filters);

        s[0] = vget_high_u8(ss[0]);
        s[1] = vget_high_u8(ss[1]);
        s[2] = vget_high_u8(ss[2]);
        s[3] = vget_high_u8(ss[3]);
        s[4] = vget_high_u8(ss[4]);
        s[5] = vget_high_u8(ss[5]);
        s[6] = vget_high_u8(ss[6]);
        s[7] = vget_high_u8(ss[7]);
        d[1] = scale_filter_8(s, filters);
        vst1q_u8(&dst[x], vcombine_u8(d[0], d[1]));
        src_y += 16;
        x += 16;
      } while (x < w);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

void aom_scaled_2d_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // --Require an additional 8 rows for the horiz_w8 transpose tail.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  DECLARE_ALIGNED(16, uint8_t, temp[(135 + 8) * 64]);
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  if (w >= 8) {
    scaledconvolve_horiz_w8(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  } else {
    scaledconvolve_horiz_w4(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  }

  if (w >= 16) {
    scaledconvolve_vert_w16(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                            dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else if (w == 8) {
    scaledconvolve_vert_w8(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else {
    scaledconvolve_vert_w4(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  }
}
