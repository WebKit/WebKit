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
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/highbd_convolve_neon.h"

#define UPSCALE_NORMATIVE_TAPS 8

void av1_highbd_convolve_horiz_rs_neon(const uint16_t *src, int src_stride,
                                       uint16_t *dst, int dst_stride, int w,
                                       int h, const int16_t *x_filters,
                                       int x0_qn, int x_step_qn, int bd) {
  const int horiz_offset = UPSCALE_NORMATIVE_TAPS / 2 - 1;

  static const int32_t kIdx[4] = { 0, 1, 2, 3 };
  const int32x4_t idx = vld1q_s32(kIdx);
  const int32x4_t subpel_mask = vdupq_n_s32(RS_SCALE_SUBPEL_MASK);
  const int32x4_t shift_s32 = vdupq_n_s32(-FILTER_BITS);
  const int32x4_t offset_s32 = vdupq_n_s32(0);
  const uint16x4_t max = vdup_n_u16((1 << bd) - 1);

  const uint16_t *src_ptr = src - horiz_offset;
  uint16_t *dst_ptr = dst;

  if (w <= 4) {
    int height = h;
    uint16_t *d = dst_ptr;

    do {
      int x_qn = x0_qn;

      // Load 4 src vectors at a time, they might be the same, but we have to
      // calculate the indices anyway. Doing it in SIMD and then storing the
      // indices is faster than having to calculate the expression
      // &src_ptr[((x_qn + 0*x_step_qn) >> RS_SCALE_SUBPEL_BITS)] 4 times
      // Ideally this should be a gather using the indices, but NEON does not
      // have that, so have to emulate
      const int32x4_t xqn_idx = vmlaq_n_s32(vdupq_n_s32(x_qn), idx, x_step_qn);
      // We have to multiply x2 to get the actual pointer as sizeof(uint16_t) =
      // 2
      const int32x4_t src_idx =
          vshlq_n_s32(vshrq_n_s32(xqn_idx, RS_SCALE_SUBPEL_BITS), 1);
      // Similarly for the filter vector indices, we calculate the filter
      // indices for 4 columns. First we calculate the indices:
      // x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS
      // Then we calculate the actual pointers, multiplying with
      // UPSCALE_UPSCALE_NORMATIVE_TAPS
      // again shift left by 1
      const int32x4_t x_filter4_idx = vshlq_n_s32(
          vshrq_n_s32(vandq_s32(xqn_idx, subpel_mask), RS_SCALE_EXTRA_BITS), 1);
      // Even though pointers are unsigned 32/64-bit ints we do signed
      // addition The reason for this is that x_qn can be negative, leading to
      // negative offsets. Argon test
      // profile0_core/streams/test10573_11003.obu was failing because of
      // this.
#if AOM_ARCH_AARCH64
      uint64x2_t tmp4[2];
      tmp4[0] = vreinterpretq_u64_s64(vaddw_s32(
          vdupq_n_s64((const int64_t)src_ptr), vget_low_s32(src_idx)));
      tmp4[1] = vreinterpretq_u64_s64(vaddw_s32(
          vdupq_n_s64((const int64_t)src_ptr), vget_high_s32(src_idx)));
      int16_t *src4_ptr[4];
      uint64_t *tmp_ptr = (uint64_t *)&src4_ptr;
      vst1q_u64(tmp_ptr, tmp4[0]);
      vst1q_u64(tmp_ptr + 2, tmp4[1]);

      // filter vectors
      tmp4[0] = vreinterpretq_u64_s64(vmlal_s32(
          vdupq_n_s64((const int64_t)x_filters), vget_low_s32(x_filter4_idx),
          vdup_n_s32(UPSCALE_NORMATIVE_TAPS)));
      tmp4[1] = vreinterpretq_u64_s64(vmlal_s32(
          vdupq_n_s64((const int64_t)x_filters), vget_high_s32(x_filter4_idx),
          vdup_n_s32(UPSCALE_NORMATIVE_TAPS)));

      const int16_t *x_filter4_ptr[4];
      tmp_ptr = (uint64_t *)&x_filter4_ptr;
      vst1q_u64(tmp_ptr, tmp4[0]);
      vst1q_u64(tmp_ptr + 2, tmp4[1]);
#else
      uint32x4_t tmp4;
      tmp4 = vreinterpretq_u32_s32(
          vaddq_s32(vdupq_n_s32((const int32_t)src_ptr), src_idx));
      int16_t *src4_ptr[4];
      uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
      vst1q_u32(tmp_ptr, tmp4);

      // filter vectors
      tmp4 = vreinterpretq_u32_s32(
          vmlaq_s32(vdupq_n_s32((const int32_t)x_filters), x_filter4_idx,
                    vdupq_n_s32(UPSCALE_NORMATIVE_TAPS)));

      const int16_t *x_filter4_ptr[4];
      tmp_ptr = (uint32_t *)&x_filter4_ptr;
      vst1q_u32(tmp_ptr, tmp4);
#endif  // AOM_ARCH_AARCH64
      // Load source
      int16x8_t s0 = vld1q_s16(src4_ptr[0]);
      int16x8_t s1 = vld1q_s16(src4_ptr[1]);
      int16x8_t s2 = vld1q_s16(src4_ptr[2]);
      int16x8_t s3 = vld1q_s16(src4_ptr[3]);

      // Actually load the filters
      const int16x8_t x_filter0 = vld1q_s16(x_filter4_ptr[0]);
      const int16x8_t x_filter1 = vld1q_s16(x_filter4_ptr[1]);
      const int16x8_t x_filter2 = vld1q_s16(x_filter4_ptr[2]);
      const int16x8_t x_filter3 = vld1q_s16(x_filter4_ptr[3]);

      // Group low and high parts and transpose
      int16x4_t filters_lo[] = { vget_low_s16(x_filter0),
                                 vget_low_s16(x_filter1),
                                 vget_low_s16(x_filter2),
                                 vget_low_s16(x_filter3) };
      int16x4_t filters_hi[] = { vget_high_s16(x_filter0),
                                 vget_high_s16(x_filter1),
                                 vget_high_s16(x_filter2),
                                 vget_high_s16(x_filter3) };
      transpose_array_inplace_u16_4x4((uint16x4_t *)filters_lo);
      transpose_array_inplace_u16_4x4((uint16x4_t *)filters_hi);

      // Run the 2D Scale convolution
      uint16x4_t d0 = highbd_convolve8_2d_scale_horiz4x8_s32_s16(
          s0, s1, s2, s3, filters_lo, filters_hi, shift_s32, offset_s32);

      d0 = vmin_u16(d0, max);

      if (w == 2) {
        store_u16_2x1(d, d0);
      } else {
        vst1_u16(d, d0);
      }

      src_ptr += src_stride;
      d += dst_stride;
      height--;
    } while (height > 0);
  } else {
    int height = h;

    do {
      int width = w;
      int x_qn = x0_qn;
      uint16_t *d = dst_ptr;
      const uint16_t *s = src_ptr;

      do {
        // Load 4 src vectors at a time, they might be the same, but we have to
        // calculate the indices anyway. Doing it in SIMD and then storing the
        // indices is faster than having to calculate the expression
        // &src_ptr[((x_qn + 0*x_step_qn) >> RS_SCALE_SUBPEL_BITS)] 4 times
        // Ideally this should be a gather using the indices, but NEON does not
        // have that, so have to emulate
        const int32x4_t xqn_idx =
            vmlaq_n_s32(vdupq_n_s32(x_qn), idx, x_step_qn);
        // We have to multiply x2 to get the actual pointer as sizeof(uint16_t)
        // = 2
        const int32x4_t src_idx =
            vshlq_n_s32(vshrq_n_s32(xqn_idx, RS_SCALE_SUBPEL_BITS), 1);

        // Similarly for the filter vector indices, we calculate the filter
        // indices for 4 columns. First we calculate the indices:
        // x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS
        // Then we calculate the actual pointers, multiplying with
        // UPSCALE_UPSCALE_NORMATIVE_TAPS
        // again shift left by 1
        const int32x4_t x_filter4_idx = vshlq_n_s32(
            vshrq_n_s32(vandq_s32(xqn_idx, subpel_mask), RS_SCALE_EXTRA_BITS),
            1);
        // Even though pointers are unsigned 32/64-bit ints we do signed
        // addition The reason for this is that x_qn can be negative, leading to
        // negative offsets. Argon test
        // profile0_core/streams/test10573_11003.obu was failing because of
        // this.
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

        // filter vectors
        tmp4[0] = vreinterpretq_u64_s64(vmlal_s32(
            vdupq_n_s64((const int64_t)x_filters), vget_low_s32(x_filter4_idx),
            vdup_n_s32(UPSCALE_NORMATIVE_TAPS)));
        tmp4[1] = vreinterpretq_u64_s64(vmlal_s32(
            vdupq_n_s64((const int64_t)x_filters), vget_high_s32(x_filter4_idx),
            vdup_n_s32(UPSCALE_NORMATIVE_TAPS)));

        const int16_t *x_filter4_ptr[4];
        tmp_ptr = (uint64_t *)&x_filter4_ptr;
        vst1q_u64(tmp_ptr, tmp4[0]);
        vst1q_u64(tmp_ptr + 2, tmp4[1]);
#else
        uint32x4_t tmp4;
        tmp4 = vreinterpretq_u32_s32(
            vaddq_s32(vdupq_n_s32((const int32_t)s), src_idx));
        int16_t *src4_ptr[4];
        uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
        vst1q_u32(tmp_ptr, tmp4);

        // filter vectors
        tmp4 = vreinterpretq_u32_s32(
            vmlaq_s32(vdupq_n_s32((const int32_t)x_filters), x_filter4_idx,
                      vdupq_n_s32(UPSCALE_NORMATIVE_TAPS)));

        const int16_t *x_filter4_ptr[4];
        tmp_ptr = (uint32_t *)&x_filter4_ptr;
        vst1q_u32(tmp_ptr, tmp4);
#endif  // AOM_ARCH_AARCH64

        // Load source
        int16x8_t s0 = vld1q_s16(src4_ptr[0]);
        int16x8_t s1 = vld1q_s16(src4_ptr[1]);
        int16x8_t s2 = vld1q_s16(src4_ptr[2]);
        int16x8_t s3 = vld1q_s16(src4_ptr[3]);

        // Actually load the filters
        const int16x8_t x_filter0 = vld1q_s16(x_filter4_ptr[0]);
        const int16x8_t x_filter1 = vld1q_s16(x_filter4_ptr[1]);
        const int16x8_t x_filter2 = vld1q_s16(x_filter4_ptr[2]);
        const int16x8_t x_filter3 = vld1q_s16(x_filter4_ptr[3]);

        // Group low and high parts and transpose
        int16x4_t filters_lo[] = { vget_low_s16(x_filter0),
                                   vget_low_s16(x_filter1),
                                   vget_low_s16(x_filter2),
                                   vget_low_s16(x_filter3) };
        int16x4_t filters_hi[] = { vget_high_s16(x_filter0),
                                   vget_high_s16(x_filter1),
                                   vget_high_s16(x_filter2),
                                   vget_high_s16(x_filter3) };
        transpose_array_inplace_u16_4x4((uint16x4_t *)filters_lo);
        transpose_array_inplace_u16_4x4((uint16x4_t *)filters_hi);

        // Run the 2D Scale X convolution
        uint16x4_t d0 = highbd_convolve8_2d_scale_horiz4x8_s32_s16(
            s0, s1, s2, s3, filters_lo, filters_hi, shift_s32, offset_s32);

        d0 = vmin_u16(d0, max);
        vst1_u16(d, d0);

        x_qn += 4 * x_step_qn;
        d += 4;
        width -= 4;
      } while (width > 0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      height--;
    } while (height > 0);
  }
}
