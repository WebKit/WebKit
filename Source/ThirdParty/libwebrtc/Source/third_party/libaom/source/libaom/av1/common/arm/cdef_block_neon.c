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
#include "config/av1_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "av1/common/cdef_block.h"

void cdef_copy_rect8_8bit_to_16bit_neon(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int width, int height) {
  do {
    const uint8_t *src_ptr = src;
    uint16_t *dst_ptr = dst;

    int w = 0;
    while (width - w >= 16) {
      uint8x16_t row = vld1q_u8(src_ptr + w);
      uint8x16x2_t row_u16 = { { row, vdupq_n_u8(0) } };
      vst2q_u8((uint8_t *)(dst_ptr + w), row_u16);

      w += 16;
    }
    if (width - w >= 8) {
      uint8x8_t row = vld1_u8(src_ptr + w);
      vst1q_u16(dst_ptr + w, vmovl_u8(row));
      w += 8;
    }
    if (width - w == 4) {
      for (int i = w; i < w + 4; i++) {
        dst_ptr[i] = src_ptr[i];
      }
    }

    src += sstride;
    dst += dstride;
  } while (--height != 0);
}

void cdef_copy_rect8_16bit_to_16bit_neon(uint16_t *dst, int dstride,
                                         const uint16_t *src, int sstride,
                                         int width, int height) {
  do {
    const uint16_t *src_ptr = src;
    uint16_t *dst_ptr = dst;

    int w = 0;
    while (width - w >= 8) {
      uint16x8_t row = vld1q_u16(src_ptr + w);
      vst1q_u16(dst_ptr + w, row);

      w += 8;
    }
    if (width - w == 4) {
      uint16x4_t row = vld1_u16(src_ptr + w);
      vst1_u16(dst_ptr + w, row);
    }

    src += sstride;
    dst += dstride;
  } while (--height != 0);
}

// partial A is a 16-bit vector of the form:
// [x8 x7 x6 x5 x4 x3 x2 x1] and partial B has the form:
// [0  y1 y2 y3 y4 y5 y6 y7].
// This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
// (x7^2+y2^7)*C7 + (x8^2+0^2)*C8 where the C1..C8 constants are in const1
// and const2.
static INLINE uint32x4_t fold_mul_and_sum_neon(int16x8_t partiala,
                                               int16x8_t partialb,
                                               uint32x4_t const1,
                                               uint32x4_t const2) {
  // Reverse partial B.
  // pattern = { 12 13 10 11 8 9 6 7 4 5 2 3 0 1 14 15 }.
  uint8x16_t pattern = vreinterpretq_u8_u64(
      vcombine_u64(vcreate_u64((uint64_t)0x07060908 << 32 | 0x0b0a0d0c),
                   vcreate_u64((uint64_t)0x0f0e0100 << 32 | 0x03020504)));

#if AOM_ARCH_AARCH64
  partialb =
      vreinterpretq_s16_s8(vqtbl1q_s8(vreinterpretq_s8_s16(partialb), pattern));
#else
  int8x8x2_t p = { { vget_low_s8(vreinterpretq_s8_s16(partialb)),
                     vget_high_s8(vreinterpretq_s8_s16(partialb)) } };
  int8x8_t shuffle_hi = vtbl2_s8(p, vget_high_s8(vreinterpretq_s8_u8(pattern)));
  int8x8_t shuffle_lo = vtbl2_s8(p, vget_low_s8(vreinterpretq_s8_u8(pattern)));
  partialb = vreinterpretq_s16_s8(vcombine_s8(shuffle_lo, shuffle_hi));
#endif

  // Square and add the corresponding x and y values.
  int32x4_t cost_lo = vmull_s16(vget_low_s16(partiala), vget_low_s16(partiala));
  cost_lo = vmlal_s16(cost_lo, vget_low_s16(partialb), vget_low_s16(partialb));
  int32x4_t cost_hi =
      vmull_s16(vget_high_s16(partiala), vget_high_s16(partiala));
  cost_hi =
      vmlal_s16(cost_hi, vget_high_s16(partialb), vget_high_s16(partialb));

  // Multiply by constant.
  uint32x4_t cost = vmulq_u32(vreinterpretq_u32_s32(cost_lo), const1);
  cost = vmlaq_u32(cost, vreinterpretq_u32_s32(cost_hi), const2);
  return cost;
}

// This function computes the cost along directions 4, 5, 6, 7. (4 is diagonal
// down-right, 6 is vertical).
//
// For each direction the lines are shifted so that we can perform a
// basic sum on each vector element. For example, direction 5 is "south by
// southeast", so we need to add the pixels along each line i below:
//
// 0  1 2 3 4 5 6 7
// 0  1 2 3 4 5 6 7
// 8  0 1 2 3 4 5 6
// 8  0 1 2 3 4 5 6
// 9  8 0 1 2 3 4 5
// 9  8 0 1 2 3 4 5
// 10 9 8 0 1 2 3 4
// 10 9 8 0 1 2 3 4
//
// For this to fit nicely in vectors, the lines need to be shifted like so:
//        0 1 2 3 4 5 6 7
//        0 1 2 3 4 5 6 7
//      8 0 1 2 3 4 5 6
//      8 0 1 2 3 4 5 6
//    9 8 0 1 2 3 4 5
//    9 8 0 1 2 3 4 5
// 10 9 8 0 1 2 3 4
// 10 9 8 0 1 2 3 4
//
// In this configuration we can now perform SIMD additions to get the cost
// along direction 5. Since this won't fit into a single 128-bit vector, we use
// two of them to compute each half of the new configuration, and pad the empty
// spaces with zeros. Similar shifting is done for other directions, except
// direction 6 which is straightforward as it's the vertical direction.
static INLINE uint32x4_t compute_vert_directions_neon(int16x8_t lines[8],
                                                      uint32_t cost[4]) {
  const int16x8_t zero = vdupq_n_s16(0);

  // Partial sums for lines 0 and 1.
  int16x8_t partial4a = vextq_s16(zero, lines[0], 1);
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[1], 2));
  int16x8_t partial4b = vextq_s16(lines[0], zero, 1);
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[1], zero, 2));
  int16x8_t tmp = vaddq_s16(lines[0], lines[1]);
  int16x8_t partial5a = vextq_s16(zero, tmp, 3);
  int16x8_t partial5b = vextq_s16(tmp, zero, 3);
  int16x8_t partial7a = vextq_s16(zero, tmp, 6);
  int16x8_t partial7b = vextq_s16(tmp, zero, 6);
  int16x8_t partial6 = tmp;

  // Partial sums for lines 2 and 3.
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[2], 3));
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[3], 4));
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[2], zero, 3));
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[3], zero, 4));
  tmp = vaddq_s16(lines[2], lines[3]);
  partial5a = vaddq_s16(partial5a, vextq_s16(zero, tmp, 4));
  partial5b = vaddq_s16(partial5b, vextq_s16(tmp, zero, 4));
  partial7a = vaddq_s16(partial7a, vextq_s16(zero, tmp, 5));
  partial7b = vaddq_s16(partial7b, vextq_s16(tmp, zero, 5));
  partial6 = vaddq_s16(partial6, tmp);

  // Partial sums for lines 4 and 5.
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[4], 5));
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[5], 6));
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[4], zero, 5));
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[5], zero, 6));
  tmp = vaddq_s16(lines[4], lines[5]);
  partial5a = vaddq_s16(partial5a, vextq_s16(zero, tmp, 5));
  partial5b = vaddq_s16(partial5b, vextq_s16(tmp, zero, 5));
  partial7a = vaddq_s16(partial7a, vextq_s16(zero, tmp, 4));
  partial7b = vaddq_s16(partial7b, vextq_s16(tmp, zero, 4));
  partial6 = vaddq_s16(partial6, tmp);

  // Partial sums for lines 6 and 7.
  partial4a = vaddq_s16(partial4a, vextq_s16(zero, lines[6], 7));
  partial4a = vaddq_s16(partial4a, lines[7]);
  partial4b = vaddq_s16(partial4b, vextq_s16(lines[6], zero, 7));
  tmp = vaddq_s16(lines[6], lines[7]);
  partial5a = vaddq_s16(partial5a, vextq_s16(zero, tmp, 6));
  partial5b = vaddq_s16(partial5b, vextq_s16(tmp, zero, 6));
  partial7a = vaddq_s16(partial7a, vextq_s16(zero, tmp, 3));
  partial7b = vaddq_s16(partial7b, vextq_s16(tmp, zero, 3));
  partial6 = vaddq_s16(partial6, tmp);

  uint32x4_t const0 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)420 << 32 | 840),
                   vcreate_u64((uint64_t)210 << 32 | 280)));
  uint32x4_t const1 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)140 << 32 | 168),
                   vcreate_u64((uint64_t)105 << 32 | 120)));
  uint32x4_t const2 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64(0), vcreate_u64((uint64_t)210 << 32 | 420)));
  uint32x4_t const3 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)105 << 32 | 140),
                   vcreate_u64((uint64_t)105 << 32 | 105)));

  // Compute costs in terms of partial sums.
  int32x4_t partial6_s32 =
      vmull_s16(vget_low_s16(partial6), vget_low_s16(partial6));
  partial6_s32 =
      vmlal_s16(partial6_s32, vget_high_s16(partial6), vget_high_s16(partial6));

  uint32x4_t costs[4];
  costs[0] = fold_mul_and_sum_neon(partial4a, partial4b, const0, const1);
  costs[1] = fold_mul_and_sum_neon(partial5a, partial5b, const2, const3);
  costs[2] = vmulq_n_u32(vreinterpretq_u32_s32(partial6_s32), 105);
  costs[3] = fold_mul_and_sum_neon(partial7a, partial7b, const2, const3);

  costs[0] = horizontal_add_4d_u32x4(costs);
  vst1q_u32(cost, costs[0]);
  return costs[0];
}

static INLINE uint32x4_t fold_mul_and_sum_pairwise_neon(int16x8_t partiala,
                                                        int16x8_t partialb,
                                                        int16x8_t partialc,
                                                        uint32x4_t const0) {
  // Reverse partial c.
  // pattern = { 10 11 8 9 6 7 4 5 2 3 0 1 12 13 14 15 }.
  uint8x16_t pattern = vreinterpretq_u8_u64(
      vcombine_u64(vcreate_u64((uint64_t)0x05040706 << 32 | 0x09080b0a),
                   vcreate_u64((uint64_t)0x0f0e0d0c << 32 | 0x01000302)));

#if AOM_ARCH_AARCH64
  partialc =
      vreinterpretq_s16_s8(vqtbl1q_s8(vreinterpretq_s8_s16(partialc), pattern));
#else
  int8x8x2_t p = { { vget_low_s8(vreinterpretq_s8_s16(partialc)),
                     vget_high_s8(vreinterpretq_s8_s16(partialc)) } };
  int8x8_t shuffle_hi = vtbl2_s8(p, vget_high_s8(vreinterpretq_s8_u8(pattern)));
  int8x8_t shuffle_lo = vtbl2_s8(p, vget_low_s8(vreinterpretq_s8_u8(pattern)));
  partialc = vreinterpretq_s16_s8(vcombine_s8(shuffle_lo, shuffle_hi));
#endif

  int32x4_t partiala_s32 = vpaddlq_s16(partiala);
  int32x4_t partialb_s32 = vpaddlq_s16(partialb);
  int32x4_t partialc_s32 = vpaddlq_s16(partialc);

  partiala_s32 = vmulq_s32(partiala_s32, partiala_s32);
  partialb_s32 = vmulq_s32(partialb_s32, partialb_s32);
  partialc_s32 = vmulq_s32(partialc_s32, partialc_s32);

  partiala_s32 = vaddq_s32(partiala_s32, partialc_s32);

  uint32x4_t cost = vmulq_n_u32(vreinterpretq_u32_s32(partialb_s32), 105);
  cost = vmlaq_u32(cost, vreinterpretq_u32_s32(partiala_s32), const0);
  return cost;
}

// This function computes the cost along directions 0, 1, 2, 3. (0 means
// 45-degree up-right, 2 is horizontal).
//
// For direction 1 and 3 ("east northeast" and "east southeast") the shifted
// lines need three vectors instead of two. For direction 1 for example, we need
// to compute the sums along the line i below:
// 0 0 1 1 2 2 3  3
// 1 1 2 2 3 3 4  4
// 2 2 3 3 4 4 5  5
// 3 3 4 4 5 5 6  6
// 4 4 5 5 6 6 7  7
// 5 5 6 6 7 7 8  8
// 6 6 7 7 8 8 9  9
// 7 7 8 8 9 9 10 10
//
// Which means we need the following configuration:
// 0 0 1 1 2 2 3 3
//     1 1 2 2 3 3 4 4
//         2 2 3 3 4 4 5 5
//             3 3 4 4 5 5 6 6
//                 4 4 5 5 6 6 7 7
//                     5 5 6 6 7 7 8 8
//                         6 6 7 7 8 8 9 9
//                             7 7 8 8 9 9 10 10
//
// Three vectors are needed to compute this, as well as some extra pairwise
// additions.
static uint32x4_t compute_horiz_directions_neon(int16x8_t lines[8],
                                                uint32_t cost[4]) {
  const int16x8_t zero = vdupq_n_s16(0);

  // Compute diagonal directions (1, 2, 3).
  // Partial sums for lines 0 and 1.
  int16x8_t partial0a = lines[0];
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[1], 7));
  int16x8_t partial0b = vextq_s16(lines[1], zero, 7);
  int16x8_t partial1a = vaddq_s16(lines[0], vextq_s16(zero, lines[1], 6));
  int16x8_t partial1b = vextq_s16(lines[1], zero, 6);
  int16x8_t partial3a = vextq_s16(lines[0], zero, 2);
  partial3a = vaddq_s16(partial3a, vextq_s16(lines[1], zero, 4));
  int16x8_t partial3b = vextq_s16(zero, lines[0], 2);
  partial3b = vaddq_s16(partial3b, vextq_s16(zero, lines[1], 4));

  // Partial sums for lines 2 and 3.
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[2], 6));
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[3], 5));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[2], zero, 6));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[3], zero, 5));
  partial1a = vaddq_s16(partial1a, vextq_s16(zero, lines[2], 4));
  partial1a = vaddq_s16(partial1a, vextq_s16(zero, lines[3], 2));
  partial1b = vaddq_s16(partial1b, vextq_s16(lines[2], zero, 4));
  partial1b = vaddq_s16(partial1b, vextq_s16(lines[3], zero, 2));
  partial3a = vaddq_s16(partial3a, vextq_s16(lines[2], zero, 6));
  partial3b = vaddq_s16(partial3b, vextq_s16(zero, lines[2], 6));
  partial3b = vaddq_s16(partial3b, lines[3]);

  // Partial sums for lines 4 and 5.
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[4], 4));
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[5], 3));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[4], zero, 4));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[5], zero, 3));
  partial1b = vaddq_s16(partial1b, lines[4]);
  partial1b = vaddq_s16(partial1b, vextq_s16(zero, lines[5], 6));
  int16x8_t partial1c = vextq_s16(lines[5], zero, 6);
  partial3b = vaddq_s16(partial3b, vextq_s16(lines[4], zero, 2));
  partial3b = vaddq_s16(partial3b, vextq_s16(lines[5], zero, 4));
  int16x8_t partial3c = vextq_s16(zero, lines[4], 2);
  partial3c = vaddq_s16(partial3c, vextq_s16(zero, lines[5], 4));

  // Partial sums for lines 6 and 7.
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[6], 2));
  partial0a = vaddq_s16(partial0a, vextq_s16(zero, lines[7], 1));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[6], zero, 2));
  partial0b = vaddq_s16(partial0b, vextq_s16(lines[7], zero, 1));
  partial1b = vaddq_s16(partial1b, vextq_s16(zero, lines[6], 4));
  partial1b = vaddq_s16(partial1b, vextq_s16(zero, lines[7], 2));
  partial1c = vaddq_s16(partial1c, vextq_s16(lines[6], zero, 4));
  partial1c = vaddq_s16(partial1c, vextq_s16(lines[7], zero, 2));
  partial3b = vaddq_s16(partial3b, vextq_s16(lines[6], zero, 6));
  partial3c = vaddq_s16(partial3c, vextq_s16(zero, lines[6], 6));
  partial3c = vaddq_s16(partial3c, lines[7]);

  // Special case for direction 2 as it's just a sum along each line.
  int16x8_t lines03[4] = { lines[0], lines[1], lines[2], lines[3] };
  int16x8_t lines47[4] = { lines[4], lines[5], lines[6], lines[7] };
  int32x4_t partial2a = horizontal_add_4d_s16x8(lines03);
  int32x4_t partial2b = horizontal_add_4d_s16x8(lines47);

  uint32x4_t partial2a_u32 =
      vreinterpretq_u32_s32(vmulq_s32(partial2a, partial2a));
  uint32x4_t partial2b_u32 =
      vreinterpretq_u32_s32(vmulq_s32(partial2b, partial2b));

  uint32x4_t const0 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)420 << 32 | 840),
                   vcreate_u64((uint64_t)210 << 32 | 280)));
  uint32x4_t const1 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)140 << 32 | 168),
                   vcreate_u64((uint64_t)105 << 32 | 120)));
  uint32x4_t const2 = vreinterpretq_u32_u64(
      vcombine_u64(vcreate_u64((uint64_t)210 << 32 | 420),
                   vcreate_u64((uint64_t)105 << 32 | 140)));

  uint32x4_t costs[4];
  costs[0] = fold_mul_and_sum_neon(partial0a, partial0b, const0, const1);
  costs[1] =
      fold_mul_and_sum_pairwise_neon(partial1a, partial1b, partial1c, const2);
  costs[2] = vaddq_u32(partial2a_u32, partial2b_u32);
  costs[2] = vmulq_n_u32(costs[2], 105);
  costs[3] =
      fold_mul_and_sum_pairwise_neon(partial3c, partial3b, partial3a, const2);

  costs[0] = horizontal_add_4d_u32x4(costs);
  vst1q_u32(cost, costs[0]);
  return costs[0];
}

int cdef_find_dir_neon(const uint16_t *img, int stride, int32_t *var,
                       int coeff_shift) {
  uint32_t cost[8];
  uint32_t best_cost = 0;
  int best_dir = 0;
  int16x8_t lines[8];
  for (int i = 0; i < 8; i++) {
    uint16x8_t s = vld1q_u16(&img[i * stride]);
    lines[i] = vreinterpretq_s16_u16(
        vsubq_u16(vshlq_u16(s, vdupq_n_s16(-coeff_shift)), vdupq_n_u16(128)));
  }

  // Compute "mostly vertical" directions.
  uint32x4_t cost47 = compute_vert_directions_neon(lines, cost + 4);

  // Compute "mostly horizontal" directions.
  uint32x4_t cost03 = compute_horiz_directions_neon(lines, cost);

  // Find max cost as well as its index to get best_dir.
  // The max cost needs to be propagated in the whole vector to find its
  // position in the original cost vectors cost03 and cost47.
  uint32x4_t cost07 = vmaxq_u32(cost03, cost47);
#if AOM_ARCH_AARCH64
  best_cost = vmaxvq_u32(cost07);
  uint32x4_t max_cost = vdupq_n_u32(best_cost);
  uint8x16x2_t costs = { { vreinterpretq_u8_u32(vceqq_u32(max_cost, cost03)),
                           vreinterpretq_u8_u32(
                               vceqq_u32(max_cost, cost47)) } };
  // idx = { 28, 24, 20, 16, 12, 8, 4, 0 };
  uint8x8_t idx = vreinterpret_u8_u64(vcreate_u64(0x0004080c1014181cULL));
  // Get the lowest 8 bit of each 32-bit elements and reverse them.
  uint8x8_t tbl = vqtbl2_u8(costs, idx);
  uint64_t a = vget_lane_u64(vreinterpret_u64_u8(tbl), 0);
  best_dir = aom_clzll(a) >> 3;
#else
  uint32x2_t cost64 = vpmax_u32(vget_low_u32(cost07), vget_high_u32(cost07));
  cost64 = vpmax_u32(cost64, cost64);
  uint32x4_t max_cost = vcombine_u32(cost64, cost64);
  best_cost = vget_lane_u32(cost64, 0);
  uint16x8_t costs = vcombine_u16(vmovn_u32(vceqq_u32(max_cost, cost03)),
                                  vmovn_u32(vceqq_u32(max_cost, cost47)));
  uint8x8_t idx =
      vand_u8(vmovn_u16(costs),
              vreinterpret_u8_u64(vcreate_u64(0x8040201008040201ULL)));
  int sum = horizontal_add_u8x8(idx);
  best_dir = get_msb(sum ^ (sum - 1));
#endif

  // Difference between the optimal variance and the variance along the
  // orthogonal direction. Again, the sum(x^2) terms cancel out.
  *var = best_cost - cost[(best_dir + 4) & 7];
  // We'd normally divide by 840, but dividing by 1024 is close enough
  // for what we're going to do with this.
  *var >>= 10;
  return best_dir;
}

void cdef_find_dir_dual_neon(const uint16_t *img1, const uint16_t *img2,
                             int stride, int32_t *var_out_1st,
                             int32_t *var_out_2nd, int coeff_shift,
                             int *out_dir_1st_8x8, int *out_dir_2nd_8x8) {
  // Process first 8x8.
  *out_dir_1st_8x8 = cdef_find_dir(img1, stride, var_out_1st, coeff_shift);

  // Process second 8x8.
  *out_dir_2nd_8x8 = cdef_find_dir(img2, stride, var_out_2nd, coeff_shift);
}

// sign(a-b) * min(abs(a-b), max(0, threshold - (abs(a-b) >> adjdamp)))
static INLINE int16x8_t constrain16(uint16x8_t a, uint16x8_t b,
                                    unsigned int threshold, int adjdamp) {
  uint16x8_t diff = vabdq_u16(a, b);
  const uint16x8_t a_gt_b = vcgtq_u16(a, b);
  const uint16x8_t s = vqsubq_u16(vdupq_n_u16(threshold),
                                  vshlq_u16(diff, vdupq_n_s16(-adjdamp)));
  const int16x8_t clip = vreinterpretq_s16_u16(vminq_u16(diff, s));
  return vbslq_s16(a_gt_b, clip, vnegq_s16(clip));
}

static INLINE void primary_filter(uint16x8_t s, uint16x8_t tap[4],
                                  const int *pri_taps, int pri_strength,
                                  int pri_damping, int16x8_t *sum) {
  // Near taps
  int16x8_t n0 = constrain16(tap[0], s, pri_strength, pri_damping);
  int16x8_t n1 = constrain16(tap[1], s, pri_strength, pri_damping);
  // sum += pri_taps[0] * (n0 + n1)
  n0 = vaddq_s16(n0, n1);
  *sum = vmlaq_n_s16(*sum, n0, pri_taps[0]);

  // Far taps
  int16x8_t f0 = constrain16(tap[2], s, pri_strength, pri_damping);
  int16x8_t f1 = constrain16(tap[3], s, pri_strength, pri_damping);
  // sum += pri_taps[1] * (f0 + f1)
  f0 = vaddq_s16(f0, f1);
  *sum = vmlaq_n_s16(*sum, f0, pri_taps[1]);
}

static INLINE void secondary_filter(uint16x8_t s, uint16x8_t tap[8],
                                    const int *sec_taps, int sec_strength,
                                    int sec_damping, int16x8_t *sum) {
  // Near taps
  int16x8_t s0 = constrain16(tap[0], s, sec_strength, sec_damping);
  int16x8_t s1 = constrain16(tap[1], s, sec_strength, sec_damping);
  int16x8_t s2 = constrain16(tap[2], s, sec_strength, sec_damping);
  int16x8_t s3 = constrain16(tap[3], s, sec_strength, sec_damping);

  // sum += sec_taps[0] * (p0 + p1 + p2 + p3)
  s0 = vaddq_s16(s0, s1);
  s2 = vaddq_s16(s2, s3);
  s0 = vaddq_s16(s0, s2);
  *sum = vmlaq_n_s16(*sum, s0, sec_taps[0]);

  // Far taps
  s0 = constrain16(tap[4], s, sec_strength, sec_damping);
  s1 = constrain16(tap[5], s, sec_strength, sec_damping);
  s2 = constrain16(tap[6], s, sec_strength, sec_damping);
  s3 = constrain16(tap[7], s, sec_strength, sec_damping);

  // sum += sec_taps[1] * (p0 + p1 + p2 + p3)
  s0 = vaddq_s16(s0, s1);
  s2 = vaddq_s16(s2, s3);
  s0 = vaddq_s16(s0, s2);
  *sum = vmlaq_n_s16(*sum, s0, sec_taps[1]);
}

void cdef_filter_8_0_neon(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  uint16x8_t max, min;
  const uint16x8_t cdef_large_value_mask =
      vdupq_n_u16(((uint16_t)~CDEF_VERY_LARGE));
  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];
  const int *sec_taps = cdef_sec_taps;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }
  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);
      max = min = s;

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = vld1q_u16(in + po1);
      pri_src[1] = vld1q_u16(in - po1);

      // Primary far taps
      pri_src[2] = vld1q_u16(in + po2);
      pri_src[3] = vld1q_u16(in - po2);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      // The source is 16 bits, however, we only really care about the lower
      // 8 bits.  The upper 8 bits contain the "large" flag.  After the final
      // primary max has been calculated, zero out the upper 8 bits.  Use this
      // to find the "16 bit" max.
      uint8x16_t pri_max0 = vmaxq_u8(vreinterpretq_u8_u16(pri_src[0]),
                                     vreinterpretq_u8_u16(pri_src[1]));
      uint8x16_t pri_max1 = vmaxq_u8(vreinterpretq_u8_u16(pri_src[2]),
                                     vreinterpretq_u8_u16(pri_src[3]));
      pri_max0 = vmaxq_u8(pri_max0, pri_max1);
      max = vmaxq_u16(max, vandq_u16(vreinterpretq_u16_u8(pri_max0),
                                     cdef_large_value_mask));

      uint16x8_t pri_min0 = vminq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_min1 = vminq_u16(pri_src[2], pri_src[3]);
      pri_min0 = vminq_u16(pri_min0, pri_min1);
      min = vminq_u16(min, pri_min0);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = vld1q_u16(in + s1o1);
      sec_src[1] = vld1q_u16(in - s1o1);
      sec_src[2] = vld1q_u16(in + s2o1);
      sec_src[3] = vld1q_u16(in - s2o1);

      // Secondary far taps
      sec_src[4] = vld1q_u16(in + s1o2);
      sec_src[5] = vld1q_u16(in - s1o2);
      sec_src[6] = vld1q_u16(in + s2o2);
      sec_src[7] = vld1q_u16(in - s2o2);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // The source is 16 bits, however, we only really care about the lower
      // 8 bits.  The upper 8 bits contain the "large" flag.  After the final
      // primary max has been calculated, zero out the upper 8 bits.  Use this
      // to find the "16 bit" max.
      uint8x16_t sec_max0 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[0]),
                                     vreinterpretq_u8_u16(sec_src[1]));
      uint8x16_t sec_max1 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[2]),
                                     vreinterpretq_u8_u16(sec_src[3]));
      uint8x16_t sec_max2 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[4]),
                                     vreinterpretq_u8_u16(sec_src[5]));
      uint8x16_t sec_max3 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[6]),
                                     vreinterpretq_u8_u16(sec_src[7]));
      sec_max0 = vmaxq_u8(sec_max0, sec_max1);
      sec_max2 = vmaxq_u8(sec_max2, sec_max3);
      sec_max0 = vmaxq_u8(sec_max0, sec_max2);
      max = vmaxq_u16(max, vandq_u16(vreinterpretq_u16_u8(sec_max0),
                                     cdef_large_value_mask));

      uint16x8_t sec_min0 = vminq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_min1 = vminq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_min2 = vminq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_min3 = vminq_u16(sec_src[6], sec_src[7]);
      sec_min0 = vminq_u16(sec_min0, sec_min1);
      sec_min2 = vminq_u16(sec_min2, sec_min3);
      sec_min0 = vminq_u16(sec_min0, sec_min2);
      min = vminq_u16(min, sec_min0);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      res_s16 = vminq_s16(vmaxq_s16(res_s16, vreinterpretq_s16_u16(min)),
                          vreinterpretq_s16_u16(max));

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      vst1_u8(dst8, res_u8);

      in += CDEF_BSTRIDE;
      dst8 += dstride;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);
      max = min = s;

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = load_unaligned_u16_4x2(in + po1, CDEF_BSTRIDE);
      pri_src[1] = load_unaligned_u16_4x2(in - po1, CDEF_BSTRIDE);

      // Primary far taps
      pri_src[2] = load_unaligned_u16_4x2(in + po2, CDEF_BSTRIDE);
      pri_src[3] = load_unaligned_u16_4x2(in - po2, CDEF_BSTRIDE);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      // The source is 16 bits, however, we only really care about the lower
      // 8 bits.  The upper 8 bits contain the "large" flag.  After the final
      // primary max has been calculated, zero out the upper 8 bits.  Use this
      // to find the "16 bit" max.
      uint8x16_t pri_max0 = vmaxq_u8(vreinterpretq_u8_u16(pri_src[0]),
                                     vreinterpretq_u8_u16(pri_src[1]));
      uint8x16_t pri_max1 = vmaxq_u8(vreinterpretq_u8_u16(pri_src[2]),
                                     vreinterpretq_u8_u16(pri_src[3]));
      pri_max0 = vmaxq_u8(pri_max0, pri_max1);
      max = vmaxq_u16(max, vandq_u16(vreinterpretq_u16_u8(pri_max0),
                                     cdef_large_value_mask));

      uint16x8_t pri_min1 = vminq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_min2 = vminq_u16(pri_src[2], pri_src[3]);
      pri_min1 = vminq_u16(pri_min1, pri_min2);
      min = vminq_u16(min, pri_min1);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = load_unaligned_u16_4x2(in + s1o1, CDEF_BSTRIDE);
      sec_src[1] = load_unaligned_u16_4x2(in - s1o1, CDEF_BSTRIDE);
      sec_src[2] = load_unaligned_u16_4x2(in + s2o1, CDEF_BSTRIDE);
      sec_src[3] = load_unaligned_u16_4x2(in - s2o1, CDEF_BSTRIDE);

      // Secondary far taps
      sec_src[4] = load_unaligned_u16_4x2(in + s1o2, CDEF_BSTRIDE);
      sec_src[5] = load_unaligned_u16_4x2(in - s1o2, CDEF_BSTRIDE);
      sec_src[6] = load_unaligned_u16_4x2(in + s2o2, CDEF_BSTRIDE);
      sec_src[7] = load_unaligned_u16_4x2(in - s2o2, CDEF_BSTRIDE);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // The source is 16 bits, however, we only really care about the lower
      // 8 bits.  The upper 8 bits contain the "large" flag.  After the final
      // primary max has been calculated, zero out the upper 8 bits.  Use this
      // to find the "16 bit" max.
      uint8x16_t sec_max0 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[0]),
                                     vreinterpretq_u8_u16(sec_src[1]));
      uint8x16_t sec_max1 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[2]),
                                     vreinterpretq_u8_u16(sec_src[3]));
      uint8x16_t sec_max2 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[4]),
                                     vreinterpretq_u8_u16(sec_src[5]));
      uint8x16_t sec_max3 = vmaxq_u8(vreinterpretq_u8_u16(sec_src[6]),
                                     vreinterpretq_u8_u16(sec_src[7]));
      sec_max0 = vmaxq_u8(sec_max0, sec_max1);
      sec_max2 = vmaxq_u8(sec_max2, sec_max3);
      sec_max0 = vmaxq_u8(sec_max0, sec_max2);
      max = vmaxq_u16(max, vandq_u16(vreinterpretq_u16_u8(sec_max0),
                                     cdef_large_value_mask));

      uint16x8_t sec_min0 = vminq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_min1 = vminq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_min2 = vminq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_min3 = vminq_u16(sec_src[6], sec_src[7]);
      sec_min0 = vminq_u16(sec_min0, sec_min1);
      sec_min2 = vminq_u16(sec_min2, sec_min3);
      sec_min0 = vminq_u16(sec_min0, sec_min2);
      min = vminq_u16(min, sec_min0);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      res_s16 = vminq_s16(vmaxq_s16(res_s16, vreinterpretq_s16_u16(min)),
                          vreinterpretq_s16_u16(max));

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      store_u8x4_strided_x2(dst8, dstride, res_u8);

      in += 2 * CDEF_BSTRIDE;
      dst8 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_1_neon(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)sec_strength;
  (void)sec_damping;

  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);

      uint16x8_t tap[4];

      // Primary near taps
      tap[0] = vld1q_u16(in + po1);
      tap[1] = vld1q_u16(in - po1);

      // Primary far taps
      tap[2] = vld1q_u16(in + po2);
      tap[3] = vld1q_u16(in - po2);

      primary_filter(s, tap, pri_taps, pri_strength, pri_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      vst1_u8(dst8, res_u8);

      in += CDEF_BSTRIDE;
      dst8 += dstride;
    } while (--h != 0);

  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = load_unaligned_u16_4x2(in + po1, CDEF_BSTRIDE);
      pri_src[1] = load_unaligned_u16_4x2(in - po1, CDEF_BSTRIDE);

      // Primary far taps
      pri_src[2] = load_unaligned_u16_4x2(in + po2, CDEF_BSTRIDE);
      pri_src[3] = load_unaligned_u16_4x2(in - po2, CDEF_BSTRIDE);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      store_u8x4_strided_x2(dst8, dstride, res_u8);

      in += 2 * CDEF_BSTRIDE;
      dst8 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_2_neon(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)pri_strength;
  (void)pri_damping;
  (void)coeff_shift;

  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  const int *sec_taps = cdef_sec_taps;

  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = vld1q_u16(in + s1o1);
      sec_src[1] = vld1q_u16(in - s1o1);
      sec_src[2] = vld1q_u16(in + s2o1);
      sec_src[3] = vld1q_u16(in - s2o1);

      // Secondary far taps
      sec_src[4] = vld1q_u16(in + s1o2);
      sec_src[5] = vld1q_u16(in - s1o2);
      sec_src[6] = vld1q_u16(in + s2o2);
      sec_src[7] = vld1q_u16(in - s2o2);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      vst1_u8(dst8, res_u8);

      in += CDEF_BSTRIDE;
      dst8 += dstride;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = load_unaligned_u16_4x2(in + s1o1, CDEF_BSTRIDE);
      sec_src[1] = load_unaligned_u16_4x2(in - s1o1, CDEF_BSTRIDE);
      sec_src[2] = load_unaligned_u16_4x2(in + s2o1, CDEF_BSTRIDE);
      sec_src[3] = load_unaligned_u16_4x2(in - s2o1, CDEF_BSTRIDE);

      // Secondary far taps
      sec_src[4] = load_unaligned_u16_4x2(in + s1o2, CDEF_BSTRIDE);
      sec_src[5] = load_unaligned_u16_4x2(in - s1o2, CDEF_BSTRIDE);
      sec_src[6] = load_unaligned_u16_4x2(in + s2o2, CDEF_BSTRIDE);
      sec_src[7] = load_unaligned_u16_4x2(in - s2o2, CDEF_BSTRIDE);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res_s16 = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      const uint8x8_t res_u8 = vqmovun_s16(res_s16);
      store_u8x4_strided_x2(dst8, dstride, res_u8);

      in += 2 * CDEF_BSTRIDE;
      dst8 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_3_neon(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)pri_strength;
  (void)sec_strength;
  (void)dir;
  (void)pri_damping;
  (void)sec_damping;
  (void)coeff_shift;
  (void)block_width;
  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      const uint16x8_t s = vld1q_u16(in);
      const uint8x8_t res = vqmovn_u16(s);
      vst1_u8(dst8, res);

      in += CDEF_BSTRIDE;
      dst8 += dstride;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    do {
      const uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);
      const uint8x8_t res = vqmovn_u16(s);
      store_u8x4_strided_x2(dst8, dstride, res);

      in += 2 * CDEF_BSTRIDE;
      dst8 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_0_neon(void *dest, int dstride, const uint16_t *in,
                           int pri_strength, int sec_strength, int dir,
                           int pri_damping, int sec_damping, int coeff_shift,
                           int block_width, int block_height) {
  uint16x8_t max, min;
  const uint16x8_t cdef_large_value_mask =
      vdupq_n_u16(((uint16_t)~CDEF_VERY_LARGE));
  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];
  const int *sec_taps = cdef_sec_taps;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }
  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);
      max = min = s;

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = vld1q_u16(in + po1);
      pri_src[1] = vld1q_u16(in - po1);

      // Primary far taps
      pri_src[2] = vld1q_u16(in + po2);
      pri_src[3] = vld1q_u16(in - po2);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      uint16x8_t pri_min0 = vminq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_min1 = vminq_u16(pri_src[2], pri_src[3]);
      pri_min0 = vminq_u16(pri_min0, pri_min1);
      min = vminq_u16(min, pri_min0);

      /* Convert CDEF_VERY_LARGE to 0 before calculating max. */
      pri_src[0] = vandq_u16(pri_src[0], cdef_large_value_mask);
      pri_src[1] = vandq_u16(pri_src[1], cdef_large_value_mask);
      pri_src[2] = vandq_u16(pri_src[2], cdef_large_value_mask);
      pri_src[3] = vandq_u16(pri_src[3], cdef_large_value_mask);

      uint16x8_t pri_max0 = vmaxq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_max1 = vmaxq_u16(pri_src[2], pri_src[3]);
      pri_max0 = vmaxq_u16(pri_max0, pri_max1);
      max = vmaxq_u16(max, pri_max0);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = vld1q_u16(in + s1o1);
      sec_src[1] = vld1q_u16(in - s1o1);
      sec_src[2] = vld1q_u16(in + s2o1);
      sec_src[3] = vld1q_u16(in - s2o1);

      // Secondary far taps
      sec_src[4] = vld1q_u16(in + s1o2);
      sec_src[5] = vld1q_u16(in - s1o2);
      sec_src[6] = vld1q_u16(in + s2o2);
      sec_src[7] = vld1q_u16(in - s2o2);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      uint16x8_t sec_min0 = vminq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_min1 = vminq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_min2 = vminq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_min3 = vminq_u16(sec_src[6], sec_src[7]);
      sec_min0 = vminq_u16(sec_min0, sec_min1);
      sec_min2 = vminq_u16(sec_min2, sec_min3);
      sec_min0 = vminq_u16(sec_min0, sec_min2);
      min = vminq_u16(min, sec_min0);

      /* Convert CDEF_VERY_LARGE to 0 before calculating max. */
      sec_src[0] = vandq_u16(sec_src[0], cdef_large_value_mask);
      sec_src[1] = vandq_u16(sec_src[1], cdef_large_value_mask);
      sec_src[2] = vandq_u16(sec_src[2], cdef_large_value_mask);
      sec_src[3] = vandq_u16(sec_src[3], cdef_large_value_mask);
      sec_src[4] = vandq_u16(sec_src[4], cdef_large_value_mask);
      sec_src[5] = vandq_u16(sec_src[5], cdef_large_value_mask);
      sec_src[6] = vandq_u16(sec_src[6], cdef_large_value_mask);
      sec_src[7] = vandq_u16(sec_src[7], cdef_large_value_mask);

      uint16x8_t sec_max0 = vmaxq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_max1 = vmaxq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_max2 = vmaxq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_max3 = vmaxq_u16(sec_src[6], sec_src[7]);
      sec_max0 = vmaxq_u16(sec_max0, sec_max1);
      sec_max2 = vmaxq_u16(sec_max2, sec_max3);
      sec_max0 = vmaxq_u16(sec_max0, sec_max2);
      max = vmaxq_u16(max, sec_max0);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      res = vminq_s16(vmaxq_s16(res, vreinterpretq_s16_u16(min)),
                      vreinterpretq_s16_u16(max));

      vst1q_u16(dst16, vreinterpretq_u16_s16(res));

      in += CDEF_BSTRIDE;
      dst16 += dstride;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);
      max = min = s;

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = load_unaligned_u16_4x2(in + po1, CDEF_BSTRIDE);
      pri_src[1] = load_unaligned_u16_4x2(in - po1, CDEF_BSTRIDE);

      // Primary far taps
      pri_src[2] = load_unaligned_u16_4x2(in + po2, CDEF_BSTRIDE);
      pri_src[3] = load_unaligned_u16_4x2(in - po2, CDEF_BSTRIDE);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      uint16x8_t pri_min1 = vminq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_min2 = vminq_u16(pri_src[2], pri_src[3]);
      pri_min1 = vminq_u16(pri_min1, pri_min2);
      min = vminq_u16(min, pri_min1);

      /* Convert CDEF_VERY_LARGE to 0 before calculating max. */
      pri_src[0] = vandq_u16(pri_src[0], cdef_large_value_mask);
      pri_src[1] = vandq_u16(pri_src[1], cdef_large_value_mask);
      pri_src[2] = vandq_u16(pri_src[2], cdef_large_value_mask);
      pri_src[3] = vandq_u16(pri_src[3], cdef_large_value_mask);
      uint16x8_t pri_max0 = vmaxq_u16(pri_src[0], pri_src[1]);
      uint16x8_t pri_max1 = vmaxq_u16(pri_src[2], pri_src[3]);
      pri_max0 = vmaxq_u16(pri_max0, pri_max1);
      max = vmaxq_u16(max, pri_max0);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = load_unaligned_u16_4x2(in + s1o1, CDEF_BSTRIDE);
      sec_src[1] = load_unaligned_u16_4x2(in - s1o1, CDEF_BSTRIDE);
      sec_src[2] = load_unaligned_u16_4x2(in + s2o1, CDEF_BSTRIDE);
      sec_src[3] = load_unaligned_u16_4x2(in - s2o1, CDEF_BSTRIDE);

      // Secondary far taps
      sec_src[4] = load_unaligned_u16_4x2(in + s1o2, CDEF_BSTRIDE);
      sec_src[5] = load_unaligned_u16_4x2(in - s1o2, CDEF_BSTRIDE);
      sec_src[6] = load_unaligned_u16_4x2(in + s2o2, CDEF_BSTRIDE);
      sec_src[7] = load_unaligned_u16_4x2(in - s2o2, CDEF_BSTRIDE);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      uint16x8_t sec_min0 = vminq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_min1 = vminq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_min2 = vminq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_min3 = vminq_u16(sec_src[6], sec_src[7]);
      sec_min0 = vminq_u16(sec_min0, sec_min1);
      sec_min2 = vminq_u16(sec_min2, sec_min3);
      sec_min0 = vminq_u16(sec_min0, sec_min2);
      min = vminq_u16(min, sec_min0);

      /* Convert CDEF_VERY_LARGE to 0 before calculating max. */
      sec_src[0] = vandq_u16(sec_src[0], cdef_large_value_mask);
      sec_src[1] = vandq_u16(sec_src[1], cdef_large_value_mask);
      sec_src[2] = vandq_u16(sec_src[2], cdef_large_value_mask);
      sec_src[3] = vandq_u16(sec_src[3], cdef_large_value_mask);
      sec_src[4] = vandq_u16(sec_src[4], cdef_large_value_mask);
      sec_src[5] = vandq_u16(sec_src[5], cdef_large_value_mask);
      sec_src[6] = vandq_u16(sec_src[6], cdef_large_value_mask);
      sec_src[7] = vandq_u16(sec_src[7], cdef_large_value_mask);

      uint16x8_t sec_max0 = vmaxq_u16(sec_src[0], sec_src[1]);
      uint16x8_t sec_max1 = vmaxq_u16(sec_src[2], sec_src[3]);
      uint16x8_t sec_max2 = vmaxq_u16(sec_src[4], sec_src[5]);
      uint16x8_t sec_max3 = vmaxq_u16(sec_src[6], sec_src[7]);
      sec_max0 = vmaxq_u16(sec_max0, sec_max1);
      sec_max2 = vmaxq_u16(sec_max2, sec_max3);
      sec_max0 = vmaxq_u16(sec_max0, sec_max2);
      max = vmaxq_u16(max, sec_max0);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      res = vminq_s16(vmaxq_s16(res, vreinterpretq_s16_u16(min)),
                      vreinterpretq_s16_u16(max));

      store_u16x4_strided_x2(dst16, dstride, vreinterpretq_u16_s16(res));

      in += 2 * CDEF_BSTRIDE;
      dst16 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_1_neon(void *dest, int dstride, const uint16_t *in,
                           int pri_strength, int sec_strength, int dir,
                           int pri_damping, int sec_damping, int coeff_shift,
                           int block_width, int block_height) {
  (void)sec_strength;
  (void)sec_damping;

  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);

      uint16x8_t tap[4];

      // Primary near taps
      tap[0] = vld1q_u16(in + po1);
      tap[1] = vld1q_u16(in - po1);

      // Primary far taps
      tap[2] = vld1q_u16(in + po2);
      tap[3] = vld1q_u16(in - po2);

      primary_filter(s, tap, pri_taps, pri_strength, pri_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      vst1q_u16(dst16, vreinterpretq_u16_s16(res));

      in += CDEF_BSTRIDE;
      dst16 += dstride;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);

      uint16x8_t pri_src[4];

      // Primary near taps
      pri_src[0] = load_unaligned_u16_4x2(in + po1, CDEF_BSTRIDE);
      pri_src[1] = load_unaligned_u16_4x2(in - po1, CDEF_BSTRIDE);

      // Primary far taps
      pri_src[2] = load_unaligned_u16_4x2(in + po2, CDEF_BSTRIDE);
      pri_src[3] = load_unaligned_u16_4x2(in - po2, CDEF_BSTRIDE);

      primary_filter(s, pri_src, pri_taps, pri_strength, pri_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      store_u16x4_strided_x2(dst16, dstride, vreinterpretq_u16_s16(res));

      in += 2 * CDEF_BSTRIDE;
      dst16 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_2_neon(void *dest, int dstride, const uint16_t *in,
                           int pri_strength, int sec_strength, int dir,
                           int pri_damping, int sec_damping, int coeff_shift,
                           int block_width, int block_height) {
  (void)pri_strength;
  (void)pri_damping;
  (void)coeff_shift;

  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  const int *sec_taps = cdef_sec_taps;

  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = vld1q_u16(in);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = vld1q_u16(in + s1o1);
      sec_src[1] = vld1q_u16(in - s1o1);
      sec_src[2] = vld1q_u16(in + s2o1);
      sec_src[3] = vld1q_u16(in - s2o1);

      // Secondary far taps
      sec_src[4] = vld1q_u16(in + s1o2);
      sec_src[5] = vld1q_u16(in - s1o2);
      sec_src[6] = vld1q_u16(in + s2o2);
      sec_src[7] = vld1q_u16(in - s2o2);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      vst1q_u16(dst16, vreinterpretq_u16_s16(res));

      in += CDEF_BSTRIDE;
      dst16 += dstride;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      int16x8_t sum = vdupq_n_s16(0);
      uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);

      uint16x8_t sec_src[8];

      // Secondary near taps
      sec_src[0] = load_unaligned_u16_4x2(in + s1o1, CDEF_BSTRIDE);
      sec_src[1] = load_unaligned_u16_4x2(in - s1o1, CDEF_BSTRIDE);
      sec_src[2] = load_unaligned_u16_4x2(in + s2o1, CDEF_BSTRIDE);
      sec_src[3] = load_unaligned_u16_4x2(in - s2o1, CDEF_BSTRIDE);

      // Secondary far taps
      sec_src[4] = load_unaligned_u16_4x2(in + s1o2, CDEF_BSTRIDE);
      sec_src[5] = load_unaligned_u16_4x2(in - s1o2, CDEF_BSTRIDE);
      sec_src[6] = load_unaligned_u16_4x2(in + s2o2, CDEF_BSTRIDE);
      sec_src[7] = load_unaligned_u16_4x2(in - s2o2, CDEF_BSTRIDE);

      secondary_filter(s, sec_src, sec_taps, sec_strength, sec_damping, &sum);

      // res = s + ((sum - (sum < 0) + 8) >> 4)
      sum =
          vaddq_s16(sum, vreinterpretq_s16_u16(vcltq_s16(sum, vdupq_n_s16(0))));
      const int16x8_t res = vrsraq_n_s16(vreinterpretq_s16_u16(s), sum, 4);

      store_u16x4_strided_x2(dst16, dstride, vreinterpretq_u16_s16(res));

      in += 2 * CDEF_BSTRIDE;
      dst16 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_3_neon(void *dest, int dstride, const uint16_t *in,
                           int pri_strength, int sec_strength, int dir,
                           int pri_damping, int sec_damping, int coeff_shift,
                           int block_width, int block_height) {
  (void)pri_strength;
  (void)sec_strength;
  (void)dir;
  (void)pri_damping;
  (void)sec_damping;
  (void)coeff_shift;
  (void)block_width;
  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      const uint16x8_t s = vld1q_u16(in);
      vst1q_u16(dst16, s);

      in += CDEF_BSTRIDE;
      dst16 += dstride;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    do {
      const uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);
      store_u16x4_strided_x2(dst16, dstride, s);

      in += 2 * CDEF_BSTRIDE;
      dst16 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}
