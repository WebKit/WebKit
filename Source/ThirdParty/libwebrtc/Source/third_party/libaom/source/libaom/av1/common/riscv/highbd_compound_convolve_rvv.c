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

#include <riscv_vector.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/riscv/highbd_compound_convolve_rvv.h"

static inline vuint16mf2_t highbd_convolve6_4_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const int16_t *filter, const vint32m1_t offset_vec,
    const int32_t round_bits, const size_t vl) {
  // Values at indices 0 and 7 of filter are zero.
  vint32m1_t sum = __riscv_vwmacc_vx_i32m1(offset_vec, filter[1], s0, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s5, vl);

  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16mf2_t res = __riscv_vnsrl_wx_u16mf2(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline vuint16m1_t highbd_convolve6_8_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, const vint32m2_t offset_vec,
    const int32_t round_bits, const size_t vl) {
  // Values at indices 0 and 7 of filter are zero.
  vint32m2_t sum = __riscv_vwmacc_vx_i32m2(offset_vec, filter[1], s0, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s5, vl);

  vuint32m2_t sum_u32 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16m1_t res = __riscv_vnsrl_wx_u16m1(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline void highbd_dist_wtd_convolve_x_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset,
    const int round_bits) {
  int height = h;
  const size_t vl = __riscv_vsetvl_e16m1(w);
  vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      vint16m1_t s00, s01, s02, s03, s04, s05;
      vint16m1_t s10, s11, s12, s13, s14, s15;
      vint16m1_t s20, s21, s22, s23, s24, s25;
      vint16m1_t s30, s31, s32, s33, s34, s35;

      // Load 6 consecutive 8-element vectors from 4 rows
      load_s16_8x6(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                   vl);
      load_s16_8x6(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                   vl);
      load_s16_8x6(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                   vl);
      load_s16_8x6(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                   vl);

      // Convolve each row
      vuint16m1_t d0 =
          highbd_convolve6_8_rvv(s00, s01, s02, s03, s04, s05, x_filter_ptr,
                                 offset_vec, round_bits, vl);
      vuint16m1_t d1 =
          highbd_convolve6_8_rvv(s10, s11, s12, s13, s14, s15, x_filter_ptr,
                                 offset_vec, round_bits, vl);
      vuint16m1_t d2 =
          highbd_convolve6_8_rvv(s20, s21, s22, s23, s24, s25, x_filter_ptr,
                                 offset_vec, round_bits, vl);
      vuint16m1_t d3 =
          highbd_convolve6_8_rvv(s30, s31, s32, s33, s34, s35, x_filter_ptr,
                                 offset_vec, round_bits, vl);

      // Store results with stride
      store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += vl;
      d += vl;
      width -= vl;
    } while (width > 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height != 0);
}

static inline vuint16mf2_t highbd_convolve8_4_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const int16_t *filter,
    const vint32m1_t offset_vec, const int32_t round_bits, const size_t vl) {
  vint32m1_t sum = __riscv_vwmacc_vx_i32m1(offset_vec, filter[0], s0, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[7], s7, vl);

  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16mf2_t res = __riscv_vnsrl_wx_u16mf2(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline vuint16m1_t highbd_convolve8_8_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const vint32m2_t offset_vec, const int32_t round_bits, const size_t vl) {
  vint32m2_t sum = __riscv_vwmacc_vx_i32m2(offset_vec, filter[0], s0, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);

  vuint32m2_t sum_u32 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16m1_t res = __riscv_vnsrl_wx_u16m1(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline vuint16mf2_t highbd_convolve4_4_x_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const int16_t *filter, const vint32m1_t offset_vec,
    const int round_bits, const size_t vl) {
  vint32m1_t sum = __riscv_vwmacc_vx_i32m1(offset_vec, filter[0], s0, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);

  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16mf2_t res = __riscv_vnsrl_wx_u16mf2(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline void highbd_dist_wtd_convolve_x_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset,
    const int round_bits) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16_t *filter = x_filter_ptr + 2;  // Skip first 2 taps
    const int16_t *s = (const int16_t *)(src_ptr + 2);
    uint16_t *d = dst_ptr;
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);

    do {
      // Load 4 taps for 4 rows
      vint16mf2_t s00, s01, s02, s03;
      vint16mf2_t s10, s11, s12, s13;
      vint16mf2_t s20, s21, s22, s23;
      vint16mf2_t s30, s31, s32, s33;

      load_s16_4x4(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, vl);
      load_s16_4x4(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, vl);
      load_s16_4x4(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, vl);
      load_s16_4x4(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, vl);

      // Convolve each row
      vuint16mf2_t d0 = highbd_convolve4_4_x_rvv(s00, s01, s02, s03, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d1 = highbd_convolve4_4_x_rvv(s10, s11, s12, s13, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d2 = highbd_convolve4_4_x_rvv(s20, s21, s22, s23, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d3 = highbd_convolve4_4_x_rvv(s30, s31, s32, s33, filter,
                                                 offset_vec, round_bits, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    // 8-tap filter path
    const int16_t *filter = x_filter_ptr;
    int height = h;
    vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        vint16m1_t s00, s01, s02, s03, s04, s05, s06, s07;
        vint16m1_t s10, s11, s12, s13, s14, s15, s16, s17;
        vint16m1_t s20, s21, s22, s23, s24, s25, s26, s27;
        vint16m1_t s30, s31, s32, s33, s34, s35, s36, s37;

        // Load elements for each of 4 rows
        load_s16_8x8(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                     &s06, &s07, vl);
        load_s16_8x8(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                     &s16, &s17, vl);
        load_s16_8x8(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                     &s26, &s27, vl);
        load_s16_8x8(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                     &s36, &s37, vl);

        // Convolve each row
        vuint16m1_t d0 =
            highbd_convolve8_8_rvv(s00, s01, s02, s03, s04, s05, s06, s07,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d1 =
            highbd_convolve8_8_rvv(s10, s11, s12, s13, s14, s15, s16, s17,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d2 =
            highbd_convolve8_8_rvv(s20, s21, s22, s23, s24, s25, s26, s27,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d3 =
            highbd_convolve8_8_rvv(s30, s31, s32, s33, s34, s35, s36, s37,
                                   filter, offset_vec, round_bits, vl);

        // Store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_highbd_dist_wtd_convolve_x_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  int dst16_stride = conv_params->dst_stride;
  const int im_stride = MAX_SB_SIZE;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  assert(FILTER_BITS == COMPOUND_ROUND1_BITS);
  const int offset_convolve = (1 << (conv_params->round_0 - 1)) +
                              (1 << (bd + FILTER_BITS)) +
                              (1 << (bd + FILTER_BITS - 1));

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_offset;
  int round_bits, shift_bits, offset_bits;

  // horizontal filter
  if (bd == 12) {
    round_bits = ROUND0_BITS + 2;
    shift_bits = ROUND_SHIFT - 2;
    offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS - 2;
  } else {
    round_bits = ROUND0_BITS;
    shift_bits = ROUND_SHIFT;
    offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  }

  if (conv_params->do_average) {
    if (x_filter_taps <= 6 && w != 4) {
      highbd_dist_wtd_convolve_x_6tap_rvv(src + 1, src_stride, im_block,
                                          im_stride, w, h, x_filter_ptr,
                                          offset_convolve, round_bits);
    } else {
      highbd_dist_wtd_convolve_x_rvv(src, src_stride, im_block, im_stride, w, h,
                                     x_filter_ptr, offset_convolve, round_bits);
    }
    if (conv_params->use_dist_wtd_comp_avg) {
      highbd_dist_wtd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   conv_params, bd, shift_bits, offset_bits);
    } else {
      highbd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                          conv_params, bd, shift_bits, offset_bits);
    }
  } else {
    if (x_filter_taps <= 6 && w != 4) {
      highbd_dist_wtd_convolve_x_6tap_rvv(src + 1, src_stride, dst16,
                                          dst16_stride, w, h, x_filter_ptr,
                                          offset_convolve, round_bits);
    } else {
      highbd_dist_wtd_convolve_x_rvv(src, src_stride, dst16, dst16_stride, w, h,
                                     x_filter_ptr, offset_convolve, round_bits);
    }
  }
}

static inline void highbd_dist_wtd_convolve_y_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset,
    const int round_bits) {
  const int16_t *filter = y_filter_ptr;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);

    // Load initial 5 rows for 6-tap filter
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    s += 5 * src_stride;

    do {
      // Load next 4 rows
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

      // Convolve 4 output rows
      vuint16mf2_t d0 = highbd_convolve6_4_rvv(s0, s1, s2, s3, s4, s5, filter,
                                               offset_vec, round_bits, vl);
      vuint16mf2_t d1 = highbd_convolve6_4_rvv(s1, s2, s3, s4, s5, s6, filter,
                                               offset_vec, round_bits, vl);
      vuint16mf2_t d2 = highbd_convolve6_4_rvv(s2, s3, s4, s5, s6, s7, filter,
                                               offset_vec, round_bits, vl);
      vuint16mf2_t d3 = highbd_convolve6_4_rvv(s3, s4, s5, s6, s7, s8, filter,
                                               offset_vec, round_bits, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Shift window: s0-s4 become s4-s8 for next iteration
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      // Load initial 5 rows for 6-tap filter
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        // Load next 4 rows
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        // Convolve 4 output rows
        vuint16m1_t d0 = highbd_convolve6_8_rvv(s0, s1, s2, s3, s4, s5, filter,
                                                offset_vec, round_bits, vl);
        vuint16m1_t d1 = highbd_convolve6_8_rvv(s1, s2, s3, s4, s5, s6, filter,
                                                offset_vec, round_bits, vl);
        vuint16m1_t d2 = highbd_convolve6_8_rvv(s2, s3, s4, s5, s6, s7, filter,
                                                offset_vec, round_bits, vl);
        vuint16m1_t d3 = highbd_convolve6_8_rvv(s3, s4, s5, s6, s7, s8, filter,
                                                offset_vec, round_bits, vl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Shift window: s0-s4 become s4-s8 for next iteration
        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline vuint16mf2_t highbd_convolve4_4_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const int16_t *filter, const int32_t offset,
    const int32_t round_bits, size_t vl) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);
  sum = __riscv_vadd_vx_i32m1(sum, offset, vl);

  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16mf2_t res = __riscv_vnsrl_wx_u16mf2(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline vuint16m1_t highbd_convolve4_8_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *filter, const int32_t offset,
    const int32_t round_bits, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  vuint32m2_t sum_u32 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(sum_u32, round_bits, __RISCV_VXRM_RDN, vl);
#else
  vuint16m1_t res = __riscv_vnsrl_wx_u16m1(sum_u32, round_bits, vl);
#endif
  return res;
}

static inline void highbd_dist_wtd_convolve_y_4tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset,
    const int round_bits) {
  const int16_t *filter = y_filter_ptr + 2;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    // Load initial 3 rows
    vint16mf2_t s0, s1, s2;
    load_s16_4x3(s, src_stride, &s0, &s1, &s2, vl);
    s += 3 * src_stride;

    do {
      // Load next 4 rows
      vint16mf2_t s3, s4, s5, s6;
      load_s16_4x4(s, src_stride, &s3, &s4, &s5, &s6, vl);

      // Convolve 4 output rows
      vuint16mf2_t d0 = highbd_convolve4_4_rvv(s0, s1, s2, s3, filter, offset,
                                               round_bits, vl);
      vuint16mf2_t d1 = highbd_convolve4_4_rvv(s1, s2, s3, s4, filter, offset,
                                               round_bits, vl);
      vuint16mf2_t d2 = highbd_convolve4_4_rvv(s2, s3, s4, s5, filter, offset,
                                               round_bits, vl);
      vuint16mf2_t d3 = highbd_convolve4_4_rvv(s3, s4, s5, s6, filter, offset,
                                               round_bits, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Shift window
      s0 = s4;
      s1 = s5;
      s2 = s6;

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      // Load initial 3 rows
      vint16m1_t s0, s1, s2;
      load_s16_8x3(s, src_stride, &s0, &s1, &s2, vl);
      s += 3 * src_stride;

      do {
        // Load next 4 rows
        vint16m1_t s3, s4, s5, s6;
        load_s16_8x4(s, src_stride, &s3, &s4, &s5, &s6, vl);

        // Convolve 4 output rows
        vuint16m1_t d0 = highbd_convolve4_8_rvv(s0, s1, s2, s3, filter, offset,
                                                round_bits, vl);
        vuint16m1_t d1 = highbd_convolve4_8_rvv(s1, s2, s3, s4, filter, offset,
                                                round_bits, vl);
        vuint16m1_t d2 = highbd_convolve4_8_rvv(s2, s3, s4, s5, filter, offset,
                                                round_bits, vl);
        vuint16m1_t d3 = highbd_convolve4_8_rvv(s3, s4, s5, s6, filter, offset,
                                                round_bits, vl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Shift window
        s0 = s4;
        s1 = s5;
        s2 = s6;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void highbd_dist_wtd_convolve_y_8tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset,
    const int32_t round_bits) {
  const int16_t *filter = y_filter_ptr;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);

    // Load initial 7 rows
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    s += 7 * src_stride;

    do {
      // Load next 4 rows
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

      // Convolve 4 output rows
      vuint16mf2_t d0 = highbd_convolve8_4_rvv(
          s0, s1, s2, s3, s4, s5, s6, s7, filter, offset_vec, round_bits, vl);
      vuint16mf2_t d1 = highbd_convolve8_4_rvv(
          s1, s2, s3, s4, s5, s6, s7, s8, filter, offset_vec, round_bits, vl);
      vuint16mf2_t d2 = highbd_convolve8_4_rvv(
          s2, s3, s4, s5, s6, s7, s8, s9, filter, offset_vec, round_bits, vl);
      vuint16mf2_t d3 = highbd_convolve8_4_rvv(
          s3, s4, s5, s6, s7, s8, s9, s10, filter, offset_vec, round_bits, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Shift window
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
    } while (h != 0);
  } else {
    vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      // Load initial 7 rows
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        // Load next 4 rows
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        // Convolve 4 output rows
        vuint16m1_t d0 = highbd_convolve8_8_rvv(
            s0, s1, s2, s3, s4, s5, s6, s7, filter, offset_vec, round_bits, vl);
        vuint16m1_t d1 = highbd_convolve8_8_rvv(
            s1, s2, s3, s4, s5, s6, s7, s8, filter, offset_vec, round_bits, vl);
        vuint16m1_t d2 = highbd_convolve8_8_rvv(
            s2, s3, s4, s5, s6, s7, s8, s9, filter, offset_vec, round_bits, vl);
        vuint16m1_t d3 =
            highbd_convolve8_8_rvv(s3, s4, s5, s6, s7, s8, s9, s10, filter,
                                   offset_vec, round_bits, vl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Shift window
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
      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

void av1_highbd_dist_wtd_convolve_y_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  int dst16_stride = conv_params->dst_stride;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  assert(FILTER_BITS == COMPOUND_ROUND1_BITS);
  const int round_offset_conv = (1 << (conv_params->round_0 - 1)) +
                                (1 << (bd + FILTER_BITS)) +
                                (1 << (bd + FILTER_BITS - 1));

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_offset * src_stride;
  int round_bits, shift_bits, offset_bits;

  if (bd == 12) {
    round_bits = ROUND0_BITS + 2;
    shift_bits = ROUND_SHIFT - 2;
    offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS - 2;
  } else {
    round_bits = ROUND0_BITS;
    shift_bits = ROUND_SHIFT;
    offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  }

  if (conv_params->do_average) {
    if (y_filter_taps <= 4) {
      highbd_dist_wtd_convolve_y_4tap_rvv(
          src + 2 * src_stride, src_stride, im_block, im_stride, w, h,
          y_filter_ptr, round_offset_conv, round_bits);
    } else if (y_filter_taps == 6) {
      highbd_dist_wtd_convolve_y_6tap_rvv(
          src + src_stride, src_stride, im_block, im_stride, w, h, y_filter_ptr,
          round_offset_conv, round_bits);
    } else {
      highbd_dist_wtd_convolve_y_8tap_rvv(src, src_stride, im_block, im_stride,
                                          w, h, y_filter_ptr, round_offset_conv,
                                          round_bits);
    }
    if (conv_params->use_dist_wtd_comp_avg) {
      highbd_dist_wtd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   conv_params, bd, shift_bits, offset_bits);
    } else {
      highbd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                          conv_params, bd, shift_bits, offset_bits);
    }
  } else {
    if (y_filter_taps <= 4) {
      highbd_dist_wtd_convolve_y_4tap_rvv(
          src + 2 * src_stride, src_stride, dst16, dst16_stride, w, h,
          y_filter_ptr, round_offset_conv, round_bits);
    } else if (y_filter_taps == 6) {
      highbd_dist_wtd_convolve_y_6tap_rvv(src + src_stride, src_stride, dst16,
                                          dst16_stride, w, h, y_filter_ptr,
                                          round_offset_conv, round_bits);
    } else {
      highbd_dist_wtd_convolve_y_8tap_rvv(src, src_stride, dst16, dst16_stride,
                                          w, h, y_filter_ptr, round_offset_conv,
                                          round_bits);
    }
  }
}

static inline void highbd_2d_copy_rvv(const uint16_t *src_ptr, int src_stride,
                                      uint16_t *dst_ptr, int dst_stride, int w,
                                      int h, int round_bits, const int offset) {
  assert(h % 4 == 0);

  const uint16_t round_val = (1 << round_bits);
  int height = h;

  if (w <= 4) {
    const size_t vl = __riscv_vsetvl_e16mf2(w);
    vuint16mf2_t vec_offset = __riscv_vmv_v_x_u16mf2(offset, vl);

    do {
      vuint16mf2_t s0, s1, s2, s3;
      load_u16_4x4(src_ptr, src_stride, &s0, &s1, &s2, &s3, vl);

      vuint16mf2_t d0 = __riscv_vmacc_vx_u16mf2(vec_offset, round_val, s0, vl);
      vuint16mf2_t d1 = __riscv_vmacc_vx_u16mf2(vec_offset, round_val, s1, vl);
      vuint16mf2_t d2 = __riscv_vmacc_vx_u16mf2(vec_offset, round_val, s2, vl);
      vuint16mf2_t d3 = __riscv_vmacc_vx_u16mf2(vec_offset, round_val, s3, vl);

      store_u16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3, vl);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    size_t vl_max = __riscv_vsetvlmax_e16m1();
    vuint16m1_t vec_offset = __riscv_vmv_v_x_u16m1(offset, vl_max);

    do {
      const uint16_t *s = src_ptr;
      uint16_t *d = dst_ptr;
      int width = w;

      do {
        size_t vl = __riscv_vsetvl_e16m1(width);
        vuint16m1_t s0, s1, s2, s3;
        load_u16_8x4(s, src_stride, &s0, &s1, &s2, &s3, vl);

        // apply shift and add round offset
        vuint16m1_t d0 = __riscv_vmacc_vx_u16m1(vec_offset, round_val, s0, vl);
        vuint16m1_t d1 = __riscv_vmacc_vx_u16m1(vec_offset, round_val, s1, vl);
        vuint16m1_t d2 = __riscv_vmacc_vx_u16m1(vec_offset, round_val, s2, vl);
        vuint16m1_t d3 = __riscv_vmacc_vx_u16m1(vec_offset, round_val, s3, vl);

        // store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

void av1_highbd_dist_wtd_convolve_2d_copy_rvv(const uint16_t *src,
                                              int src_stride, uint16_t *dst,
                                              int dst_stride, int w, int h,
                                              ConvolveParams *conv_params,
                                              int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);

  const int im_stride = MAX_SB_SIZE;
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  int dst16_stride = conv_params->dst_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  assert(round_bits >= 0);

  if (conv_params->do_average) {
    highbd_2d_copy_rvv(src, src_stride, im_block, im_stride, w, h, round_bits,
                       round_offset);
  } else {
    highbd_2d_copy_rvv(src, src_stride, dst16, dst16_stride, w, h, round_bits,
                       round_offset);
  }

  if (conv_params->do_average) {
    int shift_bits;
    if (bd == 12) {
      shift_bits = ROUND_SHIFT - 2;
    } else {
      shift_bits = ROUND_SHIFT;
    }
    if (conv_params->use_dist_wtd_comp_avg) {
      highbd_dist_wtd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   conv_params, bd, shift_bits, offset_bits);
    } else {
      highbd_comp_avg_rvv(im_block, im_stride, dst, dst_stride, w, h,
                          conv_params, bd, shift_bits, offset_bits);
    }
  }
}

static inline vuint16mf2_t highbd_convolve6_4_2d_v_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const int16_t *y_filter, const vint32m1_t offset, size_t vl) {
  // Values at indices 0 and 7 of y_filter are zero.
  // Accumulate widening multiplications into the offset vector.
  vint32m1_t sum = __riscv_vwmacc_vx_i32m1(offset, y_filter[1], s0, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[6], s5, vl);

  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16mf2(sum_u32, COMPOUND_ROUND1_BITS,
                                   __RISCV_VXRM_RNU, vl);
#else
  return __riscv_vnclipu_wx_u16mf2(sum_u32, COMPOUND_ROUND1_BITS, vl);
#endif
}

static inline vuint16m1_t highbd_convolve6_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *y_filter, const vint32m2_t offset, size_t vl) {
  // Values at indices 0 and 7 of y_filter are zero.
  // Perform widening multiply-accumulate starting with the offset vector.
  vint32m2_t sum = __riscv_vwmacc_vx_i32m2(offset, y_filter[1], s0, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[6], s5, vl);

  vuint32m2_t sum_u32 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16m1(sum_u32, COMPOUND_ROUND1_BITS,
                                  __RISCV_VXRM_RNU, vl);
#else
  return __riscv_vnclipu_wx_u16m1(sum_u32, COMPOUND_ROUND1_BITS, vl);
#endif
}

static inline void highbd_dist_wtd_convolve_2d_vert_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int offset) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w == 4) {
    // Broadcast offset to a 32-bit vector
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    // Load initial 5 rows
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    s += 5 * src_stride;

    do {
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

      // Perform 6-tap convolution for 4 rows
      vuint16mf2_t d0 = highbd_convolve6_4_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d1 = highbd_convolve6_4_2d_v_rvv(
          s1, s2, s3, s4, s5, s6, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d2 = highbd_convolve6_4_2d_v_rvv(
          s2, s3, s4, s5, s6, s7, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d3 = highbd_convolve6_4_2d_v_rvv(
          s3, s4, s5, s6, s7, s8, y_filter_ptr, offset_vec, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update sliding window state
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    // w > 4 case
    do {
      vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;
      int height = h;

      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        vuint16m1_t d0 = highbd_convolve6_8_2d_v_rvv(
            s0, s1, s2, s3, s4, s5, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d1 = highbd_convolve6_8_2d_v_rvv(
            s1, s2, s3, s4, s5, s6, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d2 = highbd_convolve6_8_2d_v_rvv(
            s2, s3, s4, s5, s6, s7, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d3 = highbd_convolve6_8_2d_v_rvv(
            s3, s4, s5, s6, s7, s8, y_filter_ptr, offset_vec, vl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);

      src_ptr += vl;
      dst_ptr += vl;
      w -= (int)vl;
    } while (w > 0);
  }
}

static inline vuint16mf2_t highbd_convolve8_4_2d_v_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const int16_t *y_filter,
    const vint32m1_t offset, size_t vl) {
  // Perform widening multiply-accumulate for all 8 taps.
  // Accumulate directly into the offset vector.
  vint32m1_t sum = __riscv_vwmacc_vx_i32m1(offset, y_filter[0], s0, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, y_filter[7], s7, vl);

  // NEON vqrshrun_n_s32 -> RVV vnclipu_wx (with default RNU rounding)
  vuint32m1_t sum_u32 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16mf2(sum_u32, COMPOUND_ROUND1_BITS,
                                   __RISCV_VXRM_RNU, vl);
#else
  return __riscv_vnclipu_wx_u16mf2(sum_u32, COMPOUND_ROUND1_BITS, vl);
#endif
}

static inline vuint16m1_t highbd_convolve8_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *y_filter,
    const vint32m2_t offset, size_t vl) {
  // Perform widening multiply-accumulate for all 8 taps.
  vint32m2_t sum = __riscv_vwmacc_vx_i32m2(offset, y_filter[0], s0, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[7], s7, vl);

  // NEON vqrshrun_n_s32 -> RVV vnclipu_wx (with default RNU rounding)
  vuint32m2_t sum_u32 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
#if __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16m1(sum_u32, COMPOUND_ROUND1_BITS,
                                  __RISCV_VXRM_RNU, vl);
#else
  return __riscv_vnclipu_wx_u16m1(sum_u32, COMPOUND_ROUND1_BITS, vl);
#endif
}

static inline void highbd_dist_wtd_convolve_2d_vert_8tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int offset) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w == 4) {
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    // Load initial 7 rows for 8-tap filter
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    s += 7 * src_stride;

    do {
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

      // Perform 8-tap convolution for 4 rows
      vuint16mf2_t d0 = highbd_convolve8_4_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, s7, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d1 = highbd_convolve8_4_2d_v_rvv(
          s1, s2, s3, s4, s5, s6, s7, s8, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d2 = highbd_convolve8_4_2d_v_rvv(
          s2, s3, s4, s5, s6, s7, s8, s9, y_filter_ptr, offset_vec, vl);
      vuint16mf2_t d3 = highbd_convolve8_4_2d_v_rvv(
          s3, s4, s5, s6, s7, s8, s9, s10, y_filter_ptr, offset_vec, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update sliding window state
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
    } while (h != 0);
  } else {
    // w > 4 case
    vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
    do {
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;
      int height = h;

      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        vuint16m1_t d0 = highbd_convolve8_8_2d_v_rvv(
            s0, s1, s2, s3, s4, s5, s6, s7, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d1 = highbd_convolve8_8_2d_v_rvv(
            s1, s2, s3, s4, s5, s6, s7, s8, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d2 = highbd_convolve8_8_2d_v_rvv(
            s2, s3, s4, s5, s6, s7, s8, s9, y_filter_ptr, offset_vec, vl);
        vuint16m1_t d3 = highbd_convolve8_8_2d_v_rvv(
            s3, s4, s5, s6, s7, s8, s9, s10, y_filter_ptr, offset_vec, vl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

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

      src_ptr += vl;
      dst_ptr += vl;
      w -= (int)vl;
    } while (w > 0);
  }
}

static inline void highbd_dist_wtd_convolve_2d_horiz_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset,
    const int32_t round_bits) {
  const int16_t *filter = x_filter_ptr;

  size_t vl = __riscv_vsetvl_e16m1(w);
  vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
  do {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    int height = h;

    // Process 4 rows at a time to reduce loop overhead
    while (height >= 4) {
      vint16m1_t s00, s01, s02, s03, s04, s05;
      vint16m1_t s10, s11, s12, s13, s14, s15;
      vint16m1_t s20, s21, s22, s23, s24, s25;
      vint16m1_t s30, s31, s32, s33, s34, s35;

      // Load 6 vectors per row with stride 1 (horizontal overlap)
      load_s16_8x6(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                   vl);
      load_s16_8x6(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                   vl);
      load_s16_8x6(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                   vl);
      load_s16_8x6(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                   vl);

      vuint16m1_t d0 = highbd_convolve6_8_rvv(
          s00, s01, s02, s03, s04, s05, filter, offset_vec, round_bits, vl);
      vuint16m1_t d1 = highbd_convolve6_8_rvv(
          s10, s11, s12, s13, s14, s15, filter, offset_vec, round_bits, vl);
      vuint16m1_t d2 = highbd_convolve6_8_rvv(
          s20, s21, s22, s23, s24, s25, filter, offset_vec, round_bits, vl);
      vuint16m1_t d3 = highbd_convolve6_8_rvv(
          s30, s31, s32, s33, s34, s35, filter, offset_vec, round_bits, vl);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    }

    // Handle remaining rows
    while (height > 0) {
      vint16m1_t s00, s01, s02, s03, s04, s05;
      load_s16_8x6(s, 1, &s00, &s01, &s02, &s03, &s04, &s05, vl);
      vuint16m1_t d0 = highbd_convolve6_8_rvv(
          s00, s01, s02, s03, s04, s05, filter, offset_vec, round_bits, vl);
      __riscv_vse16_v_u16m1(d, d0, vl);

      s += src_stride;
      d += dst_stride;
      height--;
    }

    src_ptr += vl;
    dst_ptr += vl;
    w -= (int)vl;
  } while (w > 0);
}

static inline void highbd_dist_wtd_convolve_2d_horiz_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset,
    const int32_t round_bits) {
  assert(h >= 5);

  if (w == 4) {
    size_t vl = __riscv_vsetvl_e16mf2(4);
    vint32m1_t offset_vec = __riscv_vmv_v_x_i32m1(offset, vl);
    const int16_t *filter = x_filter_ptr + 2;  // Use middle 4 taps
    const int16_t *s = (const int16_t *)(src_ptr + 1);
    uint16_t *d = dst_ptr;

    while (h >= 4) {
      vint16mf2_t s00, s01, s02, s03;
      vint16mf2_t s10, s11, s12, s13;
      vint16mf2_t s20, s21, s22, s23;
      vint16mf2_t s30, s31, s32, s33;

      load_s16_4x4(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, vl);
      load_s16_4x4(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, vl);
      load_s16_4x4(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, vl);
      load_s16_4x4(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, vl);

      vuint16mf2_t d0 = highbd_convolve4_4_x_rvv(s00, s01, s02, s03, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d1 = highbd_convolve4_4_x_rvv(s10, s11, s12, s13, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d2 = highbd_convolve4_4_x_rvv(s20, s21, s22, s23, filter,
                                                 offset_vec, round_bits, vl);
      vuint16mf2_t d3 = highbd_convolve4_4_x_rvv(s30, s31, s32, s33, filter,
                                                 offset_vec, round_bits, vl);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    }

    while (h > 0) {
      vint16mf2_t s00, s01, s02, s03;
      load_s16_4x4(s, 1, &s00, &s01, &s02, &s03, vl);
      vuint16mf2_t d0 = highbd_convolve4_4_x_rvv(s00, s01, s02, s03, filter,
                                                 offset_vec, round_bits, vl);
      __riscv_vse16_v_u16mf2(d, d0, vl);
      s += src_stride;
      d += dst_stride;
      h--;
    }
  } else {
    // w > 4 case using 8-tap filter
    const int16_t *filter = x_filter_ptr;
    size_t vl = __riscv_vsetvl_e16m1(w);
    vint32m2_t offset_vec = __riscv_vmv_v_x_i32m2(offset, vl);
    do {
      const int16_t *s_base = (const int16_t *)src_ptr;
      uint16_t *d_base = dst_ptr;
      int height = h;

      while (height >= 4) {
        vint16m1_t s00, s01, s02, s03, s04, s05, s06, s07;
        vint16m1_t s10, s11, s12, s13, s14, s15, s16, s17;
        vint16m1_t s20, s21, s22, s23, s24, s25, s26, s27;
        vint16m1_t s30, s31, s32, s33, s34, s35, s36, s37;

        load_s16_8x8(s_base + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04,
                     &s05, &s06, &s07, vl);
        load_s16_8x8(s_base + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14,
                     &s15, &s16, &s17, vl);
        load_s16_8x8(s_base + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24,
                     &s25, &s26, &s27, vl);
        load_s16_8x8(s_base + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34,
                     &s35, &s36, &s37, vl);

        vuint16m1_t d0 =
            highbd_convolve8_8_rvv(s00, s01, s02, s03, s04, s05, s06, s07,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d1 =
            highbd_convolve8_8_rvv(s10, s11, s12, s13, s14, s15, s16, s17,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d2 =
            highbd_convolve8_8_rvv(s20, s21, s22, s23, s24, s25, s26, s27,
                                   filter, offset_vec, round_bits, vl);
        vuint16m1_t d3 =
            highbd_convolve8_8_rvv(s30, s31, s32, s33, s34, s35, s36, s37,
                                   filter, offset_vec, round_bits, vl);

        store_u16_8x4(d_base, dst_stride, d0, d1, d2, d3, vl);

        s_base += 4 * src_stride;
        d_base += 4 * dst_stride;
        height -= 4;
      }

      while (height > 0) {
        vint16m1_t s00, s01, s02, s03, s04, s05, s06, s07;
        load_s16_8x8(s_base, 1, &s00, &s01, &s02, &s03, &s04, &s05, &s06, &s07,
                     vl);
        vuint16m1_t d0 =
            highbd_convolve8_8_rvv(s00, s01, s02, s03, s04, s05, s06, s07,
                                   filter, offset_vec, round_bits, vl);
        __riscv_vse16_v_u16m1(d_base, d0, vl);
        s_base += src_stride;
        d_base += dst_stride;
        height--;
      }

      src_ptr += vl;
      dst_ptr += vl;
      w -= (int)vl;
    } while (w > 0);
  }
}

void av1_highbd_dist_wtd_convolve_2d_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t,
                  im_block2[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);

  CONV_BUF_TYPE *dst16 = conv_params->dst;
  int dst16_stride = conv_params->dst_stride;
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_x_taps = x_filter_taps < 6 ? 6 : x_filter_taps;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;

  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;

  const int round_offset_conv_x =
      (1 << (bd + FILTER_BITS - 1)) + (1 << (conv_params->round_0 - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset_conv_y = (1 << y_offset_bits);

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  // 1. Horizontal Filter Pass
  int round_bits;
  if (bd == 12) {
    round_bits = ROUND0_BITS + 2;
  } else {
    round_bits = ROUND0_BITS;
  }
  if (x_filter_taps <= 6 && w != 4) {
    highbd_dist_wtd_convolve_2d_horiz_6tap_rvv(src_ptr, src_stride, im_block,
                                               im_stride, w, im_h, x_filter_ptr,
                                               round_offset_conv_x, round_bits);
  } else {
    highbd_dist_wtd_convolve_2d_horiz_rvv(src_ptr, src_stride, im_block,
                                          im_stride, w, im_h, x_filter_ptr,
                                          round_offset_conv_x, round_bits);
  }

  // 2. Vertical Filter Pass
  if (y_filter_taps <= 6) {
    if (conv_params->do_average) {
      highbd_dist_wtd_convolve_2d_vert_6tap_rvv(im_block, im_stride, im_block2,
                                                im_stride, w, h, y_filter_ptr,
                                                round_offset_conv_y);
    } else {
      highbd_dist_wtd_convolve_2d_vert_6tap_rvv(
          im_block, im_stride, dst16, dst16_stride, w, h, y_filter_ptr,
          round_offset_conv_y);
    }
  } else {
    if (conv_params->do_average) {
      highbd_dist_wtd_convolve_2d_vert_8tap_rvv(im_block, im_stride, im_block2,
                                                im_stride, w, h, y_filter_ptr,
                                                round_offset_conv_y);
    } else {
      highbd_dist_wtd_convolve_2d_vert_8tap_rvv(
          im_block, im_stride, dst16, dst16_stride, w, h, y_filter_ptr,
          round_offset_conv_y);
    }
  }

  // 3. Compound Averaging
  if (conv_params->do_average) {
    int shift_bits, offset_bits;
    if (bd == 12) {
      shift_bits = ROUND_SHIFT - 2;
      offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS - 2;
    } else {
      shift_bits = ROUND_SHIFT;
      offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
    }
    if (conv_params->use_dist_wtd_comp_avg) {
      highbd_dist_wtd_comp_avg_rvv(im_block2, im_stride, dst, dst_stride, w, h,
                                   conv_params, bd, shift_bits, offset_bits);
    } else {
      highbd_comp_avg_rvv(im_block2, im_stride, dst, dst_stride, w, h,
                          conv_params, bd, shift_bits, offset_bits);
    }
  }
}
