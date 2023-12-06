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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "config/aom_dsp_rtcd.h"

static INLINE uint64_t aom_var_2d_u8_4xh_neon_dotprod(uint8_t *src,
                                                      int src_stride, int width,
                                                      int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x2_t sum_u32 = vdup_n_u32(0);
  uint32x2_t sse_u32 = vdup_n_u32(0);

  int h = height / 2;
  do {
    int w = width;
    uint8_t *src_ptr = src;
    do {
      uint8x8_t s0 = load_unaligned_u8(src_ptr, src_stride);

      sum_u32 = vdot_u32(sum_u32, s0, vdup_n_u8(1));

      sse_u32 = vdot_u32(sse_u32, s0, s0);

      src_ptr += 8;
      w -= 8;
    } while (w >= 8);

    // Process remaining columns in the row using C.
    while (w > 0) {
      int idx = width - w;
      const uint8_t v = src[idx];
      sum += v;
      sse += v * v;
      w--;
    }

    src += 2 * src_stride;
  } while (--h != 0);

  sum += horizontal_long_add_u32x2(sum_u32);
  sse += horizontal_long_add_u32x2(sse_u32);

  return sse - sum * sum / (width * height);
}

static INLINE uint64_t aom_var_2d_u8_8xh_neon_dotprod(uint8_t *src,
                                                      int src_stride, int width,
                                                      int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x2_t sum_u32 = vdup_n_u32(0);
  uint32x2_t sse_u32 = vdup_n_u32(0);

  int h = height;
  do {
    int w = width;
    uint8_t *src_ptr = src;
    do {
      uint8x8_t s0 = vld1_u8(src_ptr);

      sum_u32 = vdot_u32(sum_u32, s0, vdup_n_u8(1));

      sse_u32 = vdot_u32(sse_u32, s0, s0);

      src_ptr += 8;
      w -= 8;
    } while (w >= 8);

    // Process remaining columns in the row using C.
    while (w > 0) {
      int idx = width - w;
      const uint8_t v = src[idx];
      sum += v;
      sse += v * v;
      w--;
    }

    src += src_stride;
  } while (--h != 0);

  sum += horizontal_long_add_u32x2(sum_u32);
  sse += horizontal_long_add_u32x2(sse_u32);

  return sse - sum * sum / (width * height);
}

static INLINE uint64_t aom_var_2d_u8_16xh_neon_dotprod(uint8_t *src,
                                                       int src_stride,
                                                       int width, int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32 = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int h = height;
  do {
    int w = width;
    uint8_t *src_ptr = src;
    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);

      sum_u32 = vdotq_u32(sum_u32, s0, vdupq_n_u8(1));

      sse_u32 = vdotq_u32(sse_u32, s0, s0);

      src_ptr += 16;
      w -= 16;
    } while (w >= 16);

    // Process remaining columns in the row using C.
    while (w > 0) {
      int idx = width - w;
      const uint8_t v = src[idx];
      sum += v;
      sse += v * v;
      w--;
    }

    src += src_stride;
  } while (--h != 0);

  sum += horizontal_long_add_u32x4(sum_u32);
  sse += horizontal_long_add_u32x4(sse_u32);

  return sse - sum * sum / (width * height);
}

uint64_t aom_var_2d_u8_neon_dotprod(uint8_t *src, int src_stride, int width,
                                    int height) {
  if (width >= 16) {
    return aom_var_2d_u8_16xh_neon_dotprod(src, src_stride, width, height);
  }
  if (width >= 8) {
    return aom_var_2d_u8_8xh_neon_dotprod(src, src_stride, width, height);
  }
  if (width >= 4 && height % 2 == 0) {
    return aom_var_2d_u8_4xh_neon_dotprod(src, src_stride, width, height);
  }
  return aom_var_2d_u8_c(src, src_stride, width, height);
}
