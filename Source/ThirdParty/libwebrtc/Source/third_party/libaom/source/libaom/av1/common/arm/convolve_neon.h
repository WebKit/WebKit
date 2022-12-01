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
                                    const int16x8_t filters,
                                    const int16x4_t filter3,
                                    const int16x4_t filter4) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, filters_lo, 0);
  sum = vmla_lane_s16(sum, s1, filters_lo, 1);
  sum = vmla_lane_s16(sum, s2, filters_lo, 2);
  sum = vmla_lane_s16(sum, s5, filters_hi, 1);
  sum = vmla_lane_s16(sum, s6, filters_hi, 2);
  sum = vmla_lane_s16(sum, s7, filters_hi, 3);
  sum = vqadd_s16(sum, vmul_s16(s3, filter3));
  sum = vqadd_s16(sum, vmul_s16(s4, filter4));
  return sum;
}

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16x8_t filters,
                                    const int16x8_t filter3,
                                    const int16x8_t filter4) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, filters_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filters_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filters_lo, 2);
  sum = vmlaq_lane_s16(sum, s5, filters_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filters_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filters_hi, 3);
  sum = vqaddq_s16(sum, vmulq_s16(s3, filter3));
  sum = vqaddq_s16(sum, vmulq_s16(s4, filter4));
  return vqrshrun_n_s16(sum, 7);
}

static INLINE uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filters) {
  const int16x8_t filter3 = vdupq_lane_s16(vget_low_s16(filters), 3);
  const int16x8_t filter4 = vdupq_lane_s16(vget_high_s16(filters), 0);
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
                     filters, filter3, filter4);
}

static INLINE uint8x8_t wiener_convolve8_vert_4x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, int16_t *filter_y, const int bd,
    const int round1_bits) {
  int16x8_t ss0, ss1, ss2;
  int32x4_t sum0, sum1;
  uint16x4_t tmp0, tmp1;
  uint16x8_t tmp;
  uint8x8_t res;

  const int32_t round_const = (1 << (bd + round1_bits - 1));
  const int32x4_t round_bits = vdupq_n_s32(-round1_bits);
  const int32x4_t zero = vdupq_n_s32(0);
  const int32x4_t round_vec = vdupq_n_s32(round_const);

  ss0 = vaddq_s16(s0, s6);
  ss1 = vaddq_s16(s1, s5);
  ss2 = vaddq_s16(s2, s4);

  sum0 = vmull_n_s16(vget_low_s16(ss0), filter_y[0]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(ss1), filter_y[1]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(ss2), filter_y[2]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s3), filter_y[3]);

  sum1 = vmull_n_s16(vget_high_s16(ss0), filter_y[0]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(ss1), filter_y[1]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(ss2), filter_y[2]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s3), filter_y[3]);

  sum0 = vsubq_s32(sum0, round_vec);
  sum1 = vsubq_s32(sum1, round_vec);

  /* right shift & rounding */
  sum0 = vrshlq_s32(sum0, round_bits);
  sum1 = vrshlq_s32(sum1, round_bits);

  sum0 = vmaxq_s32(sum0, zero);
  sum1 = vmaxq_s32(sum1, zero);

  /* from int32x4_t to uint8x8_t */
  tmp0 = vqmovn_u32(vreinterpretq_u32_s32(sum0));
  tmp1 = vqmovn_u32(vreinterpretq_u32_s32(sum1));
  tmp = vcombine_u16(tmp0, tmp1);
  res = vqmovn_u16(tmp);

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

  sum = vmulq_n_s16(s0, filter_x[0]);
  sum = vmlaq_n_s16(sum, s1, filter_x[1]);
  sum = vmlaq_n_s16(sum, s2, filter_x[2]);

  /* sum from 16x8 to 2 32x4 registers */
  sum_0 = vmovl_s16(vget_low_s16(sum));
  sum_1 = vmovl_s16(vget_high_s16(sum));

  /* s[3]*128 -- and filter coef max can be 128
   *  then max value possible = 128*128*255 exceeding 16 bit
   */

  s3_0 = vmull_n_s16(vget_low_s16(s3), filter_x[3]);
  s3_1 = vmull_n_s16(vget_high_s16(s3), filter_x[3]);
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
  const int32x4_t zero = vdupq_n_s32(0);
  const int32x4_t round_vec_0 = vdupq_n_s32(round_const_0);
  const int32x4_t round_vec_1 = vdupq_n_s32(round_const_1);

  temp0 = vadd_s16(s0, s6);
  temp1 = vadd_s16(s1, s5);
  temp2 = vadd_s16(s2, s4);

  sum = vmul_n_s16(temp0, filter_x[0]);
  sum = vmla_n_s16(sum, temp1, filter_x[1]);
  sum = vmla_n_s16(sum, temp2, filter_x[2]);
  sum_0 = vmovl_s16(sum);

  /* s[3]*128 -- and filter coff max can be 128.
   * then max value possible = 128*128*255 Therefore, 32 bits are required to
   * hold the result.
   */
  s3_0 = vmull_n_s16(s3, filter_x[3]);
  sum_0 = vaddq_s32(sum_0, s3_0);

  sum_0 = vaddq_s32(sum_0, round_vec_0);
  sum_0 = vrshlq_s32(sum_0, round_bits);

  sum_0 = vmaxq_s32(sum_0, zero);
  sum_0 = vminq_s32(sum_0, round_vec_1);
  res = vqmovun_s32(sum_0);
  return res;
}

static INLINE int16x8_t
convolve8_8x8_s16(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                  const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                  const int16x8_t s6, const int16x8_t s7, const int16_t *filter,
                  const int16x8_t horiz_const, const int16x8_t shift_round_0) {
  int16x8_t sum;
  int16x8_t res;

  sum = horiz_const;
  sum = vmlaq_n_s16(sum, s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s3, filter[3]);
  sum = vmlaq_n_s16(sum, s4, filter[4]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);

  res = vqrshlq_s16(sum, shift_round_0);

  return res;
}

static INLINE int16x4_t
convolve8_4x4_s16(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                  const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                  const int16x4_t s6, const int16x4_t s7, const int16_t *filter,
                  const int16x4_t horiz_const, const int16x4_t shift_round_0) {
  int16x4_t sum;
  sum = horiz_const;
  sum = vmla_n_s16(sum, s0, filter[0]);
  sum = vmla_n_s16(sum, s1, filter[1]);
  sum = vmla_n_s16(sum, s2, filter[2]);
  sum = vmla_n_s16(sum, s3, filter[3]);
  sum = vmla_n_s16(sum, s4, filter[4]);
  sum = vmla_n_s16(sum, s5, filter[5]);
  sum = vmla_n_s16(sum, s6, filter[6]);
  sum = vmla_n_s16(sum, s7, filter[7]);

  sum = vqrshl_s16(sum, shift_round_0);

  return sum;
}

static INLINE uint16x4_t convolve8_4x4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16_t *y_filter,
    const int32x4_t round_shift_vec, const int32x4_t offset_const) {
  int32x4_t sum0;
  uint16x4_t res;
  const int32x4_t zero = vdupq_n_s32(0);

  sum0 = vmull_n_s16(s0, y_filter[0]);
  sum0 = vmlal_n_s16(sum0, s1, y_filter[1]);
  sum0 = vmlal_n_s16(sum0, s2, y_filter[2]);
  sum0 = vmlal_n_s16(sum0, s3, y_filter[3]);
  sum0 = vmlal_n_s16(sum0, s4, y_filter[4]);
  sum0 = vmlal_n_s16(sum0, s5, y_filter[5]);
  sum0 = vmlal_n_s16(sum0, s6, y_filter[6]);
  sum0 = vmlal_n_s16(sum0, s7, y_filter[7]);

  sum0 = vaddq_s32(sum0, offset_const);
  sum0 = vqrshlq_s32(sum0, round_shift_vec);
  sum0 = vmaxq_s32(sum0, zero);
  res = vmovn_u32(vreinterpretq_u32_s32(sum0));

  return res;
}

#endif  // AOM_AV1_COMMON_ARM_CONVOLVE_NEON_H_
