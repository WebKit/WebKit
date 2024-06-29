/*
 *
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/blend.h"
#include "aom_ports/mem.h"
#include "config/av1_rtcd.h"

static INLINE void diffwtd_mask_highbd_neon(uint8_t *mask, bool inverse,
                                            const uint16_t *src0,
                                            int src0_stride,
                                            const uint16_t *src1,
                                            int src1_stride, int h, int w,
                                            const unsigned int bd) {
  assert(DIFF_FACTOR > 0);
  uint8x16_t max_alpha = vdupq_n_u8(AOM_BLEND_A64_MAX_ALPHA);
  uint8x16_t mask_base = vdupq_n_u8(38);
  uint8x16_t mask_diff = vdupq_n_u8(AOM_BLEND_A64_MAX_ALPHA - 38);

  if (bd == 8) {
    if (w >= 16) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0_lo = vld1q_u16(src0_ptr);
          uint16x8_t s0_hi = vld1q_u16(src0_ptr + 8);
          uint16x8_t s1_lo = vld1q_u16(src1_ptr);
          uint16x8_t s1_hi = vld1q_u16(src1_ptr + 8);

          uint16x8_t diff_lo_u16 = vabdq_u16(s0_lo, s1_lo);
          uint16x8_t diff_hi_u16 = vabdq_u16(s0_hi, s1_hi);
          uint8x8_t diff_lo_u8 = vshrn_n_u16(diff_lo_u16, DIFF_FACTOR_LOG2);
          uint8x8_t diff_hi_u8 = vshrn_n_u16(diff_hi_u16, DIFF_FACTOR_LOG2);
          uint8x16_t diff = vcombine_u8(diff_lo_u8, diff_hi_u8);

          uint8x16_t m;
          if (inverse) {
            m = vqsubq_u8(mask_diff, diff);
          } else {
            m = vminq_u8(vaddq_u8(diff, mask_base), max_alpha);
          }

          vst1q_u8(mask_ptr, m);

          src0_ptr += 16;
          src1_ptr += 16;
          mask_ptr += 16;
          width -= 16;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 8) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0 = vld1q_u16(src0_ptr);
          uint16x8_t s1 = vld1q_u16(src1_ptr);

          uint16x8_t diff_u16 = vabdq_u16(s0, s1);
          uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, DIFF_FACTOR_LOG2);
          uint8x8_t m;
          if (inverse) {
            m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
          } else {
            m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                        vget_low_u8(max_alpha));
          }

          vst1_u8(mask_ptr, m);

          src0_ptr += 8;
          src1_ptr += 8;
          mask_ptr += 8;
          width -= 8;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 4) {
      do {
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t diff_u16 = vabdq_u16(s0, s1);
        uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, DIFF_FACTOR_LOG2);
        uint8x8_t m;
        if (inverse) {
          m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
        } else {
          m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                      vget_low_u8(max_alpha));
        }

        store_u8x4_strided_x2(mask, w, m);

        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        mask += 2 * w;
        h -= 2;
      } while (h != 0);
    }
  } else if (bd == 10) {
    if (w >= 16) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0_lo = vld1q_u16(src0_ptr);
          uint16x8_t s0_hi = vld1q_u16(src0_ptr + 8);
          uint16x8_t s1_lo = vld1q_u16(src1_ptr);
          uint16x8_t s1_hi = vld1q_u16(src1_ptr + 8);

          uint16x8_t diff_lo_u16 = vabdq_u16(s0_lo, s1_lo);
          uint16x8_t diff_hi_u16 = vabdq_u16(s0_hi, s1_hi);
          uint8x8_t diff_lo_u8 = vshrn_n_u16(diff_lo_u16, 2 + DIFF_FACTOR_LOG2);
          uint8x8_t diff_hi_u8 = vshrn_n_u16(diff_hi_u16, 2 + DIFF_FACTOR_LOG2);
          uint8x16_t diff = vcombine_u8(diff_lo_u8, diff_hi_u8);

          uint8x16_t m;
          if (inverse) {
            m = vqsubq_u8(mask_diff, diff);
          } else {
            m = vminq_u8(vaddq_u8(diff, mask_base), max_alpha);
          }

          vst1q_u8(mask_ptr, m);

          src0_ptr += 16;
          src1_ptr += 16;
          mask_ptr += 16;
          width -= 16;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 8) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0 = vld1q_u16(src0_ptr);
          uint16x8_t s1 = vld1q_u16(src1_ptr);

          uint16x8_t diff_u16 = vabdq_u16(s0, s1);
          uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, 2 + DIFF_FACTOR_LOG2);
          uint8x8_t m;
          if (inverse) {
            m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
          } else {
            m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                        vget_low_u8(max_alpha));
          }

          vst1_u8(mask_ptr, m);

          src0_ptr += 8;
          src1_ptr += 8;
          mask_ptr += 8;
          width -= 8;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 4) {
      do {
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t diff_u16 = vabdq_u16(s0, s1);
        uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, 2 + DIFF_FACTOR_LOG2);
        uint8x8_t m;
        if (inverse) {
          m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
        } else {
          m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                      vget_low_u8(max_alpha));
        }

        store_u8x4_strided_x2(mask, w, m);

        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        mask += 2 * w;
        h -= 2;
      } while (h != 0);
    }
  } else {
    assert(bd == 12);
    if (w >= 16) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0_lo = vld1q_u16(src0_ptr);
          uint16x8_t s0_hi = vld1q_u16(src0_ptr + 8);
          uint16x8_t s1_lo = vld1q_u16(src1_ptr);
          uint16x8_t s1_hi = vld1q_u16(src1_ptr + 8);

          uint16x8_t diff_lo_u16 = vabdq_u16(s0_lo, s1_lo);
          uint16x8_t diff_hi_u16 = vabdq_u16(s0_hi, s1_hi);
          uint8x8_t diff_lo_u8 = vshrn_n_u16(diff_lo_u16, 4 + DIFF_FACTOR_LOG2);
          uint8x8_t diff_hi_u8 = vshrn_n_u16(diff_hi_u16, 4 + DIFF_FACTOR_LOG2);
          uint8x16_t diff = vcombine_u8(diff_lo_u8, diff_hi_u8);

          uint8x16_t m;
          if (inverse) {
            m = vqsubq_u8(mask_diff, diff);
          } else {
            m = vminq_u8(vaddq_u8(diff, mask_base), max_alpha);
          }

          vst1q_u8(mask_ptr, m);

          src0_ptr += 16;
          src1_ptr += 16;
          mask_ptr += 16;
          width -= 16;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 8) {
      do {
        uint8_t *mask_ptr = mask;
        const uint16_t *src0_ptr = src0;
        const uint16_t *src1_ptr = src1;
        int width = w;
        do {
          uint16x8_t s0 = vld1q_u16(src0_ptr);
          uint16x8_t s1 = vld1q_u16(src1_ptr);

          uint16x8_t diff_u16 = vabdq_u16(s0, s1);
          uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, 4 + DIFF_FACTOR_LOG2);
          uint8x8_t m;
          if (inverse) {
            m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
          } else {
            m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                        vget_low_u8(max_alpha));
          }

          vst1_u8(mask_ptr, m);

          src0_ptr += 8;
          src1_ptr += 8;
          mask_ptr += 8;
          width -= 8;
        } while (width != 0);
        mask += w;
        src0 += src0_stride;
        src1 += src1_stride;
      } while (--h != 0);
    } else if (w == 4) {
      do {
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t diff_u16 = vabdq_u16(s0, s1);
        uint8x8_t diff_u8 = vshrn_n_u16(diff_u16, 4 + DIFF_FACTOR_LOG2);
        uint8x8_t m;
        if (inverse) {
          m = vqsub_u8(vget_low_u8(mask_diff), diff_u8);
        } else {
          m = vmin_u8(vadd_u8(diff_u8, vget_low_u8(mask_base)),
                      vget_low_u8(max_alpha));
        }

        store_u8x4_strided_x2(mask, w, m);

        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        mask += 2 * w;
        h -= 2;
      } while (h != 0);
    }
  }
}

void av1_build_compound_diffwtd_mask_highbd_neon(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0,
    int src0_stride, const uint8_t *src1, int src1_stride, int h, int w,
    int bd) {
  assert(h % 4 == 0);
  assert(w % 4 == 0);
  assert(mask_type == DIFFWTD_38_INV || mask_type == DIFFWTD_38);

  if (mask_type == DIFFWTD_38) {
    diffwtd_mask_highbd_neon(mask, /*inverse=*/false, CONVERT_TO_SHORTPTR(src0),
                             src0_stride, CONVERT_TO_SHORTPTR(src1),
                             src1_stride, h, w, bd);
  } else {  // mask_type == DIFFWTD_38_INV
    diffwtd_mask_highbd_neon(mask, /*inverse=*/true, CONVERT_TO_SHORTPTR(src0),
                             src0_stride, CONVERT_TO_SHORTPTR(src1),
                             src1_stride, h, w, bd);
  }
}
