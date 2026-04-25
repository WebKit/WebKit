/*
 * Copyright (c) 2014 The WebM project authors. All rights reserved.
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
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

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/aom_convolve8_neon.h"
#include "aom_dsp/arm/aom_filter.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"

DECLARE_ALIGNED(16, static const uint8_t, kMatMul6PermuteTbl[32]) = {
  // clang-format off
  0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9,
  4,  5,  6,  7,  8,  9, 10, 11,  6,  7,  8,  9, 10, 11, 12, 13
  // clang-format on
};

DECLARE_ALIGNED(16, static const uint8_t, kMatMul8PermuteTbl[32]) = {
  // clang-format off
  1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10,
  5,  6,  7,  8,  9, 10, 11, 12,  7,  8,  9, 10, 11, 12, 13, 14
  // clang-format on
};

DECLARE_ALIGNED(16, static const uint8_t, kMatMul8FilterPermuteTbl[16]) = {
  // clang-format off
  1,  2,  3,  4,  5,  6,  7, 16, 16,  1,  2,  3,  4,  5,  6,  7
  // clang-format on
};

DECLARE_ALIGNED(16, static const uint8_t, kDotProdMergeBlockTbl[48]) = {
  // Shift left and insert new last column in transposed 4x4 block.
  1, 2, 3, 16, 5, 6, 7, 20, 9, 10, 11, 24, 13, 14, 15, 28,
  // Shift left and insert two new columns in transposed 4x4 block.
  2, 3, 16, 17, 6, 7, 20, 21, 10, 11, 24, 25, 14, 15, 28, 29,
  // Shift left and insert three new columns in transposed 4x4 block.
  3, 16, 17, 18, 7, 20, 21, 22, 11, 24, 25, 26, 15, 28, 29, 30
};

static inline int16x4_t convolve8_4_h(const uint8x16_t samples,
                                      const int8x16_t filters,
                                      const uint8x16_t permute_tbl) {
  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  uint8x16_t perm_samples = vqtbl1q_u8(samples, permute_tbl);

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum = vusmmlaq_s32(vdupq_n_s32(0), perm_samples, filters);

  // Tap 0, as well as further narrowing and packing, is applied by the caller.
  return vmovn_s32(sum);
}

static inline uint8x8_t convolve8_8_h(const uint8x16_t samples,
                                      const int8x16_t filters,
                                      const uint8x8_t f0,
                                      const uint8x16x2_t permute_tbl) {
  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  // { 5,  6,  7,  8,  9, 10, 11, 12,  7,  8,  9, 10, 11, 12, 13, 14 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                 vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum0123 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[0], filters);
  int32x4_t sum4567 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[1], filters);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));
  // Apply tap 0 and accumulate.
  sum = vreinterpretq_s16_u16(
      vmlsl_u8(vreinterpretq_u16_s16(sum), vget_low_u8(samples), f0));

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void convolve8_horiz_8tap_neon_i8mm(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int w, int h) {
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t filter_s8 = vshrn_n_s16(vld1q_s16(filter_x), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
  const uint8x16_t filter_idx = vld1q_u8(kMatMul8FilterPermuteTbl);
  const int8x16_t filter =
      vqtbl1q_s8(vcombine_s8(filter_s8, vdup_n_s8(0)), filter_idx);

  // Since f0 is always negative and samples are unsigned, subtract (unsigned)
  // s0 * -f0 to avoid signed overflow.
  const uint8x8_t f0 = vdup_n_u8(-filter_x[0] >> 1);

  if (w == 4) {
    const uint8x16_t perm_tbl = vld1q_u8(kMatMul8PermuteTbl);

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);
      uint8x8_t s01 = load_u8_4x2(src + 0 * src_stride, src_stride);
      uint8x8_t s23 = load_u8_4x2(src + 2 * src_stride, src_stride);

      int16x4_t t0 = convolve8_4_h(s0, filter, perm_tbl);
      int16x4_t t1 = convolve8_4_h(s1, filter, perm_tbl);
      int16x4_t t2 = convolve8_4_h(s2, filter, perm_tbl);
      int16x4_t t3 = convolve8_4_h(s3, filter, perm_tbl);
      // Apply tap 0 and accumulate.
      int16x8_t t01 = vcombine_s16(t0, t1);
      int16x8_t t23 = vcombine_s16(t2, t3);
      t01 =
          vreinterpretq_s16_u16(vmlsl_u8(vreinterpretq_u16_s16(t01), s01, f0));
      t23 =
          vreinterpretq_s16_u16(vmlsl_u8(vreinterpretq_u16_s16(t23), s23, f0));
      // We halved the filter values to -1 from right shift.
      uint8x8_t d01 = vqrshrun_n_s16(t01, FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(t23, FILTER_BITS - 1);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul8PermuteTbl);

    do {
      int width = w;
      const uint8_t *s = src;
      uint8_t *d = dst;
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve8_8_h(s0, filter, f0, perm_tbl);
        uint8x8_t d1 = convolve8_8_h(s1, filter, f0, perm_tbl);
        uint8x8_t d2 = convolve8_8_h(s2, filter, f0, perm_tbl);
        uint8x8_t d3 = convolve8_8_h(s3, filter, f0, perm_tbl);

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

static inline int16x4_t convolve6_4_h(const uint8x16_t samples,
                                      const int8x16_t filter,
                                      const uint8x16_t permute_tbl) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  uint8x16_t perm_samples = vqtbl1q_u8(samples, permute_tbl);

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum = vusmmlaq_s32(vdupq_n_s32(0), perm_samples, filter);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static inline uint8x8_t convolve6_8_h(const uint8x16_t samples,
                                      const int8x16_t filter,
                                      const uint8x16x2_t permute_tbl) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  // { 4,  5,  6,  7,  8,  9, 10, 11,  6,  7,  8,  9, 10, 11, 12, 13 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                 vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum0123 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[0], filter);
  int32x4_t sum4567 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[1], filter);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void convolve8_horiz_6tap_neon_i8mm(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_x, int width, int height) {
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(filter_x), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f0, f1, f2, f3, f4, f5,  0,  0,  0, f0, f1, f2, f3, f4, f5,  0 }
  const int8x16_t filter =
      vcombine_s8(vext_s8(x_filter, x_filter, 1), x_filter);

  if (width == 4) {
    const uint8x16_t perm_tbl = vld1q_u8(kMatMul6PermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t t0 = convolve6_4_h(s0, filter, perm_tbl);
      int16x4_t t1 = convolve6_4_h(s1, filter, perm_tbl);
      int16x4_t t2 = convolve6_4_h(s2, filter, perm_tbl);
      int16x4_t t3 = convolve6_4_h(s3, filter, perm_tbl);
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
    const uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);

    do {
      int w = width;
      const uint8_t *s = src;
      uint8_t *d = dst;
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve6_8_h(s0, filter, perm_tbl);
        uint8x8_t d1 = convolve6_8_h(s1, filter, perm_tbl);
        uint8x8_t d2 = convolve6_8_h(s2, filter, perm_tbl);
        uint8x8_t d3 = convolve6_8_h(s3, filter, perm_tbl);

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

void aom_convolve8_horiz_neon_i8mm(const uint8_t *src, ptrdiff_t src_stride,
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
  } else if (filter_taps <= 6) {
    convolve8_horiz_6tap_neon_i8mm(src + 1, src_stride, dst, dst_stride,
                                   filter_x, w, h);
  } else {
    convolve8_horiz_8tap_neon_i8mm(src, src_stride, dst, dst_stride, filter_x,
                                   w, h);
  }
}

static inline int16x4_t convolve8_4_v(const uint8x16_t samples_lo,
                                      const uint8x16_t samples_hi,
                                      const int8x8_t filters) {
  // Sample permutation is performed by the caller.
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), samples_lo, filters, 0);
  sum = vusdotq_lane_s32(sum, samples_hi, filters, 1);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static inline uint8x8_t convolve8_8_v(const uint8x16_t samples0_lo,
                                      const uint8x16_t samples0_hi,
                                      const uint8x16_t samples1_lo,
                                      const uint8x16_t samples1_hi,
                                      const int8x8_t filters) {
  // Sample permutation is performed by the caller.

  // First 4 output values.
  int32x4_t sum0 = vusdotq_lane_s32(vdupq_n_s32(0), samples0_lo, filters, 0);
  sum0 = vusdotq_lane_s32(sum0, samples0_hi, filters, 1);
  // Second 4 output values.
  int32x4_t sum1 = vusdotq_lane_s32(vdupq_n_s32(0), samples1_lo, filters, 0);
  sum1 = vusdotq_lane_s32(sum1, samples1_hi, filters, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void convolve8_vert_8tap_neon_i8mm(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_y, int w, int h) {
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t filter = vshrn_n_s16(vld1q_s16(filter_y), 1);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  uint8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3, s4, s5, s6;
    load_u8_8x7(src, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    uint8x16_t s0123, s1234, s2345, s3456;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);
    transpose_concat_elems_u8_4x4(s1, s2, s3, s4, &s1234);
    transpose_concat_elems_u8_4x4(s2, s3, s4, s5, &s2345);
    transpose_concat_elems_u8_4x4(s3, s4, s5, s6, &s3456);

    do {
      uint8x8_t s7, s8, s9, s10;
      load_u8_8x4(src, src_stride, &s7, &s8, &s9, &s10);

      uint8x16_t s4567, s5678, s6789, s78910;
      transpose_concat_elems_u8_4x4(s7, s8, s9, s10, &s78910);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve8_4_v(s0123, s4567, filter);
      int16x4_t d1 = convolve8_4_v(s1234, s5678, filter);
      int16x4_t d2 = convolve8_4_v(s2345, s6789, filter);
      int16x4_t d3 = convolve8_4_v(s3456, s78910, filter);
      // We halved the filter values so -1 from right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

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

      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
          s3456_lo, s3456_hi;
      transpose_concat_elems_u8_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
      transpose_concat_elems_u8_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
      transpose_concat_elems_u8_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
      transpose_concat_elems_u8_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

      do {
        uint8x8_t s7, s8, s9, s10;
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
            s78910_lo, s78910_hi;
        transpose_concat_elems_u8_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi);

        // Merge new data into block from previous iteration.
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

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

static inline int16x4_t convolve4_4_v(const uint8x16_t samples,
                                      const int8x8_t filters) {
  // Sample permutation is performed by the caller.
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), samples, filters, 0);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static inline uint8x8_t convolve4_8_v(const uint8x16_t samples0,
                                      const uint8x16_t samples1,
                                      const int8x8_t filters) {
  // Sample permutation is performed by the caller.

  // First 4 output values.
  int32x4_t sum0 = vusdotq_lane_s32(vdupq_n_s32(0), samples0, filters, 0);
  // Second 4 output values.
  int32x4_t sum1 = vusdotq_lane_s32(vdupq_n_s32(0), samples1, filters, 0);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void convolve8_vert_4tap_neon_i8mm(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, const int16_t *filter_y, int w, int h) {
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int16x8_t filter_s16 =
      vcombine_s16(vld1_s16(filter_y + 2), vdup_n_s16(0));
  const int8x8_t filter = vshrn_n_s16(filter_s16, 1);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  uint8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3;
    load_u8_8x4(src, src_stride, &s0, &s1, &s2, &s3);
    src += 4 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    uint8x16_t s0123;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);

    do {
      uint8x8_t s4, s5, s6, s7;
      load_u8_8x4(src, src_stride, &s4, &s5, &s6, &s7);

      uint8x16_t s4567;
      transpose_concat_elems_u8_4x4(s4, s5, s6, s7, &s4567);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s0123;
      samples_LUT.val[1] = s4567;
      uint8x16_t s1234 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      uint8x16_t s2345 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      uint8x16_t s3456 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve4_4_v(s0123, filter);
      int16x4_t d1 = convolve4_4_v(s1234, filter);
      int16x4_t d2 = convolve4_4_v(s2345, filter);
      int16x4_t d3 = convolve4_4_v(s3456, filter);
      // We halved the filter values so -1 from right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src;
      uint8_t *d = dst;

      uint8x8_t s0, s1, s2, s3;
      load_u8_8x4(s, src_stride, &s0, &s1, &s2, &s3);
      s += 4 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      uint8x16_t s0123_lo, s0123_hi;
      transpose_concat_elems_u8_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);

      do {
        uint8x8_t s4, s5, s6, s7;
        load_u8_8x4(s, src_stride, &s4, &s5, &s6, &s7);

        uint8x16_t s4567_lo, s4567_hi;
        transpose_concat_elems_u8_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi);

        // Merge new data into block from previous iteration.
        samples_LUT.val[0] = s0123_lo;
        samples_LUT.val[1] = s4567_lo;
        uint8x16_t s1234_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        uint8x16_t s2345_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        uint8x16_t s3456_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s0123_hi;
        samples_LUT.val[1] = s4567_hi;
        uint8x16_t s1234_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        uint8x16_t s2345_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        uint8x16_t s3456_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        uint8x8_t d0 = convolve4_8_v(s0123_lo, s0123_hi, filter);
        uint8x8_t d1 = convolve4_8_v(s1234_lo, s1234_hi, filter);
        uint8x8_t d2 = convolve4_8_v(s2345_lo, s2345_hi, filter);
        uint8x8_t d3 = convolve4_8_v(s3456_lo, s3456_hi, filter);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;

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

void aom_convolve8_vert_neon_i8mm(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const int16_t *filter_x, int x_step_q4,
                                  const int16_t *filter_y, int y_step_q4, int w,
                                  int h) {
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
    convolve8_vert_4tap_neon_i8mm(src + 2 * src_stride, src_stride, dst,
                                  dst_stride, filter_y, w, h);
  } else {
    convolve8_vert_8tap_neon_i8mm(src, src_stride, dst, dst_stride, filter_y, w,
                                  h);
  }
}
