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

#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/sum_neon.h"

#define MAX_UPSAMPLE_SZ 16

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
#if AOM_ARCH_AARCH64
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
#endif  // AOM_ARCH_AARCH64
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

void av1_filter_intra_edge_neon(uint8_t *p, int sz, int strength) {
  if (!strength) return;
  assert(sz >= 0 && sz <= 129);

  uint8_t edge[160];  // Max value of sz + enough padding for vector accesses.
  memcpy(edge + 1, p, sz * sizeof(*p));

  // Populate extra space appropriately.
  edge[0] = edge[1];
  edge[sz + 1] = edge[sz];
  edge[sz + 2] = edge[sz];

  // Don't overwrite first pixel.
  uint8_t *dst = p + 1;
  sz--;

  if (strength == 1) {  // Filter: {4, 8, 4}.
    const uint8_t *src = edge + 1;

    while (sz >= 8) {
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);

      // Make use of the identity:
      // (4*a + 8*b + 4*c) >> 4 == (a + (b << 1) + c) >> 2
      uint16x8_t t0 = vaddl_u8(s0, s2);
      uint16x8_t t1 = vaddl_u8(s1, s1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint8x8_t res = vrshrn_n_u16(sum, 2);

      vst1_u8(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }

    if (sz > 0) {  // Handle sz < 8 to avoid modifying out-of-bounds values.
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);

      uint16x8_t t0 = vaddl_u8(s0, s2);
      uint16x8_t t1 = vaddl_u8(s1, s1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint8x8_t res = vrshrn_n_u16(sum, 2);

      // Mask off out-of-bounds indices.
      uint8x8_t current_dst = vld1_u8(dst);
      uint8x8_t mask = vcgt_u8(vdup_n_u8(sz), vcreate_u8(0x0706050403020100));
      res = vbsl_u8(mask, res, current_dst);

      vst1_u8(dst, res);
    }
  } else if (strength == 2) {  // Filter: {5, 6, 5}.
    const uint8_t *src = edge + 1;

    const uint8x8x3_t filter = { { vdup_n_u8(5), vdup_n_u8(6), vdup_n_u8(5) } };

    while (sz >= 8) {
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);

      uint16x8_t accum = vmull_u8(s0, filter.val[0]);
      accum = vmlal_u8(accum, s1, filter.val[1]);
      accum = vmlal_u8(accum, s2, filter.val[2]);
      uint8x8_t res = vrshrn_n_u16(accum, 4);

      vst1_u8(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }

    if (sz > 0) {  // Handle sz < 8 to avoid modifying out-of-bounds values.
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);

      uint16x8_t accum = vmull_u8(s0, filter.val[0]);
      accum = vmlal_u8(accum, s1, filter.val[1]);
      accum = vmlal_u8(accum, s2, filter.val[2]);
      uint8x8_t res = vrshrn_n_u16(accum, 4);

      // Mask off out-of-bounds indices.
      uint8x8_t current_dst = vld1_u8(dst);
      uint8x8_t mask = vcgt_u8(vdup_n_u8(sz), vcreate_u8(0x0706050403020100));
      res = vbsl_u8(mask, res, current_dst);

      vst1_u8(dst, res);
    }
  } else {  // Filter {2, 4, 4, 4, 2}.
    const uint8_t *src = edge;

    while (sz >= 8) {
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);
      uint8x8_t s3 = vld1_u8(src + 3);
      uint8x8_t s4 = vld1_u8(src + 4);

      // Make use of the identity:
      // (2*a + 4*b + 4*c + 4*d + 2*e) >> 4 == (a + ((b + c + d) << 1) + e) >> 3
      uint16x8_t t0 = vaddl_u8(s0, s4);
      uint16x8_t t1 = vaddl_u8(s1, s2);
      t1 = vaddw_u8(t1, s3);
      t1 = vaddq_u16(t1, t1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint8x8_t res = vrshrn_n_u16(sum, 3);

      vst1_u8(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }

    if (sz > 0) {  // Handle sz < 8 to avoid modifying out-of-bounds values.
      uint8x8_t s0 = vld1_u8(src);
      uint8x8_t s1 = vld1_u8(src + 1);
      uint8x8_t s2 = vld1_u8(src + 2);
      uint8x8_t s3 = vld1_u8(src + 3);
      uint8x8_t s4 = vld1_u8(src + 4);

      uint16x8_t t0 = vaddl_u8(s0, s4);
      uint16x8_t t1 = vaddl_u8(s1, s2);
      t1 = vaddw_u8(t1, s3);
      t1 = vaddq_u16(t1, t1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint8x8_t res = vrshrn_n_u16(sum, 3);

      // Mask off out-of-bounds indices.
      uint8x8_t current_dst = vld1_u8(dst);
      uint8x8_t mask = vcgt_u8(vdup_n_u8(sz), vcreate_u8(0x0706050403020100));
      res = vbsl_u8(mask, res, current_dst);

      vst1_u8(dst, res);
    }
  }
}

void av1_upsample_intra_edge_neon(uint8_t *p, int sz) {
  if (!sz) return;

  assert(sz <= MAX_UPSAMPLE_SZ);

  uint8_t edge[MAX_UPSAMPLE_SZ + 3];
  const uint8_t *src = edge;

  // Copy p[-1..(sz-1)] and pad out both ends.
  edge[0] = p[-1];
  edge[1] = p[-1];
  memcpy(edge + 2, p, sz);
  edge[sz + 2] = p[sz - 1];
  p[-2] = p[-1];

  uint8_t *dst = p - 1;

  do {
    uint8x8_t s0 = vld1_u8(src);
    uint8x8_t s1 = vld1_u8(src + 1);
    uint8x8_t s2 = vld1_u8(src + 2);
    uint8x8_t s3 = vld1_u8(src + 3);

    int16x8_t t0 = vreinterpretq_s16_u16(vaddl_u8(s0, s3));
    int16x8_t t1 = vreinterpretq_s16_u16(vaddl_u8(s1, s2));
    t1 = vmulq_n_s16(t1, 9);
    t1 = vsubq_s16(t1, t0);

    uint8x8x2_t res = { { vqrshrun_n_s16(t1, 4), s2 } };

    vst2_u8(dst, res);

    src += 8;
    dst += 16;
    sz -= 8;
  } while (sz > 0);
}
