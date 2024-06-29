/*
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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/arm/convolve_neon.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"

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

static INLINE int16x4_t convolve12_4_x(uint8x16_t samples,
                                       const int8x16_t filter,
                                       const uint8x16x3_t permute_tbl,
                                       const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum;

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filter, 0);
  sum = vusdotq_laneq_s32(sum, permuted_samples[1], filter, 1);
  sum = vusdotq_laneq_s32(sum, permuted_samples[2], filter, 2);

  return vqrshrn_n_s32(sum, FILTER_BITS);
}

static INLINE uint8x8_t convolve12_8_x(uint8x16_t samples[2],
                                       const int8x16_t filter,
                                       const uint8x16x3_t permute_tbl,
                                       const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[4];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples[0], permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples[0], permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples[0], permute_tbl.val[2]);
  // {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 }
  permuted_samples[3] = vqtbl1q_u8(samples[1], permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filter, 0);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[1], filter, 1);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[2], filter, 2);
  // Second 4 output values.
  sum[1] = vusdotq_laneq_s32(horiz_const, permuted_samples[1], filter, 0);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[2], filter, 1);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[3], filter, 2);

  // Narrow and re-pack.
  int16x8_t sum_s16 = vcombine_s16(vqrshrn_n_s32(sum[0], FILTER_BITS),
                                   vqrshrn_n_s32(sum[1], FILTER_BITS));
  return vqmovun_s16(sum_s16);
}

static INLINE void convolve_x_sr_12tap_neon_i8mm(const uint8_t *src,
                                                 int src_stride, uint8_t *dst,
                                                 int dst_stride, int w, int h,
                                                 const int16_t *x_filter_ptr) {
  const int16x8_t filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t filter_8_11 = vld1_s16(x_filter_ptr + 8);
  const int16x8_t filter_8_15 = vcombine_s16(filter_8_11, vdup_n_s16(0));
  const int8x16_t filter =
      vcombine_s8(vmovn_s16(filter_0_7), vmovn_s16(filter_8_15));

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(filter_0_7, 5) == 128) {
    // Undo the horizontal offset in the calling function.
    src += 5;

    do {
      const uint8_t *s = src;
      uint8_t *d = dst;
      int width = w;

      do {
        uint8x8_t d0 = vld1_u8(s);
        if (w == 4) {
          store_u8_4x1(d, d0);
        } else {
          vst1_u8(d, d0);
        }

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);
    // This shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding
    // right shift by FILTER_BITS - instead of a first rounding right shift by
    // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
    // ROUND0_BITS.
    const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

    if (w <= 4) {
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

        int16x4_t d0 = convolve12_4_x(s0, filter, permute_tbl, horiz_const);
        int16x4_t d1 = convolve12_4_x(s1, filter, permute_tbl, horiz_const);
        int16x4_t d2 = convolve12_4_x(s2, filter, permute_tbl, horiz_const);
        int16x4_t d3 = convolve12_4_x(s3, filter, permute_tbl, horiz_const);

        uint8x8_t d01 = vqmovun_s16(vcombine_s16(d0, d1));
        uint8x8_t d23 = vqmovun_s16(vcombine_s16(d2, d3));

        store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
        store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

        dst += 4 * dst_stride;
        src += 4 * src_stride;
        h -= 4;
      } while (h != 0);
    } else {
      do {
        const uint8_t *s = src;
        uint8_t *d = dst;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          uint8x8_t d0 = convolve12_8_x(s0, filter, permute_tbl, horiz_const);
          uint8x8_t d1 = convolve12_8_x(s1, filter, permute_tbl, horiz_const);
          uint8x8_t d2 = convolve12_8_x(s2, filter, permute_tbl, horiz_const);
          uint8x8_t d3 = convolve12_8_x(s3, filter, permute_tbl, horiz_const);

          store_u8_8x4(d + 0 * dst_stride, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src += 4 * src_stride;
        dst += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
    }
  }
}

static INLINE int16x4_t convolve4_4_x(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16_t permute_tbl) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  uint8x16_t permuted_samples = vqtbl1q_u8(samples, permute_tbl);

  // Dot product constants:
  // Adding a shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding
  // right shift by FILTER_BITS - instead of a first rounding right shift by
  // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
  // ROUND0_BITS. Halve the total because we will halve the filter values.
  int32x4_t acc = vdupq_n_s32((1 << (ROUND0_BITS - 1)) / 2);
  int32x4_t sum = vusdotq_lane_s32(acc, permuted_samples, filters, 0);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static INLINE uint8x8_t convolve4_8_x(const uint8x16_t samples,
                                      const int8x8_t filters,
                                      const uint8x16x2_t permute_tbl) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  uint8x16_t permuted_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                     vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // Dot product constants:
  // Adding a shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding
  // right shift by FILTER_BITS - instead of a first rounding right shift by
  // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
  // ROUND0_BITS. Halve the total because we will halve the filter values.
  int32x4_t acc = vdupq_n_s32((1 << (ROUND0_BITS - 1)) / 2);

  // First 4 output values.
  int32x4_t sum0 = vusdotq_lane_s32(acc, permuted_samples[0], filters, 0);
  // Second 4 output values.
  int32x4_t sum1 = vusdotq_lane_s32(acc, permuted_samples[1], filters, 0);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_x_sr_4tap_neon_i8mm(
    const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
    ptrdiff_t dst_stride, int width, int height, const int16_t *filter_x) {
  const int16x4_t x_filter = vld1_s16(filter_x + 2);
  // All 4-tap and bilinear filter values are even, so halve them to reduce
  // intermediate precision requirements.
  const int8x8_t filter = vshrn_n_s16(vcombine_s16(x_filter, vdup_n_s16(0)), 1);

  if (width == 4) {
    const uint8x16_t perm_tbl = vld1q_u8(kDotProdPermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t t0 = convolve4_4_x(s0, filter, perm_tbl);
      int16x4_t t1 = convolve4_4_x(s1, filter, perm_tbl);
      int16x4_t t2 = convolve4_4_x(s2, filter, perm_tbl);
      int16x4_t t3 = convolve4_4_x(s3, filter, perm_tbl);
      // We halved the filter values so -1 from right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(t0, t1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(t2, t3), FILTER_BITS - 1);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint8x16x2_t perm_tbl = vld1q_u8_x2(kDotProdPermuteTbl);

    do {
      int w = width;
      const uint8_t *s = src;
      uint8_t *d = dst;
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve4_8_x(s0, filter, perm_tbl);
        uint8x8_t d1 = convolve4_8_x(s1, filter, perm_tbl);
        uint8x8_t d2 = convolve4_8_x(s2, filter, perm_tbl);
        uint8x8_t d3 = convolve4_8_x(s3, filter, perm_tbl);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

static INLINE uint8x8_t convolve8_8_x(uint8x16_t samples, const int8x8_t filter,
                                      const uint8x16x3_t permute_tbl,
                                      const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_lane_s32(horiz_const, permuted_samples[0], filter, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], filter, 1);
  // Second 4 output values.
  sum[1] = vusdotq_lane_s32(horiz_const, permuted_samples[1], filter, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], filter, 1);

  int16x8_t sum_s16 = vcombine_s16(vmovn_s32(sum[0]), vmovn_s32(sum[1]));
  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(sum_s16, FILTER_BITS - 1);
}

void av1_convolve_x_sr_neon_i8mm(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams *filter_params_x,
                                 const int subpel_x_qn,
                                 ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                        subpel_x_qn, conv_params);
    return;
  }

  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  src -= horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  int filter_taps = get_filter_tap(filter_params_x, subpel_x_qn & SUBPEL_MASK);

  if (filter_taps > 8) {
    convolve_x_sr_12tap_neon_i8mm(src, src_stride, dst, dst_stride, w, h,
                                  x_filter_ptr);
    return;
  }

  if (filter_taps <= 4) {
    convolve_x_sr_4tap_neon_i8mm(src + 2, src_stride, dst, dst_stride, w, h,
                                 x_filter_ptr);
    return;
  }

  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we will halve the filter values.
  const int32x4_t horiz_const = vdupq_n_s32(1 << ((ROUND0_BITS - 1) - 1));

  const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint8x8_t d0 = convolve8_8_x(s0, x_filter, permute_tbl, horiz_const);
      uint8x8_t d1 = convolve8_8_x(s1, x_filter, permute_tbl, horiz_const);
      uint8x8_t d2 = convolve8_8_x(s2, x_filter, permute_tbl, horiz_const);
      uint8x8_t d3 = convolve8_8_x(s3, x_filter, permute_tbl, horiz_const);

      store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    h -= 4;
  } while (h != 0);
}

static INLINE void transpose_concat_4x4(uint8x8_t a0, uint8x8_t a1,
                                        uint8x8_t a2, uint8x8_t a3,
                                        uint8x16_t *b) {
  // Transpose 8-bit elements and concatenate result rows as follows:
  // a0: 00, 01, 02, 03, XX, XX, XX, XX
  // a1: 10, 11, 12, 13, XX, XX, XX, XX
  // a2: 20, 21, 22, 23, XX, XX, XX, XX
  // a3: 30, 31, 32, 33, XX, XX, XX, XX
  //
  // b: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33

  uint8x16_t a0q = vcombine_u8(a0, vdup_n_u8(0));
  uint8x16_t a1q = vcombine_u8(a1, vdup_n_u8(0));
  uint8x16_t a2q = vcombine_u8(a2, vdup_n_u8(0));
  uint8x16_t a3q = vcombine_u8(a3, vdup_n_u8(0));

  uint8x16_t a01 = vzipq_u8(a0q, a1q).val[0];
  uint8x16_t a23 = vzipq_u8(a2q, a3q).val[0];

  uint16x8_t a0123 =
      vzipq_u16(vreinterpretq_u16_u8(a01), vreinterpretq_u16_u8(a23)).val[0];

  *b = vreinterpretq_u8_u16(a0123);
}

static INLINE void transpose_concat_8x4(uint8x8_t a0, uint8x8_t a1,
                                        uint8x8_t a2, uint8x8_t a3,
                                        uint8x16_t *b0, uint8x16_t *b1) {
  // Transpose 8-bit elements and concatenate result rows as follows:
  // a0: 00, 01, 02, 03, 04, 05, 06, 07
  // a1: 10, 11, 12, 13, 14, 15, 16, 17
  // a2: 20, 21, 22, 23, 24, 25, 26, 27
  // a3: 30, 31, 32, 33, 34, 35, 36, 37
  //
  // b0: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
  // b1: 04, 14, 24, 34, 05, 15, 25, 35, 06, 16, 26, 36, 07, 17, 27, 37

  uint8x16_t a0q = vcombine_u8(a0, vdup_n_u8(0));
  uint8x16_t a1q = vcombine_u8(a1, vdup_n_u8(0));
  uint8x16_t a2q = vcombine_u8(a2, vdup_n_u8(0));
  uint8x16_t a3q = vcombine_u8(a3, vdup_n_u8(0));

  uint8x16_t a01 = vzipq_u8(a0q, a1q).val[0];
  uint8x16_t a23 = vzipq_u8(a2q, a3q).val[0];

  uint16x8x2_t a0123 =
      vzipq_u16(vreinterpretq_u16_u8(a01), vreinterpretq_u16_u8(a23));

  *b0 = vreinterpretq_u8_u16(a0123.val[0]);
  *b1 = vreinterpretq_u8_u16(a0123.val[1]);
}

static INLINE int16x4_t convolve12_4_y(const uint8x16_t s0, const uint8x16_t s1,
                                       const uint8x16_t s2,
                                       const int8x8_t filters_0_7,
                                       const int8x8_t filters_4_11) {
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), s0, filters_0_7, 0);
  sum = vusdotq_lane_s32(sum, s1, filters_0_7, 1);
  sum = vusdotq_lane_s32(sum, s2, filters_4_11, 1);

  // Further narrowing and packing is performed by the caller.
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve12_8_y(
    const uint8x16_t s0_lo, const uint8x16_t s0_hi, const uint8x16_t s1_lo,
    const uint8x16_t s1_hi, const uint8x16_t s2_lo, const uint8x16_t s2_hi,
    const int8x8_t filters_0_7, const int8x8_t filters_4_11) {
  int32x4_t sum0123 = vusdotq_lane_s32(vdupq_n_s32(0), s0_lo, filters_0_7, 0);
  sum0123 = vusdotq_lane_s32(sum0123, s1_lo, filters_0_7, 1);
  sum0123 = vusdotq_lane_s32(sum0123, s2_lo, filters_4_11, 1);

  int32x4_t sum4567 = vusdotq_lane_s32(vdupq_n_s32(0), s0_hi, filters_0_7, 0);
  sum4567 = vusdotq_lane_s32(sum4567, s1_hi, filters_0_7, 1);
  sum4567 = vusdotq_lane_s32(sum4567, s2_hi, filters_4_11, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vqmovn_s32(sum0123), vqmovn_s32(sum4567));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve_y_sr_12tap_neon_i8mm(const uint8_t *src_ptr,
                                                 int src_stride,
                                                 uint8_t *dst_ptr,
                                                 int dst_stride, int w, int h,
                                                 const int16_t *y_filter_ptr) {
  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (y_filter_ptr[5] == 128) {
    // Undo the vertical offset in the calling function.
    src_ptr += 5 * src_stride;

    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int width = w;

      do {
        uint8x8_t d0 = vld1_u8(s);
        if (w == 4) {
          store_u8_4x1(d, d0);
        } else {
          vst1_u8(d, d0);
        }

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  } else {
    const int8x8_t filter_0_7 = vmovn_s16(vld1q_s16(y_filter_ptr));
    const int8x8_t filter_4_11 = vmovn_s16(vld1q_s16(y_filter_ptr + 4));

    const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);

    if (w == 4) {
      uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sA;
      load_u8_8x11(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,
                   &s8, &s9, &sA);
      src_ptr += 11 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // (see horizontal case) required before computing the dot product.
      uint8x16_t s0123, s1234, s2345, s3456, s4567, s5678, s6789, s789A;
      transpose_concat_4x4(s0, s1, s2, s3, &s0123);
      transpose_concat_4x4(s1, s2, s3, s4, &s1234);
      transpose_concat_4x4(s2, s3, s4, s5, &s2345);
      transpose_concat_4x4(s3, s4, s5, s6, &s3456);
      transpose_concat_4x4(s4, s5, s6, s7, &s4567);
      transpose_concat_4x4(s5, s6, s7, s8, &s5678);
      transpose_concat_4x4(s6, s7, s8, s9, &s6789);
      transpose_concat_4x4(s7, s8, s9, sA, &s789A);

      do {
        uint8x8_t sB, sC, sD, sE;
        load_u8_8x4(src_ptr, src_stride, &sB, &sC, &sD, &sE);

        uint8x16_t s89AB, s9ABC, sABCD, sBCDE;
        transpose_concat_4x4(sB, sC, sD, sE, &sBCDE);

        // Merge new data into block from previous iteration.
        uint8x16x2_t samples_LUT = { { s789A, sBCDE } };
        s89AB = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s9ABC = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        sABCD = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        int16x4_t d0 =
            convolve12_4_y(s0123, s4567, s89AB, filter_0_7, filter_4_11);
        int16x4_t d1 =
            convolve12_4_y(s1234, s5678, s9ABC, filter_0_7, filter_4_11);
        int16x4_t d2 =
            convolve12_4_y(s2345, s6789, sABCD, filter_0_7, filter_4_11);
        int16x4_t d3 =
            convolve12_4_y(s3456, s789A, sBCDE, filter_0_7, filter_4_11);
        uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
        uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

        store_u8x4_strided_x2(dst_ptr + 0 * dst_stride, dst_stride, d01);
        store_u8x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride, d23);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123 = s4567;
        s1234 = s5678;
        s2345 = s6789;
        s3456 = s789A;
        s4567 = s89AB;
        s5678 = s9ABC;
        s6789 = sABCD;
        s789A = sBCDE;

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
    } else {
      do {
        int height = h;
        const uint8_t *s = src_ptr;
        uint8_t *d = dst_ptr;

        uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sA;
        load_u8_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                     &s9, &sA);
        s += 11 * src_stride;

        // This operation combines a conventional transpose and the sample
        // permute (see horizontal case) required before computing the dot
        // product.
        uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
            s3456_lo, s3456_hi, s4567_lo, s4567_hi, s5678_lo, s5678_hi,
            s6789_lo, s6789_hi, s789A_lo, s789A_hi;
        transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
        transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
        transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
        transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);
        transpose_concat_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi);
        transpose_concat_8x4(s5, s6, s7, s8, &s5678_lo, &s5678_hi);
        transpose_concat_8x4(s6, s7, s8, s9, &s6789_lo, &s6789_hi);
        transpose_concat_8x4(s7, s8, s9, sA, &s789A_lo, &s789A_hi);

        do {
          uint8x8_t sB, sC, sD, sE;
          load_u8_8x4(s, src_stride, &sB, &sC, &sD, &sE);

          uint8x16_t s89AB_lo, s89AB_hi, s9ABC_lo, s9ABC_hi, sABCD_lo, sABCD_hi,
              sBCDE_lo, sBCDE_hi;
          transpose_concat_8x4(sB, sC, sD, sE, &sBCDE_lo, &sBCDE_hi);

          // Merge new data into block from previous iteration.
          uint8x16x2_t samples_LUT_lo = { { s789A_lo, sBCDE_lo } };
          s89AB_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[0]);
          s9ABC_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[1]);
          sABCD_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[2]);

          uint8x16x2_t samples_LUT_hi = { { s789A_hi, sBCDE_hi } };
          s89AB_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[0]);
          s9ABC_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[1]);
          sABCD_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[2]);

          uint8x8_t d0 =
              convolve12_8_y(s0123_lo, s0123_hi, s4567_lo, s4567_hi, s89AB_lo,
                             s89AB_hi, filter_0_7, filter_4_11);
          uint8x8_t d1 =
              convolve12_8_y(s1234_lo, s1234_hi, s5678_lo, s5678_hi, s9ABC_lo,
                             s9ABC_hi, filter_0_7, filter_4_11);
          uint8x8_t d2 =
              convolve12_8_y(s2345_lo, s2345_hi, s6789_lo, s6789_hi, sABCD_lo,
                             sABCD_hi, filter_0_7, filter_4_11);
          uint8x8_t d3 =
              convolve12_8_y(s3456_lo, s3456_hi, s789A_lo, s789A_hi, sBCDE_lo,
                             sBCDE_hi, filter_0_7, filter_4_11);

          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

          // Prepare block for next iteration - re-using as much as possible.
          // Shuffle everything up four rows.
          s0123_lo = s4567_lo;
          s0123_hi = s4567_hi;
          s1234_lo = s5678_lo;
          s1234_hi = s5678_hi;
          s2345_lo = s6789_lo;
          s2345_hi = s6789_hi;
          s3456_lo = s789A_lo;
          s3456_hi = s789A_hi;
          s4567_lo = s89AB_lo;
          s4567_hi = s89AB_hi;
          s5678_lo = s9ABC_lo;
          s5678_hi = s9ABC_hi;
          s6789_lo = sABCD_lo;
          s6789_hi = sABCD_hi;
          s789A_lo = sBCDE_lo;
          s789A_hi = sBCDE_hi;

          s += 4 * src_stride;
          d += 4 * dst_stride;
          height -= 4;
        } while (height != 0);
        src_ptr += 8;
        dst_ptr += 8;
        w -= 8;
      } while (w != 0);
    }
  }
}

static INLINE int16x4_t convolve8_4_y(const uint8x16_t s0, const uint8x16_t s1,
                                      const int8x8_t filters) {
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), s0, filters, 0);
  sum = vusdotq_lane_s32(sum, s1, filters, 1);

  // Further narrowing and packing is performed by the caller.
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve8_8_y(const uint8x16_t s0_lo,
                                      const uint8x16_t s0_hi,
                                      const uint8x16_t s1_lo,
                                      const uint8x16_t s1_hi,
                                      const int8x8_t filters) {
  int32x4_t sum0123 = vusdotq_lane_s32(vdupq_n_s32(0), s0_lo, filters, 0);
  sum0123 = vusdotq_lane_s32(sum0123, s1_lo, filters, 1);

  int32x4_t sum4567 = vusdotq_lane_s32(vdupq_n_s32(0), s0_hi, filters, 0);
  sum4567 = vusdotq_lane_s32(sum4567, s1_hi, filters, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vqmovn_s32(sum0123), vqmovn_s32(sum4567));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve_y_sr_8tap_neon_i8mm(const uint8_t *src_ptr,
                                                int src_stride,
                                                uint8_t *dst_ptr,
                                                int dst_stride, int w, int h,
                                                const int16_t *y_filter_ptr) {
  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (y_filter_ptr[5] == 128) {
    // Undo the vertical offset in the calling function.
    src_ptr += 5 * src_stride;

    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int width = w;

      do {
        uint8x8_t d0 = vld1_u8(s);
        if (w == 4) {
          store_u8_4x1(d, d0);
        } else {
          vst1_u8(d, d0);
        }

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  } else {
    const int8x8_t filter = vmovn_s16(vld1q_s16(y_filter_ptr));

    const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);

    if (w == 4) {
      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      src_ptr += 7 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // (see horizontal case) required before computing the dot product.
      uint8x16_t s0123, s1234, s2345, s3456;
      transpose_concat_4x4(s0, s1, s2, s3, &s0123);
      transpose_concat_4x4(s1, s2, s3, s4, &s1234);
      transpose_concat_4x4(s2, s3, s4, s5, &s2345);
      transpose_concat_4x4(s3, s4, s5, s6, &s3456);

      do {
        uint8x8_t s7, s8, s9, s10;
        load_u8_8x4(src_ptr, src_stride, &s7, &s8, &s9, &s10);

        uint8x16_t s4567, s5678, s6789, s78910;
        transpose_concat_4x4(s7, s8, s9, s10, &s78910);

        // Merge new data into block from previous iteration.
        uint8x16x2_t samples_LUT = { { s3456, s78910 } };
        s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        int16x4_t d0 = convolve8_4_y(s0123, s4567, filter);
        int16x4_t d1 = convolve8_4_y(s1234, s5678, filter);
        int16x4_t d2 = convolve8_4_y(s2345, s6789, filter);
        int16x4_t d3 = convolve8_4_y(s3456, s78910, filter);
        uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
        uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

        store_u8x4_strided_x2(dst_ptr + 0 * dst_stride, dst_stride, d01);
        store_u8x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride, d23);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123 = s4567;
        s1234 = s5678;
        s2345 = s6789;
        s3456 = s78910;

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
    } else {
      do {
        int height = h;
        const uint8_t *s = src_ptr;
        uint8_t *d = dst_ptr;

        uint8x8_t s0, s1, s2, s3, s4, s5, s6;
        load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
        s += 7 * src_stride;

        // This operation combines a conventional transpose and the sample
        // permute (see horizontal case) required before computing the dot
        // product.
        uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
            s3456_lo, s3456_hi;
        transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
        transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
        transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
        transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

        do {
          uint8x8_t s7, s8, s9, s10;
          load_u8_8x4(s, src_stride, &s7, &s8, &s9, &s10);

          uint8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
              s78910_lo, s78910_hi;
          transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi);

          // Merge new data into block from previous iteration.
          uint8x16x2_t samples_LUT_lo = { { s3456_lo, s78910_lo } };
          s4567_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[0]);
          s5678_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[1]);
          s6789_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[2]);

          uint8x16x2_t samples_LUT_hi = { { s3456_hi, s78910_hi } };
          s4567_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[0]);
          s5678_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[1]);
          s6789_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[2]);

          uint8x8_t d0 =
              convolve8_8_y(s0123_lo, s0123_hi, s4567_lo, s4567_hi, filter);
          uint8x8_t d1 =
              convolve8_8_y(s1234_lo, s1234_hi, s5678_lo, s5678_hi, filter);
          uint8x8_t d2 =
              convolve8_8_y(s2345_lo, s2345_hi, s6789_lo, s6789_hi, filter);
          uint8x8_t d3 =
              convolve8_8_y(s3456_lo, s3456_hi, s78910_lo, s78910_hi, filter);

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
        src_ptr += 8;
        dst_ptr += 8;
        w -= 8;
      } while (w != 0);
    }
  }
}

void av1_convolve_y_sr_neon_i8mm(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams *filter_params_y,
                                 const int subpel_y_qn) {
  if (w == 2 || h == 2) {
    av1_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_y,
                        subpel_y_qn);
    return;
  }

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);

  if (y_filter_taps <= 6) {
    av1_convolve_y_sr_neon(src, src_stride, dst, dst_stride, w, h,
                           filter_params_y, subpel_y_qn);
    return;
  }

  const int vert_offset = y_filter_taps / 2 - 1;
  src -= vert_offset * src_stride;

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (y_filter_taps > 8) {
    convolve_y_sr_12tap_neon_i8mm(src, src_stride, dst, dst_stride, w, h,
                                  y_filter_ptr);
    return;
  }
  convolve_y_sr_8tap_neon_i8mm(src, src_stride, dst, dst_stride, w, h,
                               y_filter_ptr);
}

static INLINE int16x4_t convolve12_4_2d_h(uint8x16_t samples,
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum;

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum = vusdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vusdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  // Narrow and re-pack.
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_8_2d_h(uint8x16_t samples[2],
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[4];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples[0], permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples[0], permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples[0], permute_tbl.val[2]);
  // {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 }
  permuted_samples[3] = vqtbl1q_u8(samples[1], permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  // Second 4 output values.
  sum[1] = vusdotq_laneq_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  // Narrow and re-pack.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS),
                      vshrn_n_s32(sum[1], ROUND0_BITS));
}

static INLINE void convolve_2d_sr_horiz_12tap_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(x_filter_0_7, 5) == 128) {
    const uint16x8_t horiz_const = vdupq_n_u16((1 << (bd - 1)));
    // Undo the horizontal offset in the calling function.
    src_ptr += 5;

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x8_t s0 = vld1_u8(s);
        uint16x8_t d0 = vaddw_u8(horiz_const, s0);
        d0 = vshlq_n_u16(d0, FILTER_BITS - ROUND0_BITS);
        // Store 8 elements to avoid additional branches. This is safe if the
        // actual block width is < 8 because the intermediate buffer is large
        // enough to accommodate 128x128 blocks.
        vst1q_s16(d, vreinterpretq_s16_u16(d0));

        d += 8;
        s += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);

  } else {
    // Narrow filter values to 8-bit.
    const int16x8x2_t x_filter_s16 = {
      { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
    };
    const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                           vmovn_s16(x_filter_s16.val[1]));
    // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
    // - which are generally faster than rounding shifts on modern CPUs.
    const int32x4_t horiz_const =
        vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);

    if (w <= 4) {
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

        int16x4_t d0 =
            convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x4_t d1 =
            convolve12_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x4_t d2 =
            convolve12_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x4_t d3 =
            convolve12_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);
        int16x4_t d0 =
            convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
        vst1_s16(dst_ptr, d0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);

    } else {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          int16x8_t d0 =
              convolve12_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
          int16x8_t d1 =
              convolve12_8_2d_h(s1, x_filter, permute_tbl, horiz_const);
          int16x8_t d2 =
              convolve12_8_2d_h(s2, x_filter, permute_tbl, horiz_const);
          int16x8_t d3 =
              convolve12_8_2d_h(s3, x_filter, permute_tbl, horiz_const);

          store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2];
          s0[0] = vld1q_u8(s);
          s0[1] = vld1q_u8(s + 4);
          int16x8_t d0 =
              convolve12_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);
    }
  }
}

static INLINE int16x4_t convolve4_4_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const uint8x16_t permute_tbl,
                                         const int32x4_t horiz_const) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  uint8x16_t permuted_samples = vqtbl1q_u8(samples, permute_tbl);

  // First 4 output values.
  int32x4_t sum = vusdotq_lane_s32(horiz_const, permuted_samples, filters, 0);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrn_n_s32(sum, ROUND0_BITS - 1);
}

static INLINE int16x8_t convolve8_8_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const uint8x16x3_t permute_tbl,
                                         const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_lane_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  // Second 4 output values.
  sum[1] = vusdotq_lane_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static INLINE void convolve_2d_sr_horiz_neon_i8mm(
    const uint8_t *src, int src_stride, int16_t *im_block, int im_stride, int w,
    int im_h, const int16_t *x_filter_ptr) {
  const int bd = 8;
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // The outermost -1 is needed because we halved the filter values.
  const int32x4_t horiz_const = vdupq_n_s32((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w <= 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kDotProdPermuteTbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 = convolve4_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      int16x4_t d1 = convolve4_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
      int16x4_t d2 = convolve4_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
      int16x4_t d3 = convolve4_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);
      int16x4_t d0 = convolve4_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x8_t d1 = convolve8_8_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x8_t d2 = convolve8_8_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x8_t d3 = convolve8_8_2d_h(s3, x_filter, permute_tbl, horiz_const);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0 = vld1q_u8(s);
        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

void av1_convolve_2d_sr_neon_i8mm(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int subpel_y_qn,
                                  ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                         filter_params_x, filter_params_y, subpel_x_qn,
                         subpel_y_qn, conv_params);
    return;
  }

  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
    const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
    const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
    const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

    convolve_2d_sr_horiz_12tap_neon_i8mm(src_ptr, src_stride, im_block,
                                         im_stride, w, im_h, x_filter_0_7,
                                         x_filter_8_11);

    convolve_2d_sr_vert_12tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_0_7, y_filter_8_11);
  } else {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

    convolve_2d_sr_horiz_neon_i8mm(src_ptr, src_stride, im_block, im_stride, w,
                                   im_h, x_filter_ptr);

    const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

    if (clamped_y_taps <= 6) {
      convolve_2d_sr_vert_6tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    } else {
      convolve_2d_sr_vert_8tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    }
  }
}
