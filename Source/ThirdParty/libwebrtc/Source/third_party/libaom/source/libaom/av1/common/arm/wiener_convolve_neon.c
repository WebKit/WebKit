/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
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
#include "config/av1_rtcd.h"

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/common.h"
#include "av1/common/arm/convolve_neon.h"

/* Wiener filter 2D
   Apply horizontal filter and store in a temporary buffer. When applying
   vertical filter, overwrite the original pixel values.
 */
void av1_wiener_convolve_add_src_neon(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h,
                                      const ConvolveParams *conv_params) {
  uint16_t *d_tmp;
  uint8_t *d;
  const uint8_t *src_ptr, *s_tmp;
  uint16_t *dst_ptr;
  (void)x_step_q4;
  (void)y_step_q4;

  int width, height;
  const int bd = 8;
  const int intermediate_height = h + SUBPEL_TAPS - 1;
  const int center_tap = ((SUBPEL_TAPS - 1) / 2);
  int16_t filter_x_tmp[7], filter_y_tmp[7];

  DECLARE_ALIGNED(16, uint16_t,
                  temp[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(!(w % 8));

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(filter_x[7] == 0);
  assert(filter_y[7] == 0);

  /* assumption of horizontal filtering output will not exceed 15 bit.
     ((bd) + 1 + FILTER_BITS - conv_params->round_0) <= 15
     16 - conv_params->round_0 <= 15 -- (conv_params->round_0) >= 1
   */
  assert((conv_params->round_0) >= 1);

  memcpy(&filter_x_tmp[0], filter_x, sizeof(*filter_x) * FILTER_BITS);
  memcpy(&filter_y_tmp[0], filter_y, sizeof(*filter_y) * FILTER_BITS);

  filter_x_tmp[3] += (1 << FILTER_BITS);
  filter_y_tmp[3] += (1 << FILTER_BITS);

  s_tmp = src - center_tap * src_stride - center_tap;
  dst_ptr = temp;
  src_ptr = s_tmp;
  height = intermediate_height;

  /* if height is a multiple of 8 */
  if (!(h & 7)) {
    int16x8_t res0, res1, res2, res3;
    uint16x8_t res4;
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
#if defined(__aarch64__)
    uint16x8_t res5, res6, res7, res8, res9, res10, res11;
    uint8x8_t t8, t9, t10, t11, t12, t13, t14;

    do {
      const uint8_t *s;

      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);
      __builtin_prefetch(src_ptr + 4 * src_stride);
      __builtin_prefetch(src_ptr + 5 * src_stride);
      __builtin_prefetch(src_ptr + 6 * src_stride);
      __builtin_prefetch(src_ptr + 7 * src_stride);

      load_u8_8x8(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      s = src_ptr + 7;
      d_tmp = dst_ptr;
      width = w;

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      __builtin_prefetch(dst_ptr + 4 * dst_stride);
      __builtin_prefetch(dst_ptr + 5 * dst_stride);
      __builtin_prefetch(dst_ptr + 6 * dst_stride);
      __builtin_prefetch(dst_ptr + 7 * dst_stride);

      do {
        load_u8_8x8(s, src_stride, &t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);
        transpose_u8_8x8(&t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t0, t6));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t1, t5));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t2, t4));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        res4 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t1, t7));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t2, t6));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t3, t5));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t4));
        res5 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t2, t8));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t3, t7));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t4, t6));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t5));
        res6 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t3, t9));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t4, t8));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t5, t7));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t6));
        res7 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t4, t10));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t5, t9));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t6, t8));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t7));
        res8 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t5, t11));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t6, t10));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t7, t9));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t8));
        res9 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t6, t12));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t7, t11));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t8, t10));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t9));
        res10 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                           bd, conv_params->round_0);

        res0 = vreinterpretq_s16_u16(vaddl_u8(t7, t13));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t8, t12));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t9, t11));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t10));
        res11 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                           bd, conv_params->round_0);

        transpose_u16_8x8(&res4, &res5, &res6, &res7, &res8, &res9, &res10,
                          &res11);
        store_u16_8x8(d_tmp, MAX_SB_SIZE, res4, res5, res6, res7, res8, res9,
                      res10, res11);

        t0 = t8;
        t1 = t9;
        t2 = t10;
        t3 = t11;
        t4 = t12;
        t5 = t13;
        t6 = t14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 8 * src_stride;
      dst_ptr += 8 * MAX_SB_SIZE;
      height -= 8;
    } while (height > 0);
#else
    uint8x8_t temp_0;

    do {
      const uint8_t *s;

      __builtin_prefetch(src_ptr);

      t0 = vld1_u8(src_ptr);  // a0 a1 a2 a3 a4 a5 a6 a7
      s = src_ptr + 8;
      d_tmp = dst_ptr;
      width = w;

      __builtin_prefetch(dst_ptr);

      do {
        t7 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
        temp_0 = t0;
        t0 = t7;

        t1 = vext_u8(temp_0, t7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        t2 = vext_u8(temp_0, t7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        t3 = vext_u8(temp_0, t7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        t4 = vext_u8(temp_0, t7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        t5 = vext_u8(temp_0, t7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        t6 = vext_u8(temp_0, t7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        t7 = vext_u8(temp_0, t7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        res0 = vreinterpretq_s16_u16(vaddl_u8(temp_0, t6));
        res1 = vreinterpretq_s16_u16(vaddl_u8(t1, t5));
        res2 = vreinterpretq_s16_u16(vaddl_u8(t2, t4));
        res3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        res4 = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp,
                                          bd, conv_params->round_0);

        vst1q_u16(d_tmp, res4);

        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += MAX_SB_SIZE;
      height--;
    } while (height > 0);
#endif
  } else {
    /*if height is a multiple of 4*/
    const uint8_t *s;
    int16x8_t tt0, tt1, tt2, tt3;
    uint16x8_t d0;
    uint8x8_t t0, t1, t2, t3;

#if defined(__aarch64__)
    uint16x4_t res0, res1, res2, res3, res4, res5, res6, res7;
    uint16x8_t d1, d2, d3;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    int16x4_t s11, s12, s13, s14;
    do {
      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);

      load_u8_8x4(src_ptr, src_stride, &t0, &t1, &t2, &t3); /*8x4*/
      transpose_u8_8x4(&t0, &t1, &t2,
                       &t3); /*first 8 pixels of 4 rows transposed-- 4x8*/

      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));

      s0 = vget_low_s16(tt0);  /*pa0 pb0 pc0 pd0 -- pixel_a0*/
      s1 = vget_low_s16(tt1);  /*pa1 pb1 pc1 pd1 */
      s2 = vget_low_s16(tt2);  /*pa2 pb2 pc2 pd2 */
      s3 = vget_low_s16(tt3);  /*pa3 pb3 pc3 pd3 */
      s4 = vget_high_s16(tt0); /*pa4 pb4 pc4 pd4 */
      s5 = vget_high_s16(tt1); /*pa5 pb5 pc5 pd5 */
      s6 = vget_high_s16(tt2); /*pa6 pb6 pc6 pd6 */

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);

      s = src_ptr + 7;
      d_tmp = dst_ptr;
      width = w;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3); /*8x4*/
        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));

        s7 = vget_low_s16(tt0); /*pa7  pb7  pc7  pd7  */ /*4x8*/
        s8 = vget_low_s16(tt1);   /*pa8  pb8  pc8  pd8  */
        s9 = vget_low_s16(tt2);   /*pa9  pb9  pc9  pd9  */
        s10 = vget_low_s16(tt3);  /*pa10 pb10 pc10 pd10 */
        s11 = vget_high_s16(tt0); /*pa11 pb11 pc11 pd11 */
        s12 = vget_high_s16(tt1); /*pa12 pb12 pc12 pd12 */
        s13 = vget_high_s16(tt2); /*pa13 pb13 pc13 pd13 */
        s14 = vget_high_s16(tt3); /*pa14 pb14 pc14 pd14 */

        res0 = wiener_convolve8_horiz_4x8(
            s0, s1, s2, s3, s4, s5, s6, filter_x_tmp, bd, conv_params->round_0);
        res1 = wiener_convolve8_horiz_4x8(
            s1, s2, s3, s4, s5, s6, s7, filter_x_tmp, bd, conv_params->round_0);
        res2 = wiener_convolve8_horiz_4x8(
            s2, s3, s4, s5, s6, s7, s8, filter_x_tmp, bd, conv_params->round_0);
        res3 = wiener_convolve8_horiz_4x8(
            s3, s4, s5, s6, s7, s8, s9, filter_x_tmp, bd, conv_params->round_0);
        res4 =
            wiener_convolve8_horiz_4x8(s4, s5, s6, s7, s8, s9, s10,
                                       filter_x_tmp, bd, conv_params->round_0);
        res5 =
            wiener_convolve8_horiz_4x8(s5, s6, s7, s8, s9, s10, s11,
                                       filter_x_tmp, bd, conv_params->round_0);
        res6 =
            wiener_convolve8_horiz_4x8(s6, s7, s8, s9, s10, s11, s12,
                                       filter_x_tmp, bd, conv_params->round_0);
        res7 =
            wiener_convolve8_horiz_4x8(s7, s8, s9, s10, s11, s12, s13,
                                       filter_x_tmp, bd, conv_params->round_0);

        transpose_u16_4x8(&res0, &res1, &res2, &res3, &res4, &res5, &res6,
                          &res7, &d0, &d1, &d2, &d3);

        store_u16_8x4(d_tmp, MAX_SB_SIZE, d0, d1, d2, d3);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * MAX_SB_SIZE;
      height -= 4;
    } while (height > 0);
#else
    uint8x8_t temp_0, t4, t5, t6, t7;

    do {
      __builtin_prefetch(src_ptr);

      t0 = vld1_u8(src_ptr);  // a0 a1 a2 a3 a4 a5 a6 a7

      __builtin_prefetch(dst_ptr);

      s = src_ptr + 8;
      d_tmp = dst_ptr;
      width = w;

      do {
        t7 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
        temp_0 = t0;
        t0 = t7;

        t1 = vext_u8(temp_0, t7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        t2 = vext_u8(temp_0, t7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        t3 = vext_u8(temp_0, t7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        t4 = vext_u8(temp_0, t7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        t5 = vext_u8(temp_0, t7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        t6 = vext_u8(temp_0, t7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        t7 = vext_u8(temp_0, t7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        tt0 = vreinterpretq_s16_u16(vaddl_u8(temp_0, t6));
        tt1 = vreinterpretq_s16_u16(vaddl_u8(t1, t5));
        tt2 = vreinterpretq_s16_u16(vaddl_u8(t2, t4));
        tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        d0 = wiener_convolve8_horiz_8x8(tt0, tt1, tt2, tt3, filter_x_tmp, bd,
                                        conv_params->round_0);

        vst1q_u16(d_tmp, d0);

        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);

      src_ptr += src_stride;
      dst_ptr += MAX_SB_SIZE;
      height -= 1;
    } while (height > 0);
#endif
  }

  {
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint8x8_t t0;
#if defined(__aarch64__)
    int16x8_t s8, s9, s10;
    uint8x8_t t1, t2, t3;
#endif
    int16_t *src_tmp_ptr, *s;
    uint8_t *dst_tmp_ptr;
    height = h;
    width = w;
    src_tmp_ptr = (int16_t *)temp;
    dst_tmp_ptr = dst;
    src_stride = MAX_SB_SIZE;

    do {
      s = src_tmp_ptr;
      s0 = vld1q_s16(s);
      s += src_stride;
      s1 = vld1q_s16(s);
      s += src_stride;
      s2 = vld1q_s16(s);
      s += src_stride;
      s3 = vld1q_s16(s);
      s += src_stride;
      s4 = vld1q_s16(s);
      s += src_stride;
      s5 = vld1q_s16(s);
      s += src_stride;
      s6 = vld1q_s16(s);
      s += src_stride;
      d = dst_tmp_ptr;
      height = h;

#if defined(__aarch64__)
      do {
        __builtin_prefetch(dst_tmp_ptr + 0 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 1 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 2 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 3 * dst_stride);

        s7 = vld1q_s16(s);
        s += src_stride;
        s8 = vld1q_s16(s);
        s += src_stride;
        s9 = vld1q_s16(s);
        s += src_stride;
        s10 = vld1q_s16(s);
        s += src_stride;

        t0 = wiener_convolve8_vert_4x8(s0, s1, s2, s3, s4, s5, s6, filter_y_tmp,
                                       bd, conv_params->round_1);
        t1 = wiener_convolve8_vert_4x8(s1, s2, s3, s4, s5, s6, s7, filter_y_tmp,
                                       bd, conv_params->round_1);
        t2 = wiener_convolve8_vert_4x8(s2, s3, s4, s5, s6, s7, s8, filter_y_tmp,
                                       bd, conv_params->round_1);
        t3 = wiener_convolve8_vert_4x8(s3, s4, s5, s6, s7, s8, s9, filter_y_tmp,
                                       bd, conv_params->round_1);

        vst1_u8(d, t0);
        d += dst_stride;
        vst1_u8(d, t1);
        d += dst_stride;
        vst1_u8(d, t2);
        d += dst_stride;
        vst1_u8(d, t3);
        d += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
      } while (height > 3);

      if (height != 0) {
        __builtin_prefetch(dst_tmp_ptr + 0 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 1 * dst_stride);

        do {
          s7 = vld1q_s16(s);
          s += src_stride;

          t0 =
              wiener_convolve8_vert_4x8(s0, s1, s2, s3, s4, s5, s6,
                                        filter_y_tmp, bd, conv_params->round_1);
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
        } while (height > 0);
      }

      src_tmp_ptr += 8;
      dst_tmp_ptr += 8;

      w -= 8;
    } while (w > 0);
#else
      do {
        __builtin_prefetch(dst_tmp_ptr + 0 * dst_stride);

        s7 = vld1q_s16(s);
        s += src_stride;

        t0 = wiener_convolve8_vert_4x8(s0, s1, s2, s3, s4, s5, s6, filter_y_tmp,
                                       bd, conv_params->round_1);

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
      } while (height > 0);

      src_tmp_ptr += 8;
      dst_tmp_ptr += 8;

      w -= 8;
    } while (w > 0);
#endif
  }
}
