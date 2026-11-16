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

static inline vuint16m1_t highbd_wiener_convolve5_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t xf1,
    const vint16m1_t xf2, const vint16m1_t xf3, const vint32m2_t round_vec,
    int horiz_shift, uint16_t im_max_val, size_t vl) {
  vint16m1_t s04 = __riscv_vadd_vv_i16m1(s0, s4, vl);
  vint16m1_t s13 = __riscv_vadd_vv_i16m1(s1, s3, vl);

  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, xf1, s04, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, xf2, s13, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, xf3, s2, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)horiz_shift, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)horiz_shift, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, im_max_val, vl);
}

static inline vuint16m1_t highbd_wiener_convolve7_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t xf0, const vint16m1_t xf1,
    const vint16m1_t xf2, const vint16m1_t xf3, const vint32m2_t round_vec,
    int horiz_shift, uint16_t im_max_val, size_t vl) {
  vint16m1_t s06 = __riscv_vadd_vv_i16m1(s0, s6, vl);
  vint16m1_t s15 = __riscv_vadd_vv_i16m1(s1, s5, vl);
  vint16m1_t s24 = __riscv_vadd_vv_i16m1(s2, s4, vl);

  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, xf0, s06, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, xf1, s15, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, xf2, s24, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, xf3, s3, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)horiz_shift, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)horiz_shift, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, im_max_val, vl);
}

static inline vuint16m1_t highbd_wiener_convolve5_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t yf1,
    const vint16m1_t yf2, const vint16m1_t yf3, const vint32m2_t round_vec,
    int vert_shift, uint16_t res_max_val, size_t vl) {
  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, yf1, s0, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf1, s4, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf2, s1, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf2, s3, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf3, s2, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)vert_shift, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)vert_shift, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, res_max_val, vl);
}

static inline vuint16m1_t highbd_wiener_convolve7_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t yf0, const vint16m1_t yf1,
    const vint16m1_t yf2, const vint16m1_t yf3, const vint32m2_t round_vec,
    int vert_shift, uint16_t res_max_val, size_t vl) {
  vint32m2_t sum = round_vec;
  sum = __riscv_vwmacc_vv_i32m2(sum, yf0, s0, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf0, s6, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf1, s1, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf1, s5, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf2, s2, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf2, s4, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, yf3, s3, vl);

#if __riscv_v_intrinsic >= 12000
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)vert_shift, __RISCV_VXRM_RNU, vl);
#else
  vuint16m1_t res = __riscv_vnclipu_wx_u16m1(
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl)),
      (size_t)vert_shift, vl);
#endif
  return __riscv_vminu_vx_u16m1(res, res_max_val, vl);
}

static void highbd_convolve_add_src_horiz_5tap_rvv(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *x_filter,
    int32_t round_val, int horiz_shift, uint16_t im_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t xf1 = __riscv_vmv_v_x_i16m1(x_filter[1], vl_max);
  const vint16m1_t xf2 = __riscv_vmv_v_x_i16m1(x_filter[2], vl_max);
  const vint16m1_t xf3 = __riscv_vmv_v_x_i16m1(x_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int row = 0;
  for (; row <= h - 2; row += 2) {
    const int16_t *sa = (const int16_t *)src_ptr;
    const int16_t *sb = (const int16_t *)(src_ptr + src_stride);
    uint16_t *da = dst_ptr;
    uint16_t *db = dst_ptr + dst_stride;
    int width = w;

    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vint16m1_t a0, a1, a2, a3, a4;
      load_s16_8x5(sa, 1, &a0, &a1, &a2, &a3, &a4, vl);
      vint16m1_t b0, b1, b2, b3, b4;
      load_s16_8x5(sb, 1, &b0, &b1, &b2, &b3, &b4, vl);

      const vuint16m1_t ra = highbd_wiener_convolve5_8_2d_h_rvv(
          a0, a1, a2, a3, a4, xf1, xf2, xf3, round_vec, horiz_shift, im_max_val,
          vl);
      const vuint16m1_t rb = highbd_wiener_convolve5_8_2d_h_rvv(
          b0, b1, b2, b3, b4, xf1, xf2, xf3, round_vec, horiz_shift, im_max_val,
          vl);
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
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;
    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, 1, &s0, &s1, &s2, &s3, &s4, vl);
      const vuint16m1_t d0 = highbd_wiener_convolve5_8_2d_h_rvv(
          s0, s1, s2, s3, s4, xf1, xf2, xf3, round_vec, horiz_shift, im_max_val,
          vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += vl;
      d += vl;
      width -= (int)vl;
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static void highbd_convolve_add_src_horiz_7tap_rvv(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *x_filter,
    int32_t round_val, int horiz_shift, uint16_t im_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t xf0 = __riscv_vmv_v_x_i16m1(x_filter[0], vl_max);
  const vint16m1_t xf1 = __riscv_vmv_v_x_i16m1(x_filter[1], vl_max);
  const vint16m1_t xf2 = __riscv_vmv_v_x_i16m1(x_filter[2], vl_max);
  const vint16m1_t xf3 = __riscv_vmv_v_x_i16m1(x_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int row = 0;
  for (; row <= h - 2; row += 2) {
    const int16_t *sa = (const int16_t *)src_ptr;
    const int16_t *sb = (const int16_t *)(src_ptr + src_stride);
    uint16_t *da = dst_ptr;
    uint16_t *db = dst_ptr + dst_stride;
    int width = w;

    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vint16m1_t a0, a1, a2, a3, a4, a5, a6;
      load_s16_8x7(sa, 1, &a0, &a1, &a2, &a3, &a4, &a5, &a6, vl);
      vint16m1_t b0, b1, b2, b3, b4, b5, b6;
      load_s16_8x7(sb, 1, &b0, &b1, &b2, &b3, &b4, &b5, &b6, vl);

      const vuint16m1_t ra = highbd_wiener_convolve7_8_2d_h_rvv(
          a0, a1, a2, a3, a4, a5, a6, xf0, xf1, xf2, xf3, round_vec,
          horiz_shift, im_max_val, vl);
      const vuint16m1_t rb = highbd_wiener_convolve7_8_2d_h_rvv(
          b0, b1, b2, b3, b4, b5, b6, xf0, xf1, xf2, xf3, round_vec,
          horiz_shift, im_max_val, vl);
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
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;
    int width = w;
    while (width > 0) {
      const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, 1, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      const vuint16m1_t d0 = highbd_wiener_convolve7_8_2d_h_rvv(
          s0, s1, s2, s3, s4, s5, s6, xf0, xf1, xf2, xf3, round_vec,
          horiz_shift, im_max_val, vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += vl;
      d += vl;
      width -= (int)vl;
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static void highbd_convolve_add_src_vert_5tap_rvv(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *y_filter,
    int32_t round_val, int vert_shift, uint16_t res_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t yf1 = __riscv_vmv_v_x_i16m1(y_filter[1], vl_max);
  const vint16m1_t yf2 = __riscv_vmv_v_x_i16m1(y_filter[2], vl_max);
  const vint16m1_t yf3 = __riscv_vmv_v_x_i16m1(y_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int width = w;
  const int16_t *s_base = (const int16_t *)src_ptr;
  uint16_t *d_base = dst_ptr;

  while (width > 0) {
    const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
    const int16_t *s = s_base;
    uint16_t *d = d_base;
    int height = h;

    while (height > 3) {
      vint16m1_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_8x8(s, (int)src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,
                   vl);
      vuint16m1_t d0 = highbd_wiener_convolve5_8_2d_v_rvv(
          s0, s1, s2, s3, s4, yf1, yf2, yf3, round_vec, vert_shift, res_max_val,
          vl);
      vuint16m1_t d1 = highbd_wiener_convolve5_8_2d_v_rvv(
          s1, s2, s3, s4, s5, yf1, yf2, yf3, round_vec, vert_shift, res_max_val,
          vl);
      vuint16m1_t d2 = highbd_wiener_convolve5_8_2d_v_rvv(
          s2, s3, s4, s5, s6, yf1, yf2, yf3, round_vec, vert_shift, res_max_val,
          vl);
      vuint16m1_t d3 = highbd_wiener_convolve5_8_2d_v_rvv(
          s3, s4, s5, s6, s7, yf1, yf2, yf3, round_vec, vert_shift, res_max_val,
          vl);
      store_u16_8x4(d, (int)dst_stride, d0, d1, d2, d3, vl);
      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    }
    while (height-- != 0) {
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, (int)src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      vuint16m1_t d0 = highbd_wiener_convolve5_8_2d_v_rvv(
          s0, s1, s2, s3, s4, yf1, yf2, yf3, round_vec, vert_shift, res_max_val,
          vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += src_stride;
      d += dst_stride;
    }

    s_base += vl;
    d_base += vl;
    width -= (int)vl;
  }
}

static void highbd_convolve_add_src_vert_7tap_rvv(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, int w, int h, const int16_t *y_filter,
    int32_t round_val, int vert_shift, uint16_t res_max_val) {
  const size_t vl_max = __riscv_vsetvlmax_e16m1();
  const vint16m1_t yf0 = __riscv_vmv_v_x_i16m1(y_filter[0], vl_max);
  const vint16m1_t yf1 = __riscv_vmv_v_x_i16m1(y_filter[1], vl_max);
  const vint16m1_t yf2 = __riscv_vmv_v_x_i16m1(y_filter[2], vl_max);
  const vint16m1_t yf3 = __riscv_vmv_v_x_i16m1(y_filter[3], vl_max);
  const vint32m2_t round_vec = __riscv_vmv_v_x_i32m2(round_val, vl_max);

  int width = w;
  const int16_t *s_base = (const int16_t *)src_ptr;
  uint16_t *d_base = dst_ptr;

  while (width > 0) {
    const size_t vl = __riscv_vsetvl_e16m1((size_t)width);
    const int16_t *s = s_base;
    uint16_t *d = d_base;
    int height = h;

    while (height > 3) {
      vint16m1_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
      load_s16_8x10(s, (int)src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,
                    &s8, &s9, vl);
      vuint16m1_t d0 = highbd_wiener_convolve7_8_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, yf0, yf1, yf2, yf3, round_vec, vert_shift,
          res_max_val, vl);
      vuint16m1_t d1 = highbd_wiener_convolve7_8_2d_v_rvv(
          s1, s2, s3, s4, s5, s6, s7, yf0, yf1, yf2, yf3, round_vec, vert_shift,
          res_max_val, vl);
      vuint16m1_t d2 = highbd_wiener_convolve7_8_2d_v_rvv(
          s2, s3, s4, s5, s6, s7, s8, yf0, yf1, yf2, yf3, round_vec, vert_shift,
          res_max_val, vl);
      vuint16m1_t d3 = highbd_wiener_convolve7_8_2d_v_rvv(
          s3, s4, s5, s6, s7, s8, s9, yf0, yf1, yf2, yf3, round_vec, vert_shift,
          res_max_val, vl);
      store_u16_8x4(d, (int)dst_stride, d0, d1, d2, d3, vl);
      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    }
    while (height-- != 0) {
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, (int)src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      vuint16m1_t d0 = highbd_wiener_convolve7_8_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, yf0, yf1, yf2, yf3, round_vec, vert_shift,
          res_max_val, vl);
      __riscv_vse16_v_u16m1(d, d0, vl);
      s += src_stride;
      d += dst_stride;
    }

    s_base += vl;
    d_base += vl;
    width -= (int)vl;
  }
}

static inline int highbd_get_wiener_filter_taps(const int16_t *filter) {
  assert(filter[7] == 0);
  if (filter[0] == 0 && filter[6] == 0) return WIENER_WIN_REDUCED;
  return WIENER_WIN;
}

void av1_highbd_wiener_convolve_add_src_rvv(
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

  const int x_filter_taps = highbd_get_wiener_filter_taps(x_filter);
  const int y_filter_taps = highbd_get_wiener_filter_taps(y_filter);

  const int horiz_shift = conv_params->round_0;
  const int vert_shift = conv_params->round_1;

  const int im_stride = MAX_SB_SIZE;
  const int im_h = h + y_filter_taps - 1;
  const int horiz_offset = x_filter_taps / 2;
  const int vert_offset = (y_filter_taps / 2) * (int)src_stride;

  const int extraprec_clamp_limit =
      WIENER_CLAMP_LIMIT(conv_params->round_0, bd);
  const uint16_t im_max_val = (uint16_t)(extraprec_clamp_limit - 1);
  const int32_t horiz_round_val = 1 << (bd + FILTER_BITS - 1);

  const uint16_t res_max_val = (uint16_t)((1 << bd) - 1);
  const int32_t vert_round_val = -(1 << (bd + conv_params->round_1 - 1));

  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

  // Filter taps are symmetric; add 128 to center tap (index 3) for rounding
  // bias, matching the NEON implementation.
  int16_t xf[4] = { x_filter[0], x_filter[1], x_filter[2],
                    (int16_t)(x_filter[3] + 128) };
  int16_t yf[4] = { y_filter[0], y_filter[1], y_filter[2],
                    (int16_t)(y_filter[3] + 128) };

  const uint16_t *hsrc = src - horiz_offset - vert_offset;

  if (x_filter_taps == WIENER_WIN_REDUCED) {
    highbd_convolve_add_src_horiz_5tap_rvv(
        hsrc, src_stride, im_block, im_stride, w, im_h, xf, horiz_round_val,
        horiz_shift, im_max_val);
  } else {
    highbd_convolve_add_src_horiz_7tap_rvv(
        hsrc, src_stride, im_block, im_stride, w, im_h, xf, horiz_round_val,
        horiz_shift, im_max_val);
  }

  if (y_filter_taps == WIENER_WIN_REDUCED) {
    highbd_convolve_add_src_vert_5tap_rvv(im_block, im_stride, dst, dst_stride,
                                          w, h, yf, vert_round_val, vert_shift,
                                          res_max_val);
  } else {
    highbd_convolve_add_src_vert_7tap_rvv(im_block, im_stride, dst, dst_stride,
                                          w, h, yf, vert_round_val, vert_shift,
                                          res_max_val);
  }
}
