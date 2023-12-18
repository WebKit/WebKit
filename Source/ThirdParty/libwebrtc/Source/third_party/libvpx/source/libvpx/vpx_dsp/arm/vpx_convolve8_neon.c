/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/arm/vpx_convolve8_neon.h"
#include "vpx_dsp/vpx_filter.h"
#include "vpx_ports/mem.h"

// Note:
// 1. src is not always 32-bit aligned, so don't call vld1_lane_u32(src).
// 2. After refactoring the shared code in kernel loops with inline functions,
// the decoder speed dropped a lot when using gcc compiler. Therefore there is
// no refactoring for those parts by now.
// 3. For horizontal convolve, there is an alternative optimization that
// convolves a single row in each loop. For each row, 8 sample banks with 4 or 8
// samples in each are read from memory: src, (src+1), (src+2), (src+3),
// (src+4), (src+5), (src+6), (src+7), or prepared by vector extract
// instructions. This optimization is much faster in speed unit test, but slowed
// down the whole decoder by 5%.

void vpx_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *filter, int x0_q4,
                              int x_step_q4, int y0_q4, int y_step_q4, int w,
                              int h) {
  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  uint8x8_t t0, t1, t2, t3;

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

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

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      transpose_u8_4x4(&d01, &d23);

      store_u8(dst + 0 * dst_stride, 2 * dst_stride, d01);
      store_u8(dst + 1 * dst_stride, 2 * dst_stride, d23);

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
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7, d04, d15, d26, d37;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w == 4) {
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
        d04 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d15 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d26 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d37 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        transpose_u8_8x4(&d04, &d15, &d26, &d37);

        store_u8(dst + 0 * dst_stride, 4 * dst_stride, d04);
        store_u8(dst + 1 * dst_stride, 4 * dst_stride, d15);
        store_u8(dst + 2 * dst_stride, 4 * dst_stride, d26);
        store_u8(dst + 3 * dst_stride, 4 * dst_stride, d37);

        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    } else {
      uint8_t *d;
      uint8x8_t d0, d1, d2, d3, d4, d5, d6, d7;
      int16x8_t s11, s12, s13, s14;

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

          d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
          d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
          d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
          d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);
          d4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters);
          d5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters);
          d6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters);
          d7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters);

          transpose_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

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

void vpx_convolve8_avg_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *filter, int x0_q4,
                                  int x_step_q4, int y0_q4, int y_step_q4,
                                  int w, int h) {
  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  uint8x8_t t0, t1, t2, t3;

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (h == 4) {
    uint8x8_t d01, d23, dd01, dd23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

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

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      transpose_u8_4x4(&d01, &d23);

      dd01 = load_u8(dst + 0 * dst_stride, 2 * dst_stride);
      dd23 = load_u8(dst + 1 * dst_stride, 2 * dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, 2 * dst_stride, d01);
      store_u8(dst + 1 * dst_stride, 2 * dst_stride, d23);

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
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w == 4) {
      uint8x8_t d04, d15, d26, d37, dd04, dd15, dd26, dd37;

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
        d04 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d15 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d26 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d37 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        transpose_u8_8x4(&d04, &d15, &d26, &d37);

        dd04 = load_u8(dst + 0 * dst_stride, 4 * dst_stride);
        dd15 = load_u8(dst + 1 * dst_stride, 4 * dst_stride);
        dd26 = load_u8(dst + 2 * dst_stride, 4 * dst_stride);
        dd37 = load_u8(dst + 3 * dst_stride, 4 * dst_stride);

        d04 = vrhadd_u8(d04, dd04);
        d15 = vrhadd_u8(d15, dd15);
        d26 = vrhadd_u8(d26, dd26);
        d37 = vrhadd_u8(d37, dd37);

        store_u8(dst + 0 * dst_stride, 4 * dst_stride, d04);
        store_u8(dst + 1 * dst_stride, 4 * dst_stride, d15);
        store_u8(dst + 2 * dst_stride, 4 * dst_stride, d26);
        store_u8(dst + 3 * dst_stride, 4 * dst_stride, d37);

        dst += 8 * dst_stride;
        h -= 8;
      } while (h != 0);
    } else {
      uint8_t *d;
      uint8x8_t d0, d1, d2, d3, d4, d5, d6, d7;
      int16x8_t s11, s12, s13, s14;

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

          d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
          d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
          d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
          d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);
          d4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters);
          d5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters);
          d6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters);
          d7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters);

          transpose_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

          d0 = vrhadd_u8(d0, vld1_u8(d + 0 * dst_stride));
          d1 = vrhadd_u8(d1, vld1_u8(d + 1 * dst_stride));
          d2 = vrhadd_u8(d2, vld1_u8(d + 2 * dst_stride));
          d3 = vrhadd_u8(d3, vld1_u8(d + 3 * dst_stride));
          d4 = vrhadd_u8(d4, vld1_u8(d + 4 * dst_stride));
          d5 = vrhadd_u8(d5, vld1_u8(d + 5 * dst_stride));
          d6 = vrhadd_u8(d6, vld1_u8(d + 6 * dst_stride));
          d7 = vrhadd_u8(d7, vld1_u8(d + 7 * dst_stride));

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
      } while (h != 0);
    }
  }
}

void vpx_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *filter, int x0_q4,
                             int x_step_q4, int y0_q4, int y_step_q4, int w,
                             int h) {
  const int16x8_t filters = vld1q_s16(filter[y0_q4]);

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t4)));
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t5)));
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t6)));

    src += 7 * src_stride;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

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
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);

      load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s = src + 7 * src_stride;
      d = dst;
      height = h;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);

        d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

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

void vpx_convolve8_avg_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *filter, int x0_q4,
                                 int x_step_q4, int y0_q4, int y_step_q4, int w,
                                 int h) {
  const int16x8_t filters = vld1q_s16(filter[y0_q4]);

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d01, d23, dd01, dd23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t4)));
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t5)));
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t6)));

    src += 7 * src_stride;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      dd01 = load_u8(dst + 0 * dst_stride, dst_stride);
      dd23 = load_u8(dst + 2 * dst_stride, dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

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
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);

      load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s = src + 7 * src_stride;
      d = dst;
      height = h;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);

        d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        d0 = vrhadd_u8(d0, vld1_u8(d + 0 * dst_stride));
        d1 = vrhadd_u8(d1, vld1_u8(d + 1 * dst_stride));
        d2 = vrhadd_u8(d2, vld1_u8(d + 2 * dst_stride));
        d3 = vrhadd_u8(d3, vld1_u8(d + 3 * dst_stride));

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
        s += 4 * src_stride;
        d += 4 * dst_stride;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
