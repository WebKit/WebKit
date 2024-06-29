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
#include "aom_dsp/arm/aom_filter.h"
#include "aom_dsp/arm/highbd_convolve8_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

static INLINE uint16x4_t
highbd_convolve8_4(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                   const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                   const int16x4_t s6, const int16x4_t s7,
                   const int16x8_t filter, const uint16x4_t max) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int32x4_t sum = vmull_lane_s16(s0, filter_lo, 0);
  sum = vmlal_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlal_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlal_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlal_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlal_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlal_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlal_lane_s16(sum, s7, filter_hi, 3);

  uint16x4_t res = vqrshrun_n_s32(sum, FILTER_BITS);

  return vmin_u16(res, max);
}

static INLINE uint16x8_t
highbd_convolve8_8(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                   const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                   const int16x8_t s6, const int16x8_t s7,
                   const int16x8_t filter, const uint16x8_t max) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), filter_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filter_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filter_hi, 3);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), filter_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filter_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filter_hi, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0, FILTER_BITS),
                                vqrshrun_n_s32(sum1, FILTER_BITS));

  return vminq_u16(res, max);
}

static void highbd_convolve_horiz_8tap_neon(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, const int16_t *x_filter_ptr, int w, int h, int bd) {
  assert(w >= 4 && h >= 4);
  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[8], s1[8], s2[8], s3[8];
      load_s16_4x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5], &s0[6], &s0[7]);
      load_s16_4x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5], &s1[6], &s1[7]);
      load_s16_4x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5], &s2[6], &s2[7]);
      load_s16_4x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5], &s3[6], &s3[7]);

      uint16x4_t d0 = highbd_convolve8_4(s0[0], s0[1], s0[2], s0[3], s0[4],
                                         s0[5], s0[6], s0[7], x_filter, max);
      uint16x4_t d1 = highbd_convolve8_4(s1[0], s1[1], s1[2], s1[3], s1[4],
                                         s1[5], s1[6], s1[7], x_filter, max);
      uint16x4_t d2 = highbd_convolve8_4(s2[0], s2[1], s2[2], s2[3], s2[4],
                                         s2[5], s2[6], s2[7], x_filter, max);
      uint16x4_t d3 = highbd_convolve8_4(s3[0], s3[1], s3[2], s3[3], s3[4],
                                         s3[5], s3[6], s3[7], x_filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
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

        uint16x8_t d0 = highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                           s0[5], s0[6], s0[7], x_filter, max);
        uint16x8_t d1 = highbd_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                           s1[5], s1[6], s1[7], x_filter, max);
        uint16x8_t d2 = highbd_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                           s2[5], s2[6], s2[7], x_filter, max);
        uint16x8_t d3 = highbd_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                           s3[5], s3[6], s3[7], x_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

static void highbd_convolve_horiz_4tap_neon(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, const int16_t *x_filter_ptr, int w, int h, int bd) {
  assert(w >= 4 && h >= 4);
  const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 =
          highbd_convolve4_4(s0[0], s0[1], s0[2], s0[3], x_filter, max);
      uint16x4_t d1 =
          highbd_convolve4_4(s1[0], s1[1], s1[2], s1[3], x_filter, max);
      uint16x4_t d2 =
          highbd_convolve4_4(s2[0], s2[1], s2[2], s2[3], x_filter, max);
      uint16x4_t d3 =
          highbd_convolve4_4(s3[0], s3[1], s3[2], s3[3], x_filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[4], s1[4], s2[4], s3[4];
        load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
        load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
        load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
        load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

        uint16x8_t d0 =
            highbd_convolve4_8(s0[0], s0[1], s0[2], s0[3], x_filter, max);
        uint16x8_t d1 =
            highbd_convolve4_8(s1[0], s1[1], s1[2], s1[3], x_filter, max);
        uint16x8_t d2 =
            highbd_convolve4_8(s2[0], s2[1], s2[2], s2[3], x_filter, max);
        uint16x8_t d3 =
            highbd_convolve4_8(s3[0], s3[1], s3[2], s3[3], x_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
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

    const int filter_taps = get_filter_taps_convolve8(filter_x);

    if (filter_taps == 2) {
      highbd_convolve8_horiz_2tap_neon(src + 3, src_stride, dst, dst_stride,
                                       filter_x, w, h, bd);
    } else if (filter_taps == 4) {
      highbd_convolve_horiz_4tap_neon(src + 2, src_stride, dst, dst_stride,
                                      filter_x, w, h, bd);
    } else {
      highbd_convolve_horiz_8tap_neon(src, src_stride, dst, dst_stride,
                                      filter_x, w, h, bd);
    }
  }
}

static void highbd_convolve_vert_8tap_neon(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, const int16_t *y_filter_ptr, int w, int h, int bd) {
  assert(w >= 4 && h >= 4);
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
          highbd_convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, max);
      uint16x4_t d1 =
          highbd_convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter, max);
      uint16x4_t d2 =
          highbd_convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter, max);
      uint16x4_t d3 =
          highbd_convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter, max);

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
    } while (h > 0);
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
            highbd_convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, max);
        uint16x8_t d1 =
            highbd_convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, y_filter, max);
        uint16x8_t d2 =
            highbd_convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, y_filter, max);
        uint16x8_t d3 =
            highbd_convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, y_filter, max);

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

    const int filter_taps = get_filter_taps_convolve8(filter_y);

    if (filter_taps == 2) {
      highbd_convolve8_vert_2tap_neon(src + 3 * src_stride, src_stride, dst,
                                      dst_stride, filter_y, w, h, bd);
    } else if (filter_taps == 4) {
      highbd_convolve8_vert_4tap_neon(src + 2 * src_stride, src_stride, dst,
                                      dst_stride, filter_y, w, h, bd);
    } else {
      highbd_convolve_vert_8tap_neon(src, src_stride, dst, dst_stride, filter_y,
                                     w, h, bd);
    }
  }
}
