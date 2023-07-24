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

#ifndef AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_NEON_H_
#define AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_NEON_H_

#include <arm_neon.h>

static INLINE int32x4_t highbd_convolve6_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t y_filter, const int32x4_t offset) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 2);

  return sum;
}

static INLINE uint16x4_t highbd_convolve6_4_s32_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t y_filter, const int32x4_t offset) {
  int32x4_t sum =
      highbd_convolve6_4_s32(s0, s1, s2, s3, s4, s5, y_filter, offset);

  return vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
}

static INLINE void highbd_convolve6_8_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t y_filter, const int32x4_t offset, int32x4_t *sum0,
    int32x4_t *sum1) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  *sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_lo, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s1), y_filter_lo, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s2), y_filter_lo, 3);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s3), y_filter_hi, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s4), y_filter_hi, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s5), y_filter_hi, 2);

  *sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_lo, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s1), y_filter_lo, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s2), y_filter_lo, 3);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s3), y_filter_hi, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s4), y_filter_hi, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s5), y_filter_hi, 2);
}

static INLINE uint16x8_t highbd_convolve6_8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t y_filter, const int32x4_t offset) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve6_8_s32(s0, s1, s2, s3, s4, s5, y_filter, offset, &sum0,
                         &sum1);

  return vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                      vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
}

static INLINE int32x4_t highbd_convolve8_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const int32x4_t offset) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_lo, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_hi, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_hi, 3);

  return sum;
}

static INLINE uint16x4_t highbd_convolve8_4_s32_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const int32x4_t offset) {
  int32x4_t sum =
      highbd_convolve8_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, offset);

  return vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
}

// Like above but also perform round shifting and subtract correction term
static INLINE uint16x4_t highbd_convolve8_4_rs_s32_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const int32x4_t round_shift, const int32x4_t offset,
    const int32x4_t correction) {
  int32x4_t sum =
      highbd_convolve8_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, offset);

  sum = vsubq_s32(vqrshlq_s32(sum, round_shift), correction);
  return vqmovun_s32(sum);
}

static INLINE void highbd_convolve8_8_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const int32x4_t offset, int32x4_t *sum0, int32x4_t *sum1) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  *sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_lo, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s1), y_filter_lo, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s2), y_filter_lo, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s3), y_filter_lo, 3);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s4), y_filter_hi, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s5), y_filter_hi, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s6), y_filter_hi, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s7), y_filter_hi, 3);

  *sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_lo, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s1), y_filter_lo, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s2), y_filter_lo, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s3), y_filter_lo, 3);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s4), y_filter_hi, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s5), y_filter_hi, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s6), y_filter_hi, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s7), y_filter_hi, 3);
}

static INLINE uint16x8_t highbd_convolve8_8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const int32x4_t offset) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve8_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, offset,
                         &sum0, &sum1);

  return vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                      vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
}

// Like above but also perform round shifting and subtract correction term
static INLINE uint16x8_t highbd_convolve8_8_rs_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const int32x4_t round_shift, const int32x4_t offset,
    const int32x4_t correction) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve8_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, offset,
                         &sum0, &sum1);

  sum0 = vsubq_s32(vqrshlq_s32(sum0, round_shift), correction);
  sum1 = vsubq_s32(vqrshlq_s32(sum1, round_shift), correction);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static INLINE int32x4_t highbd_convolve12_y_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t offset) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_0_3, 0);
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

static INLINE uint16x4_t highbd_convolve12_y_4_s32_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t offset) {
  int32x4_t sum =
      highbd_convolve12_y_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, y_filter_0_7, y_filter_8_11, offset);

  return vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
}

// Like above but also perform round shifting and subtract correction term
static INLINE uint16x4_t highbd_convolve12_y_4_rs_s32_s16(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t round_shift, const int32x4_t offset,
    const int32x4_t correction) {
  int32x4_t sum =
      highbd_convolve12_y_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, y_filter_0_7, y_filter_8_11, offset);

  sum = vsubq_s32(vqrshlq_s32(sum, round_shift), correction);
  return vqmovun_s32(sum);
}

static INLINE void highbd_convolve12_y_8_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t offset, int32x4_t *sum0, int32x4_t *sum1) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  *sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_0_3, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s1), y_filter_0_3, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s2), y_filter_0_3, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s3), y_filter_0_3, 3);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s4), y_filter_4_7, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s5), y_filter_4_7, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s6), y_filter_4_7, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s7), y_filter_4_7, 3);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s8), y_filter_8_11, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s9), y_filter_8_11, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s10), y_filter_8_11, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s11), y_filter_8_11, 3);

  *sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_0_3, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s1), y_filter_0_3, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s2), y_filter_0_3, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s3), y_filter_0_3, 3);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s4), y_filter_4_7, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s5), y_filter_4_7, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s6), y_filter_4_7, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s7), y_filter_4_7, 3);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s8), y_filter_8_11, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s9), y_filter_8_11, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s10), y_filter_8_11, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s11), y_filter_8_11, 3);
}

static INLINE uint16x8_t highbd_convolve12_y_8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t offset) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve12_y_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            y_filter_0_7, y_filter_8_11, offset, &sum0, &sum1);

  return vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                      vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
}

// Like above but also perform round shifting and subtract correction term
static INLINE uint16x8_t highbd_convolve12_y_8_rs_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t round_shift, const int32x4_t offset,
    const int32x4_t correction) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve12_y_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            y_filter_0_7, y_filter_8_11, offset, &sum0, &sum1);

  sum0 = vsubq_s32(vqrshlq_s32(sum0, round_shift), correction);
  sum1 = vsubq_s32(vqrshlq_s32(sum1, round_shift), correction);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static INLINE int32x4_t highbd_convolve8_horiz4_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7,
    const int32x4_t offset) {
  const int16x8_t s2 = vextq_s16(s0, s1, 1);
  const int16x8_t s3 = vextq_s16(s0, s1, 2);
  const int16x8_t s4 = vextq_s16(s0, s1, 3);
  const int16x4_t s0_lo = vget_low_s16(s0);
  const int16x4_t s1_lo = vget_low_s16(s2);
  const int16x4_t s2_lo = vget_low_s16(s3);
  const int16x4_t s3_lo = vget_low_s16(s4);
  const int16x4_t s4_lo = vget_high_s16(s0);
  const int16x4_t s5_lo = vget_high_s16(s2);
  const int16x4_t s6_lo = vget_high_s16(s3);
  const int16x4_t s7_lo = vget_high_s16(s4);

  return highbd_convolve8_4_s32(s0_lo, s1_lo, s2_lo, s3_lo, s4_lo, s5_lo, s6_lo,
                                s7_lo, x_filter_0_7, offset);
}

static INLINE uint16x4_t highbd_convolve8_horiz4_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7,
    const int32x4_t shift_s32, const int32x4_t offset) {
  int32x4_t sum = highbd_convolve8_horiz4_s32(s0, s1, x_filter_0_7, offset);

  sum = vqrshlq_s32(sum, shift_s32);
  return vqmovun_s32(sum);
}

static INLINE void highbd_convolve8_horiz8_s32(
    const int16x8_t s0, const int16x8_t s0_hi, const int16x8_t x_filter_0_7,
    const int32x4_t offset, int32x4_t *sum0, int32x4_t *sum1) {
  const int16x8_t s1 = vextq_s16(s0, s0_hi, 1);
  const int16x8_t s2 = vextq_s16(s0, s0_hi, 2);
  const int16x8_t s3 = vextq_s16(s0, s0_hi, 3);
  const int16x8_t s4 = vextq_s16(s0, s0_hi, 4);
  const int16x8_t s5 = vextq_s16(s0, s0_hi, 5);
  const int16x8_t s6 = vextq_s16(s0, s0_hi, 6);
  const int16x8_t s7 = vextq_s16(s0, s0_hi, 7);

  highbd_convolve8_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_0_7, offset,
                         sum0, sum1);
}

static INLINE uint16x8_t highbd_convolve8_horiz8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7,
    const int32x4_t shift_s32, const int32x4_t offset) {
  int32x4_t sum0, sum1;
  highbd_convolve8_horiz8_s32(s0, s1, x_filter_0_7, offset, &sum0, &sum1);

  sum0 = vqrshlq_s32(sum0, shift_s32);
  sum1 = vqrshlq_s32(sum1, shift_s32);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static INLINE int32x4_t highbd_convolve12_horiz4_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11, const int32x4_t offset) {
  const int16x8_t s2 = vextq_s16(s0, s1, 1);
  const int16x8_t s3 = vextq_s16(s0, s1, 2);
  const int16x8_t s4 = vextq_s16(s0, s1, 3);
  const int16x8_t s5 = vextq_s16(s0, s1, 4);
  const int16x8_t s6 = vextq_s16(s0, s1, 5);
  const int16x8_t s7 = vextq_s16(s0, s1, 6);
  const int16x8_t s8 = vextq_s16(s0, s1, 7);
  const int16x4_t s0_lo = vget_low_s16(s0);
  const int16x4_t s1_lo = vget_low_s16(s2);
  const int16x4_t s2_lo = vget_low_s16(s3);
  const int16x4_t s3_lo = vget_low_s16(s4);
  const int16x4_t s4_lo = vget_high_s16(s0);
  const int16x4_t s5_lo = vget_high_s16(s2);
  const int16x4_t s6_lo = vget_high_s16(s3);
  const int16x4_t s7_lo = vget_high_s16(s4);
  const int16x4_t s8_lo = vget_high_s16(s5);
  const int16x4_t s9_lo = vget_high_s16(s6);
  const int16x4_t s10_lo = vget_high_s16(s7);
  const int16x4_t s11_lo = vget_high_s16(s8);

  return highbd_convolve12_y_4_s32(s0_lo, s1_lo, s2_lo, s3_lo, s4_lo, s5_lo,
                                   s6_lo, s7_lo, s8_lo, s9_lo, s10_lo, s11_lo,
                                   x_filter_0_7, x_filter_8_11, offset);
}

static INLINE uint16x4_t highbd_convolve12_horiz4_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11, const int32x4_t shift_s32,
    const int32x4_t offset) {
  int32x4_t sum =
      highbd_convolve12_horiz4_s32(s0, s1, x_filter_0_7, x_filter_8_11, offset);

  sum = vqrshlq_s32(sum, shift_s32);
  return vqmovun_s32(sum);
}

static INLINE void highbd_convolve12_horiz8_s32(
    const int16x8_t s0_0, const int16x8_t s0_1, const int16x8_t s0_2,
    const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
    const int32x4_t offset, int32x4_t *sum0, int32x4_t *sum1) {
  const int16x8_t s1 = vextq_s16(s0_0, s0_1, 1);
  const int16x8_t s2 = vextq_s16(s0_0, s0_1, 2);
  const int16x8_t s3 = vextq_s16(s0_0, s0_1, 3);
  const int16x8_t s4 = vextq_s16(s0_0, s0_1, 4);
  const int16x8_t s5 = vextq_s16(s0_0, s0_1, 5);
  const int16x8_t s6 = vextq_s16(s0_0, s0_1, 6);
  const int16x8_t s7 = vextq_s16(s0_0, s0_1, 7);
  const int16x8_t s8 = s0_1;
  const int16x8_t s9 = vextq_s16(s0_1, s0_2, 1);
  const int16x8_t s10 = vextq_s16(s0_1, s0_2, 2);
  const int16x8_t s11 = vextq_s16(s0_1, s0_2, 3);

  highbd_convolve12_y_8_s32(s0_0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            x_filter_0_7, x_filter_8_11, offset, sum0, sum1);
}

static INLINE uint16x8_t highbd_convolve12_horiz8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
    const int32x4_t shift_s32, const int32x4_t offset) {
  int32x4_t sum0, sum1;
  highbd_convolve12_horiz8_s32(s0, s1, s2, x_filter_0_7, x_filter_8_11, offset,
                               &sum0, &sum1);

  sum0 = vqrshlq_s32(sum0, shift_s32);
  sum1 = vqrshlq_s32(sum1, shift_s32);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

#endif  // AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_NEON_H_
