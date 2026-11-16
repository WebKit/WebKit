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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/riscv/mem_rvv.h"
#include "av1/common/convolve.h"
#include "av1/common/restoration.h"

static inline vuint16m1_t wiener_convolve5_8_2d_h_rvv(
    vuint8mf2_t t0, vuint8mf2_t t1, vuint8mf2_t t2, vuint8mf2_t t3,
    vuint8mf2_t t4, vint16m1_t x_f1, vint16m1_t x_f2, vint16m1_t x_f3,
    vint32m2_t round_vec, uint16_t im_max_val, size_t vl) {
  // Since the Wiener filter is symmetric about the middle tap (tap 2) add
  // mirrored source elements before multiplying filter coefficients.
  vint16m1_t s04 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vwaddu_vv_u16m1(t0, t4, vl));
  vint16m1_t s13 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vwaddu_vv_u16m1(t1, t3, vl));
  vint16m1_t s2 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));

  // x_filter[0] = 0. (5-tap filters are 0-padded to 7 taps.)
  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f1, s04, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f2, s13, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f3, s2, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      WIENER_ROUND0_BITS, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      WIENER_ROUND0_BITS, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, im_max_val, vl);
}

static inline void convolve_add_src_horiz_5tap_rvv(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *x_filter,
    int32_t round_val, uint16_t im_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t x_f1 = __riscv_vmv_v_x_i16m1(x_filter[1], vl_max);
  const vint16m1_t x_f2 = __riscv_vmv_v_x_i16m1(x_filter[2], vl_max);
  const vint16m1_t x_f3 = __riscv_vmv_v_x_i16m1(x_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int row = 0;
  for (; row <= h - 2; row += 2) {
    const uint8_t *sa = src_ptr;
    const uint8_t *sb = src_ptr + src_stride;
    uint16_t *da = dst_ptr;
    uint16_t *db = dst_ptr + dst_stride;
    int width = w;

    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vuint8mf2_t a0, a1, a2, a3, a4;
      load_u8_8x5(sa, 1, &a0, &a1, &a2, &a3, &a4, vl);
      vuint8mf2_t b0, b1, b2, b3, b4;
      load_u8_8x5(sb, 1, &b0, &b1, &b2, &b3, &b4, vl);

      const vuint16m1_t ra = wiener_convolve5_8_2d_h_rvv(
          a0, a1, a2, a3, a4, x_f1, x_f2, x_f3, round_vec, im_max_val, vl);
      const vuint16m1_t rb = wiener_convolve5_8_2d_h_rvv(
          b0, b1, b2, b3, b4, x_f1, x_f2, x_f3, round_vec, im_max_val, vl);
      __riscv_vse16_v_u16m1(da, ra, vl);
      __riscv_vse16_v_u16m1(db, rb, vl);

      sa += vl;
      sb += vl;
      da += vl;
      db += vl;
      width -= (int)vl;
    }
    src_ptr += 2 * src_stride;
    dst_ptr += 2 * dst_stride;
  }
  for (; row < h; row++) {
    const uint8_t *s = src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;
    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vuint8mf2_t s0, s1, s2, s3, s4;
      load_u8_8x5(s, 1, &s0, &s1, &s2, &s3, &s4, vl);
      const vuint16m1_t d0 = wiener_convolve5_8_2d_h_rvv(
          s0, s1, s2, s3, s4, x_f1, x_f2, x_f3, round_vec, im_max_val, vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += vl;
      d += vl;
      width -= (int)vl;
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static inline vuint16m1_t wiener_convolve7_8_2d_h_rvv(
    vuint8mf2_t t0, vuint8mf2_t t1, vuint8mf2_t t2, vuint8mf2_t t3,
    vuint8mf2_t t4, vuint8mf2_t t5, vuint8mf2_t t6, vint16m1_t x_f0,
    vint16m1_t x_f1, vint16m1_t x_f2, vint16m1_t x_f3, vint32m2_t round_vec,
    uint16_t im_max_val, size_t vl) {
  // Since the Wiener filter is symmetric about the middle tap (tap 3) add
  // mirrored source elements before multiplying by filter coefficients.
  vint16m1_t s06 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vwaddu_vv_u16m1(t0, t6, vl));
  vint16m1_t s15 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vwaddu_vv_u16m1(t1, t5, vl));
  vint16m1_t s24 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vwaddu_vv_u16m1(t2, t4, vl));
  vint16m1_t s3 =
      __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));

  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f0, s06, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f1, s15, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f2, s24, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, x_f3, s3, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      WIENER_ROUND0_BITS, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      WIENER_ROUND0_BITS, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, im_max_val, vl);
}

static inline void convolve_add_src_horiz_7tap_rvv(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *x_filter,
    int32_t round_val, uint16_t im_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t x_f0 = __riscv_vmv_v_x_i16m1(x_filter[0], vl_max);
  const vint16m1_t x_f1 = __riscv_vmv_v_x_i16m1(x_filter[1], vl_max);
  const vint16m1_t x_f2 = __riscv_vmv_v_x_i16m1(x_filter[2], vl_max);
  const vint16m1_t x_f3 = __riscv_vmv_v_x_i16m1(x_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int row = 0;
  for (; row <= h - 2; row += 2) {
    const uint8_t *sa = src_ptr;
    const uint8_t *sb = src_ptr + src_stride;
    uint16_t *da = dst_ptr;
    uint16_t *db = dst_ptr + dst_stride;
    int width = w;

    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vuint8mf2_t a0, a1, a2, a3, a4, a5, a6;
      load_u8_8x7(sa, 1, &a0, &a1, &a2, &a3, &a4, &a5, &a6, vl);
      vuint8mf2_t b0, b1, b2, b3, b4, b5, b6;
      load_u8_8x7(sb, 1, &b0, &b1, &b2, &b3, &b4, &b5, &b6, vl);

      const vuint16m1_t ra =
          wiener_convolve7_8_2d_h_rvv(a0, a1, a2, a3, a4, a5, a6, x_f0, x_f1,
                                      x_f2, x_f3, round_vec, im_max_val, vl);
      const vuint16m1_t rb =
          wiener_convolve7_8_2d_h_rvv(b0, b1, b2, b3, b4, b5, b6, x_f0, x_f1,
                                      x_f2, x_f3, round_vec, im_max_val, vl);
      __riscv_vse16_v_u16m1(da, ra, vl);
      __riscv_vse16_v_u16m1(db, rb, vl);

      sa += vl;
      sb += vl;
      da += vl;
      db += vl;
      width -= (int)vl;
    }
    src_ptr += 2 * src_stride;
    dst_ptr += 2 * dst_stride;
  }
  for (; row < h; row++) {
    const uint8_t *s = src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;
    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vuint8mf2_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, 1, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      const vuint16m1_t d0 =
          wiener_convolve7_8_2d_h_rvv(s0, s1, s2, s3, s4, s5, s6, x_f0, x_f1,
                                      x_f2, x_f3, round_vec, im_max_val, vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += vl;
      d += vl;
      width -= (int)vl;
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static inline vuint8mf2_t wiener_convolve5_8_2d_v_rvv(
    vint16m1_t s0, vint16m1_t s1, vint16m1_t s2, vint16m1_t s3, vint16m1_t s4,
    vint16m1_t y_f1, vint16m1_t y_f2, vint16m1_t y_f3, vint32m2_t round_vec,
    size_t vl) {
  // Since the Wiener filter is symmetric about the middle tap (tap 2) add
  // mirrored source elements before multiplying by filter coefficients.
  vint16m1_t s04 = __riscv_vadd_vv_i16m1(s0, s4, vl);
  vint16m1_t s13 = __riscv_vadd_vv_i16m1(s1, s3, vl);

  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f1, s04, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f2, s13, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f3, s2, vl);

#if __riscv_v_intrinsic >= 12000
  vint16m1_t res_16 = __riscv_vnclip_wx_i16m1(
      sum, 2 * FILTER_BITS - WIENER_ROUND0_BITS, __RISCV_VXRM_RDN, vl);
  return __riscv_vnclipu_wx_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res_16, 0, vl)),
      0, __RISCV_VXRM_RNU, vl);
#else
  vint16m1_t res_16 =
      __riscv_vnsra_wx_i16m1(sum, 2 * FILTER_BITS - WIENER_ROUND0_BITS, vl);
  return __riscv_vnclipu_wx_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res_16, 0, vl)),
      0, vl);
#endif
}

static inline void convolve_add_src_vert_5tap_rvv(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16_t *y_filter,
    int32_t round_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t y_f1 = __riscv_vmv_v_x_i16m1(y_filter[1], vl_max);
  const vint16m1_t y_f2 = __riscv_vmv_v_x_i16m1(y_filter[2], vl_max);
  const vint16m1_t y_f3 = __riscv_vmv_v_x_i16m1(y_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int width = w;
  const int16_t *s_base = (const int16_t *)src;
  uint8_t *d_base = dst;

  while (width > 0) {
    const size_t vl = __riscv_vsetvl_e16m1((size_t)width);

    const int16_t *s = s_base;
    vint16m1_t s0 = __riscv_vle16_v_i16m1(s, vl);
    s += src_stride;
    vint16m1_t s1 = __riscv_vle16_v_i16m1(s, vl);
    s += src_stride;
    vint16m1_t s2 = __riscv_vle16_v_i16m1(s, vl);
    s += src_stride;
    vint16m1_t s3 = __riscv_vle16_v_i16m1(s, vl);
    s += src_stride;

    uint8_t *d = d_base;
    int i = 0;
    for (; i <= h - 4; i += 4) {
      vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s5 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s7 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      vuint8mf2_t d0 = wiener_convolve5_8_2d_v_rvv(s0, s1, s2, s3, s4, y_f1,
                                                   y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d1 = wiener_convolve5_8_2d_v_rvv(s1, s2, s3, s4, s5, y_f1,
                                                   y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d2 = wiener_convolve5_8_2d_v_rvv(s2, s3, s4, s5, s6, y_f1,
                                                   y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d3 = wiener_convolve5_8_2d_v_rvv(s3, s4, s5, s6, s7, y_f1,
                                                   y_f2, y_f3, round_vec, vl);

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
    }

    for (; i < h; i++) {
      vint16m1_t s4 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vuint8mf2_t d0 = wiener_convolve5_8_2d_v_rvv(s0, s1, s2, s3, s4, y_f1,
                                                   y_f2, y_f3, round_vec, vl);
      __riscv_vse8_v_u8mf2(d, d0, vl);
      d += dst_stride;
      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
    }

    s_base += vl;
    d_base += vl;
    width -= (int)vl;
  }
}

static inline vuint8mf2_t wiener_convolve7_8_2d_v_rvv(
    vint16m1_t s0, vint16m1_t s1, vint16m1_t s2, vint16m1_t s3, vint16m1_t s4,
    vint16m1_t s5, vint16m1_t s6, vint16m1_t y_f0, vint16m1_t y_f1,
    vint16m1_t y_f2, vint16m1_t y_f3, vint32m2_t round_vec, size_t vl) {
  // Since the Wiener filter is symmetric about the middle tap (tap 3) add
  // mirrored source elements before multiplying by filter coefficients.
  vint16m1_t s06 = __riscv_vadd_vv_i16m1(s0, s6, vl);
  vint16m1_t s15 = __riscv_vadd_vv_i16m1(s1, s5, vl);
  vint16m1_t s24 = __riscv_vadd_vv_i16m1(s2, s4, vl);

  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f0, s06, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f1, s15, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f2, s24, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, y_f3, s3, vl);

#if __riscv_v_intrinsic >= 12000
  vint16m1_t res_16 = __riscv_vnclip_wx_i16m1(
      sum, 2 * FILTER_BITS - WIENER_ROUND0_BITS, __RISCV_VXRM_RDN, vl);
  return __riscv_vnclipu_wx_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res_16, 0, vl)),
      0, __RISCV_VXRM_RNU, vl);
#else
  vint16m1_t res_16 =
      __riscv_vnsra_wx_i16m1(sum, 2 * FILTER_BITS - WIENER_ROUND0_BITS, vl);
  return __riscv_vnclipu_wx_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res_16, 0, vl)),
      0, vl);
#endif
}

static inline void convolve_add_src_vert_7tap_rvv(
    const uint16_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16_t *y_filter,
    int32_t round_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t y_f0 = __riscv_vmv_v_x_i16m1(y_filter[0], vl_max);
  const vint16m1_t y_f1 = __riscv_vmv_v_x_i16m1(y_filter[1], vl_max);
  const vint16m1_t y_f2 = __riscv_vmv_v_x_i16m1(y_filter[2], vl_max);
  const vint16m1_t y_f3 = __riscv_vmv_v_x_i16m1(y_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int width = w;
  const int16_t *s_base = (const int16_t *)src;
  uint8_t *d_base = dst;

  while (width > 0) {
    const size_t vl = __riscv_vsetvl_e16m1((size_t)width);

    const int16_t *s = s_base;
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

    uint8_t *d = d_base;
    int i = 0;
    for (; i <= h - 4; i += 4) {
      vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s7 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s8 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vint16m1_t s9 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;

      vuint8mf2_t d0 = wiener_convolve7_8_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, y_f0, y_f1, y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d1 = wiener_convolve7_8_2d_v_rvv(
          s1, s2, s3, s4, s5, s6, s7, y_f0, y_f1, y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d2 = wiener_convolve7_8_2d_v_rvv(
          s2, s3, s4, s5, s6, s7, s8, y_f0, y_f1, y_f2, y_f3, round_vec, vl);
      vuint8mf2_t d3 = wiener_convolve7_8_2d_v_rvv(
          s3, s4, s5, s6, s7, s8, s9, y_f0, y_f1, y_f2, y_f3, round_vec, vl);

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
    }

    for (; i < h; i++) {
      vint16m1_t s6 = __riscv_vle16_v_i16m1(s, vl);
      s += src_stride;
      vuint8mf2_t d0 = wiener_convolve7_8_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, y_f0, y_f1, y_f2, y_f3, round_vec, vl);
      __riscv_vse8_v_u8mf2(d, d0, vl);
      d += dst_stride;
      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
    }

    s_base += vl;
    d_base += vl;
    width -= (int)vl;
  }
}

static inline int get_wiener_filter_taps(const int16_t *filter) {
  assert(filter[7] == 0);
  if (filter[0] == 0 && filter[6] == 0) {
    return WIENER_WIN_REDUCED;
  }
  return WIENER_WIN;
}

// Wiener filter 2D
// Apply horizontal filter and store in a temporary buffer. When applying
// vertical filter, overwrite the original pixel values.
void av1_wiener_convolve_add_src_rvv(const uint8_t *src, ptrdiff_t src_stride,
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

  const int x_filter_taps = get_wiener_filter_taps(x_filter);
  const int y_filter_taps = get_wiener_filter_taps(y_filter);

  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + WIENER_WIN - 1) * MAX_SB_SIZE]);

  const int bd = 8;
  const uint16_t im_max_val =
      (1 << (bd + 1 + FILTER_BITS - WIENER_ROUND0_BITS)) - 1;
  const int32_t horiz_round_val = 1 << (bd + FILTER_BITS - 1);
  const int32_t vert_round_val =
      (1 << (2 * FILTER_BITS - WIENER_ROUND0_BITS - 1)) -
      (1 << (bd + (2 * FILTER_BITS - WIENER_ROUND0_BITS) - 1));

  const int im_stride = (w <= 32) ? w : MAX_SB_SIZE;
  const int im_h = h + y_filter_taps - 1;
  const int horiz_offset = x_filter_taps / 2;
  const int vert_offset = (y_filter_taps / 2) * (int)src_stride;

  int16_t x_filter_5[8];
  int16_t y_filter_5[8];
  // Add 128 to tap 3. (Needed for rounding.)
  for (int i = 0; i < 8; ++i) x_filter_5[i] = x_filter[i];
  x_filter_5[3] += 128;
  for (int i = 0; i < 8; ++i) y_filter_5[i] = y_filter[i];
  y_filter_5[3] += 128;

  if (x_filter_taps == WIENER_WIN_REDUCED) {
    convolve_add_src_horiz_5tap_rvv(src - horiz_offset - vert_offset,
                                    src_stride, im_block, im_stride, w, im_h,
                                    x_filter_5, horiz_round_val, im_max_val);
  } else {
    convolve_add_src_horiz_7tap_rvv(src - horiz_offset - vert_offset,
                                    src_stride, im_block, im_stride, w, im_h,
                                    x_filter_5, horiz_round_val, im_max_val);
  }

  if (y_filter_taps == WIENER_WIN_REDUCED) {
    convolve_add_src_vert_5tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_5, vert_round_val);
  } else {
    convolve_add_src_vert_7tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_5, vert_round_val);
  }
}
