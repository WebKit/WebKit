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

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "convolve_neon_dotprod.h"
#include "convolve_neon_i8mm.h"
#include "warp_plane_neon.h"
#include "warp_plane_neon_i8mm.h"

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f4(const uint8x16_t in,
                                                           int sx, int alpha) {
  // Only put the constant in every other lane to avoid double-counting when
  // performing the pairwise add later.
  const int32x4_t add_const =
      vreinterpretq_s32_u64(vdupq_n_u64(1 << (8 + FILTER_BITS - 1)));

  // Loading the 8 filter taps
  int16x8_t f[4];
  load_filters_4(f, sx, alpha);

  int8x16_t f01_u8 = vcombine_s8(vmovn_s16(f[0]), vmovn_s16(f[1]));
  int8x16_t f23_u8 = vcombine_s8(vmovn_s16(f[2]), vmovn_s16(f[3]));

  uint8x8_t in0 = vget_low_u8(in);
  uint8x8_t in1 = vget_low_u8(vextq_u8(in, in, 1));
  uint8x8_t in2 = vget_low_u8(vextq_u8(in, in, 2));
  uint8x8_t in3 = vget_low_u8(vextq_u8(in, in, 3));

  int32x4_t m01 = vusdotq_s32(add_const, vcombine_u8(in0, in1), f01_u8);
  int32x4_t m23 = vusdotq_s32(add_const, vcombine_u8(in2, in3), f23_u8);

  int32x4_t m0123 = vpaddq_s32(m01, m23);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(m0123, ROUND0_BITS), vdup_n_u16(0));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f8(const uint8x16_t in,
                                                           int sx, int alpha) {
  // Only put the constant in every other lane to avoid double-counting when
  // performing the pairwise add later.
  const int32x4_t add_const =
      vreinterpretq_s32_u64(vdupq_n_u64(1 << (8 + FILTER_BITS - 1)));

  // Loading the 8 filter taps
  int16x8_t f[8];
  load_filters_8(f, sx, alpha);

  int8x16_t f01_u8 = vcombine_s8(vmovn_s16(f[0]), vmovn_s16(f[1]));
  int8x16_t f23_u8 = vcombine_s8(vmovn_s16(f[2]), vmovn_s16(f[3]));
  int8x16_t f45_u8 = vcombine_s8(vmovn_s16(f[4]), vmovn_s16(f[5]));
  int8x16_t f67_u8 = vcombine_s8(vmovn_s16(f[6]), vmovn_s16(f[7]));

  uint8x8_t in0 = vget_low_u8(in);
  uint8x8_t in1 = vget_low_u8(vextq_u8(in, in, 1));
  uint8x8_t in2 = vget_low_u8(vextq_u8(in, in, 2));
  uint8x8_t in3 = vget_low_u8(vextq_u8(in, in, 3));
  uint8x8_t in4 = vget_low_u8(vextq_u8(in, in, 4));
  uint8x8_t in5 = vget_low_u8(vextq_u8(in, in, 5));
  uint8x8_t in6 = vget_low_u8(vextq_u8(in, in, 6));
  uint8x8_t in7 = vget_low_u8(vextq_u8(in, in, 7));

  int32x4_t m01 = vusdotq_s32(add_const, vcombine_u8(in0, in1), f01_u8);
  int32x4_t m23 = vusdotq_s32(add_const, vcombine_u8(in2, in3), f23_u8);
  int32x4_t m45 = vusdotq_s32(add_const, vcombine_u8(in4, in5), f45_u8);
  int32x4_t m67 = vusdotq_s32(add_const, vcombine_u8(in6, in7), f67_u8);

  int32x4_t m0123 = vpaddq_s32(m01, m23);
  int32x4_t m4567 = vpaddq_s32(m45, m67);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(m0123, ROUND0_BITS),
                                vqrshrun_n_s32(m4567, ROUND0_BITS));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f1_6tap_beta0(
    const uint8x16_t in, const int8x16_t filter, const uint8x16_t perm_tbl) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  const uint8x16_t perm_samples = vqtbl1q_u8(in, perm_tbl);

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum = vusmmlaq_s32(add_const, perm_samples, filter);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(sum, ROUND0_BITS), vdup_n_u16(0));

  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f1_8tap_beta0(
    const uint8x16_t in, const int8x16_t filter, const int32x4_t f0,
    const uint8x16_t perm_tbl, const uint8x16_t tbl_idx0_3) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  const uint8x16_t perm_samples = vqtbl1q_u8(in, perm_tbl);
  // Get samples 0..3 to apply tap 0 after matrix multiply.
  const int32x4_t samples_0_3 =
      vreinterpretq_s32_u8(vqtbl1q_u8(in, tbl_idx0_3));

  // Calculate partial 7-tap convolution.
  int32x4_t sum = vusmmlaq_s32(add_const, perm_samples, filter);
  // Apply tap 0 and accumulate.
  sum = vmlaq_s32(sum, samples_0_3, f0);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(sum, ROUND0_BITS), vdup_n_u16(0));

  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f1(const uint8x16_t in,
                                                           int sx) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  int16x8_t f_s16 = vld1q_s16(av1_warped_filter[sx >> WARPEDDIFF_PREC_BITS]);
  int8x16_t f_s8 = vcombine_s8(vmovn_s16(f_s16), vmovn_s16(f_s16));

  uint8x16_t perm0 = vld1q_u8(&kDotProdPermuteTbl[0]);
  uint8x16_t perm1 = vld1q_u8(&kDotProdPermuteTbl[16]);

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  uint8x16_t in_0123 = vqtbl1q_u8(in, perm0);
  uint8x16_t in_4567 = vqtbl1q_u8(in, perm1);

  int32x4_t m0123 = vusdotq_laneq_s32(add_const, in_0123, f_s8, 0);
  m0123 = vusdotq_laneq_s32(m0123, in_4567, f_s8, 1);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(m0123, ROUND0_BITS), vdup_n_u16(0));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f1_6tap_beta0(
    const uint8x16_t in, const int8x16_t filter, const uint8x16x2_t perm_tbl) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Permute samples ready for matrix multiply.
  // { 0,  1,  2,  3,  4,  5,  6,  7,  2,  3,  4,  5,  6,  7,  8,  9 }
  // { 4,  5,  6,  7,  8,  9, 10, 11,  6,  7,  8,  9, 10, 11, 12, 13 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(in, perm_tbl.val[0]),
                                 vqtbl1q_u8(in, perm_tbl.val[1]) };

  // These instructions multiply a 2x8 matrix (samples) by an 8x2 matrix
  // (filter), destructively accumulating into the destination register.
  int32x4_t sum0123 = vusmmlaq_s32(add_const, perm_samples[0], filter);
  int32x4_t sum4567 = vusmmlaq_s32(add_const, perm_samples[1], filter);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, ROUND0_BITS),
                                vqrshrun_n_s32(sum4567, ROUND0_BITS));

  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f1_8tap_beta0(
    const uint8x16_t in, const int8x16_t filter, const int16x4_t f0,
    const uint8x16x2_t perm_tbl) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Permute samples ready for matrix multiply.
  // { 1,  2,  3,  4,  5,  6,  7,  8,  3,  4,  5,  6,  7,  8,  9, 10 }
  // { 5,  6,  7,  8,  9, 10, 11, 12,  7,  8,  9, 10, 11, 12, 13, 14 }
  uint8x16_t perm_samples[2] = { vqtbl1q_u8(in, perm_tbl.val[0]),
                                 vqtbl1q_u8(in, perm_tbl.val[1]) };
  // Get samples 0..7 to apply tap 0 after matrix multiply.
  int16x8_t samples_0_7 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(in)));

  // Calculate partial 7-tap convolution.
  int32x4_t sum0123 = vusmmlaq_s32(add_const, perm_samples[0], filter);
  int32x4_t sum4567 = vusmmlaq_s32(add_const, perm_samples[1], filter);
  // Apply tap 0 and accumulate.
  sum0123 = vmlal_s16(sum0123, vget_low_s16(samples_0_7), f0);
  sum4567 = vmlal_s16(sum4567, vget_high_s16(samples_0_7), f0);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, ROUND0_BITS),
                                vqrshrun_n_s32(sum4567, ROUND0_BITS));

  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f1(const uint8x16_t in,
                                                           int sx) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  int16x8_t f_s16 = vld1q_s16(av1_warped_filter[sx >> WARPEDDIFF_PREC_BITS]);
  int8x16_t f_s8 = vcombine_s8(vmovn_s16(f_s16), vmovn_s16(f_s16));

  uint8x16_t perm0 = vld1q_u8(&kDotProdPermuteTbl[0]);
  uint8x16_t perm1 = vld1q_u8(&kDotProdPermuteTbl[16]);
  uint8x16_t perm2 = vld1q_u8(&kDotProdPermuteTbl[32]);

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  uint8x16_t in_0123 = vqtbl1q_u8(in, perm0);
  uint8x16_t in_4567 = vqtbl1q_u8(in, perm1);
  uint8x16_t in_89ab = vqtbl1q_u8(in, perm2);

  int32x4_t m0123 = vusdotq_laneq_s32(add_const, in_0123, f_s8, 0);
  m0123 = vusdotq_laneq_s32(m0123, in_4567, f_s8, 1);

  int32x4_t m4567 = vusdotq_laneq_s32(add_const, in_4567, f_s8, 0);
  m4567 = vusdotq_laneq_s32(m4567, in_89ab, f_s8, 1);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(m0123, ROUND0_BITS),
                                vqrshrun_n_s32(m4567, ROUND0_BITS));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE void vertical_filter_4x1_f4(const int16x8_t *src,
                                                    int32x4_t *res, int sy,
                                                    int gamma) {
  int16x8_t s0, s1, s2, s3;
  transpose_elems_s16_4x8(
      vget_low_s16(src[0]), vget_low_s16(src[1]), vget_low_s16(src[2]),
      vget_low_s16(src[3]), vget_low_s16(src[4]), vget_low_s16(src[5]),
      vget_low_s16(src[6]), vget_low_s16(src[7]), &s0, &s1, &s2, &s3);

  int16x8_t f[4];
  load_filters_4(f, sy, gamma);

  int64x2_t m0 = aom_sdotq_s16(vdupq_n_s64(0), s0, f[0]);
  int64x2_t m1 = aom_sdotq_s16(vdupq_n_s64(0), s1, f[1]);
  int64x2_t m2 = aom_sdotq_s16(vdupq_n_s64(0), s2, f[2]);
  int64x2_t m3 = aom_sdotq_s16(vdupq_n_s64(0), s3, f[3]);

  int64x2_t m01 = vpaddq_s64(m0, m1);
  int64x2_t m23 = vpaddq_s64(m2, m3);

  *res = vcombine_s32(vmovn_s64(m01), vmovn_s64(m23));
}

static AOM_FORCE_INLINE void vertical_filter_8x1_f8(const int16x8_t *src,
                                                    int32x4_t *res_low,
                                                    int32x4_t *res_high, int sy,
                                                    int gamma) {
  int16x8_t s0 = src[0];
  int16x8_t s1 = src[1];
  int16x8_t s2 = src[2];
  int16x8_t s3 = src[3];
  int16x8_t s4 = src[4];
  int16x8_t s5 = src[5];
  int16x8_t s6 = src[6];
  int16x8_t s7 = src[7];
  transpose_elems_inplace_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

  int16x8_t f[8];
  load_filters_8(f, sy, gamma);

  int64x2_t m0 = aom_sdotq_s16(vdupq_n_s64(0), s0, f[0]);
  int64x2_t m1 = aom_sdotq_s16(vdupq_n_s64(0), s1, f[1]);
  int64x2_t m2 = aom_sdotq_s16(vdupq_n_s64(0), s2, f[2]);
  int64x2_t m3 = aom_sdotq_s16(vdupq_n_s64(0), s3, f[3]);
  int64x2_t m4 = aom_sdotq_s16(vdupq_n_s64(0), s4, f[4]);
  int64x2_t m5 = aom_sdotq_s16(vdupq_n_s64(0), s5, f[5]);
  int64x2_t m6 = aom_sdotq_s16(vdupq_n_s64(0), s6, f[6]);
  int64x2_t m7 = aom_sdotq_s16(vdupq_n_s64(0), s7, f[7]);

  int64x2_t m01 = vpaddq_s64(m0, m1);
  int64x2_t m23 = vpaddq_s64(m2, m3);
  int64x2_t m45 = vpaddq_s64(m4, m5);
  int64x2_t m67 = vpaddq_s64(m6, m7);

  *res_low = vcombine_s32(vmovn_s64(m01), vmovn_s64(m23));
  *res_high = vcombine_s32(vmovn_s64(m45), vmovn_s64(m67));
}

static AOM_FORCE_INLINE void warp_affine_horizontal_sve(
    const uint8_t *ref, int width, int height, int stride, int p_width,
    int p_height, int16_t alpha, int16_t beta, const int64_t x4,
    const int64_t y4, const int i, int16x8_t tmp[]) {
  const int height_limit = AOMMIN(8, p_height - i) + 7;

  int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
  int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);

  int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
  sx4 += alpha * (-4) + beta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
         (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
  sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

  if (warp_affine_special_case(ref, ix4, iy4, width, height, stride,
                               height_limit, tmp)) {
    return;
  }

  static const uint8_t kIotaArr[] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                      8, 9, 10, 11, 12, 13, 14, 15 };
  const uint8x16_t indx = vld1q_u8(kIotaArr);

  const int out_of_boundary_left = -(ix4 - 6);
  const int out_of_boundary_right = (ix4 + 8) - width;

  if (p_width == 4) {
    if (beta == 0) {
      if (alpha == 0) {
        int16_t *f_ptr =
            (int16_t *)(av1_warped_filter + (sx4 >> WARPEDDIFF_PREC_BITS));
        int16x8_t f_s16 = vld1q_s16(f_ptr);
        const int8x8_t x_filter = vmovn_s16(f_s16);
        if ((f_ptr[0] | f_ptr[1]) == 0) {
          uint8x16_t perm_tbl = vld1q_u8(kMatMul6PermuteTbl);
          // Offset the permutation table to match filter layout.
          perm_tbl = vaddq_u8(perm_tbl, vdupq_n_u8(2));
          // Stagger filter for use with the matrix multiply instructions.
          // { f2, f3, f4, f5, f6, f7, 0, 0, 0, f2, f3, f4, f5, f6, f7, 0 }
          const int8x16_t filter = vcombine_s8(vext_s8(x_filter, x_filter, 2),
                                               vext_s8(x_filter, x_filter, 1));
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else if ((f_ptr[0] | f_ptr[7]) == 0) {
          uint8x16_t perm_tbl = vld1q_u8(kMatMul6PermuteTbl);
          // Offset the permutation table to match filter layout.
          perm_tbl = vaddq_u8(perm_tbl, vdupq_n_u8(1));
          // Stagger filter for use with the matrix multiply instructions.
          // { f1, f2, f3, f4, f5, f6, 0, 0, 0, f1, f2, f3, f4, f5, f6, 0 }
          const int8x16_t filter =
              vcombine_s8(vext_s8(x_filter, x_filter, 1), x_filter);
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else if ((f_ptr[6] | f_ptr[7]) == 0) {
          const uint8x16_t perm_tbl = vld1q_u8(kMatMul6PermuteTbl);
          // Stagger filter for use with the matrix multiply instructions.
          // { f0, f1, f2, f3, f4, f5, 0, 0, 0, f0, f1, f2, f3, f4, f5, 0 }
          const int8x16_t filter =
              vcombine_s8(x_filter, vext_s8(x_filter, x_filter, 7));
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else {
          const uint8x16_t perm_tbl = vld1q_u8(kMatMul8PermuteTbl);
          const uint8x16_t tbl_idx0_3 = vld1q_u8(kTblIdx0_3);

          // Stagger filter for use with the matrix multiply
          // instructions.
          // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
          const int8x16_t filter = vcombine_s8(
              vext_s8(x_filter, vdup_n_s8(0), 1), vset_lane_s8(0, x_filter, 0));
          const int32x4_t f0 = vdupq_n_s32(f_ptr[0]);

          APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f1_8tap_beta0, filter,
                                 f0, perm_tbl, tbl_idx0_3);
        }
      } else {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f4, sx4, alpha);
      }
    } else {
      if (alpha == 0) {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f1,
                               (sx4 + beta * (k - 3)));
      } else {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_4x1_f4, (sx4 + beta * (k - 3)),
                               alpha);
      }
    }
  } else {
    if (beta == 0) {
      if (alpha == 0) {
        int16_t *f_ptr =
            (int16_t *)(av1_warped_filter + (sx4 >> WARPEDDIFF_PREC_BITS));
        int16x8_t f_s16 = vld1q_s16(f_ptr);
        const int8x8_t x_filter = vmovn_s16(f_s16);
        if ((f_ptr[0] | f_ptr[1]) == 0) {
          uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
          // Offset the permutation table to match filter layout.
          perm_tbl.val[0] = vaddq_u8(perm_tbl.val[0], vdupq_n_u8(2));
          perm_tbl.val[1] = vaddq_u8(perm_tbl.val[1], vdupq_n_u8(2));
          // Stagger filter for use with the matrix multiply instructions.
          // { f0, f1, f2, f3, f4, f5, 0, 0, 0, f0, f1, f2, f3, f4, f5, 0 }
          const int8x16_t filter = vcombine_s8(vext_s8(x_filter, x_filter, 2),
                                               vext_s8(x_filter, x_filter, 1));
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else if ((f_ptr[0] | f_ptr[7]) == 0) {
          uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
          // Offset the permutation table to match filter layout.
          perm_tbl.val[0] = vaddq_u8(perm_tbl.val[0], vdupq_n_u8(1));
          perm_tbl.val[1] = vaddq_u8(perm_tbl.val[1], vdupq_n_u8(1));
          // Stagger filter for use with the matrix multiply instructions.
          // { f1, f2, f3, f4, f5, f6, 0, 0, 0, f1, f2, f3, f4, f5, f6, 0 }
          const int8x16_t filter =
              vcombine_s8(vext_s8(x_filter, x_filter, 1), x_filter);
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else if ((f_ptr[6] | f_ptr[7]) == 0) {
          uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul6PermuteTbl);
          // Stagger filter for use with the matrix multiply instructions.
          // { f0, f1, f2, f3, f4, f5, 0, 0, 0, f0, f1, f2, f3, f4, f5, 0 }
          const int8x16_t filter =
              vcombine_s8(x_filter, vext_s8(x_filter, x_filter, 7));
          APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f1_6tap_beta0, filter,
                                 perm_tbl);
        } else {
          uint8x16x2_t perm_tbl = vld1q_u8_x2(kMatMul8PermuteTbl);
          // Stagger filter for use with the matrix multiply instructions.
          // { f1, f2, f3, f4, f5, f6, f7, 0, 0, f1, f2, f3, f4, f5, f6, f7 }
          const int8x16_t filter = vcombine_s8(
              vext_s8(x_filter, vdup_n_s8(0), 1), vset_lane_s8(0, x_filter, 0));

          const int16x4_t f0 = vdup_n_s16(f_ptr[0]);

          APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f1_8tap_beta0, filter,
                                 f0, perm_tbl);
        }
      } else {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f8, sx4, alpha);
      }
    } else {
      if (alpha == 0) {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f1,
                               (sx4 + beta * (k - 3)));
      } else {
        APPLY_HORIZONTAL_SHIFT(horizontal_filter_8x1_f8, (sx4 + beta * (k - 3)),
                               alpha);
      }
    }
  }
}

void av1_warp_affine_sve(const int32_t *mat, const uint8_t *ref, int width,
                         int height, int stride, uint8_t *pred, int p_col,
                         int p_row, int p_width, int p_height, int p_stride,
                         int subsampling_x, int subsampling_y,
                         ConvolveParams *conv_params, int16_t alpha,
                         int16_t beta, int16_t gamma, int16_t delta) {
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const int is_compound = conv_params->is_compound;
  uint16_t *const dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;

  assert(IMPLIES(is_compound, dst != NULL));
  assert(IMPLIES(do_average, is_compound));

  for (int i = 0; i < p_height; i += 8) {
    for (int j = 0; j < p_width; j += 8) {
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];

      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      int16x8_t tmp[15];
      warp_affine_horizontal_sve(ref, width, height, stride, p_width, p_height,
                                 alpha, beta, x4, y4, i, tmp);
      warp_affine_vertical(pred, p_width, p_height, p_stride, is_compound, dst,
                           dst_stride, do_average, use_dist_wtd_comp_avg, gamma,
                           delta, y4, i, j, tmp, w0, w1);
    }
  }
}
