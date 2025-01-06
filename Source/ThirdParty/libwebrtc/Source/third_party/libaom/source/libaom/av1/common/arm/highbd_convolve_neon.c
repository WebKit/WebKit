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

#include <assert.h>
#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"

static inline uint16x4_t highbd_convolve6_4_y(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t y_filter, const uint16x4_t max) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 2);

  uint16x4_t res = vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve6_8_y(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t y_filter, const uint16x8_t max) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 2);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 2);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                                vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_y_sr_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int bd) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)(src_ptr + src_stride);
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      uint16x4_t d0 =
          highbd_convolve6_4_y(s0, s1, s2, s3, s4, s5, y_filter_0_7, max);
      uint16x4_t d1 =
          highbd_convolve6_4_y(s1, s2, s3, s4, s5, s6, y_filter_0_7, max);
      uint16x4_t d2 =
          highbd_convolve6_4_y(s2, s3, s4, s5, s6, s7, y_filter_0_7, max);
      uint16x4_t d3 =
          highbd_convolve6_4_y(s3, s4, s5, s6, s7, s8, y_filter_0_7, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    // Width is a multiple of 8 and height is a multiple of 4.
    do {
      int height = h;
      const int16_t *s = (const int16_t *)(src_ptr + src_stride);
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint16x8_t d0 =
            highbd_convolve6_8_y(s0, s1, s2, s3, s4, s5, y_filter_0_7, max);
        uint16x8_t d1 =
            highbd_convolve6_8_y(s1, s2, s3, s4, s5, s6, y_filter_0_7, max);
        uint16x8_t d2 =
            highbd_convolve6_8_y(s2, s3, s4, s5, s6, s7, y_filter_0_7, max);
        uint16x8_t d3 =
            highbd_convolve6_8_y(s3, s4, s5, s6, s7, s8, y_filter_0_7, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline uint16x4_t highbd_convolve8_4_y(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const uint16x4_t max) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_4_7, 3);

  uint16x4_t res = vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_y(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const uint16x8_t max) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_4_7, 3);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_4_7, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                                vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_y_sr_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int bd) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 =
          highbd_convolve8_4_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, max);
      uint16x4_t d1 =
          highbd_convolve8_4_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter, max);
      uint16x4_t d2 =
          highbd_convolve8_4_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter, max);
      uint16x4_t d3 =
          highbd_convolve8_4_y(s3, s4, s5, s6, s7, s8, s9, s10, y_filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

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
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 =
            highbd_convolve8_8_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, max);
        uint16x8_t d1 =
            highbd_convolve8_8_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter, max);
        uint16x8_t d2 =
            highbd_convolve8_8_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter, max);
        uint16x8_t d3 = highbd_convolve8_8_y(s3, s4, s5, s6, s7, s8, s9, s10,
                                             y_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline uint16x4_t highbd_convolve12_4_y(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const uint16x4_t max) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_0_3, 0);
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

  uint16x4_t res = vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve12_8_y(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const uint16x8_t max) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_0_3, 0);
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

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_0_3, 0);
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

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                                vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_y_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int bd) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &s10);
    s += 11 * src_stride;

    do {
      int16x4_t s11, s12, s13, s14;
      load_s16_4x4(s, src_stride, &s11, &s12, &s13, &s14);

      uint16x4_t d0 =
          highbd_convolve12_4_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, y_filter_0_7, y_filter_8_11, max);
      uint16x4_t d1 =
          highbd_convolve12_4_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                s12, y_filter_0_7, y_filter_8_11, max);
      uint16x4_t d2 =
          highbd_convolve12_4_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                s13, y_filter_0_7, y_filter_8_11, max);
      uint16x4_t d3 =
          highbd_convolve12_4_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                                s14, y_filter_0_7, y_filter_8_11, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

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
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        int16x8_t s11, s12, s13, s14;
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        uint16x8_t d0 =
            highbd_convolve12_8_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                  s11, y_filter_0_7, y_filter_8_11, max);
        uint16x8_t d1 =
            highbd_convolve12_8_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                  s12, y_filter_0_7, y_filter_8_11, max);
        uint16x8_t d2 =
            highbd_convolve12_8_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                  s13, y_filter_0_7, y_filter_8_11, max);
        uint16x8_t d3 =
            highbd_convolve12_8_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                  s13, s14, y_filter_0_7, y_filter_8_11, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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
      } while (height != 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

void av1_highbd_convolve_y_sr_neon(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_y_qn, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_y, subpel_y_qn, bd);
    return;
  }
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_offset * src_stride;

  if (y_filter_taps > 8) {
    highbd_convolve_y_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                                    y_filter_ptr, bd);
    return;
  }
  if (y_filter_taps < 8) {
    highbd_convolve_y_sr_6tap_neon(src, src_stride, dst, dst_stride, w, h,
                                   y_filter_ptr, bd);
    return;
  }

  highbd_convolve_y_sr_8tap_neon(src, src_stride, dst, dst_stride, w, h,
                                 y_filter_ptr, bd);
}

static inline uint16x8_t highbd_convolve6_8_x(const int16x8_t s[6],
                                              const int16x8_t x_filter,
                                              const int32x4_t offset,
                                              const uint16x8_t max) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter);

  int32x4_t sum0 = offset;
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[0]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 2);

  int32x4_t sum1 = offset;
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[0]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 2);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                                vqrshrun_n_s32(sum1, FILTER_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    int bd) {
  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  // This shim allows to do only one rounding shift instead of two.
  const int32x4_t offset = vdupq_n_s32(1 << (conv_params->round_0 - 1));

  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 = highbd_convolve6_8_x(s0, x_filter, offset, max);
      uint16x8_t d1 = highbd_convolve6_8_x(s1, x_filter, offset, max);
      uint16x8_t d2 = highbd_convolve6_8_x(s2, x_filter, offset, max);
      uint16x8_t d3 = highbd_convolve6_8_x(s3, x_filter, offset, max);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);

    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height != 0);
}

static inline uint16x4_t highbd_convolve4_4_x(const int16x4_t s[4],
                                              const int16x4_t x_filter,
                                              const int32x4_t offset,
                                              const uint16x4_t max) {
  int32x4_t sum = offset;
  sum = vmlal_lane_s16(sum, s[0], x_filter, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter, 3);

  uint16x4_t res = vqrshrun_n_s32(sum, FILTER_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_x(const int16x8_t s[8],
                                              const int16x8_t x_filter,
                                              const int32x4_t offset,
                                              const uint16x8_t max) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter);

  int32x4_t sum0 = offset;
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[0]), x_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[6]), x_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[7]), x_filter_4_7, 3);

  int32x4_t sum1 = offset;
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[0]), x_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[6]), x_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[7]), x_filter_4_7, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                                vqrshrun_n_s32(sum1, FILTER_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_neon(const uint16_t *src_ptr,
                                             int src_stride, uint16_t *dst_ptr,
                                             int dst_stride, int w, int h,
                                             const int16_t *x_filter_ptr,
                                             ConvolveParams *conv_params,
                                             int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int32x4_t offset = vdupq_n_s32(1 << (conv_params->round_0 - 1));

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    // 4-tap filters are used for blocks having width == 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 2);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve4_4_x(s0, x_filter, offset, max);
      uint16x4_t d1 = highbd_convolve4_4_x(s1, x_filter, offset, max);
      uint16x4_t d2 = highbd_convolve4_4_x(s2, x_filter, offset, max);
      uint16x4_t d3 = highbd_convolve4_4_x(s3, x_filter, offset, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8_x(s0, x_filter, offset, max);
        uint16x8_t d1 = highbd_convolve8_8_x(s1, x_filter, offset, max);
        uint16x8_t d2 = highbd_convolve8_8_x(s2, x_filter, offset, max);
        uint16x8_t d3 = highbd_convolve8_8_x(s3, x_filter, offset, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline uint16x4_t highbd_convolve12_4_x(const int16x4_t s[12],
                                               const int16x8_t x_filter_0_7,
                                               const int16x4_t x_filter_8_11,
                                               const int32x4_t offset,
                                               const uint16x4_t max) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = offset;
  sum = vmlal_lane_s16(sum, s[0], x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s[4], x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s[5], x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s[6], x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s[7], x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s[8], x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s[9], x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s[10], x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s[11], x_filter_8_11, 3);

  uint16x4_t res = vqrshrun_n_s32(sum, FILTER_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve12_8_x(const int16x8_t s[12],
                                               const int16x8_t x_filter_0_7,
                                               const int16x4_t x_filter_8_11,
                                               const int32x4_t offset,
                                               const uint16x8_t max) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum0 = offset;
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[0]), x_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[6]), x_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[7]), x_filter_4_7, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[8]), x_filter_8_11, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[9]), x_filter_8_11, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[10]), x_filter_8_11, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[11]), x_filter_8_11, 3);

  int32x4_t sum1 = offset;
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[0]), x_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[6]), x_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[7]), x_filter_4_7, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[8]), x_filter_8_11, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[9]), x_filter_8_11, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[10]), x_filter_8_11, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[11]), x_filter_8_11, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                                vqrshrun_n_s32(sum1, FILTER_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int32x4_t offset = vdupq_n_s32(1 << (conv_params->round_0 - 1));
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[12], s1[12], s2[12], s3[12];
      load_s16_4x12(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                    &s0[4], &s0[5], &s0[6], &s0[7], &s0[8], &s0[9], &s0[10],
                    &s0[11]);
      load_s16_4x12(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                    &s1[4], &s1[5], &s1[6], &s1[7], &s1[8], &s1[9], &s1[10],
                    &s1[11]);
      load_s16_4x12(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                    &s2[4], &s2[5], &s2[6], &s2[7], &s2[8], &s2[9], &s2[10],
                    &s2[11]);
      load_s16_4x12(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                    &s3[4], &s3[5], &s3[6], &s3[7], &s3[8], &s3[9], &s3[10],
                    &s3[11]);

      uint16x4_t d0 =
          highbd_convolve12_4_x(s0, x_filter_0_7, x_filter_8_11, offset, max);
      uint16x4_t d1 =
          highbd_convolve12_4_x(s1, x_filter_0_7, x_filter_8_11, offset, max);
      uint16x4_t d2 =
          highbd_convolve12_4_x(s2, x_filter_0_7, x_filter_8_11, offset, max);
      uint16x4_t d3 =
          highbd_convolve12_4_x(s3, x_filter_0_7, x_filter_8_11, offset, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[12], s1[12], s2[12], s3[12];
        load_s16_8x12(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                      &s0[4], &s0[5], &s0[6], &s0[7], &s0[8], &s0[9], &s0[10],
                      &s0[11]);
        load_s16_8x12(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                      &s1[4], &s1[5], &s1[6], &s1[7], &s1[8], &s1[9], &s1[10],
                      &s1[11]);
        load_s16_8x12(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                      &s2[4], &s2[5], &s2[6], &s2[7], &s2[8], &s2[9], &s2[10],
                      &s2[11]);
        load_s16_8x12(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                      &s3[4], &s3[5], &s3[6], &s3[7], &s3[8], &s3[9], &s3[10],
                      &s3[11]);

        uint16x8_t d0 =
            highbd_convolve12_8_x(s0, x_filter_0_7, x_filter_8_11, offset, max);
        uint16x8_t d1 =
            highbd_convolve12_8_x(s1, x_filter_0_7, x_filter_8_11, offset, max);
        uint16x8_t d2 =
            highbd_convolve12_8_x(s2, x_filter_0_7, x_filter_8_11, offset, max);
        uint16x8_t d3 =
            highbd_convolve12_8_x(s3, x_filter_0_7, x_filter_8_11, offset, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_highbd_convolve_x_sr_neon(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const int subpel_x_qn,
                                   ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_x, subpel_x_qn, conv_params, bd);
    return;
  }
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_offset;

  if (x_filter_taps > 8) {
    highbd_convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                                    x_filter_ptr, conv_params, bd);
    return;
  }
  if (x_filter_taps <= 6 && w != 4) {
    highbd_convolve_x_sr_6tap_neon(src + 1, src_stride, dst, dst_stride, w, h,
                                   x_filter_ptr, conv_params, bd);
    return;
  }

  highbd_convolve_x_sr_neon(src, src_stride, dst, dst_stride, w, h,
                            x_filter_ptr, conv_params, bd);
}

static inline uint16x4_t highbd_convolve6_4_2d_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t y_filter, const int32x4_t round_shift,
    const int32x4_t offset, const uint16x4_t max) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 2);

  sum = vshlq_s32(sum, round_shift);
  uint16x4_t res = vqmovun_s32(sum);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve6_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t y_filter, const int32x4_t round_shift,
    const int32x4_t offset, const uint16x8_t max) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 2);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 2);

  sum0 = vshlq_s32(sum0, round_shift);
  sum1 = vshlq_s32(sum1, round_shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_2d_sr_vert_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    int bd, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);
  const int round1_shift = conv_params->round_1;
  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_shift);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      uint16x4_t d0 = highbd_convolve6_4_2d_v(
          s0, s1, s2, s3, s4, s5, y_filter, round1_shift_s32, offset_s32, max);
      uint16x4_t d1 = highbd_convolve6_4_2d_v(
          s1, s2, s3, s4, s5, s6, y_filter, round1_shift_s32, offset_s32, max);
      uint16x4_t d2 = highbd_convolve6_4_2d_v(
          s2, s3, s4, s5, s6, s7, y_filter, round1_shift_s32, offset_s32, max);
      uint16x4_t d3 = highbd_convolve6_4_2d_v(
          s3, s4, s5, s6, s7, s8, y_filter, round1_shift_s32, offset_s32, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;
      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint16x8_t d0 =
            highbd_convolve6_8_2d_v(s0, s1, s2, s3, s4, s5, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d1 =
            highbd_convolve6_8_2d_v(s1, s2, s3, s4, s5, s6, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d2 =
            highbd_convolve6_8_2d_v(s2, s3, s4, s5, s6, s7, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d3 =
            highbd_convolve6_8_2d_v(s3, s4, s5, s6, s7, s8, y_filter,
                                    round1_shift_s32, offset_s32, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline uint16x4_t highbd_convolve8_4_2d_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const int32x4_t round_shift, const int32x4_t offset, const uint16x4_t max) {
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

  sum = vshlq_s32(sum, round_shift);
  uint16x4_t res = vqmovun_s32(sum);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const int32x4_t round_shift, const int32x4_t offset, const uint16x8_t max) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_hi, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_hi, 3);

  sum0 = vshlq_s32(sum0, round_shift);
  sum1 = vshlq_s32(sum1, round_shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_2d_sr_vert_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    int bd, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);
  const int round1_shift = conv_params->round_1;
  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_shift);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 =
          highbd_convolve8_4_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                  round1_shift_s32, offset_s32, max);
      uint16x4_t d1 =
          highbd_convolve8_4_2d_v(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                  round1_shift_s32, offset_s32, max);
      uint16x4_t d2 =
          highbd_convolve8_4_2d_v(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                  round1_shift_s32, offset_s32, max);
      uint16x4_t d3 =
          highbd_convolve8_4_2d_v(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                  round1_shift_s32, offset_s32, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

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
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 =
            highbd_convolve8_8_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d1 =
            highbd_convolve8_8_2d_v(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d2 =
            highbd_convolve8_8_2d_v(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                    round1_shift_s32, offset_s32, max);
        uint16x8_t d3 =
            highbd_convolve8_8_2d_v(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                    round1_shift_s32, offset_s32, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline uint16x4_t highbd_convolve12_4_2d_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
    const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t round_shift, const int32x4_t offset, const uint16x4_t max) {
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

  sum = vshlq_s32(sum, round_shift);
  uint16x4_t res = vqmovun_s32(sum);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve12_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
    const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
    const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
    const int32x4_t round_shift, const int32x4_t offset, const uint16x8_t max) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_0_3, 0);
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

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_0_3, 0);
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

  sum0 = vshlq_s32(sum0, round_shift);
  sum1 = vshlq_s32(sum1, round_shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
  return vminq_u16(res, max);
}

static inline void highbd_convolve_2d_sr_vert_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    const int bd, const int offset) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);
  const int round1_shift = conv_params->round_1;
  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_shift);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &s10);
    s += 11 * src_stride;

    do {
      int16x4_t s11, s12, s13, s14;
      load_s16_4x4(s, src_stride, &s11, &s12, &s13, &s14);

      uint16x4_t d0 = highbd_convolve12_4_2d_v(
          s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, max);
      uint16x4_t d1 = highbd_convolve12_4_2d_v(
          s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, max);
      uint16x4_t d2 = highbd_convolve12_4_2d_v(
          s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, max);
      uint16x4_t d3 = highbd_convolve12_4_2d_v(
          s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

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
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        int16x8_t s11, s12, s13, s14;
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        uint16x8_t d0 = highbd_convolve12_8_2d_v(
            s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, max);
        uint16x8_t d1 = highbd_convolve12_8_2d_v(
            s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, max);
        uint16x8_t d2 = highbd_convolve12_8_2d_v(
            s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, max);
        uint16x8_t d3 = highbd_convolve12_8_2d_v(
            s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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
      } while (height != 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline uint16x8_t highbd_convolve6_8_2d_h(const int16x8_t s[6],
                                                 const int16x8_t x_filter,
                                                 const int32x4_t shift_s32,
                                                 const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s[0]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 2);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s[0]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 2);

  sum0 = vqrshlq_s32(sum0, shift_s32);
  sum1 = vqrshlq_s32(sum1, shift_s32);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static inline void highbd_convolve_2d_sr_horiz_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset) {
  // The smallest block height processed by the SIMD functions is 4, and the
  // horizontal convolution needs to process an extra (filter_taps/2 - 1) lines
  // for the vertical convolution.
  assert(h >= 5);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 =
          highbd_convolve6_8_2d_h(s0, x_filter, shift_s32, offset_s32);
      uint16x8_t d1 =
          highbd_convolve6_8_2d_h(s1, x_filter, shift_s32, offset_s32);
      uint16x8_t d2 =
          highbd_convolve6_8_2d_h(s2, x_filter, shift_s32, offset_s32);
      uint16x8_t d3 =
          highbd_convolve6_8_2d_h(s3, x_filter, shift_s32, offset_s32);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height > 4);
  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6];
      load_s16_8x6(s, 1, &s0[0], &s0[1], &s0[2], &s0[3], &s0[4], &s0[5]);

      uint16x8_t d0 =
          highbd_convolve6_8_2d_h(s0, x_filter, shift_s32, offset_s32);
      vst1q_u16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--height != 0);
}

static inline uint16x4_t highbd_convolve4_4_2d_h(const int16x4_t s[4],
                                                 const int16x4_t x_filter,
                                                 const int32x4_t shift_s32,
                                                 const int32x4_t offset) {
  int32x4_t sum = vmlal_lane_s16(offset, s[0], x_filter, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter, 3);

  sum = vqrshlq_s32(sum, shift_s32);
  return vqmovun_s32(sum);
}

static inline uint16x8_t highbd_convolve8_8_2d_h(const int16x8_t s[8],
                                                 const int16x8_t x_filter,
                                                 const int32x4_t shift_s32,
                                                 const int32x4_t offset) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s[0]), x_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[6]), x_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[7]), x_filter_4_7, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s[0]), x_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[6]), x_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[7]), x_filter_4_7, 3);

  sum0 = vqrshlq_s32(sum0, shift_s32);
  sum1 = vqrshlq_s32(sum1, shift_s32);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static inline void highbd_convolve_2d_sr_horiz_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset) {
  // The smallest block height processed by the SIMD functions is 4, and the
  // horizontal convolution needs to process an extra (filter_taps/2 - 1) lines
  // for the vertical convolution.
  assert(h >= 5);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  if (w == 4) {
    // 4-tap filters are used for blocks having width <= 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 1);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 =
          highbd_convolve4_4_2d_h(s0, x_filter, shift_s32, offset_s32);
      uint16x4_t d1 =
          highbd_convolve4_4_2d_h(s1, x_filter, shift_s32, offset_s32);
      uint16x4_t d2 =
          highbd_convolve4_4_2d_h(s2, x_filter, shift_s32, offset_s32);
      uint16x4_t d3 =
          highbd_convolve4_4_2d_h(s3, x_filter, shift_s32, offset_s32);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      int16x4_t s0[4];
      load_s16_4x4(s, 1, &s0[0], &s0[1], &s0[2], &s0[3]);

      uint16x4_t d0 =
          highbd_convolve4_4_2d_h(s0, x_filter, shift_s32, offset_s32);

      vst1_u16(d, d0);

      s += src_stride;
      d += dst_stride;
    } while (--h != 0);
  } else {
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 =
            highbd_convolve8_8_2d_h(s0, x_filter, shift_s32, offset_s32);
        uint16x8_t d1 =
            highbd_convolve8_8_2d_h(s1, x_filter, shift_s32, offset_s32);
        uint16x8_t d2 =
            highbd_convolve8_8_2d_h(s2, x_filter, shift_s32, offset_s32);
        uint16x8_t d3 =
            highbd_convolve8_8_2d_h(s3, x_filter, shift_s32, offset_s32);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);

        uint16x8_t d0 =
            highbd_convolve8_8_2d_h(s0, x_filter, shift_s32, offset_s32);
        vst1q_u16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

static inline uint16x4_t highbd_convolve12_4_2d_h(const int16x4_t s[12],
                                                  const int16x8_t x_filter_0_7,
                                                  const int16x4_t x_filter_8_11,
                                                  const int32x4_t shift_s32,
                                                  const int32x4_t offset) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = vmlal_lane_s16(offset, s[0], x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s[4], x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s[5], x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s[6], x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s[7], x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s[8], x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s[9], x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s[10], x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s[11], x_filter_8_11, 3);

  sum = vqrshlq_s32(sum, shift_s32);
  return vqmovun_s32(sum);
}

static inline uint16x8_t highbd_convolve12_8_2d_h(const int16x8_t s[12],
                                                  const int16x8_t x_filter_0_7,
                                                  const int16x4_t x_filter_8_11,
                                                  const int32x4_t shift_s32,
                                                  const int32x4_t offset) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s[0]), x_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[1]), x_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[2]), x_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[3]), x_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[4]), x_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[5]), x_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[6]), x_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[7]), x_filter_4_7, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[8]), x_filter_8_11, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[9]), x_filter_8_11, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[10]), x_filter_8_11, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s[11]), x_filter_8_11, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s[0]), x_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[1]), x_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[2]), x_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[3]), x_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[4]), x_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[5]), x_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[6]), x_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[7]), x_filter_4_7, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[8]), x_filter_8_11, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[9]), x_filter_8_11, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[10]), x_filter_8_11, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s[11]), x_filter_8_11, 3);

  sum0 = vqrshlq_s32(sum0, shift_s32);
  sum1 = vqrshlq_s32(sum1, shift_s32);

  return vcombine_u16(vqmovun_s32(sum0), vqmovun_s32(sum1));
}

static inline void highbd_convolve_2d_sr_horiz_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset) {
  // The smallest block height processed by the SIMD functions is 4, and the
  // horizontal convolution needs to process an extra (filter_taps/2 - 1) lines
  // for the vertical convolution.
  assert(h >= 5);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[12], s1[12], s2[12], s3[12];
      load_s16_4x12(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                    &s0[4], &s0[5], &s0[6], &s0[7], &s0[8], &s0[9], &s0[10],
                    &s0[11]);
      load_s16_4x12(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                    &s1[4], &s1[5], &s1[6], &s1[7], &s1[8], &s1[9], &s1[10],
                    &s1[11]);
      load_s16_4x12(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                    &s2[4], &s2[5], &s2[6], &s2[7], &s2[8], &s2[9], &s2[10],
                    &s2[11]);
      load_s16_4x12(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                    &s3[4], &s3[5], &s3[6], &s3[7], &s3[8], &s3[9], &s3[10],
                    &s3[11]);

      uint16x4_t d0 = highbd_convolve12_4_2d_h(s0, x_filter_0_7, x_filter_8_11,
                                               shift_s32, offset_s32);
      uint16x4_t d1 = highbd_convolve12_4_2d_h(s1, x_filter_0_7, x_filter_8_11,
                                               shift_s32, offset_s32);
      uint16x4_t d2 = highbd_convolve12_4_2d_h(s2, x_filter_0_7, x_filter_8_11,
                                               shift_s32, offset_s32);
      uint16x4_t d3 = highbd_convolve12_4_2d_h(s3, x_filter_0_7, x_filter_8_11,
                                               shift_s32, offset_s32);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      int16x4_t s0[12];
      load_s16_4x12(s, 1, &s0[0], &s0[1], &s0[2], &s0[3], &s0[4], &s0[5],
                    &s0[6], &s0[7], &s0[8], &s0[9], &s0[10], &s0[11]);

      uint16x4_t d0 = highbd_convolve12_4_2d_h(s0, x_filter_0_7, x_filter_8_11,
                                               shift_s32, offset_s32);

      vst1_u16(d, d0);

      s += src_stride;
      d += dst_stride;
    } while (--h != 0);
  } else {
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[12], s1[12], s2[12], s3[12];
        load_s16_8x12(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                      &s0[4], &s0[5], &s0[6], &s0[7], &s0[8], &s0[9], &s0[10],
                      &s0[11]);
        load_s16_8x12(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                      &s1[4], &s1[5], &s1[6], &s1[7], &s1[8], &s1[9], &s1[10],
                      &s1[11]);
        load_s16_8x12(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                      &s2[4], &s2[5], &s2[6], &s2[7], &s2[8], &s2[9], &s2[10],
                      &s2[11]);
        load_s16_8x12(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                      &s3[4], &s3[5], &s3[6], &s3[7], &s3[8], &s3[9], &s3[10],
                      &s3[11]);

        uint16x8_t d0 = highbd_convolve12_8_2d_h(
            s0, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        uint16x8_t d1 = highbd_convolve12_8_2d_h(
            s1, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        uint16x8_t d2 = highbd_convolve12_8_2d_h(
            s2, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        uint16x8_t d3 = highbd_convolve12_8_2d_h(
            s3, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[12];
        load_s16_8x12(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                      &s0[4], &s0[5], &s0[6], &s0[7], &s0[8], &s0[9], &s0[10],
                      &s0[11]);

        uint16x8_t d0 = highbd_convolve12_8_2d_h(
            s0, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        vst1q_u16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

void av1_highbd_convolve_2d_sr_neon(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn,
                                    const int subpel_y_qn,
                                    ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                                filter_params_x, filter_params_y, subpel_x_qn,
                                subpel_y_qn, conv_params, bd);
    return;
  }
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_x_taps = x_filter_taps < 6 ? 6 : x_filter_taps;

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;
  const int x_offset_initial = (1 << (bd + FILTER_BITS - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  // The extra shim of (1 << (conv_params->round_1 - 1)) allows us to do a
  // simple shift left instead of a rounding saturating shift left.
  const int y_offset =
      (1 << (conv_params->round_1 - 1)) - (1 << (y_offset_bits - 1));

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (x_filter_taps > 8) {
    highbd_convolve_2d_sr_horiz_12tap_neon(src_ptr, src_stride, im_block,
                                           im_stride, w, im_h, x_filter_ptr,
                                           conv_params, x_offset_initial);

    highbd_convolve_2d_sr_vert_12tap_neon(im_block, im_stride, dst, dst_stride,
                                          w, h, y_filter_ptr, conv_params, bd,
                                          y_offset);
    return;
  }
  if (x_filter_taps <= 6 && w != 4) {
    highbd_convolve_2d_sr_horiz_6tap_neon(src_ptr, src_stride, im_block,
                                          im_stride, w, im_h, x_filter_ptr,
                                          conv_params, x_offset_initial);
  } else {
    highbd_convolve_2d_sr_horiz_neon(src_ptr, src_stride, im_block, im_stride,
                                     w, im_h, x_filter_ptr, conv_params,
                                     x_offset_initial);
  }

  if (y_filter_taps <= 6) {
    highbd_convolve_2d_sr_vert_6tap_neon(im_block, im_stride, dst, dst_stride,
                                         w, h, y_filter_ptr, conv_params, bd,
                                         y_offset);
  } else {
    highbd_convolve_2d_sr_vert_8tap_neon(im_block, im_stride, dst, dst_stride,
                                         w, h, y_filter_ptr, conv_params, bd,
                                         y_offset);
  }
}

// Filter used is [64, 64].
void av1_highbd_convolve_x_sr_intrabc_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd) {
  assert(subpel_x_qn == 8);
  assert(filter_params_x->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)conv_params;
  (void)bd;

  if (w <= 4) {
    do {
      uint16x4_t s0 = vld1_u16(src);
      uint16x4_t s1 = vld1_u16(src + 1);

      uint16x4_t d0 = vrhadd_u16(s0, s1);

      if (w == 2) {
        store_u16_2x1(dst, d0);
      } else {
        vst1_u16(dst, d0);
      }

      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else {
    do {
      const uint16_t *src_ptr = src;
      uint16_t *dst_ptr = dst;
      int width = w;

      do {
        uint16x8_t s0 = vld1q_u16(src_ptr);
        uint16x8_t s1 = vld1q_u16(src_ptr + 1);

        uint16x8_t d0 = vrhaddq_u16(s0, s1);

        vst1q_u16(dst_ptr, d0);

        src_ptr += 8;
        dst_ptr += 8;
        width -= 8;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  }
}

// Filter used is [64, 64].
void av1_highbd_convolve_y_sr_intrabc_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    int bd) {
  assert(subpel_y_qn == 8);
  assert(filter_params_y->taps == 2);
  (void)filter_params_y;
  (void)subpel_y_qn;
  (void)bd;

  if (w <= 4) {
    do {
      uint16x4_t s0 = vld1_u16(src);
      uint16x4_t s1 = vld1_u16(src + src_stride);

      uint16x4_t d0 = vrhadd_u16(s0, s1);

      if (w == 2) {
        store_u16_2x1(dst, d0);
      } else {
        vst1_u16(dst, d0);
      }

      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else {
    do {
      const uint16_t *src_ptr = src;
      uint16_t *dst_ptr = dst;
      int height = h;

      do {
        uint16x8_t s0 = vld1q_u16(src_ptr);
        uint16x8_t s1 = vld1q_u16(src_ptr + src_stride);

        uint16x8_t d0 = vrhaddq_u16(s0, s1);

        vst1q_u16(dst_ptr, d0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

// Both horizontal and vertical passes use the same 2-tap filter: [64, 64].
void av1_highbd_convolve_2d_sr_intrabc_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd) {
  assert(subpel_x_qn == 8);
  assert(subpel_y_qn == 8);
  assert(filter_params_x->taps == 2 && filter_params_y->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  assert(w <= MAX_SB_SIZE && h <= MAX_SB_SIZE);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)filter_params_y;
  (void)subpel_y_qn;
  (void)conv_params;
  (void)bd;

  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  int im_h = h + 1;
  int im_stride = MAX_SB_SIZE;

  uint16x8_t vert_offset = vdupq_n_u16(1);

  uint16_t *im = im_block;

  // Horizontal filter.
  if (w <= 4) {
    do {
      uint16x4_t s0 = vld1_u16(src);
      uint16x4_t s1 = vld1_u16(src + 1);

      uint16x4_t d0 = vadd_u16(s0, s1);

      // Safe to store the whole vector, the im buffer is big enough.
      vst1_u16(im, d0);

      src += src_stride;
      im += im_stride;
    } while (--im_h != 0);
  } else {
    do {
      const uint16_t *src_ptr = src;
      uint16_t *im_ptr = im;
      int width = w;

      do {
        uint16x8_t s0 = vld1q_u16(src_ptr);
        uint16x8_t s1 = vld1q_u16(src_ptr + 1);

        uint16x8_t d0 = vaddq_u16(s0, s1);

        vst1q_u16(im_ptr, d0);

        src_ptr += 8;
        im_ptr += 8;
        width -= 8;
      } while (width != 0);
      src += src_stride;
      im += im_stride;
    } while (--im_h != 0);
  }

  im = im_block;

  // Vertical filter.
  if (w <= 4) {
    do {
      uint16x4_t s0 = vld1_u16(im);
      uint16x4_t s1 = vld1_u16(im + im_stride);

      uint16x4_t d0 = vhadd_u16(s0, s1);
      d0 = vhadd_u16(d0, vget_low_u16(vert_offset));

      if (w == 2) {
        store_u16_2x1(dst, d0);
      } else {
        vst1_u16(dst, d0);
      }

      im += im_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else {
    do {
      uint16_t *im_ptr = im;
      uint16_t *dst_ptr = dst;
      int height = h;

      do {
        uint16x8_t s0 = vld1q_u16(im_ptr);
        uint16x8_t s1 = vld1q_u16(im_ptr + im_stride);

        uint16x8_t d0 = vhaddq_u16(s0, s1);
        d0 = vhaddq_u16(d0, vert_offset);

        vst1q_u16(dst_ptr, d0);

        im_ptr += im_stride;
        dst_ptr += dst_stride;
      } while (--height != 0);
      im += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
