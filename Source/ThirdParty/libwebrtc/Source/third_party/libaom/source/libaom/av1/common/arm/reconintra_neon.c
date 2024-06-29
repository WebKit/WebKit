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
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

#define MAX_UPSAMPLE_SZ 16

// These kernels are a transposed version of those defined in reconintra.c,
// with the absolute value of the negatives taken in the top row.
DECLARE_ALIGNED(16, const uint8_t,
                av1_filter_intra_taps_neon[FILTER_INTRA_MODES][7][8]) = {
  // clang-format off
  {
      {  6,  5,  3,  3,  4,  3,  3,  3 },
      { 10,  2,  1,  1,  6,  2,  2,  1 },
      {  0, 10,  1,  1,  0,  6,  2,  2 },
      {  0,  0, 10,  2,  0,  0,  6,  2 },
      {  0,  0,  0, 10,  0,  0,  0,  6 },
      { 12,  9,  7,  5,  2,  2,  2,  3 },
      {  0,  0,  0,  0, 12,  9,  7,  5 }
  },
  {
      { 10,  6,  4,  2, 10,  6,  4,  2 },
      { 16,  0,  0,  0, 16,  0,  0,  0 },
      {  0, 16,  0,  0,  0, 16,  0,  0 },
      {  0,  0, 16,  0,  0,  0, 16,  0 },
      {  0,  0,  0, 16,  0,  0,  0, 16 },
      { 10,  6,  4,  2,  0,  0,  0,  0 },
      {  0,  0,  0,  0, 10,  6,  4,  2 }
  },
  {
      {  8,  8,  8,  8,  4,  4,  4,  4 },
      {  8,  0,  0,  0,  4,  0,  0,  0 },
      {  0,  8,  0,  0,  0,  4,  0,  0 },
      {  0,  0,  8,  0,  0,  0,  4,  0 },
      {  0,  0,  0,  8,  0,  0,  0,  4 },
      { 16, 16, 16, 16,  0,  0,  0,  0 },
      {  0,  0,  0,  0, 16, 16, 16, 16 }
  },
  {
      {  2,  1,  1,  0,  1,  1,  1,  1 },
      {  8,  3,  2,  1,  4,  3,  2,  2 },
      {  0,  8,  3,  2,  0,  4,  3,  2 },
      {  0,  0,  8,  3,  0,  0,  4,  3 },
      {  0,  0,  0,  8,  0,  0,  0,  4 },
      { 10,  6,  4,  2,  3,  4,  4,  3 },
      {  0,  0,  0,  0, 10,  6,  4,  3 }
  },
  {
      { 12, 10,  9,  8, 10,  9,  8,  7 },
      { 14,  0,  0,  0, 12,  1,  0,  0 },
      {  0, 14,  0,  0,  0, 12,  0,  0 },
      {  0,  0, 14,  0,  0,  0, 12,  1 },
      {  0,  0,  0, 14,  0,  0,  0, 12 },
      { 14, 12, 11, 10,  0,  0,  1,  1 },
      {  0,  0,  0,  0, 14, 12, 11,  9 }
  }
  // clang-format on
};

#define FILTER_INTRA_SCALE_BITS 4

void av1_filter_intra_predictor_neon(uint8_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint8_t *above,
                                     const uint8_t *left, int mode) {
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  assert(width <= 32 && height <= 32);

  const uint8x8_t f0 = vld1_u8(av1_filter_intra_taps_neon[mode][0]);
  const uint8x8_t f1 = vld1_u8(av1_filter_intra_taps_neon[mode][1]);
  const uint8x8_t f2 = vld1_u8(av1_filter_intra_taps_neon[mode][2]);
  const uint8x8_t f3 = vld1_u8(av1_filter_intra_taps_neon[mode][3]);
  const uint8x8_t f4 = vld1_u8(av1_filter_intra_taps_neon[mode][4]);
  const uint8x8_t f5 = vld1_u8(av1_filter_intra_taps_neon[mode][5]);
  const uint8x8_t f6 = vld1_u8(av1_filter_intra_taps_neon[mode][6]);

  uint8_t buffer[33][33];
  // Populate the top row in the scratch buffer with data from above.
  memcpy(buffer[0], &above[-1], (width + 1) * sizeof(uint8_t));
  // Populate the first column in the scratch buffer with data from the left.
  int r = 0;
  do {
    buffer[r + 1][0] = left[r];
  } while (++r < height);

  // Computing 4 cols per iteration (instead of 8) for 8x<h> blocks is faster.
  if (width <= 8) {
    r = 1;
    do {
      int c = 1;
      uint8x8_t s0 = vld1_dup_u8(&buffer[r - 1][c - 1]);
      uint8x8_t s5 = vld1_dup_u8(&buffer[r + 0][c - 1]);
      uint8x8_t s6 = vld1_dup_u8(&buffer[r + 1][c - 1]);

      do {
        uint8x8_t s1234 = load_u8_4x1(&buffer[r - 1][c - 1] + 1);
        uint8x8_t s1 = vdup_lane_u8(s1234, 0);
        uint8x8_t s2 = vdup_lane_u8(s1234, 1);
        uint8x8_t s3 = vdup_lane_u8(s1234, 2);
        uint8x8_t s4 = vdup_lane_u8(s1234, 3);

        uint16x8_t sum = vmull_u8(s1, f1);
        // First row of each filter has all negative values so subtract.
        sum = vmlsl_u8(sum, s0, f0);
        sum = vmlal_u8(sum, s2, f2);
        sum = vmlal_u8(sum, s3, f3);
        sum = vmlal_u8(sum, s4, f4);
        sum = vmlal_u8(sum, s5, f5);
        sum = vmlal_u8(sum, s6, f6);

        uint8x8_t res =
            vqrshrun_n_s16(vreinterpretq_s16_u16(sum), FILTER_INTRA_SCALE_BITS);

        // Store buffer[r + 0][c] and buffer[r + 1][c].
        store_u8x4_strided_x2(&buffer[r][c], 33, res);

        store_u8x4_strided_x2(dst + (r - 1) * stride + c - 1, stride, res);

        s0 = s4;
        s5 = vdup_lane_u8(res, 3);
        s6 = vdup_lane_u8(res, 7);
        c += 4;
      } while (c < width + 1);

      r += 2;
    } while (r < height + 1);
  } else {
    r = 1;
    do {
      int c = 1;
      uint8x8_t s0_lo = vld1_dup_u8(&buffer[r - 1][c - 1]);
      uint8x8_t s5_lo = vld1_dup_u8(&buffer[r + 0][c - 1]);
      uint8x8_t s6_lo = vld1_dup_u8(&buffer[r + 1][c - 1]);

      do {
        uint8x8_t s1234 = vld1_u8(&buffer[r - 1][c - 1] + 1);
        uint8x8_t s1_lo = vdup_lane_u8(s1234, 0);
        uint8x8_t s2_lo = vdup_lane_u8(s1234, 1);
        uint8x8_t s3_lo = vdup_lane_u8(s1234, 2);
        uint8x8_t s4_lo = vdup_lane_u8(s1234, 3);

        uint16x8_t sum_lo = vmull_u8(s1_lo, f1);
        // First row of each filter has all negative values so subtract.
        sum_lo = vmlsl_u8(sum_lo, s0_lo, f0);
        sum_lo = vmlal_u8(sum_lo, s2_lo, f2);
        sum_lo = vmlal_u8(sum_lo, s3_lo, f3);
        sum_lo = vmlal_u8(sum_lo, s4_lo, f4);
        sum_lo = vmlal_u8(sum_lo, s5_lo, f5);
        sum_lo = vmlal_u8(sum_lo, s6_lo, f6);

        uint8x8_t res_lo = vqrshrun_n_s16(vreinterpretq_s16_u16(sum_lo),
                                          FILTER_INTRA_SCALE_BITS);

        uint8x8_t s0_hi = s4_lo;
        uint8x8_t s1_hi = vdup_lane_u8(s1234, 4);
        uint8x8_t s2_hi = vdup_lane_u8(s1234, 5);
        uint8x8_t s3_hi = vdup_lane_u8(s1234, 6);
        uint8x8_t s4_hi = vdup_lane_u8(s1234, 7);
        uint8x8_t s5_hi = vdup_lane_u8(res_lo, 3);
        uint8x8_t s6_hi = vdup_lane_u8(res_lo, 7);

        uint16x8_t sum_hi = vmull_u8(s1_hi, f1);
        // First row of each filter has all negative values so subtract.
        sum_hi = vmlsl_u8(sum_hi, s0_hi, f0);
        sum_hi = vmlal_u8(sum_hi, s2_hi, f2);
        sum_hi = vmlal_u8(sum_hi, s3_hi, f3);
        sum_hi = vmlal_u8(sum_hi, s4_hi, f4);
        sum_hi = vmlal_u8(sum_hi, s5_hi, f5);
        sum_hi = vmlal_u8(sum_hi, s6_hi, f6);

        uint8x8_t res_hi = vqrshrun_n_s16(vreinterpretq_s16_u16(sum_hi),
                                          FILTER_INTRA_SCALE_BITS);

        uint32x2x2_t res =
            vzip_u32(vreinterpret_u32_u8(res_lo), vreinterpret_u32_u8(res_hi));

        vst1_u8(&buffer[r + 0][c], vreinterpret_u8_u32(res.val[0]));
        vst1_u8(&buffer[r + 1][c], vreinterpret_u8_u32(res.val[1]));

        vst1_u8(dst + (r - 1) * stride + c - 1,
                vreinterpret_u8_u32(res.val[0]));
        vst1_u8(dst + (r + 0) * stride + c - 1,
                vreinterpret_u8_u32(res.val[1]));

        s0_lo = s4_hi;
        s5_lo = vdup_lane_u8(res_hi, 3);
        s6_lo = vdup_lane_u8(res_hi, 7);
        c += 8;
      } while (c < width + 1);

      r += 2;
    } while (r < height + 1);
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
