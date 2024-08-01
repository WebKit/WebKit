/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
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
#include <stdbool.h>

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "config/av1_rtcd.h"

static AOM_INLINE void diffwtd_mask_d16_neon(
    uint8_t *mask, const bool inverse, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  const int round =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1 + (bd - 8);
  const int16x8_t round_vec = vdupq_n_s16((int16_t)(-round));

  if (w >= 16) {
    int i = 0;
    do {
      int j = 0;
      do {
        uint16x8_t s0_lo = vld1q_u16(src0 + j);
        uint16x8_t s1_lo = vld1q_u16(src1 + j);
        uint16x8_t s0_hi = vld1q_u16(src0 + j + 8);
        uint16x8_t s1_hi = vld1q_u16(src1 + j + 8);

        uint16x8_t diff_lo_u16 = vrshlq_u16(vabdq_u16(s0_lo, s1_lo), round_vec);
        uint16x8_t diff_hi_u16 = vrshlq_u16(vabdq_u16(s0_hi, s1_hi), round_vec);
        uint8x8_t diff_lo_u8 = vshrn_n_u16(diff_lo_u16, DIFF_FACTOR_LOG2);
        uint8x8_t diff_hi_u8 = vshrn_n_u16(diff_hi_u16, DIFF_FACTOR_LOG2);
        uint8x16_t diff = vcombine_u8(diff_lo_u8, diff_hi_u8);

        uint8x16_t m;
        if (inverse) {
          m = vqsubq_u8(vdupq_n_u8(64 - 38), diff);  // Saturating to 0
        } else {
          m = vminq_u8(vaddq_u8(diff, vdupq_n_u8(38)), vdupq_n_u8(64));
        }

        vst1q_u8(mask, m);

        mask += 16;
        j += 16;
      } while (j < w);
      src0 += src0_stride;
      src1 += src1_stride;
    } while (++i < h);
  } else if (w == 8) {
    int i = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src0);
      uint16x8_t s1 = vld1q_u16(src1);

      uint16x8_t diff_u16 = vrshlq_u16(vabdq_u16(s0, s1), round_vec);
      uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, DIFF_FACTOR_LOG2);
      uint8x8_t m;
      if (inverse) {
        m = vqsub_u8(vdup_n_u8(64 - 38), diff_u8);  // Saturating to 0
      } else {
        m = vmin_u8(vadd_u8(diff_u8, vdup_n_u8(38)), vdup_n_u8(64));
      }

      vst1_u8(mask, m);

      mask += 8;
      src0 += src0_stride;
      src1 += src1_stride;
    } while (++i < h);
  } else if (w == 4) {
    int i = 0;
    do {
      uint16x8_t s0 =
          vcombine_u16(vld1_u16(src0), vld1_u16(src0 + src0_stride));
      uint16x8_t s1 =
          vcombine_u16(vld1_u16(src1), vld1_u16(src1 + src1_stride));

      uint16x8_t diff_u16 = vrshlq_u16(vabdq_u16(s0, s1), round_vec);
      uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, DIFF_FACTOR_LOG2);
      uint8x8_t m;
      if (inverse) {
        m = vqsub_u8(vdup_n_u8(64 - 38), diff_u8);  // Saturating to 0
      } else {
        m = vmin_u8(vadd_u8(diff_u8, vdup_n_u8(38)), vdup_n_u8(64));
      }

      vst1_u8(mask, m);

      mask += 8;
      src0 += 2 * src0_stride;
      src1 += 2 * src1_stride;
      i += 2;
    } while (i < h);
  }
}

void av1_build_compound_diffwtd_mask_d16_neon(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  assert(h >= 4);
  assert(w >= 4);
  assert((mask_type == DIFFWTD_38_INV) || (mask_type == DIFFWTD_38));

  if (mask_type == DIFFWTD_38) {
    diffwtd_mask_d16_neon(mask, /*inverse=*/false, src0, src0_stride, src1,
                          src1_stride, h, w, conv_params, bd);
  } else {  // mask_type == DIFFWTD_38_INV
    diffwtd_mask_d16_neon(mask, /*inverse=*/true, src0, src0_stride, src1,
                          src1_stride, h, w, conv_params, bd);
  }
}

static AOM_INLINE void diffwtd_mask_neon(uint8_t *mask, const bool inverse,
                                         const uint8_t *src0, int src0_stride,
                                         const uint8_t *src1, int src1_stride,
                                         int h, int w) {
  if (w >= 16) {
    int i = 0;
    do {
      int j = 0;
      do {
        uint8x16_t s0 = vld1q_u8(src0 + j);
        uint8x16_t s1 = vld1q_u8(src1 + j);

        uint8x16_t diff = vshrq_n_u8(vabdq_u8(s0, s1), DIFF_FACTOR_LOG2);
        uint8x16_t m;
        if (inverse) {
          m = vqsubq_u8(vdupq_n_u8(64 - 38), diff);  // Saturating to 0
        } else {
          m = vminq_u8(vaddq_u8(diff, vdupq_n_u8(38)), vdupq_n_u8(64));
        }

        vst1q_u8(mask, m);

        mask += 16;
        j += 16;
      } while (j < w);
      src0 += src0_stride;
      src1 += src1_stride;
    } while (++i < h);
  } else if (w == 8) {
    int i = 0;
    do {
      uint8x16_t s0 = vcombine_u8(vld1_u8(src0), vld1_u8(src0 + src0_stride));
      uint8x16_t s1 = vcombine_u8(vld1_u8(src1), vld1_u8(src1 + src0_stride));

      uint8x16_t diff = vshrq_n_u8(vabdq_u8(s0, s1), DIFF_FACTOR_LOG2);
      uint8x16_t m;
      if (inverse) {
        m = vqsubq_u8(vdupq_n_u8(64 - 38), diff);  // Saturating to 0
      } else {
        m = vminq_u8(vaddq_u8(diff, vdupq_n_u8(38)), vdupq_n_u8(64));
      }

      vst1q_u8(mask, m);

      mask += 16;
      src0 += 2 * src0_stride;
      src1 += 2 * src1_stride;
      i += 2;
    } while (i < h);
  } else if (w == 4) {
    int i = 0;
    do {
      uint8x16_t s0 = load_unaligned_u8q(src0, src0_stride);
      uint8x16_t s1 = load_unaligned_u8q(src1, src1_stride);

      uint8x16_t diff = vshrq_n_u8(vabdq_u8(s0, s1), DIFF_FACTOR_LOG2);
      uint8x16_t m;
      if (inverse) {
        m = vqsubq_u8(vdupq_n_u8(64 - 38), diff);  // Saturating to 0
      } else {
        m = vminq_u8(vaddq_u8(diff, vdupq_n_u8(38)), vdupq_n_u8(64));
      }

      vst1q_u8(mask, m);

      mask += 16;
      src0 += 4 * src0_stride;
      src1 += 4 * src1_stride;
      i += 4;
    } while (i < h);
  }
}

void av1_build_compound_diffwtd_mask_neon(uint8_t *mask,
                                          DIFFWTD_MASK_TYPE mask_type,
                                          const uint8_t *src0, int src0_stride,
                                          const uint8_t *src1, int src1_stride,
                                          int h, int w) {
  assert(h % 4 == 0);
  assert(w % 4 == 0);
  assert(mask_type == DIFFWTD_38_INV || mask_type == DIFFWTD_38);

  if (mask_type == DIFFWTD_38) {
    diffwtd_mask_neon(mask, /*inverse=*/false, src0, src0_stride, src1,
                      src1_stride, h, w);
  } else {  // mask_type == DIFFWTD_38_INV
    diffwtd_mask_neon(mask, /*inverse=*/true, src0, src0_stride, src1,
                      src1_stride, h, w);
  }
}
