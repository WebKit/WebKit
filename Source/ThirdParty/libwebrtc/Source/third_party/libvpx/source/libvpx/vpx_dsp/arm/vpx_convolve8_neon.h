/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VPX_DSP_ARM_VPX_CONVOLVE8_NEON_H_
#define VPX_VPX_DSP_ARM_VPX_CONVOLVE8_NEON_H_

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_filter.h"

#if VPX_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

void vpx_convolve8_2d_horiz_neon_dotprod(const uint8_t *src,
                                         ptrdiff_t src_stride, uint8_t *dst,
                                         ptrdiff_t dst_stride,
                                         const InterpKernel *filter, int x0_q4,
                                         int x_step_q4, int y0_q4,
                                         int y_step_q4, int w, int h);

static INLINE int16x4_t convolve8_4_sdot_partial(const int8x16_t samples_lo,
                                                 const int8x16_t samples_hi,
                                                 const int32x4_t correction,
                                                 const int8x8_t filters) {
  /* Sample range-clamping and permutation are performed by the caller. */
  int32x4_t sum;

  /* Accumulate dot product into 'correction' to account for range clamp. */
  sum = vdotq_lane_s32(correction, samples_lo, filters, 0);
  sum = vdotq_lane_s32(sum, samples_hi, filters, 1);

  /* Further narrowing and packing is performed by the caller. */
  return vqmovn_s32(sum);
}

static INLINE int16x4_t convolve8_4_sdot(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x2_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[2];
  int32x4_t sum;

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  sum = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum = vdotq_lane_s32(sum, permuted_samples[1], filters, 1);

  /* Further narrowing and packing is performed by the caller. */
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve8_8_sdot_partial(const int8x16_t samples0_lo,
                                                 const int8x16_t samples0_hi,
                                                 const int8x16_t samples1_lo,
                                                 const int8x16_t samples1_hi,
                                                 const int32x4_t correction,
                                                 const int8x8_t filters) {
  /* Sample range-clamping and permutation are performed by the caller. */
  int32x4_t sum0, sum1;
  int16x8_t sum;

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum0 = vdotq_lane_s32(correction, samples0_lo, filters, 0);
  sum0 = vdotq_lane_s32(sum0, samples0_hi, filters, 1);
  /* Second 4 output values. */
  sum1 = vdotq_lane_s32(correction, samples1_lo, filters, 0);
  sum1 = vdotq_lane_s32(sum1, samples1_hi, filters, 1);

  /* Narrow and re-pack. */
  sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE uint8x8_t convolve8_8_sdot(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum0, sum1;
  int16x8_t sum;

  /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  /* First 4 output values. */
  sum0 = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum0 = vdotq_lane_s32(sum0, permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum1 = vdotq_lane_s32(correction, permuted_samples[1], filters, 0);
  sum1 = vdotq_lane_s32(sum1, permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

#endif  // VPX_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

#if VPX_ARCH_AARCH64 && defined(__ARM_FEATURE_MATMUL_INT8)

void vpx_convolve8_2d_horiz_neon_i8mm(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const InterpKernel *filter, int x0_q4,
                                      int x_step_q4, int y0_q4, int y_step_q4,
                                      int w, int h);

static INLINE int16x4_t convolve8_4_usdot_partial(const uint8x16_t samples_lo,
                                                  const uint8x16_t samples_hi,
                                                  const int8x8_t filters) {
  /* Sample permutation is performed by the caller. */
  int32x4_t sum;

  sum = vusdotq_lane_s32(vdupq_n_s32(0), samples_lo, filters, 0);
  sum = vusdotq_lane_s32(sum, samples_hi, filters, 1);

  /* Further narrowing and packing is performed by the caller. */
  return vqmovn_s32(sum);
}

static INLINE int16x4_t convolve8_4_usdot(uint8x16_t samples,
                                          const int8x8_t filters,
                                          const uint8x16x2_t permute_tbl) {
  uint8x16_t permuted_samples[2];
  int32x4_t sum;

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);

  /* Accumulate dot product into 'correction' to account for range clamp. */
  sum = vusdotq_lane_s32(vdupq_n_s32(0), permuted_samples[0], filters, 0);
  sum = vusdotq_lane_s32(sum, permuted_samples[1], filters, 1);

  /* Further narrowing and packing is performed by the caller. */
  return vqmovn_s32(sum);
}

static INLINE uint8x8_t convolve8_8_usdot_partial(const uint8x16_t samples0_lo,
                                                  const uint8x16_t samples0_hi,
                                                  const uint8x16_t samples1_lo,
                                                  const uint8x16_t samples1_hi,
                                                  const int8x8_t filters) {
  /* Sample permutation is performed by the caller. */
  int32x4_t sum0, sum1;
  int16x8_t sum;

  /* First 4 output values. */
  sum0 = vusdotq_lane_s32(vdupq_n_s32(0), samples0_lo, filters, 0);
  sum0 = vusdotq_lane_s32(sum0, samples0_hi, filters, 1);
  /* Second 4 output values. */
  sum1 = vusdotq_lane_s32(vdupq_n_s32(0), samples1_lo, filters, 0);
  sum1 = vusdotq_lane_s32(sum1, samples1_hi, filters, 1);

  /* Narrow and re-pack. */
  sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE uint8x8_t convolve8_8_usdot(uint8x16_t samples,
                                          const int8x8_t filters,
                                          const uint8x16x3_t permute_tbl) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum0, sum1;
  int16x8_t sum;

  /* Permute samples ready for dot product. */
  /* { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 } */
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  /* { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 } */
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  /* { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 } */
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  /* First 4 output values. */
  sum0 = vusdotq_lane_s32(vdupq_n_s32(0), permuted_samples[0], filters, 0);
  sum0 = vusdotq_lane_s32(sum0, permuted_samples[1], filters, 1);
  /* Second 4 output values. */
  sum1 = vusdotq_lane_s32(vdupq_n_s32(0), permuted_samples[1], filters, 0);
  sum1 = vusdotq_lane_s32(sum1, permuted_samples[2], filters, 1);

  /* Narrow and re-pack. */
  sum = vcombine_s16(vqmovn_s32(sum0), vqmovn_s32(sum1));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

#endif  // VPX_ARCH_AARCH64 && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int16x4_t convolve8_4(const int16x4_t s0, const int16x4_t s1,
                                    const int16x4_t s2, const int16x4_t s3,
                                    const int16x4_t s4, const int16x4_t s5,
                                    const int16x4_t s6, const int16x4_t s7,
                                    const int16x8_t filters) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, filters_lo, 0);
  sum = vmla_lane_s16(sum, s1, filters_lo, 1);
  sum = vmla_lane_s16(sum, s2, filters_lo, 2);
  sum = vmla_lane_s16(sum, s5, filters_hi, 1);
  sum = vmla_lane_s16(sum, s6, filters_hi, 2);
  sum = vmla_lane_s16(sum, s7, filters_hi, 3);
  sum = vqadd_s16(sum, vmul_lane_s16(s3, filters_lo, 3));
  sum = vqadd_s16(sum, vmul_lane_s16(s4, filters_hi, 0));
  return sum;
}

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16x8_t filters) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, filters_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filters_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filters_lo, 2);
  sum = vmlaq_lane_s16(sum, s5, filters_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filters_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filters_hi, 3);
  sum = vqaddq_s16(sum, vmulq_lane_s16(s3, filters_lo, 3));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s4, filters_hi, 0));
  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filters) {
  int16x8_t ss[8];

  ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
  ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
  ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
  ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
  ss[4] = vreinterpretq_s16_u16(vmovl_u8(s[4]));
  ss[5] = vreinterpretq_s16_u16(vmovl_u8(s[5]));
  ss[6] = vreinterpretq_s16_u16(vmovl_u8(s[6]));
  ss[7] = vreinterpretq_s16_u16(vmovl_u8(s[7]));

  return convolve8_8(ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7],
                     filters);
}

#endif  // VPX_VPX_DSP_ARM_VPX_CONVOLVE8_NEON_H_
