/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_RISCV_CONVOLVE_RVV_H_
#define AOM_AV1_COMMON_RISCV_CONVOLVE_RVV_H_

#include "config/aom_config.h"

#include "av1/common/convolve.h"
#include "av1/common/filter.h"

// load_strided_u8_4xN
static inline vuint8mf2_t load_strided_u8_4xN(uint8_t *addr, ptrdiff_t stride,
                                              size_t vl) {
  const vuint8mf2_t px_l1 = __riscv_vle8_v_u8mf2(addr + stride, vl);
  const vuint8mf2_t px_l0 = __riscv_vle8_v_u8mf2(addr, vl);
  return __riscv_vslideup_vx_u8mf2(px_l0, px_l1, vl >> 1, vl);
}

// store_strided_u8_4xN
static inline void store_strided_u8_4xN(uint8_t *addr, vuint8mf2_t vdst,
                                        ptrdiff_t stride, size_t vl) {
  __riscv_vse8_v_u8mf2(addr, vdst, vl >> 1);
  vdst = __riscv_vslidedown_vx_u8mf2(vdst, vl >> 1, vl);
  __riscv_vse8_v_u8mf2(addr + stride, vdst, vl >> 1);
}

// load_strided_i16_4xN
static inline vint16m1_t load_strided_i16_4xN(int16_t *addr, ptrdiff_t stride,
                                              size_t vl) {
  const vint16m1_t px_l1 = __riscv_vle16_v_i16m1(addr + stride, vl >> 1);
  const vint16m1_t px_l0 = __riscv_vle16_v_i16m1(addr, vl >> 1);
  return __riscv_vslideup_vx_i16m1(px_l0, px_l1, vl >> 1, vl);
}

// store_strided_i16_4xN
static inline void store_strided_i16_4xN(int16_t *addr, vint16m1_t vdst,
                                         ptrdiff_t stride, size_t vl) {
  __riscv_vse16_v_i16m1(addr, vdst, vl >> 1);
  vdst = __riscv_vslidedown_vx_i16m1(vdst, vl >> 1, vl);
  __riscv_vse16_v_i16m1(addr + stride, vdst, vl >> 1);
}

static inline vuint8mf2_t convolve12_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *y_filter, const int16_t sub_const, const int vert_const,
    size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[11], s11, vl);
  sum = __riscv_vadd_vx_i32m2(sum, vert_const, vl);

  vint16m1_t i16_sum =
      __riscv_vnsra_wx_i16m1(sum, ((FILTER_BITS << 1) - ROUND0_BITS), vl);
  i16_sum = __riscv_vsub_vx_i16m1(i16_sum, sub_const, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_2d_sr_vert_12tap_rvv(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16_t *y_filter_ptr, size_t vl) {
  const int vert_const = (1 << ((FILTER_BITS << 1) - ROUND0_BITS)) >> 1;
  const int16_t sub_const = 1 << FILTER_BITS;

  if (w == 4) {
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s4 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s5 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s6 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s7 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s8 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s9 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;

    do {
      vint16m1_t s10 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s11 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s12 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s13 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;

      vuint8mf2_t d0 =
          convolve12_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                              y_filter_ptr, sub_const, vert_const, vl);
      vuint8mf2_t d1 =
          convolve12_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                              s13, y_filter_ptr, sub_const, vert_const, vl);

      store_strided_u8_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;
      store_strided_u8_4xN(dst_ptr, d1, dst_stride, vl);
      dst_ptr += dst_stride << 1;

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

      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      vint16m1_t s0 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s1 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s2 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s3 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s5 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s7 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s8 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s9 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s10 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      do {
        vint16m1_t s11 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s12 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s13 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s14 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;

        vuint8mf2_t d0 =
            convolve12_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                s11, y_filter_ptr, sub_const, vert_const, vl);
        vuint8mf2_t d1 =
            convolve12_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                s12, y_filter_ptr, sub_const, vert_const, vl);
        vuint8mf2_t d2 =
            convolve12_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                s13, y_filter_ptr, sub_const, vert_const, vl);
        vuint8mf2_t d3 =
            convolve12_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                                s14, y_filter_ptr, sub_const, vert_const, vl);

        __riscv_vse8_v_u8mf2(d, d0, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d1, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d2, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d3, vl);
        d += dst_stride;

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

        height -= 4;
      } while (height != 0);

      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w != 0);
  }
}

static inline vuint8mf2_t convolve8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *y_filter,
    const int16_t sub_const, const int vert_const, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[7], s7, vl);
  sum = __riscv_vadd_vx_i32m2(sum, vert_const, vl);

  vint16m1_t i16_sum =
      __riscv_vnsra_wx_i16m1(sum, ((FILTER_BITS << 1) - ROUND0_BITS), vl);
  i16_sum = __riscv_vsub_vx_i16m1(i16_sum, sub_const, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_2d_sr_vert_8tap_rvv(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16_t *y_filter_ptr, size_t vl) {
  const int vert_const = (1 << ((FILTER_BITS << 1) - ROUND0_BITS)) >> 1;
  const int16_t sub_const = 1 << FILTER_BITS;

  if (w <= 4) {
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s4 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s5 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;

    do {
      vint16m1_t s6 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s7 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;

      vuint8mf2_t d0 =
          convolve8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter_ptr,
                             sub_const, vert_const, vl);

      store_strided_u8_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;

      s0 = s2;
      s1 = s3;
      s2 = s4;
      s3 = s5;
      s4 = s6;
      s5 = s7;

      h -= 2;
    } while (h != 0);
  } else {
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      vint16m1_t s0 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s1 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s2 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s3 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s5 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      do {
        vint16m1_t s7 = __riscv_vle16_v_i16m1(s, vl);
        vuint8mf2_t d0 =
            convolve8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter_ptr,
                               sub_const, vert_const, vl);
        __riscv_vse8_v_u8mf2(d, d0, vl);

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
      } while (height != 0);

      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w != 0);
  }
}

static inline vuint8mf2_t convolve6_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *y_filter, const int16_t sub_const, const int vert_const,
    size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vadd_vx_i32m2(sum, vert_const, vl);

  vint16m1_t i16_sum =
      __riscv_vnsra_wx_i16m1(sum, ((FILTER_BITS << 1) - ROUND0_BITS), vl);
  i16_sum = __riscv_vsub_vx_i16m1(i16_sum, sub_const, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_2d_sr_vert_6tap_rvv(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16_t *y_filter_ptr, size_t vl) {
  const int vert_const = (1 << ((FILTER_BITS << 1) - ROUND0_BITS)) >> 1;
  const int16_t sub_const = 1 << FILTER_BITS;

  const int16_t *filter = y_filter_ptr + 1;

  if (w <= 4) {
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;

    do {
      vint16m1_t s4 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s5 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s6 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s7 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;

      vuint8mf2_t d0 = convolve6_2d_v_rvv(s0, s1, s2, s3, s4, s5, filter,
                                          sub_const, vert_const, vl);
      vuint8mf2_t d1 = convolve6_2d_v_rvv(s2, s3, s4, s5, s6, s7, filter,
                                          sub_const, vert_const, vl);

      store_strided_u8_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;
      store_strided_u8_4xN(dst_ptr, d1, dst_stride, vl);
      dst_ptr += dst_stride << 1;

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;

      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      vint16m1_t s0 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s1 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s2 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s3 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      do {
        vint16m1_t s5 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s7 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s8 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;

        vuint8mf2_t d0 = convolve6_2d_v_rvv(s0, s1, s2, s3, s4, s5, filter,
                                            sub_const, vert_const, vl);
        vuint8mf2_t d1 = convolve6_2d_v_rvv(s1, s2, s3, s4, s5, s6, filter,
                                            sub_const, vert_const, vl);
        vuint8mf2_t d2 = convolve6_2d_v_rvv(s2, s3, s4, s5, s6, s7, filter,
                                            sub_const, vert_const, vl);
        vuint8mf2_t d3 = convolve6_2d_v_rvv(s3, s4, s5, s6, s7, s8, filter,
                                            sub_const, vert_const, vl);

        __riscv_vse8_v_u8mf2(d, d0, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d1, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d2, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d3, vl);
        d += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;

        height -= 4;
      } while (height != 0);

      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w != 0);
  }
}

static inline vuint8mf2_t convolve4_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *y_filter, const int16_t sub_const,
    const int vert_const, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vadd_vx_i32m2(sum, vert_const, vl);

  vint16m1_t i16_sum =
      __riscv_vnsra_wx_i16m1(sum, ((FILTER_BITS << 1) - ROUND0_BITS), vl);
  i16_sum = __riscv_vsub_vx_i16m1(i16_sum, sub_const, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_2d_sr_vert_4tap_rvv(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16_t *y_filter_ptr, size_t vl) {
  const int vert_const = (1 << ((FILTER_BITS << 1) - ROUND0_BITS)) >> 1;
  const int16_t sub_const = 1 << FILTER_BITS;
  // Filter values are at offset 2
  const int16_t *filter = y_filter_ptr + 2;

  if (w <= 4) {
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(src_ptr, src_stride, vl);
    src_ptr += src_stride;

    do {
      vint16m1_t s2 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s3 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s4 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;
      vint16m1_t s5 = load_strided_i16_4xN(src_ptr, src_stride, vl);
      src_ptr += src_stride;

      vuint8mf2_t d0 =
          convolve4_2d_v_rvv(s0, s1, s2, s3, filter, sub_const, vert_const, vl);
      vuint8mf2_t d1 =
          convolve4_2d_v_rvv(s2, s3, s4, s5, filter, sub_const, vert_const, vl);

      store_strided_u8_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;
      store_strided_u8_4xN(dst_ptr, d1, dst_stride, vl);
      dst_ptr += dst_stride << 1;

      s0 = s4;
      s1 = s5;

      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      vint16m1_t s0 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s1 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s2 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      do {
        vint16m1_t s3 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s5 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;
        vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
        s += src_stride;

        vuint8mf2_t d0 = convolve4_2d_v_rvv(s0, s1, s2, s3, filter, sub_const,
                                            vert_const, vl);
        vuint8mf2_t d1 = convolve4_2d_v_rvv(s1, s2, s3, s4, filter, sub_const,
                                            vert_const, vl);
        vuint8mf2_t d2 = convolve4_2d_v_rvv(s2, s3, s4, s5, filter, sub_const,
                                            vert_const, vl);
        vuint8mf2_t d3 = convolve4_2d_v_rvv(s3, s4, s5, s6, filter, sub_const,
                                            vert_const, vl);

        __riscv_vse8_v_u8mf2(d, d0, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d1, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d2, vl);
        d += dst_stride;
        __riscv_vse8_v_u8mf2(d, d3, vl);
        d += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;

        height -= 4;
      } while (height != 0);

      src_ptr += vl;
      dst_ptr += vl;
      w -= vl;
    } while (w != 0);
  }
}

#endif  // AOM_AV1_COMMON_RISCV_CONVOLVE_RVV_H_
