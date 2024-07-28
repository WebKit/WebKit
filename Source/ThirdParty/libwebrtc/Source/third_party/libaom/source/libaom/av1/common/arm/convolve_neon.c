/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
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
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/convolve_neon.h"

static INLINE int16x4_t convolve12_4_x(const int16x4_t s0, const int16x4_t s1,
                                       const int16x4_t s2, const int16x4_t s3,
                                       const int16x4_t s4, const int16x4_t s5,
                                       const int16x4_t s6, const int16x4_t s7,
                                       const int16x4_t s8, const int16x4_t s9,
                                       const int16x4_t s10, const int16x4_t s11,
                                       const int16x8_t x_filter_0_7,
                                       const int16x4_t x_filter_8_11,
                                       const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vqrshrn_n_s32(sum, FILTER_BITS);
}

static INLINE void convolve_x_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            const int dst_stride, int w, int h,
                                            const int16_t *x_filter_ptr) {
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);

  // A shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding right
  // shift by FILTER_BITS - instead of a first rounding right shift by
  // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
  // ROUND0_BITS.
  const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

#if AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0, t1, t2, t3;
    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      int16x4_t d0 =
          convolve12_4_x(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d1 =
          convolve12_4_x(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d2 =
          convolve12_4_x(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d3 =
          convolve12_4_x(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
                         x_filter_0_7, x_filter_8_11, horiz_const);

      transpose_elems_inplace_s16_4x4(&d0, &d1, &d2, &d3);

      uint8x8_t d01 = vqmovun_s16(vcombine_s16(d0, d1));
      uint8x8_t d23 = vqmovun_s16(vcombine_s16(d2, d3));

      store_u8x4_strided_x2(d, dst_stride, d01);
      store_u8x4_strided_x2(d + 2 * dst_stride, dst_stride, d23);

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
      s += 4;
      d += 4;
      width -= 4;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h != 0);

#else   // !AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    do {
      uint8x16_t t0 = vld1q_u8(s);
      int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      int16x8_t tt8 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      int16x4_t s0 = vget_low_s16(tt0);
      int16x4_t s4 = vget_high_s16(tt0);
      int16x4_t s8 = vget_low_s16(tt8);
      int16x4_t s12 = vget_high_s16(tt8);

      int16x4_t s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      int16x4_t s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      int16x4_t s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      int16x4_t s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      int16x4_t s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      int16x4_t s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      int16x4_t s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      int16x4_t s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      int16x4_t s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      int16x4_t d0 =
          convolve12_4_x(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                         x_filter_0_7, x_filter_8_11, horiz_const);

      uint8x8_t dd0 = vqmovun_s16(vcombine_s16(d0, vdup_n_s16(0)));

      store_u8_4x1(d, dd0);

      s += 4;
      d += 4;
      width -= 4;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
#endif  // AOM_ARCH_AARCH64
}

static INLINE uint8x8_t convolve4_8_x(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x4_t filter,
                                      int16x8_t horiz_const) {
  int16x8_t sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter, 0);
  sum = vmlaq_lane_s16(sum, s1, filter, 1);
  sum = vmlaq_lane_s16(sum, s2, filter, 2);
  sum = vmlaq_lane_s16(sum, s3, filter, 3);
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_x_sr_4tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16_t *x_filter_ptr) {
  // All filter values are even, halve to reduce intermediate precision
  // requirements.
  const int16x4_t filter = vshr_n_s16(vld1_s16(x_filter_ptr + 2), 1);

  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we will halve the filter values.
  const int16x8_t horiz_const = vdupq_n_s16(1 << ((ROUND0_BITS - 1) - 1));

  if (w == 4) {
    do {
      uint8x8_t t01[4];
      t01[0] = load_unaligned_u8(src_ptr + 0, src_stride);
      t01[1] = load_unaligned_u8(src_ptr + 1, src_stride);
      t01[2] = load_unaligned_u8(src_ptr + 2, src_stride);
      t01[3] = load_unaligned_u8(src_ptr + 3, src_stride);

      int16x8_t s01[4];
      s01[0] = vreinterpretq_s16_u16(vmovl_u8(t01[0]));
      s01[1] = vreinterpretq_s16_u16(vmovl_u8(t01[1]));
      s01[2] = vreinterpretq_s16_u16(vmovl_u8(t01[2]));
      s01[3] = vreinterpretq_s16_u16(vmovl_u8(t01[3]));

      uint8x8_t d01 =
          convolve4_8_x(s01[0], s01[1], s01[2], s01[3], filter, horiz_const);

      store_u8x4_strided_x2(dst_ptr + 0 * dst_stride, dst_stride, d01);

      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      int width = w;
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      do {
        uint8x8_t t0[4], t1[4];
        load_u8_8x4(s + 0 * src_stride, 1, &t0[0], &t0[1], &t0[2], &t0[3]);
        load_u8_8x4(s + 1 * src_stride, 1, &t1[0], &t1[1], &t1[2], &t1[3]);

        int16x8_t s0[4], s1[4];
        s0[0] = vreinterpretq_s16_u16(vmovl_u8(t0[0]));
        s0[1] = vreinterpretq_s16_u16(vmovl_u8(t0[1]));
        s0[2] = vreinterpretq_s16_u16(vmovl_u8(t0[2]));
        s0[3] = vreinterpretq_s16_u16(vmovl_u8(t0[3]));

        s1[0] = vreinterpretq_s16_u16(vmovl_u8(t1[0]));
        s1[1] = vreinterpretq_s16_u16(vmovl_u8(t1[1]));
        s1[2] = vreinterpretq_s16_u16(vmovl_u8(t1[2]));
        s1[3] = vreinterpretq_s16_u16(vmovl_u8(t1[3]));

        uint8x8_t d0 =
            convolve4_8_x(s0[0], s0[1], s0[2], s0[3], filter, horiz_const);
        uint8x8_t d1 =
            convolve4_8_x(s1[0], s1[1], s1[2], s1[3], filter, horiz_const);

        store_u8_8x2(d, dst_stride, d0, d1);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  }
}

static INLINE uint8x8_t convolve8_8_x(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter,
                                      const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                        subpel_x_qn, conv_params);
    return;
  }

  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  src -= horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  int filter_taps = get_filter_tap(filter_params_x, subpel_x_qn & SUBPEL_MASK);

  if (filter_taps > 8) {
    convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             x_filter_ptr);
    return;
  }

  if (filter_taps <= 4) {
    convolve_x_sr_4tap_neon(src + 2, src_stride, dst, dst_stride, w, h,
                            x_filter_ptr);
    return;
  }

  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we will halve the filter values.
  const int16x8_t horiz_const = vdupq_n_s16(1 << ((ROUND0_BITS - 1) - 1));

  // Filter values are even so halve to reduce precision requirements.
  const int16x8_t x_filter = vshrq_n_s16(vld1q_s16(x_filter_ptr), 1);

#if AOM_ARCH_AARCH64
  while (h >= 8) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
    load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

    transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
    int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
    int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

    int width = w;
    const uint8_t *s = src + 7;
    uint8_t *d = dst;

    __builtin_prefetch(d + 0 * dst_stride);
    __builtin_prefetch(d + 1 * dst_stride);
    __builtin_prefetch(d + 2 * dst_stride);
    __builtin_prefetch(d + 3 * dst_stride);
    __builtin_prefetch(d + 4 * dst_stride);
    __builtin_prefetch(d + 5 * dst_stride);
    __builtin_prefetch(d + 6 * dst_stride);
    __builtin_prefetch(d + 7 * dst_stride);

    do {
      uint8x8_t t8, t9, t10, t11, t12, t13, t14;
      load_u8_8x8(s, src_stride, &t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);

      transpose_elems_inplace_u8_8x8(&t7, &t8, &t9, &t10, &t11, &t12, &t13,
                                     &t14);
      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));
      int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
      int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
      int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t13));
      int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t14));

      uint8x8_t d0 =
          convolve8_8_x(s0, s1, s2, s3, s4, s5, s6, s7, x_filter, horiz_const);
      uint8x8_t d1 =
          convolve8_8_x(s1, s2, s3, s4, s5, s6, s7, s8, x_filter, horiz_const);
      uint8x8_t d2 =
          convolve8_8_x(s2, s3, s4, s5, s6, s7, s8, s9, x_filter, horiz_const);
      uint8x8_t d3 =
          convolve8_8_x(s3, s4, s5, s6, s7, s8, s9, s10, x_filter, horiz_const);
      uint8x8_t d4 = convolve8_8_x(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                   horiz_const);
      uint8x8_t d5 = convolve8_8_x(s5, s6, s7, s8, s9, s10, s11, s12, x_filter,
                                   horiz_const);
      uint8x8_t d6 = convolve8_8_x(s6, s7, s8, s9, s10, s11, s12, s13, x_filter,
                                   horiz_const);
      uint8x8_t d7 = convolve8_8_x(s7, s8, s9, s10, s11, s12, s13, s14,
                                   x_filter, horiz_const);

      transpose_elems_inplace_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

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
  }
#endif  // AOM_ARCH_AARCH64

  while (h-- != 0) {
    uint8x8_t t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

    int width = w;
    const uint8_t *s = src + 8;
    uint8_t *d = dst;

    __builtin_prefetch(d);

    do {
      uint8x8_t t8 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));

      int16x8_t s1 = vextq_s16(s0, s8, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
      int16x8_t s2 = vextq_s16(s0, s8, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
      int16x8_t s3 = vextq_s16(s0, s8, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
      int16x8_t s4 = vextq_s16(s0, s8, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
      int16x8_t s5 = vextq_s16(s0, s8, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
      int16x8_t s6 = vextq_s16(s0, s8, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
      int16x8_t s7 = vextq_s16(s0, s8, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

      uint8x8_t d0 =
          convolve8_8_x(s0, s1, s2, s3, s4, s5, s6, s7, x_filter, horiz_const);

      vst1_u8(d, d0);

      s0 = s8;
      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src += src_stride;
    dst += dst_stride;
  }
}

static INLINE uint8x8_t convolve4_8_y(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x4_t filter) {
  int16x8_t sum = vmulq_lane_s16(s0, filter, 0);
  sum = vmlaq_lane_s16(sum, s1, filter, 1);
  sum = vmlaq_lane_s16(sum, s2, filter, 2);
  sum = vmlaq_lane_s16(sum, s3, filter, 3);

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_y_sr_4tap_neon(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, int w, int h,
                                           const int16_t *filter_y) {
  // All filter values are even, halve to reduce intermediate precision
  // requirements.
  const int16x4_t filter = vshr_n_s16(vld1_s16(filter_y + 2), 1);

  if (w == 4) {
    uint8x8_t t01 = load_unaligned_u8(src + 0 * src_stride, src_stride);
    uint8x8_t t12 = load_unaligned_u8(src + 1 * src_stride, src_stride);

    int16x8_t s01 = vreinterpretq_s16_u16(vmovl_u8(t01));
    int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));

    src += 2 * src_stride;

    do {
      uint8x8_t t23 = load_unaligned_u8(src + 0 * src_stride, src_stride);
      uint8x8_t t34 = load_unaligned_u8(src + 1 * src_stride, src_stride);
      uint8x8_t t45 = load_unaligned_u8(src + 2 * src_stride, src_stride);
      uint8x8_t t56 = load_unaligned_u8(src + 3 * src_stride, src_stride);

      int16x8_t s23 = vreinterpretq_s16_u16(vmovl_u8(t23));
      int16x8_t s34 = vreinterpretq_s16_u16(vmovl_u8(t34));
      int16x8_t s45 = vreinterpretq_s16_u16(vmovl_u8(t45));
      int16x8_t s56 = vreinterpretq_s16_u16(vmovl_u8(t56));

      uint8x8_t d01 = convolve4_8_y(s01, s12, s23, s34, filter);
      uint8x8_t d23 = convolve4_8_y(s23, s34, s45, s56, filter);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      s01 = s45;
      s12 = s56;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      uint8x8_t t0, t1, t2;
      load_u8_8x3(src, src_stride, &t0, &t1, &t2);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));

      int height = h;
      const uint8_t *s = src + 3 * src_stride;
      uint8_t *d = dst;

      do {
        uint8x8_t t3;
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);

        int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t0));
        int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t1));
        int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t2));
        int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t3));

        uint8x8_t d0 = convolve4_8_y(s0, s1, s2, s3, filter);
        uint8x8_t d1 = convolve4_8_y(s1, s2, s3, s4, filter);
        uint8x8_t d2 = convolve4_8_y(s2, s3, s4, s5, filter);
        uint8x8_t d3 = convolve4_8_y(s3, s4, s5, s6, filter);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;

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

static INLINE int16x4_t convolve6_4_y(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x8_t y_filter_0_7) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  // Filter values at indices 0 and 7 are 0.
  int16x4_t sum = vmul_lane_s16(s0, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmla_lane_s16(sum, s5, y_filter_4_7, 2);

  return sum;
}

static INLINE uint8x8_t convolve6_8_y(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t y_filters) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filters);
  const int16x4_t y_filter_hi = vget_high_s16(y_filters);

  // Filter values at indices 0 and 7 are 0.
  int16x8_t sum = vmulq_lane_s16(s0, y_filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s5, y_filter_hi, 2);
  // We halved the convolution filter values so -1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_y_sr_6tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16x8_t y_filter) {
  if (w <= 4) {
    uint8x8_t t0 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
    uint8x8_t t1 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
    uint8x8_t t2 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
    uint8x8_t t3 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);
    uint8x8_t t4 = load_unaligned_u8_4x1(src_ptr + 4 * src_stride);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));

    src_ptr += 5 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
      uint8x8_t t6 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
      uint8x8_t t8 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);

      int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
      int16x4_t s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
      int16x4_t s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
      int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));

      int16x4_t d0 = convolve6_4_y(s0, s1, s2, s3, s4, s5, y_filter);
      int16x4_t d1 = convolve6_4_y(s1, s2, s3, s4, s5, s6, y_filter);
      int16x4_t d2 = convolve6_4_y(s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d3 = convolve6_4_y(s3, s4, s5, s6, s7, s8, y_filter);

      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      store_u8x4_strided_x2(dst_ptr, dst_stride, d01);
      store_u8x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride, d23);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr);
      int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));

      int16x4_t d0 = convolve6_4_y(s0, s1, s2, s3, s4, s5, y_filter);
      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 =
          vqrshrun_n_s16(vcombine_s16(d0, vdup_n_s16(0)), FILTER_BITS - 1);

      store_u8_4x1(dst_ptr, d01);

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h != 0);

  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4;
      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));

      s += 5 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        uint8x8_t t5, t6, t7, t8;
        load_u8_8x4(s, src_stride, &t5, &t6, &t7, &t8);

        int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));

        uint8x8_t d0 = convolve6_8_y(s0, s1, s2, s3, s4, s5, y_filter);
        uint8x8_t d1 = convolve6_8_y(s1, s2, s3, s4, s5, s6, y_filter);
        uint8x8_t d2 = convolve6_8_y(s2, s3, s4, s5, s6, s7, y_filter);
        uint8x8_t d3 = convolve6_8_y(s3, s4, s5, s6, s7, s8, y_filter);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !AOM_ARCH_AARCH64
        int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));

        uint8x8_t d0 = convolve6_8_y(s0, s1, s2, s3, s4, s5, y_filter);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE int16x4_t convolve8_4_y(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x4_t sum = vmul_lane_s16(s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  return sum;
}

static INLINE uint8x8_t convolve8_8_y(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = vmulq_lane_s16(s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_y_sr_8tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16x8_t y_filter) {
  if (w <= 4) {
    uint8x8_t t0 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
    uint8x8_t t1 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
    uint8x8_t t2 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
    uint8x8_t t3 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);
    uint8x8_t t4 = load_unaligned_u8_4x1(src_ptr + 4 * src_stride);
    uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr + 5 * src_stride);
    uint8x8_t t6 = load_unaligned_u8_4x1(src_ptr + 6 * src_stride);

    int16x4_t s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
    int16x4_t s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
    int16x4_t s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
    int16x4_t s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));
    int16x4_t s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t4)));
    int16x4_t s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t5)));
    int16x4_t s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t6)));

    src_ptr += 7 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
      uint8x8_t t8 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
      uint8x8_t t9 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
      uint8x8_t t10 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);

      int16x4_t s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t7)));
      int16x4_t s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t8)));
      int16x4_t s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t9)));
      int16x4_t s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t10)));

      int16x4_t d0 = convolve8_4_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d1 = convolve8_4_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      int16x4_t d2 = convolve8_4_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      int16x4_t d3 = convolve8_4_y(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      store_u8x4_strided_x2(dst_ptr, dst_stride, d01);
      store_u8x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride, d23);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr);
      int16x4_t s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t7)));

      int16x4_t d0 = convolve8_4_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 =
          vqrshrun_n_s16(vcombine_s16(d0, vdup_n_s16(0)), FILTER_BITS - 1);

      store_u8_4x1(dst_ptr, d01);

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h != 0);
  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s += 7 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        uint8x8_t t7, t8, t9, t10;
        load_u8_8x4(s, src_stride, &t7, &t8, &t9, &t10);

        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
        int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
        int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));

        uint8x8_t d0 = convolve8_8_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
        uint8x8_t d1 = convolve8_8_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
        uint8x8_t d2 = convolve8_8_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
        uint8x8_t d3 = convolve8_8_y(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

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
#else   // !AOM_ARCH_AARCH64
        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));

        uint8x8_t d0 = convolve8_8_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE int16x4_t convolve12_4_y(const int16x4_t s0, const int16x4_t s1,
                                       const int16x4_t s2, const int16x4_t s3,
                                       const int16x4_t s4, const int16x4_t s5,
                                       const int16x4_t s6, const int16x4_t s7,
                                       const int16x4_t s8, const int16x4_t s9,
                                       const int16x4_t s10, const int16x4_t s11,
                                       const int16x8_t y_filter_0_7,
                                       const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, y_filter_0_3, 0);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmla_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmla_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmla_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmla_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmla_lane_s16(sum, s11, y_filter_8_11, 3);

  // Saturating addition is required for the largest filter taps to avoid
  // overflow (while staying in 16-bit elements.)
  sum = vqadd_s16(sum, vmul_lane_s16(s5, y_filter_4_7, 1));
  sum = vqadd_s16(sum, vmul_lane_s16(s6, y_filter_4_7, 2));

  return sum;
}

static INLINE uint8x8_t convolve12_8_y(const int16x8_t s0, const int16x8_t s1,
                                       const int16x8_t s2, const int16x8_t s3,
                                       const int16x8_t s4, const int16x8_t s5,
                                       const int16x8_t s6, const int16x8_t s7,
                                       const int16x8_t s8, const int16x8_t s9,
                                       const int16x8_t s10, const int16x8_t s11,
                                       const int16x8_t y_filter_0_7,
                                       const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlaq_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlaq_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlaq_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlaq_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmlaq_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmlaq_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmlaq_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmlaq_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmlaq_lane_s16(sum, s11, y_filter_8_11, 3);

  // Saturating addition is required for the largest filter taps to avoid
  // overflow (while staying in 16-bit elements.)
  sum = vqaddq_s16(sum, vmulq_lane_s16(s5, y_filter_4_7, 1));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s6, y_filter_4_7, 2));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve_y_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            int dst_stride, int w, int h,
                                            const int16_t *y_filter_ptr) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

  if (w <= 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
    load_u8_8x11(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7,
                 &t8, &t9, &t10);
    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));
    int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
    int16x4_t s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
    int16x4_t s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t9)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t10)));

    src_ptr += 11 * src_stride;

    do {
      uint8x8_t t11, t12, t13, t14;
      load_u8_8x4(src_ptr, src_stride, &t11, &t12, &t13, &t14);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t11)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t12)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t13)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t14)));

      int16x4_t d0 = convolve12_4_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, y_filter_0_7, y_filter_8_11);
      int16x4_t d1 = convolve12_4_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, s12, y_filter_0_7, y_filter_8_11);
      int16x4_t d2 = convolve12_4_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                    s12, s13, y_filter_0_7, y_filter_8_11);
      int16x4_t d3 = convolve12_4_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                    s13, s14, y_filter_0_7, y_filter_8_11);

      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      store_u8x4_strided_x2(dst_ptr, dst_stride, d01);
      store_u8x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride, d23);

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
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);

  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
      load_u8_8x11(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8,
                   &t9, &t10);
      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));

      s += 11 * src_stride;

      do {
        uint8x8_t t11, t12, t13, t14;
        load_u8_8x4(s, src_stride, &t11, &t12, &t13, &t14);

        int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
        int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
        int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t13));
        int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t14));

        uint8x8_t d0 = convolve12_8_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                      s10, s11, y_filter_0_7, y_filter_8_11);
        uint8x8_t d1 = convolve12_8_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                      s11, s12, y_filter_0_7, y_filter_8_11);
        uint8x8_t d2 = convolve12_8_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                      s12, s13, y_filter_0_7, y_filter_8_11);
        uint8x8_t d3 = convolve12_8_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                      s13, s14, y_filter_0_7, y_filter_8_11);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

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

void av1_convolve_y_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_y,
                            const int subpel_y_qn) {
  if (w == 2 || h == 2) {
    av1_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_y,
                        subpel_y_qn);
    return;
  }

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 4 ? 4 : y_filter_taps;
  const int vert_offset = clamped_y_taps / 2 - 1;

  src -= vert_offset * src_stride;

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (y_filter_taps > 8) {
    convolve_y_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             y_filter_ptr);
    return;
  }

  // Filter values are even so halve to reduce precision requirements.
  const int16x8_t y_filter = vshrq_n_s16(vld1q_s16(y_filter_ptr), 1);

  if (y_filter_taps <= 4) {
    convolve_y_sr_4tap_neon(src, src_stride, dst, dst_stride, w, h,
                            y_filter_ptr);
  } else if (y_filter_taps == 6) {
    convolve_y_sr_6tap_neon(src, src_stride, dst, dst_stride, w, h, y_filter);
  } else {
    convolve_y_sr_8tap_neon(src, src_stride, dst, dst_stride, w, h, y_filter);
  }
}

static INLINE int16x4_t
convolve12_4_2d_h(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                  const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                  const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
                  const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
                  const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
                  const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;
  // A shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts -
  // which are generally faster than rounding shifts on modern CPUs.
  const int32x4_t horiz_const =
      vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));

#if AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0, t1, t2, t3;
    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      int16x4_t d0 =
          convolve12_4_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d1 =
          convolve12_4_2d_h(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d2 =
          convolve12_4_2d_h(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d3 =
          convolve12_4_2d_h(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
                            x_filter_0_7, x_filter_8_11, horiz_const);

      transpose_elems_inplace_s16_4x4(&d0, &d1, &d2, &d3);
      store_s16_4x4(d, dst_stride, d0, d1, d2, d3);

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
      s += 4;
      d += 4;
      width -= 4;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h > 4);
#endif  // AOM_ARCH_AARCH64

  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    do {
      uint8x16_t t0 = vld1q_u8(s);
      int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      int16x8_t tt1 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      int16x4_t s0 = vget_low_s16(tt0);
      int16x4_t s4 = vget_high_s16(tt0);
      int16x4_t s8 = vget_low_s16(tt1);
      int16x4_t s12 = vget_high_s16(tt1);

      int16x4_t s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      int16x4_t s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      int16x4_t s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      int16x4_t s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      int16x4_t s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      int16x4_t s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      int16x4_t s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      int16x4_t s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      int16x4_t s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      int16x4_t d0 =
          convolve12_4_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      vst1_s16(d, d0);

      s += 4;
      d += 4;
      width -= 4;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

static INLINE int16x8_t convolve4_8_2d_h(const int16x8_t s0, const int16x8_t s1,
                                         const int16x8_t s2, const int16x8_t s3,
                                         const int16x4_t filter,
                                         const int16x8_t horiz_const) {
  int16x8_t sum = vmlaq_lane_s16(horiz_const, s0, filter, 0);
  sum = vmlaq_lane_s16(sum, s1, filter, 1);
  sum = vmlaq_lane_s16(sum, s2, filter, 2);
  sum = vmlaq_lane_s16(sum, s3, filter, 3);
  // We halved the filter values so -1 from right shift.
  return vshrq_n_s16(sum, ROUND0_BITS - 1);
}

static INLINE void convolve_2d_sr_horiz_4tap_neon(
    const uint8_t *src, ptrdiff_t src_stride, int16_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16_t *filter_x) {
  const int bd = 8;
  // All filter values are even, halve to reduce intermediate precision
  // requirements.
  const int16x4_t filter = vshr_n_s16(vld1_s16(filter_x + 2), 1);

  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));

  if (w == 4) {
    do {
      uint8x8_t t01[4];
      t01[0] = load_unaligned_u8(src + 0, (int)src_stride);
      t01[1] = load_unaligned_u8(src + 1, (int)src_stride);
      t01[2] = load_unaligned_u8(src + 2, (int)src_stride);
      t01[3] = load_unaligned_u8(src + 3, (int)src_stride);

      int16x8_t s01[4];
      s01[0] = vreinterpretq_s16_u16(vmovl_u8(t01[0]));
      s01[1] = vreinterpretq_s16_u16(vmovl_u8(t01[1]));
      s01[2] = vreinterpretq_s16_u16(vmovl_u8(t01[2]));
      s01[3] = vreinterpretq_s16_u16(vmovl_u8(t01[3]));

      int16x8_t d01 =
          convolve4_8_2d_h(s01[0], s01[1], s01[2], s01[3], filter, horiz_const);

      store_s16x4_strided_x2(dst, (int)dst_stride, d01);

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      int width = w;
      const uint8_t *s = src;
      int16_t *d = dst;

      do {
        uint8x8_t t0[4], t1[4];
        load_u8_8x4(s + 0 * src_stride, 1, &t0[0], &t0[1], &t0[2], &t0[3]);
        load_u8_8x4(s + 1 * src_stride, 1, &t1[0], &t1[1], &t1[2], &t1[3]);

        int16x8_t s0[4];
        s0[0] = vreinterpretq_s16_u16(vmovl_u8(t0[0]));
        s0[1] = vreinterpretq_s16_u16(vmovl_u8(t0[1]));
        s0[2] = vreinterpretq_s16_u16(vmovl_u8(t0[2]));
        s0[3] = vreinterpretq_s16_u16(vmovl_u8(t0[3]));

        int16x8_t s1[4];
        s1[0] = vreinterpretq_s16_u16(vmovl_u8(t1[0]));
        s1[1] = vreinterpretq_s16_u16(vmovl_u8(t1[1]));
        s1[2] = vreinterpretq_s16_u16(vmovl_u8(t1[2]));
        s1[3] = vreinterpretq_s16_u16(vmovl_u8(t1[3]));

        int16x8_t d0 =
            convolve4_8_2d_h(s0[0], s0[1], s0[2], s0[3], filter, horiz_const);
        int16x8_t d1 =
            convolve4_8_2d_h(s1[0], s1[1], s1[2], s1[3], filter, horiz_const);

        store_s16_8x2(d, dst_stride, d0, d1);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h > 2);

    do {
      const uint8_t *s = src;
      int16_t *d = dst;
      int width = w;

      do {
        uint8x8_t t0[4];
        load_u8_8x4(s, 1, &t0[0], &t0[1], &t0[2], &t0[3]);

        int16x8_t s0[4];
        s0[0] = vreinterpretq_s16_u16(vmovl_u8(t0[0]));
        s0[1] = vreinterpretq_s16_u16(vmovl_u8(t0[1]));
        s0[2] = vreinterpretq_s16_u16(vmovl_u8(t0[2]));
        s0[3] = vreinterpretq_s16_u16(vmovl_u8(t0[3]));

        int16x8_t d0 =
            convolve4_8_2d_h(s0[0], s0[1], s0[2], s0[3], filter, horiz_const);

        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  }
}

static INLINE int16x8_t convolve8_8_2d_h(const int16x8_t s0, const int16x8_t s1,
                                         const int16x8_t s2, const int16x8_t s3,
                                         const int16x8_t s4, const int16x8_t s5,
                                         const int16x8_t s6, const int16x8_t s7,
                                         const int16x8_t filter,
                                         const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = horiz_const;
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

static INLINE void convolve_2d_sr_horiz_8tap_neon(
    const uint8_t *src, int src_stride, int16_t *im_block, int im_stride, int w,
    int im_h, const int16_t *x_filter_ptr) {
  const int bd = 8;

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int16x8_t x_filter = vshrq_n_s16(vld1q_s16(x_filter_ptr), 1);

#if AOM_ARCH_AARCH64
  while (height > 8) {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
    load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
    transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
    int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
    int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

    s += 7;

    do {
      load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
      int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

      int16x8_t d0 = convolve8_8_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                      horiz_const);
      int16x8_t d1 = convolve8_8_2d_h(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                      horiz_const);
      int16x8_t d2 = convolve8_8_2d_h(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                      horiz_const);
      int16x8_t d3 = convolve8_8_2d_h(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                      horiz_const);
      int16x8_t d4 = convolve8_8_2d_h(s4, s5, s6, s7, s8, s9, s10, s11,
                                      x_filter, horiz_const);
      int16x8_t d5 = convolve8_8_2d_h(s5, s6, s7, s8, s9, s10, s11, s12,
                                      x_filter, horiz_const);
      int16x8_t d6 = convolve8_8_2d_h(s6, s7, s8, s9, s10, s11, s12, s13,
                                      x_filter, horiz_const);
      int16x8_t d7 = convolve8_8_2d_h(s7, s8, s9, s10, s11, s12, s13, s14,
                                      x_filter, horiz_const);

      transpose_elems_inplace_s16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

      store_s16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

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
    src_ptr += 8 * src_stride;
    dst_ptr += 8 * dst_stride;
    height -= 8;
  }
#endif  // AOM_ARCH_AARCH64

  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0 = vld1_u8(s);  // a0 a1 a2 a3 a4 a5 a6 a7
    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

    do {
      uint8x8_t t1 = vld1_u8(s + 8);  // a8 a9 a10 a11 a12 a13 a14 a15
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t1));

      int16x8_t s1 = vextq_s16(s0, s8, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
      int16x8_t s2 = vextq_s16(s0, s8, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
      int16x8_t s3 = vextq_s16(s0, s8, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
      int16x8_t s4 = vextq_s16(s0, s8, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
      int16x8_t s5 = vextq_s16(s0, s8, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
      int16x8_t s6 = vextq_s16(s0, s8, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
      int16x8_t s7 = vextq_s16(s0, s8, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

      int16x8_t d0 = convolve8_8_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                      horiz_const);

      vst1q_s16(d, d0);

      s0 = s8;
      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--height != 0);
}

void av1_convolve_2d_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                         filter_params_x, filter_params_y, subpel_x_qn,
                         subpel_y_qn, conv_params);
    return;
  }

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_y_taps = y_filter_taps < 4 ? 4 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
    const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
    const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
    const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

    convolve_2d_sr_horiz_12tap_neon(src_ptr, src_stride, im_block, im_stride, w,
                                    im_h, x_filter_0_7, x_filter_8_11);

    convolve_2d_sr_vert_12tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_0_7, y_filter_8_11);
  } else {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

    if (x_filter_taps <= 4) {
      convolve_2d_sr_horiz_4tap_neon(src_ptr + 2, src_stride, im_block,
                                     im_stride, w, im_h, x_filter_ptr);
    } else {
      convolve_2d_sr_horiz_8tap_neon(src_ptr, src_stride, im_block, im_stride,
                                     w, im_h, x_filter_ptr);
    }

    const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

    if (clamped_y_taps <= 4) {
      convolve_2d_sr_vert_4tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter_ptr);
    } else if (clamped_y_taps == 6) {
      convolve_2d_sr_vert_6tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    } else {
      convolve_2d_sr_vert_8tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    }
  }
}

void av1_convolve_x_sr_intrabc_neon(const uint8_t *src, int src_stride,
                                    uint8_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const int subpel_x_qn,
                                    ConvolveParams *conv_params) {
  assert(subpel_x_qn == 8);
  assert(filter_params_x->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)conv_params;

  if (w <= 4) {
    do {
      uint8x8_t s0_0 = vld1_u8(src);
      uint8x8_t s0_1 = vld1_u8(src + 1);
      uint8x8_t s1_0 = vld1_u8(src + src_stride);
      uint8x8_t s1_1 = vld1_u8(src + src_stride + 1);

      uint8x8_t d0 = vrhadd_u8(s0_0, s0_1);
      uint8x8_t d1 = vrhadd_u8(s1_0, s1_1);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d0);
        store_u8_2x1(dst + 1 * dst_stride, d1);
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d0);
        store_u8_4x1(dst + 1 * dst_stride, d1);
      }

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w == 8) {
    do {
      uint8x8_t s0_0 = vld1_u8(src);
      uint8x8_t s0_1 = vld1_u8(src + 1);
      uint8x8_t s1_0 = vld1_u8(src + src_stride);
      uint8x8_t s1_1 = vld1_u8(src + src_stride + 1);

      uint8x8_t d0 = vrhadd_u8(s0_0, s0_1);
      uint8x8_t d1 = vrhadd_u8(s1_0, s1_1);

      vst1_u8(dst, d0);
      vst1_u8(dst + dst_stride, d1);

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      const uint8_t *src_ptr = src;
      uint8_t *dst_ptr = dst;
      int width = w;

      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);
        uint8x16_t s1 = vld1q_u8(src_ptr + 1);

        uint8x16_t d0 = vrhaddq_u8(s0, s1);

        vst1q_u8(dst_ptr, d0);

        src_ptr += 16;
        dst_ptr += 16;
        width -= 16;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  }
}

void av1_convolve_y_sr_intrabc_neon(const uint8_t *src, int src_stride,
                                    uint8_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_y_qn) {
  assert(subpel_y_qn == 8);
  assert(filter_params_y->taps == 2);
  (void)filter_params_y;
  (void)subpel_y_qn;

  if (w <= 4) {
    do {
      uint8x8_t s0 = load_unaligned_u8_4x1(src);
      uint8x8_t s1 = load_unaligned_u8_4x1(src + src_stride);
      uint8x8_t s2 = load_unaligned_u8_4x1(src + 2 * src_stride);

      uint8x8_t d0 = vrhadd_u8(s0, s1);
      uint8x8_t d1 = vrhadd_u8(s1, s2);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d0);
        store_u8_2x1(dst + 1 * dst_stride, d1);
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d0);
        store_u8_4x1(dst + 1 * dst_stride, d1);
      }

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w == 8) {
    do {
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + src_stride);
      uint8x8_t s2 = vld1_u8(src + 2 * src_stride);

      uint8x8_t d0 = vrhadd_u8(s0, s1);
      uint8x8_t d1 = vrhadd_u8(s1, s2);

      vst1_u8(dst, d0);
      vst1_u8(dst + dst_stride, d1);

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      const uint8_t *src_ptr = src;
      uint8_t *dst_ptr = dst;
      int height = h;

      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);
        uint8x16_t s1 = vld1q_u8(src_ptr + src_stride);

        uint8x16_t d0 = vrhaddq_u8(s0, s1);

        vst1q_u8(dst_ptr, d0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--height != 0);
      src += 16;
      dst += 16;
      w -= 16;
    } while (w != 0);
  }
}

void av1_convolve_2d_sr_intrabc_neon(const uint8_t *src, int src_stride,
                                     uint8_t *dst, int dst_stride, int w, int h,
                                     const InterpFilterParams *filter_params_x,
                                     const InterpFilterParams *filter_params_y,
                                     const int subpel_x_qn,
                                     const int subpel_y_qn,
                                     ConvolveParams *conv_params) {
  assert(subpel_x_qn == 8);
  assert(subpel_y_qn == 8);
  assert(filter_params_x->taps == 2 && filter_params_y->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)filter_params_y;
  (void)subpel_y_qn;
  (void)conv_params;

  uint16_t im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE];
  int im_h = h + 1;
  int im_stride = w;
  assert(w <= MAX_SB_SIZE && h <= MAX_SB_SIZE);

  uint16_t *im = im_block;

  // Horizontal filter.
  if (w <= 4) {
    do {
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);

      uint16x4_t sum = vget_low_u16(vaddl_u8(s0, s1));

      // Safe to store the whole vector, the im buffer is big enough.
      vst1_u16(im, sum);

      src += src_stride;
      im += im_stride;
    } while (--im_h != 0);
  } else {
    do {
      const uint8_t *src_ptr = src;
      uint16_t *im_ptr = im;
      int width = w;

      do {
        uint8x8_t s0 = vld1_u8(src_ptr);
        uint8x8_t s1 = vld1_u8(src_ptr + 1);

        uint16x8_t sum = vaddl_u8(s0, s1);

        vst1q_u16(im_ptr, sum);

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
      uint16x4_t s2 = vld1_u16(im + 2 * im_stride);

      uint16x4_t sum0 = vadd_u16(s0, s1);
      uint16x4_t sum1 = vadd_u16(s1, s2);

      uint8x8_t d0 = vqrshrn_n_u16(vcombine_u16(sum0, vdup_n_u16(0)), 2);
      uint8x8_t d1 = vqrshrn_n_u16(vcombine_u16(sum1, vdup_n_u16(0)), 2);

      if (w == 2) {
        store_u8_2x1(dst + 0 * dst_stride, d0);
        store_u8_2x1(dst + 1 * dst_stride, d1);
      } else {
        store_u8_4x1(dst + 0 * dst_stride, d0);
        store_u8_4x1(dst + 1 * dst_stride, d1);
      }

      im += 2 * im_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      uint16_t *im_ptr = im;
      uint8_t *dst_ptr = dst;
      int height = h;

      do {
        uint16x8_t s0 = vld1q_u16(im_ptr);
        uint16x8_t s1 = vld1q_u16(im_ptr + im_stride);

        uint16x8_t sum = vaddq_u16(s0, s1);
        uint8x8_t d0 = vqrshrn_n_u16(sum, 2);

        vst1_u8(dst_ptr, d0);

        im_ptr += im_stride;
        dst_ptr += dst_stride;
      } while (--height != 0);
      im += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
