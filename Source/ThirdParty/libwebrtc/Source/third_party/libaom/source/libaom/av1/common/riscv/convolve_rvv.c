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
#include <riscv_vector.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/riscv/convolve_rvv.h"

static inline vuint8mf2_t convolve12_4_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *filter, const int32_t horiz_const, size_t vl) {
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
  sum = __riscv_vwadd_wx_i32m2(sum, horiz_const + (1 << (FILTER_BITS - 1)), vl);

  // Round and shift
  vint16m1_t i16_sum = __riscv_vnsra_wx_i16m1(sum, FILTER_BITS, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  // Convert to 8-bit
  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_x_sr_12tap_rvv(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16_t *x_filter_ptr) {
  const int32_t horiz_const = (1 << (ROUND0_BITS - 1));
  size_t vl = __riscv_vsetvl_e16m1(w);

  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    do {
      // Load
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(s + 0, vl);
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(s + 1, vl);
      vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(s + 2, vl);
      vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(s + 3, vl);
      vuint8mf2_t t4 = __riscv_vle8_v_u8mf2(s + 4, vl);
      vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(s + 5, vl);
      vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + 6, vl);
      vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + 7, vl);
      vuint8mf2_t t8 = __riscv_vle8_v_u8mf2(s + 8, vl);
      vuint8mf2_t t9 = __riscv_vle8_v_u8mf2(s + 9, vl);
      vuint8mf2_t t10 = __riscv_vle8_v_u8mf2(s + 10, vl);
      vuint8mf2_t t11 = __riscv_vle8_v_u8mf2(s + 11, vl);

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
      vint16m1_t s8 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));
      vint16m1_t s9 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t9, vl));
      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t10, vl));
      vint16m1_t s11 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t11, vl));

      // Perform convolution
      vuint8mf2_t d0 =
          convolve12_4_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                             x_filter_ptr, horiz_const, vl);

      // Store result
      __riscv_vse8_v_u8mf2(d, d0, vl);

      s += vl;
      d += vl;
      width -= vl;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

static inline vuint8mf2_t convolve4_8_x_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *filter, const int16_t horiz_const,
    size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl);

  // Round and shift
  // We halved the filter values so -1 from right shift
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(sum, 0, vl));

  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
}

static inline void load_u8_8x4(const uint8_t *s, const ptrdiff_t p,
                               vuint8mf2_t *const s0, vuint8mf2_t *const s1,
                               vuint8mf2_t *const s2, vuint8mf2_t *const s3,
                               size_t vl) {
  *s0 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s1 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s2 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s3 = __riscv_vle8_v_u8mf2(s, vl);
}

static inline void store_u8_8x2(uint8_t *s, ptrdiff_t p, const vuint8mf2_t s0,
                                const vuint8mf2_t s1, size_t vl) {
  __riscv_vse8_v_u8mf2(s, s0, vl);
  s += p;
  __riscv_vse8_v_u8mf2(s, s1, vl);
}

static inline void convolve_x_sr_4tap_rvv(const uint8_t *src_ptr,
                                          int src_stride, uint8_t *dst_ptr,
                                          const int dst_stride, int w, int h,
                                          const int16_t *x_filter_ptr) {
  size_t vl;
  const int16_t horiz_const = (1 << ((ROUND0_BITS - 1) - 1));

  // All filter values are even, halve to reduce intermediate precision
  // requirements.
  int16_t filter[4];
  for (int i = 0; i < 4; i++) filter[i] = x_filter_ptr[2 + i] >> 1;

  if (w == 4) {
    vl = 8;
    do {
      // Load 8 pixels for each row
      vuint8mf2_t t00, t01, t02, t03;
      t00 = load_strided_u8_4xN((uint8_t *)src_ptr + 0, src_stride, vl);
      t01 = load_strided_u8_4xN((uint8_t *)src_ptr + 1, src_stride, vl);
      t02 = load_strided_u8_4xN((uint8_t *)src_ptr + 2, src_stride, vl);
      t03 = load_strided_u8_4xN((uint8_t *)src_ptr + 3, src_stride, vl);

      // Convert to 16-bit integers
      vint16m1_t s00, s01, s02, s03;
      s00 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t00, vl));
      s01 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t01, vl));
      s02 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t02, vl));
      s03 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t03, vl));

      // Perform convolution
      vuint8mf2_t d01 =
          convolve4_8_x_rvv(s00, s01, s02, s03, filter, horiz_const, vl);

      // Store result
      store_strided_u8_4xN(dst_ptr + 0 * dst_stride, d01, dst_stride, vl);

      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    vl = __riscv_vsetvl_e16m1(w);
    do {
      int width = w;
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      do {
        vuint8mf2_t t00, t01, t02, t03;
        vuint8mf2_t t10, t11, t12, t13;
        load_u8_8x4(s + 0 * src_stride, 1, &t00, &t01, &t02, &t03, vl);
        load_u8_8x4(s + 1 * src_stride, 1, &t10, &t11, &t12, &t13, vl);

        // Convert to 16-bit integers
        vint16m1_t s00, s01, s02, s03;
        s00 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t00, vl));
        s01 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t01, vl));
        s02 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t02, vl));
        s03 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t03, vl));

        vint16m1_t s10, s11, s12, s13;
        s10 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t10, vl));
        s11 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t11, vl));
        s12 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t12, vl));
        s13 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t13, vl));

        // Perform convolution
        vuint8mf2_t d0 =
            convolve4_8_x_rvv(s00, s01, s02, s03, filter, horiz_const, vl);
        vuint8mf2_t d1 =
            convolve4_8_x_rvv(s10, s11, s12, s13, filter, horiz_const, vl);

        // Store result
        store_u8_8x2(d, dst_stride, d0, d1, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width > 0);
      src_ptr += 2 * src_stride;
      dst_ptr += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  }
}

static inline vuint8mf2_t convolve8_8_x_rvv(
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

  // Round and shift
  // We halved the filter values so -1 from right shift
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(sum, 0, vl));

  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
}

static inline void load_u8_8x8(const uint8_t *s, int p, vuint8mf2_t *const s0,
                               vuint8mf2_t *const s1, vuint8mf2_t *const s2,
                               vuint8mf2_t *const s3, vuint8mf2_t *const s4,
                               vuint8mf2_t *const s5, vuint8mf2_t *const s6,
                               vuint8mf2_t *const s7, size_t vl) {
  *s0 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s1 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s2 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s3 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s4 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s5 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s6 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s7 = __riscv_vle8_v_u8mf2(s, vl);
}

static inline void convolve_x_sr_8tap_rvv(const uint8_t *src_ptr,
                                          int src_stride, uint8_t *dst_ptr,
                                          const int dst_stride, int w, int h,
                                          const int16_t *x_filter_ptr) {
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we will halve the filter values.
  const int32_t horiz_const = 1 << ((ROUND0_BITS - 1) - 1);

  // Filter values are even so halve to reduce precision requirements.
  int16_t filter[8];
  for (int i = 0; i < 8; i++) filter[i] = x_filter_ptr[i] >> 1;

  size_t vl = __riscv_vsetvl_e16m1(w);
  while (h-- != 0) {
    int width = w;
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;

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
      vuint8mf2_t d0 = convolve8_8_x_rvv(s0, s1, s2, s3, s4, s5, s6, s7, filter,
                                         horiz_const, vl);

      // Store result
      __riscv_vse8_v_u8mf2(d, d0, vl);

      s += vl;
      d += vl;
      width -= vl;
    } while (width > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

void av1_convolve_x_sr_rvv(const uint8_t *src, int src_stride, uint8_t *dst,
                           int dst_stride, int w, int h,
                           const InterpFilterParams *filter_params_x,
                           const int subpel_x_qn, ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                        subpel_x_qn, conv_params);
    return;
  }

  int filter_taps = get_filter_tap(filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_rvv = src - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  if (filter_taps > 8) {
    convolve_x_sr_12tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                            x_filter_ptr);
    return;
  }

  if (filter_taps <= 4) {
    convolve_x_sr_4tap_rvv(src_rvv + 2, src_stride, dst, dst_stride, w, h,
                           x_filter_ptr);
    return;
  }

  convolve_x_sr_8tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                         x_filter_ptr);
  return;
}

static inline void store_u8_8x4(uint8_t *s, int p, const vuint8mf2_t s0,
                                const vuint8mf2_t s1, const vuint8mf2_t s2,
                                const vuint8mf2_t s3, size_t vl) {
  __riscv_vse8_v_u8mf2(s, s0, vl);
  s += p;
  __riscv_vse8_v_u8mf2(s, s1, vl);
  s += p;
  __riscv_vse8_v_u8mf2(s, s2, vl);
  s += p;
  __riscv_vse8_v_u8mf2(s, s3, vl);
}

static inline vuint8mf2_t convolve4_8_y_rvv(const vint16m1_t s0,
                                            const vint16m1_t s1,
                                            const vint16m1_t s2,
                                            const vint16m1_t s3,
                                            const int16_t *filter, size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);

  // Round and shift
  // We halved the filter values so -1 from right shift
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(sum, 0, vl));

  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
}

static inline void convolve_y_sr_4tap_rvv(const uint8_t *src,
                                          const int src_stride, uint8_t *dst,
                                          const int dst_stride, int w, int h,
                                          const int16_t *filter_y) {
  const int16_t *filter = filter_y + 2;

  if (w == 4) {
    size_t vl = 8;

    // Load initial data
    vuint8mf2_t t01 =
        load_strided_u8_4xN((uint8_t *)src + 0 * src_stride, src_stride, vl);
    vuint8mf2_t t12 =
        load_strided_u8_4xN((uint8_t *)src + 1 * src_stride, src_stride, vl);

    // Convert to 16-bit
    vint16m1_t s01 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t01, vl));
    vint16m1_t s12 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t12, vl));

    src += 2 * src_stride;

    do {
      // Load next set of data
      vuint8mf2_t t23 =
          load_strided_u8_4xN((uint8_t *)src + 0 * src_stride, src_stride, vl);
      vuint8mf2_t t34 =
          load_strided_u8_4xN((uint8_t *)src + 1 * src_stride, src_stride, vl);
      vuint8mf2_t t45 =
          load_strided_u8_4xN((uint8_t *)src + 2 * src_stride, src_stride, vl);
      vuint8mf2_t t56 =
          load_strided_u8_4xN((uint8_t *)src + 3 * src_stride, src_stride, vl);

      // Convert to 16-bit
      vint16m1_t s23 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t23, vl));
      vint16m1_t s34 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t34, vl));
      vint16m1_t s45 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t45, vl));
      vint16m1_t s56 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t56, vl));

      // Perform convolution
      vuint8mf2_t d01 = convolve4_8_y_rvv(s01, s12, s23, s34, filter, vl);
      vuint8mf2_t d23 = convolve4_8_y_rvv(s23, s34, s45, s56, filter, vl);

      // Store results
      store_strided_u8_4xN(dst + 0 * dst_stride, d01, dst_stride, vl);
      store_strided_u8_4xN(dst + 2 * dst_stride, d23, dst_stride, vl);

      s01 = __riscv_vmv_v_v_i16m1(s45, vl);
      s12 = __riscv_vmv_v_v_i16m1(s56, vl);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    // Handle width > 4 case
    size_t vl = __riscv_vsetvl_e16m1(w);
    do {
      // Load initial 3 rows of data
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src + 0 * src_stride, vl);
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src + 1 * src_stride, vl);
      vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(src + 2 * src_stride, vl);

      // Convert to 16-bit
      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));

      int height = h;
      const uint8_t *s = src + 3 * src_stride;
      uint8_t *d = dst;

      do {
        // Load next 4 rows of data
        vuint8mf2_t t3;
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3, vl);

        // Convert to 16-bit
        vint16m1_t s3 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
        vint16m1_t s4 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
        vint16m1_t s5 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
        vint16m1_t s6 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));

        // Perform convolution
        vuint8mf2_t d0 = convolve4_8_y_rvv(s0, s1, s2, s3, filter, vl);
        vuint8mf2_t d1 = convolve4_8_y_rvv(s1, s2, s3, s4, filter, vl);
        vuint8mf2_t d2 = convolve4_8_y_rvv(s2, s3, s4, s5, filter, vl);
        vuint8mf2_t d3 = convolve4_8_y_rvv(s3, s4, s5, s6, filter, vl);

        // Store results
        store_u8_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        s0 = __riscv_vmv_v_v_i16m1(s4, vl);
        s1 = __riscv_vmv_v_v_i16m1(s5, vl);
        s2 = __riscv_vmv_v_v_i16m1(s6, vl);

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += vl;
      dst += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void load_u8_8x5(const uint8_t *s, int p, vuint8mf2_t *const s0,
                               vuint8mf2_t *const s1, vuint8mf2_t *const s2,
                               vuint8mf2_t *const s3, vuint8mf2_t *const s4,
                               size_t vl) {
  *s0 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s1 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s2 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s3 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s4 = __riscv_vle8_v_u8mf2(s, vl);
}

static inline vuint8mf2_t convolve6_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, size_t vl) {
  // Filter values at indices 0 and 7 are 0, so we start from index 1
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[1], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s5, vl);

  // Round and shift
  // We halved the filter values so -1 from right shift
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(sum, 0, vl));

  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
}

static inline void convolve_y_sr_6tap_rvv(const uint8_t *src_ptr,
                                          int src_stride, uint8_t *dst_ptr,
                                          const int dst_stride, int w, int h,
                                          const int16_t *y_filter) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
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
      // Load next row of data
      vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(s + 0 * src_stride, vl);
      vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + 1 * src_stride, vl);
      vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + 2 * src_stride, vl);
      vuint8mf2_t t8 = __riscv_vle8_v_u8mf2(s + 3 * src_stride, vl);

      // Convert to 16-bit
      vint16m1_t s5 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t5, vl));
      vint16m1_t s6 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t6, vl));
      vint16m1_t s7 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));
      vint16m1_t s8 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));

      // Perform convolution
      vuint8mf2_t d0 = convolve6_8_y_rvv(s0, s1, s2, s3, s4, s5, y_filter, vl);
      vuint8mf2_t d1 = convolve6_8_y_rvv(s1, s2, s3, s4, s5, s6, y_filter, vl);
      vuint8mf2_t d2 = convolve6_8_y_rvv(s2, s3, s4, s5, s6, s7, y_filter, vl);
      vuint8mf2_t d3 = convolve6_8_y_rvv(s3, s4, s5, s6, s7, s8, y_filter, vl);

      // Store result
      store_u8_8x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update sliding window
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

static inline void load_u8_8x7(const uint8_t *s, int p, vuint8mf2_t *const s0,
                               vuint8mf2_t *const s1, vuint8mf2_t *const s2,
                               vuint8mf2_t *const s3, vuint8mf2_t *const s4,
                               vuint8mf2_t *const s5, vuint8mf2_t *const s6,
                               size_t vl) {
  *s0 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s1 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s2 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s3 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s4 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s5 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s6 = __riscv_vle8_v_u8mf2(s, vl);
}

static inline vuint8mf2_t convolve8_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, filter[7], s7, vl);

  // Round and shift
  // We halved the filter values so -1 from right shift
  vuint16m1_t d0 =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(sum, 0, vl));

  return __riscv_vnclipu_wx_u8mf2(d0, FILTER_BITS - 1, __RISCV_VXRM_RNU, vl);
}

static inline void convolve_y_sr_8tap_rvv(const uint8_t *src_ptr,
                                          int src_stride, uint8_t *dst_ptr,
                                          const int dst_stride, int w, int h,
                                          const int16_t *y_filter) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int height = h;

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
      // Load next row
      vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + 0 * src_stride, vl);
      vuint8mf2_t t8 = __riscv_vle8_v_u8mf2(s + 1 * src_stride, vl);
      vuint8mf2_t t9 = __riscv_vle8_v_u8mf2(s + 2 * src_stride, vl);
      vuint8mf2_t t10 = __riscv_vle8_v_u8mf2(s + 3 * src_stride, vl);

      // Convert to 16-bit
      vint16m1_t s7 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));
      vint16m1_t s8 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));
      vint16m1_t s9 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t9, vl));
      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t10, vl));

      // Perform 8-tap vertical convolution
      vuint8mf2_t d0 =
          convolve8_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter, vl);
      vuint8mf2_t d1 =
          convolve8_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8, y_filter, vl);
      vuint8mf2_t d2 =
          convolve8_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9, y_filter, vl);
      vuint8mf2_t d3 =
          convolve8_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10, y_filter, vl);

      // Store result
      store_u8_8x4(d, dst_stride, d0, d1, d2, d3, vl);

      // Update sliding window
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
    } while (height > 0);
    src_ptr += vl;
    dst_ptr += vl;
    w -= vl;
  } while (w > 0);
}

static inline void load_u8_8x11(const uint8_t *s, int p, vuint8mf2_t *const s0,
                                vuint8mf2_t *const s1, vuint8mf2_t *const s2,
                                vuint8mf2_t *const s3, vuint8mf2_t *const s4,
                                vuint8mf2_t *const s5, vuint8mf2_t *const s6,
                                vuint8mf2_t *const s7, vuint8mf2_t *const s8,
                                vuint8mf2_t *const s9, vuint8mf2_t *const s10,
                                size_t vl) {
  *s0 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s1 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s2 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s3 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s4 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s5 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s6 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s7 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s8 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s9 = __riscv_vle8_v_u8mf2(s, vl);
  s += p;
  *s10 = __riscv_vle8_v_u8mf2(s, vl);
}

static inline vuint8mf2_t convolve12_8_y_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *y_filter, size_t vl) {
  // Initialize sum with first multiplication
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

  // Round and shift
  sum = __riscv_vadd_vx_i32m2(sum, 1 << (FILTER_BITS - 1), vl);
  vint16m1_t i16_sum = __riscv_vnsra_wx_i16m1(sum, FILTER_BITS, vl);
  vint16m1_t iclip_sum =
      __riscv_vmin_vx_i16m1(__riscv_vmax_vx_i16m1(i16_sum, 0, vl), 255, vl);

  // Convert to 8-bit
  return __riscv_vncvt_x_x_w_u8mf2(
      __riscv_vreinterpret_v_i16m1_u16m1(iclip_sum), vl);
}

static inline void convolve_y_sr_12tap_rvv(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16_t *y_filter) {
  size_t vl = __riscv_vsetvl_e16m1(w);
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int height = h;

    // Load initial 11 rows of data
    vuint8mf2_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
    load_u8_8x11(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8,
                 &t9, &t10, vl);

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
    vint16m1_t s7 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t7, vl));
    vint16m1_t s8 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));
    vint16m1_t s9 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t9, vl));
    vint16m1_t s10 =
        __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t10, vl));

    s += 11 * src_stride;

    do {
      // Load next 4 rows
      vuint8mf2_t t11 = __riscv_vle8_v_u8mf2(s + 0 * src_stride, vl);
      vuint8mf2_t t12 = __riscv_vle8_v_u8mf2(s + 1 * src_stride, vl);
      vuint8mf2_t t13 = __riscv_vle8_v_u8mf2(s + 2 * src_stride, vl);
      vuint8mf2_t t14 = __riscv_vle8_v_u8mf2(s + 3 * src_stride, vl);

      // Convert to 16-bit
      vint16m1_t s11 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t11, vl));
      vint16m1_t s12 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t12, vl));
      vint16m1_t s13 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t13, vl));
      vint16m1_t s14 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t14, vl));

      // Perform 12-tap convolution
      vuint8mf2_t d0 = convolve12_8_y_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8,
                                          s9, s10, s11, y_filter, vl);
      vuint8mf2_t d1 = convolve12_8_y_rvv(s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                          s10, s11, s12, y_filter, vl);
      vuint8mf2_t d2 = convolve12_8_y_rvv(s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                          s11, s12, s13, y_filter, vl);
      vuint8mf2_t d3 = convolve12_8_y_rvv(s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                          s12, s13, s14, y_filter, vl);

      // Store results
      store_u8_8x4(d, dst_stride, d0, d1, d2, d3, vl);

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

void av1_convolve_y_sr_rvv(const uint8_t *src, int src_stride, uint8_t *dst,
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
  const uint8_t *src_rvv = src - vert_offset * src_stride;
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (y_filter_taps > 8) {
    convolve_y_sr_12tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                            y_filter_ptr);
    return;
  }

  // Filter values are even so halve to reduce precision requirements.
  // In RVV, we need to create a temporary array for the halved filter values
  int16_t halved_filter[8];
  for (int i = 0; i < 8; i++) {
    halved_filter[i] = y_filter_ptr[i] >> 1;
  }

  if (y_filter_taps <= 4) {
    convolve_y_sr_4tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                           halved_filter);
  } else if (y_filter_taps == 6) {
    convolve_y_sr_6tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                           halved_filter);
  } else {
    convolve_y_sr_8tap_rvv(src_rvv, src_stride, dst, dst_stride, w, h,
                           halved_filter);
  }
}

static inline vint16m1_t convolve12_4_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t filter0, const vint16m1_t filter1,
    const vint16m1_t filter2, const vint16m1_t filter3,
    const vint16m1_t filter4, const vint16m1_t filter5,
    const int16_t horiz_const, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vv_i32m2(s0, filter0, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, filter1, s1, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, filter2, s2, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, filter3, s3, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, filter4, s4, vl);
  sum = __riscv_vwmacc_vv_i32m2(sum, filter5, s5, vl);

  sum = __riscv_vadd_vv_i32m2(
      sum, __riscv_vslidedown_vx_i32m2(sum, vl >> 1, vl), vl >> 1);
  sum = __riscv_vadd_vx_i32m2(sum, horiz_const, vl >> 1);

  return __riscv_vnsra_wx_i16m1(sum, ROUND0_BITS, vl >> 1);
}

static inline vint16m1_t convolve12_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const vint16m1_t s8,
    const vint16m1_t s9, const vint16m1_t s10, const vint16m1_t s11,
    const int16_t *x_filter, const int16_t horiz_const, size_t vl) {
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

  sum = __riscv_vadd_vx_i32m2(sum, horiz_const, vl);

  return __riscv_vnsra_wx_i16m1(sum, ROUND0_BITS, vl);
}

static inline void convolve_2d_sr_horiz_12tap_rvv(
    const uint8_t *src, int src_stride, int16_t *dst, const int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, size_t vl) {
  const int bd = 8;
  const int16_t horiz_const =
      (1 << (bd + FILTER_BITS - 1)) + (1 << ((ROUND0_BITS - 1)));

  const int16_t xf0 = x_filter_ptr[0];
  const int16_t xf1 = x_filter_ptr[1];
  const int16_t xf2 = x_filter_ptr[2];
  const int16_t xf3 = x_filter_ptr[3];
  const int16_t xf4 = x_filter_ptr[4];
  const int16_t xf5 = x_filter_ptr[5];
  const int16_t xf6 = x_filter_ptr[6];
  const int16_t xf7 = x_filter_ptr[7];
  const int16_t xf8 = x_filter_ptr[8];
  const int16_t xf9 = x_filter_ptr[9];
  const int16_t xf10 = x_filter_ptr[10];
  const int16_t xf11 = x_filter_ptr[11];

  if (w == 4) {
    uint8_t *s = (uint8_t *)src;
    int16_t *d = dst;

    vl = vl << 1;

    const int16_t filter0[8] = { xf0, xf0, xf0, xf0, xf4, xf4, xf4, xf4 };
    const int16_t filter1[8] = { xf1, xf1, xf1, xf1, xf5, xf5, xf5, xf5 };
    const int16_t filter2[8] = { xf2, xf2, xf2, xf2, xf6, xf6, xf6, xf6 };
    const int16_t filter3[8] = { xf3, xf3, xf3, xf3, xf7, xf7, xf7, xf7 };
    const int16_t filter4[8] = { xf8, xf8, xf8, xf8, xf9, xf9, xf9, xf9 };
    const int16_t filter5[8] = {
      xf10, xf10, xf10, xf10, xf11, xf11, xf11, xf11
    };

    const vint16m1_t vfilter0 = __riscv_vle16_v_i16m1(filter0, vl);
    const vint16m1_t vfilter1 = __riscv_vle16_v_i16m1(filter1, vl);
    const vint16m1_t vfilter2 = __riscv_vle16_v_i16m1(filter2, vl);
    const vint16m1_t vfilter3 = __riscv_vle16_v_i16m1(filter3, vl);
    const vint16m1_t vfilter4 = __riscv_vle16_v_i16m1(filter4, vl);
    const vint16m1_t vfilter5 = __riscv_vle16_v_i16m1(filter5, vl);

    do {
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(s, vl);
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(s + 1, vl);
      vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(s + 2, vl);
      vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(s + 3, vl);
      vuint8mf2_t t4 = load_strided_u8_4xN(s + 8, 1, vl);
      vuint8mf2_t t5 = load_strided_u8_4xN(s + 10, 1, vl);

      vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + src_stride, vl);
      vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + src_stride + 1, vl);
      vuint8mf2_t t8 = __riscv_vle8_v_u8mf2(s + src_stride + 2, vl);
      vuint8mf2_t t9 = __riscv_vle8_v_u8mf2(s + src_stride + 3, vl);
      vuint8mf2_t t10 = load_strided_u8_4xN(s + src_stride + 8, 1, vl);
      vuint8mf2_t t11 = load_strided_u8_4xN(s + src_stride + 10, 1, vl);

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
      vint16m1_t s8 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));
      vint16m1_t s9 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t9, vl));
      vint16m1_t s10 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t10, vl));
      vint16m1_t s11 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t11, vl));

      vint16m1_t d0 = convolve12_4_2d_h_rvv(
          s0, s1, s2, s3, s4, s5, vfilter0, vfilter1, vfilter2, vfilter3,
          vfilter4, vfilter5, horiz_const, vl);
      vint16m1_t d1 = convolve12_4_2d_h_rvv(
          s6, s7, s8, s9, s10, s11, vfilter0, vfilter1, vfilter2, vfilter3,
          vfilter4, vfilter5, horiz_const, vl);

      __riscv_vse16_v_i16m1(d, d0, vl >> 1);
      __riscv_vse16_v_i16m1(d + dst_stride, d1, vl >> 1);

      s += src_stride << 1;
      d += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      const uint8_t *s = src;
      int16_t *d = dst;
      int width = w;

      do {
        vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(s, vl);
        vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(s + 1, vl);
        vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(s + 2, vl);
        vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(s + 3, vl);
        vuint8mf2_t t4 = __riscv_vle8_v_u8mf2(s + 4, vl);
        vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(s + 5, vl);
        vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + 6, vl);
        vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + 7, vl);
        vuint8mf2_t t8 = __riscv_vle8_v_u8mf2(s + 8, vl);
        vuint8mf2_t t9 = __riscv_vle8_v_u8mf2(s + 9, vl);
        vuint8mf2_t t10 = __riscv_vle8_v_u8mf2(s + 10, vl);
        vuint8mf2_t t11 = __riscv_vle8_v_u8mf2(s + 11, vl);

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
        vint16m1_t s8 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t8, vl));
        vint16m1_t s9 =
            __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t9, vl));
        vint16m1_t s10 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t10, vl));
        vint16m1_t s11 = __riscv_vreinterpret_v_u16m1_i16m1(
            __riscv_vzext_vf2_u16m1(t11, vl));

        vint16m1_t d0 =
            convolve12_8_2d_h_rvv(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                  s11, x_filter_ptr, horiz_const, vl);

        __riscv_vse16_v_i16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  }
}

static inline vint16m1_t convolve4_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const int16_t *x_filter, const int16_t horiz_const,
    size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, x_filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[3], s3, vl);

  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl);

  return __riscv_vsra_vx_i16m1(sum, ROUND0_BITS - 1, vl);
}

static inline void convolve_2d_sr_horiz_4tap_rvv(
    const uint8_t *src, ptrdiff_t src_stride, int16_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16_t *filter_x, size_t vl) {
  const int bd = 8;
  const int16_t *filter = filter_x + 2;
  const int16_t horiz_const =
      (1 << (bd + FILTER_BITS - 2)) + (1 << ((ROUND0_BITS - 1) - 1));

  const int16_t xf0 = filter[0] >> 1;
  const int16_t xf1 = filter[1] >> 1;
  const int16_t xf2 = filter[2] >> 1;
  const int16_t xf3 = filter[3] >> 1;
  const int16_t xfilter[4] = { xf0, xf1, xf2, xf3 };

  if (w <= 4) {
    vl = vl << 1;

    do {
      vuint8mf2_t t0 = load_strided_u8_4xN((uint8_t *)src + 0, src_stride, vl);
      vuint8mf2_t t1 = load_strided_u8_4xN((uint8_t *)src + 1, src_stride, vl);
      vuint8mf2_t t2 = load_strided_u8_4xN((uint8_t *)src + 2, src_stride, vl);
      vuint8mf2_t t3 = load_strided_u8_4xN((uint8_t *)src + 3, src_stride, vl);

      vint16m1_t s0 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t0, vl));
      vint16m1_t s1 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t1, vl));
      vint16m1_t s2 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t2, vl));
      vint16m1_t s3 =
          __riscv_vreinterpret_v_u16m1_i16m1(__riscv_vzext_vf2_u16m1(t3, vl));

      vint16m1_t d0 =
          convolve4_2d_h_rvv(s0, s1, s2, s3, xfilter, horiz_const, vl);

      store_strided_i16_4xN(dst, d0, dst_stride, vl);

      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      int width = w;
      const uint8_t *s = src;
      int16_t *d = dst;

      do {
        vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(s + 0, vl);
        vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(s + 1, vl);
        vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(s + 2, vl);
        vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(s + 3, vl);

        vuint8mf2_t t4 = __riscv_vle8_v_u8mf2(s + src_stride, vl);
        vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(s + src_stride + 1, vl);
        vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + src_stride + 2, vl);
        vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + src_stride + 3, vl);

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

        vint16m1_t d0 =
            convolve4_2d_h_rvv(s0, s1, s2, s3, xfilter, horiz_const, vl);
        vint16m1_t d1 =
            convolve4_2d_h_rvv(s4, s5, s6, s7, xfilter, horiz_const, vl);

        __riscv_vse16_v_i16m1(d, d0, vl);
        __riscv_vse16_v_i16m1(d + dst_stride, d1, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  }
}

static inline vint16m1_t convolve8_4_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t x_filter0, const vint16m1_t x_filter1,
    const vint16m1_t x_filter2, const vint16m1_t x_filter3,
    const int16_t horiz_const, size_t vl) {
  vint16m1_t sum = __riscv_vmul_vv_i16m1(s0, x_filter0, vl);
  sum = __riscv_vmacc_vv_i16m1(sum, x_filter1, s1, vl);
  sum = __riscv_vmacc_vv_i16m1(sum, x_filter2, s2, vl);
  sum = __riscv_vmacc_vv_i16m1(sum, x_filter3, s3, vl);

  sum = __riscv_vadd_vv_i16m1(
      sum, __riscv_vslidedown_vx_i16m1(sum, vl >> 1, vl), vl >> 1);
  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl >> 1);

  return __riscv_vsra_vx_i16m1(sum, ROUND0_BITS - 1, vl >> 1);
}

static inline vint16m1_t convolve8_8_2d_h_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *x_filter,
    const int16_t horiz_const, size_t vl) {
  vint16m1_t sum = __riscv_vmul_vx_i16m1(s0, x_filter[0], vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[1], s1, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[2], s2, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[3], s3, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[4], s4, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[5], s5, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[6], s6, vl);
  sum = __riscv_vmacc_vx_i16m1(sum, x_filter[7], s7, vl);

  sum = __riscv_vadd_vx_i16m1(sum, horiz_const, vl);

  return __riscv_vsra_vx_i16m1(sum, ROUND0_BITS - 1, vl);
}

static inline void convolve_2d_sr_horiz_8tap_rvv(
    const uint8_t *src, ptrdiff_t src_stride, int16_t *dst,
    ptrdiff_t dst_stride, int w, int im_h, const int16_t *x_filter_ptr,
    size_t vl) {
  const int bd = 8;
  const int16_t horiz_const =
      (1 << (bd + FILTER_BITS - 2)) + (1 << ((ROUND0_BITS - 1) - 1));

  int height = im_h;

  const int16_t xf0 = x_filter_ptr[0] >> 1;
  const int16_t xf1 = x_filter_ptr[1] >> 1;
  const int16_t xf2 = x_filter_ptr[2] >> 1;
  const int16_t xf3 = x_filter_ptr[3] >> 1;
  const int16_t xf4 = x_filter_ptr[4] >> 1;
  const int16_t xf5 = x_filter_ptr[5] >> 1;
  const int16_t xf6 = x_filter_ptr[6] >> 1;
  const int16_t xf7 = x_filter_ptr[7] >> 1;

  if (w <= 4) {
    vl = vl << 1;

    const int16_t filter0[8] = { xf0, xf0, xf0, xf0, xf4, xf4, xf4, xf4 };
    const int16_t filter1[8] = { xf1, xf1, xf1, xf1, xf5, xf5, xf5, xf5 };
    const int16_t filter2[8] = { xf2, xf2, xf2, xf2, xf6, xf6, xf6, xf6 };
    const int16_t filter3[8] = { xf3, xf3, xf3, xf3, xf7, xf7, xf7, xf7 };

    const vint16m1_t vfilter0 = __riscv_vle16_v_i16m1(filter0, vl);
    const vint16m1_t vfilter1 = __riscv_vle16_v_i16m1(filter1, vl);
    const vint16m1_t vfilter2 = __riscv_vle16_v_i16m1(filter2, vl);
    const vint16m1_t vfilter3 = __riscv_vle16_v_i16m1(filter3, vl);

    do {
      vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(src, vl);
      vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(src + 1, vl);
      vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(src + 2, vl);
      vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(src + 3, vl);

      vuint8mf2_t t4 = __riscv_vle8_v_u8mf2(src + src_stride, vl);
      vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(src + src_stride + 1, vl);
      vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(src + src_stride + 2, vl);
      vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(src + src_stride + 3, vl);

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

      vint16m1_t d0 = convolve8_4_2d_h_rvv(s0, s1, s2, s3, vfilter0, vfilter1,
                                           vfilter2, vfilter3, horiz_const, vl);
      vint16m1_t d1 = convolve8_4_2d_h_rvv(s4, s5, s6, s7, vfilter0, vfilter1,
                                           vfilter2, vfilter3, horiz_const, vl);

      __riscv_vse16_v_i16m1(dst, d0, vl >> 1);
      __riscv_vse16_v_i16m1(dst + dst_stride, d1, vl >> 1);

      src += src_stride << 1;
      dst += dst_stride << 1;
      height -= 2;
    } while (height > 0);
  } else {
    const int16_t xfilter[8] = { xf0, xf1, xf2, xf3, xf4, xf5, xf6, xf7 };

    do {
      const uint8_t *s = src;
      int16_t *d = dst;
      int width = w;

      do {
        vuint8mf2_t t0 = __riscv_vle8_v_u8mf2(s, vl);
        vuint8mf2_t t1 = __riscv_vle8_v_u8mf2(s + 1, vl);
        vuint8mf2_t t2 = __riscv_vle8_v_u8mf2(s + 2, vl);
        vuint8mf2_t t3 = __riscv_vle8_v_u8mf2(s + 3, vl);
        vuint8mf2_t t4 = __riscv_vle8_v_u8mf2(s + 4, vl);
        vuint8mf2_t t5 = __riscv_vle8_v_u8mf2(s + 5, vl);
        vuint8mf2_t t6 = __riscv_vle8_v_u8mf2(s + 6, vl);
        vuint8mf2_t t7 = __riscv_vle8_v_u8mf2(s + 7, vl);

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

        vint16m1_t d0 = convolve8_8_2d_h_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                             xfilter, horiz_const, vl);

        __riscv_vse16_v_i16m1(d, d0, vl);

        s += vl;
        d += vl;
        width -= vl;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--height != 0);
  }
}

void av1_convolve_2d_sr_rvv(const uint8_t *src, int src_stride, uint8_t *dst,
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

  size_t vl = __riscv_vsetvl_e16m1(w);

  if (filter_params_x->taps > 8) {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    convolve_2d_sr_horiz_12tap_rvv(src_ptr, src_stride, im_block, im_stride, w,
                                   im_h, x_filter_ptr, vl);
    convolve_2d_sr_vert_12tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                  y_filter_ptr, vl);
  } else {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    // horizontal filter
    if (x_filter_taps <= 4) {
      convolve_2d_sr_horiz_4tap_rvv(src_ptr + 2, src_stride, im_block,
                                    im_stride, w, im_h, x_filter_ptr, vl);
    } else {
      convolve_2d_sr_horiz_8tap_rvv(src_ptr, src_stride, im_block, im_stride, w,
                                    im_h, x_filter_ptr, vl);
    }

    // vertical filter
    if (clamped_y_taps <= 4) {
      convolve_2d_sr_vert_4tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_ptr, vl);
    } else if (clamped_y_taps == 6) {
      convolve_2d_sr_vert_6tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_ptr, vl);
    } else {
      convolve_2d_sr_vert_8tap_rvv(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_ptr, vl);
    }
  }
}

void av1_convolve_x_sr_intrabc_rvv(const uint8_t *src, int src_stride,
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

  size_t vl = __riscv_vsetvl_e8m1(w);
  if (w <= 8) {
    do {
      // Load
      vuint8mf2_t s0_0 = __riscv_vle8_v_u8mf2(src, vl);
      vuint8mf2_t s0_1 = __riscv_vle8_v_u8mf2(src + 1, vl);
      vuint8mf2_t s1_0 = __riscv_vle8_v_u8mf2(src + src_stride, vl);
      vuint8mf2_t s1_1 = __riscv_vle8_v_u8mf2(src + src_stride + 1, vl);

      // Average the values
      vuint8mf2_t d0 =
          __riscv_vaaddu_vv_u8mf2(s0_0, s0_1, __RISCV_VXRM_RNU, vl);
      vuint8mf2_t d1 =
          __riscv_vaaddu_vv_u8mf2(s1_0, s1_1, __RISCV_VXRM_RNU, vl);

      __riscv_vse8_v_u8mf2(dst, d0, vl);
      __riscv_vse8_v_u8mf2(dst + dst_stride, d1, vl);

      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      const uint8_t *src_ptr = src;
      uint8_t *dst_ptr = dst;
      int width = w;

      do {
        // Load
        vuint8m1_t s0 = __riscv_vle8_v_u8m1(src_ptr, vl);
        vuint8m1_t s1 = __riscv_vle8_v_u8m1(src_ptr + 1, vl);
        vuint8m1_t s2 = __riscv_vle8_v_u8m1(src_ptr + src_stride, vl);
        vuint8m1_t s3 = __riscv_vle8_v_u8m1(src_ptr + src_stride + 1, vl);

        // Average the values
        vuint8m1_t d0 = __riscv_vaaddu_vv_u8m1(s0, s1, __RISCV_VXRM_RNU, vl);
        vuint8m1_t d1 = __riscv_vaaddu_vv_u8m1(s2, s3, __RISCV_VXRM_RNU, vl);

        // Store
        __riscv_vse8_v_u8m1(dst_ptr, d0, vl);
        __riscv_vse8_v_u8m1(dst_ptr + dst_stride, d1, vl);

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

void av1_convolve_y_sr_intrabc_rvv(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_y_qn) {
  assert(subpel_y_qn == 8);
  assert(filter_params_y->taps == 2);
  (void)filter_params_y;
  (void)subpel_y_qn;

  size_t vl = __riscv_vsetvl_e8m1(w);
  if (w <= 8) {
    vuint8mf2_t s0 = __riscv_vle8_v_u8mf2(src, vl);

    do {
      vuint8mf2_t s1 = __riscv_vle8_v_u8mf2(src + src_stride, vl);
      vuint8mf2_t s2 = __riscv_vle8_v_u8mf2(src + 2 * src_stride, vl);

      // Average the values
      vuint8mf2_t d0 = __riscv_vaaddu_vv_u8mf2(s0, s1, __RISCV_VXRM_RNU, vl);
      vuint8mf2_t d1 = __riscv_vaaddu_vv_u8mf2(s1, s2, __RISCV_VXRM_RNU, vl);

      __riscv_vse8_v_u8mf2(dst, d0, vl);
      __riscv_vse8_v_u8mf2(dst + dst_stride, d1, vl);

      s0 = s2;
      src += src_stride << 1;
      dst += dst_stride << 1;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      const uint8_t *src_ptr = src;
      uint8_t *dst_ptr = dst;
      int height = h;

      vuint8m1_t s0 = __riscv_vle8_v_u8m1(src_ptr, vl);

      do {
        vuint8m1_t s1 = __riscv_vle8_v_u8m1(src_ptr + src_stride, vl);
        vuint8m1_t s2 = __riscv_vle8_v_u8m1(src_ptr + 2 * src_stride, vl);

        // Average the values
        vuint8m1_t d0 = __riscv_vaaddu_vv_u8m1(s0, s1, __RISCV_VXRM_RNU, vl);
        vuint8m1_t d1 = __riscv_vaaddu_vv_u8m1(s1, s2, __RISCV_VXRM_RNU, vl);

        // Store
        __riscv_vse8_v_u8m1(dst_ptr, d0, vl);
        __riscv_vse8_v_u8m1(dst_ptr + dst_stride, d1, vl);

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

void av1_convolve_2d_sr_intrabc_rvv(const uint8_t *src, int src_stride,
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

  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w <= 8) {
    // Horizontal filter.
    vuint8mf2_t s0 = __riscv_vle8_v_u8mf2(src, vl);
    vuint8mf2_t s1 = __riscv_vle8_v_u8mf2(src + 1, vl);
    src += src_stride;

    vuint16m1_t sum0 = __riscv_vwaddu_vv_u16m1(s0, s1, vl);

    do {
      vuint8mf2_t s2 = __riscv_vle8_v_u8mf2(src, vl);
      vuint8mf2_t s3 = __riscv_vle8_v_u8mf2(src + 1, vl);
      src += src_stride;
      vuint8mf2_t s4 = __riscv_vle8_v_u8mf2(src, vl);
      vuint8mf2_t s5 = __riscv_vle8_v_u8mf2(src + 1, vl);
      src += src_stride;

      vuint16m1_t sum1 = __riscv_vwaddu_vv_u16m1(s2, s3, vl);
      vuint16m1_t sum2 = __riscv_vwaddu_vv_u16m1(s4, s5, vl);

      // Vertical filter.
      vuint8mf2_t d0 = __riscv_vnclipu_wx_u8mf2(
          __riscv_vadd_vv_u16m1(sum0, sum1, vl), 2, __RISCV_VXRM_RNU, vl);
      vuint8mf2_t d1 = __riscv_vnclipu_wx_u8mf2(
          __riscv_vadd_vv_u16m1(sum1, sum2, vl), 2, __RISCV_VXRM_RNU, vl);

      __riscv_vse8_v_u8mf2(dst, d0, vl);
      dst += dst_stride;
      __riscv_vse8_v_u8mf2(dst, d1, vl);
      dst += dst_stride;

      sum0 = sum2;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      uint8_t *src_ptr = (uint8_t *)src;
      uint8_t *dst_ptr = dst;
      int height = h;

      // Horizontal filter.
      vuint8mf2_t s0 = __riscv_vle8_v_u8mf2(src_ptr, vl);
      vuint8mf2_t s1 = __riscv_vle8_v_u8mf2(src_ptr + 1, vl);
      src_ptr += src_stride;

      vuint16m1_t sum0 = __riscv_vwaddu_vv_u16m1(s0, s1, vl);

      do {
        vuint8mf2_t s2 = __riscv_vle8_v_u8mf2(src_ptr, vl);
        vuint8mf2_t s3 = __riscv_vle8_v_u8mf2(src_ptr + 1, vl);
        src_ptr += src_stride;
        vuint8mf2_t s4 = __riscv_vle8_v_u8mf2(src_ptr, vl);
        vuint8mf2_t s5 = __riscv_vle8_v_u8mf2(src_ptr + 1, vl);
        src_ptr += src_stride;

        vuint16m1_t sum1 = __riscv_vwaddu_vv_u16m1(s2, s3, vl);
        vuint16m1_t sum2 = __riscv_vwaddu_vv_u16m1(s4, s5, vl);

        // Vertical filter.
        vuint8mf2_t d0 = __riscv_vnclipu_wx_u8mf2(
            __riscv_vadd_vv_u16m1(sum0, sum1, vl), 2, __RISCV_VXRM_RNU, vl);
        vuint8mf2_t d1 = __riscv_vnclipu_wx_u8mf2(
            __riscv_vadd_vv_u16m1(sum1, sum2, vl), 2, __RISCV_VXRM_RNU, vl);

        __riscv_vse8_v_u8mf2(dst_ptr, d0, vl);
        dst_ptr += dst_stride;
        __riscv_vse8_v_u8mf2(dst_ptr, d1, vl);
        dst_ptr += dst_stride;

        sum0 = sum2;
        height -= 2;
      } while (height != 0);

      src += vl;
      dst += vl;
      w -= vl;
    } while (w != 0);
  }
}
