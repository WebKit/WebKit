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

#include "aom_dsp/arm/sum_neon.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static INLINE uint32_t highbd_mse8_8xh_neon_dotprod(const uint16_t *src_ptr,
                                                    int src_stride,
                                                    const uint16_t *ref_ptr,
                                                    int ref_stride, int h,
                                                    unsigned int *sse) {
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h / 2;
  do {
    uint16x8_t s0 = vld1q_u16(src_ptr);
    src_ptr += src_stride;
    uint16x8_t s1 = vld1q_u16(src_ptr);
    src_ptr += src_stride;
    uint16x8_t r0 = vld1q_u16(ref_ptr);
    ref_ptr += ref_stride;
    uint16x8_t r1 = vld1q_u16(ref_ptr);
    ref_ptr += ref_stride;

    uint8x16_t s = vcombine_u8(vmovn_u16(s0), vmovn_u16(s1));
    uint8x16_t r = vcombine_u8(vmovn_u16(r0), vmovn_u16(r1));

    uint8x16_t diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, diff, diff);
  } while (--i != 0);

  *sse = horizontal_add_u32x4(sse_u32);
  return *sse;
}

static INLINE uint32_t highbd_mse8_16xh_neon_dotprod(const uint16_t *src_ptr,
                                                     int src_stride,
                                                     const uint16_t *ref_ptr,
                                                     int ref_stride, int h,
                                                     unsigned int *sse) {
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint16x8_t s0 = vld1q_u16(src_ptr);
    uint16x8_t s1 = vld1q_u16(src_ptr + 8);
    uint16x8_t r0 = vld1q_u16(ref_ptr);
    uint16x8_t r1 = vld1q_u16(ref_ptr + 8);

    uint8x16_t s = vcombine_u8(vmovn_u16(s0), vmovn_u16(s1));
    uint8x16_t r = vcombine_u8(vmovn_u16(r0), vmovn_u16(r1));

    uint8x16_t diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, diff, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  *sse = horizontal_add_u32x4(sse_u32);
  return *sse;
}

#define HIGHBD_MSE_WXH_NEON_DOTPROD(w, h)                                 \
  uint32_t aom_highbd_8_mse##w##x##h##_neon_dotprod(                      \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,     \
      int ref_stride, uint32_t *sse) {                                    \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                         \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref_ptr);                         \
    highbd_mse8_##w##xh_neon_dotprod(src, src_stride, ref, ref_stride, h, \
                                     sse);                                \
    return *sse;                                                          \
  }

HIGHBD_MSE_WXH_NEON_DOTPROD(16, 16)
HIGHBD_MSE_WXH_NEON_DOTPROD(16, 8)
HIGHBD_MSE_WXH_NEON_DOTPROD(8, 16)
HIGHBD_MSE_WXH_NEON_DOTPROD(8, 8)

#undef HIGHBD_MSE_WXH_NEON_DOTPROD
