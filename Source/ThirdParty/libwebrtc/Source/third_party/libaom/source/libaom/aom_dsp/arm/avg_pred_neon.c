/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
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

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/arm/blend_neon.h"
#include "aom_dsp/arm/dist_wtd_avg_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/blend.h"

void aom_comp_avg_pred_neon(uint8_t *comp_pred, const uint8_t *pred, int width,
                            int height, const uint8_t *ref, int ref_stride) {
  if (width > 8) {
    do {
      const uint8_t *pred_ptr = pred;
      const uint8_t *ref_ptr = ref;
      uint8_t *comp_pred_ptr = comp_pred;
      int w = width;

      do {
        const uint8x16_t p = vld1q_u8(pred_ptr);
        const uint8x16_t r = vld1q_u8(ref_ptr);
        const uint8x16_t avg = vrhaddq_u8(p, r);

        vst1q_u8(comp_pred_ptr, avg);

        ref_ptr += 16;
        pred_ptr += 16;
        comp_pred_ptr += 16;
        w -= 16;
      } while (w != 0);

      ref += ref_stride;
      pred += width;
      comp_pred += width;
    } while (--height != 0);
  } else if (width == 8) {
    int h = height / 2;

    do {
      const uint8x16_t p = vld1q_u8(pred);
      const uint8x16_t r = load_u8_8x2(ref, ref_stride);
      const uint8x16_t avg = vrhaddq_u8(p, r);

      vst1q_u8(comp_pred, avg);

      ref += 2 * ref_stride;
      pred += 16;
      comp_pred += 16;
    } while (--h != 0);
  } else {
    int h = height / 4;
    assert(width == 4);

    do {
      const uint8x16_t p = vld1q_u8(pred);
      const uint8x16_t r = load_unaligned_u8q(ref, ref_stride);
      const uint8x16_t avg = vrhaddq_u8(p, r);

      vst1q_u8(comp_pred, avg);

      ref += 4 * ref_stride;
      pred += 16;
      comp_pred += 16;
    } while (--h != 0);
  }
}

void aom_comp_mask_pred_neon(uint8_t *comp_pred, const uint8_t *pred, int width,
                             int height, const uint8_t *ref, int ref_stride,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask) {
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int src_stride0 = invert_mask ? width : ref_stride;
  const int src_stride1 = invert_mask ? ref_stride : width;

  if (width > 8) {
    do {
      const uint8_t *src0_ptr = src0;
      const uint8_t *src1_ptr = src1;
      const uint8_t *mask_ptr = mask;
      uint8_t *comp_pred_ptr = comp_pred;
      int w = width;

      do {
        const uint8x16_t s0 = vld1q_u8(src0_ptr);
        const uint8x16_t s1 = vld1q_u8(src1_ptr);
        const uint8x16_t m0 = vld1q_u8(mask_ptr);

        uint8x16_t blend_u8 = alpha_blend_a64_u8x16(m0, s0, s1);

        vst1q_u8(comp_pred_ptr, blend_u8);

        src0_ptr += 16;
        src1_ptr += 16;
        mask_ptr += 16;
        comp_pred_ptr += 16;
        w -= 16;
      } while (w != 0);

      src0 += src_stride0;
      src1 += src_stride1;
      mask += mask_stride;
      comp_pred += width;
    } while (--height != 0);
  } else if (width == 8) {
    do {
      const uint8x8_t s0 = vld1_u8(src0);
      const uint8x8_t s1 = vld1_u8(src1);
      const uint8x8_t m0 = vld1_u8(mask);

      uint8x8_t blend_u8 = alpha_blend_a64_u8x8(m0, s0, s1);

      vst1_u8(comp_pred, blend_u8);

      src0 += src_stride0;
      src1 += src_stride1;
      mask += mask_stride;
      comp_pred += 8;
    } while (--height != 0);
  } else {
    int h = height / 2;
    assert(width == 4);

    do {
      const uint8x8_t s0 = load_unaligned_u8(src0, src_stride0);
      const uint8x8_t s1 = load_unaligned_u8(src1, src_stride1);
      const uint8x8_t m0 = load_unaligned_u8(mask, mask_stride);

      uint8x8_t blend_u8 = alpha_blend_a64_u8x8(m0, s0, s1);

      vst1_u8(comp_pred, blend_u8);

      src0 += 2 * src_stride0;
      src1 += 2 * src_stride1;
      mask += 2 * mask_stride;
      comp_pred += 8;
    } while (--h != 0);
  }
}
