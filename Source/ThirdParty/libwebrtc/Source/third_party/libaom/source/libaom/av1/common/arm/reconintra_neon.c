/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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
#include "aom_dsp/arm/sum_neon.h"

DECLARE_ALIGNED(16, const int8_t,
                av1_filter_intra_taps_neon[FILTER_INTRA_MODES][8][8]) = {
  {
      { -6, 0, 0, 0, -5, 10, 0, 0 },
      { 10, 0, 12, 0, 2, 0, 9, 0 },
      { -3, 1, 0, 0, -3, 1, 10, 0 },
      { 1, 10, 7, 0, 1, 2, 5, 0 },
      { -4, 0, 0, 12, -3, 6, 0, 9 },
      { 6, 0, 2, 0, 2, 0, 2, 0 },
      { -3, 2, 0, 7, -3, 2, 6, 5 },
      { 2, 6, 2, 0, 1, 2, 3, 0 },
  },
  {
      { -10, 0, 0, 0, -6, 16, 0, 0 },
      { 16, 0, 10, 0, 0, 0, 6, 0 },
      { -4, 0, 0, 0, -2, 0, 16, 0 },
      { 0, 16, 4, 0, 0, 0, 2, 0 },
      { -10, 0, 0, 10, -6, 16, 0, 6 },
      { 16, 0, 0, 0, 0, 0, 0, 0 },
      { -4, 0, 0, 4, -2, 0, 16, 2 },
      { 0, 16, 0, 0, 0, 0, 0, 0 },
  },
  {
      { -8, 0, 0, 0, -8, 8, 0, 0 },
      { 8, 0, 16, 0, 0, 0, 16, 0 },
      { -8, 0, 0, 0, -8, 0, 8, 0 },
      { 0, 8, 16, 0, 0, 0, 16, 0 },
      { -4, 0, 0, 16, -4, 4, 0, 16 },
      { 4, 0, 0, 0, 0, 0, 0, 0 },
      { -4, 0, 0, 16, -4, 0, 4, 16 },
      { 0, 4, 0, 0, 0, 0, 0, 0 },
  },
  {
      { -2, 0, 0, 0, -1, 8, 0, 0 },
      { 8, 0, 10, 0, 3, 0, 6, 0 },
      { -1, 3, 0, 0, 0, 2, 8, 0 },
      { 2, 8, 4, 0, 1, 3, 2, 0 },
      { -1, 0, 0, 10, -1, 4, 0, 6 },
      { 4, 0, 3, 0, 3, 0, 4, 0 },
      { -1, 3, 0, 4, -1, 2, 4, 3 },
      { 2, 4, 4, 0, 2, 3, 3, 0 },
  },
  {
      { -12, 0, 0, 0, -10, 14, 0, 0 },
      { 14, 0, 14, 0, 0, 0, 12, 0 },
      { -9, 0, 0, 0, -8, 0, 14, 0 },
      { 0, 14, 11, 0, 0, 0, 10, 0 },
      { -10, 0, 0, 14, -9, 12, 0, 12 },
      { 12, 0, 0, 0, 1, 0, 0, 0 },
      { -8, 0, 0, 11, -7, 0, 12, 9 },
      { 0, 12, 1, 0, 0, 1, 1, 0 },
  },
};

#define FILTER_INTRA_SCALE_BITS 4
#define SHIFT_INTRA_SCALE_BITS 15 - FILTER_INTRA_SCALE_BITS

#define MASK_LOW \
  0x604020006040200  // (0 | (2 << 8) | (4 << 16) | (6 << 24)) x 2
#define MASK_HIGH \
  0x705030107050301  // (1 | (3 << 8) | (5 << 16) | (7 << 24)) x 2

void av1_filter_intra_predictor_neon(uint8_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint8_t *above,
                                     const uint8_t *left, int mode) {
  int r, c;
  uint8_t buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  const int8x16_t f1f0 = vld1q_s8(av1_filter_intra_taps_neon[mode][0]);
  const int8x16_t f3f2 = vld1q_s8(av1_filter_intra_taps_neon[mode][2]);
  const int8x16_t f5f4 = vld1q_s8(av1_filter_intra_taps_neon[mode][4]);
  const int8x16_t f7f6 = vld1q_s8(av1_filter_intra_taps_neon[mode][6]);
  const int16x8_t f1f0_lo = vmovl_s8(vget_low_s8(f1f0));
  const int16x8_t f1f0_hi = vmovl_s8(vget_high_s8(f1f0));
  const int16x8_t f3f2_lo = vmovl_s8(vget_low_s8(f3f2));
  const int16x8_t f3f2_hi = vmovl_s8(vget_high_s8(f3f2));
  const int16x8_t f5f4_lo = vmovl_s8(vget_low_s8(f5f4));
  const int16x8_t f5f4_hi = vmovl_s8(vget_high_s8(f5f4));
  const int16x8_t f7f6_lo = vmovl_s8(vget_low_s8(f7f6));
  const int16x8_t f7f6_hi = vmovl_s8(vget_high_s8(f7f6));
  const uint8x8_t vmask_low = vcreate_u8(MASK_LOW);
  const uint8x8_t vmask_high = vcreate_u8(MASK_HIGH);

  assert(bw <= 32 && bh <= 32);

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = left[r];
  memcpy(buffer[0], &above[-1], (bw + 1) * sizeof(uint8_t));

  for (r = 1; r < bh + 1; r += 2) {
    for (c = 1; c < bw + 1; c += 4) {
      DECLARE_ALIGNED(16, uint8_t, p[8]);
      memcpy(p, &buffer[r - 1][c - 1], 5 * sizeof(uint8_t));
      p[5] = buffer[r][c - 1];
      p[6] = buffer[r + 1][c - 1];
      p[7] = 0;

      const uint8x8_t p_b = vld1_u8(p);

      const uint16x8_t p_b_lo = vmovl_u8(vtbl1_u8(p_b, vmask_low));
      const uint16x8_t p_b_hi = vmovl_u8(vtbl1_u8(p_b, vmask_high));

      int16x8_t out_01 = vmulq_s16(vreinterpretq_s16_u16(p_b_lo), f1f0_lo);
      out_01 = vmlaq_s16(out_01, vreinterpretq_s16_u16(p_b_hi), f1f0_hi);
      int16x8_t out_23 = vmulq_s16(vreinterpretq_s16_u16(p_b_lo), f3f2_lo);
      out_23 = vmlaq_s16(out_23, vreinterpretq_s16_u16(p_b_hi), f3f2_hi);
      int16x8_t out_45 = vmulq_s16(vreinterpretq_s16_u16(p_b_lo), f5f4_lo);
      out_45 = vmlaq_s16(out_45, vreinterpretq_s16_u16(p_b_hi), f5f4_hi);
      int16x8_t out_67 = vmulq_s16(vreinterpretq_s16_u16(p_b_lo), f7f6_lo);
      out_67 = vmlaq_s16(out_67, vreinterpretq_s16_u16(p_b_hi), f7f6_hi);
#if defined(__aarch64__)
      const int16x8_t out_0123 = vpaddq_s16(out_01, out_23);
      const int16x8_t out_4567 = vpaddq_s16(out_45, out_67);
      const int16x8_t out_01234567 = vpaddq_s16(out_0123, out_4567);
#else
      const int16x8_t out_0123 = vcombine_s16(vqmovn_s32(vpaddlq_s16(out_01)),
                                              vqmovn_s32(vpaddlq_s16(out_23)));
      const int16x8_t out_4567 = vcombine_s16(vqmovn_s32(vpaddlq_s16(out_45)),
                                              vqmovn_s32(vpaddlq_s16(out_67)));
      const int16x8_t out_01234567 = vcombine_s16(
          vqmovn_s32(vpaddlq_s16(out_0123)), vqmovn_s32(vpaddlq_s16(out_4567)));
#endif  // (__aarch64__)
      const uint32x2_t out_r =
          vreinterpret_u32_u8(vqmovun_s16(vrshrq_n_s16(out_01234567, 4)));
      // Storing
      vst1_lane_u32((uint32_t *)&buffer[r][c], out_r, 0);
      vst1_lane_u32((uint32_t *)&buffer[r + 1][c], out_r, 1);
    }
  }

  for (r = 0; r < bh; ++r) {
    memcpy(dst, &buffer[r + 1][1], bw * sizeof(uint8_t));
    dst += stride;
  }
}
