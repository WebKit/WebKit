/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/aom_convolve8_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "config/aom_dsp_rtcd.h"

static INLINE uint8x8_t convolve8_4_h(uint8x8_t s0, uint8x8_t s1, uint8x8_t s2,
                                      uint8x8_t s3, int8x8_t filter) {
  int8x16_t filter_x2 = vcombine_s8(filter, filter);

  uint8x16_t s01 = vcombine_u8(s0, s1);
  uint8x16_t s23 = vcombine_u8(s2, s3);

  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s01_128 = vreinterpretq_s8_u8(vsubq_u8(s01, vdupq_n_u8(128)));
  int8x16_t s23_128 = vreinterpretq_s8_u8(vsubq_u8(s23, vdupq_n_u8(128)));

  // Accumulate into 128 << (FILTER_BITS - 1) / 2 to account for range
  // transform.
  const int32x4_t acc = vdupq_n_s32((128 << (FILTER_BITS - 1)) / 2);
  int32x4_t sum01 = vdotq_s32(acc, s01_128, filter_x2);
  int32x4_t sum23 = vdotq_s32(acc, s23_128, filter_x2);

  int32x4_t sum0123 = vpaddq_s32(sum01, sum23);
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vdup_n_s16(0));

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE uint8x8_t convolve8_8_h(uint8x8_t s0, uint8x8_t s1, uint8x8_t s2,
                                      uint8x8_t s3, uint8x8_t s4, uint8x8_t s5,
                                      uint8x8_t s6, uint8x8_t s7,
                                      int8x8_t filter) {
  int8x16_t filter_x2 = vcombine_s8(filter, filter);

  uint8x16_t s01 = vcombine_u8(s0, s1);
  uint8x16_t s23 = vcombine_u8(s2, s3);
  uint8x16_t s45 = vcombine_u8(s4, s5);
  uint8x16_t s67 = vcombine_u8(s6, s7);

  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s01_128 = vreinterpretq_s8_u8(vsubq_u8(s01, vdupq_n_u8(128)));
  int8x16_t s23_128 = vreinterpretq_s8_u8(vsubq_u8(s23, vdupq_n_u8(128)));
  int8x16_t s45_128 = vreinterpretq_s8_u8(vsubq_u8(s45, vdupq_n_u8(128)));
  int8x16_t s67_128 = vreinterpretq_s8_u8(vsubq_u8(s67, vdupq_n_u8(128)));

  // Accumulate into 128 << (FILTER_BITS - 1) / 2 to account for range
  // transform.
  const int32x4_t acc = vdupq_n_s32((128 << (FILTER_BITS - 1)) / 2);
  int32x4_t sum01 = vdotq_s32(acc, s01_128, filter_x2);
  int32x4_t sum23 = vdotq_s32(acc, s23_128, filter_x2);
  int32x4_t sum45 = vdotq_s32(acc, s45_128, filter_x2);
  int32x4_t sum67 = vdotq_s32(acc, s67_128, filter_x2);

  int32x4_t sum0123 = vpaddq_s32(sum01, sum23);
  int32x4_t sum4567 = vpaddq_s32(sum45, sum67);
  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void scaled_convolve_horiz_neon_dotprod(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filter,
    const int x0_q4, const int x_step_q4, int w, int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);

  if (w == 4) {
    do {
      int x_q4 = x0_q4;

      // Process a 4x4 tile.
      for (int r = 0; r < 4; ++r) {
        // Halve filter values (all even) to avoid the need for saturating
        // arithmetic in convolution kernels.
        const int8x8_t filter =
            vshrn_n_s16(vld1q_s16(x_filter[x_q4 & SUBPEL_MASK]), 1);

        const uint8_t *s = &src[x_q4 >> SUBPEL_BITS];
        uint8x8_t s0, s1, s2, s3;
        load_u8_8x4(s, src_stride, &s0, &s1, &s2, &s3);

        uint8x8_t d0 = convolve8_4_h(s0, s1, s2, s3, filter);

        store_u8_4x1(&temp[4 * r], d0);

        x_q4 += x_step_q4;
      }

      // Transpose the 4x4 result tile and store.
      uint8x8_t d01 = vld1_u8(temp + 0);
      uint8x8_t d23 = vld1_u8(temp + 8);

      transpose_elems_inplace_u8_4x4(&d01, &d23);

      store_u8x4_strided_x2(dst + 0 * dst_stride, 2 * dst_stride, d01);
      store_u8x4_strided_x2(dst + 1 * dst_stride, 2 * dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
    return;
  }

  // w >= 8
  do {
    int x_q4 = x0_q4;
    uint8_t *d = dst;
    int width = w;

    do {
      // Process an 8x8 tile.
      for (int r = 0; r < 8; ++r) {
        // Halve filter values (all even) to avoid the need for saturating
        // arithmetic in convolution kernels.
        const int8x8_t filter =
            vshrn_n_s16(vld1q_s16(x_filter[x_q4 & SUBPEL_MASK]), 1);

        const uint8_t *s = &src[x_q4 >> SUBPEL_BITS];
        uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_u8_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        uint8x8_t d0 = convolve8_8_h(s0, s1, s2, s3, s4, s5, s6, s7, filter);

        vst1_u8(&temp[r * 8], d0);

        x_q4 += x_step_q4;
      }

      // Transpose the 8x8 result tile and store.
      uint8x8_t d0, d1, d2, d3, d4, d5, d6, d7;
      load_u8_8x8(temp, 8, &d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

      transpose_elems_inplace_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

      store_u8_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

      d += 8;
      width -= 8;
    } while (width != 0);

    src += 8 * src_stride;
    dst += 8 * dst_stride;
    h -= 8;
  } while (h > 0);
}

static INLINE uint8x8_t convolve8_4_v(uint8x8_t s0, uint8x8_t s1, uint8x8_t s2,
                                      uint8x8_t s3, uint8x8_t s4, uint8x8_t s5,
                                      uint8x8_t s6, uint8x8_t s7,
                                      int8x8_t filter) {
  uint8x16_t s01 = vcombine_u8(vzip1_u8(s0, s1), vdup_n_u8(0));
  uint8x16_t s23 = vcombine_u8(vzip1_u8(s2, s3), vdup_n_u8(0));
  uint8x16_t s45 = vcombine_u8(vzip1_u8(s4, s5), vdup_n_u8(0));
  uint8x16_t s67 = vcombine_u8(vzip1_u8(s6, s7), vdup_n_u8(0));

  uint8x16_t s0123 = vreinterpretq_u8_u16(
      vzip1q_u16(vreinterpretq_u16_u8(s01), vreinterpretq_u16_u8(s23)));
  uint8x16_t s4567 = vreinterpretq_u8_u16(
      vzip1q_u16(vreinterpretq_u16_u8(s45), vreinterpretq_u16_u8(s67)));

  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s0123_128 = vreinterpretq_s8_u8(vsubq_u8(s0123, vdupq_n_u8(128)));
  int8x16_t s4567_128 = vreinterpretq_s8_u8(vsubq_u8(s4567, vdupq_n_u8(128)));

  // Accumulate into 128 << (FILTER_BITS - 1) to account for range transform.
  int32x4_t sum = vdupq_n_s32(128 << (FILTER_BITS - 1));
  sum = vdotq_lane_s32(sum, s0123_128, filter, 0);
  sum = vdotq_lane_s32(sum, s4567_128, filter, 1);

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(vcombine_s16(vmovn_s32(sum), vdup_n_s16(0)),
                        FILTER_BITS - 1);
}

static INLINE uint8x8_t convolve8_8_v(uint8x8_t s0, uint8x8_t s1, uint8x8_t s2,
                                      uint8x8_t s3, uint8x8_t s4, uint8x8_t s5,
                                      uint8x8_t s6, uint8x8_t s7,
                                      int8x8_t filter) {
  uint8x16_t s01 =
      vzip1q_u8(vcombine_u8(s0, vdup_n_u8(0)), vcombine_u8(s1, vdup_n_u8(0)));
  uint8x16_t s23 =
      vzip1q_u8(vcombine_u8(s2, vdup_n_u8(0)), vcombine_u8(s3, vdup_n_u8(0)));
  uint8x16_t s45 =
      vzip1q_u8(vcombine_u8(s4, vdup_n_u8(0)), vcombine_u8(s5, vdup_n_u8(0)));
  uint8x16_t s67 =
      vzip1q_u8(vcombine_u8(s6, vdup_n_u8(0)), vcombine_u8(s7, vdup_n_u8(0)));

  uint8x16_t s0123[2] = {
    vreinterpretq_u8_u16(
        vzip1q_u16(vreinterpretq_u16_u8(s01), vreinterpretq_u16_u8(s23))),
    vreinterpretq_u8_u16(
        vzip2q_u16(vreinterpretq_u16_u8(s01), vreinterpretq_u16_u8(s23)))
  };
  uint8x16_t s4567[2] = {
    vreinterpretq_u8_u16(
        vzip1q_u16(vreinterpretq_u16_u8(s45), vreinterpretq_u16_u8(s67))),
    vreinterpretq_u8_u16(
        vzip2q_u16(vreinterpretq_u16_u8(s45), vreinterpretq_u16_u8(s67)))
  };

  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s0123_128[2] = {
    vreinterpretq_s8_u8(vsubq_u8(s0123[0], vdupq_n_u8(128))),
    vreinterpretq_s8_u8(vsubq_u8(s0123[1], vdupq_n_u8(128)))
  };
  int8x16_t s4567_128[2] = {
    vreinterpretq_s8_u8(vsubq_u8(s4567[0], vdupq_n_u8(128))),
    vreinterpretq_s8_u8(vsubq_u8(s4567[1], vdupq_n_u8(128)))
  };

  // Accumulate into 128 << (FILTER_BITS - 1) to account for range transform.
  const int32x4_t acc = vdupq_n_s32(128 << (FILTER_BITS - 1));

  int32x4_t sum0123 = vdotq_lane_s32(acc, s0123_128[0], filter, 0);
  sum0123 = vdotq_lane_s32(sum0123, s4567_128[0], filter, 1);

  int32x4_t sum4567 = vdotq_lane_s32(acc, s0123_128[1], filter, 0);
  sum4567 = vdotq_lane_s32(sum4567, s4567_128[1], filter, 1);

  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));
  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void scaled_convolve_vert_neon_dotprod(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filter,
    const int y0_q4, const int y_step_q4, int w, int h) {
  int y_q4 = y0_q4;

  if (w == 4) {
    do {
      const uint8_t *s = &src[(y_q4 >> SUBPEL_BITS) * src_stride];

      if (y_q4 & SUBPEL_MASK) {
        // Halve filter values (all even) to avoid the need for saturating
        // arithmetic in convolution kernels.
        const int8x8_t filter =
            vshrn_n_s16(vld1q_s16(y_filter[y_q4 & SUBPEL_MASK]), 1);

        uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_u8_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        uint8x8_t d0 = convolve8_4_v(s0, s1, s2, s3, s4, s5, s6, s7, filter);

        store_u8_4x1(dst, d0);
      } else {
        // Memcpy for non-subpel locations.
        memcpy(dst, &s[(SUBPEL_TAPS / 2 - 1) * src_stride], 4);
      }

      y_q4 += y_step_q4;
      dst += dst_stride;
    } while (--h != 0);
    return;
  }

  // w >= 8
  do {
    const uint8_t *s = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    uint8_t *d = dst;
    int width = w;

    if (y_q4 & SUBPEL_MASK) {
      // Halve filter values (all even) to avoid the need for saturating
      // arithmetic in convolution kernels.
      const int8x8_t filter =
          vshrn_n_s16(vld1q_s16(y_filter[y_q4 & SUBPEL_MASK]), 1);

      do {
        uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_u8_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        uint8x8_t d0 = convolve8_8_v(s0, s1, s2, s3, s4, s5, s6, s7, filter);

        vst1_u8(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
    } else {
      // Memcpy for non-subpel locations.
      s += (SUBPEL_TAPS / 2 - 1) * src_stride;

      do {
        uint8x8_t s0 = vld1_u8(s);
        vst1_u8(d, s0);
        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
    }

    y_q4 += y_step_q4;
    dst += dst_stride;
  } while (--h != 0);
}

void aom_scaled_2d_neon_dotprod(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride,
                                const InterpKernel *filter, int x0_q4,
                                int x_step_q4, int y0_q4, int y_step_q4, int w,
                                int h) {
  // Fixed size intermediate buffer, im_block, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the im_block buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // --Require an additional 8 rows for the horiz_w8 transpose tail.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  DECLARE_ALIGNED(16, uint8_t, im_block[(135 + 8) * 64]);
  const int im_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;
  const ptrdiff_t im_stride = 64;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  // Account for needing SUBPEL_TAPS / 2 - 1 lines prior and SUBPEL_TAPS / 2
  // lines post both horizontally and vertically.
  const ptrdiff_t horiz_offset = SUBPEL_TAPS / 2 - 1;
  const ptrdiff_t vert_offset = (SUBPEL_TAPS / 2 - 1) * src_stride;

  scaled_convolve_horiz_neon_dotprod(src - horiz_offset - vert_offset,
                                     src_stride, im_block, im_stride, filter,
                                     x0_q4, x_step_q4, w, im_height);

  scaled_convolve_vert_neon_dotprod(im_block, im_stride, dst, dst_stride,
                                    filter, y0_q4, y_step_q4, w, h);
}
