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

#ifndef AOM_AV1_COMMON_RISCV_HIGHBD_COMPOUND_CONVOLVE_RVV_H_
#define AOM_AV1_COMMON_RISCV_HIGHBD_COMPOUND_CONVOLVE_RVV_H_

#include <riscv_vector.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "aom_dsp/riscv/mem_rvv.h"
#include "aom_ports/mem.h"

#define ROUND_SHIFT 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS

static inline void highbd_comp_avg_rvv(const uint16_t *src_ptr, int src_stride,
                                       uint16_t *dst_ptr, int dst_stride, int w,
                                       int h, ConvolveParams *conv_params,
                                       const int bd, const int shift_bits,
                                       const int offset_bits) {
  const uint16_t offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                          (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t max_val = (1 << bd) - 1;
  const int32_t round_shift_val = 1 << (shift_bits - 1);

  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const size_t vl = __riscv_vsetvl_e16m1(w);

  if (w == 4) {
    do {
      vuint16m1_t src = __riscv_vle16_v_u16m1(src_ptr, vl);
      vuint16m1_t ref = __riscv_vle16_v_u16m1(ref_ptr, vl);

#if __riscv_v_intrinsic >= 12000
      vuint16m1_t avg = __riscv_vaaddu_vv_u16m1(src, ref, __RISCV_VXRM_RDN, vl);
#else
      vuint16m1_t avg =
          __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(src, ref, vl), 1, vl);
#endif
      vint32m2_t diff = __riscv_vreinterpret_v_u32m2_i32m2(
          __riscv_vwsubu_vx_u32m2(avg, offset, vl));
      diff = __riscv_vadd_vx_i32m2(diff, round_shift_val, vl);
      vint16m1_t d0 = __riscv_vnsra_wx_i16m1(diff, shift_bits, vl);
      vuint16m1_t d0_u16 =
          __riscv_vreinterpret_v_i16m1_u16m1(__riscv_vmax_vx_i16m1(d0, 0, vl));
      d0_u16 = __riscv_vminu_vx_u16m1(d0_u16, max_val, vl);

      __riscv_vse16_v_u16m1(dst_ptr, d0_u16, vl);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src_line = src_ptr;
      const uint16_t *ref_line = (const uint16_t *)ref_ptr;
      uint16_t *dst_line = dst_ptr;

      while (width > 0) {
        vuint16m1_t s = __riscv_vle16_v_u16m1(src_line, vl);
        vuint16m1_t r = __riscv_vle16_v_u16m1(ref_line, vl);

#if __riscv_v_intrinsic >= 12000
        vuint16m1_t avg = __riscv_vaaddu_vv_u16m1(s, r, __RISCV_VXRM_RDN, vl);
#else
        vuint16m1_t avg =
            __riscv_vnsrl_wx_u16m1(__riscv_vwaddu_vv_u32m2(s, r, vl), 1, vl);
#endif
        vint32m2_t diff = __riscv_vreinterpret_v_u32m2_i32m2(
            __riscv_vwsubu_vx_u32m2(avg, offset, vl));
        diff = __riscv_vadd_vx_i32m2(diff, round_shift_val, vl);
        vint16m1_t d0 = __riscv_vnsra_wx_i16m1(diff, shift_bits, vl);
        vuint16m1_t d0_u16 = __riscv_vreinterpret_v_i16m1_u16m1(
            __riscv_vmax_vx_i16m1(d0, 0, vl));
        d0_u16 = __riscv_vminu_vx_u16m1(d0_u16, max_val, vl);

        __riscv_vse16_v_u16m1(dst_line, d0_u16, vl);

        src_line += vl;
        ref_line += vl;
        dst_line += vl;
        width -= vl;
      }

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  }
}

static inline void highbd_dist_wtd_comp_avg_rvv(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, ConvolveParams *conv_params, const int bd,
    const int shift_bits, const int offset_bits) {
  const int offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                     (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const uint16_t max_val = (1 << bd) - 1;
  const uint16_t fwd = conv_params->fwd_offset;
  const uint16_t bck = conv_params->bck_offset;

  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;

  const size_t vl = __riscv_vsetvl_e16m1(w);
  vuint32m2_t offset_vec = __riscv_vmv_v_x_u32m2((uint32_t)offset, vl);
  if (w == 4) {
    do {
      vuint16m1_t src = __riscv_vle16_v_u16m1(src_ptr, vl);
      vuint16m1_t ref = __riscv_vle16_v_u16m1(ref_ptr, vl);

      vuint32m2_t wtd = __riscv_vwmulu_vx_u32m2(ref, fwd, vl);
      wtd = __riscv_vwmaccu_vx_u32m2(wtd, bck, src, vl);
      wtd = __riscv_vsrl_vx_u32m2(wtd, DIST_PRECISION_BITS, vl);

      vint32m2_t diff = __riscv_vreinterpret_v_u32m2_i32m2(
          __riscv_vsub_vv_u32m2(wtd, offset_vec, vl));

      vuint32m2_t d0 = __riscv_vreinterpret_v_i32m2_u32m2(
          __riscv_vmax_vx_i32m2(diff, 0, vl));
#if __riscv_v_intrinsic >= 12000
      vuint16m1_t d0_u16 =
          __riscv_vnclipu_wx_u16m1(d0, shift_bits, __RISCV_VXRM_RNU, vl);
#else
      vuint16m1_t d0_u16 = __riscv_vnclipu_wx_u16m1(d0, shift_bits, vl);
#endif
      d0_u16 = __riscv_vminu_vx_u16m1(d0_u16, max_val, vl);

      __riscv_vse16_v_u16m1(dst_ptr, d0_u16, vl);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src_line = src_ptr;
      const uint16_t *ref_line = (const uint16_t *)ref_ptr;
      uint16_t *dst_line = dst_ptr;

      while (width > 0) {
        vuint16m1_t s = __riscv_vle16_v_u16m1(src_line, vl);
        vuint16m1_t r = __riscv_vle16_v_u16m1(ref_line, vl);

        vuint32m2_t wtd = __riscv_vwmulu_vx_u32m2(r, fwd, vl);
        wtd = __riscv_vwmaccu_vx_u32m2(wtd, bck, s, vl);
        wtd = __riscv_vsrl_vx_u32m2(wtd, DIST_PRECISION_BITS, vl);

        vint32m2_t diff = __riscv_vreinterpret_v_u32m2_i32m2(
            __riscv_vsub_vv_u32m2(wtd, offset_vec, vl));

        vuint32m2_t d0 = __riscv_vreinterpret_v_i32m2_u32m2(
            __riscv_vmax_vx_i32m2(diff, 0, vl));
#if __riscv_v_intrinsic >= 12000
        vuint16m1_t d0_u16 =
            __riscv_vnclipu_wx_u16m1(d0, shift_bits, __RISCV_VXRM_RNU, vl);
#else
        vuint16m1_t d0_u16 = __riscv_vnclipu_wx_u16m1(d0, shift_bits, vl);
#endif
        d0_u16 = __riscv_vminu_vx_u16m1(d0_u16, max_val, vl);

        __riscv_vse16_v_u16m1(dst_line, d0_u16, vl);

        src_line += vl;
        ref_line += vl;
        dst_line += vl;
        width -= vl;
      }

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  }
}

#endif  // AOM_AV1_COMMON_RISCV_HIGHBD_COMPOUND_CONVOLVE_RVV_H_
