/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_ARM_CONVOLVE_NEON_I8MM_H_
#define AOM_AV1_COMMON_ARM_CONVOLVE_NEON_I8MM_H_

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"

DECLARE_ALIGNED(16, static const uint8_t, kDotProdPermuteTbl[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

static INLINE int16x4_t convolve12_4_2d_h(uint8x16_t samples,
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          int32x4_t horiz_const) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  uint8x16_t perm_samples[3] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                 vqtbl1q_u8(samples, permute_tbl.val[1]),
                                 vqtbl1q_u8(samples, permute_tbl.val[2]) };

  int32x4_t sum = vusdotq_laneq_s32(horiz_const, perm_samples[0], filters, 0);
  sum = vusdotq_laneq_s32(sum, perm_samples[1], filters, 1);
  sum = vusdotq_laneq_s32(sum, perm_samples[2], filters, 2);

  // Narrow and re-pack.
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_8_2d_h(uint8x16_t samples[2],
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          const int32x4_t horiz_const) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  // {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 }
  uint8x16_t perm_samples[4] = { vqtbl1q_u8(samples[0], permute_tbl.val[0]),
                                 vqtbl1q_u8(samples[0], permute_tbl.val[1]),
                                 vqtbl1q_u8(samples[0], permute_tbl.val[2]),
                                 vqtbl1q_u8(samples[1], permute_tbl.val[2]) };

  int32x4_t sum0123 =
      vusdotq_laneq_s32(horiz_const, perm_samples[0], filters, 0);
  sum0123 = vusdotq_laneq_s32(sum0123, perm_samples[1], filters, 1);
  sum0123 = vusdotq_laneq_s32(sum0123, perm_samples[2], filters, 2);

  int32x4_t sum4567 =
      vusdotq_laneq_s32(horiz_const, perm_samples[1], filters, 0);
  sum4567 = vusdotq_laneq_s32(sum4567, perm_samples[2], filters, 1);
  sum4567 = vusdotq_laneq_s32(sum4567, perm_samples[3], filters, 2);

  // Narrow and re-pack.
  return vcombine_s16(vshrn_n_s32(sum0123, ROUND0_BITS),
                      vshrn_n_s32(sum4567, ROUND0_BITS));
}

static INLINE void convolve_2d_sr_horiz_12tap_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  // The no-op filter should never be used here.
  assert(vgetq_lane_s16(x_filter_0_7, 5) != 128);

  const int bd = 8;

  // Narrow filter values to 8-bit.
  const int16x8x2_t x_filter_s16 = {
    { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
  };
  const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                         vmovn_s16(x_filter_s16.val[1]));
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
  // - which are generally faster than rounding shifts on modern CPUs.
  const int32x4_t horiz_const =
      vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
  const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);

  if (w <= 4) {
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 = convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      int16x4_t d1 = convolve12_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
      int16x4_t d2 = convolve12_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
      int16x4_t d3 = convolve12_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);
      int16x4_t d0 = convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);

  } else {
    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0[2], s1[2], s2[2], s3[2];
        load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
        load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

        int16x8_t d0 =
            convolve12_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x8_t d1 =
            convolve12_8_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x8_t d2 =
            convolve12_8_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x8_t d3 =
            convolve12_8_2d_h(s3, x_filter, permute_tbl, horiz_const);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0[2];
        s0[0] = vld1q_u8(s);
        s0[1] = vld1q_u8(s + 4);
        int16x8_t d0 =
            convolve12_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  }
}

#endif  // AOM_AV1_COMMON_ARM_CONVOLVE_NEON_I8MM_H_
