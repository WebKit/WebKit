/*
 * Copyright (c) 2014 The WebM project authors. All Rights Reserved.
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
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/aom_convolve8_neon.h"
#include "aom_dsp/arm/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

// Filter values always sum to 128.
#define FILTER_WEIGHT 128

DECLARE_ALIGNED(16, static const uint8_t, kDotProdPermuteTbl[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

DECLARE_ALIGNED(16, static const uint8_t, kDotProdMergeBlockTbl[48]) = {
  // Shift left and insert new last column in transposed 4x4 block.
  1, 2, 3, 16, 5, 6, 7, 20, 9, 10, 11, 24, 13, 14, 15, 28,
  // Shift left and insert two new columns in transposed 4x4 block.
  2, 3, 16, 17, 6, 7, 20, 21, 10, 11, 24, 25, 14, 15, 28, 29,
  // Shift left and insert three new columns in transposed 4x4 block.
  3, 16, 17, 18, 7, 20, 21, 22, 11, 24, 25, 26, 15, 28, 29, 30
};

static INLINE int16x4_t convolve8_4_h(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16x2_t permute_tbl) {
  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t samples_128 =
      vreinterpretq_s8_u8(vsubq_u8(samples, vdupq_n_u8(128)));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  int8x16_t perm_samples[2] = { vqtbl1q_s8(samples_128, permute_tbl.val[0]),
                                vqtbl1q_s8(samples_128, permute_tbl.val[1]) };

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT);
  int32x4_t sum = vdotq_lane_s32(acc, perm_samples[0], filters, 0);
  sum = vdotq_lane_s32(sum, perm_samples[1], filters, 1);

  // Further narrowing and packing is performed by the caller.
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve8_8_h(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16x3_t permute_tbl) {
  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t samples_128 =
      vreinterpretq_s8_u8(vsubq_u8(samples, vdupq_n_u8(128)));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  int8x16_t perm_samples[3] = { vqtbl1q_s8(samples_128, permute_tbl.val[0]),
                                vqtbl1q_s8(samples_128, permute_tbl.val[1]),
                                vqtbl1q_s8(samples_128, permute_tbl.val[2]) };

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT);
  // First 4 output values.
  int32x4_t sum0 = vdotq_lane_s32(acc, perm_samples[0], filters, 0);
  sum0 = vdotq_lane_s32(sum0, perm_samples[1], filters, 1);
  // Second 4 output values.
  int32x4_t sum1 = vdotq_lane_s32(acc, perm_samples[1], filters, 0);
  sum1 = vdotq_lane_s32(sum1, perm_samples[2], filters, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve8_horiz_8tap_neon_dotprod(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int w, int h) {
  const int8x8_t filter = vmovn_s16(vld1q_s16(filter_x));

  if (w == 4) {
    const uint8x16x2_t perm_tbl = vld1q_u8_x2(kDotProdPermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 = convolve8_4_h(s0, filter, perm_tbl);
      int16x4_t d1 = convolve8_4_h(s1, filter, perm_tbl);
      int16x4_t d2 = convolve8_4_h(s2, filter, perm_tbl);
      int16x4_t d3 = convolve8_4_h(s3, filter, perm_tbl);
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x3_t perm_tbl = vld1q_u8_x3(kDotProdPermuteTbl);

    do {
      int width = w;
      const uint8_t *s = src;
      uint8_t *d = dst;
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve8_8_h(s0, filter, perm_tbl);
        uint8x8_t d1 = convolve8_8_h(s1, filter, perm_tbl);
        uint8x8_t d2 = convolve8_8_h(s2, filter, perm_tbl);
        uint8x8_t d3 = convolve8_8_h(s3, filter, perm_tbl);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

static INLINE int16x4_t convolve4_4_h(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16_t permute_tbl) {
  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t samples_128 =
      vreinterpretq_s8_u8(vsubq_u8(samples, vdupq_n_u8(128)));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  int8x16_t perm_samples = vqtbl1q_s8(samples_128, permute_tbl);

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  // (Divide by 2 since we halved the filter values.)
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT / 2);
  int32x4_t sum = vdotq_lane_s32(acc, perm_samples, filters, 0);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static INLINE uint8x8_t convolve4_8_h(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16x2_t permute_tbl) {
  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t samples_128 =
      vreinterpretq_s8_u8(vsubq_u8(samples, vdupq_n_u8(128)));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  int8x16_t perm_samples[2] = { vqtbl1q_s8(samples_128, permute_tbl.val[0]),
                                vqtbl1q_s8(samples_128, permute_tbl.val[1]) };

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  // (Divide by 2 since we halved the filter values.)
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT / 2);
  // First 4 output values.
  int32x4_t sum0 = vdotq_lane_s32(acc, perm_samples[0], filters, 0);
  // Second 4 output values.
  int32x4_t sum1 = vdotq_lane_s32(acc, perm_samples[1], filters, 0);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve8_horiz_4tap_neon_dotprod(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int width, int height) {
  const int16x4_t x_filter = vld1_s16(filter_x + 2);
  // All 4-tap and bilinear filter values are even, so halve them to reduce
  // intermediate precision requirements.
  const int8x8_t filter = vshrn_n_s16(vcombine_s16(x_filter, vdup_n_s16(0)), 1);

  if (width == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kDotProdPermuteTbl);

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t t0 = convolve4_4_h(s0, filter, permute_tbl);
      int16x4_t t1 = convolve4_4_h(s1, filter, permute_tbl);
      int16x4_t t2 = convolve4_4_h(s2, filter, permute_tbl);
      int16x4_t t3 = convolve4_4_h(s3, filter, permute_tbl);
      // We halved the filter values so -1 from right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(t0, t1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(t2, t3), FILTER_BITS - 1);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(kDotProdPermuteTbl);

    do {
      const uint8_t *s = src;
      uint8_t *d = dst;
      int w = width;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve4_8_h(s0, filter, permute_tbl);
        uint8x8_t d1 = convolve4_8_h(s1, filter, permute_tbl);
        uint8x8_t d2 = convolve4_8_h(s2, filter, permute_tbl);
        uint8x8_t d3 = convolve4_8_h(s3, filter, permute_tbl);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

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

void aom_convolve8_horiz_neon_dotprod(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h) {
  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  (void)x_step_q4;
  (void)filter_y;
  (void)y_step_q4;

  src -= ((SUBPEL_TAPS / 2) - 1);

  int filter_taps = get_filter_taps_convolve8(filter_x);

  if (filter_taps == 2) {
    convolve8_horiz_2tap_neon(src + 3, src_stride, dst, dst_stride, filter_x, w,
                              h);
  } else if (filter_taps == 4) {
    convolve8_horiz_4tap_neon_dotprod(src + 2, src_stride, dst, dst_stride,
                                      filter_x, w, h);
  } else {
    convolve8_horiz_8tap_neon_dotprod(src, src_stride, dst, dst_stride,
                                      filter_x, w, h);
  }
}

static INLINE void transpose_concat_4x4(int8x8_t a0, int8x8_t a1, int8x8_t a2,
                                        int8x8_t a3, int8x16_t *b) {
  // Transpose 8-bit elements and concatenate result rows as follows:
  // a0: 00, 01, 02, 03, XX, XX, XX, XX
  // a1: 10, 11, 12, 13, XX, XX, XX, XX
  // a2: 20, 21, 22, 23, XX, XX, XX, XX
  // a3: 30, 31, 32, 33, XX, XX, XX, XX
  //
  // b: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33

  int8x16_t a0q = vcombine_s8(a0, vdup_n_s8(0));
  int8x16_t a1q = vcombine_s8(a1, vdup_n_s8(0));
  int8x16_t a2q = vcombine_s8(a2, vdup_n_s8(0));
  int8x16_t a3q = vcombine_s8(a3, vdup_n_s8(0));

  int8x16_t a01 = vzipq_s8(a0q, a1q).val[0];
  int8x16_t a23 = vzipq_s8(a2q, a3q).val[0];

  int16x8_t a0123 =
      vzipq_s16(vreinterpretq_s16_s8(a01), vreinterpretq_s16_s8(a23)).val[0];

  *b = vreinterpretq_s8_s16(a0123);
}

static INLINE void transpose_concat_8x4(int8x8_t a0, int8x8_t a1, int8x8_t a2,
                                        int8x8_t a3, int8x16_t *b0,
                                        int8x16_t *b1) {
  // Transpose 8-bit elements and concatenate result rows as follows:
  // a0: 00, 01, 02, 03, 04, 05, 06, 07
  // a1: 10, 11, 12, 13, 14, 15, 16, 17
  // a2: 20, 21, 22, 23, 24, 25, 26, 27
  // a3: 30, 31, 32, 33, 34, 35, 36, 37
  //
  // b0: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
  // b1: 04, 14, 24, 34, 05, 15, 25, 35, 06, 16, 26, 36, 07, 17, 27, 37

  int8x16_t a0q = vcombine_s8(a0, vdup_n_s8(0));
  int8x16_t a1q = vcombine_s8(a1, vdup_n_s8(0));
  int8x16_t a2q = vcombine_s8(a2, vdup_n_s8(0));
  int8x16_t a3q = vcombine_s8(a3, vdup_n_s8(0));

  int8x16_t a01 = vzipq_s8(a0q, a1q).val[0];
  int8x16_t a23 = vzipq_s8(a2q, a3q).val[0];

  int16x8x2_t a0123 =
      vzipq_s16(vreinterpretq_s16_s8(a01), vreinterpretq_s16_s8(a23));

  *b0 = vreinterpretq_s8_s16(a0123.val[0]);
  *b1 = vreinterpretq_s8_s16(a0123.val[1]);
}

static INLINE int16x4_t convolve8_4_v(const int8x16_t samples_lo,
                                      const int8x16_t samples_hi,
                                      const int8x8_t filters) {
  // The sample range transform and permutation are performed by the caller.

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT);
  int32x4_t sum = vdotq_lane_s32(acc, samples_lo, filters, 0);
  sum = vdotq_lane_s32(sum, samples_hi, filters, 1);

  // Further narrowing and packing is performed by the caller.
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve8_8_v(const int8x16_t samples0_lo,
                                      const int8x16_t samples0_hi,
                                      const int8x16_t samples1_lo,
                                      const int8x16_t samples1_hi,
                                      const int8x8_t filters) {
  // The sample range transform and permutation are performed by the caller.

  // Accumulate into 128 * FILTER_WEIGHT to account for range transform.
  int32x4_t acc = vdupq_n_s32(128 * FILTER_WEIGHT);
  // First 4 output values.
  int32x4_t sum0 = vdotq_lane_s32(acc, samples0_lo, filters, 0);
  sum0 = vdotq_lane_s32(sum0, samples0_hi, filters, 1);
  // Second 4 output values.
  int32x4_t sum1 = vdotq_lane_s32(acc, samples1_lo, filters, 0);
  sum1 = vdotq_lane_s32(sum1, samples1_hi, filters, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve8_vert_8tap_neon_dotprod(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_y, int w, int h) {
  const int8x8_t filter = vmovn_s16(vld1q_s16(filter_y));
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  int8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6;
    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    src += 7 * src_stride;

    // Clamp sample range to [-128, 127] for 8-bit signed dot product.
    int8x8_t s0 = vreinterpret_s8_u8(vsub_u8(t0, vdup_n_u8(128)));
    int8x8_t s1 = vreinterpret_s8_u8(vsub_u8(t1, vdup_n_u8(128)));
    int8x8_t s2 = vreinterpret_s8_u8(vsub_u8(t2, vdup_n_u8(128)));
    int8x8_t s3 = vreinterpret_s8_u8(vsub_u8(t3, vdup_n_u8(128)));
    int8x8_t s4 = vreinterpret_s8_u8(vsub_u8(t4, vdup_n_u8(128)));
    int8x8_t s5 = vreinterpret_s8_u8(vsub_u8(t5, vdup_n_u8(128)));
    int8x8_t s6 = vreinterpret_s8_u8(vsub_u8(t6, vdup_n_u8(128)));

    // This operation combines a conventional transpose and the sample permute
    // (see horizontal case) required before computing the dot product.
    int8x16_t s0123, s1234, s2345, s3456;
    transpose_concat_4x4(s0, s1, s2, s3, &s0123);
    transpose_concat_4x4(s1, s2, s3, s4, &s1234);
    transpose_concat_4x4(s2, s3, s4, s5, &s2345);
    transpose_concat_4x4(s3, s4, s5, s6, &s3456);

    do {
      uint8x8_t t7, t8, t9, t10;
      load_u8_8x4(src, src_stride, &t7, &t8, &t9, &t10);

      int8x8_t s7 = vreinterpret_s8_u8(vsub_u8(t7, vdup_n_u8(128)));
      int8x8_t s8 = vreinterpret_s8_u8(vsub_u8(t8, vdup_n_u8(128)));
      int8x8_t s9 = vreinterpret_s8_u8(vsub_u8(t9, vdup_n_u8(128)));
      int8x8_t s10 = vreinterpret_s8_u8(vsub_u8(t10, vdup_n_u8(128)));

      int8x16_t s4567, s5678, s6789, s78910;
      transpose_concat_4x4(s7, s8, s9, s10, &s78910);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve8_4_v(s0123, s4567, filter);
      int16x4_t d1 = convolve8_4_v(s1234, s5678, filter);
      int16x4_t d2 = convolve8_4_v(s2345, s6789, filter);
      int16x4_t d3 = convolve8_4_v(s3456, s78910, filter);
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s78910;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src;
      uint8_t *d = dst;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s += 7 * src_stride;

      // Clamp sample range to [-128, 127] for 8-bit signed dot product.
      int8x8_t s0 = vreinterpret_s8_u8(vsub_u8(t0, vdup_n_u8(128)));
      int8x8_t s1 = vreinterpret_s8_u8(vsub_u8(t1, vdup_n_u8(128)));
      int8x8_t s2 = vreinterpret_s8_u8(vsub_u8(t2, vdup_n_u8(128)));
      int8x8_t s3 = vreinterpret_s8_u8(vsub_u8(t3, vdup_n_u8(128)));
      int8x8_t s4 = vreinterpret_s8_u8(vsub_u8(t4, vdup_n_u8(128)));
      int8x8_t s5 = vreinterpret_s8_u8(vsub_u8(t5, vdup_n_u8(128)));
      int8x8_t s6 = vreinterpret_s8_u8(vsub_u8(t6, vdup_n_u8(128)));

      // This operation combines a conventional transpose and the sample permute
      // (see horizontal case) required before computing the dot product.
      int8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
          s3456_lo, s3456_hi;
      transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
      transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
      transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
      transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

      do {
        uint8x8_t t7, t8, t9, t10;
        load_u8_8x4(s, src_stride, &t7, &t8, &t9, &t10);

        int8x8_t s7 = vreinterpret_s8_u8(vsub_u8(t7, vdup_n_u8(128)));
        int8x8_t s8 = vreinterpret_s8_u8(vsub_u8(t8, vdup_n_u8(128)));
        int8x8_t s9 = vreinterpret_s8_u8(vsub_u8(t9, vdup_n_u8(128)));
        int8x8_t s10 = vreinterpret_s8_u8(vsub_u8(t10, vdup_n_u8(128)));

        int8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
            s78910_lo, s78910_hi;
        transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi);

        // Merge new data into block from previous iteration.
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        uint8x8_t d0 =
            convolve8_8_v(s0123_lo, s4567_lo, s0123_hi, s4567_hi, filter);
        uint8x8_t d1 =
            convolve8_8_v(s1234_lo, s5678_lo, s1234_hi, s5678_hi, filter);
        uint8x8_t d2 =
            convolve8_8_v(s2345_lo, s6789_lo, s2345_hi, s6789_hi, filter);
        uint8x8_t d3 =
            convolve8_8_v(s3456_lo, s78910_lo, s3456_hi, s78910_hi, filter);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;
        s1234_lo = s5678_lo;
        s1234_hi = s5678_hi;
        s2345_lo = s6789_lo;
        s2345_hi = s6789_hi;
        s3456_lo = s78910_lo;
        s3456_hi = s78910_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

void aom_convolve8_vert_neon_dotprod(const uint8_t *src, ptrdiff_t src_stride,
                                     uint8_t *dst, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y, int y_step_q4,
                                     int w, int h) {
  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  (void)filter_x;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= ((SUBPEL_TAPS / 2) - 1) * src_stride;

  int filter_taps = get_filter_taps_convolve8(filter_y);

  if (filter_taps == 2) {
    convolve8_vert_2tap_neon(src + 3 * src_stride, src_stride, dst, dst_stride,
                             filter_y, w, h);
  } else if (filter_taps == 4) {
    convolve8_vert_4tap_neon(src + 2 * src_stride, src_stride, dst, dst_stride,
                             filter_y, w, h);
  } else {
    convolve8_vert_8tap_neon_dotprod(src, src_stride, dst, dst_stride, filter_y,
                                     w, h);
  }
}
