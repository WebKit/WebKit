/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AV1_COMMON_ARM_CONVOLVE_NEON_H_
#define AOM_AV1_COMMON_ARM_CONVOLVE_NEON_H_

#include <arm_neon.h>

#define HORIZ_EXTRA_ROWS ((SUBPEL_TAPS + 7) & ~0x07)

static INLINE int16x4_t convolve8_4(const int16x4_t s0, const int16x4_t s1,
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
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);
  sum = vqadd_s16(sum, vmul_lane_s16(s3, filter_lo, 3));
  sum = vqadd_s16(sum, vmul_lane_s16(s4, filter_hi, 0));
  return sum;
}

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);
  sum = vqaddq_s16(sum, vmulq_lane_s16(s3, filter_lo, 3));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s4, filter_hi, 0));
  return vqrshrun_n_s16(sum, 7);
}

static INLINE uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filter) {
  int16x8_t ss[8];

  ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
  ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
  ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
  ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
  ss[4] = vreinterpretq_s16_u16(vmovl_u8(s[4]));
  ss[5] = vreinterpretq_s16_u16(vmovl_u8(s[5]));
  ss[6] = vreinterpretq_s16_u16(vmovl_u8(s[6]));
  ss[7] = vreinterpretq_s16_u16(vmovl_u8(s[7]));

  return convolve8_8(ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7],
                     filter);
}

static INLINE uint8x8_t wiener_convolve8_vert_4x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, int16_t *filter_y, const int bd,
    const int round1_bits) {
  int16x8_t ss0, ss1, ss2;
  int32x4_t sum0, sum1;
  int16x8_t tmp;
  uint8x8_t res;

  const int32_t round_const = (1 << (bd + round1_bits - 1));
  const int32x4_t round_bits = vdupq_n_s32(-round1_bits);
  const int32x4_t round_vec = vdupq_n_s32(round_const);
  const int16x4_t filter = vld1_s16(filter_y);

  ss0 = vaddq_s16(s0, s6);
  ss1 = vaddq_s16(s1, s5);
  ss2 = vaddq_s16(s2, s4);

  sum0 = vmull_lane_s16(vget_low_s16(ss0), filter, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(ss1), filter, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(ss2), filter, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter, 3);

  sum1 = vmull_lane_s16(vget_high_s16(ss0), filter, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(ss1), filter, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(ss2), filter, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter, 3);

  sum0 = vsubq_s32(sum0, round_vec);
  sum1 = vsubq_s32(sum1, round_vec);

  /* right shift & rounding */
  sum0 = vrshlq_s32(sum0, round_bits);
  sum1 = vrshlq_s32(sum1, round_bits);

  /* from int32x4_t to uint8x8_t */
  tmp = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  res = vqmovun_s16(tmp);

  return res;
}

static INLINE uint16x8_t wiener_convolve8_horiz_8x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, int16_t *filter_x, const int bd,
    const int round0_bits) {
  int16x8_t sum;
  uint16x8_t res;
  int32x4_t sum_0, sum_1;
  int32x4_t s3_0, s3_1;
  const int32_t round_const_0 = (1 << (bd + FILTER_BITS - 1));
  const int32_t round_const_1 = (1 << (bd + 1 + FILTER_BITS - round0_bits)) - 1;

  /* for the purpose of right shift by { conv_params->round_0 } */
  const int32x4_t round_bits = vdupq_n_s32(-round0_bits);

  const int32x4_t round_vec_0 = vdupq_n_s32(round_const_0);
  const int32x4_t round_vec_1 = vdupq_n_s32(round_const_1);
  const int16x4_t filter = vld1_s16(filter_x);

  sum = vmulq_lane_s16(s0, filter, 0);
  sum = vmlaq_lane_s16(sum, s1, filter, 1);
  sum = vmlaq_lane_s16(sum, s2, filter, 2);

  /* sum from 16x8 to 2 32x4 registers */
  sum_0 = vmovl_s16(vget_low_s16(sum));
  sum_1 = vmovl_s16(vget_high_s16(sum));

  /* s[3]*128 -- and filter coef max can be 128
   *  then max value possible = 128*128*255 exceeding 16 bit
   */

  s3_0 = vmull_lane_s16(vget_low_s16(s3), filter, 3);
  s3_1 = vmull_lane_s16(vget_high_s16(s3), filter, 3);
  sum_0 = vaddq_s32(sum_0, s3_0);
  sum_1 = vaddq_s32(sum_1, s3_1);

  /* Add the constant value */
  sum_0 = vaddq_s32(sum_0, round_vec_0);
  sum_1 = vaddq_s32(sum_1, round_vec_0);

  /* right shift & rounding & saturating */
  sum_0 = vqrshlq_s32(sum_0, round_bits);
  sum_1 = vqrshlq_s32(sum_1, round_bits);

  /* Clipping to max value */
  sum_0 = vminq_s32(sum_0, round_vec_1);
  sum_1 = vminq_s32(sum_1, round_vec_1);

  res = vcombine_u16(vqmovun_s32(sum_0), vqmovun_s32(sum_1));
  return res;
}

static INLINE uint16x4_t wiener_convolve8_horiz_4x8(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, int16_t *filter_x, const int bd,
    const int round0_bits) {
  uint16x4_t res;
  int32x4_t sum_0, s3_0;
  int16x4_t sum, temp0, temp1, temp2;

  const int32_t round_const_0 = (1 << (bd + FILTER_BITS - 1));
  const int32_t round_const_1 = (1 << (bd + 1 + FILTER_BITS - round0_bits)) - 1;
  const int32x4_t round_bits = vdupq_n_s32(-round0_bits);
  const int32x4_t round_vec_0 = vdupq_n_s32(round_const_0);
  const int32x4_t round_vec_1 = vdupq_n_s32(round_const_1);
  const int16x4_t filter = vld1_s16(filter_x);

  temp0 = vadd_s16(s0, s6);
  temp1 = vadd_s16(s1, s5);
  temp2 = vadd_s16(s2, s4);

  sum = vmul_lane_s16(temp0, filter, 0);
  sum = vmla_lane_s16(sum, temp1, filter, 1);
  sum = vmla_lane_s16(sum, temp2, filter, 2);
  sum_0 = vmovl_s16(sum);

  /* s[3]*128 -- and filter coff max can be 128.
   * then max value possible = 128*128*255 Therefore, 32 bits are required to
   * hold the result.
   */
  s3_0 = vmull_lane_s16(s3, filter, 3);
  sum_0 = vaddq_s32(sum_0, s3_0);

  sum_0 = vaddq_s32(sum_0, round_vec_0);
  sum_0 = vrshlq_s32(sum_0, round_bits);

  sum_0 = vminq_s32(sum_0, round_vec_1);
  res = vqmovun_s32(sum_0);
  return res;
}

static INLINE int16x8_t convolve8_8x8_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t filter,
    const int16x8_t horiz_const, const int16x8_t shift_round_0) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x8_t sum;

  sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  sum = vqrshlq_s16(sum, shift_round_0);

  return sum;
}

// clang versions < 16 did not include the dotprod feature for Arm architecture
// versions that should have it by default, e.g., armv8.6-a.
#if defined(__aarch64__) && \
    (defined(__ARM_FEATURE_DOTPROD) || defined(__ARM_FEATURE_MATMUL_INT8))

DECLARE_ALIGNED(16, static const uint8_t, dot_prod_permute_tbl[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

#if defined(__aarch64__) && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int16x8_t convolve8_x_8_usdot(uint8x16_t samples,
                                            const int8x8_t filters,
                                            const uint8x16x3_t permute_tbl,
                                            const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum[2];

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  /* First 4 output values. */
  sum[0] = vusdotq_lane_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum[1] = vusdotq_lane_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  return vcombine_s16(vmovn_s32(sum[0]), vmovn_s32(sum[1]));
}

static INLINE int16x8_t convolve8_horiz_8_usdot(uint8x16_t samples,
                                                const int8x8_t filters,
                                                const uint8x16x3_t permute_tbl,
                                                const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum[2];

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  /* First 4 output values. */
  sum[0] = vusdotq_lane_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum[1] = vusdotq_lane_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static INLINE int32x4_t convolve8_4_usdot(uint8x16_t samples,
                                          const int8x8_t filters,
                                          const uint8x16x2_t permute_tbl,
                                          const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[2];
  int32x4_t sum;

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);

  /* First 4 output values. */
  sum = vusdotq_lane_s32(horiz_const, permuted_samples[0], filters, 0);
  sum = vusdotq_lane_s32(sum, permuted_samples[1], filters, 1);

  /* Narrowing and packing is performed by the caller. */
  return sum;
}

#elif defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE int16x8_t convolve8_horiz_8_sdot(uint8x16_t samples,
                                               const int8x8_t filters,
                                               const int32x4_t correction,
                                               const uint8x16_t range_limit,
                                               const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum[2];

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
  sum[0] = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum[1] = vdotq_lane_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  /* We halved the convolution filter values so -1 from the right shift. */
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static INLINE int32x4_t convolve8_4_sdot(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x2_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[2];
  int32x4_t sum;

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  sum = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum = vdotq_lane_s32(sum, permuted_samples[1], filters, 1);

  /* Narrowing and packing is performed by the caller. */
  return sum;
}

static INLINE int16x8_t convolve8_8_sdot(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x3_t permute_tbl,
                                         const int16x8_t shift_round_0) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum0, sum1;
  int16x8_t sum;

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
  sum0 = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum0 = vdotq_lane_s32(sum0, permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum1 = vdotq_lane_s32(correction, permuted_samples[1], filters, 0);
  sum1 = vdotq_lane_s32(sum1, permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  sum = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  return vqrshlq_s16(sum, shift_round_0);
}

static INLINE int16x8_t convolve8_x_8_sdot(uint8x16_t samples,
                                           const int8x8_t filters,
                                           const int32x4_t correction,
                                           const uint8x16_t range_limit,
                                           const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum[2];

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
  sum[0] = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum[1] = vdotq_lane_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  return vcombine_s16(vmovn_s32(sum[0]), vmovn_s32(sum[1]));
}

#endif  // defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)

static INLINE int16x4_t convolve8_4x4_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t filter,
    const int16x4_t horiz_const, const int16x4_t shift_round_0) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x4_t sum;

  sum = horiz_const;
  sum = vmla_lane_s16(sum, s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  sum = vqrshl_s16(sum, shift_round_0);

  return sum;
}

static INLINE int16x4_t convolve6_4x4(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x8_t y_filter_0_7) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x4_t sum;

  // Filter values at indices 0 and 7 are 0.
  sum = vmul_lane_s16(s0, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmla_lane_s16(sum, s5, y_filter_4_7, 2);

  return sum;
}

static INLINE int16x8_t convolve6_8x4(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t y_filters) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filters);
  const int16x4_t y_filter_hi = vget_high_s16(y_filters);
  int16x8_t sum;

  // Filter values at indices 0 and 7 are 0.
  sum = vmulq_lane_s16(s0, y_filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s5, y_filter_hi, 2);

  return sum;
}

#if !(defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD))

static INLINE int16x4_t convolve8_horiz_4x4_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t filter,
    const int16x4_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x4_t sum;

  sum = horiz_const;
  sum = vmla_lane_s16(sum, s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vshr_n_s16(sum, ROUND0_BITS - 1);
}

static INLINE int16x8_t convolve8_horiz_8x8_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t filter,
    const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);
  int16x8_t sum;

  sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrq_n_s16(sum, ROUND0_BITS - 1);
}

#endif  // !(defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD))

#endif  // AOM_AV1_COMMON_ARM_CONVOLVE_NEON_H_
