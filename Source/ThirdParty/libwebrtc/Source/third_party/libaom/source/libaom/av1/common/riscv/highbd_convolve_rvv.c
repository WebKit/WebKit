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

#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/riscv/mem_rvv.h"
#include "aom_ports/mem.h"
#include "av1/common/filter.h"
#include "av1/common/riscv/convolve_rvv.h"

static inline vuint16mf2_t highbd_convolve6_4_y_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const int16_t *filter, const uint16_t max, size_t vl) {
  // Values at indices 0 and 7 of y_filter are zero.
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[1], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s5, vl);

  // Add rounding constant and shift
  sum = __riscv_vadd_vx_i32m1(sum, 1 << (COMPOUND_ROUND1_BITS - 1), vl);

  // Narrow result to 16-bit with rounding and saturation
  vint16mf2_t res = __riscv_vnsra_wx_i16mf2(sum, COMPOUND_ROUND1_BITS, vl);

  // Clamp result to max value
  vuint16mf2_t d0 =
      __riscv_vreinterpret_v_i16mf2_u16mf2(__riscv_vmax_vx_i16mf2(res, 0, vl));
  return __riscv_vminu_vx_u16mf2(d0, max, vl);
}

static inline vuint16m1_t highbd_convolve6_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, const uint16_t max, size_t vl) {
  // Values at indices 0 and 7 of y_filter are zero.
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[1], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s5, vl);

  // Add rounding constant and shift
  sum = __riscv_vadd_vx_i32m2(sum, 1 << (COMPOUND_ROUND1_BITS - 1), vl);

  // Narrow result to 16-bit with rounding and saturation
  vint16m1_t res = __riscv_vnsra_wx_i16m1(sum, COMPOUND_ROUND1_BITS, vl);

  // Clamp result to max value
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res, 0, vl));
  return __riscv_vminu_vx_u16m1(d0, max, vl);
}

static inline void highbd_convolve_y_sr_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter, int bd) {
  const uint16_t max = (1 << bd) - 1;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)(src_ptr + src_stride);
    uint16_t *d = dst_ptr;

    // Load initial 5 rows of data
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    s += 5 * src_stride;

    do {
      // Load next 4 rows of data
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

      // Perform 6-tap convolution for 4 rows
      vuint16mf2_t d0 =
          highbd_convolve6_4_y_rvv(s0, s1, s2, s3, s4, s5, y_filter, max, vl);
      vuint16mf2_t d1 =
          highbd_convolve6_4_y_rvv(s1, s2, s3, s4, s5, s6, y_filter, max, vl);
      vuint16mf2_t d2 =
          highbd_convolve6_4_y_rvv(s2, s3, s4, s5, s6, s7, y_filter, max, vl);
      vuint16mf2_t d3 =
          highbd_convolve6_4_y_rvv(s3, s4, s5, s6, s7, s8, y_filter, max, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update source pointers for next iteration
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)(src_ptr + src_stride);
      uint16_t *d = dst_ptr;

      // Load initial 5 rows of data
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        // Load next 4 rows of data
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        // Perform 6-tap convolution for 4 rows
        vuint16m1_t d0 =
            highbd_convolve6_8_y_rvv(s0, s1, s2, s3, s4, s5, y_filter, max, vl);
        vuint16m1_t d1 =
            highbd_convolve6_8_y_rvv(s1, s2, s3, s4, s5, s6, y_filter, max, vl);
        vuint16m1_t d2 =
            highbd_convolve6_8_y_rvv(s2, s3, s4, s5, s6, s7, y_filter, max, vl);
        vuint16m1_t d3 =
            highbd_convolve6_8_y_rvv(s3, s4, s5, s6, s7, s8, y_filter, max, vl);

        // Store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Update source pointers for next iteration
        s0 = __riscv_vmv_v_v_i16m1(s4, vl);
        s1 = __riscv_vmv_v_v_i16m1(s5, vl);
        s2 = __riscv_vmv_v_v_i16m1(s6, vl);
        s3 = __riscv_vmv_v_v_i16m1(s7, vl);
        s4 = __riscv_vmv_v_v_i16m1(s8, vl);

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

static inline vuint16mf2_t highbd_convolve8_4_y_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const int16_t *filter,
    const uint16_t max, size_t vl) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[7], s7, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m1_t d0 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16mf2(res, max, vl);
}

static inline vuint16m1_t highbd_convolve8_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const uint16_t max, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16m1(res, max, vl);
}

static inline void highbd_convolve_y_sr_8tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter, int bd) {
  const uint16_t max = (1 << bd) - 1;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    // Load initial 7 rows of data
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    s += 7 * src_stride;

    do {
      // Load next 4 rows of data
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

      // Perform 8-tap convolution for 4 rows
      vuint16mf2_t d0 = highbd_convolve8_4_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                                 y_filter, max, vl);
      vuint16mf2_t d1 = highbd_convolve8_4_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                                 y_filter, max, vl);
      vuint16mf2_t d2 = highbd_convolve8_4_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                                 y_filter, max, vl);
      vuint16mf2_t d3 = highbd_convolve8_4_y_rvv(s3, s4, s5, s6, s7, s8, s9,
                                                 s10, y_filter, max, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update source pointers for next iteration
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
      s6 = __riscv_vmv_v_v_i16mf2(s10, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      // Load initial 7 rows of data
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        // Load next 4 rows of data
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        // Perform 8-tap convolution for 4 rows
        vuint16m1_t d0 = highbd_convolve8_8_y_rvv(s0, s1, s2, s3, s4, s5, s6,
                                                  s7, y_filter, max, vl);
        vuint16m1_t d1 = highbd_convolve8_8_y_rvv(s1, s2, s3, s4, s5, s6, s7,
                                                  s8, y_filter, max, vl);
        vuint16m1_t d2 = highbd_convolve8_8_y_rvv(s2, s3, s4, s5, s6, s7, s8,
                                                  s9, y_filter, max, vl);
        vuint16m1_t d3 = highbd_convolve8_8_y_rvv(s3, s4, s5, s6, s7, s8, s9,
                                                  s10, y_filter, max, vl);

        // Store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Update source pointers for next iteration
        s0 = __riscv_vmv_v_v_i16m1(s4, vl);
        s1 = __riscv_vmv_v_v_i16m1(s5, vl);
        s2 = __riscv_vmv_v_v_i16m1(s6, vl);
        s3 = __riscv_vmv_v_v_i16m1(s7, vl);
        s4 = __riscv_vmv_v_v_i16m1(s8, vl);
        s5 = __riscv_vmv_v_v_i16m1(s9, vl);
        s6 = __riscv_vmv_v_v_i16m1(s10, vl);

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

static inline vuint16mf2_t highbd_convolve12_4_y_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const vint16mf2_t s8,
    const vint16mf2_t s9, const vint16mf2_t s10, const vint16mf2_t s11,
    const int16_t *filter, const uint16_t max, size_t vl) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[11], s11, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m1_t d0 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16mf2(res, max, vl);
}

static inline vuint16m1_t highbd_convolve12_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *filter, const uint16_t max, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[11], s11, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16m1(res, max, vl);
}

static inline void highbd_convolve_y_sr_12tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter, int bd) {
  const uint16_t max = (1 << bd) - 1;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    // Load initial 11 rows of data
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &s10, vl);
    s += 11 * src_stride;

    do {
      // Load next 4 rows of data
      vint16mf2_t s11, s12, s13, s14;
      load_s16_4x4(s, src_stride, &s11, &s12, &s13, &s14, vl);

      // Perform 12-tap convolution for 4 rows
      vuint16mf2_t d0 = highbd_convolve12_4_y_rvv(
          s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter, max, vl);
      vuint16mf2_t d1 = highbd_convolve12_4_y_rvv(
          s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter, max, vl);
      vuint16mf2_t d2 =
          highbd_convolve12_4_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                    s12, s13, y_filter, max, vl);
      vuint16mf2_t d3 =
          highbd_convolve12_4_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                    s13, s14, y_filter, max, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update source pointers for next iteration
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
      s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
      s7 = __riscv_vmv_v_v_i16mf2(s11, vl);
      s8 = __riscv_vmv_v_v_i16mf2(s12, vl);
      s9 = __riscv_vmv_v_v_i16mf2(s13, vl);
      s10 = __riscv_vmv_v_v_i16mf2(s14, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      // Load initial 11 rows of data
      vint16m1_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10, vl);
      s += 11 * src_stride;

      do {
        // Load next 4 rows of data
        vint16m1_t s11, s12, s13, s14;
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14, vl);

        // Perform 12-tap convolution for 4 rows
        vuint16m1_t d0 =
            highbd_convolve12_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                      s10, s11, y_filter, max, vl);
        vuint16m1_t d1 =
            highbd_convolve12_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                      s11, s12, y_filter, max, vl);
        vuint16m1_t d2 =
            highbd_convolve12_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                      s12, s13, y_filter, max, vl);
        vuint16m1_t d3 =
            highbd_convolve12_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                      s13, s14, y_filter, max, vl);

        // Store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // Update source pointers for next iteration
        s0 = __riscv_vmv_v_v_i16m1(s4, vl);
        s1 = __riscv_vmv_v_v_i16m1(s5, vl);
        s2 = __riscv_vmv_v_v_i16m1(s6, vl);
        s3 = __riscv_vmv_v_v_i16m1(s7, vl);
        s4 = __riscv_vmv_v_v_i16m1(s8, vl);
        s5 = __riscv_vmv_v_v_i16m1(s9, vl);
        s6 = __riscv_vmv_v_v_i16m1(s10, vl);
        s7 = __riscv_vmv_v_v_i16m1(s11, vl);
        s8 = __riscv_vmv_v_v_i16m1(s12, vl);
        s9 = __riscv_vmv_v_v_i16m1(s13, vl);
        s10 = __riscv_vmv_v_v_i16m1(s14, vl);

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

void av1_highbd_convolve_y_sr_rvv(const uint16_t *src, int src_stride,
                                  uint16_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_y_qn, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_y, subpel_y_qn, bd);
    return;
  }

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_offset * src_stride;

  if (y_filter_taps > 8) {
    highbd_convolve_y_sr_12tap_rvv(src, src_stride, dst, dst_stride, w, h,
                                   y_filter_ptr, bd);
    return;
  }
  if (y_filter_taps < 8) {
    highbd_convolve_y_sr_6tap_rvv(src, src_stride, dst, dst_stride, w, h,
                                  y_filter_ptr, bd);
    return;
  }

  highbd_convolve_y_sr_8tap_rvv(src, src_stride, dst, dst_stride, w, h,
                                y_filter_ptr, bd);
}

static inline vuint16m1_t highbd_convolve6_8_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, const int32_t offset, const uint16_t max,
    size_t vl) {
  // Values at indices 0 and 7 of y_filter are zero.
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[1], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s5, vl);

  // Add rounding constant and offset
  sum = __riscv_vadd_vx_i32m2(sum, (1 << (FILTER_BITS - 1)) + offset, vl);

  // Narrow result to 16-bit with rounding and saturation
  vint16m1_t res = __riscv_vnsra_wx_i16m1(sum, FILTER_BITS, vl);

  // Clamp result to max value
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(res, 0, vl));
  return __riscv_vminu_vx_u16m1(d0, max, vl);
}

static inline void highbd_convolve_x_sr_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter, ConvolveParams *conv_params,
    int bd) {
  const uint16_t max = (1 << bd) - 1;
  // This shim allows to do only one rounding shift instead of two.
  const int32_t offset = 1 << (conv_params->round_0 - 1);

  int height = h;
  size_t vl = __riscv_vsetvl_e16m1(w);

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      vint16m1_t s00, s01, s02, s03, s04, s05;
      vint16m1_t s10, s11, s12, s13, s14, s15;
      vint16m1_t s20, s21, s22, s23, s24, s25;
      vint16m1_t s30, s31, s32, s33, s34, s35;

      // Load 6 elements for each of 4 rows
      load_s16_8x6(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                   vl);
      load_s16_8x6(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                   vl);
      load_s16_8x6(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                   vl);
      load_s16_8x6(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                   vl);

      // Perform convolution
      vuint16m1_t d0 = highbd_convolve6_8_x_rvv(s00, s01, s02, s03, s04, s05,
                                                x_filter, offset, max, vl);
      vuint16m1_t d1 = highbd_convolve6_8_x_rvv(s10, s11, s12, s13, s14, s15,
                                                x_filter, offset, max, vl);
      vuint16m1_t d2 = highbd_convolve6_8_x_rvv(s20, s21, s22, s23, s24, s25,
                                                x_filter, offset, max, vl);
      vuint16m1_t d3 = highbd_convolve6_8_x_rvv(s30, s31, s32, s33, s34, s35,
                                                x_filter, offset, max, vl);

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

static inline vuint16mf2_t highbd_convolve4_4_x_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const int16_t *filter, const int32_t offset,
    const uint16_t max, size_t vl) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);

  // Add rounding constant and offset
  sum = __riscv_vadd_vx_i32m1(sum, (1 << (FILTER_BITS - 1)) + offset, vl);

  // Narrow result to 16-bit with rounding and saturation
  vint16mf2_t res = __riscv_vnsra_wx_i16mf2(sum, FILTER_BITS, vl);

  // Clamp result to max value
  vuint16mf2_t d0 =
      __riscv_vreinterpret_v_i16mf2_u16mf2(__riscv_vmax_vx_i16mf2(res, 0, vl));
  return __riscv_vminu_vx_u16mf2(d0, max, vl);
}

static inline vuint16m1_t highbd_convolve8_8_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const int32_t offset, const uint16_t max, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);

  sum = __riscv_vwadd_wx_i32m2(sum, offset, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(d0, FILTER_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16m1(res, max, vl);
}

static inline void highbd_convolve_x_sr_rvv(const uint16_t *src_ptr,
                                            int src_stride, uint16_t *dst_ptr,
                                            int dst_stride, int w, int h,
                                            const int16_t *x_filter,
                                            ConvolveParams *conv_params,
                                            int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int32_t offset = 1 << (conv_params->round_0 - 1);
  const uint16_t max = (1 << bd) - 1;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16_t *s = (const int16_t *)(src_ptr + 2);
    uint16_t *d = dst_ptr;
    const int16_t *x_filter_ptr = x_filter + 2;

    do {
      vint16mf2_t s00, s01, s02, s03;
      vint16mf2_t s10, s11, s12, s13;
      vint16mf2_t s20, s21, s22, s23;
      vint16mf2_t s30, s31, s32, s33;

      // Load pixels from each of 4 rows
      load_s16_4x4(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, vl);
      load_s16_4x4(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, vl);
      load_s16_4x4(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, vl);
      load_s16_4x4(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, vl);

      // Perform convolution for 4 rows
      vuint16mf2_t d0 = highbd_convolve4_4_x_rvv(s00, s01, s02, s03,
                                                 x_filter_ptr, offset, max, vl);
      vuint16mf2_t d1 = highbd_convolve4_4_x_rvv(s10, s11, s12, s13,
                                                 x_filter_ptr, offset, max, vl);
      vuint16mf2_t d2 = highbd_convolve4_4_x_rvv(s20, s21, s22, s23,
                                                 x_filter_ptr, offset, max, vl);
      vuint16mf2_t d3 = highbd_convolve4_4_x_rvv(s30, s31, s32, s33,
                                                 x_filter_ptr, offset, max, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    int height = h;
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

        // Perform convolution
        vuint16m1_t d0 = highbd_convolve8_8_x_rvv(
            s00, s01, s02, s03, s04, s05, s06, s07, x_filter, offset, max, vl);
        vuint16m1_t d1 = highbd_convolve8_8_x_rvv(
            s10, s11, s12, s13, s14, s15, s16, s17, x_filter, offset, max, vl);
        vuint16m1_t d2 = highbd_convolve8_8_x_rvv(
            s20, s21, s22, s23, s24, s25, s26, s27, x_filter, offset, max, vl);
        vuint16m1_t d3 = highbd_convolve8_8_x_rvv(
            s30, s31, s32, s33, s34, s35, s36, s37, x_filter, offset, max, vl);

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

static inline vuint16mf2_t highbd_convolve12_4_x_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const vint16mf2_t s8,
    const vint16mf2_t s9, const vint16mf2_t s10, const vint16mf2_t s11,
    const int16_t *filter, const int32_t offset, const uint16_t max,
    size_t vl) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[11], s11, vl);
  sum = __riscv_vwadd_wx_i32m1(sum, offset, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m1_t d0 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, vl));
  vuint16mf2_t res =
      __riscv_vnclipu_wx_u16mf2(d0, FILTER_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16mf2(res, max, vl);
}

static inline vuint16m1_t highbd_convolve12_8_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *filter, const int32_t offset, const uint16_t max,
    size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[11], s11, vl);
  sum = __riscv_vwadd_wx_i32m2(sum, offset, vl);

  // Convert to unsigned 16-bit with saturation
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));
  vuint16m1_t res =
      __riscv_vnclipu_wx_u16m1(d0, FILTER_BITS, __RISCV_VXRM_RNU, vl);

  // Clamp to max
  return __riscv_vminu_vx_u16m1(res, max, vl);
}

static inline void highbd_convolve_x_sr_12tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter, ConvolveParams *conv_params,
    int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int32_t offset = 1 << (conv_params->round_0 - 1);
  const uint16_t max = (1 << bd) - 1;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      vint16mf2_t s00, s01, s02, s03, s04, s05, s06, s07, s08, s09, s010, s011;
      vint16mf2_t s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s110, s111;
      vint16mf2_t s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s210, s211;
      vint16mf2_t s30, s31, s32, s33, s34, s35, s36, s37, s38, s39, s310, s311;

      // Load elements for each of 4 rows
      load_s16_4x12(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                    &s06, &s07, &s08, &s09, &s010, &s011, vl);
      load_s16_4x12(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                    &s16, &s17, &s18, &s19, &s110, &s111, vl);
      load_s16_4x12(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                    &s26, &s27, &s28, &s29, &s210, &s211, vl);
      load_s16_4x12(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                    &s36, &s37, &s38, &s39, &s310, &s311, vl);

      // Perform convolution
      vuint16mf2_t d0 =
          highbd_convolve12_4_x_rvv(s00, s01, s02, s03, s04, s05, s06, s07, s08,
                                    s09, s010, s011, x_filter, offset, max, vl);
      vuint16mf2_t d1 =
          highbd_convolve12_4_x_rvv(s10, s11, s12, s13, s14, s15, s16, s17, s18,
                                    s19, s110, s111, x_filter, offset, max, vl);
      vuint16mf2_t d2 =
          highbd_convolve12_4_x_rvv(s20, s21, s22, s23, s24, s25, s26, s27, s28,
                                    s29, s210, s211, x_filter, offset, max, vl);
      vuint16mf2_t d3 =
          highbd_convolve12_4_x_rvv(s30, s31, s32, s33, s34, s35, s36, s37, s38,
                                    s39, s310, s311, x_filter, offset, max, vl);

      // Store results
      store_u16_4x4(d, dst_stride, d0, d1, d2, d3, vl);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    int height = h;
    do {
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;
      int width = w;

      do {
        vint16m1_t s00, s01, s02, s03, s04, s05, s06, s07, s08, s09, s010, s011;
        vint16m1_t s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s110, s111;
        vint16m1_t s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s210, s211;
        vint16m1_t s30, s31, s32, s33, s34, s35, s36, s37, s38, s39, s310, s311;

        // Load elements for each of 4 rows
        load_s16_8x12(s + 0 * src_stride, 1, &s00, &s01, &s02, &s03, &s04, &s05,
                      &s06, &s07, &s08, &s09, &s010, &s011, vl);
        load_s16_8x12(s + 1 * src_stride, 1, &s10, &s11, &s12, &s13, &s14, &s15,
                      &s16, &s17, &s18, &s19, &s110, &s111, vl);
        load_s16_8x12(s + 2 * src_stride, 1, &s20, &s21, &s22, &s23, &s24, &s25,
                      &s26, &s27, &s28, &s29, &s210, &s211, vl);
        load_s16_8x12(s + 3 * src_stride, 1, &s30, &s31, &s32, &s33, &s34, &s35,
                      &s36, &s37, &s38, &s39, &s310, &s311, vl);

        // Perform convolution
        vuint16m1_t d0 = highbd_convolve12_8_x_rvv(
            s00, s01, s02, s03, s04, s05, s06, s07, s08, s09, s010, s011,
            x_filter, offset, max, vl);
        vuint16m1_t d1 = highbd_convolve12_8_x_rvv(
            s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s110, s111,
            x_filter, offset, max, vl);
        vuint16m1_t d2 = highbd_convolve12_8_x_rvv(
            s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s210, s211,
            x_filter, offset, max, vl);
        vuint16m1_t d3 = highbd_convolve12_8_x_rvv(
            s30, s31, s32, s33, s34, s35, s36, s37, s38, s39, s310, s311,
            x_filter, offset, max, vl);

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

void av1_highbd_convolve_x_sr_rvv(const uint16_t *src, int src_stride,
                                  uint16_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const int subpel_x_qn,
                                  ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_x, subpel_x_qn, conv_params, bd);
    return;
  }
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_offset;

  if (x_filter_taps > 8) {
    highbd_convolve_x_sr_12tap_rvv(src, src_stride, dst, dst_stride, w, h,
                                   x_filter_ptr, conv_params, bd);
    return;
  }
  if (x_filter_taps <= 6 && w != 4) {
    highbd_convolve_x_sr_6tap_rvv(src + 1, src_stride, dst, dst_stride, w, h,
                                  x_filter_ptr, conv_params, bd);
    return;
  }

  highbd_convolve_x_sr_rvv(src, src_stride, dst, dst_stride, w, h, x_filter_ptr,
                           conv_params, bd);
}

// store_strided_u16_4xN
static inline void store_strided_u16_4xN(uint16_t *addr, vuint16m1_t vdst,
                                         ptrdiff_t stride, size_t vl) {
  __riscv_vse16_v_u16m1(addr, vdst, vl >> 1);
  vdst = __riscv_vslidedown_vx_u16m1(vdst, vl >> 1, vl);
  __riscv_vse16_v_u16m1(addr + stride, vdst, vl >> 1);
}

static inline vuint16m1_t highbd_convolve12_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *y_filter, const int32_t offset, const int32_t shift,
    const uint16_t max, size_t vl) {
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
  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  vint16m1_t i16_sum = __riscv_vnsra_wx_i16m1(sum, shift, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), max, vl);
  return __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum);
}

static inline void highbd_convolve_2d_sr_vert_12tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    const int bd, const int offset, size_t vl) {
  const int32_t shift_s32 = conv_params->round_1;
  const int32_t offset_s32 = offset;
  const uint16_t max_u16 = (1 << bd) - 1;

  if (w == 4) {
    int16_t *s = (int16_t *)src_ptr;
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s4 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s5 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s6 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s7 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s8 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s9 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;

    do {
      vint16m1_t s10 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s11 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s12 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s13 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;

      vuint16m1_t d0 = highbd_convolve12_2d_v_rvv(
          s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_ptr,
          offset_s32, shift_s32, max_u16, vl);
      vuint16m1_t d1 = highbd_convolve12_2d_v_rvv(
          s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_ptr,
          offset_s32, shift_s32, max_u16, vl);

      store_strided_u16_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;
      store_strided_u16_4xN(dst_ptr, d1, dst_stride, vl);
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
      int16_t *s = (int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

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

        vuint16m1_t d0 = highbd_convolve12_2d_v_rvv(
            s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, y_filter_ptr,
            offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d1 = highbd_convolve12_2d_v_rvv(
            s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, y_filter_ptr,
            offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d2 = highbd_convolve12_2d_v_rvv(
            s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, y_filter_ptr,
            offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d3 = highbd_convolve12_2d_v_rvv(
            s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, y_filter_ptr,
            offset_s32, shift_s32, max_u16, vl);

        __riscv_vse16_v_u16m1(d, d0, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d1, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d2, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d3, vl);
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

static inline vuint16m1_t highbd_convolve8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *y_filter,
    const int32_t offset, const int32_t shift, const uint16_t max, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[7], s7, vl);
  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  vint16m1_t i16_sum = __riscv_vnsra_wx_i16m1(sum, shift, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), max, vl);
  return __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum);
}

static inline void highbd_convolve_2d_sr_vert_8tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    int bd, const int offset, size_t vl) {
  const int32_t shift_s32 = conv_params->round_1;
  const int32_t offset_s32 = offset;
  const uint16_t max_u16 = (1 << bd) - 1;

  if (w <= 4) {
    int16_t *s = (int16_t *)src_ptr;
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s4 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s5 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;

    do {
      vint16m1_t s6 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s7 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;

      vuint16m1_t d0 = highbd_convolve8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                                 y_filter_ptr, offset_s32,
                                                 shift_s32, max_u16, vl);

      store_strided_u16_4xN(dst_ptr, d0, dst_stride, vl);
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
      int16_t *s = (int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

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
        vuint16m1_t d0 = highbd_convolve8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6,
                                                   s7, y_filter_ptr, offset_s32,
                                                   shift_s32, max_u16, vl);
        __riscv_vse16_v_u16m1(d, d0, vl);

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

static inline vuint16m1_t highbd_convolve6_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *y_filter, const int32_t offset, const int32_t shift,
    const uint16_t max, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, y_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, y_filter[5], s5, vl);
  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  vint16m1_t i16_sum = __riscv_vnsra_wx_i16m1(sum, shift, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), max, vl);
  return __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum);
}

static inline void highbd_convolve_2d_sr_vert_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, ConvolveParams *conv_params,
    int bd, const int offset, size_t vl) {
  const int32_t shift_s32 = conv_params->round_1;
  const int32_t offset_s32 = offset;
  const uint16_t max_u16 = (1 << bd) - 1;
  const int16_t *yfilter_6tap = y_filter_ptr + 1;

  if (w == 4) {
    int16_t *s = (int16_t *)src_ptr;
    vl = vl << 1;

    vint16m1_t s0 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s1 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s2 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;
    vint16m1_t s3 = load_strided_i16_4xN(s, src_stride, vl);
    s += src_stride;

    do {
      vint16m1_t s4 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s5 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s6 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;
      vint16m1_t s7 = load_strided_i16_4xN(s, src_stride, vl);
      s += src_stride;

      vuint16m1_t d0 =
          highbd_convolve6_2d_v_rvv(s0, s1, s2, s3, s4, s5, yfilter_6tap,
                                    offset_s32, shift_s32, max_u16, vl);
      vuint16m1_t d1 =
          highbd_convolve6_2d_v_rvv(s2, s3, s4, s5, s6, s7, yfilter_6tap,
                                    offset_s32, shift_s32, max_u16, vl);

      store_strided_u16_4xN(dst_ptr, d0, dst_stride, vl);
      dst_ptr += dst_stride << 1;
      store_strided_u16_4xN(dst_ptr, d1, dst_stride, vl);
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
      int16_t *s = (int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

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

        vuint16m1_t d0 =
            highbd_convolve6_2d_v_rvv(s0, s1, s2, s3, s4, s5, yfilter_6tap,
                                      offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d1 =
            highbd_convolve6_2d_v_rvv(s1, s2, s3, s4, s5, s6, yfilter_6tap,
                                      offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d2 =
            highbd_convolve6_2d_v_rvv(s2, s3, s4, s5, s6, s7, yfilter_6tap,
                                      offset_s32, shift_s32, max_u16, vl);
        vuint16m1_t d3 =
            highbd_convolve6_2d_v_rvv(s3, s4, s5, s6, s7, s8, yfilter_6tap,
                                      offset_s32, shift_s32, max_u16, vl);

        __riscv_vse16_v_u16m1(d, d0, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d1, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d2, vl);
        d += dst_stride;
        __riscv_vse16_v_u16m1(d, d3, vl);
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

static inline vint16m1_t highbd_convolve12_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *x_filter, const int32_t offset, const int32_t shift,
    size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, x_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[7], s7, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[8], s8, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[9], s9, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[10], s10, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[11], s11, vl);

  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  return __riscv_vnclip_wx_i16m1(sum, shift, __RISCV_VXRM_RNU, vl);
}

static inline void highbd_convolve_2d_sr_horiz_12tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset, size_t vl) {
  assert(h >= 5);
  const int32_t shift_s32 = conv_params->round_0;
  const int32_t offset_s32 = offset;

  if (w == 4) {
    const int16_t *s = (int16_t *)src_ptr;
    int16_t *d = (int16_t *)dst_ptr;

    do {
      vint16m1_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11;

      load_s16_8x12(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8, &t9,
                    &t10, &t11, vl);

      vint16m1_t d0 = highbd_convolve12_8_2d_h_rvv(
          t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, x_filter_ptr,
          offset_s32, shift_s32, vl);

      __riscv_vse16_v_i16m1(d, d0, vl);

      s += src_stride;
      d += dst_stride;

    } while (--h != 0);
  } else {
    int height = h;

    do {
      const int16_t *s = (int16_t *)src_ptr;
      int16_t *d = (int16_t *)dst_ptr;
      int width = w;

      do {
        vint16m1_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11;

        load_s16_8x12(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8, &t9,
                      &t10, &t11, vl);

        vint16m1_t d0 = highbd_convolve12_8_2d_h_rvv(
            t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, x_filter_ptr,
            offset_s32, shift_s32, vl);

        __riscv_vse16_v_i16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

static inline vint16m1_t highbd_convolve8_4_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *x_filter, const int32_t offset,
    const int32_t shift, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, x_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[3], s3, vl);

  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  return __riscv_vnclip_wx_i16m1(sum, shift, __RISCV_VXRM_RNU, vl);
}

static inline vint16m1_t highbd_convolve8_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *x_filter,
    const int32_t offset, const int32_t shift, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, x_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[7], s7, vl);

  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  return __riscv_vnclip_wx_i16m1(sum, shift, __RISCV_VXRM_RNU, vl);
}

static inline void highbd_convolve_2d_sr_horiz_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset, size_t vl) {
  assert(h >= 5);
  const int32_t shift_s32 = conv_params->round_0;
  const int32_t offset_s32 = offset;

  if (w == 4) {
    const int16_t *x_filter = (x_filter_ptr + 2);
    const int16_t *s = (int16_t *)(src_ptr + 1);
    int16_t *d = (int16_t *)dst_ptr;

    do {
      vint16m1_t t0, t1, t2, t3;

      load_s16_8x4(s, 1, &t0, &t1, &t2, &t3, vl);

      vint16m1_t d0 = highbd_convolve8_4_2d_h_rvv(t0, t1, t2, t3, x_filter,
                                                  offset_s32, shift_s32, vl);

      __riscv_vse16_v_i16m1(d, d0, vl);

      s += src_stride;
      d += dst_stride;
    } while (--h != 0);
  } else {
    do {
      const int16_t *s = (int16_t *)src_ptr;
      int16_t *d = (int16_t *)dst_ptr;
      int width = w;

      do {
        vint16m1_t t0, t1, t2, t3, t4, t5, t6, t7;

        load_s16_8x8(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, vl);

        vint16m1_t d0 = highbd_convolve8_8_2d_h_rvv(t0, t1, t2, t3, t4, t5, t6,
                                                    t7, x_filter_ptr,
                                                    offset_s32, shift_s32, vl);
        __riscv_vse16_v_i16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  }
}

static inline vint16m1_t highbd_convolve6_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *x_filter, const int32_t offset, const int32_t shift,
    size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, x_filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, x_filter[5], s5, vl);

  sum = __riscv_vadd_vx_i32m2(sum, offset, vl);

  return __riscv_vnclip_wx_i16m1(sum, shift, __RISCV_VXRM_RNU, vl);
}

static inline void highbd_convolve_2d_sr_horiz_6tap_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, ConvolveParams *conv_params,
    const int offset, size_t vl) {
  assert(h >= 5);
  const int32_t shift_s32 = conv_params->round_0;
  const int32_t offset_s32 = offset;
  const int16_t *x_filter = (x_filter_ptr + 1);

  do {
    const int16_t *s = (int16_t *)src_ptr;
    int16_t *d = (int16_t *)dst_ptr;
    int width = w;

    do {
      vint16m1_t t0, t1, t2, t3, t4, t5;

      load_s16_8x6(s, 1, &t0, &t1, &t2, &t3, &t4, &t5, vl);

      vint16m1_t d0 = highbd_convolve6_8_2d_h_rvv(
          t0, t1, t2, t3, t4, t5, x_filter, offset_s32, shift_s32, vl);

      __riscv_vse16_v_i16m1(d, d0, vl);

      s += vl;
      d += vl;
      width -= vl;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

void av1_highbd_convolve_2d_sr_rvv(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_x_qn, const int subpel_y_qn,
                                   ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                                filter_params_x, filter_params_y, subpel_x_qn,
                                subpel_y_qn, conv_params, bd);
    return;
  }
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_x_taps = x_filter_taps < 6 ? 6 : x_filter_taps;

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;
  const int x_offset_initial = (1 << (bd + FILTER_BITS - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  // The extra shim of (1 << (conv_params->round_1 - 1)) allows us to do a
  // simple shift left instead of a rounding saturating shift left.
  const int y_offset =
      (1 << (conv_params->round_1 - 1)) - (1 << (y_offset_bits - 1));

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  size_t vl = __riscv_vsetvl_e16m1(w);

  if (x_filter_taps > 8) {
    highbd_convolve_2d_sr_horiz_12tap_rvv(src_ptr, src_stride, im_block,
                                          im_stride, w, im_h, x_filter_ptr,
                                          conv_params, x_offset_initial, vl);

    highbd_convolve_2d_sr_vert_12tap_rvv(im_block, im_stride, dst, dst_stride,
                                         w, h, y_filter_ptr, conv_params, bd,
                                         y_offset, vl);
    return;
  }
  if (x_filter_taps <= 6 && w != 4) {
    highbd_convolve_2d_sr_horiz_6tap_rvv(src_ptr, src_stride, im_block,
                                         im_stride, w, im_h, x_filter_ptr,
                                         conv_params, x_offset_initial, vl);
  } else {
    highbd_convolve_2d_sr_horiz_rvv(src_ptr, src_stride, im_block, im_stride, w,
                                    im_h, x_filter_ptr, conv_params,
                                    x_offset_initial, vl);
  }

  if (y_filter_taps <= 6) {
    highbd_convolve_2d_sr_vert_6tap_rvv(im_block, im_stride, dst, dst_stride, w,
                                        h, y_filter_ptr, conv_params, bd,
                                        y_offset, vl);
  } else {
    highbd_convolve_2d_sr_vert_8tap_rvv(im_block, im_stride, dst, dst_stride, w,
                                        h, y_filter_ptr, conv_params, bd,
                                        y_offset, vl);
  }
}

// Filter used is [64, 64].
void av1_highbd_convolve_x_sr_intrabc_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd) {
  assert(subpel_x_qn == 8);
  assert(filter_params_x->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)conv_params;
  (void)bd;

  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w <= 4) {
    do {
      // Load
      vuint16mf2_t s0_0 = __riscv_vle16_v_u16mf2(src, vl);
      vuint16mf2_t s0_1 = __riscv_vle16_v_u16mf2(src + 1, vl);
      vuint16mf2_t s1_0 = __riscv_vle16_v_u16mf2(src + src_stride, vl);
      vuint16mf2_t s1_1 = __riscv_vle16_v_u16mf2(src + src_stride + 1, vl);

      // Average the values
      vuint16mf2_t d0 =
          __riscv_vaaddu_vv_u16mf2(s0_0, s0_1, __RISCV_VXRM_RNU, vl);
      vuint16mf2_t d1 =
          __riscv_vaaddu_vv_u16mf2(s1_0, s1_1, __RISCV_VXRM_RNU, vl);

      // Store
      __riscv_vse16_v_u16mf2(dst, d0, vl);
      __riscv_vse16_v_u16mf2(dst + dst_stride, d1, vl);

      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      const uint16_t *src_ptr = src;
      uint16_t *dst_ptr = dst;
      int width = w;

      do {
        // Load
        vuint16m1_t s0 = __riscv_vle16_v_u16m1(src_ptr, vl);
        vuint16m1_t s1 = __riscv_vle16_v_u16m1(src_ptr + 1, vl);
        vuint16m1_t s2 = __riscv_vle16_v_u16m1(src_ptr + src_stride, vl);
        vuint16m1_t s3 = __riscv_vle16_v_u16m1(src_ptr + src_stride + 1, vl);

        // Average the values
        vuint16m1_t d0 = __riscv_vaaddu_vv_u16m1(s0, s1, __RISCV_VXRM_RNU, vl);
        vuint16m1_t d1 = __riscv_vaaddu_vv_u16m1(s2, s3, __RISCV_VXRM_RNU, vl);

        // Store
        __riscv_vse16_v_u16m1(dst_ptr, d0, vl);
        __riscv_vse16_v_u16m1(dst_ptr + dst_stride, d1, vl);

        src_ptr += vl;
        dst_ptr += vl;
        width -= vl;
      } while (width > 0);
      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  }
}

// Filter used is [64, 64].
void av1_highbd_convolve_y_sr_intrabc_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    int bd) {
  assert(subpel_y_qn == 8);
  assert(filter_params_y->taps == 2);
  (void)filter_params_y;
  (void)subpel_y_qn;
  (void)bd;

  size_t vl = __riscv_vsetvl_e16m1(w);
  if (w <= 4) {
    vuint16mf2_t s0 = __riscv_vle16_v_u16mf2(src, vl);

    do {
      vuint16mf2_t s1 = __riscv_vle16_v_u16mf2(src + src_stride, vl);
      vuint16mf2_t s2 = __riscv_vle16_v_u16mf2(src + 2 * src_stride, vl);

      // Average the values
      vuint16mf2_t d0 = __riscv_vaaddu_vv_u16mf2(s0, s1, __RISCV_VXRM_RNU, vl);
      vuint16mf2_t d1 = __riscv_vaaddu_vv_u16mf2(s1, s2, __RISCV_VXRM_RNU, vl);

      // Store
      __riscv_vse16_v_u16mf2(dst, d0, vl);
      __riscv_vse16_v_u16mf2(dst + dst_stride, d1, vl);

      s0 = s2;
      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      const uint16_t *src_ptr = src;
      uint16_t *dst_ptr = dst;
      int height = h;

      vuint16m1_t s0 = __riscv_vle16_v_u16m1(src_ptr, vl);

      do {
        vuint16m1_t s1 = __riscv_vle16_v_u16m1(src_ptr + src_stride, vl);
        vuint16m1_t s2 = __riscv_vle16_v_u16m1(src_ptr + 2 * src_stride, vl);

        // Average the values
        vuint16m1_t d0 = __riscv_vaaddu_vv_u16m1(s0, s1, __RISCV_VXRM_RNU, vl);
        vuint16m1_t d1 = __riscv_vaaddu_vv_u16m1(s1, s2, __RISCV_VXRM_RNU, vl);

        // Store
        __riscv_vse16_v_u16m1(dst_ptr, d0, vl);
        __riscv_vse16_v_u16m1(dst_ptr + dst_stride, d1, vl);

        s0 = s2;
        src_ptr += src_stride << 1;
        dst_ptr += dst_stride << 1;
        height -= 2;
      } while (height > 0);
      src += vl;
      dst += vl;
      w -= vl;
    } while (w > 0);
  }
}

// Both horizontal and vertical passes use the same 2-tap filter: [64, 64].
void av1_highbd_convolve_2d_sr_intrabc_rvv(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd) {
  assert(subpel_x_qn == 8);
  assert(subpel_y_qn == 8);
  assert(filter_params_x->taps == 2 && filter_params_y->taps == 2);
  assert((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS);
  assert(w <= MAX_SB_SIZE && h <= MAX_SB_SIZE);
  (void)filter_params_x;
  (void)subpel_x_qn;
  (void)filter_params_y;
  (void)subpel_y_qn;
  (void)conv_params;
  (void)bd;

  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w <= 8) {
    // Horizontal filter.
    vuint16m1_t s0 = __riscv_vle16_v_u16m1(src, vl);
    vuint16m1_t s1 = __riscv_vle16_v_u16m1(src + 1, vl);
    src += src_stride;

    vuint16m1_t sum0 = __riscv_vadd_vv_u16m1(s0, s1, vl);

    do {
      vuint16m1_t s2 = __riscv_vle16_v_u16m1(src, vl);
      vuint16m1_t s3 = __riscv_vle16_v_u16m1(src + 1, vl);
      src += src_stride;
      vuint16m1_t s4 = __riscv_vle16_v_u16m1(src, vl);
      vuint16m1_t s5 = __riscv_vle16_v_u16m1(src + 1, vl);
      src += src_stride;

      vuint16m1_t sum1 = __riscv_vadd_vv_u16m1(s2, s3, vl);
      vuint16m1_t sum2 = __riscv_vadd_vv_u16m1(s4, s5, vl);

      // Vertical filter.
      vuint16m1_t d0 =
          __riscv_vadd_vx_u16m1(__riscv_vadd_vv_u16m1(sum0, sum1, vl), 2, vl);
      vuint16m1_t d1 =
          __riscv_vadd_vx_u16m1(__riscv_vadd_vv_u16m1(sum1, sum2, vl), 2, vl);

      d0 = __riscv_vsrl_vx_u16m1(d0, 2, vl);
      d1 = __riscv_vsrl_vx_u16m1(d1, 2, vl);

      __riscv_vse16_v_u16m1(dst, d0, vl);
      dst += dst_stride;
      __riscv_vse16_v_u16m1(dst, d1, vl);
      dst += dst_stride;

      sum0 = sum2;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      uint16_t *src_ptr = (uint16_t *)src;
      uint16_t *dst_ptr = dst;
      int height = h;

      // Horizontal filter.
      vuint16m1_t s0 = __riscv_vle16_v_u16m1(src_ptr, vl);
      vuint16m1_t s1 = __riscv_vle16_v_u16m1(src_ptr + 1, vl);
      src_ptr += src_stride;

      vuint16m1_t sum0 = __riscv_vadd_vv_u16m1(s0, s1, vl);

      do {
        vuint16m1_t s2 = __riscv_vle16_v_u16m1(src_ptr, vl);
        vuint16m1_t s3 = __riscv_vle16_v_u16m1(src_ptr + 1, vl);
        src_ptr += src_stride;
        vuint16m1_t s4 = __riscv_vle16_v_u16m1(src_ptr, vl);
        vuint16m1_t s5 = __riscv_vle16_v_u16m1(src_ptr + 1, vl);
        src_ptr += src_stride;

        vuint16m1_t sum1 = __riscv_vadd_vv_u16m1(s2, s3, vl);
        vuint16m1_t sum2 = __riscv_vadd_vv_u16m1(s4, s5, vl);

        // Vertical filter.
        vuint16m1_t d0 =
            __riscv_vadd_vx_u16m1(__riscv_vadd_vv_u16m1(sum0, sum1, vl), 2, vl);
        vuint16m1_t d1 =
            __riscv_vadd_vx_u16m1(__riscv_vadd_vv_u16m1(sum1, sum2, vl), 2, vl);

        d0 = __riscv_vsrl_vx_u16m1(d0, 2, vl);
        d1 = __riscv_vsrl_vx_u16m1(d1, 2, vl);

        __riscv_vse16_v_u16m1(dst_ptr, d0, vl);
        dst_ptr += dst_stride;
        __riscv_vse16_v_u16m1(dst_ptr, d1, vl);
        dst_ptr += dst_stride;

        sum0 = __riscv_vmv_v_v_u16m1(sum2, vl);
        height -= 2;
      } while (height != 0);

      src += vl;
      dst += vl;
      w -= vl;
    } while (w != 0);
  }
}
