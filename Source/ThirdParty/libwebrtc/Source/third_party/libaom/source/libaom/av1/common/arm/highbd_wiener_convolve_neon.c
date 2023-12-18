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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/convolve.h"
#include "av1/common/arm/highbd_convolve_neon.h"

static void highbd_convolve_add_src_horiz_hip(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, const int16_t *x_filter_ptr, int x_step_q4, int w,
    int h, int round0_bits, int bd) {
  const int extraprec_clamp_limit = WIENER_CLAMP_LIMIT(round0_bits, bd);

  static const int32_t kIdx[4] = { 0, 1, 2, 3 };
  const int32x4_t idx = vld1q_s32(kIdx);
  const int32x4_t shift_s32 = vdupq_n_s32(-round0_bits);
  const uint16x4_t max = vdup_n_u16(extraprec_clamp_limit - 1);
  const int32x4_t rounding0 = vdupq_n_s32(1 << (bd + FILTER_BITS - 1));

  int height = h;
  do {
    int width = w;
    int x_q4 = 0;
    uint16_t *d = dst_ptr;
    const uint16_t *s = src_ptr;

    do {
      // Load 4 src vectors at a time, they might be the same, but we have to
      // calculate the indices anyway. Doing it in SIMD and then storing the
      // indices is faster than having to calculate the expression
      // &src_ptr[((x_q4 + i*x_step_q4) >> SUBPEL_BITS)] 4 times
      // Ideally this should be a gather using the indices, but NEON does not
      // have that, so have to emulate
      const int32x4_t xq4_idx = vmlaq_n_s32(vdupq_n_s32(x_q4), idx, x_step_q4);
      // We have to multiply x2 to get the actual pointer as sizeof(uint16_t)
      // = 2
      const int32x4_t src_idx =
          vshlq_n_s32(vshrq_n_s32(xq4_idx, SUBPEL_BITS), 1);

#if AOM_ARCH_AARCH64
      uint64x2_t tmp4[2];
      tmp4[0] = vreinterpretq_u64_s64(
          vaddw_s32(vdupq_n_s64((const int64_t)s), vget_low_s32(src_idx)));
      tmp4[1] = vreinterpretq_u64_s64(
          vaddw_s32(vdupq_n_s64((const int64_t)s), vget_high_s32(src_idx)));
      int16_t *src4_ptr[4];
      uint64_t *tmp_ptr = (uint64_t *)&src4_ptr;
      vst1q_u64(tmp_ptr, tmp4[0]);
      vst1q_u64(tmp_ptr + 2, tmp4[1]);
#else
      uint32x4_t tmp4;
      tmp4 = vreinterpretq_u32_s32(
          vaddq_s32(vdupq_n_s32((const int32_t)s), src_idx));
      int16_t *src4_ptr[4];
      uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
      vst1q_u32(tmp_ptr, tmp4);
#endif  // AOM_ARCH_AARCH64
      // Load source
      int16x8_t s0 = vld1q_s16(src4_ptr[0]);
      int16x8_t s1 = vld1q_s16(src4_ptr[1]);
      int16x8_t s2 = vld1q_s16(src4_ptr[2]);
      int16x8_t s3 = vld1q_s16(src4_ptr[3]);

      // Actually load the filters
      const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

      const int16_t *rounding_ptr = (const int16_t *)src4_ptr[0];
      int16x4_t rounding_s16 = vld1_s16(&rounding_ptr[SUBPEL_TAPS / 2 - 1]);
      int32x4_t rounding = vshlq_n_s32(vmovl_s16(rounding_s16), FILTER_BITS);
      rounding = vaddq_s32(rounding, rounding0);

      uint16x4_t d0 = highbd_convolve8_horiz4x8_s32_s16(
          s0, s1, s2, s3, x_filter, shift_s32, rounding);
      d0 = vmin_u16(d0, max);
      vst1_u16(d, d0);

      x_q4 += 4 * x_step_q4;
      d += 4;
      width -= 4;
    } while (width > 0);

    src_ptr += src_stride;
    dst_ptr += dst_stride;
    height--;
  } while (height > 0);
}

static void highbd_convolve_add_src_vert_hip(
    const uint16_t *src_ptr, ptrdiff_t src_stride, uint16_t *dst_ptr,
    ptrdiff_t dst_stride, const int16_t *y_filter_ptr, int y_step_q4, int w,
    int h, int round1_bits, int bd) {
  static const int32_t kIdx[4] = { 0, 1, 2, 3 };
  const int32x4_t idx = vld1q_s32(kIdx);
  const int32x4_t shift_s32 = vdupq_n_s32(-round1_bits);
  const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
  const int32x4_t rounding0 = vdupq_n_s32(1 << (bd + round1_bits - 1));

  int width = w;
  do {
    int height = h;
    int y_q4 = 0;
    uint16_t *d = dst_ptr;
    const uint16_t *s = src_ptr;

    do {
      // Load 4 src vectors at a time, they might be the same, but we have to
      // calculate the indices anyway. Doing it in SIMD and then storing the
      // indices is faster than having to calculate the expression
      // &src_ptr[((x_q4 + i*x_step_q4) >> SUBPEL_BITS)] 4 times
      // Ideally this should be a gather using the indices, but NEON does not
      // have that, so have to emulate
      const int32x4_t yq4_idx = vmlaq_n_s32(vdupq_n_s32(y_q4), idx, y_step_q4);
      // We have to multiply x2 to get the actual pointer as sizeof(uint16_t)
      // = 2
      const int32x4_t src_idx =
          vshlq_n_s32(vshrq_n_s32(yq4_idx, SUBPEL_BITS), 1);
#if AOM_ARCH_AARCH64
      uint64x2_t tmp4[2];
      tmp4[0] = vreinterpretq_u64_s64(
          vaddw_s32(vdupq_n_s64((const int64_t)s), vget_low_s32(src_idx)));
      tmp4[1] = vreinterpretq_u64_s64(
          vaddw_s32(vdupq_n_s64((const int64_t)s), vget_high_s32(src_idx)));
      const int16_t *src4_ptr[4];
      uint64_t *tmp_ptr = (uint64_t *)&src4_ptr;
      vst1q_u64(tmp_ptr, tmp4[0]);
      vst1q_u64(tmp_ptr + 2, tmp4[1]);
#else
      uint32x4_t tmp4;
      tmp4 = vreinterpretq_u32_s32(
          vaddq_s32(vdupq_n_s32((const int32_t)s), src_idx));
      int16_t *src4_ptr[4];
      uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
      vst1q_u32(tmp_ptr, tmp4);
#endif  // AOM_ARCH_AARCH64

      // Load source
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(src4_ptr[0], src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6,
                   &s7);

      // Actually load the filters
      const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

      const int16_t *rounding_ptr = (const int16_t *)src4_ptr[0];
      int16x4_t rounding_s16 =
          vld1_s16(&rounding_ptr[(SUBPEL_TAPS / 2 - 1) * src_stride]);
      int32x4_t rounding = vshlq_n_s32(vmovl_s16(rounding_s16), FILTER_BITS);
      rounding = vsubq_s32(rounding, rounding0);

      // Run the convolution
      uint16x4_t d0 = highbd_convolve8_4_sr_s32_s16(
          s0, s1, s2, s3, s4, s5, s6, s7, y_filter, shift_s32, rounding);
      d0 = vmin_u16(d0, max);
      vst1_u16(d, d0);

      s += src_stride;
      d += dst_stride;
      height--;
    } while (height > 0);

    y_q4 += 4 * y_step_q4;
    src_ptr += 4;
    dst_ptr += 4;
    width -= 4;
  } while (width > 0);
}

#define WIENER_MAX_EXT_SIZE 263

void av1_highbd_wiener_convolve_add_src_neon(
    const uint8_t *src8, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const int16_t *x_filter_ptr, int x_step_q4,
    const int16_t *y_filter_ptr, int y_step_q4, int w, int h,
    const ConvolveParams *conv_params, int bd) {
  assert(x_step_q4 == 16 && y_step_q4 == 16);

  DECLARE_ALIGNED(16, uint16_t, im_block[WIENER_MAX_EXT_SIZE * MAX_SB_SIZE]);
  const int im_h = (((h - 1) * y_step_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = SUBPEL_TAPS / 2 - 1;
  const int horiz_offset = SUBPEL_TAPS / 2 - 1;

  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  highbd_convolve_add_src_horiz_hip(src_ptr, src_stride, im_block, im_stride,
                                    x_filter_ptr, x_step_q4, w, im_h,
                                    conv_params->round_0, bd);
  highbd_convolve_add_src_vert_hip(im_block, im_stride, dst, dst_stride,
                                   y_filter_ptr, y_step_q4, w, h,
                                   conv_params->round_1, bd);
}
