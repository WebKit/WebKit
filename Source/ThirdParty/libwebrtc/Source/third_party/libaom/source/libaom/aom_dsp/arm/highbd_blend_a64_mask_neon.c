/*
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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/arm/blend_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/blend.h"

#define HBD_BLEND_A64_D16_MASK(bd, round0_bits)                               \
  static INLINE uint16x8_t alpha_##bd##_blend_a64_d16_u16x8(                  \
      uint16x8_t m, uint16x8_t a, uint16x8_t b, int32x4_t round_offset) {     \
    const uint16x8_t m_inv =                                                  \
        vsubq_u16(vdupq_n_u16(AOM_BLEND_A64_MAX_ALPHA), m);                   \
                                                                              \
    uint32x4_t blend_u32_lo = vmlal_u16(vreinterpretq_u32_s32(round_offset),  \
                                        vget_low_u16(m), vget_low_u16(a));    \
    uint32x4_t blend_u32_hi = vmlal_u16(vreinterpretq_u32_s32(round_offset),  \
                                        vget_high_u16(m), vget_high_u16(a));  \
                                                                              \
    blend_u32_lo =                                                            \
        vmlal_u16(blend_u32_lo, vget_low_u16(m_inv), vget_low_u16(b));        \
    blend_u32_hi =                                                            \
        vmlal_u16(blend_u32_hi, vget_high_u16(m_inv), vget_high_u16(b));      \
                                                                              \
    uint16x4_t blend_u16_lo =                                                 \
        vqrshrun_n_s32(vreinterpretq_s32_u32(blend_u32_lo),                   \
                       AOM_BLEND_A64_ROUND_BITS + 2 * FILTER_BITS -           \
                           round0_bits - COMPOUND_ROUND1_BITS);               \
    uint16x4_t blend_u16_hi =                                                 \
        vqrshrun_n_s32(vreinterpretq_s32_u32(blend_u32_hi),                   \
                       AOM_BLEND_A64_ROUND_BITS + 2 * FILTER_BITS -           \
                           round0_bits - COMPOUND_ROUND1_BITS);               \
                                                                              \
    uint16x8_t blend_u16 = vcombine_u16(blend_u16_lo, blend_u16_hi);          \
    blend_u16 = vminq_u16(blend_u16, vdupq_n_u16((1 << bd) - 1));             \
                                                                              \
    return blend_u16;                                                         \
  }                                                                           \
                                                                              \
  static INLINE void highbd_##bd##_blend_a64_d16_mask_neon(                   \
      uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,          \
      uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,  \
      const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw,      \
      int subh) {                                                             \
    const int offset_bits = bd + 2 * FILTER_BITS - round0_bits;               \
    int32_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +      \
                           (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));   \
    int32x4_t offset =                                                        \
        vdupq_n_s32(-(round_offset << AOM_BLEND_A64_ROUND_BITS));             \
                                                                              \
    if ((subw | subh) == 0) {                                                 \
      if (w >= 8) {                                                           \
        do {                                                                  \
          int i = 0;                                                          \
          do {                                                                \
            uint16x8_t m0 = vmovl_u8(vld1_u8(mask + i));                      \
            uint16x8_t s0 = vld1q_u16(src0 + i);                              \
            uint16x8_t s1 = vld1q_u16(src1 + i);                              \
                                                                              \
            uint16x8_t blend =                                                \
                alpha_##bd##_blend_a64_d16_u16x8(m0, s0, s1, offset);         \
                                                                              \
            vst1q_u16(dst + i, blend);                                        \
            i += 8;                                                           \
          } while (i < w);                                                    \
                                                                              \
          mask += mask_stride;                                                \
          src0 += src0_stride;                                                \
          src1 += src1_stride;                                                \
          dst += dst_stride;                                                  \
        } while (--h != 0);                                                   \
      } else {                                                                \
        do {                                                                  \
          uint16x8_t m0 = vmovl_u8(load_unaligned_u8_4x2(mask, mask_stride)); \
          uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);          \
          uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);          \
                                                                              \
          uint16x8_t blend =                                                  \
              alpha_##bd##_blend_a64_d16_u16x8(m0, s0, s1, offset);           \
                                                                              \
          store_unaligned_u16_4x2(dst, dst_stride, blend);                    \
                                                                              \
          mask += 2 * mask_stride;                                            \
          src0 += 2 * src0_stride;                                            \
          src1 += 2 * src1_stride;                                            \
          dst += 2 * dst_stride;                                              \
          h -= 2;                                                             \
        } while (h != 0);                                                     \
      }                                                                       \
    } else if ((subw & subh) == 1) {                                          \
      if (w >= 8) {                                                           \
        do {                                                                  \
          int i = 0;                                                          \
          do {                                                                \
            uint8x16_t m0 = vld1q_u8(mask + 0 * mask_stride + 2 * i);         \
            uint8x16_t m1 = vld1q_u8(mask + 1 * mask_stride + 2 * i);         \
            uint16x8_t s0 = vld1q_u16(src0 + i);                              \
            uint16x8_t s1 = vld1q_u16(src1 + i);                              \
                                                                              \
            uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8_4(            \
                vget_low_u8(m0), vget_low_u8(m1), vget_high_u8(m0),           \
                vget_high_u8(m1)));                                           \
            uint16x8_t blend =                                                \
                alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);      \
                                                                              \
            vst1q_u16(dst + i, blend);                                        \
            i += 8;                                                           \
          } while (i < w);                                                    \
                                                                              \
          mask += 2 * mask_stride;                                            \
          src0 += src0_stride;                                                \
          src1 += src1_stride;                                                \
          dst += dst_stride;                                                  \
        } while (--h != 0);                                                   \
      } else {                                                                \
        do {                                                                  \
          uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride);                     \
          uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride);                     \
          uint8x8_t m2 = vld1_u8(mask + 2 * mask_stride);                     \
          uint8x8_t m3 = vld1_u8(mask + 3 * mask_stride);                     \
          uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);          \
          uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);          \
                                                                              \
          uint16x8_t m_avg =                                                  \
              vmovl_u8(avg_blend_pairwise_u8x8_4(m0, m1, m2, m3));            \
          uint16x8_t blend =                                                  \
              alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);        \
                                                                              \
          store_unaligned_u16_4x2(dst, dst_stride, blend);                    \
                                                                              \
          mask += 4 * mask_stride;                                            \
          src0 += 2 * src0_stride;                                            \
          src1 += 2 * src1_stride;                                            \
          dst += 2 * dst_stride;                                              \
          h -= 2;                                                             \
        } while (h != 0);                                                     \
      }                                                                       \
    } else if (subw == 1 && subh == 0) {                                      \
      if (w >= 8) {                                                           \
        do {                                                                  \
          int i = 0;                                                          \
          do {                                                                \
            uint8x8_t m0 = vld1_u8(mask + 2 * i);                             \
            uint8x8_t m1 = vld1_u8(mask + 2 * i + 8);                         \
            uint16x8_t s0 = vld1q_u16(src0 + i);                              \
            uint16x8_t s1 = vld1q_u16(src1 + i);                              \
                                                                              \
            uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8(m0, m1));     \
            uint16x8_t blend =                                                \
                alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);      \
                                                                              \
            vst1q_u16(dst + i, blend);                                        \
            i += 8;                                                           \
          } while (i < w);                                                    \
                                                                              \
          mask += mask_stride;                                                \
          src0 += src0_stride;                                                \
          src1 += src1_stride;                                                \
          dst += dst_stride;                                                  \
        } while (--h != 0);                                                   \
      } else {                                                                \
        do {                                                                  \
          uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride);                     \
          uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride);                     \
          uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);          \
          uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);          \
                                                                              \
          uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8(m0, m1));       \
          uint16x8_t blend =                                                  \
              alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);        \
                                                                              \
          store_unaligned_u16_4x2(dst, dst_stride, blend);                    \
                                                                              \
          mask += 2 * mask_stride;                                            \
          src0 += 2 * src0_stride;                                            \
          src1 += 2 * src1_stride;                                            \
          dst += 2 * dst_stride;                                              \
          h -= 2;                                                             \
        } while (h != 0);                                                     \
      }                                                                       \
    } else {                                                                  \
      if (w >= 8) {                                                           \
        do {                                                                  \
          int i = 0;                                                          \
          do {                                                                \
            uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride + i);               \
            uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride + i);               \
            uint16x8_t s0 = vld1q_u16(src0 + i);                              \
            uint16x8_t s1 = vld1q_u16(src1 + i);                              \
                                                                              \
            uint16x8_t m_avg = vmovl_u8(avg_blend_u8x8(m0, m1));              \
            uint16x8_t blend =                                                \
                alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);      \
                                                                              \
            vst1q_u16(dst + i, blend);                                        \
            i += 8;                                                           \
          } while (i < w);                                                    \
                                                                              \
          mask += 2 * mask_stride;                                            \
          src0 += src0_stride;                                                \
          src1 += src1_stride;                                                \
          dst += dst_stride;                                                  \
        } while (--h != 0);                                                   \
      } else {                                                                \
        do {                                                                  \
          uint8x8_t m0_2 =                                                    \
              load_unaligned_u8_4x2(mask + 0 * mask_stride, 2 * mask_stride); \
          uint8x8_t m1_3 =                                                    \
              load_unaligned_u8_4x2(mask + 1 * mask_stride, 2 * mask_stride); \
          uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);          \
          uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);          \
                                                                              \
          uint16x8_t m_avg = vmovl_u8(avg_blend_u8x8(m0_2, m1_3));            \
          uint16x8_t blend =                                                  \
              alpha_##bd##_blend_a64_d16_u16x8(m_avg, s0, s1, offset);        \
                                                                              \
          store_unaligned_u16_4x2(dst, dst_stride, blend);                    \
                                                                              \
          mask += 4 * mask_stride;                                            \
          src0 += 2 * src0_stride;                                            \
          src1 += 2 * src1_stride;                                            \
          dst += 2 * dst_stride;                                              \
          h -= 2;                                                             \
        } while (h != 0);                                                     \
      }                                                                       \
    }                                                                         \
  }

// 12 bitdepth
HBD_BLEND_A64_D16_MASK(12, (ROUND0_BITS + 2))
// 10 bitdepth
HBD_BLEND_A64_D16_MASK(10, ROUND0_BITS)
// 8 bitdepth
HBD_BLEND_A64_D16_MASK(8, ROUND0_BITS)

void aom_highbd_blend_a64_d16_mask_neon(
    uint8_t *dst_8, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params, const int bd) {
  (void)conv_params;
  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  uint16_t *dst = CONVERT_TO_SHORTPTR(dst_8);
  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  if (bd == 12) {
    highbd_12_blend_a64_d16_mask_neon(dst, dst_stride, src0, src0_stride, src1,
                                      src1_stride, mask, mask_stride, w, h,
                                      subw, subh);
  } else if (bd == 10) {
    highbd_10_blend_a64_d16_mask_neon(dst, dst_stride, src0, src0_stride, src1,
                                      src1_stride, mask, mask_stride, w, h,
                                      subw, subh);
  } else {
    highbd_8_blend_a64_d16_mask_neon(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, w, h, subw,
                                     subh);
  }
}

void aom_highbd_blend_a64_mask_neon(uint8_t *dst_8, uint32_t dst_stride,
                                    const uint8_t *src0_8, uint32_t src0_stride,
                                    const uint8_t *src1_8, uint32_t src1_stride,
                                    const uint8_t *mask, uint32_t mask_stride,
                                    int w, int h, int subw, int subh, int bd) {
  (void)bd;

  const uint16_t *src0 = CONVERT_TO_SHORTPTR(src0_8);
  const uint16_t *src1 = CONVERT_TO_SHORTPTR(src1_8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst_8);

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);

  if ((subw | subh) == 0) {
    if (w >= 8) {
      do {
        int i = 0;
        do {
          uint16x8_t m0 = vmovl_u8(vld1_u8(mask + i));
          uint16x8_t s0 = vld1q_u16(src0 + i);
          uint16x8_t s1 = vld1q_u16(src1 + i);

          uint16x8_t blend = alpha_blend_a64_u16x8(m0, s0, s1);

          vst1q_u16(dst + i, blend);
          i += 8;
        } while (i < w);

        mask += mask_stride;
        src0 += src0_stride;
        src1 += src1_stride;
        dst += dst_stride;
      } while (--h != 0);
    } else {
      do {
        uint16x8_t m0 = vmovl_u8(load_unaligned_u8_4x2(mask, mask_stride));
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t blend = alpha_blend_a64_u16x8(m0, s0, s1);

        store_unaligned_u16_4x2(dst, dst_stride, blend);

        mask += 2 * mask_stride;
        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h != 0);
    }
  } else if ((subw & subh) == 1) {
    if (w >= 8) {
      do {
        int i = 0;
        do {
          uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride + 2 * i);
          uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride + 2 * i);
          uint8x8_t m2 = vld1_u8(mask + 0 * mask_stride + 2 * i + 8);
          uint8x8_t m3 = vld1_u8(mask + 1 * mask_stride + 2 * i + 8);
          uint16x8_t s0 = vld1q_u16(src0 + i);
          uint16x8_t s1 = vld1q_u16(src1 + i);

          uint16x8_t m_avg =
              vmovl_u8(avg_blend_pairwise_u8x8_4(m0, m1, m2, m3));

          uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

          vst1q_u16(dst + i, blend);

          i += 8;
        } while (i < w);

        mask += 2 * mask_stride;
        src0 += src0_stride;
        src1 += src1_stride;
        dst += dst_stride;
      } while (--h != 0);
    } else {
      do {
        uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride);
        uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride);
        uint8x8_t m2 = vld1_u8(mask + 2 * mask_stride);
        uint8x8_t m3 = vld1_u8(mask + 3 * mask_stride);
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8_4(m0, m1, m2, m3));
        uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

        store_unaligned_u16_4x2(dst, dst_stride, blend);

        mask += 4 * mask_stride;
        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h != 0);
    }
  } else if (subw == 1 && subh == 0) {
    if (w >= 8) {
      do {
        int i = 0;

        do {
          uint8x8_t m0 = vld1_u8(mask + 2 * i);
          uint8x8_t m1 = vld1_u8(mask + 2 * i + 8);
          uint16x8_t s0 = vld1q_u16(src0 + i);
          uint16x8_t s1 = vld1q_u16(src1 + i);

          uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8(m0, m1));
          uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

          vst1q_u16(dst + i, blend);

          i += 8;
        } while (i < w);

        mask += mask_stride;
        src0 += src0_stride;
        src1 += src1_stride;
        dst += dst_stride;
      } while (--h != 0);
    } else {
      do {
        uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride);
        uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride);
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t m_avg = vmovl_u8(avg_blend_pairwise_u8x8(m0, m1));
        uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

        store_unaligned_u16_4x2(dst, dst_stride, blend);

        mask += 2 * mask_stride;
        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h != 0);
    }
  } else {
    if (w >= 8) {
      do {
        int i = 0;
        do {
          uint8x8_t m0 = vld1_u8(mask + 0 * mask_stride + i);
          uint8x8_t m1 = vld1_u8(mask + 1 * mask_stride + i);
          uint16x8_t s0 = vld1q_u16(src0 + i);
          uint16x8_t s1 = vld1q_u16(src1 + i);

          uint16x8_t m_avg = vmovl_u8(avg_blend_u8x8(m0, m1));
          uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

          vst1q_u16(dst + i, blend);

          i += 8;
        } while (i < w);

        mask += 2 * mask_stride;
        src0 += src0_stride;
        src1 += src1_stride;
        dst += dst_stride;
      } while (--h != 0);
    } else {
      do {
        uint8x8_t m0_2 =
            load_unaligned_u8_4x2(mask + 0 * mask_stride, 2 * mask_stride);
        uint8x8_t m1_3 =
            load_unaligned_u8_4x2(mask + 1 * mask_stride, 2 * mask_stride);
        uint16x8_t s0 = load_unaligned_u16_4x2(src0, src0_stride);
        uint16x8_t s1 = load_unaligned_u16_4x2(src1, src1_stride);

        uint16x8_t m_avg = vmovl_u8(avg_blend_u8x8(m0_2, m1_3));
        uint16x8_t blend = alpha_blend_a64_u16x8(m_avg, s0, s1);

        store_unaligned_u16_4x2(dst, dst_stride, blend);

        mask += 4 * mask_stride;
        src0 += 2 * src0_stride;
        src1 += 2 * src1_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h != 0);
    }
  }
}
