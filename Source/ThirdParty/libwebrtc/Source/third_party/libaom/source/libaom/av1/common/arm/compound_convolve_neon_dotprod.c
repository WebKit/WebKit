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
#include "av1/common/arm/compound_convolve_neon.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

DECLARE_ALIGNED(16, static const uint8_t, dot_prod_permute_tbl[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

static inline int16x4_t convolve4_4_2d_h(uint8x16_t samples,
                                         const int8x8_t x_filter,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16_t permute_tbl) {
  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t clamped_samples =
      vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  int8x16_t permuted_samples = vqtbl1q_s8(clamped_samples, permute_tbl);

  // Accumulate dot product into 'correction' to account for range clamp.
  int32x4_t sum = vdotq_lane_s32(correction, permuted_samples, x_filter, 0);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrn_n_s32(sum, ROUND0_BITS - 1);
}

static inline int16x8_t convolve8_8_2d_h(uint8x16_t samples,
                                         const int8x8_t x_filter,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum[2];

  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product. */
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  // Accumulate dot product into 'correction' to account for range clamp.
  // First 4 output values.
  sum[0] = vdotq_lane_s32(correction, permuted_samples[0], x_filter, 0);
  sum[0] = vdotq_lane_s32(sum[0], permuted_samples[1], x_filter, 1);
  // Second 4 output values.
  sum[1] = vdotq_lane_s32(correction, permuted_samples[1], x_filter, 0);
  sum[1] = vdotq_lane_s32(sum[1], permuted_samples[2], x_filter, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static inline void dist_wtd_convolve_2d_horiz_neon_dotprod(
    const uint8_t *src, int src_stride, int16_t *im_block, const int im_stride,
    const int16_t *x_filter_ptr, const int im_h, int w) {
  const int bd = 8;
  // Dot product constants and other shims.
  const int16x8_t x_filter_s16 = vld1q_s16(x_filter_ptr);
  // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
  // - which are generally faster than rounding shifts on modern CPUs.
  const int32_t horiz_const =
      ((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
  // Halve the total because we will halve the filter values.
  const int32x4_t correction =
      vdupq_n_s32(((128 << FILTER_BITS) + horiz_const) / 2);
  const uint8x16_t range_limit = vdupq_n_u8(128);

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 =
          convolve4_4_2d_h(s0, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d1 =
          convolve4_4_2d_h(s1, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d2 =
          convolve4_4_2d_h(s2, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d3 =
          convolve4_4_2d_h(s3, x_filter, correction, range_limit, permute_tbl);

      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);

      int16x4_t d0 =
          convolve4_4_2d_h(s0, x_filter, correction, range_limit, permute_tbl);

      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d1 = convolve8_8_2d_h(s1, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d2 = convolve8_8_2d_h(s2, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d3 = convolve8_8_2d_h(s3, x_filter, correction, range_limit,
                                        permute_tbl);

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

        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, correction, range_limit,
                                        permute_tbl);

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

void av1_dist_wtd_convolve_2d_neon_dotprod(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

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

  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

  dist_wtd_convolve_2d_horiz_neon_dotprod(src_ptr, src_stride, im_block,
                                          im_stride, x_filter_ptr, im_h, w);

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

static inline uint16x4_t convolve4_4_x(uint8x16_t samples,
                                       const int8x8_t x_filter,
                                       const int32x4_t correction,
                                       const uint8x16_t range_limit,
                                       const uint8x16_t permute_tbl) {
  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t clamped_samples =
      vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  int8x16_t permuted_samples = vqtbl1q_s8(clamped_samples, permute_tbl);

  // Accumulate dot product into 'correction' to account for range clamp.
  int32x4_t sum = vdotq_lane_s32(correction, permuted_samples, x_filter, 0);

  // We halved the convolution filter values so -1 from the right shift.
  return vreinterpret_u16_s16(vshrn_n_s32(sum, ROUND0_BITS - 1));
}

static inline uint16x8_t convolve8_8_x(uint8x16_t samples,
                                       const int8x8_t x_filter,
                                       const int32x4_t correction,
                                       const uint8x16_t range_limit,
                                       const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum[2];

  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product. */
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  // Accumulate dot product into 'correction' to account for range clamp.
  // First 4 output values.
  sum[0] = vdotq_lane_s32(correction, permuted_samples[0], x_filter, 0);
  sum[0] = vdotq_lane_s32(sum[0], permuted_samples[1], x_filter, 1);
  // Second 4 output values.
  sum[1] = vdotq_lane_s32(correction, permuted_samples[1], x_filter, 0);
  sum[1] = vdotq_lane_s32(sum[1], permuted_samples[2], x_filter, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  int16x8_t res = vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                               vshrn_n_s32(sum[1], ROUND0_BITS - 1));
  return vreinterpretq_u16_s16(res);
}

static inline void dist_wtd_convolve_x_dist_wtd_avg_neon_dotprod(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  const uint16_t fwd_offset = conv_params->fwd_offset;
  const uint16_t bck_offset = conv_params->bck_offset;

  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16x8_t x_filter_s16 = vld1q_s16(x_filter_ptr);

  // Dot-product constants and other shims.
  const uint8x16_t range_limit = vdupq_n_u8(128);
  // Fold round_offset into the dot-product filter correction constant. The
  // additional shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // Halve the total because we will halve the filter values.
  int32x4_t correction =
      vdupq_n_s32(((128 << FILTER_BITS) + (round_offset << ROUND0_BITS) +
                   (1 << (ROUND0_BITS - 1))) /
                  2);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  uint8_t *dst8_ptr = dst8;
  int dst_stride = conv_params->dst_stride;
  int height = h;

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve4_4_x(s0, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d1 =
          convolve4_4_x(s1, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d2 =
          convolve4_4_x(s2, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d3 =
          convolve4_4_x(s3, x_filter, correction, range_limit, permute_tbl);

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d01_u8, d23_u8;
      compute_dist_wtd_avg_4x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3, fwd_offset,
                               bck_offset, round_offset_vec, &d01_u8, &d23_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d01_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d23_u8);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);

    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve8_8_x(s0, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d1 =
            convolve8_8_x(s1, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d2 =
            convolve8_8_x(s2, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d3 =
            convolve8_8_x(s3, x_filter, correction, range_limit, permute_tbl);

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
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline void dist_wtd_convolve_x_avg_neon_dotprod(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));
  const int16x8_t round_offset_vec = vdupq_n_s16(round_offset);

  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16x8_t x_filter_s16 = vld1q_s16(x_filter_ptr);

  // Dot-product constants and other shims.
  const uint8x16_t range_limit = vdupq_n_u8(128);
  // Fold round_offset into the dot-product filter correction constant. The
  // additional shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // Halve the total because we will halve the filter values.
  int32x4_t correction =
      vdupq_n_s32(((128 << FILTER_BITS) + (round_offset << ROUND0_BITS) +
                   (1 << (ROUND0_BITS - 1))) /
                  2);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  uint8_t *dst8_ptr = dst8;
  int dst_stride = conv_params->dst_stride;
  int height = h;

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve4_4_x(s0, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d1 =
          convolve4_4_x(s1, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d2 =
          convolve4_4_x(s2, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d3 =
          convolve4_4_x(s3, x_filter, correction, range_limit, permute_tbl);

      uint16x4_t dd0, dd1, dd2, dd3;
      load_u16_4x4(dst_ptr, dst_stride, &dd0, &dd1, &dd2, &dd3);

      uint8x8_t d01_u8, d23_u8;
      compute_basic_avg_4x4(dd0, dd1, dd2, dd3, d0, d1, d2, d3,
                            round_offset_vec, &d01_u8, &d23_u8);

      store_u8x4_strided_x2(dst8_ptr + 0 * dst8_stride, dst8_stride, d01_u8);
      store_u8x4_strided_x2(dst8_ptr + 2 * dst8_stride, dst8_stride, d23_u8);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);

    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      uint8_t *d_u8 = dst8_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve8_8_x(s0, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d1 =
            convolve8_8_x(s1, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d2 =
            convolve8_8_x(s2, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d3 =
            convolve8_8_x(s3, x_filter, correction, range_limit, permute_tbl);

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
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      dst8_ptr += 4 * dst8_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline void dist_wtd_convolve_x_neon_dotprod(
    const uint8_t *src, int src_stride, int w, int h,
    const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int16_t round_offset = (1 << (offset_bits - COMPOUND_ROUND1_BITS)) +
                               (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));

  // Horizontal filter.
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16x8_t x_filter_s16 = vld1q_s16(x_filter_ptr);

  // Dot-product constants and other shims.
  const uint8x16_t range_limit = vdupq_n_u8(128);
  // Fold round_offset into the dot-product filter correction constant. The
  // additional shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // Halve the total because we will halve the vilter values.
  int32x4_t correction =
      vdupq_n_s32(((128 << FILTER_BITS) + (round_offset << ROUND0_BITS) +
                   (1 << (ROUND0_BITS - 1))) /
                  2);

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - horiz_offset;
  CONV_BUF_TYPE *dst_ptr = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int height = h;

  if (w == 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 =
          convolve4_4_x(s0, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d1 =
          convolve4_4_x(s1, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d2 =
          convolve4_4_x(s2, x_filter, correction, range_limit, permute_tbl);
      uint16x4_t d3 =
          convolve4_4_x(s3, x_filter, correction, range_limit, permute_tbl);

      store_u16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);

    do {
      const uint8_t *s = src_ptr;
      CONV_BUF_TYPE *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint16x8_t d0 =
            convolve8_8_x(s0, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d1 =
            convolve8_8_x(s1, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d2 =
            convolve8_8_x(s2, x_filter, correction, range_limit, permute_tbl);
        uint16x8_t d3 =
            convolve8_8_x(s3, x_filter, correction, range_limit, permute_tbl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_dist_wtd_convolve_x_neon_dotprod(
    const uint8_t *src, int src_stride, uint8_t *dst8, int dst8_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  if (conv_params->do_average) {
    if (UNLIKELY(conv_params->use_dist_wtd_comp_avg)) {
      dist_wtd_convolve_x_dist_wtd_avg_neon_dotprod(
          src, src_stride, dst8, dst8_stride, w, h, filter_params_x,
          subpel_x_qn, conv_params);
    } else {
      dist_wtd_convolve_x_avg_neon_dotprod(src, src_stride, dst8, dst8_stride,
                                           w, h, filter_params_x, subpel_x_qn,
                                           conv_params);
    }
  } else {
    dist_wtd_convolve_x_neon_dotprod(src, src_stride, w, h, filter_params_x,
                                     subpel_x_qn, conv_params);
  }
}
