/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/mem_neon.h"
#include "config/aom_dsp_rtcd.h"

static INLINE uint32x4_t sum_squares_i16_4x4_neon(const int16_t *src,
                                                  int stride) {
  const int16x4_t v_val_01_lo = vld1_s16(src + 0 * stride);
  const int16x4_t v_val_01_hi = vld1_s16(src + 1 * stride);
  const int16x4_t v_val_23_lo = vld1_s16(src + 2 * stride);
  const int16x4_t v_val_23_hi = vld1_s16(src + 3 * stride);
  int32x4_t v_sq_01_d = vmull_s16(v_val_01_lo, v_val_01_lo);
  v_sq_01_d = vmlal_s16(v_sq_01_d, v_val_01_hi, v_val_01_hi);
  int32x4_t v_sq_23_d = vmull_s16(v_val_23_lo, v_val_23_lo);
  v_sq_23_d = vmlal_s16(v_sq_23_d, v_val_23_hi, v_val_23_hi);
#if defined(__aarch64__)
  return vreinterpretq_u32_s32(vpaddq_s32(v_sq_01_d, v_sq_23_d));
#else
  return vreinterpretq_u32_s32(vcombine_s32(
      vqmovn_s64(vpaddlq_s32(v_sq_01_d)), vqmovn_s64(vpaddlq_s32(v_sq_23_d))));
#endif
}

uint64_t aom_sum_squares_2d_i16_4x4_neon(const int16_t *src, int stride) {
  const uint32x4_t v_sum_0123_d = sum_squares_i16_4x4_neon(src, stride);
#if defined(__aarch64__)
  return (uint64_t)vaddvq_u32(v_sum_0123_d);
#else
  uint64x2_t v_sum_d = vpaddlq_u32(v_sum_0123_d);
  v_sum_d = vaddq_u64(v_sum_d, vextq_u64(v_sum_d, v_sum_d, 1));
  return vgetq_lane_u64(v_sum_d, 0);
#endif
}

uint64_t aom_sum_squares_2d_i16_4xn_neon(const int16_t *src, int stride,
                                         int height) {
  int r = 0;
  uint32x4_t v_acc_q = vdupq_n_u32(0);
  do {
    const uint32x4_t v_acc_d = sum_squares_i16_4x4_neon(src, stride);
    v_acc_q = vaddq_u32(v_acc_q, v_acc_d);
    src += stride << 2;
    r += 4;
  } while (r < height);

  uint64x2_t v_acc_64 = vpaddlq_u32(v_acc_q);
#if defined(__aarch64__)
  return vaddvq_u64(v_acc_64);
#else
  v_acc_64 = vaddq_u64(v_acc_64, vextq_u64(v_acc_64, v_acc_64, 1));
  return vgetq_lane_u64(v_acc_64, 0);
#endif
}

uint64_t aom_sum_squares_2d_i16_nxn_neon(const int16_t *src, int stride,
                                         int width, int height) {
  int r = 0;
  const int32x4_t zero = vdupq_n_s32(0);
  uint64x2_t v_acc_q = vreinterpretq_u64_s32(zero);
  do {
    int32x4_t v_sum = zero;
    int c = 0;
    do {
      const int16_t *b = src + c;
      const int16x8_t v_val_0 = vld1q_s16(b + 0 * stride);
      const int16x8_t v_val_1 = vld1q_s16(b + 1 * stride);
      const int16x8_t v_val_2 = vld1q_s16(b + 2 * stride);
      const int16x8_t v_val_3 = vld1q_s16(b + 3 * stride);
      const int16x4_t v_val_0_lo = vget_low_s16(v_val_0);
      const int16x4_t v_val_1_lo = vget_low_s16(v_val_1);
      const int16x4_t v_val_2_lo = vget_low_s16(v_val_2);
      const int16x4_t v_val_3_lo = vget_low_s16(v_val_3);
      int32x4_t v_sum_01 = vmull_s16(v_val_0_lo, v_val_0_lo);
      v_sum_01 = vmlal_s16(v_sum_01, v_val_1_lo, v_val_1_lo);
      int32x4_t v_sum_23 = vmull_s16(v_val_2_lo, v_val_2_lo);
      v_sum_23 = vmlal_s16(v_sum_23, v_val_3_lo, v_val_3_lo);
#if defined(__aarch64__)
      v_sum_01 = vmlal_high_s16(v_sum_01, v_val_0, v_val_0);
      v_sum_01 = vmlal_high_s16(v_sum_01, v_val_1, v_val_1);
      v_sum_23 = vmlal_high_s16(v_sum_23, v_val_2, v_val_2);
      v_sum_23 = vmlal_high_s16(v_sum_23, v_val_3, v_val_3);
      v_sum = vaddq_s32(v_sum, vpaddq_s32(v_sum_01, v_sum_23));
#else
      const int16x4_t v_val_0_hi = vget_high_s16(v_val_0);
      const int16x4_t v_val_1_hi = vget_high_s16(v_val_1);
      const int16x4_t v_val_2_hi = vget_high_s16(v_val_2);
      const int16x4_t v_val_3_hi = vget_high_s16(v_val_3);
      v_sum_01 = vmlal_s16(v_sum_01, v_val_0_hi, v_val_0_hi);
      v_sum_01 = vmlal_s16(v_sum_01, v_val_1_hi, v_val_1_hi);
      v_sum_23 = vmlal_s16(v_sum_23, v_val_2_hi, v_val_2_hi);
      v_sum_23 = vmlal_s16(v_sum_23, v_val_3_hi, v_val_3_hi);
      v_sum = vaddq_s32(v_sum, vcombine_s32(vqmovn_s64(vpaddlq_s32(v_sum_01)),
                                            vqmovn_s64(vpaddlq_s32(v_sum_23))));
#endif
      c += 8;
    } while (c < width);

    v_acc_q = vpadalq_u32(v_acc_q, vreinterpretq_u32_s32(v_sum));

    src += 4 * stride;
    r += 4;
  } while (r < height);
#if defined(__aarch64__)
  return vaddvq_u64(v_acc_q);
#else
  v_acc_q = vaddq_u64(v_acc_q, vextq_u64(v_acc_q, v_acc_q, 1));
  return vgetq_lane_u64(v_acc_q, 0);
#endif
}

uint64_t aom_sum_squares_2d_i16_neon(const int16_t *src, int stride, int width,
                                     int height) {
  // 4 elements per row only requires half an SIMD register, so this
  // must be a special case, but also note that over 75% of all calls
  // are with size == 4, so it is also the common case.
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_neon(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_neon(src, stride, height);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case
    return aom_sum_squares_2d_i16_nxn_neon(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}
