/*
 * Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
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

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

static INLINE int32x4_t highbd_convolve8_4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_lo, 0);
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
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter) {
  int32x4_t sum =
      highbd_convolve8_4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

  return vqrshrun_n_s32(sum, FILTER_BITS);
}

static INLINE int32x4_t highbd_convolve8_horiz4_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7) {
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
                                s7_lo, x_filter_0_7);
}

static INLINE uint16x4_t highbd_convolve8_horiz4_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7) {
  int32x4_t sum = highbd_convolve8_horiz4_s32(s0, s1, x_filter_0_7);

  return vqrshrun_n_s32(sum, FILTER_BITS);
}

static INLINE void highbd_convolve8_8_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    int32x4_t *sum0, int32x4_t *sum1) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  *sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_lo, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s1), y_filter_lo, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s2), y_filter_lo, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s3), y_filter_lo, 3);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s4), y_filter_hi, 0);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s5), y_filter_hi, 1);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s6), y_filter_hi, 2);
  *sum0 = vmlal_lane_s16(*sum0, vget_low_s16(s7), y_filter_hi, 3);

  *sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_lo, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s1), y_filter_lo, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s2), y_filter_lo, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s3), y_filter_lo, 3);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s4), y_filter_hi, 0);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s5), y_filter_hi, 1);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s6), y_filter_hi, 2);
  *sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s7), y_filter_hi, 3);
}

static INLINE void highbd_convolve8_horiz8_s32(const int16x8_t s0,
                                               const int16x8_t s0_hi,
                                               const int16x8_t x_filter_0_7,
                                               int32x4_t *sum0,
                                               int32x4_t *sum1) {
  const int16x8_t s1 = vextq_s16(s0, s0_hi, 1);
  const int16x8_t s2 = vextq_s16(s0, s0_hi, 2);
  const int16x8_t s3 = vextq_s16(s0, s0_hi, 3);
  const int16x8_t s4 = vextq_s16(s0, s0_hi, 4);
  const int16x8_t s5 = vextq_s16(s0, s0_hi, 5);
  const int16x8_t s6 = vextq_s16(s0, s0_hi, 6);
  const int16x8_t s7 = vextq_s16(s0, s0_hi, 7);

  highbd_convolve8_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_0_7, sum0,
                         sum1);
}

static INLINE uint16x8_t highbd_convolve8_horiz8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t x_filter_0_7) {
  int32x4_t sum0, sum1;
  highbd_convolve8_horiz8_s32(s0, s1, x_filter_0_7, &sum0, &sum1);

  return vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                      vqrshrun_n_s32(sum1, FILTER_BITS));
}

static INLINE uint16x8_t highbd_convolve8_8_s32_s16(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter) {
  int32x4_t sum0;
  int32x4_t sum1;
  highbd_convolve8_8_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, &sum0,
                         &sum1);

  return vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                      vqrshrun_n_s32(sum1, FILTER_BITS));
}

static void highbd_convolve_horiz_neon(const uint16_t *src_ptr,
                                       ptrdiff_t src_stride, uint16_t *dst_ptr,
                                       ptrdiff_t dst_stride,
                                       const int16_t *x_filter_ptr,
                                       int x_step_q4, int w, int h, int bd) {
  assert(w >= 4 && h >= 4);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0, s1, s2, s3;
      load_s16_8x2(s, src_stride, &s0, &s2);
      load_s16_8x2(s + 8, src_stride, &s1, &s3);

      uint16x4_t d0 = highbd_convolve8_horiz4_s32_s16(s0, s1, x_filter);
      uint16x4_t d1 = highbd_convolve8_horiz4_s32_s16(s2, s3, x_filter);

      uint16x8_t d01 = vcombine_u16(d0, d1);
      d01 = vminq_u16(d01, max);

      vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
      vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));

      s += 2 * src_stride;
      d += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;
      int x_q4 = 0;

      const int16_t *src_x = &s[x_q4 >> SUBPEL_BITS];
      int16x8_t s0, s2, s4, s6;
      load_s16_8x4(src_x, src_stride, &s0, &s2, &s4, &s6);
      src_x += 8;

      do {
        int16x8_t s1, s3, s5, s7;
        load_s16_8x4(src_x, src_stride, &s1, &s3, &s5, &s7);

        uint16x8_t d0 = highbd_convolve8_horiz8_s32_s16(s0, s1, x_filter);
        uint16x8_t d1 = highbd_convolve8_horiz8_s32_s16(s2, s3, x_filter);
        uint16x8_t d2 = highbd_convolve8_horiz8_s32_s16(s4, s5, x_filter);
        uint16x8_t d3 = highbd_convolve8_horiz8_s32_s16(s6, s7, x_filter);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s1;
        s2 = s3;
        s4 = s5;
        s6 = s7;
        src_x += 8;
        d += 8;
        width -= 8;
        x_q4 += 8 * x_step_q4;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

void aom_highbd_convolve8_horiz_neon(const uint8_t *src8, ptrdiff_t src_stride,
                                     uint8_t *dst8, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y, int y_step_q4,
                                     int w, int h, int bd) {
  if (x_step_q4 != 16) {
    aom_highbd_convolve8_horiz_c(src8, src_stride, dst8, dst_stride, filter_x,
                                 x_step_q4, filter_y, y_step_q4, w, h, bd);
  } else {
    (void)filter_y;
    (void)y_step_q4;

    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

    src -= SUBPEL_TAPS / 2 - 1;
    highbd_convolve_horiz_neon(src, src_stride, dst, dst_stride, filter_x,
                               x_step_q4, w, h, bd);
  }
}

static void highbd_convolve_vert_neon(const uint16_t *src_ptr,
                                      ptrdiff_t src_stride, uint16_t *dst_ptr,
                                      ptrdiff_t dst_stride,
                                      const int16_t *y_filter_ptr, int w, int h,
                                      int bd) {
  assert(w >= 4 && h >= 4);
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 =
          highbd_convolve8_4_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      uint16x4_t d1 =
          highbd_convolve8_4_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      uint16x4_t d2 =
          highbd_convolve8_4_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      uint16x4_t d3 =
          highbd_convolve8_4_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      uint16x8_t d01 = vcombine_u16(d0, d1);
      uint16x8_t d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
      vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
      vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
      vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));

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
    } while (h > 0);
  } else {
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

        uint16x8_t d0 = highbd_convolve8_8_s32_s16(s0, s1, s2, s3, s4, s5, s6,
                                                   s7, y_filter);
        uint16x8_t d1 = highbd_convolve8_8_s32_s16(s1, s2, s3, s4, s5, s6, s7,
                                                   s8, y_filter);
        uint16x8_t d2 = highbd_convolve8_8_s32_s16(s2, s3, s4, s5, s6, s7, s8,
                                                   s9, y_filter);
        uint16x8_t d3 = highbd_convolve8_8_s32_s16(s3, s4, s5, s6, s7, s8, s9,
                                                   s10, y_filter);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

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
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void aom_highbd_convolve8_vert_neon(const uint8_t *src8, ptrdiff_t src_stride,
                                    uint8_t *dst8, ptrdiff_t dst_stride,
                                    const int16_t *filter_x, int x_step_q4,
                                    const int16_t *filter_y, int y_step_q4,
                                    int w, int h, int bd) {
  if (y_step_q4 != 16) {
    aom_highbd_convolve8_vert_c(src8, src_stride, dst8, dst_stride, filter_x,
                                x_step_q4, filter_y, y_step_q4, w, h, bd);
  } else {
    (void)filter_x;
    (void)x_step_q4;

    uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

    src -= (SUBPEL_TAPS / 2 - 1) * src_stride;
    highbd_convolve_vert_neon(src, src_stride, dst, dst_stride, filter_y, w, h,
                              bd);
  }
}
