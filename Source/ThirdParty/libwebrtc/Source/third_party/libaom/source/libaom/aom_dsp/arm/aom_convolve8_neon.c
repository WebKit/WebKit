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
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

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
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

void aom_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4, int w,
                              int h) {
  const int16x8_t filter = vld1q_s16(filter_x);

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  (void)x_step_q4;
  (void)filter_y;
  (void)y_step_q4;

  src -= ((SUBPEL_TAPS / 2) - 1);

  if (h == 4) {
    uint8x8_t t0, t1, t2, t3, d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);
    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);
      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filter);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filter);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filter);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filter);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      transpose_elems_inplace_u8_4x4(&d01, &d23);

      store_u8_4x1(dst + 0 * dst_stride, d01, 0);
      store_u8_4x1(dst + 1 * dst_stride, d23, 0);
      store_u8_4x1(dst + 2 * dst_stride, d01, 1);
      store_u8_4x1(dst + 3 * dst_stride, d23, 1);

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
    } while (w != 0);
  } else {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w == 4) {
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        transpose_elems_u8_4x8(t0, t1, t2, t3, t4, t5, t6, t7, &t0, &t1, &t2,
                               &t3);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filter);
        d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filter);
        d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filter);
        d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filter);

        transpose_elems_inplace_u8_8x4(&d0, &d1, &d2, &d3);

        store_u8_4x1(dst + 0 * dst_stride, d0, 0);
        store_u8_4x1(dst + 1 * dst_stride, d1, 0);
        store_u8_4x1(dst + 2 * dst_stride, d2, 0);
        store_u8_4x1(dst + 3 * dst_stride, d3, 0);
        store_u8_4x1(dst + 4 * dst_stride, d0, 1);
        store_u8_4x1(dst + 5 * dst_stride, d1, 1);
        store_u8_4x1(dst + 6 * dst_stride, d2, 1);
        store_u8_4x1(dst + 7 * dst_stride, d3, 1);

        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    } else {
      uint8x8_t d4, d5, d6, d7;
      int16x8_t s11, s12, s13, s14;
      int width;
      const uint8_t *s;
      uint8_t *d;

      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
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

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6,
                                         &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filter);
          d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filter);
          d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filter);
          d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filter);
          d4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filter);
          d5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filter);
          d6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filter);
          d7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filter);

          transpose_elems_inplace_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6,
                                         &d7);

          store_u8_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

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
        } while (width != 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    }
  }
}

void aom_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x, int x_step_q4,
                             const int16_t *filter_y, int y_step_q4, int w,
                             int h) {
  const int16x8_t filter = vld1q_s16(filter_y);

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  (void)filter_x;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= ((SUBPEL_TAPS / 2) - 1) * src_stride;

  if (w == 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));
    s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
    s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));

    src += 7 * src_stride;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filter);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filter);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filter);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filter);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      store_u8_4x1(dst + 0 * dst_stride, d01, 0);
      store_u8_4x1(dst + 1 * dst_stride, d01, 1);
      store_u8_4x1(dst + 2 * dst_stride, d23, 0);
      store_u8_4x1(dst + 3 * dst_stride, d23, 1);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int height;
    const uint8_t *s;
    uint8_t *d;

    do {
      load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      height = h;
      s = src + 7 * src_stride;
      d = dst;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filter);
        d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filter);
        d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filter);
        d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filter);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

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
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
