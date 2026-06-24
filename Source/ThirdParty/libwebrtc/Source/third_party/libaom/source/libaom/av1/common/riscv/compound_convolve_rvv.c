/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <riscv_vector.h>

#include "av1/common/riscv/compound_convolve_rvv.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

static uint16_t trans_data[64] = {
  0, 8,  16, 24, 32, 40, 48, 56, 1, 9,  17, 25, 33, 41, 49, 57,
  2, 10, 18, 26, 34, 42, 50, 58, 3, 11, 19, 27, 35, 43, 51, 59,
  4, 12, 20, 28, 36, 44, 52, 60, 5, 13, 21, 29, 37, 45, 53, 61,
  6, 14, 22, 30, 38, 46, 54, 62, 7, 15, 23, 31, 39, 47, 55, 63,
};

static inline void transpose_elems_inplace_s16_8x8_rvv(
    vint16m1_t *d0, vint16m1_t *d1, vint16m1_t *d2, vint16m1_t *d3,
    vint16m1_t *d4, vint16m1_t *d5, vint16m1_t *d6, vint16m1_t *d7,
    const vuint16m8_t trans_index) {
  vint16m8_t matrix_8x8 = __riscv_vundefined_i16m8();
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 0, *d0);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 1, *d1);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 2, *d2);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 3, *d3);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 4, *d4);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 5, *d5);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 6, *d6);
  matrix_8x8 = __riscv_vset_v_i16m1_i16m8(matrix_8x8, 7, *d7);

  // transpose_elems_inplace_s16_8x8
  vint16m8_t trans_matrix =
      __riscv_vrgather_vv_i16m8(matrix_8x8, trans_index, 64);

  *d0 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 0);
  *d1 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 1);
  *d2 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 2);
  *d3 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 3);
  *d4 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 4);
  *d5 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 5);
  *d6 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 6);
  *d7 = __riscv_vget_v_i16m8_i16m1(trans_matrix, 7);
}

static inline vint16m1_t convolve4_4_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *filter, const int16_t horiz_const,
    size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl);

  // We halved the convolution filter values so -1 from the right shift.
  return __riscv_vsra_vx_i16m1(sum, ROUND0_BITS - 1, vl);
}

static inline vint16m1_t convolve8_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const int16_t horiz_const, size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[7], s7, vl);
  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl);

  // We halved the convolution filter values so -1 from the right shift.
  return __riscv_vsra_vx_i16m1(sum, ROUND0_BITS - 1, vl);
}

static inline void dist_wtd_convolve_2d_horiz_rvv(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    const int16_t *x_filter_ptr, const int im_h, int w) {
  const int bd = 8;
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int16_t horiz_const =
      ((1 << (bd + FILTER_BITS - 2)) + (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16_t *x_filter = x_filter_ptr + 2;
    int16_t filter[4];
    for (int i = 0; i < 4; i++) filter[i] = *x_filter++ >> 1;

    src_ptr += 2;

    do {
      // load
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src_ptr + 0 * src_stride,
                                            8);  // a0 a1 a2 a3 a4 a5 a6 a7
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src_ptr + 1 * src_stride,
                                            8);  // b0 b1 b2 b3 b4 b5 b6 b7

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);

      // widen to 16-bit
      vint16m1_t s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, 8));
      vint16m1_t s01 =
          __riscv_vslide1down_vx_i16m1(s00, 0, 8);  // a1 a2 a3 a4 a5 a6 a7 0
      vint16m1_t s02 =
          __riscv_vslide1down_vx_i16m1(s01, 0, 8);  // a2 a3 a4 a5 a6 a7 0 0
      vint16m1_t s03 =
          __riscv_vslide1down_vx_i16m1(s02, 0, 8);  // a3 a4 a5 a6 a7 0 0 0

      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, 8));
      vint16m1_t s11 =
          __riscv_vslide1down_vx_i16m1(s10, 0, 8);  // b1 b2 b3 b4 b5 b6 b7 0
      vint16m1_t s12 =
          __riscv_vslide1down_vx_i16m1(s11, 0, 8);  // b2 b3 b4 b5 b6 b7 0 0
      vint16m1_t s13 =
          __riscv_vslide1down_vx_i16m1(s12, 0, 8);  // b3 b4 b5 b6 b7 0 0 0

      // perform convolution
      vint16m1_t d0 =
          convolve4_4_2d_h_rvv(s00, s01, s02, s03, filter, horiz_const, vl);
      vint16m1_t d1 =
          convolve4_4_2d_h_rvv(s10, s11, s12, s13, filter, horiz_const, vl);

      // store result
      __riscv_vse16_v_i16m1(dst_ptr + 0 * dst_stride, d0, vl);
      __riscv_vse16_v_i16m1(dst_ptr + 1 * dst_stride, d1, vl);
      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      height -= 2;
    } while (height >= 2);

    if (height > 0) {
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src_ptr + 0 * src_stride,
                                            8);  // a0 a1 a2 a3 a4 a5 a6 a7

      // widen to 16-bit
      vint16m1_t s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, 8));
      vint16m1_t s01 =
          __riscv_vslide1down_vx_i16m1(s00, 0, 8);  // a1 a2 a3 a4 a5 a6 a7 0
      vint16m1_t s02 =
          __riscv_vslide1down_vx_i16m1(s01, 0, 8);  // a2 a3 a4 a5 a6 a7 0 0
      vint16m1_t s03 =
          __riscv_vslide1down_vx_i16m1(s02, 0, 8);  // a3 a4 a5 a6 a7 0 0 0

      // perform convolution
      vint16m1_t d0 =
          convolve4_4_2d_h_rvv(s00, s01, s02, s03, filter, horiz_const, vl);

      // store result
      __riscv_vse16_v_i16m1(dst_ptr + 0 * dst_stride, d0, vl);
    }
  } else {
    // Filter values are even, so halve to reduce intermediate precision reqs.
    int16_t filter[8];
    for (int i = 0; i < 8; i++) filter[i] = *x_filter_ptr++ >> 1;

    // for VLEN = 128 case
    if ((vl == 8) && (w > 8) && (height >= 8)) {
      vuint16m8_t trans_index = __riscv_vle16_v_u16m8(trans_data, 64);

      while (height >= 8) {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                           &t7, vl);

        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(d + 4 * dst_stride);
        __builtin_prefetch(d + 5 * dst_stride);
        __builtin_prefetch(d + 6 * dst_stride);
        __builtin_prefetch(d + 7 * dst_stride);

        s += 7;

        do {
          load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                             &t7, vl);

          vint16m1_t s7 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t0, vl));
          vint16m1_t s8 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t1, vl));
          vint16m1_t s9 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t2, vl));
          vint16m1_t s10 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t3, vl));
          vint16m1_t s11 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t4, vl));
          vint16m1_t s12 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t5, vl));
          vint16m1_t s13 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t6, vl));
          vint16m1_t s14 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t7, vl));

          // Perform convolution
          vint16m1_t d0 = convolve8_8_2d_h_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                               filter, horiz_const, vl);
          vint16m1_t d1 = convolve8_8_2d_h_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                               filter, horiz_const, vl);
          vint16m1_t d2 = convolve8_8_2d_h_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                               filter, horiz_const, vl);
          vint16m1_t d3 = convolve8_8_2d_h_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                               filter, horiz_const, vl);
          vint16m1_t d4 = convolve8_8_2d_h_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                               filter, horiz_const, vl);
          vint16m1_t d5 = convolve8_8_2d_h_rvv(s5, s6, s7, s8, s9, s10, s11,
                                               s12, filter, horiz_const, vl);
          vint16m1_t d6 = convolve8_8_2d_h_rvv(s6, s7, s8, s9, s10, s11, s12,
                                               s13, filter, horiz_const, vl);
          vint16m1_t d7 = convolve8_8_2d_h_rvv(s7, s8, s9, s10, s11, s12, s13,
                                               s14, filter, horiz_const, vl);

          // transpose
          transpose_elems_inplace_s16_8x8_rvv(&d0, &d1, &d2, &d3, &d4, &d5, &d6,
                                              &d7, trans_index);

          // Store result
          store_s16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7, vl);

          // Update sliding window
          s0 = __riscv_vmv_v_v_i16m1(s8, vl);
          s1 = __riscv_vmv_v_v_i16m1(s9, vl);
          s2 = __riscv_vmv_v_v_i16m1(s10, vl);
          s3 = __riscv_vmv_v_v_i16m1(s11, vl);
          s4 = __riscv_vmv_v_v_i16m1(s12, vl);
          s5 = __riscv_vmv_v_v_i16m1(s13, vl);
          s6 = __riscv_vmv_v_v_i16m1(s14, vl);
          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src_ptr += 8 * src_stride;
        dst_ptr += 8 * dst_stride;
        height -= 8;
      }
    }

    while (height > 0) {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      __builtin_prefetch(d);

      do {
        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_u8_8x8(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit integers
        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vint16m1_t d0 = convolve8_8_2d_h_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                             filter, horiz_const, vl);

        // Store result
        __riscv_vse16_v_i16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      height--;
    }
  }
}

void av1_dist_wtd_convolve_2d_rvv(const uint8_t *src, int src_stride,
                                  uint8_t *dst8, int dst8_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int subpel_y_qn,
                                  ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;

  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  dist_wtd_convolve_2d_horiz_rvv(src_ptr, src_stride, im_block, im_stride,
                                 x_filter_ptr, im_h, w);

  if (clamped_y_taps == 6) {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_2d_vert_6tap_dist_wtd_avg_rvv(
            im_block, im_stride, dst8, dst8_stride, conv_params, y_filter_ptr,
            h, w);
      } else {
        dist_wtd_convolve_2d_vert_6tap_avg_rvv(im_block, im_stride, dst8,
                                               dst8_stride, conv_params,
                                               y_filter_ptr, h, w);
      }
    } else {
      dist_wtd_convolve_2d_vert_6tap_rvv(im_block, im_stride, conv_params,
                                         y_filter_ptr, h, w);
    }
  } else {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_2d_vert_8tap_dist_wtd_avg_rvv(
            im_block, im_stride, dst8, dst8_stride, conv_params, y_filter_ptr,
            h, w);
      } else {
        dist_wtd_convolve_2d_vert_8tap_avg_rvv(im_block, im_stride, dst8,
                                               dst8_stride, conv_params,
                                               y_filter_ptr, h, w);
      }
    } else {
      dist_wtd_convolve_2d_vert_8tap_rvv(im_block, im_stride, conv_params,
                                         y_filter_ptr, h, w);
    }
  }
}

static inline void dist_wtd_convolve_2d_copy_dist_wtd_avg_rvv(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const uint16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                                (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint8_t shift_by_bits = 1 << (FILTER_BITS - ROUND0_BITS);

  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int height = h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    vuint16mf2_t round_offset_u16 = __riscv_vmv_v_x_u16mf2(round_offset, vl);
    do {
      // load 4 block of uint8_t
      vuint8mf4_t s0 = __riscv_vle8_v_u8mf4(src + 0 * src_stride, vl);
      vuint8mf4_t s1 = __riscv_vle8_v_u8mf4(src + 1 * src_stride, vl);
      vuint8mf4_t s2 = __riscv_vle8_v_u8mf4(src + 2 * src_stride, vl);
      vuint8mf4_t s3 = __riscv_vle8_v_u8mf4(src + 3 * src_stride, vl);

      // apply shift and add round offset
      vuint16mf2_t d0 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s0, vl);
      vuint16mf2_t d1 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s1, vl);
      vuint16mf2_t d2 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s2, vl);
      vuint16mf2_t d3 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s3, vl);

      // load existing dst values
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

      // compute distance-weighted average
      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_dist_wtd_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                   fwd_offset, bck_offset, round_offset, &d0_u8,
                                   &d1_u8, &d2_u8, &d3_u8, vl);

      // store results
      __riscv_vse8_v_u8mf4(dst8 + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 3 * dst8_stride, d3_u8, vl);

      // update pointers
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  } else {
    vuint16m1_t round_offset_u16 = __riscv_vmv_v_x_u16m1(round_offset, vl);
    do {
      const uint8_t *s = src;
      CONV_BUF_TYPE *d = dst;
      uint8_t *d_u8 = dst8;
      int width = w;

      do {
        // load 4 block of uint8_t
        vuint8mf2_t s0, s1, s2, s3;
        load_u8_8x4(s, src_stride, &s0, &s1, &s2, &s3, vl);

        // apply shift and add round offset
        vuint16m1_t d0 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s0, vl);
        vuint16m1_t d1 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s1, vl);
        vuint16m1_t d2 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s2, vl);
        vuint16m1_t d3 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s3, vl);

        // load existing dst values
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        // compute distance-weighted average
        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);

        // update pointers
        s += vl;
        d += vl;
        d_u8 += vl;
        width -= vl;
      } while (width > 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline void dist_wtd_convolve_2d_copy_avg_rvv(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const uint16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                                (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint8_t shift_by_bits = 1 << (FILTER_BITS - ROUND0_BITS);

  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int height = h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    vuint16mf2_t round_offset_u16 = __riscv_vmv_v_x_u16mf2(round_offset, vl);
    do {
      // load 4 block of uint8_t
      vuint8mf4_t s0 = __riscv_vle8_v_u8mf4(src + 0 * src_stride, vl);
      vuint8mf4_t s1 = __riscv_vle8_v_u8mf4(src + 1 * src_stride, vl);
      vuint8mf4_t s2 = __riscv_vle8_v_u8mf4(src + 2 * src_stride, vl);
      vuint8mf4_t s3 = __riscv_vle8_v_u8mf4(src + 3 * src_stride, vl);

      // apply shift and add round offset
      vuint16mf2_t d0 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s0, vl);
      vuint16mf2_t d1 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s1, vl);
      vuint16mf2_t d2 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s2, vl);
      vuint16mf2_t d3 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s3, vl);

      // load existing dst values
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

      // compute basic average
      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_basic_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                vl);

      // store result
      __riscv_vse8_v_u8mf4(dst8 + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8 + 3 * dst8_stride, d3_u8, vl);

      // update pointers
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  } else {
    vuint16m1_t round_offset_u16 = __riscv_vmv_v_x_u16m1(round_offset, vl);
    do {
      const uint8_t *s = src;
      CONV_BUF_TYPE *d = dst;
      uint8_t *d_u8 = dst8;
      int width = w;

      do {
        // load 4 block of uint8_t
        vuint8mf2_t s0, s1, s2, s3;
        load_u8_8x4(s, src_stride, &s0, &s1, &s2, &s3, vl);

        // apply shift and add round offset
        vuint16m1_t d0 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s0, vl);
        vuint16m1_t d1 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s1, vl);
        vuint16m1_t d2 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s2, vl);
        vuint16m1_t d3 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s3, vl);

        // load existing dst values
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        // compute basic average
        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);

        // update pointers
        s += vl;
        d += vl;
        d_u8 += vl;
        width -= vl;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline void dist_wtd_convolve_2d_copy_rvv(const uint8_t *src,
                                                 int src_stride, int w, int h,
                                                 ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const uint16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                                (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint8_t shift_by_bits = 1 << (FILTER_BITS - ROUND0_BITS);

  CONV_BUF_TYPE *dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int height = h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    vuint16mf2_t round_offset_u16 = __riscv_vmv_v_x_u16mf2(round_offset, vl);
    do {
      // load 4 block of uint8_t
      vuint8mf4_t s0 = __riscv_vle8_v_u8mf4(src + 0 * src_stride, vl);
      vuint8mf4_t s1 = __riscv_vle8_v_u8mf4(src + 1 * src_stride, vl);
      vuint8mf4_t s2 = __riscv_vle8_v_u8mf4(src + 2 * src_stride, vl);
      vuint8mf4_t s3 = __riscv_vle8_v_u8mf4(src + 3 * src_stride, vl);

      // apply shift and add round offset
      vuint16mf2_t d0 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s0, vl);
      vuint16mf2_t d1 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s1, vl);
      vuint16mf2_t d2 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s2, vl);
      vuint16mf2_t d3 =
          __riscv_vwmaccu_vx_u16mf2(round_offset_u16, shift_by_bits, s3, vl);

      // store result
      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3, vl);

      // update pointers
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    vuint16m1_t round_offset_u16 = __riscv_vmv_v_x_u16m1(round_offset, vl);
    do {
      const uint8_t *s = src;
      CONV_BUF_TYPE *d = dst;
      int width = w;

      do {
        // load 4 block of uint8_t
        vuint8mf2_t s0, s1, s2, s3;
        load_u8_8x4(s, src_stride, &s0, &s1, &s2, &s3, vl);

        // apply shift and add round offset
        vuint16m1_t d0 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s0, vl);
        vuint16m1_t d1 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s1, vl);
        vuint16m1_t d2 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s2, vl);
        vuint16m1_t d3 =
            __riscv_vwmaccu_vx_u16m1(round_offset_u16, shift_by_bits, s3, vl);

        // store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // update pointers
        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_dist_wtd_convolve_2d_copy_rvv(const uint8_t *src, int src_stride,
                                       uint8_t *dst8, int dst8_stride, int w,
                                       int h, ConvolveParams *conv_params) {
  if (conv_params->do_average) {
    if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
      dist_wtd_convolve_2d_copy_dist_wtd_avg_rvv(
          src, src_stride, dst8, dst8_stride, w, h, conv_params);
    } else {
      dist_wtd_convolve_2d_copy_avg_rvv(src, src_stride, dst8, dst8_stride, w,
                                        h, conv_params);
    }
  } else {
    dist_wtd_convolve_2d_copy_rvv(src, src_stride, w, h, conv_params);
  }
}

static inline vuint16m1_t convolve4_4_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *filter, const int16_t round_offset,
    size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);

  // Right shift with rounding: vrsra_n_s16(round_offset, sum, ROUND0_BITS - 1)
  // vrsra_n_s16(a, b, n) = a + ((b + (1 << (n-1))) >> n)
  // We halved the convolution filter values so -1 from the right shift.
  vint16m1_t res = __riscv_vadd_vx_i16m1(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16m1(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16m1(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16m1_u16m1(res);
}

static inline vuint16m1_t convolve8_8_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const int16_t round_offset, size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[7], s7, vl);

  vint16m1_t res = __riscv_vadd_vx_i16m1(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16m1(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16m1(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16m1_u16m1(res);
}

static inline void transpose_elems_inplace_u16_8x8_rvv(
    vuint16m1_t *d0, vuint16m1_t *d1, vuint16m1_t *d2, vuint16m1_t *d3,
    vuint16m1_t *d4, vuint16m1_t *d5, vuint16m1_t *d6, vuint16m1_t *d7,
    const vuint16m8_t trans_index) {
  vuint16m8_t matrix_8x8 = __riscv_vundefined_u16m8();
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 0, *d0);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 1, *d1);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 2, *d2);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 3, *d3);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 4, *d4);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 5, *d5);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 6, *d6);
  matrix_8x8 = __riscv_vset_v_u16m1_u16m8(matrix_8x8, 7, *d7);

  // transpose_elems_inplace_u16_8x8
  vuint16m8_t trans_matrix =
      __riscv_vrgather_vv_u16m8(matrix_8x8, trans_index, 64);

  *d0 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 0);
  *d1 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 1);
  *d2 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 2);
  *d3 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 3);
  *d4 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 4);
  *d5 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 5);
  *d6 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 6);
  *d7 = __riscv_vget_v_u16m8_u16m1(trans_matrix, 7);
}

static inline void dist_wtd_convolve_x_dist_wtd_avg_rvv(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  uint8_t *dst8_ptr = dst8;
  int dst_stride = conv_params->dst_stride;
  int height = h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16_t *x_filter = x_filter_ptr + 2;
    int16_t filter[4];
    for (int i = 0; i < 4; i++) filter[i] = *x_filter++ >> 1;

    src_ptr += 2;

    do {
      // Load
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src_ptr + 0 * src_stride,
                                            8);  // a0 a1 a2 a3 a4 a5 a6 a7
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src_ptr + 1 * src_stride,
                                            8);  // b0 b1 b2 b3 b4 b5 b6 b7

      __builtin_prefetch(dst_ptr);
      __builtin_prefetch(dst8_ptr);

      // Widen to 16-bit
      vint16m1_t s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, 8));
      vint16m1_t s01 =
          __riscv_vslide1down_vx_i16m1(s00, 0, 8);  // a1 a2 a3 a4 a5 a6 a7 0
      vint16m1_t s02 =
          __riscv_vslide1down_vx_i16m1(s01, 0, 8);  // a2 a3 a4 a5 a6 a7 0 0
      vint16m1_t s03 =
          __riscv_vslide1down_vx_i16m1(s02, 0, 8);  // a3 a4 a5 a6 a7 0 0 0

      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, 8));
      vint16m1_t s11 =
          __riscv_vslide1down_vx_i16m1(s10, 0, 8);  // b1 b2 b3 b4 b5 b6 b7 0
      vint16m1_t s12 =
          __riscv_vslide1down_vx_i16m1(s11, 0, 8);  // b2 b3 b4 b5 b6 b7 0 0
      vint16m1_t s13 =
          __riscv_vslide1down_vx_i16m1(s12, 0, 8);  // b3 b4 b5 b6 b7 0 0 0

      // Perform convolution
      vuint8mf2_t d01, d11;
      vuint16m1_t d0 =
          convolve4_4_x_rvv(s00, s01, s02, s03, filter, round_offset, vl);
      vuint16m1_t d1 =
          convolve4_4_x_rvv(s10, s11, s12, s13, filter, round_offset, vl);

      // weighted average
      vuint16m1_t dd0 = __riscv_vle16_v_u16m1(dst_ptr + 0 * dst_stride, vl);
      vuint16m1_t dd1 = __riscv_vle16_v_u16m1(dst_ptr + 1 * dst_stride, vl);
      compute_dist_wtd_avg_4x1_rvv(dd0, d0, fwd_offset, bck_offset,
                                   round_offset, &d01, vl);
      compute_dist_wtd_avg_4x1_rvv(dd1, d1, fwd_offset, bck_offset,
                                   round_offset, &d11, vl);

      // Store result
      __riscv_vse8_v_u8mf2(dst8_ptr + 0 * dst8_stride, d01, vl);
      __riscv_vse8_v_u8mf2(dst8_ptr + 1 * dst8_stride, d11, vl);

      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      dst8_ptr += 2 * dst8_stride;
      height -= 2;
    } while (height >= 2);
  } else {
    // Filter values are even, so halve to reduce intermediate precision reqs.
    int16_t filter[8];
    for (int i = 0; i < 8; i++) filter[i] = *x_filter_ptr++ >> 1;

    // for VLEN = 128 case
    if ((w > 8) && (vl == 8) && (height >= 8)) {
      vuint16m8_t trans_index = __riscv_vle16_v_u16m8(trans_data, 64);

      while (height >= 8) {
        const uint8_t *s = src_ptr;
        CONV_BUF_TYPE *d = dst_ptr;
        uint8_t *d_u8 = dst8_ptr;
        int width = w;

        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                           &t7, vl);

        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(d + 4 * dst_stride);
        __builtin_prefetch(d + 5 * dst_stride);
        __builtin_prefetch(d + 6 * dst_stride);
        __builtin_prefetch(d + 7 * dst_stride);

        s += 7;

        do {
          load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                             &t7, vl);

          vint16m1_t s7 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t0, vl));
          vint16m1_t s8 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t1, vl));
          vint16m1_t s9 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t2, vl));
          vint16m1_t s10 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t3, vl));
          vint16m1_t s11 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t4, vl));
          vint16m1_t s12 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t5, vl));
          vint16m1_t s13 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t6, vl));
          vint16m1_t s14 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t7, vl));

          // Perform convolution
          vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                             filter, round_offset, vl);
          vuint16m1_t d1 = convolve8_8_x_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                             filter, round_offset, vl);
          vuint16m1_t d2 = convolve8_8_x_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                             filter, round_offset, vl);
          vuint16m1_t d3 = convolve8_8_x_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                             filter, round_offset, vl);
          vuint16m1_t d4 = convolve8_8_x_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                             filter, round_offset, vl);
          vuint16m1_t d5 = convolve8_8_x_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                             filter, round_offset, vl);
          vuint16m1_t d6 = convolve8_8_x_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                             filter, round_offset, vl);
          vuint16m1_t d7 = convolve8_8_x_rvv(s7, s8, s9, s10, s11, s12, s13,
                                             s14, filter, round_offset, vl);

          // transpose
          transpose_elems_inplace_u16_8x8_rvv(&d0, &d1, &d2, &d3, &d4, &d5, &d6,
                                              &d7, trans_index);

          vuint16m1_t dd0, dd1, dd2, dd3;
          load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

          vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
          compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                       fwd_offset, bck_offset, round_offset,
                                       &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);
          store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);

          vuint16m1_t dd4, dd5, dd6, dd7;
          load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                       vl);

          vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
          compute_dist_wtd_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                       fwd_offset, bck_offset, round_offset,
                                       &d4_u8, &d5_u8, &d6_u8, &d7_u8, vl);
          store_u8_8x4(d_u8 + 4 * dst8_stride, dst8_stride, d4_u8, d5_u8, d6_u8,
                       d7_u8, vl);

          // Update sliding window
          s0 = __riscv_vmv_v_v_i16m1(s8, vl);
          s1 = __riscv_vmv_v_v_i16m1(s9, vl);
          s2 = __riscv_vmv_v_v_i16m1(s10, vl);
          s3 = __riscv_vmv_v_v_i16m1(s11, vl);
          s4 = __riscv_vmv_v_v_i16m1(s12, vl);
          s5 = __riscv_vmv_v_v_i16m1(s13, vl);
          s6 = __riscv_vmv_v_v_i16m1(s14, vl);
          s += 8;
          d += 8;
          d_u8 += 8;
          width -= 8;
        } while (width != 0);
        src_ptr += 8 * src_stride;
        dst_ptr += 8 * dst_stride;
        dst8_ptr += 8 * dst8_stride;
        height -= 8;
      }
    }

    while (height > 0) {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int width = w;

      do {
        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_u8_8x8(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit integers
        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           filter, round_offset, vl);
        vuint16m1_t dd0 = __riscv_vle16_v_u16m1(d, vl);

        vuint8mf2_t d0_u8;
        compute_dist_wtd_avg_8x1_rvv(dd0, d0, fwd_offset, bck_offset,
                                     round_offset, &d0_u8, vl);

        // Store result
        __riscv_vse8_v_u8mf2(d_u8, d0_u8, vl);

        s += vl;
        d += vl;
        d_u8 += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      dst8_ptr += dst8_stride;
      height--;
    }
  }
}

static inline void dist_wtd_convolve_x_avg_rvv(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  uint8_t *dst8_ptr = dst8;
  int dst_stride = conv_params->dst_stride;
  int height = h;

  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w == 4) {
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16_t *x_filter = x_filter_ptr + 2;
    int16_t filter[4];
    for (int i = 0; i < 4; i++) filter[i] = *x_filter++ >> 1;

    src_ptr += 2;

    do {
      // Load
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src_ptr + 0 * src_stride,
                                            8);  // a0 a1 a2 a3 a4 a5 a6 a7
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src_ptr + 1 * src_stride,
                                            8);  // b0 b1 b2 b3 b4 b5 b6 b7

      __builtin_prefetch(dst_ptr);
      __builtin_prefetch(dst8_ptr);

      // Widen to 16-bit
      vint16m1_t s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, 8));
      vint16m1_t s01 =
          __riscv_vslide1down_vx_i16m1(s00, 0, 8);  // a1 a2 a3 a4 a5 a6 a7 0
      vint16m1_t s02 =
          __riscv_vslide1down_vx_i16m1(s01, 0, 8);  // a2 a3 a4 a5 a6 a7 0 0
      vint16m1_t s03 =
          __riscv_vslide1down_vx_i16m1(s02, 0, 8);  // a3 a4 a5 a6 a7 0 0 0

      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, 8));
      vint16m1_t s11 =
          __riscv_vslide1down_vx_i16m1(s10, 0, 8);  // b1 b2 b3 b4 b5 b6 b7 0
      vint16m1_t s12 =
          __riscv_vslide1down_vx_i16m1(s11, 0, 8);  // b2 b3 b4 b5 b6 b7 0 0
      vint16m1_t s13 =
          __riscv_vslide1down_vx_i16m1(s12, 0, 8);  // b3 b4 b5 b6 b7 0 0 0

      // Perform convolution
      vuint8mf2_t d01, d11;
      vuint16m1_t d0 =
          convolve4_4_x_rvv(s00, s01, s02, s03, filter, round_offset, vl);
      vuint16m1_t d1 =
          convolve4_4_x_rvv(s10, s11, s12, s13, filter, round_offset, vl);

      // average
      vuint16m1_t dd0 = __riscv_vle16_v_u16m1(dst_ptr + 0 * dst_stride, vl);
      vuint16m1_t dd1 = __riscv_vle16_v_u16m1(dst_ptr + 1 * dst_stride, vl);
      compute_basic_avg_4x1_rvv(dd0, d0, round_offset, &d01, vl);
      compute_basic_avg_4x1_rvv(dd1, d1, round_offset, &d11, vl);

      // Store result
      __riscv_vse8_v_u8mf2(dst8_ptr + 0 * dst8_stride, d01, vl);
      __riscv_vse8_v_u8mf2(dst8_ptr + 1 * dst8_stride, d11, vl);

      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      dst8_ptr += 2 * dst8_stride;
      height -= 2;
    } while (height >= 2);
  } else {
    // Filter values are even, so halve to reduce intermediate precision reqs.
    int16_t filter[8];
    for (int i = 0; i < 8; i++) filter[i] = *x_filter_ptr++ >> 1;

    // for VLEN = 128 case
    if ((w > 8) && (vl == 8) && (height >= 8)) {
      vuint16m8_t trans_index = __riscv_vle16_v_u16m8(trans_data, 64);

      while (height >= 8) {
        const uint8_t *s = src_ptr;
        CONV_BUF_TYPE *d = dst_ptr;
        uint8_t *d_u8 = dst8_ptr;
        int width = w;

        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                           &t7, vl);

        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(d + 4 * dst_stride);
        __builtin_prefetch(d + 5 * dst_stride);
        __builtin_prefetch(d + 6 * dst_stride);
        __builtin_prefetch(d + 7 * dst_stride);

        s += 7;

        do {
          load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                             &t7, vl);

          vint16m1_t s7 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t0, vl));
          vint16m1_t s8 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t1, vl));
          vint16m1_t s9 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t2, vl));
          vint16m1_t s10 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t3, vl));
          vint16m1_t s11 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t4, vl));
          vint16m1_t s12 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t5, vl));
          vint16m1_t s13 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t6, vl));
          vint16m1_t s14 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t7, vl));

          // Perform convolution
          vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                             filter, round_offset, vl);
          vuint16m1_t d1 = convolve8_8_x_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                             filter, round_offset, vl);
          vuint16m1_t d2 = convolve8_8_x_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                             filter, round_offset, vl);
          vuint16m1_t d3 = convolve8_8_x_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                             filter, round_offset, vl);
          vuint16m1_t d4 = convolve8_8_x_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                             filter, round_offset, vl);
          vuint16m1_t d5 = convolve8_8_x_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                             filter, round_offset, vl);
          vuint16m1_t d6 = convolve8_8_x_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                             filter, round_offset, vl);
          vuint16m1_t d7 = convolve8_8_x_rvv(s7, s8, s9, s10, s11, s12, s13,
                                             s14, filter, round_offset, vl);

          // transpose
          transpose_elems_inplace_u16_8x8_rvv(&d0, &d1, &d2, &d3, &d4, &d5, &d6,
                                              &d7, trans_index);

          vuint16m1_t dd0, dd1, dd2, dd3;
          load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

          vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
          compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                    round_offset, &d0_u8, &d1_u8, &d2_u8,
                                    &d3_u8, vl);
          store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);

          vuint16m1_t dd4, dd5, dd6, dd7;
          load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                       vl);

          vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
          compute_basic_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                    round_offset, &d4_u8, &d5_u8, &d6_u8,
                                    &d7_u8, vl);
          store_u8_8x4(d_u8 + 4 * dst8_stride, dst8_stride, d4_u8, d5_u8, d6_u8,
                       d7_u8, vl);

          // Update sliding window
          s0 = __riscv_vmv_v_v_i16m1(s8, vl);
          s1 = __riscv_vmv_v_v_i16m1(s9, vl);
          s2 = __riscv_vmv_v_v_i16m1(s10, vl);
          s3 = __riscv_vmv_v_v_i16m1(s11, vl);
          s4 = __riscv_vmv_v_v_i16m1(s12, vl);
          s5 = __riscv_vmv_v_v_i16m1(s13, vl);
          s6 = __riscv_vmv_v_v_i16m1(s14, vl);
          s += 8;
          d += 8;
          d_u8 += 8;
          width -= 8;
        } while (width != 0);
        src_ptr += 8 * src_stride;
        dst_ptr += 8 * dst_stride;
        dst8_ptr += 8 * dst8_stride;
        height -= 8;
      }
    }

    while (height > 0) {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int width = w;

      do {
        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_u8_8x8(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit integers
        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           filter, round_offset, vl);
        vuint16m1_t dd0 = __riscv_vle16_v_u16m1(d, vl);

        vuint8mf2_t d0_u8;
        compute_basic_avg_8x1_rvv(dd0, d0, round_offset, &d0_u8, vl);

        // Store result
        __riscv_vse8_v_u8mf2(d_u8, d0_u8, vl);

        s += vl;
        d += vl;
        d_u8 += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      dst8_ptr += dst8_stride;
      height--;
    }
  }
}

static inline void dist_wtd_convolve_x_rvv(
    const uint8_t *src, int src_stride, int w, int h,
    const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int height = h;

  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w == 4) {
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16_t *x_filter = x_filter_ptr + 2;
    int16_t filter[4];
    for (int i = 0; i < 4; i++) filter[i] = *(x_filter + i) >> 1;

    src_ptr += 2;

    do {
      // Load
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src_ptr + 0 * src_stride,
                                            8);  // a0 a1 a2 a3 a4 a5 a6 a7
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src_ptr + 1 * src_stride,
                                            8);  // b0 b1 b2 b3 b4 b5 b6 b7

      __builtin_prefetch(dst_ptr);

      // Widen to 16-bit
      vint16m1_t s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, 8));
      vint16m1_t s01 =
          __riscv_vslide1down_vx_i16m1(s00, 0, 8);  // a1 a2 a3 a4 a5 a6 a7 0
      vint16m1_t s02 =
          __riscv_vslide1down_vx_i16m1(s01, 0, 8);  // a2 a3 a4 a5 a6 a7 0 0
      vint16m1_t s03 =
          __riscv_vslide1down_vx_i16m1(s02, 0, 8);  // a3 a4 a5 a6 a7 0 0 0

      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, 8));
      vint16m1_t s11 =
          __riscv_vslide1down_vx_i16m1(s10, 0, 8);  // b1 b2 b3 b4 b5 b6 b7 0
      vint16m1_t s12 =
          __riscv_vslide1down_vx_i16m1(s11, 0, 8);  // b2 b3 b4 b5 b6 b7 0 0
      vint16m1_t s13 =
          __riscv_vslide1down_vx_i16m1(s12, 0, 8);  // b3 b4 b5 b6 b7 0 0 0

      // Perform convolution
      vuint16m1_t d0 =
          convolve4_4_x_rvv(s00, s01, s02, s03, filter, round_offset, vl);
      vuint16m1_t d1 =
          convolve4_4_x_rvv(s10, s11, s12, s13, filter, round_offset, vl);

      // Store result
      __riscv_vse16_v_u16m1(dst_ptr + 0 * dst_stride, d0, vl);
      __riscv_vse16_v_u16m1(dst_ptr + 1 * dst_stride, d1, vl);

      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      height -= 2;
    } while (height >= 2);
  } else {
    // Filter values are even, so halve to reduce intermediate precision reqs.
    int16_t filter[8];
    for (int i = 0; i < 8; i++) filter[i] = *x_filter_ptr++ >> 1;

    // for VLEN = 128 case
    if ((w > 8) && (vl == 8) && (height >= 8)) {
      vuint16m8_t trans_index = __riscv_vle16_v_u16m8(trans_data, 64);

      while (height >= 8) {
        const uint8_t *s = src_ptr;
        CONV_BUF_TYPE *d = dst_ptr;
        int width = w;

        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                           &t7, vl);

        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(d + 4 * dst_stride);
        __builtin_prefetch(d + 5 * dst_stride);
        __builtin_prefetch(d + 6 * dst_stride);
        __builtin_prefetch(d + 7 * dst_stride);

        s += 7;

        do {
          load_stride_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                             &t7, vl);

          vint16m1_t s7 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t0, vl));
          vint16m1_t s8 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t1, vl));
          vint16m1_t s9 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t2, vl));
          vint16m1_t s10 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t3, vl));
          vint16m1_t s11 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t4, vl));
          vint16m1_t s12 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t5, vl));
          vint16m1_t s13 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t6, vl));
          vint16m1_t s14 = __riscv_vreinterpret_v_u16m1_i16m1(
              __riscv_vzext_vf2_u16m1(t7, vl));

          // Perform convolution
          vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                             filter, round_offset, vl);
          vuint16m1_t d1 = convolve8_8_x_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                             filter, round_offset, vl);
          vuint16m1_t d2 = convolve8_8_x_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                             filter, round_offset, vl);
          vuint16m1_t d3 = convolve8_8_x_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                             filter, round_offset, vl);
          vuint16m1_t d4 = convolve8_8_x_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                             filter, round_offset, vl);
          vuint16m1_t d5 = convolve8_8_x_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                             filter, round_offset, vl);
          vuint16m1_t d6 = convolve8_8_x_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                             filter, round_offset, vl);
          vuint16m1_t d7 = convolve8_8_x_rvv(s7, s8, s9, s10, s11, s12, s13,
                                             s14, filter, round_offset, vl);

          // transpose
          transpose_elems_inplace_u16_8x8_rvv(&d0, &d1, &d2, &d3, &d4, &d5, &d6,
                                              &d7, trans_index);

          // Store result
          store_u16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7, vl);

          // Update sliding window
          s0 = __riscv_vmv_v_v_i16m1(s8, vl);
          s1 = __riscv_vmv_v_v_i16m1(s9, vl);
          s2 = __riscv_vmv_v_v_i16m1(s10, vl);
          s3 = __riscv_vmv_v_v_i16m1(s11, vl);
          s4 = __riscv_vmv_v_v_i16m1(s12, vl);
          s5 = __riscv_vmv_v_v_i16m1(s13, vl);
          s6 = __riscv_vmv_v_v_i16m1(s14, vl);
          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src_ptr += 8 * src_stride;
        dst_ptr += 8 * dst_stride;
        height -= 8;
      }
    }

    while (height > 0) {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int width = w;

      __builtin_prefetch(d);

      do {
        // Load
        vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7;
        load_u8_8x8(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit integers
        vint16m1_t s0 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s1 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s2 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           filter, round_offset, vl);

        // Store result
        __riscv_vse16_v_u16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      height--;
    }
  }
}

void av1_dist_wtd_convolve_x_rvv(const uint8_t *src, int src_stride,
                                 uint8_t *dst8, int dst8_stride, int w, int h,
                                 const InterpFilterParams *filter_params_x,
                                 const int subpel_x_qn,
                                 ConvolveParams *conv_params) {
  if (conv_params->do_average) {
    if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
      dist_wtd_convolve_x_dist_wtd_avg_rvv(src, src_stride, dst8, dst8_stride,
                                           w, h, filter_params_x, subpel_x_qn,
                                           conv_params);
    } else {
      dist_wtd_convolve_x_avg_rvv(src, src_stride, dst8, dst8_stride, w, h,
                                  filter_params_x, subpel_x_qn, conv_params);
    }
  } else {
    dist_wtd_convolve_x_rvv(src, src_stride, w, h, filter_params_x, subpel_x_qn,
                            conv_params);
  }
}

static inline vuint16mf2_t convolve6_4_y_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const int16_t *filter, const int16_t round_offset, size_t vl) {
  // Filter values at indices 0 and 7 are 0.
  vint16mf2_t sum = __riscv_vmul_vx_i16mf2(s0, filter[1], vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[2], s1, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[3], s2, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[4], s3, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[5], s4, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[6], s5, vl);

  // Right shift with rounding: vrsra_n_s16(round_offset, sum, ROUND0_BITS - 1)
  // vrsra_n_s16(a, b, n) = a + ((b + (1 << (n-1))) >> n)
  // We halved the convolution filter values so -1 from the right shift.
  vint16mf2_t res =
      __riscv_vadd_vx_i16mf2(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16mf2(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16mf2(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16mf2_u16mf2(res);
}

static inline vuint16m1_t convolve6_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, const int16_t round_offset, size_t vl) {
  // Filter values at indices 0 and 7 are 0.
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[1], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s5, vl);

  // Right shift with rounding: vrsra_n_s16(round_offset, sum, ROUND0_BITS - 1)
  // vrsra_n_s16(a, b, n) = a + ((b + (1 << (n-1))) >> n)
  // We halved the convolution filter values so -1 from the right shift.
  vint16m1_t res = __riscv_vadd_vx_i16m1(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16m1(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16m1(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16m1_u16m1(res);
}

static inline void dist_wtd_convolve_y_6tap_dist_wtd_avg_rvv(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4;
      load_u8_4x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));

      s += 5 * src_stride;

      do {
        // Load next rows of data
        vuint8mf4_t t5 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        vuint8mf4_t t6 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        vuint8mf4_t t7 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        vuint8mf4_t t8 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t5, vl));
        vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t6, vl));
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t7, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t8, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve6_4_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d1 = convolve6_4_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d2 = convolve6_4_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d3 = convolve6_4_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                            round_offset, vl);

        vuint16mf2_t dd0, dd1, dd2, dd3;
        load_u16_4x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);

        // Store result
        __riscv_vse8_v_u8mf4(d_u8 + 0 * dst8_stride, d0_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 1 * dst8_stride, d1_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 2 * dst8_stride, d2_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 3 * dst8_stride, d3_u8, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      dst8_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4;
      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));

      s += 5 * src_stride;

      do {
        vuint8mf2_t t5, t6, t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve6_8_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                           round_offset, vl);
        vuint16m1_t d1 = convolve6_8_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                           round_offset, vl);
        vuint16m1_t d2 = convolve6_8_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                           round_offset, vl);
        vuint16m1_t d3 = convolve6_8_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                           round_offset, vl);
        vuint16m1_t d4 = convolve6_8_y_rvv(s4, s5, s6, s7, s8, s9, y_filter,
                                           round_offset, vl);
        vuint16m1_t d5 = convolve6_8_y_rvv(s5, s6, s7, s8, s9, s10, y_filter,
                                           round_offset, vl);
        vuint16m1_t d6 = convolve6_8_y_rvv(s6, s7, s8, s9, s10, s11, y_filter,
                                           round_offset, vl);
        vuint16m1_t d7 = convolve6_8_y_rvv(s7, s8, s9, s10, s11, s12, y_filter,
                                           round_offset, vl);

        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        vuint16m1_t dd4, dd5, dd6, dd7;
        load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                     vl);

        vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
        compute_dist_wtd_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                     fwd_offset, bck_offset, round_offset,
                                     &d4_u8, &d5_u8, &d6_u8, &d7_u8, vl);
        store_u8_8x4(d_u8, dst8_stride, d4_u8, d5_u8, d6_u8, d7_u8, vl);
        d_u8 += 4 * dst8_stride;

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      dst8_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}

static inline void dist_wtd_convolve_y_6tap_avg_rvv(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4;
      load_u8_4x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));

      s += 5 * src_stride;

      do {
        // Load next rows of data
        vuint8mf4_t t5 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        vuint8mf4_t t6 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        vuint8mf4_t t7 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        vuint8mf4_t t8 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t5, vl));
        vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t6, vl));
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t7, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t8, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve6_4_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d1 = convolve6_4_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d2 = convolve6_4_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d3 = convolve6_4_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                            round_offset, vl);

        vuint16mf2_t dd0, dd1, dd2, dd3;
        load_u16_4x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);

        // Store result
        __riscv_vse8_v_u8mf4(d_u8 + 0 * dst8_stride, d0_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 1 * dst8_stride, d1_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 2 * dst8_stride, d2_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 3 * dst8_stride, d3_u8, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      dst8_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4;
      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));

      s += 5 * src_stride;

      do {
        vuint8mf2_t t5, t6, t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve6_8_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                           round_offset, vl);
        vuint16m1_t d1 = convolve6_8_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                           round_offset, vl);
        vuint16m1_t d2 = convolve6_8_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                           round_offset, vl);
        vuint16m1_t d3 = convolve6_8_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                           round_offset, vl);
        vuint16m1_t d4 = convolve6_8_y_rvv(s4, s5, s6, s7, s8, s9, y_filter,
                                           round_offset, vl);
        vuint16m1_t d5 = convolve6_8_y_rvv(s5, s6, s7, s8, s9, s10, y_filter,
                                           round_offset, vl);
        vuint16m1_t d6 = convolve6_8_y_rvv(s6, s7, s8, s9, s10, s11, y_filter,
                                           round_offset, vl);
        vuint16m1_t d7 = convolve6_8_y_rvv(s7, s8, s9, s10, s11, s12, y_filter,
                                           round_offset, vl);

        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        vuint16m1_t dd4, dd5, dd6, dd7;
        load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                     vl);

        vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
        compute_basic_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                  round_offset, &d4_u8, &d5_u8, &d6_u8, &d7_u8,
                                  vl);
        store_u8_8x4(d_u8, dst8_stride, d4_u8, d5_u8, d6_u8, d7_u8, vl);
        d_u8 += 4 * dst8_stride;

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      dst8_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}

static inline void dist_wtd_convolve_y_6tap_rvv(const uint8_t *src_ptr,
                                                int src_stride, int w, int h,
                                                const int16_t *y_filter,
                                                ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4;
      load_u8_4x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));

      s += 5 * src_stride;

      do {
        // Load next rows of data
        vuint8mf4_t t5 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        vuint8mf4_t t6 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        vuint8mf4_t t7 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        vuint8mf4_t t8 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t5, vl));
        vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t6, vl));
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t7, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t8, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve6_4_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d1 = convolve6_4_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d2 = convolve6_4_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                            round_offset, vl);
        vuint16mf2_t d3 = convolve6_4_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                            round_offset, vl);

        // Store result
        store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      // Load initial 5 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4;
      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));

      s += 5 * src_stride;

      do {
        vuint8mf2_t t5, t6, t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        // Perform convolution
        vuint16m1_t d0 = convolve6_8_y_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                           round_offset, vl);
        vuint16m1_t d1 = convolve6_8_y_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                           round_offset, vl);
        vuint16m1_t d2 = convolve6_8_y_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                           round_offset, vl);
        vuint16m1_t d3 = convolve6_8_y_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                           round_offset, vl);
        vuint16m1_t d4 = convolve6_8_y_rvv(s4, s5, s6, s7, s8, s9, y_filter,
                                           round_offset, vl);
        vuint16m1_t d5 = convolve6_8_y_rvv(s5, s6, s7, s8, s9, s10, y_filter,
                                           round_offset, vl);
        vuint16m1_t d6 = convolve6_8_y_rvv(s6, s7, s8, s9, s10, s11, y_filter,
                                           round_offset, vl);
        vuint16m1_t d7 = convolve6_8_y_rvv(s7, s8, s9, s10, s11, s12, y_filter,
                                           round_offset, vl);

        // Store result
        store_u16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}

static inline vuint16mf2_t convolve8_4_y_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const int16_t *filter,
    const int16_t round_offset, size_t vl) {
  // Filter values at indices 0 and 7 are 0.
  vint16mf2_t sum = __riscv_vmul_vx_i16mf2(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16mf2(sum, filter[7], s7, vl);

  // Right shift with rounding: vrsra_n_s16(round_offset, sum, ROUND0_BITS - 1)
  // vrsra_n_s16(a, b, n) = a + ((b + (1 << (n-1))) >> n)
  // We halved the convolution filter values so -1 from the right shift.
  vint16mf2_t res =
      __riscv_vadd_vx_i16mf2(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16mf2(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16mf2(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16mf2_u16mf2(res);
}

static inline vuint16m1_t convolve8_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const int16_t round_offset, size_t vl) {
  // Filter values at indices 0 and 7 are 0.
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[7], s7, vl);

  // Right shift with rounding: vrsra_n_s16(round_offset, sum, ROUND0_BITS - 1)
  // vrsra_n_s16(a, b, n) = a + ((b + (1 << (n-1))) >> n)
  // We halved the convolution filter values so -1 from the right shift.
  vint16m1_t res = __riscv_vadd_vx_i16m1(sum, 1 << ((ROUND0_BITS - 1) - 1), vl);
  res = __riscv_vsra_vx_i16m1(res, ROUND0_BITS - 1, vl);
  res = __riscv_vadd_vx_i16m1(res, round_offset, vl);

  // Reinterpret as uint16
  return __riscv_vreinterpret_v_i16m1_u16m1(res);
}

static inline void dist_wtd_convolve_y_8tap_dist_wtd_avg_rvv(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      // Load initial 7 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_4x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));
      vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t5, vl));
      vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t6, vl));

      s += 7 * src_stride;

      do {
        // Load next rows of data
        t0 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        t1 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        t2 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        t3 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t0, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t1, vl));
        vint16mf2_t s9 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t2, vl));
        vint16mf2_t s10 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t3, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve8_4_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d1 = convolve8_4_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d2 = convolve8_4_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d3 = convolve8_4_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                            y_filter, round_offset, vl);

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);

        __builtin_prefetch(d_u8 + 0 * dst8_stride);
        __builtin_prefetch(d_u8 + 1 * dst8_stride);
        __builtin_prefetch(d_u8 + 2 * dst8_stride);
        __builtin_prefetch(d_u8 + 3 * dst8_stride);

        vuint16mf2_t dd0, dd1, dd2, dd3;
        load_u16_4x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);

        // Store result
        __riscv_vse8_v_u8mf4(d_u8 + 0 * dst8_stride, d0_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 1 * dst8_stride, d1_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 2 * dst8_stride, d2_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 3 * dst8_stride, d3_u8, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
        s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      dst8_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);
      __builtin_prefetch(s + 4 * src_stride);
      __builtin_prefetch(s + 5 * src_stride);
      __builtin_prefetch(s + 6 * src_stride);

      // Load initial 7 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
      vint16m1_t s5 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
      vint16m1_t s6 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

      s += 7 * src_stride;

      do {
        vuint8mf2_t t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s13 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s14 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           y_filter, round_offset, vl);
        vuint16m1_t d1 = convolve8_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                           y_filter, round_offset, vl);
        vuint16m1_t d2 = convolve8_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                           y_filter, round_offset, vl);
        vuint16m1_t d3 = convolve8_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                           y_filter, round_offset, vl);
        vuint16m1_t d4 = convolve8_8_y_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                           y_filter, round_offset, vl);
        vuint16m1_t d5 = convolve8_8_y_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                           y_filter, round_offset, vl);
        vuint16m1_t d6 = convolve8_8_y_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                           y_filter, round_offset, vl);
        vuint16m1_t d7 = convolve8_8_y_rvv(s7, s8, s9, s10, s11, s12, s13, s14,
                                           y_filter, round_offset, vl);

        __builtin_prefetch(d_u8 + 0 * dst8_stride);
        __builtin_prefetch(d_u8 + 1 * dst8_stride);
        __builtin_prefetch(d_u8 + 2 * dst8_stride);
        __builtin_prefetch(d_u8 + 3 * dst8_stride);

        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        vuint16m1_t dd4, dd5, dd6, dd7;
        load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                     vl);

        vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
        compute_dist_wtd_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                     fwd_offset, bck_offset, round_offset,
                                     &d4_u8, &d5_u8, &d6_u8, &d7_u8, vl);
        store_u8_8x4(d_u8, dst8_stride, d4_u8, d5_u8, d6_u8, d7_u8, vl);
        d_u8 += 4 * dst8_stride;

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s5 = __riscv_vmv_v_v_i16m1(s13, vl);
        s6 = __riscv_vmv_v_v_i16m1(s14, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      dst8_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}

static inline void dist_wtd_convolve_y_8tap_avg_rvv(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      // Load initial 7 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_4x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));
      vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t5, vl));
      vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t6, vl));

      s += 7 * src_stride;

      do {
        // Load next rows of data
        t0 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        t1 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        t2 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        t3 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t0, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t1, vl));
        vint16mf2_t s9 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t2, vl));
        vint16mf2_t s10 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t3, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve8_4_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d1 = convolve8_4_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d2 = convolve8_4_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d3 = convolve8_4_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                            y_filter, round_offset, vl);

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);

        __builtin_prefetch(d_u8 + 0 * dst8_stride);
        __builtin_prefetch(d_u8 + 1 * dst8_stride);
        __builtin_prefetch(d_u8 + 2 * dst8_stride);
        __builtin_prefetch(d_u8 + 3 * dst8_stride);

        vuint16mf2_t dd0, dd1, dd2, dd3;
        load_u16_4x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);

        // Store result
        __riscv_vse8_v_u8mf4(d_u8 + 0 * dst8_stride, d0_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 1 * dst8_stride, d1_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 2 * dst8_stride, d2_u8, vl);
        __riscv_vse8_v_u8mf4(d_u8 + 3 * dst8_stride, d3_u8, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
        s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      dst8_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);
      __builtin_prefetch(s + 4 * src_stride);
      __builtin_prefetch(s + 5 * src_stride);
      __builtin_prefetch(s + 6 * src_stride);

      // Load initial 7 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
      vint16m1_t s5 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
      vint16m1_t s6 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

      s += 7 * src_stride;

      do {
        vuint8mf2_t t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s13 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s14 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           y_filter, round_offset, vl);
        vuint16m1_t d1 = convolve8_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                           y_filter, round_offset, vl);
        vuint16m1_t d2 = convolve8_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                           y_filter, round_offset, vl);
        vuint16m1_t d3 = convolve8_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                           y_filter, round_offset, vl);
        vuint16m1_t d4 = convolve8_8_y_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                           y_filter, round_offset, vl);
        vuint16m1_t d5 = convolve8_8_y_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                           y_filter, round_offset, vl);
        vuint16m1_t d6 = convolve8_8_y_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                           y_filter, round_offset, vl);
        vuint16m1_t d7 = convolve8_8_y_rvv(s7, s8, s9, s10, s11, s12, s13, s14,
                                           y_filter, round_offset, vl);

        __builtin_prefetch(d_u8 + 0 * dst8_stride);
        __builtin_prefetch(d_u8 + 1 * dst8_stride);
        __builtin_prefetch(d_u8 + 2 * dst8_stride);
        __builtin_prefetch(d_u8 + 3 * dst8_stride);

        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        vuint16m1_t dd4, dd5, dd6, dd7;
        load_u16_8x4(d + 4 * dst_stride, dst_stride, &dd4, &dd5, &dd6, &dd7,
                     vl);

        vuint8mf2_t d4_u8, d5_u8, d6_u8, d7_u8;
        compute_basic_avg_8x4_rvv(dd4, dd5, dd6, dd7, d4, d5, d6, d7,
                                  round_offset, &d4_u8, &d5_u8, &d6_u8, &d7_u8,
                                  vl);
        store_u8_8x4(d_u8, dst8_stride, d4_u8, d5_u8, d6_u8, d7_u8, vl);
        d_u8 += 4 * dst8_stride;

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s5 = __riscv_vmv_v_v_i16m1(s13, vl);
        s6 = __riscv_vmv_v_v_i16m1(s14, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      dst8_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}

static inline void dist_wtd_convolve_y_8tap_rvv(const uint8_t *src_ptr,
                                                int src_stride, int w, int h,
                                                const int16_t *y_filter,
                                                ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  int width = w;

  if (w == 4 || h == 4) {
    size_t vl = __riscv_vsetvl_e16m1(4);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      // Load initial 7 rows of data
      vuint8mf4_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_4x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16mf2_t s0 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t0, vl));
      vint16mf2_t s1 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t1, vl));
      vint16mf2_t s2 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t2, vl));
      vint16mf2_t s3 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t3, vl));
      vint16mf2_t s4 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t4, vl));
      vint16mf2_t s5 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t5, vl));
      vint16mf2_t s6 = __riscv_vreinterpret_v_u16mf2_i16mf2(
          __riscv_vzext_vf2_u16mf2(t6, vl));

      s += 7 * src_stride;

      do {
        // Load next rows of data
        t0 = __riscv_vle8_v_u8mf4(s + 0 * src_stride, vl);
        t1 = __riscv_vle8_v_u8mf4(s + 1 * src_stride, vl);
        t2 = __riscv_vle8_v_u8mf4(s + 2 * src_stride, vl);
        t3 = __riscv_vle8_v_u8mf4(s + 3 * src_stride, vl);

        // Convert to 16-bit
        vint16mf2_t s7 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t0, vl));
        vint16mf2_t s8 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t1, vl));
        vint16mf2_t s9 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t2, vl));
        vint16mf2_t s10 = __riscv_vreinterpret_v_u16mf2_i16mf2(
            __riscv_vzext_vf2_u16mf2(t3, vl));

        // Perform convolution
        vuint16mf2_t d0 = convolve8_4_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d1 = convolve8_4_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d2 = convolve8_4_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                            y_filter, round_offset, vl);
        vuint16mf2_t d3 = convolve8_4_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                            y_filter, round_offset, vl);

        // Store result
        store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
        s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
        s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
        s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
        s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
        s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
        s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 4;
      dst_ptr += 4;
      width -= 4;
    } while (width != 0);
  } else {
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);
      __builtin_prefetch(s + 4 * src_stride);
      __builtin_prefetch(s + 5 * src_stride);
      __builtin_prefetch(s + 6 * src_stride);

      // Load initial 7 rows of data
      vuint8mf2_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
      vint16m1_t s4 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
      vint16m1_t s5 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
      vint16m1_t s6 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));

      s += 7 * src_stride;

      do {
        vuint8mf2_t t7;
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        // Convert to 16-bit
        vint16m1_t s7 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s10 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));
        vint16m1_t s11 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t4, vl));
        vint16m1_t s12 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
        vint16m1_t s13 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
        vint16m1_t s14 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);

        // Perform convolution
        vuint16m1_t d0 = convolve8_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                           y_filter, round_offset, vl);
        vuint16m1_t d1 = convolve8_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                           y_filter, round_offset, vl);
        vuint16m1_t d2 = convolve8_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                           y_filter, round_offset, vl);
        vuint16m1_t d3 = convolve8_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                           y_filter, round_offset, vl);
        vuint16m1_t d4 = convolve8_8_y_rvv(s4, s5, s6, s7, s8, s9, s10, s11,
                                           y_filter, round_offset, vl);
        vuint16m1_t d5 = convolve8_8_y_rvv(s5, s6, s7, s8, s9, s10, s11, s12,
                                           y_filter, round_offset, vl);
        vuint16m1_t d6 = convolve8_8_y_rvv(s6, s7, s8, s9, s10, s11, s12, s13,
                                           y_filter, round_offset, vl);
        vuint16m1_t d7 = convolve8_8_y_rvv(s7, s8, s9, s10, s11, s12, s13, s14,
                                           y_filter, round_offset, vl);

        // Store result
        store_u16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7, vl);

        // Update sliding window
        s0 = __riscv_vmv_v_v_i16m1(s8, vl);
        s1 = __riscv_vmv_v_v_i16m1(s9, vl);
        s2 = __riscv_vmv_v_v_i16m1(s10, vl);
        s3 = __riscv_vmv_v_v_i16m1(s11, vl);
        s4 = __riscv_vmv_v_v_i16m1(s12, vl);
        s5 = __riscv_vmv_v_v_i16m1(s13, vl);
        s6 = __riscv_vmv_v_v_i16m1(s14, vl);
        s += 8 * src_stride;
        d += 8 * dst_stride;
        height -= 8;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      width -= vl;
    } while (width > 0);
  }
}
void av1_dist_wtd_convolve_y_rvv(const uint8_t *src, int src_stride,
                                 uint8_t *dst8, int dst8_stride, int w, int h,
                                 const InterpFilterParams *filter_params_y,
                                 const int subpel_y_qn,
                                 ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  // Vertical filter.
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);
  // Filter values are even, so downshift by 1 to reduce intermediate
  // precision requirements.
  int16_t y_filter[8];
  for (int i = 0; i < 8; i++) y_filter[i] = *y_filter_ptr++ >> 1;

  const int vert_offset = filter_params_y->taps / 2 - 1;
  const uint8_t *src_ptr = src - (vert_offset * src_stride);

  if (get_filter_tap(filter_params_y, subpel_y_qn) <= 6) {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_y_6tap_dist_wtd_avg_rvv(src_ptr + src_stride,
                                                  src_stride, dst8, dst8_stride,
                                                  w, h, y_filter, conv_params);
      } else {
        dist_wtd_convolve_y_6tap_avg_rvv(src_ptr + src_stride, src_stride, dst8,
                                         dst8_stride, w, h, y_filter,
                                         conv_params);
      }
    } else {
      dist_wtd_convolve_y_6tap_rvv(src_ptr + src_stride, src_stride, w, h,
                                   y_filter, conv_params);
    }
  } else {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_y_8tap_dist_wtd_avg_rvv(src_ptr, src_stride, dst8,
                                                  dst8_stride, w, h, y_filter,
                                                  conv_params);
      } else {
        dist_wtd_convolve_y_8tap_avg_rvv(src_ptr, src_stride, dst8, dst8_stride,
                                         w, h, y_filter, conv_params);
      }
    } else {
      dist_wtd_convolve_y_8tap_rvv(src_ptr, src_stride, w, h, y_filter,
                                   conv_params);
    }
  }
}
