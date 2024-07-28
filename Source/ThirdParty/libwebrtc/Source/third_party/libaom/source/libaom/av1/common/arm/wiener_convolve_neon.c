/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "av1/common/common.h"
#include "av1/common/restoration.h"

static INLINE uint16x8_t wiener_convolve5_8_2d_h(
    const uint8x8_t t0, const uint8x8_t t1, const uint8x8_t t2,
    const uint8x8_t t3, const uint8x8_t t4, const int16x4_t x_filter,
    const int32x4_t round_vec, const uint16x8_t im_max_val) {
  // Since the Wiener filter is symmetric about the middle tap (tap 2) add
  // mirrored source elements before multiplying filter coefficients.
  int16x8_t s04 = vreinterpretq_s16_u16(vaddl_u8(t0, t4));
  int16x8_t s13 = vreinterpretq_s16_u16(vaddl_u8(t1, t3));
  int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));

  // x_filter[0] = 0. (5-tap filters are 0-padded to 7 taps.)
  int32x4_t sum_lo = vmlal_lane_s16(round_vec, vget_low_s16(s04), x_filter, 1);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s13), x_filter, 2);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s2), x_filter, 3);

  int32x4_t sum_hi = vmlal_lane_s16(round_vec, vget_high_s16(s04), x_filter, 1);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s13), x_filter, 2);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s2), x_filter, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum_lo, WIENER_ROUND0_BITS),
                                vqrshrun_n_s32(sum_hi, WIENER_ROUND0_BITS));

  return vminq_u16(res, im_max_val);
}

static INLINE void convolve_add_src_horiz_5tap_neon(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16x4_t x_filter,
    const int32x4_t round_vec, const uint16x8_t im_max_val) {
  do {
    const uint8_t *s = src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;

    do {
      uint8x8_t s0, s1, s2, s3, s4;
      load_u8_8x5(s, 1, &s0, &s1, &s2, &s3, &s4);

      uint16x8_t d0 = wiener_convolve5_8_2d_h(s0, s1, s2, s3, s4, x_filter,
                                              round_vec, im_max_val);

      vst1q_u16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

static INLINE uint16x8_t wiener_convolve7_8_2d_h(
    const uint8x8_t t0, const uint8x8_t t1, const uint8x8_t t2,
    const uint8x8_t t3, const uint8x8_t t4, const uint8x8_t t5,
    const uint8x8_t t6, const int16x4_t x_filter, const int32x4_t round_vec,
    const uint16x8_t im_max_val) {
  // Since the Wiener filter is symmetric about the middle tap (tap 3) add
  // mirrored source elements before multiplying by filter coefficients.
  int16x8_t s06 = vreinterpretq_s16_u16(vaddl_u8(t0, t6));
  int16x8_t s15 = vreinterpretq_s16_u16(vaddl_u8(t1, t5));
  int16x8_t s24 = vreinterpretq_s16_u16(vaddl_u8(t2, t4));
  int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));

  int32x4_t sum_lo = vmlal_lane_s16(round_vec, vget_low_s16(s06), x_filter, 0);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s15), x_filter, 1);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s24), x_filter, 2);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s3), x_filter, 3);

  int32x4_t sum_hi = vmlal_lane_s16(round_vec, vget_high_s16(s06), x_filter, 0);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s15), x_filter, 1);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s24), x_filter, 2);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s3), x_filter, 3);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum_lo, WIENER_ROUND0_BITS),
                                vqrshrun_n_s32(sum_hi, WIENER_ROUND0_BITS));

  return vminq_u16(res, im_max_val);
}

static INLINE void convolve_add_src_horiz_7tap_neon(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16x4_t x_filter,
    const int32x4_t round_vec, const uint16x8_t im_max_val) {
  do {
    const uint8_t *s = src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;

    do {
      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, 1, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

      uint16x8_t d0 = wiener_convolve7_8_2d_h(s0, s1, s2, s3, s4, s5, s6,
                                              x_filter, round_vec, im_max_val);

      vst1q_u16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

static INLINE uint8x8_t wiener_convolve5_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x4_t y_filter,
    const int32x4_t round_vec) {
  // Since the Wiener filter is symmetric about the middle tap (tap 2) add
  // mirrored source elements before multiplying by filter coefficients.
  int16x8_t s04 = vaddq_s16(s0, s4);
  int16x8_t s13 = vaddq_s16(s1, s3);

  int32x4_t sum_lo = vmlal_lane_s16(round_vec, vget_low_s16(s04), y_filter, 1);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s13), y_filter, 2);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s2), y_filter, 3);

  int32x4_t sum_hi = vmlal_lane_s16(round_vec, vget_high_s16(s04), y_filter, 1);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s13), y_filter, 2);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s2), y_filter, 3);

  int16x4_t res_lo = vshrn_n_s32(sum_lo, 2 * FILTER_BITS - WIENER_ROUND0_BITS);
  int16x4_t res_hi = vshrn_n_s32(sum_hi, 2 * FILTER_BITS - WIENER_ROUND0_BITS);

  return vqmovun_s16(vcombine_s16(res_lo, res_hi));
}

static INLINE void convolve_add_src_vert_5tap_neon(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16x4_t y_filter,
    const int32x4_t round_vec) {
  do {
    const int16_t *s = (int16_t *)src;
    uint8_t *d = dst;
    int height = h;

    while (height > 3) {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      uint8x8_t d0 =
          wiener_convolve5_8_2d_v(s0, s1, s2, s3, s4, y_filter, round_vec);
      uint8x8_t d1 =
          wiener_convolve5_8_2d_v(s1, s2, s3, s4, s5, y_filter, round_vec);
      uint8x8_t d2 =
          wiener_convolve5_8_2d_v(s2, s3, s4, s5, s6, y_filter, round_vec);
      uint8x8_t d3 =
          wiener_convolve5_8_2d_v(s3, s4, s5, s6, s7, y_filter, round_vec);

      store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    }

    while (height-- != 0) {
      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);

      uint8x8_t d0 =
          wiener_convolve5_8_2d_v(s0, s1, s2, s3, s4, y_filter, round_vec);

      vst1_u8(d, d0);

      d += dst_stride;
      s += src_stride;
    }

    src += 8;
    dst += 8;
    w -= 8;
  } while (w != 0);
}

static INLINE uint8x8_t wiener_convolve7_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x4_t y_filter, const int32x4_t round_vec) {
  // Since the Wiener filter is symmetric about the middle tap (tap 3) add
  // mirrored source elements before multiplying by filter coefficients.
  int16x8_t s06 = vaddq_s16(s0, s6);
  int16x8_t s15 = vaddq_s16(s1, s5);
  int16x8_t s24 = vaddq_s16(s2, s4);

  int32x4_t sum_lo = vmlal_lane_s16(round_vec, vget_low_s16(s06), y_filter, 0);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s15), y_filter, 1);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s24), y_filter, 2);
  sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s3), y_filter, 3);

  int32x4_t sum_hi = vmlal_lane_s16(round_vec, vget_high_s16(s06), y_filter, 0);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s15), y_filter, 1);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s24), y_filter, 2);
  sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s3), y_filter, 3);

  int16x4_t res_lo = vshrn_n_s32(sum_lo, 2 * FILTER_BITS - WIENER_ROUND0_BITS);
  int16x4_t res_hi = vshrn_n_s32(sum_hi, 2 * FILTER_BITS - WIENER_ROUND0_BITS);

  return vqmovun_s16(vcombine_s16(res_lo, res_hi));
}

static INLINE void convolve_add_src_vert_7tap_neon(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16x4_t y_filter,
    const int32x4_t round_vec) {
  do {
    const int16_t *s = (int16_t *)src;
    uint8_t *d = dst;
    int height = h;

    while (height > 3) {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
      load_s16_8x10(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9);

      uint8x8_t d0 = wiener_convolve7_8_2d_v(s0, s1, s2, s3, s4, s5, s6,
                                             y_filter, round_vec);
      uint8x8_t d1 = wiener_convolve7_8_2d_v(s1, s2, s3, s4, s5, s6, s7,
                                             y_filter, round_vec);
      uint8x8_t d2 = wiener_convolve7_8_2d_v(s2, s3, s4, s5, s6, s7, s8,
                                             y_filter, round_vec);
      uint8x8_t d3 = wiener_convolve7_8_2d_v(s3, s4, s5, s6, s7, s8, s9,
                                             y_filter, round_vec);

      store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    }

    while (height-- != 0) {
      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);

      uint8x8_t d0 = wiener_convolve7_8_2d_v(s0, s1, s2, s3, s4, s5, s6,
                                             y_filter, round_vec);

      vst1_u8(d, d0);

      d += dst_stride;
      s += src_stride;
    }

    src += 8;
    dst += 8;
    w -= 8;
  } while (w != 0);
}

static AOM_INLINE int get_wiener_filter_taps(const int16_t *filter) {
  assert(filter[7] == 0);
  if (filter[0] == 0 && filter[6] == 0) {
    return WIENER_WIN_REDUCED;
  }
  return WIENER_WIN;
}

// Wiener filter 2D
// Apply horizontal filter and store in a temporary buffer. When applying
// vertical filter, overwrite the original pixel values.
void av1_wiener_convolve_add_src_neon(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *x_filter, int x_step_q4,
                                      const int16_t *y_filter, int y_step_q4,
                                      int w, int h,
                                      const WienerConvolveParams *conv_params) {
  (void)x_step_q4;
  (void)y_step_q4;
  (void)conv_params;

  assert(w % 8 == 0);
  assert(w <= MAX_SB_SIZE && h <= MAX_SB_SIZE);
  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(x_filter[7] == 0 && y_filter[7] == 0);
  // For bd == 8, assert horizontal filtering output will not exceed 15-bit:
  assert(8 + 1 + FILTER_BITS - conv_params->round_0 <= 15);

  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + WIENER_WIN - 1) * MAX_SB_SIZE]);

  const int x_filter_taps = get_wiener_filter_taps(x_filter);
  const int y_filter_taps = get_wiener_filter_taps(y_filter);
  int16x4_t x_filter_s16 = vld1_s16(x_filter);
  int16x4_t y_filter_s16 = vld1_s16(y_filter);
  // Add 128 to tap 3. (Needed for rounding.)
  x_filter_s16 = vadd_s16(x_filter_s16, vcreate_s16(128ULL << 48));
  y_filter_s16 = vadd_s16(y_filter_s16, vcreate_s16(128ULL << 48));

  const int im_stride = MAX_SB_SIZE;
  const int im_h = h + y_filter_taps - 1;
  const int horiz_offset = x_filter_taps / 2;
  const int vert_offset = (y_filter_taps / 2) * (int)src_stride;

  const int bd = 8;
  const uint16x8_t im_max_val =
      vdupq_n_u16((1 << (bd + 1 + FILTER_BITS - WIENER_ROUND0_BITS)) - 1);
  const int32x4_t horiz_round_vec = vdupq_n_s32(1 << (bd + FILTER_BITS - 1));

  const int32x4_t vert_round_vec =
      vdupq_n_s32((1 << (2 * FILTER_BITS - WIENER_ROUND0_BITS - 1)) -
                  (1 << (bd + (2 * FILTER_BITS - WIENER_ROUND0_BITS) - 1)));

  if (x_filter_taps == WIENER_WIN_REDUCED) {
    convolve_add_src_horiz_5tap_neon(src - horiz_offset - vert_offset,
                                     src_stride, im_block, im_stride, w, im_h,
                                     x_filter_s16, horiz_round_vec, im_max_val);
  } else {
    convolve_add_src_horiz_7tap_neon(src - horiz_offset - vert_offset,
                                     src_stride, im_block, im_stride, w, im_h,
                                     x_filter_s16, horiz_round_vec, im_max_val);
  }

  if (y_filter_taps == WIENER_WIN_REDUCED) {
    convolve_add_src_vert_5tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter_s16, vert_round_vec);
  } else {
    convolve_add_src_vert_7tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter_s16, vert_round_vec);
  }
}
