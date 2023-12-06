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
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/arm/blend_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "config/aom_dsp_rtcd.h"

void aom_blend_a64_vmask_neon(uint8_t *dst, uint32_t dst_stride,
                              const uint8_t *src0, uint32_t src0_stride,
                              const uint8_t *src1, uint32_t src1_stride,
                              const uint8_t *mask, int w, int h) {
  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 2);
  assert(w >= 2);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (w > 8) {
    do {
      uint8x16_t m0 = vdupq_n_u8(mask[0]);
      int i = 0;
      do {
        uint8x16_t s0 = vld1q_u8(src0 + i);
        uint8x16_t s1 = vld1q_u8(src1 + i);

        uint8x16_t blend = alpha_blend_a64_u8x16(m0, s0, s1);

        vst1q_u8(dst + i, blend);

        i += 16;
      } while (i < w);

      mask += 1;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else if (w == 8) {
    do {
      uint8x8_t m0 = vdup_n_u8(mask[0]);
      uint8x8_t s0 = vld1_u8(src0);
      uint8x8_t s1 = vld1_u8(src1);

      uint8x8_t blend = alpha_blend_a64_u8x8(m0, s0, s1);

      vst1_u8(dst, blend);

      mask += 1;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else if (w == 4) {
    do {
      const uint16x4_t m0 = vdup_n_u16((uint16_t)mask[0]);
      const uint16x4_t m1 = vdup_n_u16((uint16_t)mask[1]);
      const uint8x8_t m = vmovn_u16(vcombine_u16(m0, m1));
      uint8x8_t s0 = load_unaligned_u8_4x2(src0, src0_stride);
      uint8x8_t s1 = load_unaligned_u8_4x2(src1, src1_stride);

      uint8x8_t blend = alpha_blend_a64_u8x8(m, s0, s1);

      store_unaligned_u8_4x2(dst, dst_stride, blend);

      mask += 2;
      src0 += 2 * src0_stride;
      src1 += 2 * src1_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w == 2 && h >= 16) {
    do {
      uint16x4_t m0 = vdup_n_u16(0);
      m0 = vld1_lane_u16((uint16_t *)mask, m0, 0);
      uint8x8_t m =
          vzip_u8(vreinterpret_u8_u16(m0), vreinterpret_u8_u16(m0)).val[0];
      uint8x8_t s0 = load_unaligned_u8_2x2(src0, src0_stride);
      uint8x8_t s1 = load_unaligned_u8_2x2(src1, src1_stride);

      uint8x8_t blend = alpha_blend_a64_u8x8(m, s0, s1);

      store_unaligned_u8_2x2(dst, dst_stride, blend);

      mask += 2;
      src0 += 2 * src0_stride;
      src1 += 2 * src1_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h != 0);
  } else {
    aom_blend_a64_vmask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                          mask, w, h);
  }
}
