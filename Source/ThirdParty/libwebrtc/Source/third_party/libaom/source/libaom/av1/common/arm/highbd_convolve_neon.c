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

#include <assert.h>
#include <arm_neon.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/highbd_convolve_neon.h"

static INLINE void highbd_convolve_y_sr_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int bd) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int32x4_t zero_s32 = vdupq_n_s32(0);

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
    uint16x4_t d0, d1, d2, d3;
    uint16x8_t d01, d23;
    const int16_t *s = (const int16_t *)(src_ptr + src_stride);
    uint16_t *d = dst_ptr;

    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      d0 = highbd_convolve6_4_s32_s16(s0, s1, s2, s3, s4, s5, y_filter_0_7,
                                      zero_s32);
      d1 = highbd_convolve6_4_s32_s16(s1, s2, s3, s4, s5, s6, y_filter_0_7,
                                      zero_s32);
      d2 = highbd_convolve6_4_s32_s16(s2, s3, s4, s5, s6, s7, y_filter_0_7,
                                      zero_s32);
      d3 = highbd_convolve6_4_s32_s16(s3, s4, s5, s6, s7, s8, y_filter_0_7,
                                      zero_s32);

      d01 = vcombine_u16(d0, d1);
      d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u16q_2x1(d + 2 * dst_stride, d23, 0);
          store_u16q_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
        if (h != 2) {
          vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
          vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    // if width is a multiple of 8 & height is a multiple of 4
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
    uint16x8_t d0, d1, d2, d3;

    do {
      int height = h;
      const int16_t *s = (const int16_t *)(src_ptr + src_stride);
      uint16_t *d = dst_ptr;

      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        d0 = highbd_convolve6_8_s32_s16(s0, s1, s2, s3, s4, s5, y_filter_0_7,
                                        zero_s32);
        d1 = highbd_convolve6_8_s32_s16(s1, s2, s3, s4, s5, s6, y_filter_0_7,
                                        zero_s32);
        d2 = highbd_convolve6_8_s32_s16(s2, s3, s4, s5, s6, s7, y_filter_0_7,
                                        zero_s32);
        d3 = highbd_convolve6_8_s32_s16(s3, s4, s5, s6, s7, s8, y_filter_0_7,
                                        zero_s32);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
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

static INLINE void highbd_convolve_y_sr_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int bd) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t zero_s32 = vdupq_n_s32(0);

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint16x4_t d0, d1, d2, d3;
    uint16x8_t d01, d23;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      d0 = highbd_convolve8_4_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                      zero_s32);
      d1 = highbd_convolve8_4_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                      zero_s32);
      d2 = highbd_convolve8_4_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                      zero_s32);
      d3 = highbd_convolve8_4_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                      zero_s32);

      d01 = vcombine_u16(d0, d1);
      d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u16q_2x1(d + 2 * dst_stride, d23, 0);
          store_u16q_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
        if (h != 2) {
          vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
          vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));
        }
      }

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
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint16x8_t d0, d1, d2, d3;
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        d0 = highbd_convolve8_8_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7,
                                        y_filter, zero_s32);
        d1 = highbd_convolve8_8_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8,
                                        y_filter, zero_s32);
        d2 = highbd_convolve8_8_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9,
                                        y_filter, zero_s32);
        d3 = highbd_convolve8_8_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10,
                                        y_filter, zero_s32);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

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

static INLINE void highbd_convolve_y_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int bd) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);
  const int32x4_t zero_s32 = vdupq_n_s32(0);

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
    uint16x4_t d0, d1, d2, d3;
    uint16x8_t d01, d23;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &s10);
    s += 11 * src_stride;

    do {
      load_s16_4x4(s, src_stride, &s11, &s12, &s13, &s14);

      d0 = highbd_convolve12_y_4_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                         s10, s11, y_filter_0_7, y_filter_8_11,
                                         zero_s32);
      d1 = highbd_convolve12_y_4_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                         s10, s11, s12, y_filter_0_7,
                                         y_filter_8_11, zero_s32);
      d2 = highbd_convolve12_y_4_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                         s11, s12, s13, y_filter_0_7,
                                         y_filter_8_11, zero_s32);
      d3 = highbd_convolve12_y_4_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                         s12, s13, s14, y_filter_0_7,
                                         y_filter_8_11, zero_s32);

      d01 = vcombine_u16(d0, d1);
      d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u16q_2x1(d + 2 * dst_stride, d23, 0);
          store_u16q_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
        if (h != 2) {
          vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
          vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));
        }
      }

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
    } while (h > 0);
  } else {
    uint16x8_t d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        d0 = highbd_convolve12_y_8_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, s8,
                                           s9, s10, s11, y_filter_0_7,
                                           y_filter_8_11, zero_s32);
        d1 = highbd_convolve12_y_8_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                           s10, s11, s12, y_filter_0_7,
                                           y_filter_8_11, zero_s32);
        d2 = highbd_convolve12_y_8_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                           s11, s12, s13, y_filter_0_7,
                                           y_filter_8_11, zero_s32);
        d3 = highbd_convolve12_y_8_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                           s12, s13, s14, y_filter_0_7,
                                           y_filter_8_11, zero_s32);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

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
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void av1_highbd_convolve_y_sr_neon(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_y_qn, int bd) {
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int vert_zero_s32 = filter_params_y->taps / 2 - 1;
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_zero_s32 * src_stride;

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

static INLINE void highbd_convolve_x_sr_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    int bd) {
  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int bits = FILTER_BITS - conv_params->round_0;
  const int16x8_t bits_s16 = vdupq_n_s16(-bits);
  const int32x4_t zero_s32 = vdupq_n_s32(0);

  if (w <= 4) {
    int16x8_t s0, s1, s2, s3;
    uint16x4_t d0, d1;
    uint16x8_t d01;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      load_s16_8x2(s, src_stride, &s0, &s2);
      load_s16_8x2(s + 8, src_stride, &s1, &s3);

      d0 = highbd_convolve8_horiz4_s32_s16(s0, s1, x_filter, shift_s32,
                                           zero_s32);
      d1 = highbd_convolve8_horiz4_s32_s16(s2, s3, x_filter, shift_s32,
                                           zero_s32);

      d01 = vcombine_u16(d0, d1);
      d01 = vqrshlq_u16(d01, bits_s16);
      d01 = vminq_u16(d01, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
      }

      s += 2 * src_stride;
      d += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    int height = h;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint16x8_t d0, d1, d2, d3;
    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x4(s, src_stride, &s0, &s2, &s4, &s6);
      s += 8;

      do {
        load_s16_8x4(s, src_stride, &s1, &s3, &s5, &s7);

        d0 = highbd_convolve8_horiz8_s32_s16(s0, s1, x_filter, shift_s32,
                                             zero_s32);
        d1 = highbd_convolve8_horiz8_s32_s16(s2, s3, x_filter, shift_s32,
                                             zero_s32);
        d2 = highbd_convolve8_horiz8_s32_s16(s4, s5, x_filter, shift_s32,
                                             zero_s32);
        d3 = highbd_convolve8_horiz8_s32_s16(s6, s7, x_filter, shift_s32,
                                             zero_s32);

        d0 = vqrshlq_u16(d0, bits_s16);
        d1 = vqrshlq_u16(d1, bits_s16);
        d2 = vqrshlq_u16(d2, bits_s16);
        d3 = vqrshlq_u16(d3, bits_s16);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

        s0 = s1;
        s2 = s3;
        s4 = s5;
        s6 = s7;
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

static INLINE void highbd_convolve_x_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    int bd) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int bits = FILTER_BITS - conv_params->round_0;
  const int16x8_t bits_s16 = vdupq_n_s16(-bits);
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int32x4_t zero_s32 = vdupq_n_s32(0);

  if (w <= 4) {
    int16x8_t s0, s1, s2, s3;
    uint16x4_t d0, d1;
    uint16x8_t d01;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      load_s16_8x2(s, src_stride, &s0, &s2);
      load_s16_8x2(s + 8, src_stride, &s1, &s3);

      d0 = highbd_convolve12_horiz4_s32_s16(s0, s1, x_filter_0_7, x_filter_8_11,
                                            shift_s32, zero_s32);
      d1 = highbd_convolve12_horiz4_s32_s16(s2, s3, x_filter_0_7, x_filter_8_11,
                                            shift_s32, zero_s32);

      d01 = vcombine_u16(d0, d1);
      d01 = vqrshlq_u16(d01, bits_s16);
      d01 = vminq_u16(d01, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
      }

      s += 2 * src_stride;
      d += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    int height = h;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uint16x8_t d0, d1, d2, d3;
    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x4(s, src_stride, &s0, &s3, &s6, &s9);
      s += 8;

      do {
        load_s16_8x4(s, src_stride, &s1, &s4, &s7, &s10);
        load_s16_8x4(s + 8, src_stride, &s2, &s5, &s8, &s11);

        d0 = highbd_convolve12_horiz8_s32_s16(
            s0, s1, s2, x_filter_0_7, x_filter_8_11, shift_s32, zero_s32);
        d1 = highbd_convolve12_horiz8_s32_s16(
            s3, s4, s5, x_filter_0_7, x_filter_8_11, shift_s32, zero_s32);
        d2 = highbd_convolve12_horiz8_s32_s16(
            s6, s7, s8, x_filter_0_7, x_filter_8_11, shift_s32, zero_s32);
        d3 = highbd_convolve12_horiz8_s32_s16(
            s9, s10, s11, x_filter_0_7, x_filter_8_11, shift_s32, zero_s32);

        d0 = vqrshlq_u16(d0, bits_s16);
        d1 = vqrshlq_u16(d1, bits_s16);
        d2 = vqrshlq_u16(d2, bits_s16);
        d3 = vqrshlq_u16(d3, bits_s16);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

        s0 = s1;
        s1 = s2;
        s3 = s4;
        s4 = s5;
        s6 = s7;
        s7 = s8;
        s9 = s10;
        s10 = s11;
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

void av1_highbd_convolve_x_sr_neon(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const int subpel_x_qn,
                                   ConvolveParams *conv_params, int bd) {
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int horiz_zero_s32 = filter_params_x->taps / 2 - 1;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_zero_s32;

  if (x_filter_taps > 8) {
    highbd_convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                                    x_filter_ptr, conv_params, bd);
    return;
  }

  highbd_convolve_x_sr_8tap_neon(src, src_stride, dst, dst_stride, w, h,
                                 x_filter_ptr, conv_params, bd);
}

static INLINE void highbd_convolve_2d_y_sr_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    int bd, const int offset, const int correction) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);
  const int round1_shift = conv_params->round_1;
  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_shift);
  const int32x4_t correction_s32 = vdupq_n_s32(correction);

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint16x4_t d0, d1, d2, d3;
    uint16x8_t d01, d23;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      d0 = highbd_convolve8_4_rs_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7,
                                         y_filter, round1_shift_s32, offset_s32,
                                         correction_s32);
      d1 = highbd_convolve8_4_rs_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8,
                                         y_filter, round1_shift_s32, offset_s32,
                                         correction_s32);
      d2 = highbd_convolve8_4_rs_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9,
                                         y_filter, round1_shift_s32, offset_s32,
                                         correction_s32);
      d3 = highbd_convolve8_4_rs_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10,
                                         y_filter, round1_shift_s32, offset_s32,
                                         correction_s32);

      d01 = vcombine_u16(d0, d1);
      d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u16q_2x1(d + 2 * dst_stride, d23, 0);
          store_u16q_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
        if (h != 2) {
          vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
          vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));
        }
      }

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
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    uint16x8_t d0, d1, d2, d3;
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        d0 = highbd_convolve8_8_rs_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7,
                                           y_filter, round1_shift_s32,
                                           offset_s32, correction_s32);
        d1 = highbd_convolve8_8_rs_s32_s16(s1, s2, s3, s4, s5, s6, s7, s8,
                                           y_filter, round1_shift_s32,
                                           offset_s32, correction_s32);
        d2 = highbd_convolve8_8_rs_s32_s16(s2, s3, s4, s5, s6, s7, s8, s9,
                                           y_filter, round1_shift_s32,
                                           offset_s32, correction_s32);
        d3 = highbd_convolve8_8_rs_s32_s16(s3, s4, s5, s6, s7, s8, s9, s10,
                                           y_filter, round1_shift_s32,
                                           offset_s32, correction_s32);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

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

static INLINE void highbd_convolve_2d_y_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    const int bd, const int offset, const int correction) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);
  const int round1_shift = conv_params->round_1;
  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_shift);
  const int32x4_t correction_s32 = vdupq_n_s32(correction);

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
    uint16x4_t d0, d1, d2, d3;
    uint16x8_t d01, d23;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &s10);
    s += 11 * src_stride;

    do {
      load_s16_4x4(s, src_stride, &s11, &s12, &s13, &s14);

      d0 = highbd_convolve12_y_4_rs_s32_s16(
          s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
      d1 = highbd_convolve12_y_4_rs_s32_s16(
          s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
      d2 = highbd_convolve12_y_4_rs_s32_s16(
          s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
      d3 = highbd_convolve12_y_4_rs_s32_s16(
          s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, y_filter_0_7,
          y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);

      d01 = vcombine_u16(d0, d1);
      d23 = vcombine_u16(d2, d3);

      d01 = vminq_u16(d01, max);
      d23 = vminq_u16(d23, max);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u16q_2x1(d + 2 * dst_stride, d23, 0);
          store_u16q_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
        if (h != 2) {
          vst1_u16(d + 2 * dst_stride, vget_low_u16(d23));
          vst1_u16(d + 3 * dst_stride, vget_high_u16(d23));
        }
      }

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
    } while (h > 0);
  } else {
    uint16x8_t d0, d1, d2, d3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;

    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        d0 = highbd_convolve12_y_8_rs_s32_s16(
            s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
        d1 = highbd_convolve12_y_8_rs_s32_s16(
            s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
        d2 = highbd_convolve12_y_8_rs_s32_s16(
            s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);
        d3 = highbd_convolve12_y_8_rs_s32_s16(
            s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, y_filter_0_7,
            y_filter_8_11, round1_shift_s32, offset_s32, correction_s32);

        d0 = vminq_u16(d0, max);
        d1 = vminq_u16(d1, max);
        d2 = vminq_u16(d2, max);
        d3 = vminq_u16(d3, max);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

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
      } while (height > 0);

      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE void highbd_convolve_2d_x_sr_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset) {
  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  if (w <= 4) {
    int16x8_t s0, s1, s2, s3;
    uint16x4_t d0, d1;
    uint16x8_t d01;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      load_s16_8x2(s, src_stride, &s0, &s2);
      load_s16_8x2(s + 8, src_stride, &s1, &s3);

      d0 = highbd_convolve8_horiz4_s32_s16(s0, s1, x_filter, shift_s32,
                                           offset_s32);
      d1 = highbd_convolve8_horiz4_s32_s16(s2, s3, x_filter, shift_s32,
                                           offset_s32);

      d01 = vcombine_u16(d0, d1);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
      }

      s += 2 * src_stride;
      d += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    int height = h;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint16x8_t d0, d1, d2, d3;
    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x4(s, src_stride, &s0, &s2, &s4, &s6);
      s += 8;

      do {
        load_s16_8x4(s, src_stride, &s1, &s3, &s5, &s7);

        d0 = highbd_convolve8_horiz8_s32_s16(s0, s1, x_filter, shift_s32,
                                             offset_s32);
        d1 = highbd_convolve8_horiz8_s32_s16(s2, s3, x_filter, shift_s32,
                                             offset_s32);
        d2 = highbd_convolve8_horiz8_s32_s16(s4, s5, x_filter, shift_s32,
                                             offset_s32);
        d3 = highbd_convolve8_horiz8_s32_s16(s6, s7, x_filter, shift_s32,
                                             offset_s32);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

        s0 = s1;
        s2 = s3;
        s4 = s5;
        s6 = s7;
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

static INLINE void highbd_convolve_2d_x_sr_12tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset) {
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  if (w <= 4) {
    int16x8_t s0, s1, s2, s3;
    uint16x4_t d0, d1;
    uint16x8_t d01;

    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      load_s16_8x2(s, src_stride, &s0, &s2);
      load_s16_8x2(s + 8, src_stride, &s1, &s3);

      d0 = highbd_convolve12_horiz4_s32_s16(s0, s1, x_filter_0_7, x_filter_8_11,
                                            shift_s32, offset_s32);
      d1 = highbd_convolve12_horiz4_s32_s16(s2, s3, x_filter_0_7, x_filter_8_11,
                                            shift_s32, offset_s32);

      d01 = vcombine_u16(d0, d1);

      if (w == 2) {
        store_u16q_2x1(d + 0 * dst_stride, d01, 0);
        store_u16q_2x1(d + 1 * dst_stride, d01, 2);
      } else {
        vst1_u16(d + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(d + 1 * dst_stride, vget_high_u16(d01));
      }

      s += 2 * src_stride;
      d += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    int height = h;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uint16x8_t d0, d1, d2, d3;
    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      load_s16_8x4(s, src_stride, &s0, &s3, &s6, &s9);
      s += 8;

      do {
        load_s16_8x4(s, src_stride, &s1, &s4, &s7, &s10);
        load_s16_8x4(s + 8, src_stride, &s2, &s5, &s8, &s11);

        d0 = highbd_convolve12_horiz8_s32_s16(
            s0, s1, s2, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        d1 = highbd_convolve12_horiz8_s32_s16(
            s3, s4, s5, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        d2 = highbd_convolve12_horiz8_s32_s16(
            s6, s7, s8, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);
        d3 = highbd_convolve12_horiz8_s32_s16(
            s9, s10, s11, x_filter_0_7, x_filter_8_11, shift_s32, offset_s32);

        if (h == 2) {
          store_u16_8x2(d, dst_stride, d0, d1);
        } else {
          store_u16_8x4(d, dst_stride, d0, d1, d2, d3);
        }

        s0 = s1;
        s1 = s2;
        s3 = s4;
        s4 = s5;
        s6 = s7;
        s7 = s8;
        s9 = s10;
        s10 = s11;
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

void av1_highbd_convolve_2d_sr_neon(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn,
                                    const int subpel_y_qn,
                                    ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  const int im_h = h + filter_params_y->taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int x_offset_bits = (1 << (bd + FILTER_BITS - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int y_offset_initial = (1 << y_offset_bits);
  const int y_offset_correction_s32 =
      ((1 << (y_offset_bits - conv_params->round_1)) +
       (1 << (y_offset_bits - conv_params->round_1 - 1)));

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    highbd_convolve_2d_x_sr_12tap_neon(src_ptr, src_stride, im_block, im_stride,
                                       w, im_h, x_filter_ptr, conv_params,
                                       x_offset_bits);

    highbd_convolve_2d_y_sr_12tap_neon(
        im_block, im_stride, dst, dst_stride, w, h, y_filter_ptr, conv_params,
        bd, y_offset_initial, y_offset_correction_s32);
  } else {
    highbd_convolve_2d_x_sr_8tap_neon(src_ptr, src_stride, im_block, im_stride,
                                      w, im_h, x_filter_ptr, conv_params,
                                      x_offset_bits);

    highbd_convolve_2d_y_sr_8tap_neon(
        im_block, im_stride, dst, dst_stride, w, h, y_filter_ptr, conv_params,
        bd, y_offset_initial, y_offset_correction_s32);
  }
}
