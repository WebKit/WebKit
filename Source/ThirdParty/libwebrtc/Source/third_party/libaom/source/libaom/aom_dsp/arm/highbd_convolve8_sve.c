/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
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

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/aom_filter.h"
#include "aom_dsp/arm/highbd_convolve8_neon.h"
#include "aom_dsp/arm/mem_neon.h"

static inline uint16x4_t highbd_convolve8_4_h(int16x8_t s[4], int16x8_t filter,
                                              uint16x4_t max) {
  int64x2_t sum[4];

  sum[0] = aom_sdotq_s16(vdupq_n_s64(0), s[0], filter);
  sum[1] = aom_sdotq_s16(vdupq_n_s64(0), s[1], filter);
  sum[2] = aom_sdotq_s16(vdupq_n_s64(0), s[2], filter);
  sum[3] = aom_sdotq_s16(vdupq_n_s64(0), s[3], filter);

  int64x2_t sum01 = vpaddq_s64(sum[0], sum[1]);
  int64x2_t sum23 = vpaddq_s64(sum[2], sum[3]);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));

  uint16x4_t res = vqrshrun_n_s32(sum0123, FILTER_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_h(int16x8_t s[8], int16x8_t filter,
                                              uint16x8_t max) {
  int64x2_t sum[8];

  sum[0] = aom_sdotq_s16(vdupq_n_s64(0), s[0], filter);
  sum[1] = aom_sdotq_s16(vdupq_n_s64(0), s[1], filter);
  sum[2] = aom_sdotq_s16(vdupq_n_s64(0), s[2], filter);
  sum[3] = aom_sdotq_s16(vdupq_n_s64(0), s[3], filter);
  sum[4] = aom_sdotq_s16(vdupq_n_s64(0), s[4], filter);
  sum[5] = aom_sdotq_s16(vdupq_n_s64(0), s[5], filter);
  sum[6] = aom_sdotq_s16(vdupq_n_s64(0), s[6], filter);
  sum[7] = aom_sdotq_s16(vdupq_n_s64(0), s[7], filter);

  int64x2_t sum01 = vpaddq_s64(sum[0], sum[1]);
  int64x2_t sum23 = vpaddq_s64(sum[2], sum[3]);
  int64x2_t sum45 = vpaddq_s64(sum[4], sum[5]);
  int64x2_t sum67 = vpaddq_s64(sum[6], sum[7]);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));
  return vminq_u16(res, max);
}

static inline void highbd_convolve8_horiz_8tap_sve(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int width, int height,
    int bd) {
  const int16x8_t filter = vld1q_s16(filter_x);

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x8_t s0[4], s1[4], s2[4], s3[4];
      load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve8_4_h(s0, filter, max);
      uint16x4_t d1 = highbd_convolve8_4_h(s1, filter, max);
      uint16x4_t d2 = highbd_convolve8_4_h(s2, filter, max);
      uint16x4_t d3 = highbd_convolve8_4_h(s3, filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    do {
      const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8_h(s0, filter, max);
        uint16x8_t d1 = highbd_convolve8_8_h(s1, filter, max);
        uint16x8_t d2 = highbd_convolve8_8_h(s2, filter, max);
        uint16x8_t d3 = highbd_convolve8_8_h(s3, filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

// clang-format off
DECLARE_ALIGNED(16, static const uint16_t, kDotProdTbl[16]) = {
  0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6,
};

DECLARE_ALIGNED(16, static const uint16_t, kDeinterleaveTbl[8]) = {
  0, 2, 4, 6, 1, 3, 5, 7,
};
// clang-format on

static inline uint16x4_t highbd_convolve4_4_h(int16x8_t s, int16x8_t filter,
                                              uint16x8x2_t permute_tbl,
                                              uint16x4_t max) {
  int16x8_t permuted_samples0 = aom_tbl_s16(s, permute_tbl.val[0]);
  int16x8_t permuted_samples1 = aom_tbl_s16(s, permute_tbl.val[1]);

  int64x2_t sum0 =
      aom_svdot_lane_s16(vdupq_n_s64(0), permuted_samples0, filter, 0);
  int64x2_t sum1 =
      aom_svdot_lane_s16(vdupq_n_s64(0), permuted_samples1, filter, 0);

  int32x4_t res_s32 = vcombine_s32(vmovn_s64(sum0), vmovn_s64(sum1));
  uint16x4_t res = vqrshrun_n_s32(res_s32, FILTER_BITS);

  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve4_8_h(int16x8_t s[4], int16x8_t filter,
                                              uint16x8_t idx, uint16x8_t max) {
  int64x2_t sum04 = aom_svdot_lane_s16(vdupq_n_s64(0), s[0], filter, 0);
  int64x2_t sum15 = aom_svdot_lane_s16(vdupq_n_s64(0), s[1], filter, 0);
  int64x2_t sum26 = aom_svdot_lane_s16(vdupq_n_s64(0), s[2], filter, 0);
  int64x2_t sum37 = aom_svdot_lane_s16(vdupq_n_s64(0), s[3], filter, 0);

  int32x4_t res0 = vcombine_s32(vmovn_s64(sum04), vmovn_s64(sum15));
  int32x4_t res1 = vcombine_s32(vmovn_s64(sum26), vmovn_s64(sum37));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(res0, FILTER_BITS),
                                vqrshrun_n_s32(res1, FILTER_BITS));

  res = aom_tbl_u16(res, idx);

  return vminq_u16(res, max);
}

static inline void highbd_convolve8_horiz_4tap_sve(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int width, int height,
    int bd) {
  const int16x8_t filter = vcombine_s16(vld1_s16(filter_x + 2), vdup_n_s16(0));

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    uint16x8x2_t permute_tbl = vld1q_u16_x2(kDotProdTbl);

    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x8_t s0, s1, s2, s3;
      load_s16_8x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 = highbd_convolve4_4_h(s0, filter, permute_tbl, max);
      uint16x4_t d1 = highbd_convolve4_4_h(s1, filter, permute_tbl, max);
      uint16x4_t d2 = highbd_convolve4_4_h(s2, filter, permute_tbl, max);
      uint16x4_t d3 = highbd_convolve4_4_h(s3, filter, permute_tbl, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    uint16x8_t idx = vld1q_u16(kDeinterleaveTbl);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0[4], s1[4], s2[4], s3[4];
        load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
        load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
        load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
        load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

        uint16x8_t d0 = highbd_convolve4_8_h(s0, filter, idx, max);
        uint16x8_t d1 = highbd_convolve4_8_h(s1, filter, idx, max);
        uint16x8_t d2 = highbd_convolve4_8_h(s2, filter, idx, max);
        uint16x8_t d3 = highbd_convolve4_8_h(s3, filter, idx, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

void aom_highbd_convolve8_horiz_sve(const uint8_t *src8, ptrdiff_t src_stride,
                                    uint8_t *dst8, ptrdiff_t dst_stride,
                                    const int16_t *filter_x, int x_step_q4,
                                    const int16_t *filter_y, int y_step_q4,
                                    int width, int height, int bd) {
  assert(x_step_q4 == 16);
  assert(width >= 4 && height >= 4);
  (void)filter_y;
  (void)x_step_q4;
  (void)y_step_q4;

  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

  src -= SUBPEL_TAPS / 2 - 1;

  const int filter_taps = get_filter_taps_convolve8(filter_x);

  if (filter_taps == 2) {
    highbd_convolve8_horiz_2tap_neon(src + 3, src_stride, dst, dst_stride,
                                     filter_x, width, height, bd);
  } else if (filter_taps == 4) {
    highbd_convolve8_horiz_4tap_sve(src + 2, src_stride, dst, dst_stride,
                                    filter_x, width, height, bd);
  } else {
    highbd_convolve8_horiz_8tap_sve(src, src_stride, dst, dst_stride, filter_x,
                                    width, height, bd);
  }
}

DECLARE_ALIGNED(16, static const uint8_t, kDotProdMergeBlockTbl[48]) = {
  // Shift left and insert new last column in transposed 4x4 block.
  2, 3, 4, 5, 6, 7, 16, 17, 10, 11, 12, 13, 14, 15, 24, 25,
  // Shift left and insert two new columns in transposed 4x4 block.
  4, 5, 6, 7, 16, 17, 18, 19, 12, 13, 14, 15, 24, 25, 26, 27,
  // Shift left and insert three new columns in transposed 4x4 block.
  6, 7, 16, 17, 18, 19, 20, 21, 14, 15, 24, 25, 26, 27, 28, 29
};

static inline void transpose_concat_4x4(int16x4_t s0, int16x4_t s1,
                                        int16x4_t s2, int16x4_t s3,
                                        int16x8_t res[2]) {
  // Transpose 16-bit elements and concatenate result rows as follows:
  // s0: 00, 01, 02, 03
  // s1: 10, 11, 12, 13
  // s2: 20, 21, 22, 23
  // s3: 30, 31, 32, 33
  //
  // res[0]: 00 10 20 30 01 11 21 31
  // res[1]: 02 12 22 32 03 13 23 33

  int16x8_t s0q = vcombine_s16(s0, vdup_n_s16(0));
  int16x8_t s1q = vcombine_s16(s1, vdup_n_s16(0));
  int16x8_t s2q = vcombine_s16(s2, vdup_n_s16(0));
  int16x8_t s3q = vcombine_s16(s3, vdup_n_s16(0));

  int32x4_t s01 = vreinterpretq_s32_s16(vzip1q_s16(s0q, s1q));
  int32x4_t s23 = vreinterpretq_s32_s16(vzip1q_s16(s2q, s3q));

  int32x4x2_t s0123 = vzipq_s32(s01, s23);

  res[0] = vreinterpretq_s16_s32(s0123.val[0]);
  res[1] = vreinterpretq_s16_s32(s0123.val[1]);
}

static inline void transpose_concat_8x4(int16x8_t s0, int16x8_t s1,
                                        int16x8_t s2, int16x8_t s3,
                                        int16x8_t res[4]) {
  // Transpose 16-bit elements and concatenate result rows as follows:
  // s0: 00, 01, 02, 03, 04, 05, 06, 07
  // s1: 10, 11, 12, 13, 14, 15, 16, 17
  // s2: 20, 21, 22, 23, 24, 25, 26, 27
  // s3: 30, 31, 32, 33, 34, 35, 36, 37
  //
  // res_lo[0]: 00 10 20 30 01 11 21 31
  // res_lo[1]: 02 12 22 32 03 13 23 33
  // res_hi[0]: 04 14 24 34 05 15 25 35
  // res_hi[1]: 06 16 26 36 07 17 27 37

  int16x8x2_t tr01_16 = vzipq_s16(s0, s1);
  int16x8x2_t tr23_16 = vzipq_s16(s2, s3);

  int32x4x2_t tr01_32 = vzipq_s32(vreinterpretq_s32_s16(tr01_16.val[0]),
                                  vreinterpretq_s32_s16(tr23_16.val[0]));
  int32x4x2_t tr23_32 = vzipq_s32(vreinterpretq_s32_s16(tr01_16.val[1]),
                                  vreinterpretq_s32_s16(tr23_16.val[1]));

  res[0] = vreinterpretq_s16_s32(tr01_32.val[0]);
  res[1] = vreinterpretq_s16_s32(tr01_32.val[1]);
  res[2] = vreinterpretq_s16_s32(tr23_32.val[0]);
  res[3] = vreinterpretq_s16_s32(tr23_32.val[1]);
}

static inline void aom_tbl2x4_s16(int16x8_t t0[4], int16x8_t t1[4],
                                  uint8x16_t tbl, int16x8_t res[4]) {
  int8x16x2_t samples0 = { vreinterpretq_s8_s16(t0[0]),
                           vreinterpretq_s8_s16(t1[0]) };
  int8x16x2_t samples1 = { vreinterpretq_s8_s16(t0[1]),
                           vreinterpretq_s8_s16(t1[1]) };
  int8x16x2_t samples2 = { vreinterpretq_s8_s16(t0[2]),
                           vreinterpretq_s8_s16(t1[2]) };
  int8x16x2_t samples3 = { vreinterpretq_s8_s16(t0[3]),
                           vreinterpretq_s8_s16(t1[3]) };

  res[0] = vreinterpretq_s16_s8(vqtbl2q_s8(samples0, tbl));
  res[1] = vreinterpretq_s16_s8(vqtbl2q_s8(samples1, tbl));
  res[2] = vreinterpretq_s16_s8(vqtbl2q_s8(samples2, tbl));
  res[3] = vreinterpretq_s16_s8(vqtbl2q_s8(samples3, tbl));
}

static inline void aom_tbl2x2_s16(int16x8_t t0[2], int16x8_t t1[2],
                                  uint8x16_t tbl, int16x8_t res[2]) {
  int8x16x2_t samples0 = { vreinterpretq_s8_s16(t0[0]),
                           vreinterpretq_s8_s16(t1[0]) };
  int8x16x2_t samples1 = { vreinterpretq_s8_s16(t0[1]),
                           vreinterpretq_s8_s16(t1[1]) };

  res[0] = vreinterpretq_s16_s8(vqtbl2q_s8(samples0, tbl));
  res[1] = vreinterpretq_s16_s8(vqtbl2q_s8(samples1, tbl));
}

static inline uint16x4_t highbd_convolve8_4_v(int16x8_t samples_lo[2],
                                              int16x8_t samples_hi[2],
                                              int16x8_t filter,
                                              uint16x4_t max) {
  int64x2_t sum[2];

  sum[0] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[0], filter, 0);
  sum[0] = aom_svdot_lane_s16(sum[0], samples_hi[0], filter, 1);

  sum[1] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[1], filter, 0);
  sum[1] = aom_svdot_lane_s16(sum[1], samples_hi[1], filter, 1);

  int32x4_t res_s32 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[1]));

  uint16x4_t res = vqrshrun_n_s32(res_s32, FILTER_BITS);

  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_v(int16x8_t samples_lo[4],
                                              int16x8_t samples_hi[4],
                                              int16x8_t filter,
                                              uint16x8_t max) {
  int64x2_t sum[4];

  sum[0] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[0], filter, 0);
  sum[0] = aom_svdot_lane_s16(sum[0], samples_hi[0], filter, 1);

  sum[1] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[1], filter, 0);
  sum[1] = aom_svdot_lane_s16(sum[1], samples_hi[1], filter, 1);

  sum[2] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[2], filter, 0);
  sum[2] = aom_svdot_lane_s16(sum[2], samples_hi[2], filter, 1);

  sum[3] = aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[3], filter, 0);
  sum[3] = aom_svdot_lane_s16(sum[3], samples_hi[3], filter, 1);

  int32x4_t res0 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[1]));
  int32x4_t res1 = vcombine_s32(vmovn_s64(sum[2]), vmovn_s64(sum[3]));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(res0, FILTER_BITS),
                                vqrshrun_n_s32(res1, FILTER_BITS));

  return vminq_u16(res, max);
}

static inline void highbd_convolve8_vert_8tap_sve(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_y, int width, int height,
    int bd) {
  const int16x8_t y_filter = vld1q_s16(filter_y);

  uint8x16_t merge_block_tbl[3];
  merge_block_tbl[0] = vld1q_u8(kDotProdMergeBlockTbl);
  merge_block_tbl[1] = vld1q_u8(kDotProdMergeBlockTbl + 16);
  merge_block_tbl[2] = vld1q_u8(kDotProdMergeBlockTbl + 32);

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    int16_t *s = (int16_t *)src;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      int16x8_t s4567[2], s5678[2], s6789[2], s78910[2];

      // Transpose and shuffle the 4 lines that were loaded.
      transpose_concat_4x4(s7, s8, s9, s10, s78910);

      // Merge new data into block from previous iteration.
      aom_tbl2x2_s16(s3456, s78910, merge_block_tbl[0], s4567);
      aom_tbl2x2_s16(s3456, s78910, merge_block_tbl[1], s5678);
      aom_tbl2x2_s16(s3456, s78910, merge_block_tbl[2], s6789);

      uint16x4_t d0 = highbd_convolve8_4_v(s0123, s4567, y_filter, max);
      uint16x4_t d1 = highbd_convolve8_4_v(s1234, s5678, y_filter, max);
      uint16x4_t d2 = highbd_convolve8_4_v(s2345, s6789, y_filter, max);
      uint16x4_t d3 = highbd_convolve8_4_v(s3456, s78910, y_filter, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s78910[0];
      s3456[1] = s78910[1];

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    do {
      int h = height;
      int16_t *s = (int16_t *)src;
      uint16_t *d = dst;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
      transpose_concat_8x4(s0, s1, s2, s3, s0123);
      transpose_concat_8x4(s1, s2, s3, s4, s1234);
      transpose_concat_8x4(s2, s3, s4, s5, s2345);
      transpose_concat_8x4(s3, s4, s5, s6, s3456);

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        int16x8_t s4567[4], s5678[4], s6789[4], s78910[4];

        // Transpose and shuffle the 4 lines that were loaded.
        transpose_concat_8x4(s7, s8, s9, s10, s78910);

        // Merge new data into block from previous iteration.
        aom_tbl2x4_s16(s3456, s78910, merge_block_tbl[0], s4567);
        aom_tbl2x4_s16(s3456, s78910, merge_block_tbl[1], s5678);
        aom_tbl2x4_s16(s3456, s78910, merge_block_tbl[2], s6789);

        uint16x8_t d0 = highbd_convolve8_8_v(s0123, s4567, y_filter, max);
        uint16x8_t d1 = highbd_convolve8_8_v(s1234, s5678, y_filter, max);
        uint16x8_t d2 = highbd_convolve8_8_v(s2345, s6789, y_filter, max);
        uint16x8_t d3 = highbd_convolve8_8_v(s3456, s78910, y_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123[0] = s4567[0];
        s0123[1] = s4567[1];
        s0123[2] = s4567[2];
        s0123[3] = s4567[3];

        s1234[0] = s5678[0];
        s1234[1] = s5678[1];
        s1234[2] = s5678[2];
        s1234[3] = s5678[3];

        s2345[0] = s6789[0];
        s2345[1] = s6789[1];
        s2345[2] = s6789[2];
        s2345[3] = s6789[3];

        s3456[0] = s78910[0];
        s3456[1] = s78910[1];
        s3456[2] = s78910[2];
        s3456[3] = s78910[3];

        s += 4 * src_stride;
        d += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
      src += 8;
      dst += 8;
      width -= 8;
    } while (width != 0);
  }
}

void aom_highbd_convolve8_vert_sve(const uint8_t *src8, ptrdiff_t src_stride,
                                   uint8_t *dst8, ptrdiff_t dst_stride,
                                   const int16_t *filter_x, int x_step_q4,
                                   const int16_t *filter_y, int y_step_q4,
                                   int width, int height, int bd) {
  assert(y_step_q4 == 16);
  assert(width >= 4 && height >= 4);
  (void)filter_x;
  (void)y_step_q4;
  (void)x_step_q4;

  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride;

  const int filter_taps = get_filter_taps_convolve8(filter_y);

  if (filter_taps == 2) {
    highbd_convolve8_vert_2tap_neon(src + 3 * src_stride, src_stride, dst,
                                    dst_stride, filter_y, width, height, bd);
  } else if (filter_taps == 4) {
    highbd_convolve8_vert_4tap_neon(src + 2 * src_stride, src_stride, dst,
                                    dst_stride, filter_y, width, height, bd);
  } else {
    highbd_convolve8_vert_8tap_sve(src, src_stride, dst, dst_stride, filter_y,
                                   width, height, bd);
  }
}
