/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
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

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/intrapred_common.h"

//------------------------------------------------------------------------------
// DC 4x4

static INLINE uint16x8_t dc_load_sum_4(const uint8_t *in) {
  const uint8x8_t a = load_u8_4x1_lane0(in);
  const uint16x4_t p0 = vpaddl_u8(a);
  const uint16x4_t p1 = vpadd_u16(p0, p0);
  return vcombine_u16(p1, vdup_n_u16(0));
}

static INLINE void dc_store_4xh(uint8_t *dst, ptrdiff_t stride, int h,
                                uint8x8_t dc) {
  for (int i = 0; i < h; ++i) {
    store_u8_4x1(dst + i * stride, dc, 0);
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
  uint8x8_t a = load_u8_4x1_lane0(above);
  uint8x8_t l = vld1_u8(left);
  uint32_t sum = horizontal_add_u16x8(vaddl_u8(a, l));
  uint32_t dc = calculate_dc_from_sum(4, 8, sum, 2, DC_MULTIPLIER_1X2);
  dc_store_4xh(dst, stride, 8, vdup_n_u8(dc));
}

void aom_dc_predictor_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = vld1_u8(above);
  uint8x8_t l = load_u8_4x1_lane0(left);
  uint32_t sum = horizontal_add_u16x8(vaddl_u8(a, l));
  uint32_t dc = calculate_dc_from_sum(8, 4, sum, 2, DC_MULTIPLIER_1X2);
  dc_store_8xh(dst, stride, 4, vdup_n_u8(dc));
}

void aom_dc_predictor_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x8_t a = load_u8_4x1_lane0(above);
  uint8x16_t l = vld1q_u8(left);
  uint16x8_t sum_al = vaddw_u8(vpaddlq_u8(l), a);
  uint32_t sum = horizontal_add_u16x8(sum_al);
  uint32_t dc = calculate_dc_from_sum(4, 16, sum, 2, DC_MULTIPLIER_1X4);
  dc_store_4xh(dst, stride, 16, vdup_n_u8(dc));
}

void aom_dc_predictor_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  uint8x16_t a = vld1q_u8(above);
  uint8x8_t l = load_u8_4x1_lane0(left);
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
    store_u8_4x1(dst + i * stride, d0, 0);
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
  v_store_4xh(dst, stride, 4, load_u8_4x1_lane0(above));
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
  v_store_4xh(dst, stride, 8, load_u8_4x1_lane0(above));
}

void aom_v_predictor_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_store_4xh(dst, stride, 16, load_u8_4x1_lane0(above));
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
  store_u8_4x1(dst + 0 * stride, vdup_lane_u8(d0, 0), 0);
  store_u8_4x1(dst + 1 * stride, vdup_lane_u8(d0, 1), 0);
  store_u8_4x1(dst + 2 * stride, vdup_lane_u8(d0, 2), 0);
  store_u8_4x1(dst + 3 * stride, vdup_lane_u8(d0, 3), 0);
  store_u8_4x1(dst + 4 * stride, vdup_lane_u8(d0, 4), 0);
  store_u8_4x1(dst + 5 * stride, vdup_lane_u8(d0, 5), 0);
  store_u8_4x1(dst + 6 * stride, vdup_lane_u8(d0, 6), 0);
  store_u8_4x1(dst + 7 * stride, vdup_lane_u8(d0, 7), 0);
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
  const uint8x8_t d0 = load_u8_4x1_lane0(left);
  (void)above;
  store_u8_4x1(dst + 0 * stride, vdup_lane_u8(d0, 0), 0);
  store_u8_4x1(dst + 1 * stride, vdup_lane_u8(d0, 1), 0);
  store_u8_4x1(dst + 2 * stride, vdup_lane_u8(d0, 2), 0);
  store_u8_4x1(dst + 3 * stride, vdup_lane_u8(d0, 3), 0);
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
  const uint8x8_t d0 = load_u8_4x1_lane0(left);
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
  const uint8x8_t d0 = load_u8_4x1_lane0(left);
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

static DECLARE_ALIGNED(16, uint8_t, EvenOddMaskx[8][16]) = {
  { 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15 },
  { 0, 1, 3, 5, 7, 9, 11, 13, 0, 2, 4, 6, 8, 10, 12, 14 },
  { 0, 0, 2, 4, 6, 8, 10, 12, 0, 0, 3, 5, 7, 9, 11, 13 },
  { 0, 0, 0, 3, 5, 7, 9, 11, 0, 0, 0, 4, 6, 8, 10, 12 },
  { 0, 0, 0, 0, 4, 6, 8, 10, 0, 0, 0, 0, 5, 7, 9, 11 },
  { 0, 0, 0, 0, 0, 5, 7, 9, 0, 0, 0, 0, 0, 6, 8, 10 },
  { 0, 0, 0, 0, 0, 0, 6, 8, 0, 0, 0, 0, 0, 0, 7, 9 },
  { 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 8 }
};

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

  const uint16x8_t a16 = vdupq_n_u16(16);
  const uint8x8_t a_mbase_x = vdup_n_u8(above[max_base_x]);
  const uint8x8_t v_32 = vdup_n_u8(32);

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
    uint16x8_t a32 = vmlal_u8(a16, a01_128.val[0], v_32);
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

  const uint16x8_t a16 = vdupq_n_u16(16);
  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);
  const uint8x8_t v_32 = vdup_n_u8(32);
  const uint8x16_t v_zero = vdupq_n_u8(0);

  int x = dx;
  for (int r = 0; r < W; r++) {
    uint16x8x2_t res;
    uint16x8_t shift;
    uint8x16_t a0_128, a1_128;

    int base = x >> frac_bits;
    int base_max_diff = (max_base_x - base) >> upsample_above;
    if (base_max_diff <= 0) {
      for (int i = r; i < W; ++i) {
        dst[i] = a_mbase_x;  // save 4 values
      }
      return;
    }

    if (base_max_diff > H) base_max_diff = H;

    if (upsample_above) {
      uint8x8x2_t v_tmp_a0_128 = vld2_u8(above + base);
      a0_128 = vcombine_u8(v_tmp_a0_128.val[0], v_tmp_a0_128.val[1]);
      a1_128 = vextq_u8(a0_128, v_zero, 8);
      shift = vdupq_n_u16(((x << upsample_above) & 0x3f) >> 1);
    } else {
      a0_128 = vld1q_u8(above + base);
      a1_128 = vld1q_u8(above + base + 1);
      shift = vdupq_n_u16((x & 0x3f) >> 1);
    }
    uint16x8x2_t diff, a32;
    diff.val[0] = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
    diff.val[1] = vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
    a32.val[0] = vmlal_u8(a16, vget_low_u8(a0_128), v_32);
    a32.val[1] = vmlal_u8(a16, vget_high_u8(a0_128), v_32);
    res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shift);
    res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shift);
    uint8x16_t v_temp =
        vcombine_u8(vshrn_n_u16(res.val[0], 5), vshrn_n_u16(res.val[1], 5));

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
    int N, uint8x16x2_t *dstvec, const uint8_t *above, int upsample_above,
    int dx) {
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((32 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);
  const uint16x8_t a16 = vdupq_n_u16(16);
  const uint8x8_t v_32 = vdup_n_u8(32);

  int x = dx;
  for (int r = 0; r < N; r++) {
    uint8x16_t res16[2];

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

    for (int j = 0, jj = 0; j < 32; j += 16, jj++) {
      int mdiff = base_max_diff - j;
      if (mdiff <= 0) {
        res16[jj] = a_mbase_x;
      } else {
        uint16x8x2_t a32, diff, res;
        uint8x16_t a0_128, a1_128;
        a0_128 = vld1q_u8(above + base + j);
        a1_128 = vld1q_u8(above + base + j + 1);
        diff.val[0] = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
        diff.val[1] = vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
        a32.val[0] = vmlal_u8(a16, vget_low_u8(a0_128), v_32);
        a32.val[1] = vmlal_u8(a16, vget_high_u8(a0_128), v_32);
        res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shift);
        res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shift);

        res16[jj] =
            vcombine_u8(vshrn_n_u16(res.val[0], 5), vshrn_n_u16(res.val[1], 5));
      }
    }

    uint8x16x2_t mask;

    mask.val[0] = vld1q_u8(BaseMask[base_max_diff]);
    mask.val[1] = vld1q_u8(BaseMask[base_max_diff] + 16);
    dstvec[r].val[0] = vbslq_u8(mask.val[0], res16[0], a_mbase_x);
    dstvec[r].val[1] = vbslq_u8(mask.val[1], res16[1], a_mbase_x);
    x += dx;
  }
}

static void dr_prediction_z1_32xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  uint8x16x2_t dstvec[64];

  dr_prediction_z1_32xN_internal_neon(N, dstvec, above, upsample_above, dx);
  for (int i = 0; i < N; i++) {
    vst1q_u8(dst + stride * i, dstvec[i].val[0]);
    vst1q_u8(dst + stride * i + 16, dstvec[i].val[1]);
  }
}

static void dr_prediction_z1_64xN_neon(int N, uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above, int upsample_above,
                                       int dx) {
  // here upsample_above is 0 by design of av1_use_intra_edge_upsample
  (void)upsample_above;
  const int frac_bits = 6;
  const int max_base_x = ((64 + N) - 1);

  // pre-filter above pixels
  // store in temp buffers:
  //   above[x] * 32 + 16
  //   above[x+1] - above[x]
  // final pixels will be calculated as:
  //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

  const uint16x8_t a16 = vdupq_n_u16(16);
  const uint8x16_t a_mbase_x = vdupq_n_u8(above[max_base_x]);
  const uint8x16_t max_base_x128 = vdupq_n_u8(max_base_x);
  const uint8x8_t v_32 = vdup_n_u8(32);
  const uint8x16_t v_zero = vdupq_n_u8(0);
  const uint8x16_t step = vdupq_n_u8(16);

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
      int mdif = max_base_x - (base + j);
      if (mdif <= 0) {
        vst1q_u8(dst + j, a_mbase_x);
      } else {
        uint16x8x2_t a32, diff, res;
        uint8x16_t a0_128, a1_128, mask128, res128;
        a0_128 = vld1q_u8(above + base + j);
        a1_128 = vld1q_u8(above + base + 1 + j);
        diff.val[0] = vsubl_u8(vget_low_u8(a1_128), vget_low_u8(a0_128));
        diff.val[1] = vsubl_u8(vget_high_u8(a1_128), vget_high_u8(a0_128));
        a32.val[0] = vmlal_u8(a16, vget_low_u8(a0_128), v_32);
        a32.val[1] = vmlal_u8(a16, vget_high_u8(a0_128), v_32);
        res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shift);
        res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shift);
        uint8x16_t v_temp =
            vcombine_u8(vshrn_n_u16(res.val[0], 5), vshrn_n_u16(res.val[1], 5));

        mask128 = vcgtq_u8(vqsubq_u8(max_base_x128, base_inc128), v_zero);
        res128 = vbslq_u8(mask128, v_temp, a_mbase_x);
        vst1q_u8(dst + j, res128);

        base_inc128 = vaddq_u8(base_inc128, step);
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
    case 32:
      dr_prediction_z1_32xN_neon(bh, dst, stride, above, upsample_above, dx);
      break;
    case 64:
      dr_prediction_z1_64xN_neon(bh, dst, stride, above, upsample_above, dx);
      break;
    default: break;
  }
}

/* ---------------------P R E D I C T I O N   Z 2--------------------------- */

static DECLARE_ALIGNED(16, uint8_t, LoadMaskz2[4][16]) = {
  { 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
    0, 0, 0 },
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff }
};

static AOM_FORCE_INLINE void vector_shift_x4(uint8x8_t *vec, uint8x8_t *v_zero,
                                             int shift_value) {
  switch (shift_value) {
    case 1: *vec = vext_u8(*v_zero, *vec, 7); break;
    case 2: *vec = vext_u8(*v_zero, *vec, 6); break;
    case 3: *vec = vext_u8(*v_zero, *vec, 5); break;
    default: break;
  }
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
  uint16x8_t a0_x, a1_x, a32, diff;
  uint16x8_t v_32 = vdupq_n_u16(32);
  uint16x8_t v_zero = vdupq_n_u16(0);
  uint16x8_t a16 = vdupq_n_u16(16);

  uint8x8_t v_zero_u8 = vdup_n_u8(0);
  uint16x4_t v_c3f = vdup_n_u16(0x3f);
  uint16x4_t r6 = vcreate_u16(0x00C0008000400000);
  int16x4_t v_upsample_left = vdup_n_s16(upsample_left);
  int16x4_t v_upsample_above = vdup_n_s16(upsample_above);
  int16x4_t v_1234 = vcreate_s16(0x0004000300020001);
  int16x4_t dy64 = vdup_n_s16(dy);
  int16x4_t v_frac_bits_y = vdup_n_s16(-frac_bits_y);
  int16x4_t min_base_y64 = vdup_n_s16(min_base_y);

#if AOM_ARCH_AARCH64
  // Use ext rather than loading left + 14 directly to avoid over-read.
  const uint8x16_t left_m2 = vld1q_u8(left - 2);
  const uint8x16_t left_0 = vld1q_u8(left);
  const uint8x16_t left_14 = vextq_u8(left_0, left_0, 14);
  const uint8x16x2_t left_vals = { { left_m2, left_14 } };
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < N; r++) {
    uint16x8_t res, shift;
    uint8x8_t resx, resy;
    uint16x4x2_t v_shift;
    v_shift.val[1] = vdup_n_u16(0);
    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 4) {
      base_min_diff = 4;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    if (base_shift > 3) {
      a0_x = v_zero;
      a1_x = v_zero;
      v_shift.val[0] = vreinterpret_u16_u8(v_zero_u8);
      v_shift.val[1] = vreinterpret_u16_u8(v_zero_u8);
    } else {
      uint16x4_t ydx = vdup_n_u16(y * dx);

      if (upsample_above) {
        uint8x8x2_t v_tmp;
        v_tmp.val[0] = vld1_u8(above + base_x + base_shift);
        v_tmp.val[1] = vld1_u8(above + base_x + base_shift + 8);
        uint8x8_t v_index_low = vld1_u8(EvenOddMaskx[base_shift]);
        uint8x8_t v_index_high = vld1_u8(EvenOddMaskx[base_shift] + 8);
        a0_x = vmovl_u8(vtbl2_u8(v_tmp, v_index_low));
        a1_x = vmovl_u8(vtbl2_u8(v_tmp, v_index_high));
        v_shift.val[0] = vshr_n_u16(
            vand_u16(vshl_u16(vsub_u16(r6, ydx), v_upsample_above), v_c3f), 1);
      } else {
        uint8x8_t v_a0_x64 = vld1_u8(above + base_x + base_shift);
        vector_shift_x4(&v_a0_x64, &v_zero_u8, base_shift);
        uint8x8_t v_a1_x64 = vext_u8(v_a0_x64, v_zero_u8, 1);
        v_shift.val[0] = vshr_n_u16(vand_u16(vsub_u16(r6, ydx), v_c3f), 1);
        a0_x = vmovl_u8(v_a0_x64);
        a1_x = vmovl_u8(v_a1_x64);
      }
    }

    // y calc
    if (base_x < min_base_x) {
      int16x4_t v_r6 = vdup_n_s16(r << 6);
      int16x4_t y_c64 = vmls_s16(v_r6, v_1234, dy64);
      int16x4_t base_y_c64 = vshl_s16(y_c64, v_frac_bits_y);
      uint16x4_t mask64 = vcgt_s16(min_base_y64, base_y_c64);

      // Values in base_y_c64 range from -2 through 14 inclusive.
      base_y_c64 = vbic_s16(base_y_c64, vreinterpret_s16_u16(mask64));

#if AOM_ARCH_AARCH64
      uint8x8_t left_idx0 =
          vreinterpret_u8_s16(vadd_s16(base_y_c64, vdup_n_s16(2)));  // [0, 16]
      uint8x8_t left_idx1 =
          vreinterpret_u8_s16(vadd_s16(base_y_c64, vdup_n_s16(3)));  // [1, 17]

      uint8x8_t a0_y = vtrn1_u8(vqtbl2_u8(left_vals, left_idx0), v_zero_u8);
      uint8x8_t a1_y = vtrn1_u8(vqtbl2_u8(left_vals, left_idx1), v_zero_u8);
#else   // !AOM_ARCH_AARCH64
      DECLARE_ALIGNED(32, int16_t, base_y_c[4]);

      vst1_s16(base_y_c, base_y_c64);
      uint8x8_t a0_y = vdup_n_u8(0);
      a0_y = vld1_lane_u8(left + base_y_c[0], a0_y, 0);
      a0_y = vld1_lane_u8(left + base_y_c[1], a0_y, 2);
      a0_y = vld1_lane_u8(left + base_y_c[2], a0_y, 4);
      a0_y = vld1_lane_u8(left + base_y_c[3], a0_y, 6);

      base_y_c64 = vadd_s16(base_y_c64, vdup_n_s16(1));
      vst1_s16(base_y_c, base_y_c64);
      uint8x8_t a1_y = vdup_n_u8(0);
      a1_y = vld1_lane_u8(left + base_y_c[0], a1_y, 0);
      a1_y = vld1_lane_u8(left + base_y_c[1], a1_y, 2);
      a1_y = vld1_lane_u8(left + base_y_c[2], a1_y, 4);
      a1_y = vld1_lane_u8(left + base_y_c[3], a1_y, 6);
#endif  // AOM_ARCH_AARCH64

      if (upsample_left) {
        v_shift.val[1] = vshr_n_u16(
            vand_u16(vshl_u16(vreinterpret_u16_s16(y_c64), v_upsample_left),
                     v_c3f),
            1);
      } else {
        v_shift.val[1] =
            vshr_n_u16(vand_u16(vreinterpret_u16_s16(y_c64), v_c3f), 1);
      }

      a0_x = vcombine_u16(vget_low_u16(a0_x), vreinterpret_u16_u8(a0_y));
      a1_x = vcombine_u16(vget_low_u16(a1_x), vreinterpret_u16_u8(a1_y));
    }
    shift = vcombine_u16(v_shift.val[0], v_shift.val[1]);
    diff = vsubq_u16(a1_x, a0_x);      // a[x+1] - a[x]
    a32 = vmlaq_u16(a16, a0_x, v_32);  // a[x] * 32 + 16
    res = vmlaq_u16(a32, diff, shift);
    resx = vshrn_n_u16(res, 5);
    resy = vext_u8(resx, v_zero_u8, 4);

    uint8x8_t mask = vld1_u8(BaseMask[base_min_diff]);
    uint8x8_t v_resxy = vbsl_u8(mask, resy, resx);
    vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(v_resxy), 0);

    dst += stride;
  }
}

static AOM_FORCE_INLINE void vector_shuffle(uint8x16_t *vec, uint8x16_t *vzero,
                                            int shift_value) {
  switch (shift_value) {
    case 1: *vec = vextq_u8(*vzero, *vec, 15); break;
    case 2: *vec = vextq_u8(*vzero, *vec, 14); break;
    case 3: *vec = vextq_u8(*vzero, *vec, 13); break;
    case 4: *vec = vextq_u8(*vzero, *vec, 12); break;
    case 5: *vec = vextq_u8(*vzero, *vec, 11); break;
    case 6: *vec = vextq_u8(*vzero, *vec, 10); break;
    case 7: *vec = vextq_u8(*vzero, *vec, 9); break;
    case 8: *vec = vextq_u8(*vzero, *vec, 8); break;
    case 9: *vec = vextq_u8(*vzero, *vec, 7); break;
    case 10: *vec = vextq_u8(*vzero, *vec, 6); break;
    case 11: *vec = vextq_u8(*vzero, *vec, 5); break;
    case 12: *vec = vextq_u8(*vzero, *vec, 4); break;
    case 13: *vec = vextq_u8(*vzero, *vec, 3); break;
    case 14: *vec = vextq_u8(*vzero, *vec, 2); break;
    case 15: *vec = vextq_u8(*vzero, *vec, 1); break;
    default: break;
  }
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
  uint16x8x2_t diff, a32;
  uint8x16_t v_zero = vdupq_n_u8(0);
  int16x8_t v_upsample_left = vdupq_n_s16(upsample_left);
  int16x8_t v_upsample_above = vdupq_n_s16(upsample_above);
  int16x8_t v_frac_bits_y = vdupq_n_s16(-frac_bits_y);

  uint16x8_t a16 = vdupq_n_u16(16);
  uint16x8_t c3f = vdupq_n_u16(0x3f);
  int16x8_t min_base_y128 = vdupq_n_s16(min_base_y);
  int16x8_t dy128 = vdupq_n_s16(dy);
  uint16x8_t c1234 = vcombine_u16(vcreate_u16(0x0004000300020001),
                                  vcreate_u16(0x0008000700060005));

#if AOM_ARCH_AARCH64
  // Use ext rather than loading left + 30 directly to avoid over-read.
  const uint8x16_t left_m2 = vld1q_u8(left - 2);
  const uint8x16_t left_0 = vld1q_u8(left + 0);
  const uint8x16_t left_16 = vld1q_u8(left + 16);
  const uint8x16_t left_14 = vextq_u8(left_0, left_16, 14);
  const uint8x16_t left_30 = vextq_u8(left_16, left_16, 14);
  const uint8x16x3_t left_vals = { { left_m2, left_14, left_30 } };
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < N; r++) {
    uint8x8_t resx, resy, resxy;
    uint16x8x2_t res, shift;
    shift.val[1] = vdupq_n_u16(0);

    int y = r + 1;
    int base_x = (-y * dx) >> frac_bits_x;
    int base_shift = 0;
    if (base_x < (min_base_x - 1)) {
      base_shift = (min_base_x - base_x - 1) >> upsample_above;
    }
    int base_min_diff =
        (min_base_x - base_x + upsample_above) >> upsample_above;
    if (base_min_diff > 8) {
      base_min_diff = 8;
    } else {
      if (base_min_diff < 0) base_min_diff = 0;
    }

    uint8x8_t a0_x0, a1_x0;
    if (base_shift > 7) {
      a0_x0 = vdup_n_u8(0);
      a1_x0 = vdup_n_u8(0);
      shift.val[0] = vreinterpretq_u16_u8(v_zero);
      shift.val[1] = vreinterpretq_u16_u8(v_zero);
    } else {
      uint16x8_t ydx = vdupq_n_u16(y * dx);
      uint16x8_t r6 =
          vshlq_n_u16(vextq_u16(c1234, vreinterpretq_u16_u8(v_zero), 2), 6);

      if (upsample_above) {
        uint8x8x2_t v_tmp;
        v_tmp.val[0] = vld1_u8(above + base_x + base_shift);
        v_tmp.val[1] = vld1_u8(above + base_x + base_shift + 8);
        uint8x8_t v_index_low = vld1_u8(EvenOddMaskx[base_shift]);
        uint8x8_t v_index_high = vld1_u8(EvenOddMaskx[base_shift] + 8);
        shift.val[0] = vshrq_n_u16(
            vandq_u16(vshlq_u16(vsubq_u16(r6, ydx), v_upsample_above), c3f), 1);
        a0_x0 = vtbl2_u8(v_tmp, v_index_low);
        a1_x0 = vtbl2_u8(v_tmp, v_index_high);
      } else {
        uint8x16_t a0_x128, a1_x128;
        a0_x128 = vld1q_u8(above + base_x + base_shift);
        a1_x128 = vextq_u8(a0_x128, v_zero, 1);
        vector_shuffle(&a0_x128, &v_zero, base_shift);
        vector_shuffle(&a1_x128, &v_zero, base_shift);
        shift.val[0] = vshrq_n_u16(vandq_u16(vsubq_u16(r6, ydx), c3f), 1);
        a0_x0 = vget_low_u8(a0_x128);
        a1_x0 = vget_low_u8(a1_x128);
      }
    }

    diff.val[0] = vsubl_u8(a1_x0, a0_x0);              // a[x+1] - a[x]
    a32.val[0] = vmlal_u8(a16, a0_x0, vdup_n_u8(32));  // a[x] * 32 + 16
    res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shift.val[0]);
    resx = vshrn_n_u16(res.val[0], 5);

    // y calc
    if (base_x < min_base_x) {
      int16x8_t y_c128, base_y_c128;
      uint16x8_t mask128;
      int16x8_t v_r6 = vdupq_n_s16(r << 6);

      y_c128 = vmlsq_s16(v_r6, vreinterpretq_s16_u16(c1234), dy128);
      base_y_c128 = vshlq_s16(y_c128, v_frac_bits_y);
      mask128 = vcgtq_s16(min_base_y128, base_y_c128);

      // Values in base_y_c128 range from -2 through 31 inclusive.
      base_y_c128 = vbicq_s16(base_y_c128, vreinterpretq_s16_u16(mask128));

#if AOM_ARCH_AARCH64
      uint8x16_t left_idx0 = vreinterpretq_u8_s16(
          vaddq_s16(base_y_c128, vdupq_n_s16(2)));  // [0, 33]
      uint8x16_t left_idx1 = vreinterpretq_u8_s16(
          vaddq_s16(base_y_c128, vdupq_n_s16(3)));  // [1, 34]
      uint8x16_t left_idx01 = vuzp1q_u8(left_idx0, left_idx1);

      uint8x16_t a01_x = vqtbl3q_u8(left_vals, left_idx01);
      uint8x8_t a0_x1 = vget_low_u8(a01_x);
      uint8x8_t a1_x1 = vget_high_u8(a01_x);
#else   // !AOM_ARCH_AARCH64
      DECLARE_ALIGNED(32, int16_t, base_y_c[16]);

      vst1q_s16(base_y_c, base_y_c128);
      uint8x8_t a0_x1 = vdup_n_u8(0);
      a0_x1 = vld1_lane_u8(left + base_y_c[0], a0_x1, 0);
      a0_x1 = vld1_lane_u8(left + base_y_c[1], a0_x1, 1);
      a0_x1 = vld1_lane_u8(left + base_y_c[2], a0_x1, 2);
      a0_x1 = vld1_lane_u8(left + base_y_c[3], a0_x1, 3);
      a0_x1 = vld1_lane_u8(left + base_y_c[4], a0_x1, 4);
      a0_x1 = vld1_lane_u8(left + base_y_c[5], a0_x1, 5);
      a0_x1 = vld1_lane_u8(left + base_y_c[6], a0_x1, 6);
      a0_x1 = vld1_lane_u8(left + base_y_c[7], a0_x1, 7);

      base_y_c128 = vaddq_s16(base_y_c128, vdupq_n_s16(1));
      vst1q_s16(base_y_c, base_y_c128);
      uint8x8_t a1_x1 = vdup_n_u8(0);
      a1_x1 = vld1_lane_u8(left + base_y_c[0], a1_x1, 0);
      a1_x1 = vld1_lane_u8(left + base_y_c[1], a1_x1, 1);
      a1_x1 = vld1_lane_u8(left + base_y_c[2], a1_x1, 2);
      a1_x1 = vld1_lane_u8(left + base_y_c[3], a1_x1, 3);
      a1_x1 = vld1_lane_u8(left + base_y_c[4], a1_x1, 4);
      a1_x1 = vld1_lane_u8(left + base_y_c[5], a1_x1, 5);
      a1_x1 = vld1_lane_u8(left + base_y_c[6], a1_x1, 6);
      a1_x1 = vld1_lane_u8(left + base_y_c[7], a1_x1, 7);
#endif  // AOM_ARCH_AARCH64

      if (upsample_left) {
        shift.val[1] = vshrq_n_u16(
            vandq_u16(vshlq_u16(vreinterpretq_u16_s16(y_c128), v_upsample_left),
                      c3f),
            1);
      } else {
        shift.val[1] =
            vshrq_n_u16(vandq_u16(vreinterpretq_u16_s16(y_c128), c3f), 1);
      }

      diff.val[1] = vsubl_u8(a1_x1, a0_x1);
      a32.val[1] = vmlal_u8(a16, a0_x1, vdup_n_u8(32));
      res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shift.val[1]);
      resy = vshrn_n_u16(res.val[1], 5);
      uint8x8_t mask = vld1_u8(BaseMask[base_min_diff]);
      resxy = vbsl_u8(mask, resy, resx);
      vst1_u8(dst, resxy);
    } else {
      vst1_u8(dst, resx);
    }

    dst += stride;
  }
}

static void dr_prediction_z2_HxW_neon(int H, int W, uint8_t *dst,
                                      ptrdiff_t stride, const uint8_t *above,
                                      const uint8_t *left, int upsample_above,
                                      int upsample_left, int dx, int dy) {
  // here upsample_above and upsample_left are 0 by design of
  // av1_use_intra_edge_upsample
  const int min_base_x = -1;
  const int min_base_y = -1;
  (void)upsample_above;
  (void)upsample_left;
  const int frac_bits_x = 6;
  const int frac_bits_y = 6;

  uint16x8x2_t a32, c0123, c1234, diff, shifty;
  uint8x16x2_t a0_x, a1_x;
  uint16x8_t v_32 = vdupq_n_u16(32);
  uint8x16_t v_zero = vdupq_n_u8(0);
  int16x8_t v_frac_bits_y = vdupq_n_s16(-frac_bits_y);

  uint16x8_t a16 = vdupq_n_u16(16);
  uint16x8_t c1 = vshrq_n_u16(a16, 4);
  int16x8_t min_base_y256 = vdupq_n_s16(min_base_y);
  uint16x8_t c3f = vdupq_n_u16(0x3f);
  int16x8_t dy256 = vdupq_n_s16(dy);
  c0123.val[0] = vcombine_u16(vcreate_u16(0x0003000200010000),
                              vcreate_u16(0x0007000600050004));
  c0123.val[1] = vcombine_u16(vcreate_u16(0x000B000A00090008),
                              vcreate_u16(0x000F000E000D000C));
  c1234.val[0] = vaddq_u16(c0123.val[0], c1);
  c1234.val[1] = vaddq_u16(c0123.val[1], c1);

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
#endif  // AOM_ARCH_AARCH64

  for (int r = 0; r < H; r++) {
    uint16x8x2_t res, r6, shift;
    uint16x8_t j256;
    uint8x16_t resx, resy, resxy;
    int y = r + 1;
    uint16x8_t ydx = vdupq_n_u16((uint16_t)(y * dx));

    int base_x = (-y * dx) >> frac_bits_x;
    for (int j = 0; j < W; j += 16) {
      j256 = vdupq_n_u16(j);

      int base_shift = 0;
      if ((base_x + j) < (min_base_x - 1)) {
        base_shift = (min_base_x - (base_x + j) - 1);
      }
      int base_min_diff = (min_base_x - base_x - j);
      if (base_min_diff > 16) {
        base_min_diff = 16;
      } else {
        if (base_min_diff < 0) base_min_diff = 0;
      }

      if (base_shift < 16) {
        uint8x16_t a0_x128, a1_x128;
        a0_x128 = vld1q_u8(above + base_x + base_shift + j);
        a1_x128 = vld1q_u8(above + base_x + base_shift + 1 + j);
        vector_shuffle(&a0_x128, &v_zero, base_shift);
        vector_shuffle(&a1_x128, &v_zero, base_shift);
        a0_x = vzipq_u8(a0_x128, v_zero);
        a1_x = vzipq_u8(a1_x128, v_zero);
        r6.val[0] = vshlq_n_u16(vaddq_u16(c0123.val[0], j256), 6);
        r6.val[1] = vshlq_n_u16(vaddq_u16(c0123.val[1], j256), 6);
        shift.val[0] =
            vshrq_n_u16(vandq_u16(vsubq_u16(r6.val[0], ydx), c3f), 1);
        shift.val[1] =
            vshrq_n_u16(vandq_u16(vsubq_u16(r6.val[1], ydx), c3f), 1);
        diff.val[0] =
            vsubq_u16(vreinterpretq_u16_u8(a1_x.val[0]),
                      vreinterpretq_u16_u8(a0_x.val[0]));  // a[x+1] - a[x]
        diff.val[1] =
            vsubq_u16(vreinterpretq_u16_u8(a1_x.val[1]),
                      vreinterpretq_u16_u8(a0_x.val[1]));  // a[x+1] - a[x]
        a32.val[0] = vmlaq_u16(a16, vreinterpretq_u16_u8(a0_x.val[0]),
                               v_32);  // a[x] * 32 + 16
        a32.val[1] = vmlaq_u16(a16, vreinterpretq_u16_u8(a0_x.val[1]),
                               v_32);  // a[x] * 32 + 16
        res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shift.val[0]);
        res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shift.val[1]);
        resx =
            vcombine_u8(vshrn_n_u16(res.val[0], 5), vshrn_n_u16(res.val[1], 5));
      } else {
        resx = v_zero;
      }

      // y calc
      if (base_x < min_base_x) {
        uint16x8x2_t mask256;
        int16x8x2_t c256, y_c256, base_y_c256, mul16;
        int16x8_t v_r6 = vdupq_n_s16(r << 6);

        c256.val[0] = vaddq_s16(vreinterpretq_s16_u16(j256),
                                vreinterpretq_s16_u16(c1234.val[0]));
        c256.val[1] = vaddq_s16(vreinterpretq_s16_u16(j256),
                                vreinterpretq_s16_u16(c1234.val[1]));
        mul16.val[0] = vreinterpretq_s16_u16(
            vminq_u16(vreinterpretq_u16_s16(vmulq_s16(c256.val[0], dy256)),
                      vshrq_n_u16(vreinterpretq_u16_s16(min_base_y256), 1)));
        mul16.val[1] = vreinterpretq_s16_u16(
            vminq_u16(vreinterpretq_u16_s16(vmulq_s16(c256.val[1], dy256)),
                      vshrq_n_u16(vreinterpretq_u16_s16(min_base_y256), 1)));
        y_c256.val[0] = vsubq_s16(v_r6, mul16.val[0]);
        y_c256.val[1] = vsubq_s16(v_r6, mul16.val[1]);

        base_y_c256.val[0] = vshlq_s16(y_c256.val[0], v_frac_bits_y);
        base_y_c256.val[1] = vshlq_s16(y_c256.val[1], v_frac_bits_y);
        mask256.val[0] = vcgtq_s16(min_base_y256, base_y_c256.val[0]);
        mask256.val[1] = vcgtq_s16(min_base_y256, base_y_c256.val[1]);

        base_y_c256.val[0] =
            vbslq_s16(mask256.val[0], min_base_y256, base_y_c256.val[0]);
        base_y_c256.val[1] =
            vbslq_s16(mask256.val[1], min_base_y256, base_y_c256.val[1]);

        int16_t min_y = vgetq_lane_s16(base_y_c256.val[1], 7);
        int16_t max_y = vgetq_lane_s16(base_y_c256.val[0], 0);
        int16_t offset_diff = max_y - min_y;

        uint8x8_t a0_y0;
        uint8x8_t a0_y1;
        uint8x8_t a1_y0;
        uint8x8_t a1_y1;

        if (offset_diff < 16) {
          assert(offset_diff >= 0);
          int16x8_t min_y256 =
              vdupq_lane_s16(vget_high_s16(base_y_c256.val[1]), 3);

          int16x8x2_t base_y_offset;
          base_y_offset.val[0] = vsubq_s16(base_y_c256.val[0], min_y256);
          base_y_offset.val[1] = vsubq_s16(base_y_c256.val[1], min_y256);

          int8x16_t base_y_offset128 =
              vcombine_s8(vqmovn_s16(base_y_offset.val[0]),
                          vqmovn_s16(base_y_offset.val[1]));

          uint8x16_t a0_y128, a1_y128;
          uint8x16_t v_loadmaskz2 = vld1q_u8(LoadMaskz2[offset_diff / 4]);
          a0_y128 = vld1q_u8(left + min_y);
          a0_y128 = vandq_u8(a0_y128, v_loadmaskz2);
          a1_y128 = vld1q_u8(left + min_y + 1);
          a1_y128 = vandq_u8(a1_y128, v_loadmaskz2);
#if AOM_ARCH_AARCH64
          a0_y128 = vqtbl1q_u8(a0_y128, vreinterpretq_u8_s8(base_y_offset128));
          a1_y128 = vqtbl1q_u8(a1_y128, vreinterpretq_u8_s8(base_y_offset128));
#else
          uint8x8x2_t v_tmp;
          uint8x8x2_t v_res;
          uint8x8_t v_index_low =
              vget_low_u8(vreinterpretq_u8_s8(base_y_offset128));
          uint8x8_t v_index_high =
              vget_high_u8(vreinterpretq_u8_s8(base_y_offset128));
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
#endif
          a0_y0 = vget_low_u8(a0_y128);
          a0_y1 = vget_high_u8(a0_y128);
          a1_y0 = vget_low_u8(a1_y128);
          a1_y1 = vget_high_u8(a1_y128);
        } else {
          // Values in base_y_c256 range from -1 through 62 inclusive.
          base_y_c256.val[0] = vbicq_s16(base_y_c256.val[0],
                                         vreinterpretq_s16_u16(mask256.val[0]));
          base_y_c256.val[1] = vbicq_s16(base_y_c256.val[1],
                                         vreinterpretq_s16_u16(mask256.val[1]));

#if AOM_ARCH_AARCH64
          // Values in left_idx{0,1} range from 0 through 63 inclusive.
          uint8x16_t left_idx0 = vreinterpretq_u8_s16(
              vaddq_s16(base_y_c256.val[0], vdupq_n_s16(1)));
          uint8x16_t left_idx1 = vreinterpretq_u8_s16(
              vaddq_s16(base_y_c256.val[1], vdupq_n_s16(1)));

          uint8x16_t left_idx01 = vuzp1q_u8(left_idx0, left_idx1);

          uint8x16_t a0_y01 = vqtbl4q_u8(left_vals0, left_idx01);
          uint8x16_t a1_y01 = vqtbl4q_u8(left_vals1, left_idx01);

          a0_y0 = vget_low_u8(a0_y01);
          a0_y1 = vget_high_u8(a0_y01);
          a1_y0 = vget_low_u8(a1_y01);
          a1_y1 = vget_high_u8(a1_y01);
#else   // !AOM_ARCH_AARCH64
          DECLARE_ALIGNED(32, int16_t, base_y_c[16]);

          vst1q_s16(base_y_c, base_y_c256.val[0]);
          vst1q_s16(base_y_c + 8, base_y_c256.val[1]);
          a0_y0 = vdup_n_u8(0);
          a0_y0 = vld1_lane_u8(left + base_y_c[0], a0_y0, 0);
          a0_y0 = vld1_lane_u8(left + base_y_c[1], a0_y0, 1);
          a0_y0 = vld1_lane_u8(left + base_y_c[2], a0_y0, 2);
          a0_y0 = vld1_lane_u8(left + base_y_c[3], a0_y0, 3);
          a0_y0 = vld1_lane_u8(left + base_y_c[4], a0_y0, 4);
          a0_y0 = vld1_lane_u8(left + base_y_c[5], a0_y0, 5);
          a0_y0 = vld1_lane_u8(left + base_y_c[6], a0_y0, 6);
          a0_y0 = vld1_lane_u8(left + base_y_c[7], a0_y0, 7);
          a0_y1 = vdup_n_u8(0);
          a0_y1 = vld1_lane_u8(left + base_y_c[8], a0_y1, 0);
          a0_y1 = vld1_lane_u8(left + base_y_c[9], a0_y1, 1);
          a0_y1 = vld1_lane_u8(left + base_y_c[10], a0_y1, 2);
          a0_y1 = vld1_lane_u8(left + base_y_c[11], a0_y1, 3);
          a0_y1 = vld1_lane_u8(left + base_y_c[12], a0_y1, 4);
          a0_y1 = vld1_lane_u8(left + base_y_c[13], a0_y1, 5);
          a0_y1 = vld1_lane_u8(left + base_y_c[14], a0_y1, 6);
          a0_y1 = vld1_lane_u8(left + base_y_c[15], a0_y1, 7);

          base_y_c256.val[0] =
              vaddq_s16(base_y_c256.val[0], vreinterpretq_s16_u16(c1));
          base_y_c256.val[1] =
              vaddq_s16(base_y_c256.val[1], vreinterpretq_s16_u16(c1));

          vst1q_s16(base_y_c, base_y_c256.val[0]);
          vst1q_s16(base_y_c + 8, base_y_c256.val[1]);
          a1_y0 = vdup_n_u8(0);
          a1_y0 = vld1_lane_u8(left + base_y_c[0], a1_y0, 0);
          a1_y0 = vld1_lane_u8(left + base_y_c[1], a1_y0, 1);
          a1_y0 = vld1_lane_u8(left + base_y_c[2], a1_y0, 2);
          a1_y0 = vld1_lane_u8(left + base_y_c[3], a1_y0, 3);
          a1_y0 = vld1_lane_u8(left + base_y_c[4], a1_y0, 4);
          a1_y0 = vld1_lane_u8(left + base_y_c[5], a1_y0, 5);
          a1_y0 = vld1_lane_u8(left + base_y_c[6], a1_y0, 6);
          a1_y0 = vld1_lane_u8(left + base_y_c[7], a1_y0, 7);
          a1_y1 = vdup_n_u8(0);
          a1_y1 = vld1_lane_u8(left + base_y_c[8], a1_y1, 0);
          a1_y1 = vld1_lane_u8(left + base_y_c[9], a1_y1, 1);
          a1_y1 = vld1_lane_u8(left + base_y_c[10], a1_y1, 2);
          a1_y1 = vld1_lane_u8(left + base_y_c[11], a1_y1, 3);
          a1_y1 = vld1_lane_u8(left + base_y_c[12], a1_y1, 4);
          a1_y1 = vld1_lane_u8(left + base_y_c[13], a1_y1, 5);
          a1_y1 = vld1_lane_u8(left + base_y_c[14], a1_y1, 6);
          a1_y1 = vld1_lane_u8(left + base_y_c[15], a1_y1, 7);
#endif  // AOM_ARCH_AARCH64
        }

        shifty.val[0] = vshrq_n_u16(
            vandq_u16(vreinterpretq_u16_s16(y_c256.val[0]), c3f), 1);
        shifty.val[1] = vshrq_n_u16(
            vandq_u16(vreinterpretq_u16_s16(y_c256.val[1]), c3f), 1);
        diff.val[0] = vsubl_u8(a1_y0, a0_y0);              // a[x+1] - a[x]
        diff.val[1] = vsubl_u8(a1_y1, a0_y1);              // a[x+1] - a[x]
        a32.val[0] = vmlal_u8(a16, a0_y0, vdup_n_u8(32));  // a[x] * 32 + 16
        a32.val[1] = vmlal_u8(a16, a0_y1, vdup_n_u8(32));  // a[x] * 32 + 16
        res.val[0] = vmlaq_u16(a32.val[0], diff.val[0], shifty.val[0]);
        res.val[1] = vmlaq_u16(a32.val[1], diff.val[1], shifty.val[1]);

        resy =
            vcombine_u8(vshrn_n_u16(res.val[0], 5), vshrn_n_u16(res.val[1], 5));
      } else {
        resy = v_zero;
      }
      uint8x16_t mask = vld1q_u8(BaseMask[base_min_diff]);
      resxy = vbslq_u8(mask, resy, resx);
      vst1q_u8(dst + j, resxy);
    }  // for j
    dst += stride;
  }
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
      dr_prediction_z2_HxW_neon(bh, bw, dst, stride, above, left,
                                upsample_above, upsample_left, dx, dy);
      break;
  }
}

/* ---------------------P R E D I C T I O N   Z 3--------------------------- */

static AOM_FORCE_INLINE void transpose4x16_neon(uint8x16_t *x,
                                                uint16x8x2_t *d) {
  uint8x16x2_t w0, w1;

  w0 = vzipq_u8(x[0], x[1]);
  w1 = vzipq_u8(x[2], x[3]);

  d[0] = vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                   vreinterpretq_u16_u8(w1.val[0]));
  d[1] = vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                   vreinterpretq_u16_u8(w1.val[1]));
}

static AOM_FORCE_INLINE void transpose4x8_8x4_low_neon(uint8x8_t *x,
                                                       uint16x4x2_t *d) {
  uint8x8x2_t w0, w1;

  w0 = vzip_u8(x[0], x[1]);
  w1 = vzip_u8(x[2], x[3]);

  *d = vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0]));
}

static AOM_FORCE_INLINE void transpose4x8_8x4_neon(uint8x8_t *x,
                                                   uint16x4x2_t *d) {
  uint8x8x2_t w0, w1;

  w0 = vzip_u8(x[0], x[1]);
  w1 = vzip_u8(x[2], x[3]);

  d[0] =
      vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0]));
  d[1] =
      vzip_u16(vreinterpret_u16_u8(w0.val[1]), vreinterpret_u16_u8(w1.val[1]));
}

static AOM_FORCE_INLINE void transpose8x8_low_neon(uint8x8_t *x,
                                                   uint32x2x2_t *d) {
  uint8x8x2_t w0, w1, w2, w3;
  uint16x4x2_t w4, w5;

  w0 = vzip_u8(x[0], x[1]);
  w1 = vzip_u8(x[2], x[3]);
  w2 = vzip_u8(x[4], x[5]);
  w3 = vzip_u8(x[6], x[7]);

  w4 = vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0]));
  w5 = vzip_u16(vreinterpret_u16_u8(w2.val[0]), vreinterpret_u16_u8(w3.val[0]));

  d[0] = vzip_u32(vreinterpret_u32_u16(w4.val[0]),
                  vreinterpret_u32_u16(w5.val[0]));
  d[1] = vzip_u32(vreinterpret_u32_u16(w4.val[1]),
                  vreinterpret_u32_u16(w5.val[1]));
}

static AOM_FORCE_INLINE void transpose8x8_neon(uint8x8_t *x, uint32x2x2_t *d) {
  uint8x8x2_t w0, w1, w2, w3;
  uint16x4x2_t w4, w5, w6, w7;

  w0 = vzip_u8(x[0], x[1]);
  w1 = vzip_u8(x[2], x[3]);
  w2 = vzip_u8(x[4], x[5]);
  w3 = vzip_u8(x[6], x[7]);

  w4 = vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0]));
  w5 = vzip_u16(vreinterpret_u16_u8(w2.val[0]), vreinterpret_u16_u8(w3.val[0]));

  d[0] = vzip_u32(vreinterpret_u32_u16(w4.val[0]),
                  vreinterpret_u32_u16(w5.val[0]));
  d[1] = vzip_u32(vreinterpret_u32_u16(w4.val[1]),
                  vreinterpret_u32_u16(w5.val[1]));

  w6 = vzip_u16(vreinterpret_u16_u8(w0.val[1]), vreinterpret_u16_u8(w1.val[1]));
  w7 = vzip_u16(vreinterpret_u16_u8(w2.val[1]), vreinterpret_u16_u8(w3.val[1]));

  d[2] = vzip_u32(vreinterpret_u32_u16(w6.val[0]),
                  vreinterpret_u32_u16(w7.val[0]));
  d[3] = vzip_u32(vreinterpret_u32_u16(w6.val[1]),
                  vreinterpret_u32_u16(w7.val[1]));
}

static AOM_FORCE_INLINE void transpose16x8_8x16_neon(uint8x8_t *x,
                                                     uint64x2_t *d) {
  uint8x8x2_t w0, w1, w2, w3, w8, w9, w10, w11;
  uint16x4x2_t w4, w5, w12, w13;
  uint32x2x2_t w6, w7, w14, w15;

  w0 = vzip_u8(x[0], x[1]);
  w1 = vzip_u8(x[2], x[3]);
  w2 = vzip_u8(x[4], x[5]);
  w3 = vzip_u8(x[6], x[7]);

  w8 = vzip_u8(x[8], x[9]);
  w9 = vzip_u8(x[10], x[11]);
  w10 = vzip_u8(x[12], x[13]);
  w11 = vzip_u8(x[14], x[15]);

  w4 = vzip_u16(vreinterpret_u16_u8(w0.val[0]), vreinterpret_u16_u8(w1.val[0]));
  w5 = vzip_u16(vreinterpret_u16_u8(w2.val[0]), vreinterpret_u16_u8(w3.val[0]));
  w12 =
      vzip_u16(vreinterpret_u16_u8(w8.val[0]), vreinterpret_u16_u8(w9.val[0]));
  w13 = vzip_u16(vreinterpret_u16_u8(w10.val[0]),
                 vreinterpret_u16_u8(w11.val[0]));

  w6 = vzip_u32(vreinterpret_u32_u16(w4.val[0]),
                vreinterpret_u32_u16(w5.val[0]));
  w7 = vzip_u32(vreinterpret_u32_u16(w4.val[1]),
                vreinterpret_u32_u16(w5.val[1]));
  w14 = vzip_u32(vreinterpret_u32_u16(w12.val[0]),
                 vreinterpret_u32_u16(w13.val[0]));
  w15 = vzip_u32(vreinterpret_u32_u16(w12.val[1]),
                 vreinterpret_u32_u16(w13.val[1]));

  // Store first 4-line result
  d[0] = vcombine_u64(vreinterpret_u64_u32(w6.val[0]),
                      vreinterpret_u64_u32(w14.val[0]));
  d[1] = vcombine_u64(vreinterpret_u64_u32(w6.val[1]),
                      vreinterpret_u64_u32(w14.val[1]));
  d[2] = vcombine_u64(vreinterpret_u64_u32(w7.val[0]),
                      vreinterpret_u64_u32(w15.val[0]));
  d[3] = vcombine_u64(vreinterpret_u64_u32(w7.val[1]),
                      vreinterpret_u64_u32(w15.val[1]));

  w4 = vzip_u16(vreinterpret_u16_u8(w0.val[1]), vreinterpret_u16_u8(w1.val[1]));
  w5 = vzip_u16(vreinterpret_u16_u8(w2.val[1]), vreinterpret_u16_u8(w3.val[1]));
  w12 =
      vzip_u16(vreinterpret_u16_u8(w8.val[1]), vreinterpret_u16_u8(w9.val[1]));
  w13 = vzip_u16(vreinterpret_u16_u8(w10.val[1]),
                 vreinterpret_u16_u8(w11.val[1]));

  w6 = vzip_u32(vreinterpret_u32_u16(w4.val[0]),
                vreinterpret_u32_u16(w5.val[0]));
  w7 = vzip_u32(vreinterpret_u32_u16(w4.val[1]),
                vreinterpret_u32_u16(w5.val[1]));
  w14 = vzip_u32(vreinterpret_u32_u16(w12.val[0]),
                 vreinterpret_u32_u16(w13.val[0]));
  w15 = vzip_u32(vreinterpret_u32_u16(w12.val[1]),
                 vreinterpret_u32_u16(w13.val[1]));

  // Store second 4-line result
  d[4] = vcombine_u64(vreinterpret_u64_u32(w6.val[0]),
                      vreinterpret_u64_u32(w14.val[0]));
  d[5] = vcombine_u64(vreinterpret_u64_u32(w6.val[1]),
                      vreinterpret_u64_u32(w14.val[1]));
  d[6] = vcombine_u64(vreinterpret_u64_u32(w7.val[0]),
                      vreinterpret_u64_u32(w15.val[0]));
  d[7] = vcombine_u64(vreinterpret_u64_u32(w7.val[1]),
                      vreinterpret_u64_u32(w15.val[1]));
}

static AOM_FORCE_INLINE void transpose8x16_16x8_neon(uint8x16_t *x,
                                                     uint64x2_t *d) {
  uint8x16x2_t w0, w1, w2, w3;
  uint16x8x2_t w4, w5, w6, w7;
  uint32x4x2_t w8, w9, w10, w11;

  w0 = vzipq_u8(x[0], x[1]);
  w1 = vzipq_u8(x[2], x[3]);
  w2 = vzipq_u8(x[4], x[5]);
  w3 = vzipq_u8(x[6], x[7]);

  w4 = vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                 vreinterpretq_u16_u8(w1.val[0]));
  w5 = vzipq_u16(vreinterpretq_u16_u8(w2.val[0]),
                 vreinterpretq_u16_u8(w3.val[0]));
  w6 = vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                 vreinterpretq_u16_u8(w1.val[1]));
  w7 = vzipq_u16(vreinterpretq_u16_u8(w2.val[1]),
                 vreinterpretq_u16_u8(w3.val[1]));

  w8 = vzipq_u32(vreinterpretq_u32_u16(w4.val[0]),
                 vreinterpretq_u32_u16(w5.val[0]));
  w9 = vzipq_u32(vreinterpretq_u32_u16(w6.val[0]),
                 vreinterpretq_u32_u16(w7.val[0]));
  w10 = vzipq_u32(vreinterpretq_u32_u16(w4.val[1]),
                  vreinterpretq_u32_u16(w5.val[1]));
  w11 = vzipq_u32(vreinterpretq_u32_u16(w6.val[1]),
                  vreinterpretq_u32_u16(w7.val[1]));

#if AOM_ARCH_AARCH64
  d[0] = vzip1q_u64(vreinterpretq_u64_u32(w8.val[0]),
                    vreinterpretq_u64_u32(w9.val[0]));
  d[1] = vzip2q_u64(vreinterpretq_u64_u32(w8.val[0]),
                    vreinterpretq_u64_u32(w9.val[0]));
  d[2] = vzip1q_u64(vreinterpretq_u64_u32(w8.val[1]),
                    vreinterpretq_u64_u32(w9.val[1]));
  d[3] = vzip2q_u64(vreinterpretq_u64_u32(w8.val[1]),
                    vreinterpretq_u64_u32(w9.val[1]));
  d[4] = vzip1q_u64(vreinterpretq_u64_u32(w10.val[0]),
                    vreinterpretq_u64_u32(w11.val[0]));
  d[5] = vzip2q_u64(vreinterpretq_u64_u32(w10.val[0]),
                    vreinterpretq_u64_u32(w11.val[0]));
  d[6] = vzip1q_u64(vreinterpretq_u64_u32(w10.val[1]),
                    vreinterpretq_u64_u32(w11.val[1]));
  d[7] = vzip2q_u64(vreinterpretq_u64_u32(w10.val[1]),
                    vreinterpretq_u64_u32(w11.val[1]));
#else
  d[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w8.val[0]), vget_low_u32(w9.val[0])));
  d[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w8.val[0]), vget_high_u32(w9.val[0])));
  d[2] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w8.val[1]), vget_low_u32(w9.val[1])));
  d[3] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w8.val[1]), vget_high_u32(w9.val[1])));
  d[4] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w10.val[0]), vget_low_u32(w11.val[0])));
  d[5] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w10.val[0]), vget_high_u32(w11.val[0])));
  d[6] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w10.val[1]), vget_low_u32(w11.val[1])));
  d[7] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w10.val[1]), vget_high_u32(w11.val[1])));
#endif
}

static AOM_FORCE_INLINE void transpose16x16_neon(uint8x16_t *x, uint64x2_t *d) {
  uint8x16x2_t w0, w1, w2, w3, w4, w5, w6, w7;
  uint16x8x2_t w8, w9, w10, w11;
  uint32x4x2_t w12, w13, w14, w15;

  w0 = vzipq_u8(x[0], x[1]);
  w1 = vzipq_u8(x[2], x[3]);
  w2 = vzipq_u8(x[4], x[5]);
  w3 = vzipq_u8(x[6], x[7]);

  w4 = vzipq_u8(x[8], x[9]);
  w5 = vzipq_u8(x[10], x[11]);
  w6 = vzipq_u8(x[12], x[13]);
  w7 = vzipq_u8(x[14], x[15]);

  w8 = vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                 vreinterpretq_u16_u8(w1.val[0]));
  w9 = vzipq_u16(vreinterpretq_u16_u8(w2.val[0]),
                 vreinterpretq_u16_u8(w3.val[0]));
  w10 = vzipq_u16(vreinterpretq_u16_u8(w4.val[0]),
                  vreinterpretq_u16_u8(w5.val[0]));
  w11 = vzipq_u16(vreinterpretq_u16_u8(w6.val[0]),
                  vreinterpretq_u16_u8(w7.val[0]));

  w12 = vzipq_u32(vreinterpretq_u32_u16(w8.val[0]),
                  vreinterpretq_u32_u16(w9.val[0]));
  w13 = vzipq_u32(vreinterpretq_u32_u16(w10.val[0]),
                  vreinterpretq_u32_u16(w11.val[0]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w8.val[1]),
                  vreinterpretq_u32_u16(w9.val[1]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w10.val[1]),
                  vreinterpretq_u32_u16(w11.val[1]));

#if AOM_ARCH_AARCH64
  d[0] = vzip1q_u64(vreinterpretq_u64_u32(w12.val[0]),
                    vreinterpretq_u64_u32(w13.val[0]));
  d[1] = vzip2q_u64(vreinterpretq_u64_u32(w12.val[0]),
                    vreinterpretq_u64_u32(w13.val[0]));
  d[2] = vzip1q_u64(vreinterpretq_u64_u32(w12.val[1]),
                    vreinterpretq_u64_u32(w13.val[1]));
  d[3] = vzip2q_u64(vreinterpretq_u64_u32(w12.val[1]),
                    vreinterpretq_u64_u32(w13.val[1]));
  d[4] = vzip1q_u64(vreinterpretq_u64_u32(w14.val[0]),
                    vreinterpretq_u64_u32(w15.val[0]));
  d[5] = vzip2q_u64(vreinterpretq_u64_u32(w14.val[0]),
                    vreinterpretq_u64_u32(w15.val[0]));
  d[6] = vzip1q_u64(vreinterpretq_u64_u32(w14.val[1]),
                    vreinterpretq_u64_u32(w15.val[1]));
  d[7] = vzip2q_u64(vreinterpretq_u64_u32(w14.val[1]),
                    vreinterpretq_u64_u32(w15.val[1]));
#else
  d[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w12.val[0]), vget_low_u32(w13.val[0])));
  d[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w12.val[0]), vget_high_u32(w13.val[0])));
  d[2] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w12.val[1]), vget_low_u32(w13.val[1])));
  d[3] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w12.val[1]), vget_high_u32(w13.val[1])));
  d[4] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w14.val[0]), vget_low_u32(w15.val[0])));
  d[5] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w14.val[0]), vget_high_u32(w15.val[0])));
  d[6] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w14.val[1]), vget_low_u32(w15.val[1])));
  d[7] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w14.val[1]), vget_high_u32(w15.val[1])));
#endif

  // upper half
  w8 = vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                 vreinterpretq_u16_u8(w1.val[1]));
  w9 = vzipq_u16(vreinterpretq_u16_u8(w2.val[1]),
                 vreinterpretq_u16_u8(w3.val[1]));
  w10 = vzipq_u16(vreinterpretq_u16_u8(w4.val[1]),
                  vreinterpretq_u16_u8(w5.val[1]));
  w11 = vzipq_u16(vreinterpretq_u16_u8(w6.val[1]),
                  vreinterpretq_u16_u8(w7.val[1]));

  w12 = vzipq_u32(vreinterpretq_u32_u16(w8.val[0]),
                  vreinterpretq_u32_u16(w9.val[0]));
  w13 = vzipq_u32(vreinterpretq_u32_u16(w10.val[0]),
                  vreinterpretq_u32_u16(w11.val[0]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w8.val[1]),
                  vreinterpretq_u32_u16(w9.val[1]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w10.val[1]),
                  vreinterpretq_u32_u16(w11.val[1]));

#if AOM_ARCH_AARCH64
  d[8] = vzip1q_u64(vreinterpretq_u64_u32(w12.val[0]),
                    vreinterpretq_u64_u32(w13.val[0]));
  d[9] = vzip2q_u64(vreinterpretq_u64_u32(w12.val[0]),
                    vreinterpretq_u64_u32(w13.val[0]));
  d[10] = vzip1q_u64(vreinterpretq_u64_u32(w12.val[1]),
                     vreinterpretq_u64_u32(w13.val[1]));
  d[11] = vzip2q_u64(vreinterpretq_u64_u32(w12.val[1]),
                     vreinterpretq_u64_u32(w13.val[1]));
  d[12] = vzip1q_u64(vreinterpretq_u64_u32(w14.val[0]),
                     vreinterpretq_u64_u32(w15.val[0]));
  d[13] = vzip2q_u64(vreinterpretq_u64_u32(w14.val[0]),
                     vreinterpretq_u64_u32(w15.val[0]));
  d[14] = vzip1q_u64(vreinterpretq_u64_u32(w14.val[1]),
                     vreinterpretq_u64_u32(w15.val[1]));
  d[15] = vzip2q_u64(vreinterpretq_u64_u32(w14.val[1]),
                     vreinterpretq_u64_u32(w15.val[1]));
#else
  d[8] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w12.val[0]), vget_low_u32(w13.val[0])));
  d[9] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w12.val[0]), vget_high_u32(w13.val[0])));
  d[10] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w12.val[1]), vget_low_u32(w13.val[1])));
  d[11] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w12.val[1]), vget_high_u32(w13.val[1])));
  d[12] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w14.val[0]), vget_low_u32(w15.val[0])));
  d[13] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w14.val[0]), vget_high_u32(w15.val[0])));
  d[14] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w14.val[1]), vget_low_u32(w15.val[1])));
  d[15] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w14.val[1]), vget_high_u32(w15.val[1])));
#endif
}

static AOM_FORCE_INLINE void transpose16x32_neon(uint8x16x2_t *x,
                                                 uint64x2x2_t *d) {
  uint8x16x2_t w0, w1, w2, w3, w8, w9, w10, w11;
  uint16x8x2_t w4, w5, w12, w13;
  uint32x4x2_t w6, w7, w14, w15;

  w0 = vzipq_u8(x[0].val[0], x[1].val[0]);
  w1 = vzipq_u8(x[2].val[0], x[3].val[0]);
  w2 = vzipq_u8(x[4].val[0], x[5].val[0]);
  w3 = vzipq_u8(x[6].val[0], x[7].val[0]);

  w8 = vzipq_u8(x[8].val[0], x[9].val[0]);
  w9 = vzipq_u8(x[10].val[0], x[11].val[0]);
  w10 = vzipq_u8(x[12].val[0], x[13].val[0]);
  w11 = vzipq_u8(x[14].val[0], x[15].val[0]);

  w4 = vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                 vreinterpretq_u16_u8(w1.val[0]));
  w5 = vzipq_u16(vreinterpretq_u16_u8(w2.val[0]),
                 vreinterpretq_u16_u8(w3.val[0]));
  w12 = vzipq_u16(vreinterpretq_u16_u8(w8.val[0]),
                  vreinterpretq_u16_u8(w9.val[0]));
  w13 = vzipq_u16(vreinterpretq_u16_u8(w10.val[0]),
                  vreinterpretq_u16_u8(w11.val[0]));

  w6 = vzipq_u32(vreinterpretq_u32_u16(w4.val[0]),
                 vreinterpretq_u32_u16(w5.val[0]));
  w7 = vzipq_u32(vreinterpretq_u32_u16(w4.val[1]),
                 vreinterpretq_u32_u16(w5.val[1]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w12.val[0]),
                  vreinterpretq_u32_u16(w13.val[0]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w12.val[1]),
                  vreinterpretq_u32_u16(w13.val[1]));

  // Store first 4-line result

#if AOM_ARCH_AARCH64
  d[0].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[0].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[1].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[1].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[2].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[0]),
                           vreinterpretq_u64_u32(w15.val[0]));
  d[2].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[0]),
                           vreinterpretq_u64_u32(w15.val[0]));
  d[3].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[1]),
                           vreinterpretq_u64_u32(w15.val[1]));
  d[3].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[1]),
                           vreinterpretq_u64_u32(w15.val[1]));
#else
  d[0].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[0]), vget_low_u32(w14.val[0])));
  d[0].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[0]), vget_high_u32(w14.val[0])));
  d[1].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[1]), vget_low_u32(w14.val[1])));
  d[1].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[1]), vget_high_u32(w14.val[1])));
  d[2].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[0]), vget_low_u32(w15.val[0])));
  d[2].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[0]), vget_high_u32(w15.val[0])));
  d[3].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[1]), vget_low_u32(w15.val[1])));
  d[3].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[1]), vget_high_u32(w15.val[1])));
#endif

  w4 = vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                 vreinterpretq_u16_u8(w1.val[1]));
  w5 = vzipq_u16(vreinterpretq_u16_u8(w2.val[1]),
                 vreinterpretq_u16_u8(w3.val[1]));
  w12 = vzipq_u16(vreinterpretq_u16_u8(w8.val[1]),
                  vreinterpretq_u16_u8(w9.val[1]));
  w13 = vzipq_u16(vreinterpretq_u16_u8(w10.val[1]),
                  vreinterpretq_u16_u8(w11.val[1]));

  w6 = vzipq_u32(vreinterpretq_u32_u16(w4.val[0]),
                 vreinterpretq_u32_u16(w5.val[0]));
  w7 = vzipq_u32(vreinterpretq_u32_u16(w4.val[1]),
                 vreinterpretq_u32_u16(w5.val[1]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w12.val[0]),
                  vreinterpretq_u32_u16(w13.val[0]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w12.val[1]),
                  vreinterpretq_u32_u16(w13.val[1]));

  // Store second 4-line result

#if AOM_ARCH_AARCH64
  d[4].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[4].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[5].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[5].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[6].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[0]),
                           vreinterpretq_u64_u32(w15.val[0]));
  d[6].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[0]),
                           vreinterpretq_u64_u32(w15.val[0]));
  d[7].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[1]),
                           vreinterpretq_u64_u32(w15.val[1]));
  d[7].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[1]),
                           vreinterpretq_u64_u32(w15.val[1]));
#else
  d[4].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[0]), vget_low_u32(w14.val[0])));
  d[4].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[0]), vget_high_u32(w14.val[0])));
  d[5].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[1]), vget_low_u32(w14.val[1])));
  d[5].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[1]), vget_high_u32(w14.val[1])));
  d[6].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[0]), vget_low_u32(w15.val[0])));
  d[6].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[0]), vget_high_u32(w15.val[0])));
  d[7].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[1]), vget_low_u32(w15.val[1])));
  d[7].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[1]), vget_high_u32(w15.val[1])));
#endif

  // upper half
  w0 = vzipq_u8(x[0].val[1], x[1].val[1]);
  w1 = vzipq_u8(x[2].val[1], x[3].val[1]);
  w2 = vzipq_u8(x[4].val[1], x[5].val[1]);
  w3 = vzipq_u8(x[6].val[1], x[7].val[1]);

  w8 = vzipq_u8(x[8].val[1], x[9].val[1]);
  w9 = vzipq_u8(x[10].val[1], x[11].val[1]);
  w10 = vzipq_u8(x[12].val[1], x[13].val[1]);
  w11 = vzipq_u8(x[14].val[1], x[15].val[1]);

  w4 = vzipq_u16(vreinterpretq_u16_u8(w0.val[0]),
                 vreinterpretq_u16_u8(w1.val[0]));
  w5 = vzipq_u16(vreinterpretq_u16_u8(w2.val[0]),
                 vreinterpretq_u16_u8(w3.val[0]));
  w12 = vzipq_u16(vreinterpretq_u16_u8(w8.val[0]),
                  vreinterpretq_u16_u8(w9.val[0]));
  w13 = vzipq_u16(vreinterpretq_u16_u8(w10.val[0]),
                  vreinterpretq_u16_u8(w11.val[0]));

  w6 = vzipq_u32(vreinterpretq_u32_u16(w4.val[0]),
                 vreinterpretq_u32_u16(w5.val[0]));
  w7 = vzipq_u32(vreinterpretq_u32_u16(w4.val[1]),
                 vreinterpretq_u32_u16(w5.val[1]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w12.val[0]),
                  vreinterpretq_u32_u16(w13.val[0]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w12.val[1]),
                  vreinterpretq_u32_u16(w13.val[1]));

  // Store first 4-line result

#if AOM_ARCH_AARCH64
  d[8].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[8].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[0]),
                           vreinterpretq_u64_u32(w14.val[0]));
  d[9].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[9].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[1]),
                           vreinterpretq_u64_u32(w14.val[1]));
  d[10].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[0]),
                            vreinterpretq_u64_u32(w15.val[0]));
  d[10].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[0]),
                            vreinterpretq_u64_u32(w15.val[0]));
  d[11].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[1]),
                            vreinterpretq_u64_u32(w15.val[1]));
  d[11].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[1]),
                            vreinterpretq_u64_u32(w15.val[1]));
#else
  d[8].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[0]), vget_low_u32(w14.val[0])));
  d[8].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[0]), vget_high_u32(w14.val[0])));
  d[9].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[1]), vget_low_u32(w14.val[1])));
  d[9].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[1]), vget_high_u32(w14.val[1])));
  d[10].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[0]), vget_low_u32(w15.val[0])));
  d[10].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[0]), vget_high_u32(w15.val[0])));
  d[11].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[1]), vget_low_u32(w15.val[1])));
  d[11].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[1]), vget_high_u32(w15.val[1])));
#endif

  w4 = vzipq_u16(vreinterpretq_u16_u8(w0.val[1]),
                 vreinterpretq_u16_u8(w1.val[1]));
  w5 = vzipq_u16(vreinterpretq_u16_u8(w2.val[1]),
                 vreinterpretq_u16_u8(w3.val[1]));
  w12 = vzipq_u16(vreinterpretq_u16_u8(w8.val[1]),
                  vreinterpretq_u16_u8(w9.val[1]));
  w13 = vzipq_u16(vreinterpretq_u16_u8(w10.val[1]),
                  vreinterpretq_u16_u8(w11.val[1]));

  w6 = vzipq_u32(vreinterpretq_u32_u16(w4.val[0]),
                 vreinterpretq_u32_u16(w5.val[0]));
  w7 = vzipq_u32(vreinterpretq_u32_u16(w4.val[1]),
                 vreinterpretq_u32_u16(w5.val[1]));
  w14 = vzipq_u32(vreinterpretq_u32_u16(w12.val[0]),
                  vreinterpretq_u32_u16(w13.val[0]));
  w15 = vzipq_u32(vreinterpretq_u32_u16(w12.val[1]),
                  vreinterpretq_u32_u16(w13.val[1]));

  // Store second 4-line result

#if AOM_ARCH_AARCH64
  d[12].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[0]),
                            vreinterpretq_u64_u32(w14.val[0]));
  d[12].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[0]),
                            vreinterpretq_u64_u32(w14.val[0]));
  d[13].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w6.val[1]),
                            vreinterpretq_u64_u32(w14.val[1]));
  d[13].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w6.val[1]),
                            vreinterpretq_u64_u32(w14.val[1]));
  d[14].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[0]),
                            vreinterpretq_u64_u32(w15.val[0]));
  d[14].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[0]),
                            vreinterpretq_u64_u32(w15.val[0]));
  d[15].val[0] = vzip1q_u64(vreinterpretq_u64_u32(w7.val[1]),
                            vreinterpretq_u64_u32(w15.val[1]));
  d[15].val[1] = vzip2q_u64(vreinterpretq_u64_u32(w7.val[1]),
                            vreinterpretq_u64_u32(w15.val[1]));
#else
  d[12].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[0]), vget_low_u32(w14.val[0])));
  d[12].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[0]), vget_high_u32(w14.val[0])));
  d[13].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w6.val[1]), vget_low_u32(w14.val[1])));
  d[13].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w6.val[1]), vget_high_u32(w14.val[1])));
  d[14].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[0]), vget_low_u32(w15.val[0])));
  d[14].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[0]), vget_high_u32(w15.val[0])));
  d[15].val[0] = vreinterpretq_u64_u32(
      vcombine_u32(vget_low_u32(w7.val[1]), vget_low_u32(w15.val[1])));
  d[15].val[1] = vreinterpretq_u64_u32(
      vcombine_u32(vget_high_u32(w7.val[1]), vget_high_u32(w15.val[1])));
#endif
}

static void transpose_TX_16X16(const uint8_t *src, ptrdiff_t pitchSrc,
                               uint8_t *dst, ptrdiff_t pitchDst) {
  uint8x16_t r[16];
  uint64x2_t d[16];
  for (int i = 0; i < 16; i++) {
    r[i] = vld1q_u8(src + i * pitchSrc);
  }
  transpose16x16_neon(r, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + i * pitchDst, vreinterpretq_u8_u64(d[i]));
  }
}

static void transpose(const uint8_t *src, ptrdiff_t pitchSrc, uint8_t *dst,
                      ptrdiff_t pitchDst, int width, int height) {
  for (int j = 0; j < height; j += 16) {
    for (int i = 0; i < width; i += 16) {
      transpose_TX_16X16(src + i * pitchSrc + j, pitchSrc,
                         dst + j * pitchDst + i, pitchDst);
    }
  }
}

static void dr_prediction_z3_4x4_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[4];
  uint16x4x2_t dest;

  dr_prediction_z1_HxW_internal_neon_64(4, 4, dstvec, left, upsample_left, dy);
  transpose4x8_8x4_low_neon(dstvec, &dest);
  vst1_lane_u32((uint32_t *)(dst + stride * 0),
                vreinterpret_u32_u16(dest.val[0]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 1),
                vreinterpret_u32_u16(dest.val[0]), 1);
  vst1_lane_u32((uint32_t *)(dst + stride * 2),
                vreinterpret_u32_u16(dest.val[1]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 3),
                vreinterpret_u32_u16(dest.val[1]), 1);
}

static void dr_prediction_z3_8x8_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[8];
  uint32x2x2_t d[4];

  dr_prediction_z1_HxW_internal_neon_64(8, 8, dstvec, left, upsample_left, dy);
  transpose8x8_neon(dstvec, d);
  vst1_u32((uint32_t *)(dst + 0 * stride), d[0].val[0]);
  vst1_u32((uint32_t *)(dst + 1 * stride), d[0].val[1]);
  vst1_u32((uint32_t *)(dst + 2 * stride), d[1].val[0]);
  vst1_u32((uint32_t *)(dst + 3 * stride), d[1].val[1]);
  vst1_u32((uint32_t *)(dst + 4 * stride), d[2].val[0]);
  vst1_u32((uint32_t *)(dst + 5 * stride), d[2].val[1]);
  vst1_u32((uint32_t *)(dst + 6 * stride), d[3].val[0]);
  vst1_u32((uint32_t *)(dst + 7 * stride), d[3].val[1]);
}

static void dr_prediction_z3_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[4];
  uint16x4x2_t d[2];

  dr_prediction_z1_HxW_internal_neon_64(8, 4, dstvec, left, upsample_left, dy);
  transpose4x8_8x4_neon(dstvec, d);
  vst1_lane_u32((uint32_t *)(dst + stride * 0),
                vreinterpret_u32_u16(d[0].val[0]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 1),
                vreinterpret_u32_u16(d[0].val[0]), 1);
  vst1_lane_u32((uint32_t *)(dst + stride * 2),
                vreinterpret_u32_u16(d[0].val[1]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 3),
                vreinterpret_u32_u16(d[0].val[1]), 1);
  vst1_lane_u32((uint32_t *)(dst + stride * 4),
                vreinterpret_u32_u16(d[1].val[0]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 5),
                vreinterpret_u32_u16(d[1].val[0]), 1);
  vst1_lane_u32((uint32_t *)(dst + stride * 6),
                vreinterpret_u32_u16(d[1].val[1]), 0);
  vst1_lane_u32((uint32_t *)(dst + stride * 7),
                vreinterpret_u32_u16(d[1].val[1]), 1);
}

static void dr_prediction_z3_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *left, int upsample_left,
                                      int dy) {
  uint8x8_t dstvec[8];
  uint32x2x2_t d[2];

  dr_prediction_z1_HxW_internal_neon_64(4, 8, dstvec, left, upsample_left, dy);
  transpose8x8_low_neon(dstvec, d);
  vst1_u32((uint32_t *)(dst + 0 * stride), d[0].val[0]);
  vst1_u32((uint32_t *)(dst + 1 * stride), d[0].val[1]);
  vst1_u32((uint32_t *)(dst + 2 * stride), d[1].val[0]);
  vst1_u32((uint32_t *)(dst + 3 * stride), d[1].val[1]);
}

static void dr_prediction_z3_8x16_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x16_t dstvec[8];
  uint64x2_t d[8];

  dr_prediction_z1_HxW_internal_neon(16, 8, dstvec, left, upsample_left, dy);
  transpose8x16_16x8_neon(dstvec, d);
  for (int i = 0; i < 8; i++) {
    vst1_u8(dst + i * stride, vreinterpret_u8_u64(vget_low_u64(d[i])));
    vst1_u8(dst + (i + 8) * stride, vreinterpret_u8_u64(vget_high_u64(d[i])));
  }
}

static void dr_prediction_z3_16x8_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[16];
  uint64x2_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(8, 16, dstvec, left, upsample_left, dy);
  transpose16x8_8x16_neon(dstvec, d);
  for (int i = 0; i < 8; i++) {
    vst1q_u8(dst + i * stride, vreinterpretq_u8_u64(d[i]));
  }
}

static void dr_prediction_z3_4x16_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x16_t dstvec[4];
  uint16x8x2_t d[2];

  dr_prediction_z1_HxW_internal_neon(16, 4, dstvec, left, upsample_left, dy);
  transpose4x16_neon(dstvec, d);
  vst1q_lane_u32((uint32_t *)(dst + stride * 0),
                 vreinterpretq_u32_u16(d[0].val[0]), 0);
  vst1q_lane_u32((uint32_t *)(dst + stride * 1),
                 vreinterpretq_u32_u16(d[0].val[0]), 1);
  vst1q_lane_u32((uint32_t *)(dst + stride * 2),
                 vreinterpretq_u32_u16(d[0].val[0]), 2);
  vst1q_lane_u32((uint32_t *)(dst + stride * 3),
                 vreinterpretq_u32_u16(d[0].val[0]), 3);

  vst1q_lane_u32((uint32_t *)(dst + stride * 4),
                 vreinterpretq_u32_u16(d[0].val[1]), 0);
  vst1q_lane_u32((uint32_t *)(dst + stride * 5),
                 vreinterpretq_u32_u16(d[0].val[1]), 1);
  vst1q_lane_u32((uint32_t *)(dst + stride * 6),
                 vreinterpretq_u32_u16(d[0].val[1]), 2);
  vst1q_lane_u32((uint32_t *)(dst + stride * 7),
                 vreinterpretq_u32_u16(d[0].val[1]), 3);

  vst1q_lane_u32((uint32_t *)(dst + stride * 8),
                 vreinterpretq_u32_u16(d[1].val[0]), 0);
  vst1q_lane_u32((uint32_t *)(dst + stride * 9),
                 vreinterpretq_u32_u16(d[1].val[0]), 1);
  vst1q_lane_u32((uint32_t *)(dst + stride * 10),
                 vreinterpretq_u32_u16(d[1].val[0]), 2);
  vst1q_lane_u32((uint32_t *)(dst + stride * 11),
                 vreinterpretq_u32_u16(d[1].val[0]), 3);

  vst1q_lane_u32((uint32_t *)(dst + stride * 12),
                 vreinterpretq_u32_u16(d[1].val[1]), 0);
  vst1q_lane_u32((uint32_t *)(dst + stride * 13),
                 vreinterpretq_u32_u16(d[1].val[1]), 1);
  vst1q_lane_u32((uint32_t *)(dst + stride * 14),
                 vreinterpretq_u32_u16(d[1].val[1]), 2);
  vst1q_lane_u32((uint32_t *)(dst + stride * 15),
                 vreinterpretq_u32_u16(d[1].val[1]), 3);
}

static void dr_prediction_z3_16x4_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[16];
  uint64x2_t d[8];

  dr_prediction_z1_HxW_internal_neon_64(4, 16, dstvec, left, upsample_left, dy);
  transpose16x8_8x16_neon(dstvec, d);
  for (int i = 0; i < 4; i++) {
    vst1q_u8(dst + i * stride, vreinterpretq_u8_u64(d[i]));
  }
}

static void dr_prediction_z3_8x32_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x16x2_t dstvec[16];
  uint64x2x2_t d[16];
  uint8x16_t v_zero = vdupq_n_u8(0);

  dr_prediction_z1_32xN_internal_neon(8, dstvec, left, upsample_left, dy);
  for (int i = 8; i < 16; i++) {
    dstvec[i].val[0] = v_zero;
    dstvec[i].val[1] = v_zero;
  }
  transpose16x32_neon(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1_u8(dst + 2 * i * stride,
            vreinterpret_u8_u64(vget_low_u64(d[i].val[0])));
    vst1_u8(dst + (2 * i + 1) * stride,
            vreinterpret_u8_u64(vget_low_u64(d[i].val[1])));
  }
}

static void dr_prediction_z3_32x8_neon(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *left, int upsample_left,
                                       int dy) {
  uint8x8_t dstvec[32];
  uint64x2_t d[16];

  dr_prediction_z1_HxW_internal_neon_64(8, 32, dstvec, left, upsample_left, dy);
  transpose16x8_8x16_neon(dstvec, d);
  transpose16x8_8x16_neon(dstvec + 16, d + 8);
  for (int i = 0; i < 8; i++) {
    vst1q_u8(dst + i * stride, vreinterpretq_u8_u64(d[i]));
    vst1q_u8(dst + i * stride + 16, vreinterpretq_u8_u64(d[i + 8]));
  }
}

static void dr_prediction_z3_16x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[16];
  uint64x2_t d[16];

  dr_prediction_z1_HxW_internal_neon(16, 16, dstvec, left, upsample_left, dy);
  transpose16x16_neon(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + i * stride, vreinterpretq_u8_u64(d[i]));
  }
}

static void dr_prediction_z3_32x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16x2_t dstvec[32];
  uint64x2x2_t d[32];

  dr_prediction_z1_32xN_internal_neon(32, dstvec, left, upsample_left, dy);
  transpose16x32_neon(dstvec, d);
  transpose16x32_neon(dstvec + 16, d + 16);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + 2 * i * stride, vreinterpretq_u8_u64(d[i].val[0]));
    vst1q_u8(dst + 2 * i * stride + 16, vreinterpretq_u8_u64(d[i + 16].val[0]));
    vst1q_u8(dst + (2 * i + 1) * stride, vreinterpretq_u8_u64(d[i].val[1]));
    vst1q_u8(dst + (2 * i + 1) * stride + 16,
             vreinterpretq_u8_u64(d[i + 16].val[1]));
  }
}

static void dr_prediction_z3_64x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  DECLARE_ALIGNED(16, uint8_t, dstT[64 * 64]);

  dr_prediction_z1_64xN_neon(64, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 64, 64);
}

static void dr_prediction_z3_16x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16x2_t dstvec[16];
  uint64x2x2_t d[16];

  dr_prediction_z1_32xN_internal_neon(16, dstvec, left, upsample_left, dy);
  transpose16x32_neon(dstvec, d);
  for (int i = 0; i < 16; i++) {
    vst1q_u8(dst + 2 * i * stride, vreinterpretq_u8_u64(d[i].val[0]));
    vst1q_u8(dst + (2 * i + 1) * stride, vreinterpretq_u8_u64(d[i].val[1]));
  }
}

static void dr_prediction_z3_32x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[32];
  uint64x2_t d[16];

  dr_prediction_z1_HxW_internal_neon(16, 32, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 32; i += 16) {
    transpose16x16_neon((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      vst1q_u8(dst + j * stride + i, vreinterpretq_u8_u64(d[j]));
    }
  }
}

static void dr_prediction_z3_32x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[64 * 32];

  dr_prediction_z1_64xN_neon(32, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 32, 64);
}

static void dr_prediction_z3_64x32_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[32 * 64];

  dr_prediction_z1_32xN_neon(64, dstT, 32, left, upsample_left, dy);
  transpose(dstT, 32, dst, stride, 64, 32);
}

static void dr_prediction_z3_16x64_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8_t dstT[64 * 16];

  dr_prediction_z1_64xN_neon(16, dstT, 64, left, upsample_left, dy);
  transpose(dstT, 64, dst, stride, 16, 64);
}

static void dr_prediction_z3_64x16_neon(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *left, int upsample_left,
                                        int dy) {
  uint8x16_t dstvec[64];
  uint64x2_t d[16];

  dr_prediction_z1_HxW_internal_neon(16, 64, dstvec, left, upsample_left, dy);
  for (int i = 0; i < 64; i += 16) {
    transpose16x16_neon((dstvec + i), d);
    for (int j = 0; j < 16; j++) {
      vst1q_u8(dst + j * stride + i, vreinterpretq_u8_u64(d[j]));
    }
  }
}

void av1_dr_prediction_z3_neon(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left,
                               int upsample_left, int dx, int dy) {
  (void)above;
  (void)dx;
  assert(dx == 1);
  assert(dy > 0);

  if (bw == bh) {
    switch (bw) {
      case 4:
        dr_prediction_z3_4x4_neon(dst, stride, left, upsample_left, dy);
        break;
      case 8:
        dr_prediction_z3_8x8_neon(dst, stride, left, upsample_left, dy);
        break;
      case 16:
        dr_prediction_z3_16x16_neon(dst, stride, left, upsample_left, dy);
        break;
      case 32:
        dr_prediction_z3_32x32_neon(dst, stride, left, upsample_left, dy);
        break;
      case 64:
        dr_prediction_z3_64x64_neon(dst, stride, left, upsample_left, dy);
        break;
    }
  } else {
    if (bw < bh) {
      if (bw + bw == bh) {
        switch (bw) {
          case 4:
            dr_prediction_z3_4x8_neon(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_8x16_neon(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_16x32_neon(dst, stride, left, upsample_left, dy);
            break;
          case 32:
            dr_prediction_z3_32x64_neon(dst, stride, left, upsample_left, dy);
            break;
        }
      } else {
        switch (bw) {
          case 4:
            dr_prediction_z3_4x16_neon(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_8x32_neon(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_16x64_neon(dst, stride, left, upsample_left, dy);
            break;
        }
      }
    } else {
      if (bh + bh == bw) {
        switch (bh) {
          case 4:
            dr_prediction_z3_8x4_neon(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_16x8_neon(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_32x16_neon(dst, stride, left, upsample_left, dy);
            break;
          case 32:
            dr_prediction_z3_64x32_neon(dst, stride, left, upsample_left, dy);
            break;
        }
      } else {
        switch (bh) {
          case 4:
            dr_prediction_z3_16x4_neon(dst, stride, left, upsample_left, dy);
            break;
          case 8:
            dr_prediction_z3_32x8_neon(dst, stride, left, upsample_left, dy);
            break;
          case 16:
            dr_prediction_z3_64x16_neon(dst, stride, left, upsample_left, dy);
            break;
        }
      }
    }
  }
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

  uint8x8_t UNINITIALIZED_IS_SAFE(top_v);
  load_u8_4x1(top_row, &top_v, 0);
  const uint8x8_t top_right_v = vdup_n_u8(top_right);
  const uint8x8_t bottom_left_v = vdup_n_u8(bottom_left);
  uint8x8_t UNINITIALIZED_IS_SAFE(weights_x_v);
  load_u8_4x1(smooth_weights, &weights_x_v, 0);
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
    uint8x8_t UNINITIALIZED_IS_SAFE(top_v);                           \
    if ((W) == 4) {                                                   \
      load_u8_4x1(top_row, &top_v, 0);                                \
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
  uint8x8_t UNINITIALIZED_IS_SAFE(top);
  if (width == 4) {
    load_u8_4x1(top_row, &top, 0);
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
      store_unaligned_u8_4x1(dest, result, 0);
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
