/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
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
#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/reinterpret_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/intrapred_common.h"

//------------------------------------------------------------------------------
// DC 4x4

static INLINE uint16x8_t dc_load_sum_4(const uint8_t *in) {
  const uint8x8_t a = load_u8_4x1(in);
  const uint16x4_t p0 = vpaddl_u8(a);
  const uint16x4_t p1 = vpadd_u16(p0, p0);
  return vcombine_u16(p1, vdup_n_u16(0));
}

static INLINE void dc_store_4xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x8_t dc) {
  for (int i = 0; i < h; ++i) {
    store_u8_4x1(dst + i * stride, dc);
  }
}

void aom_dc_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_4(above);
  const uint16x8_t sum_left = dc_load_sum_4(left);
  const uint16x8_t sum = vaddq_u16(sum_left, sum_top);
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 3);
  dc_store_4xh(dst, stride, 4, vdup_lane_u8(dc0, 0));
}

void aom_dc_left_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_left = dc_load_sum_4(left);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_left, 2);
  (void)above;
  dc_store_4xh(dst, stride, 4, vdup_lane_u8(dc0, 0));
}

void aom_dc_top_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_4(above);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_top, 2);
  (void)left;
  dc_store_4xh(dst, stride, 4, vdup_lane_u8(dc0, 0));
}

void aom_dc_128_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const uint8x8_t dc0 = vdup_n_u8(0x80);
  (void)above;
  (void)left;
  dc_store_4xh(dst, stride, 4, dc0);
}

//------------------------------------------------------------------------------
// DC 8x8

static INLINE uint16x8_t dc_load_sum_8(const uint8_t *in) {
  // This isn't used in the case where we want to load both above and left
  // vectors, since we want to avoid performing the reduction twice.
  const uint8x8_t a = vld1_u8(in);
  const uint16x4_t p0 = vpaddl_u8(a);
  const uint16x4_t p1 = vpadd_u16(p0, p0);
  const uint16x4_t p2 = vpadd_u16(p1, p1);
  return vcombine_u16(p2, vdup_n_u16(0));
}

static INLINE uint16x8_t horizontal_add_and_broadcast_u16x8(uint16x8_t a) {
#if AOM_ARCH_AARCH64
  // On AArch64 we could also use vdupq_n_u16(vaddvq_u16(a)) here to save an
  // instruction, however the addv instruction is usually slightly more
  // expensive than a pairwise addition, so the need for immediately
  // broadcasting the result again seems to negate any benefit.
  const uint16x8_t b = vpaddq_u16(a, a);
  const uint16x8_t c = vpaddq_u16(b, b);
  return vpaddq_u16(c, c);
#else
  const uint16x4_t b = vadd_u16(vget_low_u16(a), vget_high_u16(a));
  const uint16x4_t c = vpadd_u16(b, b);
  const uint16x4_t d = vpadd_u16(c, c);
  return vcombine_u16(d, d);
#endif
}

static INLINE void dc_store_8xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1_u8(dst + i * stride, dc);
  }
}

void aom_dc_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x8_t sum_top = vld1_u8(above);
  const uint8x8_t sum_left = vld1_u8(left);
  uint16x8_t sum = vaddl_u8(sum_left, sum_top);
  sum = horizontal_add_and_broadcast_u16x8(sum);
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 4);
  dc_store_8xh(dst, stride, 8, vdup_lane_u8(dc0, 0));
}

void aom_dc_left_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_left = dc_load_sum_8(left);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_left, 3);
  (void)above;
  dc_store_8xh(dst, stride, 8, vdup_lane_u8(dc0, 0));
}

void aom_dc_top_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_8(above);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_top, 3);
  (void)left;
  dc_store_8xh(dst, stride, 8, vdup_lane_u8(dc0, 0));
}

void aom_dc_128_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const uint8x8_t dc0 = vdup_n_u8(0x80);
  (void)above;
  (void)left;
  dc_store_8xh(dst, stride, 8, dc0);
}

//------------------------------------------------------------------------------
// DC 16x16

static INLINE uint16x8_t dc_load_partial_sum_16(const uint8_t *in) {
  const uint8x16_t a = vld1q_u8(in);
  // delay the remainder of the reduction until
  // horizontal_add_and_broadcast_u16x8, since we want to do it once rather
  // than twice in the case we are loading both above and left.
  return vpaddlq_u8(a);
}

static INLINE uint16x8_t dc_load_sum_16(const uint8_t *in) {
  return horizontal_add_and_broadcast_u16x8(dc_load_partial_sum_16(in));
}

static INLINE void dc_store_16xh(uint8_t *dst, ptrdiff_t stride, int h,
                                 uint8x16_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + i * stride, dc);
  }
}

void aom_dc_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_partial_sum_16(above);
  const uint16x8_t sum_left = dc_load_partial_sum_16(left);
  uint16x8_t sum = vaddq_u16(sum_left, sum_top);
  sum = horizontal_add_and_broadcast_u16x8(sum);
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 5);
  dc_store_16xh(dst, stride, 16, vdupq_lane_u8(dc0, 0));
}

void aom_dc_left_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  const uint16x8_t sum_left = dc_load_sum_16(left);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_left, 4);
  (void)above;
  dc_store_16xh(dst, stride, 16, vdupq_lane_u8(dc0, 0));
}

void aom_dc_top_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_16(above);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_top, 4);
  (void)left;
  dc_store_16xh(dst, stride, 16, vdupq_lane_u8(dc0, 0));
}

void aom_dc_128_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint8x16_t dc0 = vdupq_n_u8(0x80);
  (void)above;
  (void)left;
  dc_store_16xh(dst, stride, 16, dc0);
}

//------------------------------------------------------------------------------
// DC 32x32

static INLINE uint16x8_t dc_load_partial_sum_32(const uint8_t *in) {
  const uint8x16_t a0 = vld1q_u8(in);
  const uint8x16_t a1 = vld1q_u8(in + 16);
  // delay the remainder of the reduction until
  // horizontal_add_and_broadcast_u16x8, since we want to do it once rather
  // than twice in the case we are loading both above and left.
  return vpadalq_u8(vpaddlq_u8(a0), a1);
}

static INLINE uint16x8_t dc_load_sum_32(const uint8_t *in) {
  return horizontal_add_and_broadcast_u16x8(dc_load_partial_sum_32(in));
}

static INLINE void dc_store_32xh(uint8_t *dst, ptrdiff_t stride, int h,
                                 uint8x16_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + i * stride, dc);
    vst1q_u8(dst + i * stride + 16, dc);
  }
}

void aom_dc_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_partial_sum_32(above);
  const uint16x8_t sum_left = dc_load_partial_sum_32(left);
  uint16x8_t sum = vaddq_u16(sum_left, sum_top);
  sum = horizontal_add_and_broadcast_u16x8(sum);
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 6);
  dc_store_32xh(dst, stride, 32, vdupq_lane_u8(dc0, 0));
}

void aom_dc_left_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  const uint16x8_t sum_left = dc_load_sum_32(left);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_left, 5);
  (void)above;
  dc_store_32xh(dst, stride, 32, vdupq_lane_u8(dc0, 0));
}

void aom_dc_top_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_32(above);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_top, 5);
  (void)left;
  dc_store_32xh(dst, stride, 32, vdupq_lane_u8(dc0, 0));
}

void aom_dc_128_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint8x16_t dc0 = vdupq_n_u8(0x80);
  (void)above;
  (void)left;
  dc_store_32xh(dst, stride, 32, dc0);
}

//------------------------------------------------------------------------------
// DC 64x64

static INLINE uint16x8_t dc_load_partial_sum_64(const uint8_t *in) {
  const uint8x16_t a0 = vld1q_u8(in);
  const uint8x16_t a1 = vld1q_u8(in + 16);
  const uint8x16_t a2 = vld1q_u8(in + 32);
  const uint8x16_t a3 = vld1q_u8(in + 48);
  const uint16x8_t p01 = vpadalq_u8(vpaddlq_u8(a0), a1);
  const uint16x8_t p23 = vpadalq_u8(vpaddlq_u8(a2), a3);
  // delay the remainder of the reduction until
  // horizontal_add_and_broadcast_u16x8, since we want to do it once rather
  // than twice in the case we are loading both above and left.
  return vaddq_u16(p01, p23);
}

static INLINE uint16x8_t dc_load_sum_64(const uint8_t *in) {
  return horizontal_add_and_broadcast_u16x8(dc_load_partial_sum_64(in));
}

static INLINE void dc_store_64xh(uint8_t *dst, ptrdiff_t stride, int h,
                                 uint8x16_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + i * stride, dc);
    vst1q_u8(dst + i * stride + 16, dc);
    vst1q_u8(dst + i * stride + 32, dc);
    vst1q_u8(dst + i * stride + 48, dc);
  }
}

void aom_dc_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_partial_sum_64(above);
  const uint16x8_t sum_left = dc_load_partial_sum_64(left);
  uint16x8_t sum = vaddq_u16(sum_left, sum_top);
  sum = horizontal_add_and_broadcast_u16x8(sum);
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 7);
  dc_store_64xh(dst, stride, 64, vdupq_lane_u8(dc0, 0));
}

void aom_dc_left_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  const uint16x8_t sum_left = dc_load_sum_64(left);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_left, 6);
  (void)above;
  dc_store_64xh(dst, stride, 64, vdupq_lane_u8(dc0, 0));
}

void aom_dc_top_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint16x8_t sum_top = dc_load_sum_64(above);
  const uint8x8_t dc0 = vrshrn_n_u16(sum_top, 6);
  (void)left;
  dc_store_64xh(dst, stride, 64, vdupq_lane_u8(dc0, 0));
}

void aom_dc_128_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const uint8x16_t dc0 = vdupq_n_u8(0x80);
  (void)above;
  (void)left;
  dc_store_64xh(dst, stride, 64, dc0);
}

//------------------------------------------------------------------------------
// DC rectangular cases

#define DC_MULTIPLIER_1X2 0x5556
#define DC_MULTIPLIER_1X4 0x3334

#define DC_SHIFT2 16

static INLINE int divide_using_multiply_shift(int num, int shift1,
                                              int multiplier, int shift2) {
  const int interm = num >> shift1;
  return interm * multiplier >> shift2;
}

static INLINE int calculate_dc_from_sum(int bw, int bh, uint32_t sum,
                                        int shift1, int multiplier) {
  const int expected_dc = divide_using_multiply_shift(
      sum + ((bw + bh) >> 1), shift1, multiplier, DC_SHIFT2);
  assert(expected_dc < (1 << 8));
  return expected_dc;
}

#undef DC_SHIFT2

void aom_dc_predictor_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = load_u8_4x1(above);
  uint8x8_t l = vld1_u8(left);
  uint32_t sum = horizontal_add_u16x8(vaddl_u8(a, l));
  uint32_t dc = calculate_dc_from_sum(4, 8, sum, 2, DC_MULTIPLIER_1X2);
  dc_store_4xh(dst, stride, 8, vdup_n_u8(dc));
}

void aom_dc_predictor_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = vld1_u8(above);
  uint8x8_t l = load_u8_4x1(left);
  uint32_t sum = horizontal_add_u16x8(vaddl_u8(a, l));
  uint32_t dc = calculate_dc_from_sum(8, 4, sum, 2, DC_MULTIPLIER_1X2);
  dc_store_8xh(dst, stride, 4, vdup_n_u8(dc));
}

void aom_dc_predictor_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = load_u8_4x1(above);
  uint8x16_t l = vld1q_u8(left);
  uint16x8_t sum_al = vaddw_u8(vpaddlq_u8(l), a);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(4, 16, sum, 2, DC_MULTIPLIER_1X4);
  dc_store_4xh(dst, stride, 16, vdup_n_u8(dc));
}

void aom_dc_predictor_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x16_t a = vld1q_u8(above);
  uint8x8_t l = load_u8_4x1(left);
  uint16x8_t sum_al = vaddw_u8(vpaddlq_u8(a), l);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(16, 4, sum, 2, DC_MULTIPLIER_1X4);
  dc_store_16xh(dst, stride, 4, vdupq_n_u8(dc));
}

void aom_dc_predictor_8x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = vld1_u8(above);
  uint8x16_t l = vld1q_u8(left);
  uint16x8_t sum_al = vaddw_u8(vpaddlq_u8(l), a);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(8, 16, sum, 3, DC_MULTIPLIER_1X2);
  dc_store_8xh(dst, stride, 16, vdup_n_u8(dc));
}

void aom_dc_predictor_16x8_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x16_t a = vld1q_u8(above);
  uint8x8_t l = vld1_u8(left);
  uint16x8_t sum_al = vaddw_u8(vpaddlq_u8(a), l);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(16, 8, sum, 3, DC_MULTIPLIER_1X2);
  dc_store_16xh(dst, stride, 8, vdupq_n_u8(dc));
}

void aom_dc_predictor_8x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = vld1_u8(above);
  uint16x8_t sum_left = dc_load_partial_sum_32(left);
  uint16x8_t sum_al = vaddw_u8(sum_left, a);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(8, 32, sum, 3, DC_MULTIPLIER_1X4);
  dc_store_8xh(dst, stride, 32, vdup_n_u8(dc));
}

void aom_dc_predictor_32x8_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_top = dc_load_partial_sum_32(above);
  uint8x8_t l = vld1_u8(left);
  uint16x8_t sum_al = vaddw_u8(sum_top, l);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(32, 8, sum, 3, DC_MULTIPLIER_1X4);
  dc_store_32xh(dst, stride, 8, vdupq_n_u8(dc));
}

void aom_dc_predictor_16x32_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_16(above);
  uint16x8_t sum_left = dc_load_partial_sum_32(left);
  uint16x8_t sum_al = vaddq_u16(sum_left, sum_above);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(16, 32, sum, 4, DC_MULTIPLIER_1X2);
  dc_store_16xh(dst, stride, 32, vdupq_n_u8(dc));
}

void aom_dc_predictor_32x16_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_32(above);
  uint16x8_t sum_left = dc_load_partial_sum_16(left);
  uint16x8_t sum_al = vaddq_u16(sum_left, sum_above);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(32, 16, sum, 4, DC_MULTIPLIER_1X2);
  dc_store_32xh(dst, stride, 16, vdupq_n_u8(dc));
}

void aom_dc_predictor_16x64_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_16(above);
  uint16x8_t sum_left = dc_load_partial_sum_64(left);
  uint16x8_t sum_al = vaddq_u16(sum_left, sum_above);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(16, 64, sum, 4, DC_MULTIPLIER_1X4);
  dc_store_16xh(dst, stride, 64, vdupq_n_u8(dc));
}

void aom_dc_predictor_64x16_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_64(above);
  uint16x8_t sum_left = dc_load_partial_sum_16(left);
  uint16x8_t sum_al = vaddq_u16(sum_above, sum_left);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(64, 16, sum, 4, DC_MULTIPLIER_1X4);
  dc_store_64xh(dst, stride, 16, vdupq_n_u8(dc));
}

void aom_dc_predictor_32x64_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_32(above);
  uint16x8_t sum_left = dc_load_partial_sum_64(left);
  uint16x8_t sum_al = vaddq_u16(sum_above, sum_left);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(32, 64, sum, 5, DC_MULTIPLIER_1X2);
  dc_store_32xh(dst, stride, 64, vdupq_n_u8(dc));
}

void aom_dc_predictor_64x32_neon(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  uint16x8_t sum_above = dc_load_partial_sum_64(above);
  uint16x8_t sum_left = dc_load_partial_sum_32(left);
  uint16x8_t sum_al = vaddq_u16(sum_above, sum_left);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(64, 32, sum, 5, DC_MULTIPLIER_1X2);
  dc_store_64xh(dst, stride, 32, vdupq_n_u8(dc));
}

#undef DC_MULTIPLIER_1X2
#undef DC_MULTIPLIER_1X4

#define DC_PREDICTOR_128(w, h, q)                                            \
  void aom_dc_128_predictor_##w##x##h##_neon(uint8_t *dst, ptrdiff_t stride, \
                                             const uint8_t *above,           \
                                             const uint8_t *left) {          \
    (void)above;                                                             \
    (void)left;                                                              \
    dc_store_##w##xh(dst, stride, (h), vdup##q##_n_u8(0x80));                \
  }

DC_PREDICTOR_128(4, 8, )
DC_PREDICTOR_128(4, 16, )
DC_PREDICTOR_128(8, 4, )
DC_PREDICTOR_128(8, 16, )
DC_PREDICTOR_128(8, 32, )
DC_PREDICTOR_128(16, 4, q)
DC_PREDICTOR_128(16, 8, q)
DC_PREDICTOR_128(16, 32, q)
DC_PREDICTOR_128(16, 64, q)
DC_PREDICTOR_128(32, 8, q)
DC_PREDICTOR_128(32, 16, q)
DC_PREDICTOR_128(32, 64, q)
DC_PREDICTOR_128(64, 32, q)
DC_PREDICTOR_128(64, 16, q)

#undef DC_PREDICTOR_128

#define DC_PREDICTOR_LEFT(w, h, shift, q)                                     \
  void aom_dc_left_predictor_##w##x##h##_neon(uint8_t *dst, ptrdiff_t stride, \
                                              const uint8_t *above,           \
                                              const uint8_t *left) {          \
    (void)above;                                                              \
    const uint16x8_t sum = dc_load_sum_##h(left);                             \
    const uint8x8_t dc0 = vrshrn_n_u16(sum, (shift));                         \
    dc_store_##w##xh(dst, stride, (h), vdup##q##_lane_u8(dc0, 0));            \
  }

DC_PREDICTOR_LEFT(4, 8, 3, )
DC_PREDICTOR_LEFT(8, 4, 2, )
DC_PREDICTOR_LEFT(8, 16, 4, )
DC_PREDICTOR_LEFT(16, 8, 3, q)
DC_PREDICTOR_LEFT(16, 32, 5, q)
DC_PREDICTOR_LEFT(32, 16, 4, q)
DC_PREDICTOR_LEFT(32, 64, 6, q)
DC_PREDICTOR_LEFT(64, 32, 5, q)
DC_PREDICTOR_LEFT(4, 16, 4, )
DC_PREDICTOR_LEFT(16, 4, 2, q)
DC_PREDICTOR_LEFT(8, 32, 5, )
DC_PREDICTOR_LEFT(32, 8, 3, q)
DC_PREDICTOR_LEFT(16, 64, 6, q)
DC_PREDICTOR_LEFT(64, 16, 4, q)

#undef DC_PREDICTOR_LEFT

#define DC_PREDICTOR_TOP(w, h, shift, q)                                     \
  void aom_dc_top_predictor_##w##x##h##_neon(uint8_t *dst, ptrdiff_t stride, \
                                             const uint8_t *above,           \
                                             const uint8_t *left) {          \
    (void)left;                                                              \
    const uint16x8_t sum = dc_load_sum_##w(above);                           \
    const uint8x8_t dc0 = vrshrn_n_u16(sum, (shift));                        \
    dc_store_##w##xh(dst, stride, (h), vdup##q##_lane_u8(dc0, 0));           \
  }

DC_PREDICTOR_TOP(4, 8, 2, )
DC_PREDICTOR_TOP(4, 16, 2, )
DC_PREDICTOR_TOP(8, 4, 3, )
DC_PREDICTOR_TOP(8, 16, 3, )
DC_PREDICTOR_TOP(8, 32, 3, )
DC_PREDICTOR_TOP(16, 4, 4, q)
DC_PREDICTOR_TOP(16, 8, 4, q)
DC_PREDICTOR_TOP(16, 32, 4, q)
DC_PREDICTOR_TOP(16, 64, 4, q)
DC_PREDICTOR_TOP(32, 8, 5, q)
DC_PREDICTOR_TOP(32, 16, 5, q)
DC_PREDICTOR_TOP(32, 64, 5, q)
DC_PREDICTOR_TOP(64, 16, 6, q)
DC_PREDICTOR_TOP(64, 32, 6, q)

#undef DC_PREDICTOR_TOP

// -----------------------------------------------------------------------------

static INLINE void v_store_4xh(uint8_t *dst, ptrdiff_t stride, int h,
                               uint8x8_t d0) {
  for (int i = 0; i < h; ++i) {
    store_u8_4x1(dst + i * stride, d0);
  }
}

static INLINE void v_store_8xh(uint8_t *dst, ptrdiff_t stride, int h,
                               uint8x8_t d0) {
  for (int i = 0; i < h; ++i) {
    vst1_u8(dst + i * stride, d0);
  }
}

static INLINE void v_store_16xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x16_t d0) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + i * stride, d0);
  }
}

static INLINE void v_store_32xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x16_t d0, uint8x16_t d1) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + 0, d0);
    vst1q_u8(dst + 16, d1);
    dst += stride;
  }
}

static INLINE void v_store_64xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x16_t d0, uint8x16_t d1, uint8x16_t d2,
                                uint8x16_t d3) {
  for (int i = 0; i < h; ++i) {
    vst1q_u8(dst + 0, d0);
    vst1q_u8(dst + 16, d1);
    vst1q_u8(dst + 32, d2);
    vst1q_u8(dst + 48, d3);
    dst += stride;
  }
}

void aom_v_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_4xh(dst, stride, 4, load_u8_4x1(above));
}

void aom_v_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_8xh(dst, stride, 8, vld1_u8(above));
}

void aom_v_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_16xh(dst, stride, 16, vld1q_u8(above));
}

void aom_v_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  (void)left;
  v_store_32xh(dst, stride, 32, d0, d1);
}

void aom_v_predictor_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_4xh(dst, stride, 8, load_u8_4x1(above));
}

void aom_v_predictor_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_4xh(dst, stride, 16, load_u8_4x1(above));
}

void aom_v_predictor_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_8xh(dst, stride, 4, vld1_u8(above));
}

void aom_v_predictor_8x16_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_8xh(dst, stride, 16, vld1_u8(above));
}

void aom_v_predictor_8x32_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_8xh(dst, stride, 32, vld1_u8(above));
}

void aom_v_predictor_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_16xh(dst, stride, 4, vld1q_u8(above));
}

void aom_v_predictor_16x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_16xh(dst, stride, 8, vld1q_u8(above));
}

void aom_v_predictor_16x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_16xh(dst, stride, 32, vld1q_u8(above));
}

void aom_v_predictor_16x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_16xh(dst, stride, 64, vld1q_u8(above));
}

void aom_v_predictor_32x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  (void)left;
  v_store_32xh(dst, stride, 8, d0, d1);
}

void aom_v_predictor_32x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  (void)left;
  v_store_32xh(dst, stride, 16, d0, d1);
}

void aom_v_predictor_32x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  (void)left;
  v_store_32xh(dst, stride, 64, d0, d1);
}

void aom_v_predictor_64x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  const uint8x16_t d2 = vld1q_u8(above + 32);
  const uint8x16_t d3 = vld1q_u8(above + 48);
  (void)left;
  v_store_64xh(dst, stride, 16, d0, d1, d2, d3);
}

void aom_v_predictor_64x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  const uint8x16_t d2 = vld1q_u8(above + 32);
  const uint8x16_t d3 = vld1q_u8(above + 48);
  (void)left;
  v_store_64xh(dst, stride, 32, d0, d1, d2, d3);
}

void aom_v_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(above);
  const uint8x16_t d1 = vld1q_u8(above + 16);
  const uint8x16_t d2 = vld1q_u8(above + 32);
  const uint8x16_t d3 = vld1q_u8(above + 48);
  (void)left;
  v_store_64xh(dst, stride, 64, d0, d1, d2, d3);
}

// -----------------------------------------------------------------------------

static INLINE void h_store_4x8(uint8_t *dst, ptrdiff_t stride, uint8x8_t d0) {
  store_u8_4x1(dst + 0 * stride, vdup_lane_u8(d0, 0));
  store_u8_4x1(dst + 1 * stride, vdup_lane_u8(d0, 1));
  store_u8_4x1(dst + 2 * stride, vdup_lane_u8(d0, 2));
  store_u8_4x1(dst + 3 * stride, vdup_lane_u8(d0, 3));
  store_u8_4x1(dst + 4 * stride, vdup_lane_u8(d0, 4));
  store_u8_4x1(dst + 5 * stride, vdup_lane_u8(d0, 5));
  store_u8_4x1(dst + 6 * stride, vdup_lane_u8(d0, 6));
  store_u8_4x1(dst + 7 * stride, vdup_lane_u8(d0, 7));
}

static INLINE void h_store_8x8(uint8_t *dst, ptrdiff_t stride, uint8x8_t d0) {
  vst1_u8(dst + 0 * stride, vdup_lane_u8(d0, 0));
  vst1_u8(dst + 1 * stride, vdup_lane_u8(d0, 1));
  vst1_u8(dst + 2 * stride, vdup_lane_u8(d0, 2));
  vst1_u8(dst + 3 * stride, vdup_lane_u8(d0, 3));
  vst1_u8(dst + 4 * stride, vdup_lane_u8(d0, 4));
  vst1_u8(dst + 5 * stride, vdup_lane_u8(d0, 5));
  vst1_u8(dst + 6 * stride, vdup_lane_u8(d0, 6));
  vst1_u8(dst + 7 * stride, vdup_lane_u8(d0, 7));
}

static INLINE void h_store_16x8(uint8_t *dst, ptrdiff_t stride, uint8x8_t d0) {
  vst1q_u8(dst + 0 * stride, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 1 * stride, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 2 * stride, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 3 * stride, vdupq_lane_u8(d0, 3));
  vst1q_u8(dst + 4 * stride, vdupq_lane_u8(d0, 4));
  vst1q_u8(dst + 5 * stride, vdupq_lane_u8(d0, 5));
  vst1q_u8(dst + 6 * stride, vdupq_lane_u8(d0, 6));
  vst1q_u8(dst + 7 * stride, vdupq_lane_u8(d0, 7));
}

static INLINE void h_store_32x8(uint8_t *dst, ptrdiff_t stride, uint8x8_t d0) {
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 0));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 1));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 2));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 3));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 3));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 4));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 4));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 5));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 5));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 6));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 6));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 7));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 7));
}

static INLINE void h_store_64x8(uint8_t *dst, ptrdiff_t stride, uint8x8_t d0) {
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 0));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 1));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 2));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 3));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 3));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 3));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 3));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 4));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 4));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 4));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 4));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 5));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 5));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 5));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 5));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 6));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 6));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 6));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 6));
  dst += stride;
  vst1q_u8(dst + 0, vdupq_lane_u8(d0, 7));
  vst1q_u8(dst + 16, vdupq_lane_u8(d0, 7));
  vst1q_u8(dst + 32, vdupq_lane_u8(d0, 7));
  vst1q_u8(dst + 48, vdupq_lane_u8(d0, 7));
}

void aom_h_predictor_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = load_u8_4x1(left);
  (void)above;
  store_u8_4x1(dst + 0 * stride, vdup_lane_u8(d0, 0));
  store_u8_4x1(dst + 1 * stride, vdup_lane_u8(d0, 1));
  store_u8_4x1(dst + 2 * stride, vdup_lane_u8(d0, 2));
  store_u8_4x1(dst + 3 * stride, vdup_lane_u8(d0, 3));
}

void aom_h_predictor_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = vld1_u8(left);
  (void)above;
  h_store_8x8(dst, stride, d0);
}

void aom_h_predictor_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  (void)above;
  h_store_16x8(dst, stride, vget_low_u8(d0));
  h_store_16x8(dst + 8 * stride, stride, vget_high_u8(d0));
}

void aom_h_predictor_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  const uint8x16_t d1 = vld1q_u8(left + 16);
  (void)above;
  h_store_32x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_32x8(dst + 8 * stride, stride, vget_high_u8(d0));
  h_store_32x8(dst + 16 * stride, stride, vget_low_u8(d1));
  h_store_32x8(dst + 24 * stride, stride, vget_high_u8(d1));
}

void aom_h_predictor_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = vld1_u8(left);
  (void)above;
  h_store_4x8(dst, stride, d0);
}

void aom_h_predictor_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  (void)above;
  h_store_4x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_4x8(dst + 8 * stride, stride, vget_high_u8(d0));
}

void aom_h_predictor_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = load_u8_4x1(left);
  (void)above;
  vst1_u8(dst + 0 * stride, vdup_lane_u8(d0, 0));
  vst1_u8(dst + 1 * stride, vdup_lane_u8(d0, 1));
  vst1_u8(dst + 2 * stride, vdup_lane_u8(d0, 2));
  vst1_u8(dst + 3 * stride, vdup_lane_u8(d0, 3));
}

void aom_h_predictor_8x16_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  (void)above;
  h_store_8x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_8x8(dst + 8 * stride, stride, vget_high_u8(d0));
}

void aom_h_predictor_8x32_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  const uint8x16_t d1 = vld1q_u8(left + 16);
  (void)above;
  h_store_8x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_8x8(dst + 8 * stride, stride, vget_high_u8(d0));
  h_store_8x8(dst + 16 * stride, stride, vget_low_u8(d1));
  h_store_8x8(dst + 24 * stride, stride, vget_high_u8(d1));
}

void aom_h_predictor_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = load_u8_4x1(left);
  (void)above;
  vst1q_u8(dst + 0 * stride, vdupq_lane_u8(d0, 0));
  vst1q_u8(dst + 1 * stride, vdupq_lane_u8(d0, 1));
  vst1q_u8(dst + 2 * stride, vdupq_lane_u8(d0, 2));
  vst1q_u8(dst + 3 * stride, vdupq_lane_u8(d0, 3));
}

void aom_h_predictor_16x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = vld1_u8(left);
  (void)above;
  h_store_16x8(dst, stride, d0);
}

void aom_h_predictor_16x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  const uint8x16_t d1 = vld1q_u8(left + 16);
  (void)above;
  h_store_16x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_16x8(dst + 8 * stride, stride, vget_high_u8(d0));
  h_store_16x8(dst + 16 * stride, stride, vget_low_u8(d1));
  h_store_16x8(dst + 24 * stride, stride, vget_high_u8(d1));
}

void aom_h_predictor_16x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  const uint8x16_t d1 = vld1q_u8(left + 16);
  const uint8x16_t d2 = vld1q_u8(left + 32);
  const uint8x16_t d3 = vld1q_u8(left + 48);
  (void)above;
  h_store_16x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_16x8(dst + 8 * stride, stride, vget_high_u8(d0));
  h_store_16x8(dst + 16 * stride, stride, vget_low_u8(d1));
  h_store_16x8(dst + 24 * stride, stride, vget_high_u8(d1));
  h_store_16x8(dst + 32 * stride, stride, vget_low_u8(d2));
  h_store_16x8(dst + 40 * stride, stride, vget_high_u8(d2));
  h_store_16x8(dst + 48 * stride, stride, vget_low_u8(d3));
  h_store_16x8(dst + 56 * stride, stride, vget_high_u8(d3));
}

void aom_h_predictor_32x8_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint8x8_t d0 = vld1_u8(left);
  (void)above;
  h_store_32x8(dst, stride, d0);
}

void aom_h_predictor_32x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  (void)above;
  h_store_32x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_32x8(dst + 8 * stride, stride, vget_high_u8(d0));
}

void aom_h_predictor_32x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left + 0);
  const uint8x16_t d1 = vld1q_u8(left + 16);
  const uint8x16_t d2 = vld1q_u8(left + 32);
  const uint8x16_t d3 = vld1q_u8(left + 48);
  (void)above;
  h_store_32x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_32x8(dst + 8 * stride, stride, vget_high_u8(d0));
  h_store_32x8(dst + 16 * stride, stride, vget_low_u8(d1));
  h_store_32x8(dst + 24 * stride, stride, vget_high_u8(d1));
  h_store_32x8(dst + 32 * stride, stride, vget_low_u8(d2));
  h_store_32x8(dst + 40 * stride, stride, vget_high_u8(d2));
  h_store_32x8(dst + 48 * stride, stride, vget_low_u8(d3));
  h_store_32x8(dst + 56 * stride, stride, vget_high_u8(d3));
}

void aom_h_predictor_64x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const uint8x16_t d0 = vld1q_u8(left);
  (void)above;
  h_store_64x8(dst + 0 * stride, stride, vget_low_u8(d0));
  h_store_64x8(dst + 8 * stride, stride, vget_high_u8(d0));
}

void aom_h_predictor_64x32_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  for (int i = 0; i < 2; ++i) {
    const uint8x16_t d0 = vld1q_u8(left);
    h_store_64x8(dst + 0 * stride, stride, vget_low_u8(d0));
    h_store_64x8(dst + 8 * stride, stride, vget_high_u8(d0));
    left += 16;
    dst += 16 * stride;
  }
}

void aom_h_predictor_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  for (int i = 0; i < 4; ++i) {
    const uint8x16_t d0 = vld1q_u8(left);
    h_store_64x8(dst + 0 * stride, stride, vget_low_u8(d0));
    h_store_64x8(dst + 8 * stride, stride, vget_high_u8(d0));
    left += 16;
    dst += 16 * stride;
  }
}

/* ---------------------P R E D I C T I O N   Z 1--------------------------- */

// Low bit depth functions
static DECLARE_ALIGNED(32, uint8_t, BaseMask[33][32]) = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0,    0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0,    0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,    0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
};

static AOM_FORCE_INLINE void dr_prediction_z1_HxW_internal_neon_64(
    int H, int W, uint8x8_t *dst, const uint8_t *above, int upsample_above,
    int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((W + H) - 1) << upsample_above;

  assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint8x8_t a_mbase_x = vdup_n_u8(above[max_base_x]);

  int x = dx;
  for (int r = 0; r < W; r++) {
    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base) >> upsample_above;
    if (base_max_diff <= 0) {
      for (int i = r; i < W; ++i) {
        dst[i] = a_mbase_x;  // save 4 values
      }
      return;
    }

    if (base_max_diff > H) base_max_diff = H;

    uint8x8x2_t a01_128;
    uint16x8_t shift;
    if (upsample_above) {
      a01_128 = vld2_u8(above + base);
      shift = vdupq_n_u16(((x << upsample_above) & 0x3f) >> 1);
    } else {
      a01_128.val[0] = vld1_u8(above + base);
      a01_128.val[1] = vld1_u8(above + base + 1);
      shift = vdupq_n_u16((x & 0x3f) >> 1);
    }
    uint16x8_t diff = vsubl_u8(a01_128.val[1], a01_128.val[0]);
    uint16x8_t a32 = vmlal_u8(vdupq_n_u16(16), a01_128.val[0], vdup_n_u8(32));
    uint16x8_t res = vmlaq_u16(a32, diff, shift);

    uint8x8_t mask = vld1_u8(BaseMask[base_max_diff]);
    dst[r] = vbsl_u8(mask, vshrn_n_u16(res, 5), a_mbase_x);

    x += dx;
  }
}

static void dr_prediction_z1_4xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, int upsample_above,
                                      int dx) {
  uint8x8_t dstvec[16];

  dr_prediction_z1_HxW_internal_neon_64(4, N, dstvec, above, upsample_above,
                                        dx);
  for (int i = 0; i < N; i++) {
    vst1_lane_u32((uint32_t *)(dst + stride * i),
                  vreinterpret_u32_u8(dstvec[i]), 0);
  }
}

static void dr_prediction_z1_8xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, int upsample_above,
                                      int dx) {
  uint8x8_t dstvec[32];

  dr_prediction_z1_HxW_internal_neon_64(8, N, dstvec, above, upsample_above,
                                        dx);
  for (int i = 0; i < N; i++) {
    vst1_u8(dst + stride * i, dstvec[i]);
  }
}

static AOM_FORCE_INLINE void dr_prediction_z1_HxW_internal_neon(
    int H, int W, uint8x16_t *dst, const uint8_t *above, int upsample_above,
    int dx) {
  const int frac_bits = 6 - upsample_above;
  const int max_base_x = ((W + H) - 1) << upsample_above;

  assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);

  int x = dx;
  for (int r = 0; r < W; r++) {
    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base) >> upsample_above;
    if (base_max_diff <= 0) {
      for (int i = r; i < W; ++i) {
        dst[i] = a_mbase_x;  // save 4 values
      }
      return;
    }

    if (base_max_diff > H) base_max_diff = H;

    uint16x8_t shift;
    uint8x16_t a0_128, a1_128;
    if (upsample_above) {
      uint8x8x2_t v_tmp_a0_128 = vld2_u8(above + base);
      a0_128 = vcombine_u8(v_tmp_a0_128.val[0], v_tmp_a0_128.val[1]);
      a1_128 = vextq_u8(a0_128, vdupq_n_u8(0), 8);
      shift = vdupq_n_u16(x & 0x1f);
    } else {
      a0_128 = vld1q_u8(above + base);
      a1_128 = vld1q_u8(above + base + 1);
      shift = vdupq_n_u16((x & 0x3f) >> 1);
    }
    uint16x8_t diff_lo = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
    uint16x8_t diff_hi = vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
    uint16x8_t a32_lo =
        vmlal_u8(vdupq_n_u16(16), vget_low_u8(a0_128), vdup_n_u8(32));
    uint16x8_t a32_hi =
        vmlal_u8(vdupq_n_u16(16), vget_high_u8(a0_128), vdup_n_u8(32));
    uint16x8_t res_lo = vmlaq_u16(a32_lo, diff_lo, shift);
    uint16x8_t res_hi = vmlaq_u16(a32_hi, diff_hi, shift);
    uint8x16_t v_temp =
        vcombine_u8(vshrn_n_u16(res_lo, 5), vshrn_n_u16(res_hi, 5));

    uint8x16_t mask = vld1q_u8(BaseMask[base_max_diff]);
    dst[r] = vbslq_u8(mask, v_temp, a_mbase_x);

    x += dx;
  }
}

static void dr_prediction_z1_16xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  uint8x16_t dstvec[64];

  dr_prediction_z1_HxW_internal_neon(16, N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    vst1q_u8(dst + stride * i, dstvec[i]);
  }
}

static AOM_FORCE_INLINE void dr_prediction_z1_32xN_internal_neon(
    int N, uint8x16x2_t *dstvec, const uint8_t *above, int dx) {
  const int frac_bits = 6;
  const int max_base_x = ((32 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);

  int x = dx;
  for (int r = 0; r < N; r++) {
    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base);
    if (base_max_diff <= 0) {
      for (int i = r; i < N; ++i) {
        dstvec[i].val[0] = a_mbase_x;  // save 32 values
        dstvec[i].val[1] = a_mbase_x;
      }
      return;
    }
    if (base_max_diff > 32) base_max_diff = 32;

    uint16x8_t shift = vdupq_n_u16((x & 0x3f) >> 1);

    uint8x16_t res16[2];
    for (int j = 0, jj = 0; j < 32; j += 16, jj++) {
      int mdiff = base_max_diff - j;
      if (mdiff <= 0) {
        res16[jj] = a_mbase_x;
      } else {
        uint8x16_t a0_128 = vld1q_u8(above + base + j);
        uint8x16_t a1_128 = vld1q_u8(above + base + j + 1);
        uint16x8_t diff_lo = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
        uint16x8_t diff_hi =
            vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
        uint16x8_t a32_lo =
            vmlal_u8(vdupq_n_u16(16), vget_low_u8(a0_128), vdup_n_u8(32));
        uint16x8_t a32_hi =
            vmlal_u8(vdupq_n_u16(16), vget_high_u8(a0_128), vdup_n_u8(32));
        uint16x8_t res_lo = vmlaq_u16(a32_lo, diff_lo, shift);
        uint16x8_t res_hi = vmlaq_u16(a32_hi, diff_hi, shift);

        res16[jj] = vcombine_u8(vshrn_n_u16(res_lo, 5), vshrn_n_u16(res_hi, 5));
      }
    }

    uint8x16_t mask_lo = vld1q_u8(BaseMask[base_max_diff]);
    uint8x16_t mask_hi = vld1q_u8(BaseMask[base_max_diff] + 16);
    dstvec[r].val[0] = vbslq_u8(mask_lo, res16[0], a_mbase_x);
    dstvec[r].val[1] = vbslq_u8(mask_hi, res16[1], a_mbase_x);
    x += dx;
  }
}

static void dr_prediction_z1_32xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int dx) {
  uint8x16x2_t dstvec[64];

  dr_prediction_z1_32xN_internal_neon(N, dstvec, above, dx);
  for (int i = 0; i < N; i++) {
    vst1q_u8(dst + stride * i, dstvec[i].val[0]);
    vst1q_u8(dst + stride * i + 16, dstvec[i].val[1]);
  }
}

// clang-format off
static const uint8_t kLoadMaxShuffles[] = {
  15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
   9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
   8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15,
   7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15,
   6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15,
   5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15,
   4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15,
   3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15, 15,
   2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15, 15,
   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
};
// clang-format on

static INLINE uint8x16_t z1_load_masked_neon(const uint8_t *ptr,
                                             int shuffle_idx) {
  uint8x16_t shuffle = vld1q_u8(&kLoadMaxShuffles[16 * shuffle_idx]);
  uint8x16_t src = vld1q_u8(ptr);
#if AOM_ARCH_AARCH64
  return vqtbl1q_u8(src, shuffle);
#else
  uint8x8x2_t src2 = { { vget_low_u8(src), vget_high_u8(src) } };
  uint8x8_t lo = vtbl2_u8(src2, vget_low_u8(shuffle));
  uint8x8_t hi = vtbl2_u8(src2, vget_high_u8(shuffle));
  return vcombine_u8(lo, hi);
#endif
}

static void dr_prediction_z1_64xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int dx) {
  const int frac_bits = 6;
  const int max_base_x = ((64 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);

  int x = dx;
  for (int r = 0; r < N; r++, dst += stride) {
    int base = x >> frac_bits;
    if (base >= max_base_x) {
      for (int i = r; i < N; ++i) {
        vst1q_u8(dst, a_mbase_x);
        vst1q_u8(dst + 16, a_mbase_x);
        vst1q_u8(dst + 32, a_mbase_x);
        vst1q_u8(dst + 48, a_mbase_x);
        dst += stride;
      }
      return;
    }

    uint16x8_t shift = vdupq_n_u16((x & 0x3f) >> 1);
    uint8x16_t base_inc128 =
        vaddq_u8(vdupq_n_u8(base), vcombine_u8(vcreate_u8(0x0706050403020100),
                                               vcreate_u8(0x0F0E0D0C0B0A0908)));

    for (int j = 0; j < 64; j += 16) {
      if (base + j >= max_base_x) {
        vst1q_u8(dst + j, a_mbase_x);
      } else {
        uint8x16_t a0_128;
        uint8x16_t a1_128;
        if (base + j + 15 >= max_base_x) {
          int shuffle_idx = max_base_x - base - j;
          a0_128 = z1_load_masked_neon(above + (max_base_x - 15), shuffle_idx);
        } else {
          a0_128 = vld1q_u8(above + base + j);
        }
        if (base + j + 16 >= max_base_x) {
          int shuffle_idx = max_base_x - base - j - 1;
          a1_128 = z1_load_masked_neon(above + (max_base_x - 15), shuffle_idx);
        } else {
          a1_128 = vld1q_u8(above + base + j + 1);
        }

        uint16x8_t diff_lo = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
        uint16x8_t diff_hi =
            vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
        uint16x8_t a32_lo =
            vmlal_u8(vdupq_n_u16(16), vget_low_u8(a0_128), vdup_n_u8(32));
        uint16x8_t a32_hi =
            vmlal_u8(vdupq_n_u16(16), vget_high_u8(a0_128), vdup_n_u8(32));
        uint16x8_t res_lo = vmlaq_u16(a32_lo, diff_lo, shift);
        uint16x8_t res_hi = vmlaq_u16(a32_hi, diff_hi, shift);
        vst1q_u8(dst + j,
                 vcombine_u8(vshrn_n_u16(res_lo, 5), vshrn_n_u16(res_hi, 5)));

        base_inc128 = vaddq_u8(base_inc128, vdupq_n_u8(16));
      }
    }
    x += dx;
  }
}

// Directional prediction, zone 1: 0 < angle < 90
void av1_dr_prediction_z1_neon(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_above, int dx, int dy) {
  (void)left;
  (void)dy;

  switch (bw) {
    case 4:
      dr_prediction_z1_4xN_neon(bh, dst, stride, above, upsample_above, dx);
      break;
    case 8:
      dr_prediction_z1_8xN_neon(bh, dst, stride, above, upsample_above, dx);
      break;
    case 16:
      dr_prediction_z1_16xN_neon(bh, dst, stride, above, upsample_above, dx);
      break;
    case 32: dr_prediction_z1_32xN_neon(bh, dst, stride, above, dx); break;
    case 64: dr_prediction_z1_64xN_neon(bh, dst, stride, above, dx); break;
    default: break;
  }
}

/* ---------------------P R E D I C T I O N   Z 2--------------------------- */

#if !AOM_ARCH_AARCH64
static DECLARE_ALIGNED(16, uint8_t, LoadMaskz2[4][16]) = {
  { 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff }
};
#endif  // !AOM_ARCH_AARCH64

static AOM_FORCE_INLINE void dr_prediction_z2_Nx4_above_neon(
    const uint8_t *above, int upsample_above, int dx, int base_x, int y,
    uint8x8_t *a0_x, uint8x8_t *a1_x, uint16x4_t *shift0) {
  uint16x4_t r6 = vcreate_u16(0x00C0008000400000);
  uint16x4_t ydx = vdup_n_u16(y * dx);
  if (upsample_above) {
    // Cannot use LD2 here since we only want to load eight bytes, but LD2 can
    // only load either 16 or 32.
    uint8x8_t v_tmp = vld1_u8(above + base_x);
    *a0_x = vuzp_u8(v_tmp, vdup_n_u8(0)).val[0];
    *a1_x = vuzp_u8(v_tmp, vdup_n_u8(0)).val[1];
    *shift0 = vand_u16(vsub_u16(r6, ydx), vdup_n_u16(0x1f));
  } else {
    *a0_x = load_unaligned_u8_4x1(above + base_x);
    *a1_x = load_unaligned_u8_4x1(above + base_x + 1);
    *shift0 = vand_u16(vhsub_u16(r6, ydx), vdup_n_u16(0x1f));
  }
}

static AOM_FORCE_INLINE void dr_prediction_z2_Nx4_left_neon(
#if AOM_ARCH_AARCH64
    uint8x16x2_t left_vals,
#else
    const uint8_t *left,
#endif
    int upsample_left, int dy, int r, int min_base_y, int frac_bits_y,
    uint16x4_t *a0_y, uint16x4_t *a1_y, uint16x4_t *shift1) {
  int16x4_t dy64 = vdup_n_s16(dy);
  int16x4_t v_1234 = vcreate_s16(0x0004000300020001);
  int16x4_t v_frac_bits_y = vdup_n_s16(-frac_bits_y);
  int16x4_t min_base_y64 = vdup_n_s16(min_base_y);
  int16x4_t v_r6 = vdup_n_s16(r << 6);
  int16x4_t y_c64 = vmls_s16(v_r6, v_1234, dy64);
  int16x4_t base_y_c64 = vshl_s16(y_c64, v_frac_bits_y);

  // Values in base_y_c64 range from -2 through 14 inclusive.
  base_y_c64 = vmax_s16(base_y_c64, min_base_y64);

#if AOM_ARCH_AARCH64
  uint8x8_t left_idx0 =
      vreinterpret_u8_s16(vadd_s16(base_y_c64, vdup_n_s16(2)));  // [0, 16]
  uint8x8_t left_idx1 =
      vreinterpret_u8_s16(vadd_s16(base_y_c64, vdup_n_s16(3)));  // [1, 17]

  *a0_y = vreinterpret_u16_u8(vqtbl2_u8(left_vals, left_idx0));
  *a1_y = vreinterpret_u16_u8(vqtbl2_u8(left_vals, left_idx1));
#else   // !AOM_ARCH_AARCH64
  DECLARE_ALIGNED(32, int16_t, base_y_c[4]);

  vst1_s16(base_y_c, base_y_c64);
  uint8x8_t a0_y_u8 = vdup_n_u8(0);
  a0_y_u8 = vld1_lane_u8(left + base_y_c[0], a0_y_u8, 0);
  a0_y_u8 = vld1_lane_u8(left + base_y_c[1], a0_y_u8, 2);
  a0_y_u8 = vld1_lane_u8(left + base_y_c[2], a0_y_u8, 4);
  a0_y_u8 = vld1_lane_u8(left + base_y_c[3], a0_y_u8, 6);

  base_y_c64 = vadd_s16(base_y_c64, vdup_n_s16(1));
  vst1_s16(base_y_c, base_y_c64);
  uint8x8_t a1_y_u8 = vdup_n_u8(0);
  a1_y_u8 = vld1_lane_u8(left + base_y_c[0], a1_y_u8, 0);
  a1_y_u8 = vld1_lane_u8(left + base_y_c[1], a1_y_u8, 2);
  a1_y_u8 = vld1_lane_u8(left + base_y_c[2], a1_y_u8, 4);
  a1_y_u8 = vld1_lane_u8(left + base_y_c[3], a1_y_u8, 6);

  *a0_y = vreinterpret_u16_u8(a0_y_u8);
  *a1_y = vreinterpret_u16_u8(a1_y_u8);
#endif  // AOM_ARCH_AARCH64

  if (upsample_left) {
    *shift1 = vand_u16(vreinterpret_u16_s16(y_c64), vdup_n_u16(0x1f));
  } else {
    *shift1 =
        vand_u16(vshr_n_u16(vreinterpret_u16_s16(y_c64), 1), vdup_n_u16(0x1f));
  }
}

static AOM_FORCE_INLINE uint8x8_t dr_prediction_z2_Nx8_above_neon(
    const uint8_t *above, int upsample_above, int dx, int base_x, int y) {
  uint16x8_t c1234 = vcombine_u16(vcreate_u16(0x0004000300020001),
                                  vcreate_u16(0x0008000700060005));
  uint16x8_t ydx = vdupq_n_u16(y * dx);
  uint16x8_t r6 = vshlq_n_u16(vextq_u16(c1234, vdupq_n_u16(0), 2), 6);

  uint16x8_t shift0;
  uint8x8_t a0_x0;
  uint8x8_t a1_x0;
  if (upsample_above) {
    uint8x8x2_t v_tmp = vld2_u8(above + base_x);
    a0_x0 = v_tmp.val[0];
    a1_x0 = v_tmp.val[1];
    shift0 = vandq_u16(vsubq_u16(r6, ydx), vdupq_n_u16(0x1f));
  } else {
    a0_x0 = vld1_u8(above + base_x);
    a1_x0 = vld1_u8(above + base_x + 1);
    shift0 = vandq_u16(vhsubq_u16(r6, ydx), vdupq_n_u16(0x1f));
  }

  uint16x8_t diff0 = vsubl_u8(a1_x0, a0_x0);  // a[x+1] - a[x]
  uint16x8_t a32 =
      vmlal_u8(vdupq_n_u16(16), a0_x0, vdup_n_u8(32));  // a[x] * 32 + 16
  uint16x8_t res = vmlaq_u16(a32, diff0, shift0);
  return vshrn_n_u16(res, 5);
}

static AOM_FORCE_INLINE uint8x8_t dr_prediction_z2_Nx8_left_neon(
#if AOM_ARCH_AARCH64
    uint8x16x3_t left_vals,
#else
    const uint8_t *left,
#endif
    int upsample_left, int dy, int r, int min_base_y, int frac_bits_y) {
  int16x8_t v_r6 = vdupq_n_s16(r << 6);
  int16x8_t dy128 = vdupq_n_s16(dy);
  int16x8_t v_frac_bits_y = vdupq_n_s16(-frac_bits_y);
  int16x8_t min_base_y128 = vdupq_n_s16(min_base_y);

  uint16x8_t c1234 = vcombine_u16(vcreate_u16(0x0004000300020001),
                                  vcreate_u16(0x0008000700060005));
  int16x8_t y_c128 = vmlsq_s16(v_r6, vreinterpretq_s16_u16(c1234), dy128);
  int16x8_t base_y_c128 = vshlq_s16(y_c128, v_frac_bits_y);

  // Values in base_y_c128 range from -2 through 31 inclusive.
  base_y_c128 = vmaxq_s16(base_y_c128, min_base_y128);

#if AOM_ARCH_AARCH64
  uint8x16_t left_idx0 =
      vreinterpretq_u8_s16(vaddq_s16(base_y_c128, vdupq_n_s16(2)));  // [0, 33]
  uint8x16_t left_idx1 =
      vreinterpretq_u8_s16(vaddq_s16(base_y_c128, vdupq_n_s16(3)));  // [1, 34]
  uint8x16_t left_idx01 = vuzp1q_u8(left_idx0, left_idx1);

  uint8x16_t a01_x = vqtbl3q_u8(left_vals, left_idx01);
  uint8x8_t a0_x1 = vget_low_u8(a01_x);
  uint8x8_t a1_x1 = vget_high_u8(a01_x);
#else   // !AOM_ARCH_AARCH64
  uint8x8_t a0_x1 = load_u8_gather_s16_x8(left, base_y_c128);
  uint8x8_t a1_x1 = load_u8_gather_s16_x8(left + 1, base_y_c128);
#endif  // AOM_ARCH_AARCH64

  uint16x8_t shift1;
  if (upsample_left) {
    shift1 = vandq_u16(vreinterpretq_u16_s16(y_c128), vdupq_n_u16(0x1f));
  } else {
    shift1 = vshrq_n_u16(
        vandq_u16(vreinterpretq_u16_s16(y_c128), vdupq_n_u16(0x3f)), 1);
  }

  uint16x8_t diff1 = vsubl_u8(a1_x1, a0_x1);
  uint16x8_t a32 = vmlal_u8(vdupq_n_u16(16), a0_x1, vdup_n_u8(32));
  uint16x8_t res = vmlaq_u16(a32, diff1, shift1);
  return vshrn_n_u16(res, 5);
}

static AOM_FORCE_INLINE uint8x16_t dr_prediction_z2_NxW_above_neon(
    const uint8_t *above, int dx, int base_x, int y, int j) {
  uint16x8x2_t c0123 = { { vcombine_u16(vcreate_u16(0x0003000200010000),
                                        vcreate_u16(0x0007000600050004)),
                           vcombine_u16(vcreate_u16(0x000B000A00090008),
                                        vcreate_u16(0x000F000E000D000C)) } };
  uint16x8_t j256 = vdupq_n_u16(j);
  uint16x8_t ydx = vdupq_n_u16((uint16_t)(y * dx));

  const uint8x16_t a0_x128 = vld1q_u8(above + base_x + j);
  const uint8x16_t a1_x128 = vld1q_u8(above + base_x + j + 1);
  uint16x8_t res6_0 = vshlq_n_u16(vaddq_u16(c0123.val[0], j256), 6);
  uint16x8_t res6_1 = vshlq_n_u16(vaddq_u16(c0123.val[1], j256), 6);
  uint16x8_t shift0 =
      vshrq_n_u16(vandq_u16(vsubq_u16(res6_0, ydx), vdupq_n_u16(0x3f)), 1);
  uint16x8_t shift1 =
      vshrq_n_u16(vandq_u16(vsubq_u16(res6_1, ydx), vdupq_n_u16(0x3f)), 1);
  // a[x+1] - a[x]
  uint16x8_t diff0 = vsubl_u8(vget_low_u8(a1_x128), vget_low_u8(a0_x128));
  uint16x8_t diff1 = vsubl_u8(vget_high_u8(a1_x128), vget_high_u8(a0_x128));
  // a[x] * 32 + 16
  uint16x8_t a32_0 =
      vmlal_u8(vdupq_n_u16(16), vget_low_u8(a0_x128), vdup_n_u8(32));
  uint16x8_t a32_1 =
      vmlal_u8(vdupq_n_u16(16), vget_high_u8(a0_x128), vdup_n_u8(32));
  uint16x8_t res0 = vmlaq_u16(a32_0, diff0, shift0);
  uint16x8_t res1 = vmlaq_u16(a32_1, diff1, shift1);
  return vcombine_u8(vshrn_n_u16(res0, 5), vshrn_n_u16(res1, 5));
}

static AOM_FORCE_INLINE uint8x16_t dr_prediction_z2_NxW_left_neon(
#if AOM_ARCH_AARCH64
    uint8x16x4_t left_vals0, uint8x16x4_t left_vals1,
#else
    const uint8_t *left,
#endif
    int dy, int r, int j) {
  // here upsample_above and upsample_left are 0 by design of
  // av1_use_intra_edge_upsample
  const int min_base_y = -1;

  int16x8_t min_base_y256 = vdupq_n_s16(min_base_y);
  int16x8_t half_min_base_y256 = vdupq_n_s16(min_base_y >> 1);
  int16x8_t dy256 = vdupq_n_s16(dy);
  uint16x8_t j256 = vdupq_n_u16(j);

  uint16x8x2_t c0123 = { { vcombine_u16(vcreate_u16(0x0003000200010000),
                                        vcreate_u16(0x0007000600050004)),
                           vcombine_u16(vcreate_u16(0x000B000A00090008),
                                        vcreate_u16(0x000F000E000D000C)) } };
  uint16x8x2_t c1234 = { { vaddq_u16(c0123.val[0], vdupq_n_u16(1)),
                           vaddq_u16(c0123.val[1], vdupq_n_u16(1)) } };

  int16x8_t v_r6 = vdupq_n_s16(r << 6);

  int16x8_t c256_0 = vreinterpretq_s16_u16(vaddq_u16(j256, c1234.val[0]));
  int16x8_t c256_1 = vreinterpretq_s16_u16(vaddq_u16(j256, c1234.val[1]));
  int16x8_t mul16_lo = vreinterpretq_s16_u16(
      vminq_u16(vreinterpretq_u16_s16(vmulq_s16(c256_0, dy256)),
                vreinterpretq_u16_s16(half_min_base_y256)));
  int16x8_t mul16_hi = vreinterpretq_s16_u16(
      vminq_u16(vreinterpretq_u16_s16(vmulq_s16(c256_1, dy256)),
                vreinterpretq_u16_s16(half_min_base_y256)));
  int16x8_t y_c256_lo = vsubq_s16(v_r6, mul16_lo);
  int16x8_t y_c256_hi = vsubq_s16(v_r6, mul16_hi);

  int16x8_t base_y_c256_lo = vshrq_n_s16(y_c256_lo, 6);
  int16x8_t base_y_c256_hi = vshrq_n_s16(y_c256_hi, 6);

  base_y_c256_lo = vmaxq_s16(min_base_y256, base_y_c256_lo);
  base_y_c256_hi = vmaxq_s16(min_base_y256, base_y_c256_hi);

#if !AOM_ARCH_AARCH64
  int16_t min_y = vgetq_lane_s16(base_y_c256_hi, 7);
  int16_t max_y = vgetq_lane_s16(base_y_c256_lo, 0);
  int16_t offset_diff = max_y - min_y;

  uint8x8_t a0_y0;
  uint8x8_t a0_y1;
  uint8x8_t a1_y0;
  uint8x8_t a1_y1;
  if (offset_diff < 16) {
    // Avoid gathers where the data we want is close together in memory.
    // We don't need this for AArch64 since we can already use TBL to cover the
    // full range of possible values.
    assert(offset_diff >= 0);
    int16x8_t min_y256 = vdupq_lane_s16(vget_high_s16(base_y_c256_hi), 3);

    int16x8x2_t base_y_offset;
    base_y_offset.val[0] = vsubq_s16(base_y_c256_lo, min_y256);
    base_y_offset.val[1] = vsubq_s16(base_y_c256_hi, min_y256);

    int8x16_t base_y_offset128 = vcombine_s8(vqmovn_s16(base_y_offset.val[0]),
                                             vqmovn_s16(base_y_offset.val[1]));

    uint8x16_t v_loadmaskz2 = vld1q_u8(LoadMaskz2[offset_diff / 4]);
    uint8x16_t a0_y128 = vld1q_u8(left + min_y);
    uint8x16_t a1_y128 = vld1q_u8(left + min_y + 1);
    a0_y128 = vandq_u8(a0_y128, v_loadmaskz2);
    a1_y128 = vandq_u8(a1_y128, v_loadmaskz2);

    uint8x8_t v_index_low = vget_low_u8(vreinterpretq_u8_s8(base_y_offset128));
    uint8x8_t v_index_high =
        vget_high_u8(vreinterpretq_u8_s8(base_y_offset128));
    uint8x8x2_t v_tmp, v_res;
    v_tmp.val[0] = vget_low_u8(a0_y128);
    v_tmp.val[1] = vget_high_u8(a0_y128);
    v_res.val[0] = vtbl2_u8(v_tmp, v_index_low);
    v_res.val[1] = vtbl2_u8(v_tmp, v_index_high);
    a0_y128 = vcombine_u8(v_res.val[0], v_res.val[1]);
    v_tmp.val[0] = vget_low_u8(a1_y128);
    v_tmp.val[1] = vget_high_u8(a1_y128);
    v_res.val[0] = vtbl2_u8(v_tmp, v_index_low);
    v_res.val[1] = vtbl2_u8(v_tmp, v_index_high);
    a1_y128 = vcombine_u8(v_res.val[0], v_res.val[1]);

    a0_y0 = vget_low_u8(a0_y128);
    a0_y1 = vget_high_u8(a0_y128);
    a1_y0 = vget_low_u8(a1_y128);
    a1_y1 = vget_high_u8(a1_y128);
  } else {
    a0_y0 = load_u8_gather_s16_x8(left, base_y_c256_lo);
    a0_y1 = load_u8_gather_s16_x8(left, base_y_c256_hi);
    a1_y0 = load_u8_gather_s16_x8(left + 1, base_y_c256_lo);
    a1_y1 = load_u8_gather_s16_x8(left + 1, base_y_c256_hi);
  }
#else
  // Values in left_idx{0,1} range from 0 through 63 inclusive.
  uint8x16_t left_idx0 =
      vreinterpretq_u8_s16(vaddq_s16(base_y_c256_lo, vdupq_n_s16(1)));
  uint8x16_t left_idx1 =
      vreinterpretq_u8_s16(vaddq_s16(base_y_c256_hi, vdupq_n_s16(1)));
  uint8x16_t left_idx01 = vuzp1q_u8(left_idx0, left_idx1);

  uint8x16_t a0_y01 = vqtbl4q_u8(left_vals0, left_idx01);
  uint8x16_t a1_y01 = vqtbl4q_u8(left_vals1, left_idx01);

  uint8x8_t a0_y0 = vget_low_u8(a0_y01);
  uint8x8_t a0_y1 = vget_high_u8(a0_y01);
  uint8x8_t a1_y0 = vget_low_u8(a1_y01);
  uint8x8_t a1_y1 = vget_high_u8(a1_y01);
#endif  // !AOM_ARCH_AARCH64

  uint16x8_t shifty_lo = vshrq_n_u16(
      vandq_u16(vreinterpretq_u16_s16(y_c256_lo), vdupq_n_u16(0x3f)), 1);
  uint16x8_t shifty_hi = vshrq_n_u16(
      vandq_u16(vreinterpretq_u16_s16(y_c256_hi), vdupq_n_u16(0x3f)), 1);

  // a[x+1] - a[x]
  uint16x8_t diff_lo = vsubl_u8(a1_y0, a0_y0);
  uint16x8_t diff_hi = vsubl_u8(a1_y1, a0_y1);
  // a[x] * 32 + 16
  uint16x8_t a32_lo = vmlal_u8(vdupq_n_u16(16), a0_y0, vdup_n_u8(32));
  uint16x8_t a32_hi = vmlal_u8(vdupq_n_u16(16), a0_y1, vdup_n_u8(32));

  uint16x8_t res0 = vmlaq_u16(a32_lo, diff_lo, shifty_lo);
  uint16x8_t res1 = vmlaq_u16(a32_hi, diff_hi, shifty_hi);

  return vcombine_u8(vshrn_n_u16(res0, 5), vshrn_n_u16(res1, 5));
}

static void dr_prediction_z2_Nx4_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, const uint8_t *left,
                                      int upsample_above, int upsample_left,
                                      int dx, int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  assert(dx > 0);
  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

#if AOM_ARCH_AARCH64
  // Use ext rather than loading left + 14 directly to avoid over-read.
  const uint8x16_t left_m2 = vld1q_u8(left - 2);
  const uint8x16_t left_0 = vld1q_u8(left);
  const uint8x16_t left_14 = vextq_u8(left_0, left_0, 14);
  const uint8x16x2_t left_vals = { { left_m2, left_14 } };
#define LEFT left_vals
#else  // !AOM_ARCH_AARCH64
#define LEFT left
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < N; r++) {
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    const int base_min_diff =
        (min_base_x - ((-y * dx) >> frac_bits_x) + upsample_above) >>
        upsample_above;

    if (base_min_diff <= 0) {
      uint8x8_t a0_x_u8, a1_x_u8;
      uint16x4_t shift0;
      dr_prediction_z2_Nx4_above_neon(above, upsample_above, dx, base_x, y,
                                      &a0_x_u8, &a1_x_u8, &shift0);
      uint8x8_t a0_x = a0_x_u8;
      uint8x8_t a1_x = a1_x_u8;

      uint16x8_t diff = vsubl_u8(a1_x, a0_x);  // a[x+1] - a[x]
      uint16x8_t a32 =
          vmlal_u8(vdupq_n_u16(16), a0_x, vdup_n_u8(32));  // a[x] * 32 + 16
      uint16x8_t res =
          vmlaq_u16(a32, diff, vcombine_u16(shift0, vdup_n_u16(0)));
      uint8x8_t resx = vshrn_n_u16(res, 5);
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(resx), 0);
    } else if (base_min_diff < 4) {
      uint8x8_t a0_x_u8, a1_x_u8;
      uint16x4_t shift0;
      dr_prediction_z2_Nx4_above_neon(above, upsample_above, dx, base_x, y,
                                      &a0_x_u8, &a1_x_u8, &shift0);
      uint16x8_t a0_x = vmovl_u8(a0_x_u8);
      uint16x8_t a1_x = vmovl_u8(a1_x_u8);

      uint16x4_t a0_y;
      uint16x4_t a1_y;
      uint16x4_t shift1;
      dr_prediction_z2_Nx4_left_neon(LEFT, upsample_left, dy, r, min_base_y,
                                     frac_bits_y, &a0_y, &a1_y, &shift1);
      a0_x = vcombine_u16(vget_low_u16(a0_x), a0_y);
      a1_x = vcombine_u16(vget_low_u16(a1_x), a1_y);

      uint16x8_t shift = vcombine_u16(shift0, shift1);
      uint16x8_t diff = vsubq_u16(a1_x, a0_x);  // a[x+1] - a[x]
      uint16x8_t a32 =
          vmlaq_n_u16(vdupq_n_u16(16), a0_x, 32);  // a[x] * 32 + 16
      uint16x8_t res = vmlaq_u16(a32, diff, shift);
      uint8x8_t resx = vshrn_n_u16(res, 5);
      uint8x8_t resy = vext_u8(resx, vdup_n_u8(0), 4);

      uint8x8_t mask = vld1_u8(BaseMask[base_min_diff]);
      uint8x8_t v_resxy = vbsl_u8(mask, resy, resx);
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(v_resxy), 0);
    } else {
      uint16x4_t a0_y, a1_y;
      uint16x4_t shift1;
      dr_prediction_z2_Nx4_left_neon(LEFT, upsample_left, dy, r, min_base_y,
                                     frac_bits_y, &a0_y, &a1_y, &shift1);
      uint16x4_t diff = vsub_u16(a1_y, a0_y);                 // a[x+1] - a[x]
      uint16x4_t a32 = vmla_n_u16(vdup_n_u16(16), a0_y, 32);  // a[x] * 32 + 16
      uint16x4_t res = vmla_u16(a32, diff, shift1);
      uint8x8_t resy = vshrn_n_u16(vcombine_u16(res, vdup_n_u16(0)), 5);

      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(resy), 0);
    }

    dst += stride;
  }
#undef LEFT
}

static void dr_prediction_z2_Nx8_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, const uint8_t *left,
                                      int upsample_above, int upsample_left,
                                      int dx, int dy) {
  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

#if AOM_ARCH_AARCH64
  // Use ext rather than loading left + 30 directly to avoid over-read.
  const uint8x16_t left_m2 = vld1q_u8(left - 2);
  const uint8x16_t left_0 = vld1q_u8(left + 0);
  const uint8x16_t left_16 = vld1q_u8(left + 16);
  const uint8x16_t left_14 = vextq_u8(left_0, left_16, 14);
  const uint8x16_t left_30 = vextq_u8(left_16, left_16, 14);
  const uint8x16x3_t left_vals = { { left_m2, left_14, left_30 } };
#define LEFT left_vals
#else  // !AOM_ARCH_AARCH64
#define LEFT left
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < N; r++) {
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;

    if (base_min_diff <= 0) {
      uint8x8_t resx =
          dr_prediction_z2_Nx8_above_neon(above, upsample_above, dx, base_x, y);
      vst1_u8(dst, resx);
    } else if (base_min_diff < 8) {
      uint8x8_t resx =
          dr_prediction_z2_Nx8_above_neon(above, upsample_above, dx, base_x, y);
      uint8x8_t resy = dr_prediction_z2_Nx8_left_neon(
          LEFT, upsample_left, dy, r, min_base_y, frac_bits_y);
      uint8x8_t mask = vld1_u8(BaseMask[base_min_diff]);
      uint8x8_t resxy = vbsl_u8(mask, resy, resx);
      vst1_u8(dst, resxy);
    } else {
      uint8x8_t resy = dr_prediction_z2_Nx8_left_neon(
          LEFT, upsample_left, dy, r, min_base_y, frac_bits_y);
      vst1_u8(dst, resy);
    }

    dst += stride;
  }
#undef LEFT
}

static void dr_prediction_z2_HxW_neon(int H, int W, uint8_t *dst,
                                      ptrdiff_t stride, const uint8_t *above,
                                      const uint8_t *left, int dx, int dy) {
  // here upsample_above and upsample_left are 0 by design of
  // av1_use_intra_edge_upsample
  const int min_base_x = -1;

#if AOM_ARCH_AARCH64
  const uint8x16_t left_m1 = vld1q_u8(left - 1);
  const uint8x16_t left_0 = vld1q_u8(left + 0);
  const uint8x16_t left_16 = vld1q_u8(left + 16);
  const uint8x16_t left_32 = vld1q_u8(left + 32);
  const uint8x16_t left_48 = vld1q_u8(left + 48);
  const uint8x16_t left_15 = vextq_u8(left_0, left_16, 15);
  const uint8x16_t left_31 = vextq_u8(left_16, left_32, 15);
  const uint8x16_t left_47 = vextq_u8(left_32, left_48, 15);
  const uint8x16x4_t left_vals0 = { { left_m1, left_15, left_31, left_47 } };
  const uint8x16x4_t left_vals1 = { { left_0, left_16, left_32, left_48 } };
#define LEFT left_vals0, left_vals1
#else  // !AOM_ARCH_AARCH64
#define LEFT left
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < H; r++) {
    int y = r + 1;
    int base_x = (-y * dx) >> 6;
    for (int j = 0; j < W; j += 16) {
      const int base_min_diff = min_base_x - base_x - j;

      if (base_min_diff <= 0) {
        uint8x16_t resx =
            dr_prediction_z2_NxW_above_neon(above, dx, base_x, y, j);
        vst1q_u8(dst + j, resx);
      } else if (base_min_diff < 16) {
        uint8x16_t resx =
            dr_prediction_z2_NxW_above_neon(above, dx, base_x, y, j);
        uint8x16_t resy = dr_prediction_z2_NxW_left_neon(LEFT, dy, r, j);
        uint8x16_t mask = vld1q_u8(BaseMask[base_min_diff]);
        uint8x16_t resxy = vbslq_u8(mask, resy, resx);
        vst1q_u8(dst + j, resxy);
      } else {
        uint8x16_t resy = dr_prediction_z2_NxW_left_neon(LEFT, dy, r, j);
        vst1q_u8(dst + j, resy);
      }
    }  // for j
    dst += stride;
  }
#undef LEFT
}

// Directional prediction, zone 2: 90 < angle < 180
void av1_dr_prediction_z2_neon(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_above, int upsample_left, int dx,
                               int dy) {
  assert(dx > 0);
  assert(dy > 0);

  switch (bw) {
    case 4:
      dr_prediction_z2_Nx4_neon(bh, dst, stride, above, left, upsample_above,
                                upsample_left, dx, dy);
      break;
    case 8:
      dr_prediction_z2_Nx8_neon(bh, dst, stride, above, left, upsample_above,
                                upsample_left, dx, dy);
      break;
    default:
      dr_prediction_z2_HxW_neon(bh, bw, dst, stride, above, left, dx, dy);
      break;
  }
}

/* ---------------------P R E D I C T I O N   Z 3--------------------------- */

static AOM_FORCE_INLINE void z3_transpose_arrays_u8_16x4(const uint8x16_t *x,
                                                         uint8x16x2_t *d) {
  uint8x16x2_t w0 = vzipq_u8(x[0], x[1]);
  uint8x16x2_t w1 = vzipq_u8(x[2], x[3]);

  d[0] = aom_reinterpretq_u8_u16_x2(vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                                              vreinterpretq_u16_u8(w1.val[0])));
  d[1] = aom_reinterpretq_u8_u16_x2(vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                                              vreinterpretq_u16_u8(w1.val[1])));
}

static AOM_FORCE_INLINE void z3_transpose_arrays_u8_4x4(const uint8x8_t *x,
                                                        uint8x8x2_t *d) {
  uint8x8x2_t w0 = vzip_u8(x[0], x[1]);
  uint8x8x2_t w1 = vzip_u8(x[2], x[3]);

  *d = aom_reinterpret_u8_u16_x2(
      vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0])));
}

static AOM_FORCE_INLINE void z3_transpose_arrays_u8_8x4(const uint8x8_t *x,
                                                        uint8x8x2_t *d) {
  uint8x8x2_t w0 = vzip_u8(x[0], x[1]);
  uint8x8x2_t w1 = vzip_u8(x[2], x[3]);

  d[0] = aom_reinterpret_u8_u16_x2(
      vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0])));
  d[1] = aom_reinterpret_u8_u16_x2(
      vzip_u16(vreinterpret_u16_u8(w0.val[1]), vreinterpret_u16_u8(w1.val[1])));
}

static void z3_transpose_arrays_u8_16x16(const uint8_t *src, ptrdiff_t pitchSrc,
                                         uint8_t *dst, ptrdiff_t pitchDst) {
  // The same as the normal transposes in transpose_neon.h, but with a stride
  // between consecutive vectors of elements.
  uint8x16_t r[16];
  uint8x16_t d[16];
  for (int i = 0; i < 16; i++) {
    r[i] = vld1q_u8(src + i * pitchSrc);
  }
  transpose_arrays_u8_16x16(r, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + i * pitchDst, d[i]);
  }
}

static void z3_transpose_arrays_u8_16nx16n(const uint8_t *src,
                                           ptrdiff_t pitchSrc, uint8_t *dst,
                                           ptrdiff_t pitchDst, int width,
                                           int height) {
  for (int j = 0; j < height; j += 16) {
    for (int i = 0; i < width; i += 16) {
      z3_transpose_arrays_u8_16x16(src + i * pitchSrc + j, pitchSrc,
                                   dst + j * pitchDst + i, pitchDst);
    }
  }
}

static void dr_prediction_z3_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[4];
  uint8x8x2_t dest;

  dr_prediction_z1_HxW_internal_neon_64(4, 4, dstvec, left, upsample_left, dy);
  z3_transpose_arrays_u8_4x4(dstvec, &dest);
  store_u8x4_strided_x2(dst + stride * 0, stride, dest.val[0]);
  store_u8x4_strided_x2(dst + stride * 2, stride, dest.val[1]);
}

static void dr_prediction_z3_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[8];
  uint8x8_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(8, 8, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_8x8(dstvec, d);
  store_u8_8x8(dst, stride, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
}

static void dr_prediction_z3_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[4];
  uint8x8x2_t d[2];

  dr_prediction_z1_HxW_internal_neon_64(8, 4, dstvec, left, upsample_left, dy);
  z3_transpose_arrays_u8_8x4(dstvec, d);
  store_u8x4_strided_x2(dst + stride * 0, stride, d[0].val[0]);
  store_u8x4_strided_x2(dst + stride * 2, stride, d[0].val[1]);
  store_u8x4_strided_x2(dst + stride * 4, stride, d[1].val[0]);
  store_u8x4_strided_x2(dst + stride * 6, stride, d[1].val[1]);
}

static void dr_prediction_z3_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[8];
  uint8x8_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(4, 8, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_8x8(dstvec, d);
  store_u8_8x4(dst, stride, d[0], d[1], d[2], d[3]);
}

static void dr_prediction_z3_8x16_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x16_t dstvec[8];
  uint8x8_t d[16];

  dr_prediction_z1_HxW_internal_neon(16, 8, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_16x8(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1_u8(dst + i * stride, d[i]);
  }
}

static void dr_prediction_z3_16x8_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[16];
  uint8x16_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(8, 16, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_8x16(dstvec, d);
  for (int i = 0; i < 8; i++) {
    vst1q_u8(dst + i * stride, d[i]);
  }
}

static void dr_prediction_z3_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x16_t dstvec[4];
  uint8x16x2_t d[2];

  dr_prediction_z1_HxW_internal_neon(16, 4, dstvec, left, upsample_left, dy);
  z3_transpose_arrays_u8_16x4(dstvec, d);
  store_u8x4_strided_x4(dst + stride * 0, stride, d[0].val[0]);
  store_u8x4_strided_x4(dst + stride * 4, stride, d[0].val[1]);
  store_u8x4_strided_x4(dst + stride * 8, stride, d[1].val[0]);
  store_u8x4_strided_x4(dst + stride * 12, stride, d[1].val[1]);
}

static void dr_prediction_z3_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[16];
  uint8x16_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(4, 16, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_8x16(dstvec, d);
  for (int i = 0; i < 4; i++) {
    vst1q_u8(dst + i * stride, d[i]);
  }
}

static void dr_prediction_z3_8x32_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  (void)upsample_left;
  uint8x16x2_t dstvec[16];
  uint8x16_t d[32];
  uint8x16_t v_zero = vdupq_n_u8(0);

  dr_prediction_z1_32xN_internal_neon(8, dstvec, left, dy);
  for (int i = 8; i < 16; i++) {
    dstvec[i].val[0] = v_zero;
    dstvec[i].val[1] = v_zero;
  }
  transpose_arrays_u8_32x16(dstvec, d);
  for (int i = 0; i < 32; i++) {
    vst1_u8(dst + i * stride, vget_low_u8(d[i]));
  }
}

static void dr_prediction_z3_32x8_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[32];
  uint8x16_t d[16];

  dr_prediction_z1_HxW_internal_neon_64(8, 32, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_8x16(dstvec, d);
  transpose_arrays_u8_8x16(dstvec + 16, d + 8);
  for (int i = 0; i < 8; i++) {
    vst1q_u8(dst + i * stride, d[i]);
    vst1q_u8(dst + i * stride + 16, d[i + 8]);
  }
}

static void dr_prediction_z3_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[16];
  uint8x16_t d[16];

  dr_prediction_z1_HxW_internal_neon(16, 16, dstvec, left, upsample_left, dy);
  transpose_arrays_u8_16x16(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + i * stride, d[i]);
  }
}

static void dr_prediction_z3_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  uint8x16x2_t dstvec[32];
  uint8x16_t d[64];

  dr_prediction_z1_32xN_internal_neon(32, dstvec, left, dy);
  transpose_arrays_u8_32x16(dstvec, d);
  transpose_arrays_u8_32x16(dstvec + 16, d + 32);
  for (int i = 0; i < 32; i++) {
    vst1q_u8(dst + i * stride, d[i]);
    vst1q_u8(dst + i * stride + 16, d[i + 32]);
  }
}

static void dr_prediction_z3_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  DECLARE_ALIGNED(16, uint8_t, dstT[64 * 64]);

  dr_prediction_z1_64xN_neon(64, dstT, 64, left, dy);
  z3_transpose_arrays_u8_16nx16n(dstT, 64, dst, stride, 64, 64);
}

static void dr_prediction_z3_16x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  uint8x16x2_t dstvec[16];
  uint8x16_t d[32];

  dr_prediction_z1_32xN_internal_neon(16, dstvec, left, dy);
  transpose_arrays_u8_32x16(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + 2 * i * stride, d[2 * i + 0]);
    vst1q_u8(dst + (2 * i + 1) * stride, d[2 * i + 1]);
  }
}

static void dr_prediction_z3_32x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[32];

  dr_prediction_z1_HxW_internal_neon(16, 32, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 32; i += 16) {
    uint8x16_t d[16];
    transpose_arrays_u8_16x16(dstvec + i, d);
    for (int j = 0; j < 16; j++) {
      vst1q_u8(dst + j * stride + i, d[j]);
    }
  }
}

static void dr_prediction_z3_32x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  uint8_t dstT[64 * 32];

  dr_prediction_z1_64xN_neon(32, dstT, 64, left, dy);
  z3_transpose_arrays_u8_16nx16n(dstT, 64, dst, stride, 32, 64);
}

static void dr_prediction_z3_64x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  uint8_t dstT[32 * 64];

  dr_prediction_z1_32xN_neon(64, dstT, 32, left, dy);
  z3_transpose_arrays_u8_16nx16n(dstT, 32, dst, stride, 64, 32);
}

static void dr_prediction_z3_16x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  (void)upsample_left;
  uint8_t dstT[64 * 16];

  dr_prediction_z1_64xN_neon(16, dstT, 64, left, dy);
  z3_transpose_arrays_u8_16nx16n(dstT, 64, dst, stride, 16, 64);
}

static void dr_prediction_z3_64x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[64];

  dr_prediction_z1_HxW_internal_neon(16, 64, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 64; i += 16) {
    uint8x16_t d[16];
    transpose_arrays_u8_16x16(dstvec + i, d);
    for (int j = 0; j < 16; ++j) {
      vst1q_u8(dst + j * stride + i, d[j]);
    }
  }
}

typedef void (*dr_prediction_z3_fn)(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *left, int upsample_left,
                                    int dy);

static dr_prediction_z3_fn dr_prediction_z3_arr[7][7] = {
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, dr_prediction_z3_4x4_neon, dr_prediction_z3_4x8_neon,
    dr_prediction_z3_4x16_neon, NULL, NULL },
  { NULL, NULL, dr_prediction_z3_8x4_neon, dr_prediction_z3_8x8_neon,
    dr_prediction_z3_8x16_neon, dr_prediction_z3_8x32_neon, NULL },
  { NULL, NULL, dr_prediction_z3_16x4_neon, dr_prediction_z3_16x8_neon,
    dr_prediction_z3_16x16_neon, dr_prediction_z3_16x32_neon,
    dr_prediction_z3_16x64_neon },
  { NULL, NULL, NULL, dr_prediction_z3_32x8_neon, dr_prediction_z3_32x16_neon,
    dr_prediction_z3_32x32_neon, dr_prediction_z3_32x64_neon },
  { NULL, NULL, NULL, NULL, dr_prediction_z3_64x16_neon,
    dr_prediction_z3_64x32_neon, dr_prediction_z3_64x64_neon },
};

void av1_dr_prediction_z3_neon(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_left, int dx, int dy) {
  (void)above;
  (void)dx;
  assert(dx == 1);
  assert(dy > 0);

  dr_prediction_z3_fn f = dr_prediction_z3_arr[get_msb(bw)][get_msb(bh)];
  assert(f != NULL);
  f(dst, stride, left, upsample_left, dy);
}

// -----------------------------------------------------------------------------
// SMOOTH_PRED

// 256 - v = vneg_s8(v)
static INLINE uint8x8_t negate_s8(const uint8x8_t v) {
  return vreinterpret_u8_s8(vneg_s8(vreinterpret_s8_u8(v)));
}

static void smooth_4xh_neon(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *const top_row,
                            const uint8_t *const left_column,
                            const int height) {
  const uint8_t top_right = top_row[3];
  const uint8_t bottom_left = left_column[height - 1];
  const uint8_t *const weights_y = smooth_weights + height - 4;

  uint8x8_t top_v = load_u8_4x1(top_row);
  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);
  uint8x8_t weights_x_v = load_u8_4x1(smooth_weights);
  const uint8x8_t scaled_weights_x = negate_s8(weights_x_v);
  const uint16x8_t weighted_tr = vmull_u8(scaled_weights_x, top_right_v);

  assert(height > 0);
  int y = 0;
  do {
    const uint8x8_t left_v = vdup_n_u8(left_column[y]);
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = negate_s8(weights_y_v);
    const uint16x8_t weighted_bl = vmull_u8(scaled_weights_y, bottom_left_v);
    const uint16x8_t weighted_top_bl =
        vmlal_u8(weighted_bl, weights_y_v, top_v);
    const uint16x8_t weighted_left_tr =
        vmlal_u8(weighted_tr, weights_x_v, left_v);
    // Maximum value of each parameter: 0xFF00
    const uint16x8_t avg = vhaddq_u16(weighted_top_bl, weighted_left_tr);
    const uint8x8_t result = vrshrn_n_u16(avg, SMOOTH_WEIGHT_LOG2_SCALE);

    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(result), 0);
    dst += stride;
  } while (++y != height);
}

static INLINE uint8x8_t calculate_pred(const uint16x8_t weighted_top_bl,
                                       const uint16x8_t weighted_left_tr) {
  // Maximum value of each parameter: 0xFF00
  const uint16x8_t avg = vhaddq_u16(weighted_top_bl, weighted_left_tr);
  return vrshrn_n_u16(avg, SMOOTH_WEIGHT_LOG2_SCALE);
}

static INLINE uint8x8_t calculate_weights_and_pred(
    const uint8x8_t top, const uint8x8_t left, const uint16x8_t weighted_tr,
    const uint8x8_t bottom_left, const uint8x8_t weights_x,
    const uint8x8_t scaled_weights_y, const uint8x8_t weights_y) {
  const uint16x8_t weighted_top = vmull_u8(weights_y, top);
  const uint16x8_t weighted_top_bl =
      vmlal_u8(weighted_top, scaled_weights_y, bottom_left);
  const uint16x8_t weighted_left_tr = vmlal_u8(weighted_tr, weights_x, left);
  return calculate_pred(weighted_top_bl, weighted_left_tr);
}

static void smooth_8xh_neon(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *const top_row,
                            const uint8_t *const left_column,
                            const int height) {
  const uint8_t top_right = top_row[7];
  const uint8_t bottom_left = left_column[height - 1];
  const uint8_t *const weights_y = smooth_weights + height - 4;

  const uint8x8_t top_v = vld1_u8(top_row);
  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);
  const uint8x8_t weights_x_v = vld1_u8(smooth_weights + 4);
  const uint8x8_t scaled_weights_x = negate_s8(weights_x_v);
  const uint16x8_t weighted_tr = vmull_u8(scaled_weights_x, top_right_v);

  assert(height > 0);
  int y = 0;
  do {
    const uint8x8_t left_v = vdup_n_u8(left_column[y]);
    const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);
    const uint8x8_t scaled_weights_y = negate_s8(weights_y_v);
    const uint8x8_t result =
        calculate_weights_and_pred(top_v, left_v, weighted_tr, bottom_left_v,
                                   weights_x_v, scaled_weights_y, weights_y_v);

    vst1_u8(dst, result);
    dst += stride;
  } while (++y != height);
}

#define SMOOTH_NXM(W, H)                                                       \
  void aom_smooth_predictor_##W##x##H##_neon(uint8_t *dst, ptrdiff_t y_stride, \
                                             const uint8_t *above,             \
                                             const uint8_t *left) {            \
    smooth_##W##xh_neon(dst, y_stride, above, left, H);                        \
  }

SMOOTH_NXM(4, 4)
SMOOTH_NXM(4, 8)
SMOOTH_NXM(8, 4)
SMOOTH_NXM(8, 8)
SMOOTH_NXM(4, 16)
SMOOTH_NXM(8, 16)
SMOOTH_NXM(8, 32)

#undef SMOOTH_NXM

static INLINE uint8x16_t calculate_weights_and_predq(
    const uint8x16_t top, const uint8x8_t left, const uint8x8_t top_right,
    const uint8x8_t weights_y, const uint8x16_t weights_x,
    const uint8x16_t scaled_weights_x, const uint16x8_t weighted_bl) {
  const uint16x8_t weighted_top_bl_low =
      vmlal_u8(weighted_bl, weights_y, vget_low_u8(top));
  const uint16x8_t weighted_left_low = vmull_u8(vget_low_u8(weights_x), left);
  const uint16x8_t weighted_left_tr_low =
      vmlal_u8(weighted_left_low, vget_low_u8(scaled_weights_x), top_right);
  const uint8x8_t result_low =
      calculate_pred(weighted_top_bl_low, weighted_left_tr_low);

  const uint16x8_t weighted_top_bl_high =
      vmlal_u8(weighted_bl, weights_y, vget_high_u8(top));
  const uint16x8_t weighted_left_high = vmull_u8(vget_high_u8(weights_x), left);
  const uint16x8_t weighted_left_tr_high =
      vmlal_u8(weighted_left_high, vget_high_u8(scaled_weights_x), top_right);
  const uint8x8_t result_high =
      calculate_pred(weighted_top_bl_high, weighted_left_tr_high);

  return vcombine_u8(result_low, result_high);
}

// 256 - v = vneg_s8(v)
static INLINE uint8x16_t negate_s8q(const uint8x16_t v) {
  return vreinterpretq_u8_s8(vnegq_s8(vreinterpretq_s8_u8(v)));
}

// For width 16 and above.
#define SMOOTH_PREDICTOR(W)                                                 \
  static void smooth_##W##xh_neon(                                          \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *const top_row,         \
      const uint8_t *const left_column, const int height) {                 \
    const uint8_t top_right = top_row[(W)-1];                               \
    const uint8_t bottom_left = left_column[height - 1];                    \
    const uint8_t *const weights_y = smooth_weights + height - 4;           \
                                                                            \
    uint8x16_t top_v[4];                                                    \
    top_v[0] = vld1q_u8(top_row);                                           \
    if ((W) > 16) {                                                         \
      top_v[1] = vld1q_u8(top_row + 16);                                    \
      if ((W) == 64) {                                                      \
        top_v[2] = vld1q_u8(top_row + 32);                                  \
        top_v[3] = vld1q_u8(top_row + 48);                                  \
      }                                                                     \
    }                                                                       \
                                                                            \
    const uint8x8_t top_right_v = vdup_n_u8(top_right);                     \
    const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);                 \
                                                                            \
    uint8x16_t weights_x_v[4];                                              \
    weights_x_v[0] = vld1q_u8(smooth_weights + (W)-4);                      \
    if ((W) > 16) {                                                         \
      weights_x_v[1] = vld1q_u8(smooth_weights + (W) + 16 - 4);             \
      if ((W) == 64) {                                                      \
        weights_x_v[2] = vld1q_u8(smooth_weights + (W) + 32 - 4);           \
        weights_x_v[3] = vld1q_u8(smooth_weights + (W) + 48 - 4);           \
      }                                                                     \
    }                                                                       \
                                                                            \
    uint8x16_t scaled_weights_x[4];                                         \
    scaled_weights_x[0] = negate_s8q(weights_x_v[0]);                       \
    if ((W) > 16) {                                                         \
      scaled_weights_x[1] = negate_s8q(weights_x_v[1]);                     \
      if ((W) == 64) {                                                      \
        scaled_weights_x[2] = negate_s8q(weights_x_v[2]);                   \
        scaled_weights_x[3] = negate_s8q(weights_x_v[3]);                   \
      }                                                                     \
    }                                                                       \
                                                                            \
    for (int y = 0; y < height; ++y) {                                      \
      const uint8x8_t left_v = vdup_n_u8(left_column[y]);                   \
      const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);                \
      const uint8x8_t scaled_weights_y = negate_s8(weights_y_v);            \
      const uint16x8_t weighted_bl =                                        \
          vmull_u8(scaled_weights_y, bottom_left_v);                        \
                                                                            \
      vst1q_u8(dst, calculate_weights_and_predq(                            \
                        top_v[0], left_v, top_right_v, weights_y_v,         \
                        weights_x_v[0], scaled_weights_x[0], weighted_bl)); \
                                                                            \
      if ((W) > 16) {                                                       \
        vst1q_u8(dst + 16,                                                  \
                 calculate_weights_and_predq(                               \
                     top_v[1], left_v, top_right_v, weights_y_v,            \
                     weights_x_v[1], scaled_weights_x[1], weighted_bl));    \
        if ((W) == 64) {                                                    \
          vst1q_u8(dst + 32,                                                \
                   calculate_weights_and_predq(                             \
                       top_v[2], left_v, top_right_v, weights_y_v,          \
                       weights_x_v[2], scaled_weights_x[2], weighted_bl));  \
          vst1q_u8(dst + 48,                                                \
                   calculate_weights_and_predq(                             \
                       top_v[3], left_v, top_right_v, weights_y_v,          \
                       weights_x_v[3], scaled_weights_x[3], weighted_bl));  \
        }                                                                   \
      }                                                                     \
                                                                            \
      dst += stride;                                                        \
    }                                                                       \
  }

SMOOTH_PREDICTOR(16)
SMOOTH_PREDICTOR(32)
SMOOTH_PREDICTOR(64)

#undef SMOOTH_PREDICTOR

#define SMOOTH_NXM_WIDE(W, H)                                                  \
  void aom_smooth_predictor_##W##x##H##_neon(uint8_t *dst, ptrdiff_t y_stride, \
                                             const uint8_t *above,             \
                                             const uint8_t *left) {            \
    smooth_##W##xh_neon(dst, y_stride, above, left, H);                        \
  }

SMOOTH_NXM_WIDE(16, 4)
SMOOTH_NXM_WIDE(16, 8)
SMOOTH_NXM_WIDE(16, 16)
SMOOTH_NXM_WIDE(16, 32)
SMOOTH_NXM_WIDE(16, 64)
SMOOTH_NXM_WIDE(32, 8)
SMOOTH_NXM_WIDE(32, 16)
SMOOTH_NXM_WIDE(32, 32)
SMOOTH_NXM_WIDE(32, 64)
SMOOTH_NXM_WIDE(64, 16)
SMOOTH_NXM_WIDE(64, 32)
SMOOTH_NXM_WIDE(64, 64)

#undef SMOOTH_NXM_WIDE

// -----------------------------------------------------------------------------
// SMOOTH_V_PRED

// For widths 4 and 8.
#define SMOOTH_V_PREDICTOR(W)                                         \
  static void smooth_v_##W##xh_neon(                                  \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *const top_row,   \
      const uint8_t *const left_column, const int height) {           \
    const uint8_t bottom_left = left_column[height - 1];              \
    const uint8_t *const weights_y = smooth_weights + height - 4;     \
                                                                      \
    uint8x8_t top_v;                                                  \
    if ((W) == 4) {                                                   \
      top_v = load_u8_4x1(top_row);                                   \
    } else { /* width == 8 */                                         \
      top_v = vld1_u8(top_row);                                       \
    }                                                                 \
                                                                      \
    const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);           \
                                                                      \
    assert(height > 0);                                               \
    int y = 0;                                                        \
    do {                                                              \
      const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);          \
      const uint8x8_t scaled_weights_y = negate_s8(weights_y_v);      \
                                                                      \
      const uint16x8_t weighted_top = vmull_u8(weights_y_v, top_v);   \
      const uint16x8_t weighted_top_bl =                              \
          vmlal_u8(weighted_top, scaled_weights_y, bottom_left_v);    \
      const uint8x8_t pred =                                          \
          vrshrn_n_u16(weighted_top_bl, SMOOTH_WEIGHT_LOG2_SCALE);    \
                                                                      \
      if ((W) == 4) {                                                 \
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(pred), 0); \
      } else { /* width == 8 */                                       \
        vst1_u8(dst, pred);                                           \
      }                                                               \
      dst += stride;                                                  \
    } while (++y != height);                                          \
  }

SMOOTH_V_PREDICTOR(4)
SMOOTH_V_PREDICTOR(8)

#undef SMOOTH_V_PREDICTOR

#define SMOOTH_V_NXM(W, H)                                    \
  void aom_smooth_v_predictor_##W##x##H##_neon(               \
      uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, \
      const uint8_t *left) {                                  \
    smooth_v_##W##xh_neon(dst, y_stride, above, left, H);     \
  }

SMOOTH_V_NXM(4, 4)
SMOOTH_V_NXM(4, 8)
SMOOTH_V_NXM(4, 16)
SMOOTH_V_NXM(8, 4)
SMOOTH_V_NXM(8, 8)
SMOOTH_V_NXM(8, 16)
SMOOTH_V_NXM(8, 32)

#undef SMOOTH_V_NXM

static INLINE uint8x16_t calculate_vertical_weights_and_pred(
    const uint8x16_t top, const uint8x8_t weights_y,
    const uint16x8_t weighted_bl) {
  const uint16x8_t pred_low =
      vmlal_u8(weighted_bl, weights_y, vget_low_u8(top));
  const uint16x8_t pred_high =
      vmlal_u8(weighted_bl, weights_y, vget_high_u8(top));
  const uint8x8_t pred_scaled_low =
      vrshrn_n_u16(pred_low, SMOOTH_WEIGHT_LOG2_SCALE);
  const uint8x8_t pred_scaled_high =
      vrshrn_n_u16(pred_high, SMOOTH_WEIGHT_LOG2_SCALE);
  return vcombine_u8(pred_scaled_low, pred_scaled_high);
}

// For width 16 and above.
#define SMOOTH_V_PREDICTOR(W)                                            \
  static void smooth_v_##W##xh_neon(                                     \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *const top_row,      \
      const uint8_t *const left_column, const int height) {              \
    const uint8_t bottom_left = left_column[height - 1];                 \
    const uint8_t *const weights_y = smooth_weights + height - 4;        \
                                                                         \
    uint8x16_t top_v[4];                                                 \
    top_v[0] = vld1q_u8(top_row);                                        \
    if ((W) > 16) {                                                      \
      top_v[1] = vld1q_u8(top_row + 16);                                 \
      if ((W) == 64) {                                                   \
        top_v[2] = vld1q_u8(top_row + 32);                               \
        top_v[3] = vld1q_u8(top_row + 48);                               \
      }                                                                  \
    }                                                                    \
                                                                         \
    const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);              \
                                                                         \
    assert(height > 0);                                                  \
    int y = 0;                                                           \
    do {                                                                 \
      const uint8x8_t weights_y_v = vdup_n_u8(weights_y[y]);             \
      const uint8x8_t scaled_weights_y = negate_s8(weights_y_v);         \
      const uint16x8_t weighted_bl =                                     \
          vmull_u8(scaled_weights_y, bottom_left_v);                     \
                                                                         \
      const uint8x16_t pred_0 = calculate_vertical_weights_and_pred(     \
          top_v[0], weights_y_v, weighted_bl);                           \
      vst1q_u8(dst, pred_0);                                             \
                                                                         \
      if ((W) > 16) {                                                    \
        const uint8x16_t pred_1 = calculate_vertical_weights_and_pred(   \
            top_v[1], weights_y_v, weighted_bl);                         \
        vst1q_u8(dst + 16, pred_1);                                      \
                                                                         \
        if ((W) == 64) {                                                 \
          const uint8x16_t pred_2 = calculate_vertical_weights_and_pred( \
              top_v[2], weights_y_v, weighted_bl);                       \
          vst1q_u8(dst + 32, pred_2);                                    \
                                                                         \
          const uint8x16_t pred_3 = calculate_vertical_weights_and_pred( \
              top_v[3], weights_y_v, weighted_bl);                       \
          vst1q_u8(dst + 48, pred_3);                                    \
        }                                                                \
      }                                                                  \
                                                                         \
      dst += stride;                                                     \
    } while (++y != height);                                             \
  }

SMOOTH_V_PREDICTOR(16)
SMOOTH_V_PREDICTOR(32)
SMOOTH_V_PREDICTOR(64)

#undef SMOOTH_V_PREDICTOR

#define SMOOTH_V_NXM_WIDE(W, H)                               \
  void aom_smooth_v_predictor_##W##x##H##_neon(               \
      uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, \
      const uint8_t *left) {                                  \
    smooth_v_##W##xh_neon(dst, y_stride, above, left, H);     \
  }

SMOOTH_V_NXM_WIDE(16, 4)
SMOOTH_V_NXM_WIDE(16, 8)
SMOOTH_V_NXM_WIDE(16, 16)
SMOOTH_V_NXM_WIDE(16, 32)
SMOOTH_V_NXM_WIDE(16, 64)
SMOOTH_V_NXM_WIDE(32, 8)
SMOOTH_V_NXM_WIDE(32, 16)
SMOOTH_V_NXM_WIDE(32, 32)
SMOOTH_V_NXM_WIDE(32, 64)
SMOOTH_V_NXM_WIDE(64, 16)
SMOOTH_V_NXM_WIDE(64, 32)
SMOOTH_V_NXM_WIDE(64, 64)

#undef SMOOTH_V_NXM_WIDE

// -----------------------------------------------------------------------------
// SMOOTH_H_PRED

// For widths 4 and 8.
#define SMOOTH_H_PREDICTOR(W)                                               \
  static void smooth_h_##W##xh_neon(                                        \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *const top_row,         \
      const uint8_t *const left_column, const int height) {                 \
    const uint8_t top_right = top_row[(W)-1];                               \
                                                                            \
    const uint8x8_t top_right_v = vdup_n_u8(top_right);                     \
    /* Over-reads for 4xN but still within the array. */                    \
    const uint8x8_t weights_x = vld1_u8(smooth_weights + (W)-4);            \
    const uint8x8_t scaled_weights_x = negate_s8(weights_x);                \
    const uint16x8_t weighted_tr = vmull_u8(scaled_weights_x, top_right_v); \
                                                                            \
    assert(height > 0);                                                     \
    int y = 0;                                                              \
    do {                                                                    \
      const uint8x8_t left_v = vdup_n_u8(left_column[y]);                   \
      const uint16x8_t weighted_left_tr =                                   \
          vmlal_u8(weighted_tr, weights_x, left_v);                         \
      const uint8x8_t pred =                                                \
          vrshrn_n_u16(weighted_left_tr, SMOOTH_WEIGHT_LOG2_SCALE);         \
                                                                            \
      if ((W) == 4) {                                                       \
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(pred), 0);       \
      } else { /* width == 8 */                                             \
        vst1_u8(dst, pred);                                                 \
      }                                                                     \
      dst += stride;                                                        \
    } while (++y != height);                                                \
  }

SMOOTH_H_PREDICTOR(4)
SMOOTH_H_PREDICTOR(8)

#undef SMOOTH_H_PREDICTOR

#define SMOOTH_H_NXM(W, H)                                    \
  void aom_smooth_h_predictor_##W##x##H##_neon(               \
      uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, \
      const uint8_t *left) {                                  \
    smooth_h_##W##xh_neon(dst, y_stride, above, left, H);     \
  }

SMOOTH_H_NXM(4, 4)
SMOOTH_H_NXM(4, 8)
SMOOTH_H_NXM(4, 16)
SMOOTH_H_NXM(8, 4)
SMOOTH_H_NXM(8, 8)
SMOOTH_H_NXM(8, 16)
SMOOTH_H_NXM(8, 32)

#undef SMOOTH_H_NXM

static INLINE uint8x16_t calculate_horizontal_weights_and_pred(
    const uint8x8_t left, const uint8x8_t top_right, const uint8x16_t weights_x,
    const uint8x16_t scaled_weights_x) {
  const uint16x8_t weighted_left_low = vmull_u8(vget_low_u8(weights_x), left);
  const uint16x8_t weighted_left_tr_low =
      vmlal_u8(weighted_left_low, vget_low_u8(scaled_weights_x), top_right);
  const uint8x8_t pred_scaled_low =
      vrshrn_n_u16(weighted_left_tr_low, SMOOTH_WEIGHT_LOG2_SCALE);

  const uint16x8_t weighted_left_high = vmull_u8(vget_high_u8(weights_x), left);
  const uint16x8_t weighted_left_tr_high =
      vmlal_u8(weighted_left_high, vget_high_u8(scaled_weights_x), top_right);
  const uint8x8_t pred_scaled_high =
      vrshrn_n_u16(weighted_left_tr_high, SMOOTH_WEIGHT_LOG2_SCALE);

  return vcombine_u8(pred_scaled_low, pred_scaled_high);
}

// For width 16 and above.
#define SMOOTH_H_PREDICTOR(W)                                              \
  static void smooth_h_##W##xh_neon(                                       \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *const top_row,        \
      const uint8_t *const left_column, const int height) {                \
    const uint8_t top_right = top_row[(W)-1];                              \
                                                                           \
    const uint8x8_t top_right_v = vdup_n_u8(top_right);                    \
                                                                           \
    uint8x16_t weights_x[4];                                               \
    weights_x[0] = vld1q_u8(smooth_weights + (W)-4);                       \
    if ((W) > 16) {                                                        \
      weights_x[1] = vld1q_u8(smooth_weights + (W) + 16 - 4);              \
      if ((W) == 64) {                                                     \
        weights_x[2] = vld1q_u8(smooth_weights + (W) + 32 - 4);            \
        weights_x[3] = vld1q_u8(smooth_weights + (W) + 48 - 4);            \
      }                                                                    \
    }                                                                      \
                                                                           \
    uint8x16_t scaled_weights_x[4];                                        \
    scaled_weights_x[0] = negate_s8q(weights_x[0]);                        \
    if ((W) > 16) {                                                        \
      scaled_weights_x[1] = negate_s8q(weights_x[1]);                      \
      if ((W) == 64) {                                                     \
        scaled_weights_x[2] = negate_s8q(weights_x[2]);                    \
        scaled_weights_x[3] = negate_s8q(weights_x[3]);                    \
      }                                                                    \
    }                                                                      \
                                                                           \
    assert(height > 0);                                                    \
    int y = 0;                                                             \
    do {                                                                   \
      const uint8x8_t left_v = vdup_n_u8(left_column[y]);                  \
                                                                           \
      const uint8x16_t pred_0 = calculate_horizontal_weights_and_pred(     \
          left_v, top_right_v, weights_x[0], scaled_weights_x[0]);         \
      vst1q_u8(dst, pred_0);                                               \
                                                                           \
      if ((W) > 16) {                                                      \
        const uint8x16_t pred_1 = calculate_horizontal_weights_and_pred(   \
            left_v, top_right_v, weights_x[1], scaled_weights_x[1]);       \
        vst1q_u8(dst + 16, pred_1);                                        \
                                                                           \
        if ((W) == 64) {                                                   \
          const uint8x16_t pred_2 = calculate_horizontal_weights_and_pred( \
              left_v, top_right_v, weights_x[2], scaled_weights_x[2]);     \
          vst1q_u8(dst + 32, pred_2);                                      \
                                                                           \
          const uint8x16_t pred_3 = calculate_horizontal_weights_and_pred( \
              left_v, top_right_v, weights_x[3], scaled_weights_x[3]);     \
          vst1q_u8(dst + 48, pred_3);                                      \
        }                                                                  \
      }                                                                    \
      dst += stride;                                                       \
    } while (++y != height);                                               \
  }

SMOOTH_H_PREDICTOR(16)
SMOOTH_H_PREDICTOR(32)
SMOOTH_H_PREDICTOR(64)

#undef SMOOTH_H_PREDICTOR

#define SMOOTH_H_NXM_WIDE(W, H)                               \
  void aom_smooth_h_predictor_##W##x##H##_neon(               \
      uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, \
      const uint8_t *left) {                                  \
    smooth_h_##W##xh_neon(dst, y_stride, above, left, H);     \
  }

SMOOTH_H_NXM_WIDE(16, 4)
SMOOTH_H_NXM_WIDE(16, 8)
SMOOTH_H_NXM_WIDE(16, 16)
SMOOTH_H_NXM_WIDE(16, 32)
SMOOTH_H_NXM_WIDE(16, 64)
SMOOTH_H_NXM_WIDE(32, 8)
SMOOTH_H_NXM_WIDE(32, 16)
SMOOTH_H_NXM_WIDE(32, 32)
SMOOTH_H_NXM_WIDE(32, 64)
SMOOTH_H_NXM_WIDE(64, 16)
SMOOTH_H_NXM_WIDE(64, 32)
SMOOTH_H_NXM_WIDE(64, 64)

#undef SMOOTH_H_NXM_WIDE

// -----------------------------------------------------------------------------
// PAETH

static INLINE void paeth_4or8_x_h_neon(uint8_t *dest, ptrdiff_t stride,
                                       const uint8_t *const top_row,
                                       const uint8_t *const left_column,
                                       int width, int height) {
  const uint8x8_t top_left = vdup_n_u8(top_row[-1]);
  const uint16x8_t top_left_x2 = vdupq_n_u16(top_row[-1] + top_row[-1]);
  uint8x8_t top;
  if (width == 4) {
    top = load_u8_4x1(top_row);
  } else {  // width == 8
    top = vld1_u8(top_row);
  }

  assert(height > 0);
  int y = 0;
  do {
    const uint8x8_t left = vdup_n_u8(left_column[y]);

    const uint8x8_t left_dist = vabd_u8(top, top_left);
    const uint8x8_t top_dist = vabd_u8(left, top_left);
    const uint16x8_t top_left_dist =
        vabdq_u16(vaddl_u8(top, left), top_left_x2);

    const uint8x8_t left_le_top = vcle_u8(left_dist, top_dist);
    const uint8x8_t left_le_top_left =
        vmovn_u16(vcleq_u16(vmovl_u8(left_dist), top_left_dist));
    const uint8x8_t top_le_top_left =
        vmovn_u16(vcleq_u16(vmovl_u8(top_dist), top_left_dist));

    // if (left_dist <= top_dist && left_dist <= top_left_dist)
    const uint8x8_t left_mask = vand_u8(left_le_top, left_le_top_left);
    //   dest[x] = left_column[y];
    // Fill all the unused spaces with 'top'. They will be overwritten when
    // the positions for top_left are known.
    uint8x8_t result = vbsl_u8(left_mask, left, top);
    // else if (top_dist <= top_left_dist)
    //   dest[x] = top_row[x];
    // Add these values to the mask. They were already set.
    const uint8x8_t left_or_top_mask = vorr_u8(left_mask, top_le_top_left);
    // else
    //   dest[x] = top_left;
    result = vbsl_u8(left_or_top_mask, result, top_left);

    if (width == 4) {
      store_u8_4x1(dest, result);
    } else {  // width == 8
      vst1_u8(dest, result);
    }
    dest += stride;
  } while (++y != height);
}

#define PAETH_NXM(W, H)                                                     \
  void aom_paeth_predictor_##W##x##H##_neon(uint8_t *dst, ptrdiff_t stride, \
                                            const uint8_t *above,           \
                                            const uint8_t *left) {          \
    paeth_4or8_x_h_neon(dst, stride, above, left, W, H);                    \
  }

PAETH_NXM(4, 4)
PAETH_NXM(4, 8)
PAETH_NXM(8, 4)
PAETH_NXM(8, 8)
PAETH_NXM(8, 16)

PAETH_NXM(4, 16)
PAETH_NXM(8, 32)

// Calculate X distance <= TopLeft distance and pack the resulting mask into
// uint8x8_t.
static INLINE uint8x16_t x_le_top_left(const uint8x16_t x_dist,
                                       const uint16x8_t top_left_dist_low,
                                       const uint16x8_t top_left_dist_high) {
  const uint8x16_t top_left_dist = vcombine_u8(vqmovn_u16(top_left_dist_low),
                                               vqmovn_u16(top_left_dist_high));
  return vcleq_u8(x_dist, top_left_dist);
}

// Select the closest values and collect them.
static INLINE uint8x16_t select_paeth(const uint8x16_t top,
                                      const uint8x16_t left,
                                      const uint8x16_t top_left,
                                      const uint8x16_t left_le_top,
                                      const uint8x16_t left_le_top_left,
                                      const uint8x16_t top_le_top_left) {
  // if (left_dist <= top_dist && left_dist <= top_left_dist)
  const uint8x16_t left_mask = vandq_u8(left_le_top, left_le_top_left);
  //   dest[x] = left_column[y];
  // Fill all the unused spaces with 'top'. They will be overwritten when
  // the positions for top_left are known.
  uint8x16_t result = vbslq_u8(left_mask, left, top);
  // else if (top_dist <= top_left_dist)
  //   dest[x] = top_row[x];
  // Add these values to the mask. They were already set.
  const uint8x16_t left_or_top_mask = vorrq_u8(left_mask, top_le_top_left);
  // else
  //   dest[x] = top_left;
  return vbslq_u8(left_or_top_mask, result, top_left);
}

// Generate numbered and high/low versions of top_left_dist.
#define TOP_LEFT_DIST(num)                                              \
  const uint16x8_t top_left_##num##_dist_low = vabdq_u16(               \
      vaddl_u8(vget_low_u8(top[num]), vget_low_u8(left)), top_left_x2); \
  const uint16x8_t top_left_##num##_dist_high = vabdq_u16(              \
      vaddl_u8(vget_high_u8(top[num]), vget_low_u8(left)), top_left_x2)

// Generate numbered versions of XLeTopLeft with x = left.
#define LEFT_LE_TOP_LEFT(num)                                     \
  const uint8x16_t left_le_top_left_##num =                       \
      x_le_top_left(left_##num##_dist, top_left_##num##_dist_low, \
                    top_left_##num##_dist_high)

// Generate numbered versions of XLeTopLeft with x = top.
#define TOP_LE_TOP_LEFT(num)                              \
  const uint8x16_t top_le_top_left_##num = x_le_top_left( \
      top_dist, top_left_##num##_dist_low, top_left_##num##_dist_high)

static INLINE void paeth16_plus_x_h_neon(uint8_t *dest, ptrdiff_t stride,
                                         const uint8_t *const top_row,
                                         const uint8_t *const left_column,
                                         int width, int height) {
  const uint8x16_t top_left = vdupq_n_u8(top_row[-1]);
  const uint16x8_t top_left_x2 = vdupq_n_u16(top_row[-1] + top_row[-1]);
  uint8x16_t top[4];
  top[0] = vld1q_u8(top_row);
  if (width > 16) {
    top[1] = vld1q_u8(top_row + 16);
    if (width == 64) {
      top[2] = vld1q_u8(top_row + 32);
      top[3] = vld1q_u8(top_row + 48);
    }
  }

  assert(height > 0);
  int y = 0;
  do {
    const uint8x16_t left = vdupq_n_u8(left_column[y]);

    const uint8x16_t top_dist = vabdq_u8(left, top_left);

    const uint8x16_t left_0_dist = vabdq_u8(top[0], top_left);
    TOP_LEFT_DIST(0);
    const uint8x16_t left_0_le_top = vcleq_u8(left_0_dist, top_dist);
    LEFT_LE_TOP_LEFT(0);
    TOP_LE_TOP_LEFT(0);

    const uint8x16_t result_0 =
        select_paeth(top[0], left, top_left, left_0_le_top, left_le_top_left_0,
                     top_le_top_left_0);
    vst1q_u8(dest, result_0);

    if (width > 16) {
      const uint8x16_t left_1_dist = vabdq_u8(top[1], top_left);
      TOP_LEFT_DIST(1);
      const uint8x16_t left_1_le_top = vcleq_u8(left_1_dist, top_dist);
      LEFT_LE_TOP_LEFT(1);
      TOP_LE_TOP_LEFT(1);

      const uint8x16_t result_1 =
          select_paeth(top[1], left, top_left, left_1_le_top,
                       left_le_top_left_1, top_le_top_left_1);
      vst1q_u8(dest + 16, result_1);

      if (width == 64) {
        const uint8x16_t left_2_dist = vabdq_u8(top[2], top_left);
        TOP_LEFT_DIST(2);
        const uint8x16_t left_2_le_top = vcleq_u8(left_2_dist, top_dist);
        LEFT_LE_TOP_LEFT(2);
        TOP_LE_TOP_LEFT(2);

        const uint8x16_t result_2 =
            select_paeth(top[2], left, top_left, left_2_le_top,
                         left_le_top_left_2, top_le_top_left_2);
        vst1q_u8(dest + 32, result_2);

        const uint8x16_t left_3_dist = vabdq_u8(top[3], top_left);
        TOP_LEFT_DIST(3);
        const uint8x16_t left_3_le_top = vcleq_u8(left_3_dist, top_dist);
        LEFT_LE_TOP_LEFT(3);
        TOP_LE_TOP_LEFT(3);

        const uint8x16_t result_3 =
            select_paeth(top[3], left, top_left, left_3_le_top,
                         left_le_top_left_3, top_le_top_left_3);
        vst1q_u8(dest + 48, result_3);
      }
    }

    dest += stride;
  } while (++y != height);
}

#define PAETH_NXM_WIDE(W, H)                                                \
  void aom_paeth_predictor_##W##x##H##_neon(uint8_t *dst, ptrdiff_t stride, \
                                            const uint8_t *above,           \
                                            const uint8_t *left) {          \
    paeth16_plus_x_h_neon(dst, stride, above, left, W, H);                  \
  }

PAETH_NXM_WIDE(16, 8)
PAETH_NXM_WIDE(16, 16)
PAETH_NXM_WIDE(16, 32)
PAETH_NXM_WIDE(32, 16)
PAETH_NXM_WIDE(32, 32)
PAETH_NXM_WIDE(32, 64)
PAETH_NXM_WIDE(64, 32)
PAETH_NXM_WIDE(64, 64)

PAETH_NXM_WIDE(16, 4)
PAETH_NXM_WIDE(16, 64)
PAETH_NXM_WIDE(32, 8)
PAETH_NXM_WIDE(64, 16)
