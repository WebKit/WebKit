/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <math.h>

#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/restoration.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

int64_t av1_lowbd_pixel_proj_error_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int i, j, k;
  const int32_t shift = SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS;
  const int32x4_t zero = vdupq_n_s32(0);
  uint64x2_t sum64 = vreinterpretq_u64_s32(zero);
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;

  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {
    for (i = 0; i < height; ++i) {
      int32x4_t err0 = zero;
      for (j = 0; j <= width - 8; j += 8) {
        const uint8x8_t d0 = vld1_u8(&dat[j]);
        const uint8x8_t s0 = vld1_u8(&src[j]);
        const int16x8_t flt0_16b =
            vcombine_s16(vqmovn_s32(vld1q_s32(&flt0[j])),
                         vqmovn_s32(vld1q_s32(&flt0[j + 4])));
        const int16x8_t flt1_16b =
            vcombine_s16(vqmovn_s32(vld1q_s32(&flt1[j])),
                         vqmovn_s32(vld1q_s32(&flt1[j + 4])));
        const int16x8_t u0 =
            vreinterpretq_s16_u16(vshll_n_u8(d0, SGRPROJ_RST_BITS));
        const int16x8_t flt0_0_sub_u = vsubq_s16(flt0_16b, u0);
        const int16x8_t flt1_0_sub_u = vsubq_s16(flt1_16b, u0);
        const int16x4_t flt0_16b_sub_u_lo = vget_low_s16(flt0_0_sub_u);
        const int16x4_t flt0_16b_sub_u_hi = vget_high_s16(flt0_0_sub_u);
        const int16x4_t flt1_16b_sub_u_lo = vget_low_s16(flt1_0_sub_u);
        const int16x4_t flt1_16b_sub_u_hi = vget_high_s16(flt1_0_sub_u);

        int32x4_t v0 = vmull_n_s16(flt0_16b_sub_u_lo, (int16_t)xq[0]);
        v0 = vmlal_n_s16(v0, flt1_16b_sub_u_lo, (int16_t)xq[1]);
        int32x4_t v1 = vmull_n_s16(flt0_16b_sub_u_hi, (int16_t)xq[0]);
        v1 = vmlal_n_s16(v1, flt1_16b_sub_u_hi, (int16_t)xq[1]);
        const int16x4_t vr0 = vqrshrn_n_s32(v0, 11);
        const int16x4_t vr1 = vqrshrn_n_s32(v1, 11);
        const int16x8_t e0 = vaddq_s16(vcombine_s16(vr0, vr1),
                                       vreinterpretq_s16_u16(vsubl_u8(d0, s0)));
        const int16x4_t e0_lo = vget_low_s16(e0);
        const int16x4_t e0_hi = vget_high_s16(e0);
        err0 = vmlal_s16(err0, e0_lo, e0_lo);
        err0 = vmlal_s16(err0, e0_hi, e0_hi);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = dat[k] << SGRPROJ_RST_BITS;
        int32_t v = xq[0] * (flt0[k] - u) + xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, 11) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
      sum64 = vpadalq_u32(sum64, vreinterpretq_u32_s32(err0));
    }

  } else if (params->r[0] > 0 || params->r[1] > 0) {
    const int xq_active = (params->r[0] > 0) ? xq[0] : xq[1];
    const int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    const int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    for (i = 0; i < height; ++i) {
      int32x4_t err0 = zero;
      for (j = 0; j <= width - 8; j += 8) {
        const uint8x8_t d0 = vld1_u8(&dat[j]);
        const uint8x8_t s0 = vld1_u8(&src[j]);
        const uint16x8_t d0s0 = vsubl_u8(d0, s0);
        const uint16x8x2_t d0w =
            vzipq_u16(vmovl_u8(d0), vreinterpretq_u16_s32(zero));

        const int32x4_t flt_16b_lo = vld1q_s32(&flt[j]);
        const int32x4_t flt_16b_hi = vld1q_s32(&flt[j + 4]);

        int32x4_t v0 = vmulq_n_s32(flt_16b_lo, xq_active);
        v0 = vmlsq_n_s32(v0, vreinterpretq_s32_u16(d0w.val[0]),
                         xq_active << SGRPROJ_RST_BITS);
        int32x4_t v1 = vmulq_n_s32(flt_16b_hi, xq_active);
        v1 = vmlsq_n_s32(v1, vreinterpretq_s32_u16(d0w.val[1]),
                         xq_active << SGRPROJ_RST_BITS);
        const int16x4_t vr0 = vqrshrn_n_s32(v0, 11);
        const int16x4_t vr1 = vqrshrn_n_s32(v1, 11);
        const int16x8_t e0 =
            vaddq_s16(vcombine_s16(vr0, vr1), vreinterpretq_s16_u16(d0s0));
        const int16x4_t e0_lo = vget_low_s16(e0);
        const int16x4_t e0_hi = vget_high_s16(e0);
        err0 = vmlal_s16(err0, e0_lo, e0_lo);
        err0 = vmlal_s16(err0, e0_hi, e0_hi);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = dat[k] << SGRPROJ_RST_BITS;
        int32_t v = xq_active * (flt[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt += flt_stride;
      sum64 = vpadalq_u32(sum64, vreinterpretq_u32_s32(err0));
    }
  } else {
    uint32x4_t err0 = vreinterpretq_u32_s32(zero);
    for (i = 0; i < height; ++i) {
      for (j = 0; j <= width - 16; j += 16) {
        const uint8x16_t d = vld1q_u8(&dat[j]);
        const uint8x16_t s = vld1q_u8(&src[j]);
        const uint8x16_t diff = vabdq_u8(d, s);
        const uint8x8_t diff0 = vget_low_u8(diff);
        const uint8x8_t diff1 = vget_high_u8(diff);
        err0 = vpadalq_u16(err0, vmull_u8(diff0, diff0));
        err0 = vpadalq_u16(err0, vmull_u8(diff1, diff1));
      }
      for (k = j; k < width; ++k) {
        const int32_t e = dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
    }
    sum64 = vpaddlq_u32(err0);
  }
#if defined(__aarch64__)
  err += vaddvq_u64(sum64);
#else
  err += vget_lane_u64(vadd_u64(vget_low_u64(sum64), vget_high_u64(sum64)), 0);
#endif  // __aarch64__
  return err;
}
