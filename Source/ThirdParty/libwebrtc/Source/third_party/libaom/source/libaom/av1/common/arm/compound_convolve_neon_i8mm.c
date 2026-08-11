/*
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/arm/compound_convolve_neon.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "convolve_neon_dotprod.h"
#include "convolve_neon_i8mm.h"

static inline int16x4_t convolve6_4_2d_h(uint8x16_t samples,
                                         const int8x16_t x_filter,
                                         const uint8x16_t permute_tbl,
                                         const int32x4_t horiz_const) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  uint8x16_t permuted_samples = vqtbl1q_u8(samples, permute_tbl);

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum = vusmmlaq_s32(horiz_const, permuted_samples, x_filter);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrn_n_s32(sum, ROUND0_BITS - 1);
}

static inline int16x8_t convolve6_8_2d_h(uint8x16_t samples,
                                         const int8x16_t x_filter,
                                         const uint8x16x2_t permute_tbl,
                                         const int32x4_t horiz_const) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  // { 4,  5,  6,  7,  8,  9, 10, 11,  6,  7,  8,  9, 10, 11, 12, 13 }
  uint8x16_t permuted_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                     vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum0123 = vusmmlaq_s32(horiz_const, permuted_samples[0], x_filter);
  int32x4_t sum4567 = vusmmlaq_s32(horiz_const, permuted_samples[1], x_filter);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum0123, ROUND0_BITS - 1),
                      vshrn_n_s32(sum4567, ROUND0_BITS - 1));
}

static inline void dist_wtd_convolve_2d_horiz_6tap_neon_i8mm(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    const int16_t *x_filter_ptr, const int im_h, int w) {
  const int bd = 8;
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int32x4_t horiz_const = vdupq_n_s32((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f0, f1, f2, f3, f4, f5,  0,  0,  0, f0, f1, f2, f3, f4, f5,  0 }
  const int8x16_t x_filter =
      vcombine_s8(vext_s8(x_filter_s8, x_filter_s8, 1), x_filter_s8);

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kMatMul6PermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 = convolve6_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      int16x4_t d1 = convolve6_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
      int16x4_t d2 = convolve6_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
      int16x4_t d3 = convolve6_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);

      int16x4_t d0 = convolve6_4_2d_h(s0, x_filter, permute_tbl, horiz_const);

      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        int16x8_t d0 = convolve6_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x8_t d1 = convolve6_8_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x8_t d2 = convolve6_8_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x8_t d3 = convolve6_8_2d_h(s3, x_filter, permute_tbl, horiz_const);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
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

        int16x8_t d0 = convolve6_8_2d_h(s0, x_filter, permute_tbl, horiz_const);

        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

static inline int16x8_t convolve8_8_2d_h(uint8x16_t samples,
                                         const int8x16_t x_filter,
                                         const uint8x8_t f0,
                                         const uint8x16x2_t permute_tbl,
                                         const uint16x8_t horiz_const) {
  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  // { 5,  6,  7,  8,  9, 10, 11, 12,  7,  8,  9, 10, 11, 12, 13, 14 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                 vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // Calculate partial 7-tap convolution.
  int32x4_t sum0123 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[0], x_filter);
  int32x4_t sum4567 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[1], x_filter);
  uint16x8_t sum = vreinterpretq_u16_s16(
      vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567)));

  // Apply tap 0 and accumulate.
  sum = vmlsl_u8(sum, vget_low_u8(samples), f0);

  sum = vaddq_u16(sum, horiz_const);

  // We halved the convolution filter values so -1 from the right shift.
  return vreinterpretq_s16_u16(vshrq_n_u16(sum, ROUND0_BITS - 1));
}

static inline void dist_wtd_convolve_2d_horiz_8tap_neon_i8mm(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    const int16_t *x_filter_ptr, const int im_h, int w) {
  const int bd = 8;
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const uint16x8_t horiz_const = vdupq_n_u16((1 << (bd + FILTER_BITS - 2)) +
                                             (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul8PermuteTbl);

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

  // Stagger the filter for use with the matrix multiply instructions.
  // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
  const uint8x16_t filter_idx = vld1q_u8(kFilterPermuteTbl);
  const int8x16_t x_filter =
      vqtbl1q_s8(vcombine_s8(x_filter_s8, vdup_n_s8(0)), filter_idx);

  // Since f0 is always negative and s0 is unsigned, subtract (unsigned) s0 *
  // -f0 to avoid signed overflow.
  const uint8x8_t f0 = vdup_n_u8(-x_filter_ptr[0] >> 1);

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

      int16x8_t d0 =
          convolve8_8_2d_h(s0, x_filter, f0, permute_tbl, horiz_const);
      int16x8_t d1 =
          convolve8_8_2d_h(s1, x_filter, f0, permute_tbl, horiz_const);
      int16x8_t d2 =
          convolve8_8_2d_h(s2, x_filter, f0, permute_tbl, horiz_const);
      int16x8_t d3 =
          convolve8_8_2d_h(s3, x_filter, f0, permute_tbl, horiz_const);

      store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width > 0);
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

      int16x8_t d0 =
          convolve8_8_2d_h(s0, x_filter, f0, permute_tbl, horiz_const);

      vst1q_s16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--height != 0);
}

void av1_dist_wtd_convolve_2d_neon_i8mm(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_x_taps = x_filter_taps < 6 ? 6 : x_filter_taps;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;

  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

  if (clamped_x_taps == 6) {
    dist_wtd_convolve_2d_horiz_6tap_neon_i8mm(src_ptr, src_stride, im_block,
                                              im_stride, x_filter_ptr, im_h, w);
  } else {
    dist_wtd_convolve_2d_horiz_8tap_neon_i8mm(src_ptr, src_stride, im_block,
                                              im_stride, x_filter_ptr, im_h, w);
  }

  if (clamped_y_taps == 6) {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_2d_vert_6tap_dist_wtd_avg_neon(
            im_block, im_stride, dst8, dst8_stride, conv_params, y_filter, h,
            w);
      } else {
        dist_wtd_convolve_2d_vert_6tap_avg_neon(im_block, im_stride, dst8,
                                                dst8_stride, conv_params,
                                                y_filter, h, w);
      }
    } else {
      dist_wtd_convolve_2d_vert_6tap_neon(im_block, im_stride, conv_params,
                                          y_filter, h, w);
    }
  } else {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_2d_vert_8tap_dist_wtd_avg_neon(
            im_block, im_stride, dst8, dst8_stride, conv_params, y_filter, h,
            w);
      } else {
        dist_wtd_convolve_2d_vert_8tap_avg_neon(im_block, im_stride, dst8,
                                                dst8_stride, conv_params,
                                                y_filter, h, w);
      }
    } else {
      dist_wtd_convolve_2d_vert_8tap_neon(im_block, im_stride, conv_params,
                                          y_filter, h, w);
    }
  }
}

static inline uint16x4_t convolve6_4_x(uint8x16_t samples,
                                       const int8x16_t x_filter,
                                       const uint8x16_t permute_tbl,
                                       const int32x4_t round_offset) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  uint8x16_t permuted_samples = vqtbl1q_u8(samples, permute_tbl);

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum = vusmmlaq_s32(round_offset, permuted_samples, x_filter);

  // We halved the convolution filter values so -1 from the right shift.
  return vreinterpret_u16_s16(vshrn_n_s32(sum, ROUND0_BITS - 1));
}

static inline uint16x8_t convolve6_8_x(uint8x16_t samples,
                                       const int8x16_t x_filter,
                                       const uint8x16x2_t permute_tbl,
                                       const int32x4_t round_offset) {
  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  // { 4,  5,  6,  7,  8,  9, 10, 11,  6,  7,  8,  9, 10, 11, 12, 13 }
  uint8x16_t permuted_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                     vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum0123 = vusmmlaq_s32(round_offset, permuted_samples[0], x_filter);
  int32x4_t sum4567 = vusmmlaq_s32(round_offset, permuted_samples[1], x_filter);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  int16x8_t res = vcombine_s16(vshrn_n_s32(sum0123, ROUND0_BITS - 1),
                               vshrn_n_s32(sum4567, ROUND0_BITS - 1));
  return vreinterpretq_u16_s16(res);
}

static inline uint16x8_t convolve8_8_x_usdot(uint8x16_t samples,
                                             const int8x8_t x_filter,
                                             const uint8x16x3_t permute_tbl,
                                             const int32x4_t round_offset) {
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
  sum[0] = vusdotq_lane_s32(round_offset, permuted_samples[0], x_filter, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], x_filter, 1);
  // Second 4 output values.
  sum[1] = vusdotq_lane_s32(round_offset, permuted_samples[1], x_filter, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], x_filter, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  int16x8_t res = vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                               vshrn_n_s32(sum[1], ROUND0_BITS - 1));
  return vreinterpretq_u16_s16(res);
}

static inline uint16x8_t convolve8_8_x_usmmla(uint8x16_t samples,
                                              const int8x16_t x_filter,
                                              const uint8x8_t f0,
                                              const uint8x16x2_t permute_tbl,
                                              const uint16x8_t horiz_const) {
  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  // { 5,  6,  7,  8,  9, 10, 11, 12,  7,  8,  9, 10, 11, 12, 13, 14 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(samples, permute_tbl.val[0]),
                                 vqtbl1q_u8(samples, permute_tbl.val[1]) };

  // Calculate partial 7-tap convolution.
  int32x4_t sum0123 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[0], x_filter);
  int32x4_t sum4567 = vusmmlaq_s32(vdupq_n_s32(0), perm_samples[1], x_filter);
  uint16x8_t sum = vreinterpretq_u16_s16(
      vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567)));

  // Apply tap 0 and accumulate.
  sum = vmlsl_u8(sum, vget_low_u8(samples), f0);

  sum = vaddq_u16(sum, horiz_const);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrq_n_u16(sum, ROUND0_BITS - 1);
}

static inline void dist_wtd_convolve_x_dist_wtd_avg_6tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride,
    uint8_t *dst8, int dst8_stride, int w, int h, const int16_t *x_filter_ptr,
    const uint16_t fwd_offset, const uint16_t bck_offset) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int32x4_t round_offset_shim = vdupq_n_s32(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f0, f1, f2, f3, f4, f5,  0,  0,  0, f0, f1, f2, f3, f4, f5,  0 }
  const int8x16_t x_filter =
      vcombine_s8(vext_s8(x_filter_s8, x_filter_s8, 1), x_filter_s8);

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kMatMul6PermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve6_4_x(s0, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d1 =
          convolve6_4_x(s1, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d2 =
          convolve6_4_x(s2, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d3 =
          convolve6_4_x(s3, x_filter, permute_tbl, round_offset_shim);

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d01_u8, d23_u8;
      compute_dist_wtd_avg_4x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                               bck_offset, round_offset_vec, &d01_u8, &d23_u8);

      store_u8x4_strided_x2(dst8 + 0 * dst8_stride, dst8_stride, d01_u8);
      store_u8x4_strided_x2(dst8 + 2 * dst8_stride, dst8_stride, d23_u8);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
    do {
      const uint8_t *s = src;
      uint16_t *d = dst;
      uint8_t *d_u8 = dst8;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve6_8_x(s0, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d1 =
            convolve6_8_x(s1, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d2 =
            convolve6_8_x(s2, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d3 =
            convolve6_8_x(s3, x_filter, permute_tbl, round_offset_shim);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                                 bck_offset, round_offset_vec, &d0_u8, &d1_u8,
                                 &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

        s += 8;
        d += 8;
        d_u8 += 8;
        width -= 8;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  }
}

static inline void dist_wtd_convolve_x_dist_wtd_avg_8tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride,
    uint8_t *dst8, int dst8_stride, int w, int h, const int16_t *x_filter_ptr,
    const uint16_t fwd_offset, const uint16_t bck_offset) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int32x4_t round_offset_shim = vdupq_n_s32(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8x16x3_t permute_tbl = vld1q_u8_x3(kDotProdPermuteTbl);
  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

  do {
    const uint8_t *s = src;
    uint16_t *d = dst;
    uint8_t *d_u8 = dst8;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x8_t d0 =
          convolve8_8_x_usdot(s0, x_filter, permute_tbl, round_offset_shim);
      uint16x8_t d1 =
          convolve8_8_x_usdot(s1, x_filter, permute_tbl, round_offset_shim);
      uint16x8_t d2 =
          convolve8_8_x_usdot(s2, x_filter, permute_tbl, round_offset_shim);
      uint16x8_t d3 =
          convolve8_8_x_usdot(s3, x_filter, permute_tbl, round_offset_shim);

      uint16x8_t dd0, dd1, dd2, dd3;
      load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_dist_wtd_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                               bck_offset, round_offset_vec, &d0_u8, &d1_u8,
                               &d2_u8, &d3_u8);

      store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

      s += 8;
      d += 8;
      d_u8 += 8;
      width -= 8;
    } while (width != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    dst8 += 4 * dst8_stride;
    h -= 4;
  } while (h != 0);
}

static inline void dist_wtd_convolve_x_avg_6tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride,
    uint8_t *dst8, int dst8_stride, int w, int h, const int16_t *x_filter_ptr) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int32x4_t round_offset_shim = vdupq_n_s32(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f0, f1, f2, f3, f4, f5,  0,  0,  0, f0, f1, f2, f3, f4, f5,  0 }
  const int8x16_t x_filter =
      vcombine_s8(vext_s8(x_filter_s8, x_filter_s8, 1), x_filter_s8);

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kMatMul6PermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve6_4_x(s0, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d1 =
          convolve6_4_x(s1, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d2 =
          convolve6_4_x(s2, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d3 =
          convolve6_4_x(s3, x_filter, permute_tbl, round_offset_shim);

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d01_u8, d23_u8;
      compute_basic_avg_4x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                            round_offset_vec, &d01_u8, &d23_u8);

      store_u8x4_strided_x2(dst8 + 0 * dst8_stride, dst8_stride, d01_u8);
      store_u8x4_strided_x2(dst8 + 2 * dst8_stride, dst8_stride, d23_u8);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
    do {
      const uint8_t *s = src;
      uint16_t *d = dst;
      uint8_t *d_u8 = dst8;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve6_8_x(s0, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d1 =
            convolve6_8_x(s1, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d2 =
            convolve6_8_x(s2, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d3 =
            convolve6_8_x(s3, x_filter, permute_tbl, round_offset_shim);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                              round_offset_vec, &d0_u8, &d1_u8, &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

        s += 8;
        d += 8;
        d_u8 += 8;
        width -= 8;
      } while (width != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      dst8 += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  }
}

static inline void dist_wtd_convolve_x_avg_8tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride,
    uint8_t *dst8, int dst8_stride, int w, int h, const int16_t *x_filter_ptr) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const uint16x8_t round_offset_shim = vdupq_n_u16(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul8PermuteTbl);

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

  // Stagger the filter for use with the matrix multiply instructions.
  // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
  const uint8x16_t filter_idx = vld1q_u8(kFilterPermuteTbl);
  const int8x16_t x_filter =
      vqtbl1q_s8(vcombine_s8(x_filter_s8, vdup_n_s8(0)), filter_idx);

  // Since f0 is always negative and s0 is unsigned, subtract (unsigned) s0 *
  // -f0 to avoid signed overflow.
  const uint8x8_t f0 = vdup_n_u8(-x_filter_ptr[0] >> 1);

  do {
    const uint8_t *s = src;
    uint16_t *d = dst;
    uint8_t *d_u8 = dst8;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x8_t d0 = convolve8_8_x_usmmla(s0, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d1 = convolve8_8_x_usmmla(s1, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d2 = convolve8_8_x_usmmla(s2, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d3 = convolve8_8_x_usmmla(s3, x_filter, f0, permute_tbl,
                                           round_offset_shim);

      uint16x8_t dd0, dd1, dd2, dd3;
      load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
      compute_basic_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                            round_offset_vec, &d0_u8, &d1_u8, &d2_u8, &d3_u8);

      store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

      s += 8;
      d += 8;
      d_u8 += 8;
      width -= 8;
    } while (width != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    dst8 += 4 * dst8_stride;
    h -= 4;
  } while (h != 0);
}

static inline void dist_wtd_convolve_x_6tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const int16_t *x_filter_ptr) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const int32x4_t round_offset_shim = vdupq_n_s32(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);
  // Stagger the filter for use with the matrix multiply instructions.
  // { f0, f1, f2, f3, f4, f5,  0,  0,  0, f0, f1, f2, f3, f4, f5,  0 }
  const int8x16_t x_filter =
      vcombine_s8(vext_s8(x_filter_s8, x_filter_s8, 1), x_filter_s8);

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(kMatMul6PermuteTbl);
    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve6_4_x(s0, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d1 =
          convolve6_4_x(s1, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d2 =
          convolve6_4_x(s2, x_filter, permute_tbl, round_offset_shim);
      uint16x4_t d3 =
          convolve6_4_x(s3, x_filter, permute_tbl, round_offset_shim);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
    do {
      const uint8_t *s = src;
      uint16_t *d = dst;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve6_8_x(s0, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d1 =
            convolve6_8_x(s1, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d2 =
            convolve6_8_x(s2, x_filter, permute_tbl, round_offset_shim);
        uint16x8_t d3 =
            convolve6_8_x(s3, x_filter, permute_tbl, round_offset_shim);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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

static inline void dist_wtd_convolve_x_8tap_neon_i8mm(
    const uint8_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const int16_t *x_filter_ptr) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // (The extra -1 is needed because we halved the filter values.)
  const uint16x8_t round_offset_shim = vdupq_n_u16(
      (round_offset << (ROUND0_BITS - 1)) + (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8x16x2_t permute_tbl = vld1q_u8_x2(kMatMul8PermuteTbl);

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t x_filter_s8 = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

  // Stagger the filter for use with the matrix multiply instructions.
  // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
  const uint8x16_t filter_idx = vld1q_u8(kFilterPermuteTbl);
  const int8x16_t x_filter =
      vqtbl1q_s8(vcombine_s8(x_filter_s8, vdup_n_s8(0)), filter_idx);

  // Since f0 is always negative and s0 is unsigned, subtract (unsigned) s0 *
  // -f0 to avoid signed overflow.
  const uint8x8_t f0 = vdup_n_u8(-x_filter_ptr[0] >> 1);

  do {
    const uint8_t *s = src;
    uint16_t *d = dst;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x8_t d0 = convolve8_8_x_usmmla(s0, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d1 = convolve8_8_x_usmmla(s1, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d2 = convolve8_8_x_usmmla(s2, x_filter, f0, permute_tbl,
                                           round_offset_shim);
      uint16x8_t d3 = convolve8_8_x_usmmla(s3, x_filter, f0, permute_tbl,
                                           round_offset_shim);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    h -= 4;
  } while (h != 0);
}

void av1_dist_wtd_convolve_x_neon_i8mm(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int filter_taps =
      get_filter_tap(filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= (SUBPEL_TAPS / 2 - 1);

  if (conv_params->do_average) {
    if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
      if (filter_taps < 8) {
        dist_wtd_convolve_x_dist_wtd_avg_6tap_neon_i8mm(
            src + 1, src_stride, conv_params->dst, conv_params->dst_stride,
            dst8, dst8_stride, w, h, x_filter_ptr, conv_params->fwd_offset,
            conv_params->bck_offset);
        return;
      }

      dist_wtd_convolve_x_dist_wtd_avg_8tap_neon_i8mm(
          src, src_stride, conv_params->dst, conv_params->dst_stride, dst8,
          dst8_stride, w, h, x_filter_ptr, conv_params->fwd_offset,
          conv_params->bck_offset);
    } else {
      if (filter_taps < 8) {
        dist_wtd_convolve_x_avg_6tap_neon_i8mm(
            src + 1, src_stride, conv_params->dst, conv_params->dst_stride,
            dst8, dst8_stride, w, h, x_filter_ptr);
        return;
      }

      dist_wtd_convolve_x_avg_8tap_neon_i8mm(src, src_stride, conv_params->dst,
                                             conv_params->dst_stride, dst8,
                                             dst8_stride, w, h, x_filter_ptr);
    }
  } else {
    if (filter_taps < 8) {
      dist_wtd_convolve_x_6tap_neon_i8mm(src + 1, src_stride, conv_params->dst,
                                         conv_params->dst_stride, w, h,
                                         x_filter_ptr);
      return;
    }

    dist_wtd_convolve_x_8tap_neon_i8mm(src, src_stride, conv_params->dst,
                                       conv_params->dst_stride, w, h,
                                       x_filter_ptr);
  }
}

static inline int16x4_t convolve8_4_y(const uint8x16_t s0, const uint8x16_t s1,
                                      const int8x8_t filters) {
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), s0, filters, 0);
  sum = vusdotq_lane_s32(sum, s1, filters, 1);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static inline uint16x8_t convolve8_8_y(const uint8x16_t s0_lo,
                                       const uint8x16_t s0_hi,
                                       const uint8x16_t s1_lo,
                                       const uint8x16_t s1_hi,
                                       const int8x8_t filters,
                                       const int16x8_t round_offset) {
  int32x4_t sum0123 = vusdotq_lane_s32(vdupq_n_s32(0), s0_lo, filters, 0);
  sum0123 = vusdotq_lane_s32(sum0123, s1_lo, filters, 1);

  int32x4_t sum4567 = vusdotq_lane_s32(vdupq_n_s32(0), s0_hi, filters, 0);
  sum4567 = vusdotq_lane_s32(sum4567, s1_hi, filters, 1);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));

  // We halved the filter values so -1 from right shift.
  return vreinterpretq_u16_s16(
      vrsraq_n_s16(round_offset, sum, ROUND0_BITS - 1));
}

static inline void dist_wtd_convolve_y_8tap_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, int w, int h,
    const int16_t *y_filter_ptr, ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t filter = vshrn_n_s16(vld1q_s16(y_filter_ptr), 1);

  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3, s4, s5, s6;
    load_u8_8x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src_ptr += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // (see horizontal case) required before computing the dot product.
    uint8x16_t s0123, s1234, s2345, s3456;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);
    transpose_concat_elems_u8_4x4(s1, s2, s3, s4, &s1234);
    transpose_concat_elems_u8_4x4(s2, s3, s4, s5, &s2345);
    transpose_concat_elems_u8_4x4(s3, s4, s5, s6, &s3456);

    do {
      uint8x8_t s7, s8, s9, sA;
      load_u8_8x4(src_ptr, src_stride, &s7, &s8, &s9, &sA);

      uint8x16_t s4567, s5678, s6789, s789A;
      transpose_concat_elems_u8_4x4(s7, s8, s9, sA, &s789A);

      // Merge new data into block from previous iteration.
      uint8x16x2_t samples_LUT = { { s3456, s789A } };
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve8_4_y(s0123, s4567, filter);
      int16x4_t d1 = convolve8_4_y(s1234, s5678, filter);
      int16x4_t d2 = convolve8_4_y(s2345, s6789, filter);
      int16x4_t d3 = convolve8_4_y(s3456, s789A, filter);

      // We halved the filter values so -1 from right shift.
      int16x8_t d01 =
          vrsraq_n_s16(round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1);
      int16x8_t d23 =
          vrsraq_n_s16(round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1);

      store_u16x4_strided_x2(dst_ptr + 0 * dst_stride, dst_stride,
                             vreinterpretq_u16_s16(d01));
      store_u16x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride,
                             vreinterpretq_u16_s16(d23));

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s789A;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;

      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample
      // permute (see horizontal case) required before computing the dot
      // product.
      uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
          s3456_lo, s3456_hi;
      transpose_concat_elems_u8_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
      transpose_concat_elems_u8_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
      transpose_concat_elems_u8_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
      transpose_concat_elems_u8_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

      do {
        uint8x8_t s7, s8, s9, sA;
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &sA);

        uint8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
            s789A_lo, s789A_hi;
        transpose_concat_elems_u8_8x4(s7, s8, s9, sA, &s789A_lo, &s789A_hi);

        // Merge new data into block from previous iteration.
        uint8x16x2_t samples_LUT_lo = { { s3456_lo, s789A_lo } };
        s4567_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[2]);

        uint8x16x2_t samples_LUT_hi = { { s3456_hi, s789A_hi } };
        s4567_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[2]);

        uint16x8_t d0 = convolve8_8_y(s0123_lo, s0123_hi, s4567_lo, s4567_hi,
                                      filter, round_offset_vec);
        uint16x8_t d1 = convolve8_8_y(s1234_lo, s1234_hi, s5678_lo, s5678_hi,
                                      filter, round_offset_vec);
        uint16x8_t d2 = convolve8_8_y(s2345_lo, s2345_hi, s6789_lo, s6789_hi,
                                      filter, round_offset_vec);
        uint16x8_t d3 = convolve8_8_y(s3456_lo, s3456_hi, s789A_lo, s789A_hi,
                                      filter, round_offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

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

static inline void dist_wtd_convolve_y_8tap_dist_wtd_avg_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t filter = vshrn_n_s16(vld1q_s16(y_filter_ptr), 1);

  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3, s4, s5, s6;
    load_u8_8x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src_ptr += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // (see horizontal case) required before computing the dot product.
    uint8x16_t s0123, s1234, s2345, s3456;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);
    transpose_concat_elems_u8_4x4(s1, s2, s3, s4, &s1234);
    transpose_concat_elems_u8_4x4(s2, s3, s4, s5, &s2345);
    transpose_concat_elems_u8_4x4(s3, s4, s5, s6, &s3456);

    do {
      uint8x8_t s7, s8, s9, sA;
      load_u8_8x4(src_ptr, src_stride, &s7, &s8, &s9, &sA);

      uint8x16_t s4567, s5678, s6789, s789A;
      transpose_concat_elems_u8_4x4(s7, s8, s9, sA, &s789A);

      // Merge new data into block from previous iteration.
      uint8x16x2_t samples_LUT = { { s3456, s789A } };
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve8_4_y(s0123, s4567, filter);
      int16x4_t d1 = convolve8_4_y(s1234, s5678, filter);
      int16x4_t d2 = convolve8_4_y(s2345, s6789, filter);
      int16x4_t d3 = convolve8_4_y(s3456, s789A, filter);

      // We halved the filter values so -1 from right shift.
      uint16x8_t d01 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1));
      uint16x8_t d23 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1));

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8;
      compute_dist_wtd_avg_8x2(vcombine_u16(dd0, dd1), vcombine_u16(dd2, dd3),
                               d01, d23, fwd_offset, bck_offset,
                               round_offset_vec, &d0_u8, &d1_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d0_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d1_u8);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s789A;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;

      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample
      // permute (see horizontal case) required before computing the dot
      // product.
      uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
          s3456_lo, s3456_hi;
      transpose_concat_elems_u8_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
      transpose_concat_elems_u8_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
      transpose_concat_elems_u8_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
      transpose_concat_elems_u8_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

      do {
        uint8x8_t s7, s8, s9, sA;
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &sA);

        uint8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
            s789A_lo, s789A_hi;
        transpose_concat_elems_u8_8x4(s7, s8, s9, sA, &s789A_lo, &s789A_hi);

        // Merge new data into block from previous iteration.
        uint8x16x2_t samples_LUT_lo = { { s3456_lo, s789A_lo } };
        s4567_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[2]);

        uint8x16x2_t samples_LUT_hi = { { s3456_hi, s789A_hi } };
        s4567_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[2]);

        uint16x8_t d0 = convolve8_8_y(s0123_lo, s0123_hi, s4567_lo, s4567_hi,
                                      filter, round_offset_vec);
        uint16x8_t d1 = convolve8_8_y(s1234_lo, s1234_hi, s5678_lo, s5678_hi,
                                      filter, round_offset_vec);
        uint16x8_t d2 = convolve8_8_y(s2345_lo, s2345_hi, s6789_lo, s6789_hi,
                                      filter, round_offset_vec);
        uint16x8_t d3 = convolve8_8_y(s3456_lo, s3456_hi, s789A_lo, s789A_hi,
                                      filter, round_offset_vec);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                                 bck_offset, round_offset_vec, &d0_u8, &d1_u8,
                                 &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

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

        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      dst8_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline void dist_wtd_convolve_y_8tap_avg_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int8x8_t filter = vshrn_n_s16(vld1q_s16(y_filter_ptr), 1);

  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3, s4, s5, s6;
    load_u8_8x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src_ptr += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // (see horizontal case) required before computing the dot product.
    uint8x16_t s0123, s1234, s2345, s3456;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);
    transpose_concat_elems_u8_4x4(s1, s2, s3, s4, &s1234);
    transpose_concat_elems_u8_4x4(s2, s3, s4, s5, &s2345);
    transpose_concat_elems_u8_4x4(s3, s4, s5, s6, &s3456);

    do {
      uint8x8_t s7, s8, s9, sA;
      load_u8_8x4(src_ptr, src_stride, &s7, &s8, &s9, &sA);

      uint8x16_t s4567, s5678, s6789, s789A;
      transpose_concat_elems_u8_4x4(s7, s8, s9, sA, &s789A);

      // Merge new data into block from previous iteration.
      uint8x16x2_t samples_LUT = { { s3456, s789A } };
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve8_4_y(s0123, s4567, filter);
      int16x4_t d1 = convolve8_4_y(s1234, s5678, filter);
      int16x4_t d2 = convolve8_4_y(s2345, s6789, filter);
      int16x4_t d3 = convolve8_4_y(s3456, s789A, filter);

      // We halved the filter values so -1 from right shift.
      uint16x8_t d01 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1));
      uint16x8_t d23 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1));

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8;
      compute_basic_avg_8x2(vcombine_u16(dd0, dd1), vcombine_u16(dd2, dd3), d01,
                            d23, round_offset_vec, &d0_u8, &d1_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d0_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d1_u8);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s789A;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;

      uint8x8_t s0, s1, s2, s3, s4, s5, s6;
      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample
      // permute (see horizontal case) required before computing the dot
      // product.
      uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
          s3456_lo, s3456_hi;
      transpose_concat_elems_u8_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi);
      transpose_concat_elems_u8_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi);
      transpose_concat_elems_u8_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi);
      transpose_concat_elems_u8_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi);

      do {
        uint8x8_t s7, s8, s9, sA;
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &sA);

        uint8x16_t s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo, s6789_hi,
            s789A_lo, s789A_hi;
        transpose_concat_elems_u8_8x4(s7, s8, s9, sA, &s789A_lo, &s789A_hi);

        // Merge new data into block from previous iteration.
        uint8x16x2_t samples_LUT_lo = { { s3456_lo, s789A_lo } };
        s4567_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT_lo, merge_block_tbl.val[2]);

        uint8x16x2_t samples_LUT_hi = { { s3456_hi, s789A_hi } };
        s4567_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT_hi, merge_block_tbl.val[2]);

        uint16x8_t d0 = convolve8_8_y(s0123_lo, s0123_hi, s4567_lo, s4567_hi,
                                      filter, round_offset_vec);
        uint16x8_t d1 = convolve8_8_y(s1234_lo, s1234_hi, s5678_lo, s5678_hi,
                                      filter, round_offset_vec);
        uint16x8_t d2 = convolve8_8_y(s2345_lo, s2345_hi, s6789_lo, s6789_hi,
                                      filter, round_offset_vec);
        uint16x8_t d3 = convolve8_8_y(s3456_lo, s3456_hi, s789A_lo, s789A_hi,
                                      filter, round_offset_vec);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                              round_offset_vec, &d0_u8, &d1_u8, &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

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

        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      dst8_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline int16x4_t convolve4_4_y(const uint8x16_t s0,
                                      const int8x8_t filters) {
  int32x4_t sum = vusdotq_lane_s32(vdupq_n_s32(0), s0, filters, 0);

  // Further narrowing and packing is performed by the caller.
  return vmovn_s32(sum);
}

static inline uint16x8_t convolve4_8_y(const uint8x16_t s0, const uint8x16_t s1,
                                       const int8x8_t filters,
                                       const int16x8_t round_offset) {
  int32x4_t sum0123 = vusdotq_lane_s32(vdupq_n_s32(0), s0, filters, 0);
  int32x4_t sum4567 = vusdotq_lane_s32(vdupq_n_s32(0), s1, filters, 0);

  // Narrow and re-pack.
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));

  // We halved the filter values so -1 from right shift.
  return vreinterpretq_u16_s16(
      vrsraq_n_s16(round_offset, sum, ROUND0_BITS - 1));
}

static inline void dist_wtd_convolve_y_4tap_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, int w, int h,
    const int16_t *y_filter_ptr, ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int16x8_t filter_s16 =
      vcombine_s16(vld1_s16(y_filter_ptr + 2), vdup_n_s16(0));
  const int8x8_t filter = vshrn_n_s16(filter_s16, 1);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  uint8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3;
    load_u8_8x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);
    src_ptr += 4 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    uint8x16_t s0123;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);

    do {
      uint8x8_t s4, s5, s6, s7;
      load_u8_8x4(src_ptr, src_stride, &s4, &s5, &s6, &s7);

      uint8x16_t s4567;
      transpose_concat_elems_u8_4x4(s4, s5, s6, s7, &s4567);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s0123;
      samples_LUT.val[1] = s4567;
      uint8x16_t s1234 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      uint8x16_t s2345 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      uint8x16_t s3456 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve4_4_y(s0123, filter);
      int16x4_t d1 = convolve4_4_y(s1234, filter);
      int16x4_t d2 = convolve4_4_y(s2345, filter);
      int16x4_t d3 = convolve4_4_y(s3456, filter);

      // We halved the filter values so -1 from right shift.
      int16x8_t d01 =
          vrsraq_n_s16(round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1);
      int16x8_t d23 =
          vrsraq_n_s16(round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1);

      store_u16x4_strided_x2(dst_ptr + 0 * dst_stride, dst_stride,
                             vreinterpretq_u16_s16(d01));
      store_u16x4_strided_x2(dst_ptr + 2 * dst_stride, dst_stride,
                             vreinterpretq_u16_s16(d23));

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;

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

        uint16x8_t d0 =
            convolve4_8_y(s0123_lo, s0123_hi, filter, round_offset_vec);
        uint16x8_t d1 =
            convolve4_8_y(s1234_lo, s1234_hi, filter, round_offset_vec);
        uint16x8_t d2 =
            convolve4_8_y(s2345_lo, s2345_hi, filter, round_offset_vec);
        uint16x8_t d3 =
            convolve4_8_y(s3456_lo, s3456_hi, filter, round_offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;

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

static inline void dist_wtd_convolve_y_4tap_avg_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int16x8_t filter_s16 =
      vcombine_s16(vld1_s16(y_filter_ptr + 2), vdup_n_s16(0));
  const int8x8_t filter = vshrn_n_s16(filter_s16, 1);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  uint8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3;
    load_u8_8x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);
    src_ptr += 4 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    uint8x16_t s0123;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);

    do {
      uint8x8_t s4, s5, s6, s7;
      load_u8_8x4(src_ptr, src_stride, &s4, &s5, &s6, &s7);

      uint8x16_t s4567;
      transpose_concat_elems_u8_4x4(s4, s5, s6, s7, &s4567);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s0123;
      samples_LUT.val[1] = s4567;
      uint8x16_t s1234 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      uint8x16_t s2345 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      uint8x16_t s3456 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve4_4_y(s0123, filter);
      int16x4_t d1 = convolve4_4_y(s1234, filter);
      int16x4_t d2 = convolve4_4_y(s2345, filter);
      int16x4_t d3 = convolve4_4_y(s3456, filter);

      // We halved the filter values so -1 from right shift.
      uint16x8_t d01 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1));
      uint16x8_t d23 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1));

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8;
      compute_basic_avg_8x2(vcombine_u16(dd0, dd1), vcombine_u16(dd2, dd3), d01,
                            d23, round_offset_vec, &d0_u8, &d1_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d0_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d1_u8);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;

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

        uint16x8_t d0 =
            convolve4_8_y(s0123_lo, s0123_hi, filter, round_offset_vec);
        uint16x8_t d1 =
            convolve4_8_y(s1234_lo, s1234_hi, filter, round_offset_vec);
        uint16x8_t d2 =
            convolve4_8_y(s2345_lo, s2345_hi, filter, round_offset_vec);
        uint16x8_t d3 =
            convolve4_8_y(s3456_lo, s3456_hi, filter, round_offset_vec);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_basic_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                              round_offset_vec, &d0_u8, &d1_u8, &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      dst8_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static inline void dist_wtd_convolve_y_4tap_dist_wtd_avg_neon_i8mm(
    const uint8_t *src_ptr, int src_stride, uint8_t *dst8_ptr,
    const int dst8_stride, int w, int h, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;

  // Filter values are even, so halve to reduce intermediate precision reqs.
  const int16x8_t filter_s16 =
      vcombine_s16(vld1_s16(y_filter_ptr + 2), vdup_n_s16(0));
  const int8x8_t filter = vshrn_n_s16(filter_s16, 1);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(kDotProdMergeBlockTbl);
  uint8x16x2_t samples_LUT;

  if (w == 4) {
    uint8x8_t s0, s1, s2, s3;
    load_u8_8x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);
    src_ptr += 4 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    uint8x16_t s0123;
    transpose_concat_elems_u8_4x4(s0, s1, s2, s3, &s0123);

    do {
      uint8x8_t s4, s5, s6, s7;
      load_u8_8x4(src_ptr, src_stride, &s4, &s5, &s6, &s7);

      uint8x16_t s4567;
      transpose_concat_elems_u8_4x4(s4, s5, s6, s7, &s4567);

      // Merge new data into block from previous iteration.
      samples_LUT.val[0] = s0123;
      samples_LUT.val[1] = s4567;
      uint8x16_t s1234 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      uint8x16_t s2345 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      uint8x16_t s3456 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      int16x4_t d0 = convolve4_4_y(s0123, filter);
      int16x4_t d1 = convolve4_4_y(s1234, filter);
      int16x4_t d2 = convolve4_4_y(s2345, filter);
      int16x4_t d3 = convolve4_4_y(s3456, filter);

      // We halved the filter values so -1 from right shift.
      uint16x8_t d01 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d0, d1), ROUND0_BITS - 1));
      uint16x8_t d23 = vreinterpretq_u16_s16(vrsraq_n_s16(
          round_offset_vec, vcombine_s16(d2, d3), ROUND0_BITS - 1));

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d0_u8, d1_u8;
      compute_dist_wtd_avg_8x2(vcombine_u16(dd0, dd1), vcombine_u16(dd2, dd3),
                               d01, d23, fwd_offset, bck_offset,
                               round_offset_vec, &d0_u8, &d1_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d0_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d1_u8);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123 = s4567;

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;

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

        uint16x8_t d0 =
            convolve4_8_y(s0123_lo, s0123_hi, filter, round_offset_vec);
        uint16x8_t d1 =
            convolve4_8_y(s1234_lo, s1234_hi, filter, round_offset_vec);
        uint16x8_t d2 =
            convolve4_8_y(s2345_lo, s2345_hi, filter, round_offset_vec);
        uint16x8_t d3 =
            convolve4_8_y(s3456_lo, s3456_hi, filter, round_offset_vec);

        uint16x8_t dd0, dd1, dd2, dd3;
        load_u16_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        uint8x8_t d0_u8, d1_u8, d2_u8, d3_u8;
        compute_dist_wtd_avg_8x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                                 bck_offset, round_offset_vec, &d0_u8, &d1_u8,
                                 &d2_u8, &d3_u8);

        store_u8_8x4(d_u8, dst8_stride, d0_u8, d1_u8, d2_u8, d3_u8);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        d_u8 += 4 * dst8_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      dst8_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

void av1_dist_wtd_convolve_y_neon_i8mm(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (get_filter_tap(filter_params_y, subpel_y_qn) <= 4) {
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_y_4tap_dist_wtd_avg_neon_i8mm(
            src - src_stride, src_stride, dst8, dst8_stride, w, h, y_filter_ptr,
            conv_params);
      } else {
        dist_wtd_convolve_y_4tap_avg_neon_i8mm(src - src_stride, src_stride,
                                               dst8, dst8_stride, w, h,
                                               y_filter_ptr, conv_params);
      }
    } else {
      dist_wtd_convolve_y_4tap_neon_i8mm(src - src_stride, src_stride, w, h,
                                         y_filter_ptr, conv_params);
    }
  } else {  // filter tap >= 6
    if (conv_params->do_average) {
      if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
        dist_wtd_convolve_y_8tap_dist_wtd_avg_neon_i8mm(
            src - 3 * src_stride, src_stride, dst8, dst8_stride, w, h,
            y_filter_ptr, conv_params);
      } else {
        dist_wtd_convolve_y_8tap_avg_neon_i8mm(src - 3 * src_stride, src_stride,
                                               dst8, dst8_stride, w, h,
                                               y_filter_ptr, conv_params);
      }
    } else {
      dist_wtd_convolve_y_8tap_neon_i8mm(src - 3 * src_stride, src_stride, w, h,
                                         y_filter_ptr, conv_params);
    }
  }
}
