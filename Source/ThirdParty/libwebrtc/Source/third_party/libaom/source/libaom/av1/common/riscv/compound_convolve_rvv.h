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

#ifndef AOM_AV1_COMMON_RISCV_COMPOUND_CONVOLVE_RVV_H_
#define AOM_AV1_COMMON_RISCV_COMPOUND_CONVOLVE_RVV_H_

#include <riscv_vector.h>

#include "aom_dsp/riscv/mem_rvv.h"
#include "av1/common/convolve.h"
#include "av1/common/enums.h"
#include "av1/common/filter.h"

static inline void compute_dist_wtd_avg_4x1_rvv(const vuint16m1_t dd0,
                                                const vuint16m1_t d0,
                                                const uint16_t fwd_offset,
                                                const uint16_t bck_offset,
                                                const int16_t round_offset,
                                                vuint8mf2_t *d0_u8, size_t vl) {
  // blend0 = dd0 * fwd_offset + d0 * bck_offset (32-bit accumulation)
  vuint32m2_t vblend0 = __riscv_vwmulu_vx_u32m2(dd0, fwd_offset, vl);
  vblend0 = __riscv_vwmaccu_vx_u32m2(vblend0, bck_offset, d0, vl);

  // avg0 = vblend0 >> DIST_PRECISION_BITS (narrow to 16-bit)
  vuint16m1_t vavg0 = __riscv_vnsrl_wx_u16m1(vblend0, DIST_PRECISION_BITS, vl);

  // dst0 = (int16_t)vavg0 - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);

  // Pack to 8 lanes for rounding and narrowing
  vint16m1_t vec_zero = __riscv_vmv_s_x_i16m1(0, vl);
  vdst0 =
      __riscv_vslideup_vx_i16m1(vdst0, vec_zero, 4, 8);  // upper 4 lanes = 0
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS,
                                    __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_basic_avg_4x1_rvv(const vuint16m1_t dd0,
                                             const vuint16m1_t d0,
                                             const int16_t round_offset,
                                             vuint8mf2_t *d0_u8, size_t vl) {
  // avg0 = (dd0 + d0) >> 1 (vector halving add, no rounding)
#if __riscv_v_intrinsic == 11000
  vuint16m1_t vavg0 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd0, d0, vl), 1, vl);
#elif __riscv_v_intrinsic >= 12000
  vuint16m1_t vavg0 = __riscv_vaaddu_vv_u16m1(dd0, d0, __RISCV_VXRM_RDN, vl);
#endif

  // dst0 = (int16_t)vavg0 - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);

  // Pack to 8 lanes for rounding and narrowing
  vint16m1_t vec_zero = __riscv_vmv_s_x_i16m1(0, vl);
  vdst0 =
      __riscv_vslideup_vx_i16m1(vdst0, vec_zero, 4, 8);  // upper 4 lanes = 0
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS,
                                    __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_dist_wtd_avg_8x1_rvv(const vuint16m1_t dd0,
                                                const vuint16m1_t d0,
                                                const uint16_t fwd_offset,
                                                const uint16_t bck_offset,
                                                const int16_t round_offset,
                                                vuint8mf2_t *d0_u8, size_t vl) {
  // blend0 = dd0 * fwd_offset + d0 * bck_offset (32-bit accumulation)
  vuint32m2_t vblend0 = __riscv_vwmulu_vx_u32m2(dd0, fwd_offset, vl);
  vblend0 = __riscv_vwmaccu_vx_u32m2(vblend0, bck_offset, d0, vl);

  // avg0 = vblend0 >> DIST_PRECISION_BITS (narrow to 16-bit)
  vuint16m1_t vavg0 = __riscv_vnsrl_wx_u16m1(vblend0, DIST_PRECISION_BITS, vl);

  // dst0 = (int16_t)vavg0 - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);

  // Rounding shift right and narrow to u8
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS,
                                    __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_basic_avg_8x1_rvv(const vuint16m1_t dd0,
                                             const vuint16m1_t d0,
                                             const int16_t round_offset,
                                             vuint8mf2_t *d0_u8, size_t vl) {
  // avg0 = (dd0 + d0) >> 1 (vector halving add, no rounding)
#if __riscv_v_intrinsic == 11000
  vuint16m1_t vavg0 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd0, d0, vl), 1, vl);
#elif __riscv_v_intrinsic >= 12000
  vuint16m1_t vavg0 = __riscv_vaaddu_vv_u16m1(dd0, d0, __RISCV_VXRM_RDN, vl);
#endif

  // dst0 = (int16_t)vavg0 - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);

  // Rounding shift right and narrow to u8
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, FILTER_BITS - ROUND0_BITS,
                                    __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_dist_wtd_avg_4x4_rvv(
    const vuint16mf2_t dd0, const vuint16mf2_t dd1, const vuint16mf2_t dd2,
    const vuint16mf2_t dd3, const vuint16mf2_t d0, const vuint16mf2_t d1,
    const vuint16mf2_t d2, const vuint16mf2_t d3, const uint16_t fwd_offset,
    const uint16_t bck_offset, const int16_t round_offset, vuint8mf4_t *d0_u8,
    vuint8mf4_t *d1_u8, vuint8mf4_t *d2_u8, vuint8mf4_t *d3_u8, size_t vl) {
  // blend = dd* * fwd_offset + d* * bck_offset (32-bit accumulation)
  vuint32m1_t vblend0 = __riscv_vwmulu_vx_u32m1(dd0, fwd_offset, vl);
  vblend0 = __riscv_vwmaccu_vx_u32m1(vblend0, bck_offset, d0, vl);

  vuint32m1_t vblend1 = __riscv_vwmulu_vx_u32m1(dd1, fwd_offset, vl);
  vblend1 = __riscv_vwmaccu_vx_u32m1(vblend1, bck_offset, d1, vl);

  vuint32m1_t vblend2 = __riscv_vwmulu_vx_u32m1(dd2, fwd_offset, vl);
  vblend2 = __riscv_vwmaccu_vx_u32m1(vblend2, bck_offset, d2, vl);

  vuint32m1_t vblend3 = __riscv_vwmulu_vx_u32m1(dd3, fwd_offset, vl);
  vblend3 = __riscv_vwmaccu_vx_u32m1(vblend3, bck_offset, d3, vl);

  // avg = blend >> DIST_PRECISION_BITS (narrow to 16-bit)
  vuint16mf2_t vavg0 =
      __riscv_vnsrl_wx_u16mf2(vblend0, DIST_PRECISION_BITS, vl);
  vuint16mf2_t vavg1 =
      __riscv_vnsrl_wx_u16mf2(vblend1, DIST_PRECISION_BITS, vl);
  vuint16mf2_t vavg2 =
      __riscv_vnsrl_wx_u16mf2(vblend2, DIST_PRECISION_BITS, vl);
  vuint16mf2_t vavg3 =
      __riscv_vnsrl_wx_u16mf2(vblend3, DIST_PRECISION_BITS, vl);

  // dst = vavg - round_offset
  vint16mf2_t vdst0 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg0), round_offset, vl);
  vint16mf2_t vdst1 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg1), round_offset, vl);
  vint16mf2_t vdst2 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg2), round_offset, vl);
  vint16mf2_t vdst3 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg3), round_offset, vl);

  // Saturating Rounded Shift right and narrow to u8
  const int shift = FILTER_BITS - ROUND0_BITS;
  vuint16mf2_t d0_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst0, 0, vl));
  vuint16mf2_t d1_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst1, 0, vl));
  vuint16mf2_t d2_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst2, 0, vl));
  vuint16mf2_t d3_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst3, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf4(d0_clip, shift, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf4(d1_clip, shift, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf4(d2_clip, shift, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf4(d3_clip, shift, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf4(d0_clip, shift, __RISCV_VXRM_RNU, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf4(d1_clip, shift, __RISCV_VXRM_RNU, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf4(d2_clip, shift, __RISCV_VXRM_RNU, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf4(d3_clip, shift, __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_basic_avg_4x4_rvv(
    const vuint16mf2_t dd0, const vuint16mf2_t dd1, const vuint16mf2_t dd2,
    const vuint16mf2_t dd3, const vuint16mf2_t d0, const vuint16mf2_t d1,
    const vuint16mf2_t d2, const vuint16mf2_t d3, const int16_t round_offset,
    vuint8mf4_t *d0_u8, vuint8mf4_t *d1_u8, vuint8mf4_t *d2_u8,
    vuint8mf4_t *d3_u8, size_t vl) {
  // avg = (dd + d) >> 1 (vector halving add)
#if __riscv_v_intrinsic == 11000
  vuint16mf2_t vavg0 =
      __riscv_vnsrl_wx_u16mf2(__riscv_vwaddu_vv_u32m1(dd0, d0, vl), 1, vl);
  vuint16mf2_t vavg1 =
      __riscv_vnsrl_wx_u16mf2(__riscv_vwaddu_vv_u32m1(dd1, d1, vl), 1, vl);
  vuint16mf2_t vavg2 =
      __riscv_vnsrl_wx_u16mf2(__riscv_vwaddu_vv_u32m1(dd2, d2, vl), 1, vl);
  vuint16mf2_t vavg3 =
      __riscv_vnsrl_wx_u16mf2(__riscv_vwaddu_vv_u32m1(dd3, d3, vl), 1, vl);
#elif __riscv_v_intrinsic >= 12000
  vuint16mf2_t vavg0 = __riscv_vaaddu_vv_u16mf2(dd0, d0, __RISCV_VXRM_RDN, vl);
  vuint16mf2_t vavg1 = __riscv_vaaddu_vv_u16mf2(dd1, d1, __RISCV_VXRM_RDN, vl);
  vuint16mf2_t vavg2 = __riscv_vaaddu_vv_u16mf2(dd2, d2, __RISCV_VXRM_RDN, vl);
  vuint16mf2_t vavg3 = __riscv_vaaddu_vv_u16mf2(dd3, d3, __RISCV_VXRM_RDN, vl);
#endif

  // dst_01 = (int16x8_t) vavg01 - round_offset
  vint16mf2_t vdst0 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg0), round_offset, 8);
  vint16mf2_t vdst1 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg1), round_offset, 8);
  vint16mf2_t vdst2 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg2), round_offset, 8);
  vint16mf2_t vdst3 = __riscv_vsub_vx_i16mf2(
      __riscv_vreinterpret_v_u16mf2_i16mf2(vavg3), round_offset, 8);

  // Saturating Rounded Shift right and narrow to u8
  const int shift = FILTER_BITS - ROUND0_BITS;
  vuint16mf2_t d0_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst0, 0, vl));
  vuint16mf2_t d1_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst1, 0, vl));
  vuint16mf2_t d2_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst2, 0, vl));
  vuint16mf2_t d3_clip = __riscv_vreinterpret_v_i16mf2_u16mf2(
      __riscv_vmax_vx_i16mf2(vdst3, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf4(d0_clip, shift, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf4(d1_clip, shift, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf4(d2_clip, shift, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf4(d3_clip, shift, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf4(d0_clip, shift, __RISCV_VXRM_RNU, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf4(d1_clip, shift, __RISCV_VXRM_RNU, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf4(d2_clip, shift, __RISCV_VXRM_RNU, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf4(d3_clip, shift, __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_dist_wtd_avg_8x4_rvv(
    const vuint16m1_t dd0, const vuint16m1_t dd1, const vuint16m1_t dd2,
    const vuint16m1_t dd3, const vuint16m1_t d0, const vuint16m1_t d1,
    const vuint16m1_t d2, const vuint16m1_t d3, const uint16_t fwd_offset,
    const uint16_t bck_offset, const int16_t round_offset, vuint8mf2_t *d0_u8,
    vuint8mf2_t *d1_u8, vuint8mf2_t *d2_u8, vuint8mf2_t *d3_u8, size_t vl) {
  // blend = dd* * fwd_offset + d* * bck_offset (32-bit accumulation)
  vuint32m2_t vblend0 = __riscv_vwmulu_vx_u32m2(dd0, fwd_offset, vl);
  vblend0 = __riscv_vwmaccu_vx_u32m2(vblend0, bck_offset, d0, vl);

  vuint32m2_t vblend1 = __riscv_vwmulu_vx_u32m2(dd1, fwd_offset, vl);
  vblend1 = __riscv_vwmaccu_vx_u32m2(vblend1, bck_offset, d1, vl);

  vuint32m2_t vblend2 = __riscv_vwmulu_vx_u32m2(dd2, fwd_offset, vl);
  vblend2 = __riscv_vwmaccu_vx_u32m2(vblend2, bck_offset, d2, vl);

  vuint32m2_t vblend3 = __riscv_vwmulu_vx_u32m2(dd3, fwd_offset, vl);
  vblend3 = __riscv_vwmaccu_vx_u32m2(vblend3, bck_offset, d3, vl);

  // avg = blend >> DIST_PRECISION_BITS (narrow to 16-bit)
  vuint16m1_t vavg0 = __riscv_vnsrl_wx_u16m1(vblend0, DIST_PRECISION_BITS, vl);
  vuint16m1_t vavg1 = __riscv_vnsrl_wx_u16m1(vblend1, DIST_PRECISION_BITS, vl);
  vuint16m1_t vavg2 = __riscv_vnsrl_wx_u16m1(vblend2, DIST_PRECISION_BITS, vl);
  vuint16m1_t vavg3 = __riscv_vnsrl_wx_u16m1(vblend3, DIST_PRECISION_BITS, vl);

  // dst = vavg - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);
  vint16m1_t vdst1 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg1), round_offset, vl);
  vint16m1_t vdst2 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg2), round_offset, vl);
  vint16m1_t vdst3 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg3), round_offset, vl);

  // Saturating Rounded Shift right and narrow to u8
  const int shift = FILTER_BITS - ROUND0_BITS;
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
  vuint16m1_t d1_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst1, 0, vl));
  vuint16m1_t d2_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst2, 0, vl));
  vuint16m1_t d3_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst3, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, shift, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf2(d1_clip, shift, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf2(d2_clip, shift, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf2(d3_clip, shift, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, shift, __RISCV_VXRM_RNU, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf2(d1_clip, shift, __RISCV_VXRM_RNU, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf2(d2_clip, shift, __RISCV_VXRM_RNU, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf2(d3_clip, shift, __RISCV_VXRM_RNU, vl);
#endif
}

static inline void compute_basic_avg_8x4_rvv(
    const vuint16m1_t dd0, const vuint16m1_t dd1, const vuint16m1_t dd2,
    const vuint16m1_t dd3, const vuint16m1_t d0, const vuint16m1_t d1,
    const vuint16m1_t d2, const vuint16m1_t d3, const int16_t round_offset,
    vuint8mf2_t *d0_u8, vuint8mf2_t *d1_u8, vuint8mf2_t *d2_u8,
    vuint8mf2_t *d3_u8, size_t vl) {
  // avg = (dd + d) >> 1 (vector halving add)
#if __riscv_v_intrinsic == 11000
  vuint16m1_t vavg0 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd0, d0, vl), 1, vl);
  vuint16m1_t vavg1 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd1, d1, vl), 1, vl);
  vuint16m1_t vavg2 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd2, d2, vl), 1, vl);
  vuint16m1_t vavg3 =
      __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(dd3, d3, vl), 1, vl);
#elif __riscv_v_intrinsic >= 12000
  vuint16m1_t vavg0 = __riscv_vaaddu_vv_u16m1(dd0, d0, __RISCV_VXRM_RDN, vl);
  vuint16m1_t vavg1 = __riscv_vaaddu_vv_u16m1(dd1, d1, __RISCV_VXRM_RDN, vl);
  vuint16m1_t vavg2 = __riscv_vaaddu_vv_u16m1(dd2, d2, __RISCV_VXRM_RDN, vl);
  vuint16m1_t vavg3 = __riscv_vaaddu_vv_u16m1(dd3, d3, __RISCV_VXRM_RDN, vl);
#endif

  // vdst = (int16x8_t) vavg - round_offset
  vint16m1_t vdst0 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg0), round_offset, vl);
  vint16m1_t vdst1 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg1), round_offset, vl);
  vint16m1_t vdst2 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg2), round_offset, vl);
  vint16m1_t vdst3 = __riscv_vsub_vx_i16m1(
      __riscv_vreinterpret_v_u16m1_i16m1(vavg3), round_offset, vl);

  // Saturating Rounded Shift right and narrow to u8
  const int shift = FILTER_BITS - ROUND0_BITS;
  vuint16m1_t d0_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst0, 0, vl));
  vuint16m1_t d1_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst1, 0, vl));
  vuint16m1_t d2_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst2, 0, vl));
  vuint16m1_t d3_clip =
      __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(vdst3, 0, vl));
#if __riscv_v_intrinsic == 11000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, shift, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf2(d1_clip, shift, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf2(d2_clip, shift, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf2(d3_clip, shift, vl);
#elif __riscv_v_intrinsic >= 12000
  *d0_u8 = __riscv_vnclipu_wx_u8mf2(d0_clip, shift, __RISCV_VXRM_RNU, vl);
  *d1_u8 = __riscv_vnclipu_wx_u8mf2(d1_clip, shift, __RISCV_VXRM_RNU, vl);
  *d2_u8 = __riscv_vnclipu_wx_u8mf2(d2_clip, shift, __RISCV_VXRM_RNU, vl);
  *d3_u8 = __riscv_vnclipu_wx_u8mf2(d3_clip, shift, __RISCV_VXRM_RNU, vl);
#endif
}

static inline vuint16mf2_t convolve6_4_2d_v_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const int16_t *filter, const int32_t offset_const) {
  // Filter values at indices 0 and 7 are 0.
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[1], 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s1, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s2, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s3, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s4, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s5, 4);
  sum = __riscv_vadd_vx_i32m1(sum, offset_const, 4);

  // Round and shift
  vuint32m1_t d0 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, 4));

#if __riscv_v_intrinsic == 11000
  return __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, 4);
#elif __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU,
                                   4);
#endif
}

static inline vuint16m1_t convolve6_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const int16_t *filter, const int32_t offset_const, size_t vl) {
  // Filter values at indices 0 and 7 are 0.
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[1], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s5, vl);
  sum = __riscv_vadd_vx_i32m2(sum, offset_const, vl);

  // Round and shift
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));

#if __riscv_v_intrinsic == 11000
  return __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU,
                                  vl);
#endif
}

static inline void dist_wtd_convolve_2d_vert_6tap_dist_wtd_avg_rvv(
    int16_t *src_ptr, const int src_stride, uint8_t *dst8_ptr, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));

  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 5 rows of data
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    src_ptr += 5 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(src_ptr, src_stride, &s5, &s6, &s7, &s8, vl);

      // perform 6-tap convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve6_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter, offset_const);
      d1 = convolve6_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter, offset_const);
      d2 = convolve6_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter, offset_const);
      d3 = convolve6_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter, offset_const);

      // weighted average
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);
      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_dist_wtd_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                   fwd_offset, bck_offset, round_offset, &d0_u8,
                                   &d1_u8, &d2_u8, &d3_u8, vl);

      // store results
      __riscv_vse8_v_u8mf4(dst8_ptr + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 3 * dst8_stride, d3_u8, vl);

      // update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        // perform 6-tap convolution
        vuint16m1_t d0, d1, d2, d3;
        d0 = convolve6_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                  offset_const, vl);
        d1 = convolve6_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                  offset_const, vl);
        d2 = convolve6_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                  offset_const, vl);
        d3 = convolve6_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                  offset_const, vl);

        // weighted average
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        // update sliding window
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
      dst8_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void dist_wtd_convolve_2d_vert_6tap_avg_rvv(
    int16_t *src_ptr, const int src_stride, uint8_t *dst8_ptr, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 5 rows of data
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    src_ptr += 5 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(src_ptr, src_stride, &s5, &s6, &s7, &s8, vl);

      // perform 6-tap convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve6_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter, offset_const);
      d1 = convolve6_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter, offset_const);
      d2 = convolve6_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter, offset_const);
      d3 = convolve6_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter, offset_const);

      // average
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_basic_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                vl);

      // Store result
      __riscv_vse8_v_u8mf4(dst8_ptr + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 3 * dst8_stride, d3_u8, vl);
      dst8_ptr += 4 * dst8_stride;

      // Update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // Load initial 5 rows of data
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        // perform 6-tap convolution
        vuint16m1_t d0, d1, d2, d3;
        d0 = convolve6_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                  offset_const, vl);
        d1 = convolve6_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                  offset_const, vl);
        d2 = convolve6_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                  offset_const, vl);
        d3 = convolve6_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                  offset_const, vl);

        // average
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        // update sliding window
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
      dst8_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void dist_wtd_convolve_2d_vert_6tap_rvv(
    int16_t *src_ptr, const int src_stride, ConvolveParams *conv_params,
    const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 5 rows of data
    vint16mf2_t s0, s1, s2, s3, s4;
    load_s16_4x5(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
    src_ptr += 5 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s5, s6, s7, s8;
      load_s16_4x4(src_ptr, src_stride, &s5, &s6, &s7, &s8, vl);

      // perform 6-tap convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve6_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter, offset_const);
      d1 = convolve6_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter, offset_const);
      d2 = convolve6_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter, offset_const);
      d3 = convolve6_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter, offset_const);

      // Store result
      store_u16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3, vl);

      // Update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      // Load initial 5 rows of data
      vint16m1_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4, vl);
      s += 5 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8, vl);

        // perform 6-tap convolution
        vuint16m1_t d0, d1, d2, d3;
        d0 = convolve6_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, y_filter,
                                  offset_const, vl);
        d1 = convolve6_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, y_filter,
                                  offset_const, vl);
        d2 = convolve6_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, y_filter,
                                  offset_const, vl);
        d3 = convolve6_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, y_filter,
                                  offset_const, vl);

        // store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // update sliding window
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

static inline vuint16mf2_t convolve8_4_2d_v_rvv(
    const vint16mf2_t s0, const vint16mf2_t s1, const vint16mf2_t s2,
    const vint16mf2_t s3, const vint16mf2_t s4, const vint16mf2_t s5,
    const vint16mf2_t s6, const vint16mf2_t s7, const int16_t *filter,
    const int32_t offset_const) {
  vint32m1_t sum = __riscv_vwmul_vx_i32m1(s0, filter[0], 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[1], s1, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[2], s2, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[3], s3, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[4], s4, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[5], s5, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[6], s6, 4);
  sum = __riscv_vwmacc_vx_i32m1(sum, filter[7], s7, 4);
  sum = __riscv_vadd_vx_i32m1(sum, offset_const, 4);

  // Round and shift
  vuint32m1_t d0 =
      __riscv_vreinterpret_v_i32m1_u32m1(__riscv_vmax_vx_i32m1(sum, 0, 4));

#if __riscv_v_intrinsic == 11000
  return __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, 4);
#elif __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16mf2(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU,
                                   4);
#endif
}

static inline vuint16m1_t convolve8_8_2d_v_rvv(
    const vint16m1_t s0, const vint16m1_t s1, const vint16m1_t s2,
    const vint16m1_t s3, const vint16m1_t s4, const vint16m1_t s5,
    const vint16m1_t s6, const vint16m1_t s7, const int16_t *filter,
    const int32_t offset_const, size_t vl) {
  vint32m2_t sum = __riscv_vwmul_vx_i32m2(s0, filter[0], vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[1], s1, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[2], s2, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[3], s3, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[4], s4, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[5], s5, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[6], s6, vl);
  sum = __riscv_vwmacc_vx_i32m2(sum, filter[7], s7, vl);
  sum = __riscv_vadd_vx_i32m2(sum, offset_const, vl);

  // Round and shift
  vuint32m2_t d0 =
      __riscv_vreinterpret_v_i32m2_u32m2(__riscv_vmax_vx_i32m2(sum, 0, vl));

#if __riscv_v_intrinsic == 11000
  return __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, vl);
#elif __riscv_v_intrinsic >= 12000
  return __riscv_vnclipu_wx_u16m1(d0, COMPOUND_ROUND1_BITS, __RISCV_VXRM_RNU,
                                  vl);
#endif
}

static inline void dist_wtd_convolve_2d_vert_8tap_dist_wtd_avg_rvv(
    int16_t *src_ptr, const int src_stride, uint8_t *dst8_ptr, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 7 rows of data
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    src_ptr += 7 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(src_ptr, src_stride, &s7, &s8, &s9, &s10, vl);

      // Perform convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve8_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                offset_const);
      d1 = convolve8_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                offset_const);
      d2 = convolve8_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                offset_const);
      d3 = convolve8_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                offset_const);

      // weighted average
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);
      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_dist_wtd_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                   fwd_offset, bck_offset, round_offset, &d0_u8,
                                   &d1_u8, &d2_u8, &d3_u8, vl);

      // store results
      __riscv_vse8_v_u8mf4(dst8_ptr + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 3 * dst8_stride, d3_u8, vl);

      // update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
      s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // load initial 7 rows of data
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        // perform convolution
        vuint16m1_t d0 = convolve8_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                              y_filter, offset_const, vl);
        vuint16m1_t d1 = convolve8_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                              y_filter, offset_const, vl);
        vuint16m1_t d2 = convolve8_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                              y_filter, offset_const, vl);
        vuint16m1_t d3 = convolve8_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                              y_filter, offset_const, vl);

        // weighted average
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                     fwd_offset, bck_offset, round_offset,
                                     &d0_u8, &d1_u8, &d2_u8, &d3_u8, vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        // update sliding window
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
      dst8_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void dist_wtd_convolve_2d_vert_8tap_avg_rvv(
    int16_t *src_ptr, const int src_stride, uint8_t *dst8_ptr, int dst8_stride,
    ConvolveParams *conv_params, const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 7 rows of data
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    src_ptr += 7 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(src_ptr, src_stride, &s7, &s8, &s9, &s10, vl);

      // Perform convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve8_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                offset_const);
      d1 = convolve8_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                offset_const);
      d2 = convolve8_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                offset_const);
      d3 = convolve8_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                offset_const);

      // average
      vuint16mf2_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

      vuint8mf4_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_basic_avg_4x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                vl);

      // Store result
      __riscv_vse8_v_u8mf4(dst8_ptr + 0 * dst8_stride, d0_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 1 * dst8_stride, d1_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 2 * dst8_stride, d2_u8, vl);
      __riscv_vse8_v_u8mf4(dst8_ptr + 3 * dst8_stride, d3_u8, vl);
      dst8_ptr += 4 * dst8_stride;

      // update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
      s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int height = h;

      // load initial 7 rows of data
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        // perform convolution
        vuint16m1_t d0 = convolve8_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                              y_filter, offset_const, vl);
        vuint16m1_t d1 = convolve8_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                              y_filter, offset_const, vl);
        vuint16m1_t d2 = convolve8_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                              y_filter, offset_const, vl);
        vuint16m1_t d3 = convolve8_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                              y_filter, offset_const, vl);

        // average
        vuint16m1_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3, vl);

        vuint8mf2_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4_rvv(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                                  round_offset, &d0_u8, &d1_u8, &d2_u8, &d3_u8,
                                  vl);

        // store results
        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8, vl);
        d_u8 += 4 * dst8_stride;

        // update sliding window
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
      dst8_ptr += vl;
      w -= vl;
    } while (w > 0);
  }
}

static inline void dist_wtd_convolve_2d_vert_8tap_rvv(
    int16_t *src_ptr, const int src_stride, ConvolveParams *conv_params,
    const int16_t *y_filter, int h, int w) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int32_t offset_const = 1 << offset_bits;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    // load initial 7 rows of data
    vint16mf2_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
    src_ptr += 7 * src_stride;

    do {
      // load next 4 rows of data
      vint16mf2_t s7, s8, s9, s10;
      load_s16_4x4(src_ptr, src_stride, &s7, &s8, &s9, &s10, vl);

      // perform convolution
      vuint16mf2_t d0, d1, d2, d3;
      d0 = convolve8_4_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                offset_const);
      d1 = convolve8_4_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                offset_const);
      d2 = convolve8_4_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                offset_const);
      d3 = convolve8_4_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                offset_const);

      // store result
      store_u16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3, vl);

      // update sliding window
      s0 = __riscv_vmv_v_v_i16mf2(s4, vl);
      s1 = __riscv_vmv_v_v_i16mf2(s5, vl);
      s2 = __riscv_vmv_v_v_i16mf2(s6, vl);
      s3 = __riscv_vmv_v_v_i16mf2(s7, vl);
      s4 = __riscv_vmv_v_v_i16mf2(s8, vl);
      s5 = __riscv_vmv_v_v_i16mf2(s9, vl);
      s6 = __riscv_vmv_v_v_i16mf2(s10, vl);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int16_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int height = h;

      // load initial 7 rows of data
      vint16m1_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, vl);
      s += 7 * src_stride;

      do {
        // load next 4 rows of data
        vint16m1_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10, vl);

        // perform convolution
        vuint16m1_t d0 = convolve8_8_2d_v_rvv(s0, s1, s2, s3, s4, s5, s6, s7,
                                              y_filter, offset_const, vl);
        vuint16m1_t d1 = convolve8_8_2d_v_rvv(s1, s2, s3, s4, s5, s6, s7, s8,
                                              y_filter, offset_const, vl);
        vuint16m1_t d2 = convolve8_8_2d_v_rvv(s2, s3, s4, s5, s6, s7, s8, s9,
                                              y_filter, offset_const, vl);
        vuint16m1_t d3 = convolve8_8_2d_v_rvv(s3, s4, s5, s6, s7, s8, s9, s10,
                                              y_filter, offset_const, vl);

        // store results
        store_u16_8x4(d, dst_stride, d0, d1, d2, d3, vl);

        // update sliding window
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

#endif  // AOM_AV1_COMMON_RISCV_COMPOUND_CONVOLVE_RVV_H_
