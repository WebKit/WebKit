/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
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
#include "config/av1_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"

#define FILTER_INTRA_SCALE_BITS 4

// The input arrays are reordered compared to the C implementation: the first
// four vectors contain the lower elements of the original filter, the next four
// vectors contain the upper elements. This layout allows all 8 multiplications
// to accumulate into a single element using USDOT instructions, instead of
// having two partial sums in adjacent vector elements and needing to combine
// them with an additional pairwise add.
DECLARE_ALIGNED(16, static const int8_t,
                av1_filter_intra_taps_neon_i8mm[FILTER_INTRA_MODES][8][8]) = {
  {
      { -6, 10, 0, 0, -5, 2, 10, 0 },
      { -3, 1, 1, 10, -3, 1, 1, 2 },
      { -4, 6, 0, 0, -3, 2, 6, 0 },
      { -3, 2, 2, 6, -3, 1, 2, 2 },
      { 0, 12, 0, 0, 0, 9, 0, 0 },
      { 0, 7, 0, 0, 10, 5, 0, 0 },
      { 0, 2, 12, 0, 0, 2, 9, 0 },
      { 0, 2, 7, 0, 6, 3, 5, 0 },
  },
  {
      { -10, 16, 0, 0, -6, 0, 16, 0 },
      { -4, 0, 0, 16, -2, 0, 0, 0 },
      { -10, 16, 0, 0, -6, 0, 16, 0 },
      { -4, 0, 0, 16, -2, 0, 0, 0 },
      { 0, 10, 0, 0, 0, 6, 0, 0 },
      { 0, 4, 0, 0, 16, 2, 0, 0 },
      { 0, 0, 10, 0, 0, 0, 6, 0 },
      { 0, 0, 4, 0, 16, 0, 2, 0 },
  },
  {
      { -8, 8, 0, 0, -8, 0, 8, 0 },
      { -8, 0, 0, 8, -8, 0, 0, 0 },
      { -4, 4, 0, 0, -4, 0, 4, 0 },
      { -4, 0, 0, 4, -4, 0, 0, 0 },
      { 0, 16, 0, 0, 0, 16, 0, 0 },
      { 0, 16, 0, 0, 8, 16, 0, 0 },
      { 0, 0, 16, 0, 0, 0, 16, 0 },
      { 0, 0, 16, 0, 4, 0, 16, 0 },
  },
  {
      { -2, 8, 0, 0, -1, 3, 8, 0 },
      { -1, 2, 3, 8, 0, 1, 2, 3 },
      { -1, 4, 0, 0, -1, 3, 4, 0 },
      { -1, 2, 3, 4, -1, 2, 2, 3 },
      { 0, 10, 0, 0, 0, 6, 0, 0 },
      { 0, 4, 0, 0, 8, 2, 0, 0 },
      { 0, 3, 10, 0, 0, 4, 6, 0 },
      { 0, 4, 4, 0, 4, 3, 3, 0 },
  },
  {
      { -12, 14, 0, 0, -10, 0, 14, 0 },
      { -9, 0, 0, 14, -8, 0, 0, 0 },
      { -10, 12, 0, 0, -9, 1, 12, 0 },
      { -8, 0, 0, 12, -7, 0, 0, 1 },
      { 0, 14, 0, 0, 0, 12, 0, 0 },
      { 0, 11, 0, 0, 14, 10, 0, 0 },
      { 0, 0, 14, 0, 0, 0, 12, 0 },
      { 0, 1, 11, 0, 12, 1, 9, 0 },
  },
};

static inline uint8x8_t filter_intra_predictor(uint8x16_t p_lo, uint8x16_t p_hi,
                                               const int8x16_t f01,
                                               const int8x16_t f23,
                                               const int8x16_t f45,
                                               const int8x16_t f67) {
  int32x4_t acc_0123 = vusdotq_s32(vdupq_n_s32(0), p_lo, f01);
  acc_0123 = vusdotq_s32(acc_0123, p_hi, f23);

  int32x4_t acc_4567 = vusdotq_s32(vdupq_n_s32(0), p_lo, f45);
  acc_4567 = vusdotq_s32(acc_4567, p_hi, f67);

  const int16x8_t acc = vcombine_s16(vmovn_s32(acc_0123), vmovn_s32(acc_4567));

  return vqrshrun_n_s16(acc, FILTER_INTRA_SCALE_BITS);
}

void av1_filter_intra_predictor_neon_i8mm(uint8_t *dst, ptrdiff_t stride,
                                          TX_SIZE tx_size, const uint8_t *above,
                                          const uint8_t *left, int mode) {
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  if (bw == 4 || (bw == 8 && bh < 16) || (bw == 16 && bh <= 4) || bw == 32) {
    av1_filter_intra_predictor_neon(dst, stride, tx_size, above, left, mode);
    return;
  }

  assert(bw <= 32 && bh <= 32);

  const int8x16_t f01 = vld1q_s8(av1_filter_intra_taps_neon_i8mm[mode][0]);
  const int8x16_t f45 = vld1q_s8(av1_filter_intra_taps_neon_i8mm[mode][2]);
  const int8x16_t f23 = vld1q_s8(av1_filter_intra_taps_neon_i8mm[mode][4]);
  const int8x16_t f67 = vld1q_s8(av1_filter_intra_taps_neon_i8mm[mode][6]);

  // indexes : 0, 19, 23, -1
  uint8x16_t p_hi_idx = vreinterpretq_u8_u32(vdupq_n_u32(0xFF171300));

  uint64_t l01 = ((uint64_t)left[0] << 24) | ((uint64_t)left[1] << 56);
  uint8x16_t l = vreinterpretq_u8_u64(vdupq_n_u64(l01));

  int c = 0;
  do {
    const uint8_t *ptr = above + c - 1;
    uint32_t lo =
        ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
    uint8x16_t p_lo = vreinterpretq_u8_u32(vdupq_n_u32(lo));

    uint8x16x2_t hi;
    hi.val[0] = vdupq_n_u8(ptr[4]);
    hi.val[1] = l;
    uint8x16_t p_hi = vqtbl2q_u8(hi, p_hi_idx);

    const uint8x8_t res =
        filter_intra_predictor(p_lo, p_hi, f01, f23, f45, f67);

    store_u8x4_strided_x2(dst + c, stride, res);

    l = vcombine_u8(res, res);

    c += 4;
  } while (c < bw);

  dst += 2 * stride;
  int r = 2;
  while (r < bh) {
    const uint8_t *ptr = dst - stride;
    uint32_t lo =
        left[r - 1] | (ptr[0] << 8) | (ptr[1] << 16) | ((uint32_t)ptr[2] << 24);
    uint32_t hi = ptr[3] | (left[r] << 8) | (left[r + 1] << 16);
    uint8x16_t p_lo = vreinterpretq_u8_u32(vdupq_n_u32(lo));
    uint8x16_t p_hi = vreinterpretq_u8_u32(vdupq_n_u32(hi));

    uint8x8_t res = filter_intra_predictor(p_lo, p_hi, f01, f23, f45, f67);

    store_u8x4_strided_x2(dst, stride, res);

    l = vcombine_u8(res, res);

    c = 4;
    while (c < bw) {
      ptr = dst - stride + c - 1;
      lo = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
      p_lo = vreinterpretq_u8_u32(vdupq_n_u32(lo));

      uint8x16x2_t hi_v;
      hi_v.val[0] = vdupq_n_u8(ptr[4]);
      hi_v.val[1] = l;
      p_hi = vqtbl2q_u8(hi_v, p_hi_idx);

      res = filter_intra_predictor(p_lo, p_hi, f01, f23, f45, f67);

      store_u8x4_strided_x2(dst + c, stride, res);

      l = vcombine_u8(res, res);
      c += 4;
    }

    r += 2;
    dst += 2 * stride;
  }
}
