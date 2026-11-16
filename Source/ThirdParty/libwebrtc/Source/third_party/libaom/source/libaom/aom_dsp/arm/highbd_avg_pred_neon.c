/*
 * Copyright (c) 2023 The WebM project authors. All rights reserved.
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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/arm/blend_neon.h"
#include "aom_dsp/arm/dist_wtd_avg_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/blend.h"

void aom_highbd_comp_avg_pred_neon(uint8_t *comp_pred8, const uint8_t *pred8,
                                   int width, int height, const uint8_t *ref8,
                                   int ref_stride) {
  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);

  int i = height;
  if (width > 8) {
    do {
      int j = 0;
      do {
        const uint16x8_t p = vld1q_u16(pred + j);
        const uint16x8_t r = vld1q_u16(ref + j);

        uint16x8_t avg = vrhaddq_u16(p, r);
        vst1q_u16(comp_pred + j, avg);

        j += 8;
      } while (j < width);

      comp_pred += width;
      pred += width;
      ref += ref_stride;
    } while (--i != 0);
  } else if (width == 8) {
    do {
      const uint16x8_t p = vld1q_u16(pred);
      const uint16x8_t r = vld1q_u16(ref);

      uint16x8_t avg = vrhaddq_u16(p, r);
      vst1q_u16(comp_pred, avg);

      comp_pred += width;
      pred += width;
      ref += ref_stride;
    } while (--i != 0);
  } else {
    assert(width == 4);
    do {
      const uint16x4_t p = vld1_u16(pred);
      const uint16x4_t r = vld1_u16(ref);

      uint16x4_t avg = vrhadd_u16(p, r);
      vst1_u16(comp_pred, avg);

      comp_pred += width;
      pred += width;
      ref += ref_stride;
    } while (--i != 0);
  }
}

void aom_highbd_comp_mask_pred_neon(uint8_t *comp_pred8, const uint8_t *pred8,
                                    int width, int height, const uint8_t *ref8,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask) {
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);

  const uint16_t *src0 = invert_mask ? pred : ref;
  const uint16_t *src1 = invert_mask ? ref : pred;
  const int src_stride0 = invert_mask ? width : ref_stride;
  const int src_stride1 = invert_mask ? ref_stride : width;

  if (width >= 8) {
    do {
      int j = 0;

      do {
        const uint16x8_t s0 = vld1q_u16(src0 + j);
        const uint16x8_t s1 = vld1q_u16(src1 + j);
        const uint16x8_t m0 = vmovl_u8(vld1_u8(mask + j));

        uint16x8_t blend_u16 = alpha_blend_a64_u16x8(m0, s0, s1);

        vst1q_u16(comp_pred + j, blend_u16);

        j += 8;
      } while (j < width);

      src0 += src_stride0;
      src1 += src_stride1;
      mask += mask_stride;
      comp_pred += width;
    } while (--height != 0);
  } else {
    assert(width == 4);

    do {
      const uint16x4_t s0 = vld1_u16(src0);
      const uint16x4_t s1 = vld1_u16(src1);
      const uint16x4_t m0 = vget_low_u16(vmovl_u8(load_unaligned_u8_4x1(mask)));

      uint16x4_t blend_u16 = alpha_blend_a64_u16x4(m0, s0, s1);

      vst1_u16(comp_pred, blend_u16);

      src0 += src_stride0;
      src1 += src_stride1;
      mask += mask_stride;
      comp_pred += 4;
    } while (--height != 0);
  }
}
