/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "config/av1_rtcd.h"

void av1_build_compound_diffwtd_mask_d16_neon(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  assert(h >= 4);
  assert(w >= 4);
  assert((mask_type == DIFFWTD_38_INV) || (mask_type == DIFFWTD_38));
  const int round =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1 + (bd - 8);
  uint16x8_t diff_q, tmp0, tmp1;
  uint8x8_t diff_d, diff_select;
  const CONV_BUF_TYPE *src0_1, *src1_1;
  const int16x8_t dup_round = vdupq_n_s16((int16_t)(-round));
  const uint8x8_t dup_38 = vdup_n_u8(38);
  const uint8x8_t dup_64 = vdup_n_u8(AOM_BLEND_A64_MAX_ALPHA);
  if (mask_type == DIFFWTD_38) {
    diff_select = vdup_n_u8(255);
  } else {
    diff_select = vdup_n_u8(0);
  }
  if (w >= 8) {
    for (int i = 0; i < h; ++i) {
      src0_1 = src0;
      src1_1 = src1;
      for (int j = 0; j < w; j += 8) {
        __builtin_prefetch(src0_1);
        __builtin_prefetch(src1_1);
        diff_q = vabdq_u16(vld1q_u16(src0_1), vld1q_u16(src1_1));
        diff_q = vrshlq_u16(diff_q, dup_round);
        diff_d = vshrn_n_u16(diff_q, DIFF_FACTOR_LOG2);
        diff_d = vmin_u8(vadd_u8(diff_d, dup_38), dup_64);
        diff_d = vbsl_u8(diff_select, diff_d, vsub_u8(dup_64, diff_d));
        vst1_u8(mask, diff_d);
        src0_1 += 8;
        src1_1 += 8;
        mask += 8;
      }
      src0 += src0_stride;
      src1 += src1_stride;
    }
  } else if (w == 4) {
    for (int i = 0; i < h; i += 2) {
      src0_1 = src0;
      src1_1 = src1;
      __builtin_prefetch(src0_1 + 0 * src0_stride);
      __builtin_prefetch(src0_1 + 1 * src0_stride);
      __builtin_prefetch(src1_1 + 0 * src1_stride);
      __builtin_prefetch(src1_1 + 1 * src1_stride);
      tmp0 = vcombine_u16(vld1_u16(src0_1 + (0 * src0_stride)),
                          vld1_u16(src0_1 + (1 * src0_stride)));
      tmp1 = vcombine_u16(vld1_u16(src1_1 + (0 * src1_stride)),
                          vld1_u16(src1_1 + (1 * src1_stride)));
      diff_q = vabdq_u16(tmp0, tmp1);
      diff_q = vrshlq_u16(diff_q, dup_round);
      diff_d = vshrn_n_u16(diff_q, DIFF_FACTOR_LOG2);
      diff_d = vmin_u8(vadd_u8(diff_d, dup_38), dup_64);
      diff_d = vbsl_u8(diff_select, diff_d, vsub_u8(dup_64, diff_d));
      vst1_u8(mask, diff_d);
      src0 += src0_stride * 2;
      src1 += src1_stride * 2;
      mask += w * 2;
    }
  }
}
