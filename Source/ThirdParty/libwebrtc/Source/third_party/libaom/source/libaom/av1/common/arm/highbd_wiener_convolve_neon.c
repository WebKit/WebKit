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

#include <arm_neon.h>
#include <assert.h>

#include "aom_dsp/arm/mem_neon.h"
#include "av1/common/convolve.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#define HBD_WIENER_5TAP_HORIZ(name, shift)                              \
  static inline uint16x8_t name##_wiener_convolve5_8_2d_h(              \
      const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,       \
      const int16x8_t s3, const int16x8_t s4, const int16x4_t x_filter, \
      const int32x4_t round_vec, const uint16x8_t im_max_val) {         \
    /* Wiener filter is symmetric so add mirrored source elements. */   \
    int16x8_t s04 = vaddq_s16(s0, s4);                                  \
    int16x8_t s13 = vaddq_s16(s1, s3);                                  \
                                                                        \
    /* x_filter[0] = 0. (5-tap filters are 0-padded to 7 taps.) */      \
    int32x4_t sum_lo =                                                  \
        vmlal_lane_s16(round_vec, vget_low_s16(s04), x_filter, 1);      \
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s13), x_filter, 2);    \
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s2), x_filter, 3);     \
                                                                        \
    int32x4_t sum_hi =                                                  \
        vmlal_lane_s16(round_vec, vget_high_s16(s04), x_filter, 1);     \
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s13), x_filter, 2);   \
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s2), x_filter, 3);    \
                                                                        \
    uint16x4_t res_lo = vqrshrun_n_s32(sum_lo, shift);                  \
    uint16x4_t res_hi = vqrshrun_n_s32(sum_hi, shift);                  \
                                                                        \
    return vminq_u16(vcombine_u16(res_lo, res_hi), im_max_val);         \
  }                                                                     \
                                                                        \
  static inline void name##_convolve_add_src_5tap_horiz(                \
      const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr, \
      ptrdiff_t dst_stride, int w, int h, const int16x4_t x_filter,     \
      const int32x4_t round_vec, const uint16x8_t im_max_val) {         \
    do {                                                                \
      const int16_t *s = (int16_t *)src_ptr;                            \
      uint16_t *d = dst_ptr;                                            \
      int width = w;                                                    \
                                                                        \
      do {                                                              \
        int16x8_t s0, s1, s2, s3, s4;                                   \
        load_s16_8x5(s, 1, &s0, &s1, &s2, &s3, &s4);                    \
                                                                        \
        uint16x8_t d0 = name##_wiener_convolve5_8_2d_h(                 \
            s0, s1, s2, s3, s4, x_filter, round_vec, im_max_val);       \
                                                                        \
        vst1q_u16(d, d0);                                               \
                                                                        \
        s += 8;                                                         \
        d += 8;                                                         \
        width -= 8;                                                     \
      } while (width != 0);                                             \
      src_ptr += src_stride;                                            \
      dst_ptr += dst_stride;                                            \
    } while (--h != 0);                                                 \
  }

HBD_WIENER_5TAP_HORIZ(highbd, WIENER_ROUND0_BITS)
HBD_WIENER_5TAP_HORIZ(highbd_12, WIENER_ROUND0_BITS + 2)

#undef HBD_WIENER_5TAP_HORIZ

#define HBD_WIENER_7TAP_HORIZ(name, shift)                                     \
  static inline uint16x8_t name##_wiener_convolve7_8_2d_h(                     \
      const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,              \
      const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,              \
      const int16x8_t s6, const int16x4_t x_filter, const int32x4_t round_vec, \
      const uint16x8_t im_max_val) {                                           \
    /* Wiener filter is symmetric so add mirrored source elements. */          \
    int16x8_t s06 = vaddq_s16(s0, s6);                                         \
    int16x8_t s15 = vaddq_s16(s1, s5);                                         \
    int16x8_t s24 = vaddq_s16(s2, s4);                                         \
                                                                               \
    int32x4_t sum_lo =                                                         \
        vmlal_lane_s16(round_vec, vget_low_s16(s06), x_filter, 0);             \
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s15), x_filter, 1);           \
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s24), x_filter, 2);           \
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s3), x_filter, 3);            \
                                                                               \
    int32x4_t sum_hi =                                                         \
        vmlal_lane_s16(round_vec, vget_high_s16(s06), x_filter, 0);            \
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s15), x_filter, 1);          \
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s24), x_filter, 2);          \
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s3), x_filter, 3);           \
                                                                               \
    uint16x4_t res_lo = vqrshrun_n_s32(sum_lo, shift);                         \
    uint16x4_t res_hi = vqrshrun_n_s32(sum_hi, shift);                         \
                                                                               \
    return vminq_u16(vcombine_u16(res_lo, res_hi), im_max_val);                \
  }                                                                            \
                                                                               \
  static inline void name##_convolve_add_src_7tap_horiz(                       \
      const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,        \
      ptrdiff_t dst_stride, int w, int h, const int16x4_t x_filter,            \
      const int32x4_t round_vec, const uint16x8_t im_max_val) {                \
    do {                                                                       \
      const int16_t *s = (int16_t *)src_ptr;                                   \
      uint16_t *d = dst_ptr;                                                   \
      int width = w;                                                           \
                                                                               \
      do {                                                                     \
        int16x8_t s0, s1, s2, s3, s4, s5, s6;                                  \
        load_s16_8x7(s, 1, &s0, &s1, &s2, &s3, &s4, &s5, &s6);                 \
                                                                               \
        uint16x8_t d0 = name##_wiener_convolve7_8_2d_h(                        \
            s0, s1, s2, s3, s4, s5, s6, x_filter, round_vec, im_max_val);      \
                                                                               \
        vst1q_u16(d, d0);                                                      \
                                                                               \
        s += 8;                                                                \
        d += 8;                                                                \
        width -= 8;                                                            \
      } while (width != 0);                                                    \
      src_ptr += src_stride;                                                   \
      dst_ptr += dst_stride;                                                   \
    } while (--h != 0);                                                        \
  }

HBD_WIENER_7TAP_HORIZ(highbd, WIENER_ROUND0_BITS)
HBD_WIENER_7TAP_HORIZ(highbd_12, WIENER_ROUND0_BITS + 2)

#undef HBD_WIENER_7TAP_HORIZ

#define HBD_WIENER_5TAP_VERT(name, shift)                                     \
  static inline uint16x8_t name##_wiener_convolve5_8_2d_v(                    \
      const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,             \
      const int16x8_t s3, const int16x8_t s4, const int16x4_t y_filter,       \
      const int32x4_t round_vec, const uint16x8_t res_max_val) {              \
    const int32x2_t y_filter_lo = vget_low_s32(vmovl_s16(y_filter));          \
    const int32x2_t y_filter_hi = vget_high_s32(vmovl_s16(y_filter));         \
    /* Wiener filter is symmetric so add mirrored source elements. */         \
    int32x4_t s04_lo = vaddl_s16(vget_low_s16(s0), vget_low_s16(s4));         \
    int32x4_t s13_lo = vaddl_s16(vget_low_s16(s1), vget_low_s16(s3));         \
                                                                              \
    /* y_filter[0] = 0. (5-tap filters are 0-padded to 7 taps.) */            \
    int32x4_t sum_lo = vmlaq_lane_s32(round_vec, s04_lo, y_filter_lo, 1);     \
    sum_lo = vmlaq_lane_s32(sum_lo, s13_lo, y_filter_hi, 0);                  \
    sum_lo =                                                                  \
        vmlaq_lane_s32(sum_lo, vmovl_s16(vget_low_s16(s2)), y_filter_hi, 1);  \
                                                                              \
    int32x4_t s04_hi = vaddl_s16(vget_high_s16(s0), vget_high_s16(s4));       \
    int32x4_t s13_hi = vaddl_s16(vget_high_s16(s1), vget_high_s16(s3));       \
                                                                              \
    int32x4_t sum_hi = vmlaq_lane_s32(round_vec, s04_hi, y_filter_lo, 1);     \
    sum_hi = vmlaq_lane_s32(sum_hi, s13_hi, y_filter_hi, 0);                  \
    sum_hi =                                                                  \
        vmlaq_lane_s32(sum_hi, vmovl_s16(vget_high_s16(s2)), y_filter_hi, 1); \
                                                                              \
    uint16x4_t res_lo = vqrshrun_n_s32(sum_lo, shift);                        \
    uint16x4_t res_hi = vqrshrun_n_s32(sum_hi, shift);                        \
                                                                              \
    return vminq_u16(vcombine_u16(res_lo, res_hi), res_max_val);              \
  }                                                                           \
                                                                              \
  static inline void name##_convolve_add_src_5tap_vert(                       \
      const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,       \
      ptrdiff_t dst_stride, int w, int h, const int16x4_t y_filter,           \
      const int32x4_t round_vec, const uint16x8_t res_max_val) {              \
    do {                                                                      \
      const int16_t *s = (int16_t *)src_ptr;                                  \
      uint16_t *d = dst_ptr;                                                  \
      int height = h;                                                         \
                                                                              \
      while (height > 3) {                                                    \
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;                             \
        load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);  \
                                                                              \
        uint16x8_t d0 = name##_wiener_convolve5_8_2d_v(                       \
            s0, s1, s2, s3, s4, y_filter, round_vec, res_max_val);            \
        uint16x8_t d1 = name##_wiener_convolve5_8_2d_v(                       \
            s1, s2, s3, s4, s5, y_filter, round_vec, res_max_val);            \
        uint16x8_t d2 = name##_wiener_convolve5_8_2d_v(                       \
            s2, s3, s4, s5, s6, y_filter, round_vec, res_max_val);            \
        uint16x8_t d3 = name##_wiener_convolve5_8_2d_v(                       \
            s3, s4, s5, s6, s7, y_filter, round_vec, res_max_val);            \
                                                                              \
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);                         \
                                                                              \
        s += 4 * src_stride;                                                  \
        d += 4 * dst_stride;                                                  \
        height -= 4;                                                          \
      }                                                                       \
                                                                              \
      while (height-- != 0) {                                                 \
        int16x8_t s0, s1, s2, s3, s4;                                         \
        load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);                 \
                                                                              \
        uint16x8_t d0 = name##_wiener_convolve5_8_2d_v(                       \
            s0, s1, s2, s3, s4, y_filter, round_vec, res_max_val);            \
                                                                              \
        vst1q_u16(d, d0);                                                     \
                                                                              \
        s += src_stride;                                                      \
        d += dst_stride;                                                      \
      }                                                                       \
                                                                              \
      src_ptr += 8;                                                           \
      dst_ptr += 8;                                                           \
      w -= 8;                                                                 \
    } while (w != 0);                                                         \
  }

HBD_WIENER_5TAP_VERT(highbd, 2 * FILTER_BITS - WIENER_ROUND0_BITS)
HBD_WIENER_5TAP_VERT(highbd_12, 2 * FILTER_BITS - WIENER_ROUND0_BITS - 2)

#undef HBD_WIENER_5TAP_VERT

#define HBD_WIENER_7TAP_VERT(name, shift)                                      \
  static inline uint16x8_t name##_wiener_convolve7_8_2d_v(                     \
      const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,              \
      const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,              \
      const int16x8_t s6, const int16x4_t y_filter, const int32x4_t round_vec, \
      const uint16x8_t res_max_val) {                                          \
    const int32x2_t y_filter_lo = vget_low_s32(vmovl_s16(y_filter));           \
    const int32x2_t y_filter_hi = vget_high_s32(vmovl_s16(y_filter));          \
    /* Wiener filter is symmetric so add mirrored source elements. */          \
    int32x4_t s06_lo = vaddl_s16(vget_low_s16(s0), vget_low_s16(s6));          \
    int32x4_t s15_lo = vaddl_s16(vget_low_s16(s1), vget_low_s16(s5));          \
    int32x4_t s24_lo = vaddl_s16(vget_low_s16(s2), vget_low_s16(s4));          \
                                                                               \
    int32x4_t sum_lo = vmlaq_lane_s32(round_vec, s06_lo, y_filter_lo, 0);      \
    sum_lo = vmlaq_lane_s32(sum_lo, s15_lo, y_filter_lo, 1);                   \
    sum_lo = vmlaq_lane_s32(sum_lo, s24_lo, y_filter_hi, 0);                   \
    sum_lo =                                                                   \
        vmlaq_lane_s32(sum_lo, vmovl_s16(vget_low_s16(s3)), y_filter_hi, 1);   \
                                                                               \
    int32x4_t s06_hi = vaddl_s16(vget_high_s16(s0), vget_high_s16(s6));        \
    int32x4_t s15_hi = vaddl_s16(vget_high_s16(s1), vget_high_s16(s5));        \
    int32x4_t s24_hi = vaddl_s16(vget_high_s16(s2), vget_high_s16(s4));        \
                                                                               \
    int32x4_t sum_hi = vmlaq_lane_s32(round_vec, s06_hi, y_filter_lo, 0);      \
    sum_hi = vmlaq_lane_s32(sum_hi, s15_hi, y_filter_lo, 1);                   \
    sum_hi = vmlaq_lane_s32(sum_hi, s24_hi, y_filter_hi, 0);                   \
    sum_hi =                                                                   \
        vmlaq_lane_s32(sum_hi, vmovl_s16(vget_high_s16(s3)), y_filter_hi, 1);  \
                                                                               \
    uint16x4_t res_lo = vqrshrun_n_s32(sum_lo, shift);                         \
    uint16x4_t res_hi = vqrshrun_n_s32(sum_hi, shift);                         \
                                                                               \
    return vminq_u16(vcombine_u16(res_lo, res_hi), res_max_val);               \
  }                                                                            \
                                                                               \
  static inline void name##_convolve_add_src_7tap_vert(                        \
      const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,        \
      ptrdiff_t dst_stride, int w, int h, const int16x4_t y_filter,            \
      const int32x4_t round_vec, const uint16x8_t res_max_val) {               \
    do {                                                                       \
      const int16_t *s = (int16_t *)src_ptr;                                   \
      uint16_t *d = dst_ptr;                                                   \
      int height = h;                                                          \
                                                                               \
      while (height > 3) {                                                     \
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;                      \
        load_s16_8x10(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,   \
                      &s8, &s9);                                               \
                                                                               \
        uint16x8_t d0 = name##_wiener_convolve7_8_2d_v(                        \
            s0, s1, s2, s3, s4, s5, s6, y_filter, round_vec, res_max_val);     \
        uint16x8_t d1 = name##_wiener_convolve7_8_2d_v(                        \
            s1, s2, s3, s4, s5, s6, s7, y_filter, round_vec, res_max_val);     \
        uint16x8_t d2 = name##_wiener_convolve7_8_2d_v(                        \
            s2, s3, s4, s5, s6, s7, s8, y_filter, round_vec, res_max_val);     \
        uint16x8_t d3 = name##_wiener_convolve7_8_2d_v(                        \
            s3, s4, s5, s6, s7, s8, s9, y_filter, round_vec, res_max_val);     \
                                                                               \
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);                          \
                                                                               \
        s += 4 * src_stride;                                                   \
        d += 4 * dst_stride;                                                   \
        height -= 4;                                                           \
      }                                                                        \
                                                                               \
      while (height-- != 0) {                                                  \
        int16x8_t s0, s1, s2, s3, s4, s5, s6;                                  \
        load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);        \
                                                                               \
        uint16x8_t d0 = name##_wiener_convolve7_8_2d_v(                        \
            s0, s1, s2, s3, s4, s5, s6, y_filter, round_vec, res_max_val);     \
                                                                               \
        vst1q_u16(d, d0);                                                      \
                                                                               \
        s += src_stride;                                                       \
        d += dst_stride;                                                       \
      }                                                                        \
                                                                               \
      src_ptr += 8;                                                            \
      dst_ptr += 8;                                                            \
      w -= 8;                                                                  \
    } while (w != 0);                                                          \
  }

HBD_WIENER_7TAP_VERT(highbd, 2 * FILTER_BITS - WIENER_ROUND0_BITS)
HBD_WIENER_7TAP_VERT(highbd_12, 2 * FILTER_BITS - WIENER_ROUND0_BITS - 2)

#undef HBD_WIENER_7TAP_VERT

static inline int get_wiener_filter_taps(const int16_t *filter) {
  assert(filter[7] == 0);
  if (filter[0] == 0 && filter[6] == 0) {
    return WIENER_WIN_REDUCED;
  }
  return WIENER_WIN;
}

void av1_highbd_wiener_convolve_add_src_neon(
    const uint8_t *src8, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const int16_t *x_filter, int x_step_q4,
    const int16_t *y_filter, int y_step_q4, int w, int h,
    const WienerConvolveParams *conv_params, int bd) {
  (void)x_step_q4;
  (void)y_step_q4;

  assert(w % 8 == 0);
  assert(w <= MAX_SB_SIZE && h <= MAX_SB_SIZE);
  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(x_filter[7] == 0 && y_filter[7] == 0);

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

  const int extraprec_clamp_limit =
      WIENER_CLAMP_LIMIT(conv_params->round_0, bd);
  const uint16x8_t im_max_val = vdupq_n_u16(extraprec_clamp_limit - 1);
  const int32x4_t horiz_round_vec = vdupq_n_s32(1 << (bd + FILTER_BITS - 1));

  const uint16x8_t res_max_val = vdupq_n_u16((1 << bd) - 1);
  const int32x4_t vert_round_vec =
      vdupq_n_s32(-(1 << (bd + conv_params->round_1 - 1)));

  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

  if (bd == 12) {
    if (x_filter_taps == WIENER_WIN_REDUCED) {
      highbd_12_convolve_add_src_5tap_horiz(
          src - horiz_offset - vert_offset, src_stride, im_block, im_stride, w,
          im_h, x_filter_s16, horiz_round_vec, im_max_val);
    } else {
      highbd_12_convolve_add_src_7tap_horiz(
          src - horiz_offset - vert_offset, src_stride, im_block, im_stride, w,
          im_h, x_filter_s16, horiz_round_vec, im_max_val);
    }

    if (y_filter_taps == WIENER_WIN_REDUCED) {
      highbd_12_convolve_add_src_5tap_vert(im_block, im_stride, dst, dst_stride,
                                           w, h, y_filter_s16, vert_round_vec,
                                           res_max_val);
    } else {
      highbd_12_convolve_add_src_7tap_vert(im_block, im_stride, dst, dst_stride,
                                           w, h, y_filter_s16, vert_round_vec,
                                           res_max_val);
    }

  } else {
    if (x_filter_taps == WIENER_WIN_REDUCED) {
      highbd_convolve_add_src_5tap_horiz(
          src - horiz_offset - vert_offset, src_stride, im_block, im_stride, w,
          im_h, x_filter_s16, horiz_round_vec, im_max_val);
    } else {
      highbd_convolve_add_src_7tap_horiz(
          src - horiz_offset - vert_offset, src_stride, im_block, im_stride, w,
          im_h, x_filter_s16, horiz_round_vec, im_max_val);
    }

    if (y_filter_taps == WIENER_WIN_REDUCED) {
      highbd_convolve_add_src_5tap_vert(im_block, im_stride, dst, dst_stride, w,
                                        h, y_filter_s16, vert_round_vec,
                                        res_max_val);
    } else {
      highbd_convolve_add_src_7tap_vert(im_block, im_stride, dst, dst_stride, w,
                                        h, y_filter_s16, vert_round_vec,
                                        res_max_val);
    }
  }
}
