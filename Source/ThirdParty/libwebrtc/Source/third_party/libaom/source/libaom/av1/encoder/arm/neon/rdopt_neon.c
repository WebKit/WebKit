/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include <arm_neon.h>

#include "av1/encoder/rdopt.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

// Process horizontal and vertical correlations in a 4x4 block of pixels.
// We actually use the 4x4 pixels to calculate correlations corresponding to
// the top-left 3x3 pixels, so this function must be called with 1x1 overlap,
// moving the window along/down by 3 pixels at a time.
INLINE static void horver_correlation_4x4(const int16_t *diff, int stride,
                                          int32x4_t *xy_sum_32,
                                          int32x4_t *xz_sum_32,
                                          int32x4_t *x_sum_32,
                                          int32x4_t *x2_sum_32) {
  // Pixels in this 4x4   [ a b c d ]
  // are referred to as:  [ e f g h ]
  //                      [ i j k l ]
  //                      [ m n o p ]

  const int16x4_t pixelsa_2_lo = vld1_s16(diff + (0 * stride));
  const int16x4_t pixelsa_2_sli =
      vreinterpret_s16_s64(vshl_n_s64(vreinterpret_s64_s16(pixelsa_2_lo), 16));
  const int16x4_t pixelsb_2_lo = vld1_s16(diff + (1 * stride));
  const int16x4_t pixelsb_2_sli =
      vreinterpret_s16_s64(vshl_n_s64(vreinterpret_s64_s16(pixelsb_2_lo), 16));
  const int16x4_t pixelsa_1_lo = vld1_s16(diff + (2 * stride));
  const int16x4_t pixelsa_1_sli =
      vreinterpret_s16_s64(vshl_n_s64(vreinterpret_s64_s16(pixelsa_1_lo), 16));
  const int16x4_t pixelsb_1_lo = vld1_s16(diff + (3 * stride));
  const int16x4_t pixelsb_1_sli =
      vreinterpret_s16_s64(vshl_n_s64(vreinterpret_s64_s16(pixelsb_1_lo), 16));

  const int16x8_t slli_a = vcombine_s16(pixelsa_1_sli, pixelsa_2_sli);

  *xy_sum_32 = vmlal_s16(*xy_sum_32, pixelsa_1_lo, pixelsa_1_sli);
  *xy_sum_32 = vmlal_s16(*xy_sum_32, pixelsa_2_lo, pixelsa_2_sli);
  *xy_sum_32 = vmlal_s16(*xy_sum_32, pixelsb_2_lo, pixelsb_2_sli);

  *xz_sum_32 = vmlal_s16(*xz_sum_32, pixelsa_1_sli, pixelsb_1_sli);
  *xz_sum_32 = vmlal_s16(*xz_sum_32, pixelsa_2_sli, pixelsb_2_sli);
  *xz_sum_32 = vmlal_s16(*xz_sum_32, pixelsa_1_sli, pixelsb_2_sli);

  // Now calculate the straight sums, x_sum += a+b+c+e+f+g+i+j+k
  // (sum up every element in slli_a and swap_b)
  *x_sum_32 = vpadalq_s16(*x_sum_32, slli_a);
  *x_sum_32 = vaddw_s16(*x_sum_32, pixelsb_2_sli);

  // Also sum their squares
  *x2_sum_32 = vmlal_s16(*x2_sum_32, pixelsa_1_sli, pixelsa_1_sli);
  *x2_sum_32 = vmlal_s16(*x2_sum_32, pixelsa_2_sli, pixelsa_2_sli);
  *x2_sum_32 = vmlal_s16(*x2_sum_32, pixelsb_2_sli, pixelsb_2_sli);
}

void av1_get_horver_correlation_full_neon(const int16_t *diff, int stride,
                                          int width, int height, float *hcorr,
                                          float *vcorr) {
  // The following notation is used:
  // x - current pixel
  // y - right neighbour pixel
  // z - below neighbour pixel
  // w - down-right neighbour pixel
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t x_sum = 0, x2_sum = 0;
  int32x4_t zero = vdupq_n_s32(0);
  int64x2_t v_x_sum = vreinterpretq_s64_s32(zero);
  int64x2_t v_xy_sum = vreinterpretq_s64_s32(zero);
  int64x2_t v_xz_sum = vreinterpretq_s64_s32(zero);
  int64x2_t v_x2_sum = vreinterpretq_s64_s32(zero);
  // Process horizontal and vertical correlations through the body in 4x4
  // blocks.  This excludes the final row and column and possibly one extra
  // column depending how 3 divides into width and height

  for (int i = 0; i <= height - 4; i += 3) {
    int32x4_t xy_sum_32 = zero;
    int32x4_t xz_sum_32 = zero;
    int32x4_t x_sum_32 = zero;
    int32x4_t x2_sum_32 = zero;
    for (int j = 0; j <= width - 4; j += 3) {
      horver_correlation_4x4(&diff[i * stride + j], stride, &xy_sum_32,
                             &xz_sum_32, &x_sum_32, &x2_sum_32);
    }
    v_xy_sum = vpadalq_s32(v_xy_sum, xy_sum_32);
    v_xz_sum = vpadalq_s32(v_xz_sum, xz_sum_32);
    v_x_sum = vpadalq_s32(v_x_sum, x_sum_32);
    v_x2_sum = vpadalq_s32(v_x2_sum, x2_sum_32);
  }
#if AOM_ARCH_AARCH64
  xy_sum = vaddvq_s64(v_xy_sum);
  xz_sum = vaddvq_s64(v_xz_sum);
  x2_sum = vaddvq_s64(v_x2_sum);
  x_sum = vaddvq_s64(v_x_sum);
#else
  xy_sum = vget_lane_s64(
      vadd_s64(vget_low_s64(v_xy_sum), vget_high_s64(v_xy_sum)), 0);
  xz_sum = vget_lane_s64(
      vadd_s64(vget_low_s64(v_xz_sum), vget_high_s64(v_xz_sum)), 0);
  x2_sum = vget_lane_s64(
      vadd_s64(vget_low_s64(v_x2_sum), vget_high_s64(v_x2_sum)), 0);
  x_sum =
      vget_lane_s64(vadd_s64(vget_low_s64(v_x_sum), vget_high_s64(v_x_sum)), 0);
#endif
  // x_sum now covers every pixel except the final 1-2 rows and 1-2 cols
  int64_t x_finalrow = 0, x_finalcol = 0, x2_finalrow = 0, x2_finalcol = 0;

  // Do we have 2 rows remaining or just the one?  Note that width and height
  // are powers of 2, so each modulo 3 must be 1 or 2.
  if (height % 3 == 1) {  // Just horiz corrs on the final row
    const int16_t x0 = diff[(height - 1) * stride];
    x_sum += x0;
    x_finalrow += x0;
    x2_sum += x0 * x0;
    x2_finalrow += x0 * x0;
    if (width >= 8) {
      int32x4_t v_y_sum = zero;
      int32x4_t v_y2_sum = zero;
      int32x4_t v_xy_sum_a = zero;
      int k = width - 1;
      int j = 0;
      while ((k - 8) > 0) {
        const int16x8_t v_x = vld1q_s16(&diff[(height - 1) * stride + j]);
        const int16x8_t v_y = vld1q_s16(&diff[(height - 1) * stride + j + 1]);
        const int16x4_t v_x_lo = vget_low_s16(v_x);
        const int16x4_t v_x_hi = vget_high_s16(v_x);
        const int16x4_t v_y_lo = vget_low_s16(v_y);
        const int16x4_t v_y_hi = vget_high_s16(v_y);
        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_lo, v_y_lo);
        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_hi, v_y_hi);
        v_y2_sum = vmlal_s16(v_y2_sum, v_y_lo, v_y_lo);
        v_y2_sum = vmlal_s16(v_y2_sum, v_y_hi, v_y_hi);
        v_y_sum = vpadalq_s16(v_y_sum, v_y);
        k -= 8;
        j += 8;
      }

      const int16x8_t v_l = vld1q_s16(&diff[(height - 1) * stride] + j);
      const int16x8_t v_x =
          vextq_s16(vextq_s16(vreinterpretq_s16_s32(zero), v_l, 7),
                    vreinterpretq_s16_s32(zero), 1);
      const int16x8_t v_y = vextq_s16(v_l, vreinterpretq_s16_s32(zero), 1);
      const int16x4_t v_x_lo = vget_low_s16(v_x);
      const int16x4_t v_x_hi = vget_high_s16(v_x);
      const int16x4_t v_y_lo = vget_low_s16(v_y);
      const int16x4_t v_y_hi = vget_high_s16(v_y);
      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_lo, v_y_lo);
      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_hi, v_y_hi);
      v_y2_sum = vmlal_s16(v_y2_sum, v_y_lo, v_y_lo);
      v_y2_sum = vmlal_s16(v_y2_sum, v_y_hi, v_y_hi);
      const int32x4_t v_y_sum_a = vpadalq_s16(v_y_sum, v_y);
      const int64x2_t v_xy_sum2 = vpaddlq_s32(v_xy_sum_a);
#if AOM_ARCH_AARCH64
      const int64x2_t v_y2_sum_a = vpaddlq_s32(v_y2_sum);
      xy_sum += vaddvq_s64(v_xy_sum2);
      const int32_t y = vaddvq_s32(v_y_sum_a);
      const int64_t y2 = vaddvq_s64(v_y2_sum_a);
#else
      xy_sum += vget_lane_s64(
          vadd_s64(vget_low_s64(v_xy_sum2), vget_high_s64(v_xy_sum2)), 0);
      const int64x2_t v_y_a = vpaddlq_s32(v_y_sum_a);
      const int64_t y =
          vget_lane_s64(vadd_s64(vget_low_s64(v_y_a), vget_high_s64(v_y_a)), 0);
      const int64x2_t v_y2_sum_b = vpaddlq_s32(v_y2_sum);
      int64_t y2 = vget_lane_s64(
          vadd_s64(vget_low_s64(v_y2_sum_b), vget_high_s64(v_y2_sum_b)), 0);
#endif
      x_sum += y;
      x2_sum += y2;
      x_finalrow += y;
      x2_finalrow += y2;
    } else {
      for (int j = 0; j < width - 1; ++j) {
        const int16_t x = diff[(height - 1) * stride + j];
        const int16_t y = diff[(height - 1) * stride + j + 1];
        xy_sum += x * y;
        x_sum += y;
        x2_sum += y * y;
        x_finalrow += y;
        x2_finalrow += y * y;
      }
    }
  } else {  // Two rows remaining to do
    const int16_t x0 = diff[(height - 2) * stride];
    const int16_t z0 = diff[(height - 1) * stride];
    x_sum += x0 + z0;
    x2_sum += x0 * x0 + z0 * z0;
    x_finalrow += z0;
    x2_finalrow += z0 * z0;
    if (width >= 8) {
      int32x4_t v_y2_sum = zero;
      int32x4_t v_w2_sum = zero;
      int32x4_t v_xy_sum_a = zero;
      int32x4_t v_xz_sum_a = zero;
      int32x4_t v_x_sum_a = zero;
      int32x4_t v_w_sum = zero;
      int k = width - 1;
      int j = 0;
      while ((k - 8) > 0) {
        const int16x8_t v_x = vld1q_s16(&diff[(height - 2) * stride + j]);
        const int16x8_t v_y = vld1q_s16(&diff[(height - 2) * stride + j + 1]);
        const int16x8_t v_z = vld1q_s16(&diff[(height - 1) * stride + j]);
        const int16x8_t v_w = vld1q_s16(&diff[(height - 1) * stride + j + 1]);

        const int16x4_t v_x_lo = vget_low_s16(v_x);
        const int16x4_t v_y_lo = vget_low_s16(v_y);
        const int16x4_t v_z_lo = vget_low_s16(v_z);
        const int16x4_t v_w_lo = vget_low_s16(v_w);
        const int16x4_t v_x_hi = vget_high_s16(v_x);
        const int16x4_t v_y_hi = vget_high_s16(v_y);
        const int16x4_t v_z_hi = vget_high_s16(v_z);
        const int16x4_t v_w_hi = vget_high_s16(v_w);

        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_lo, v_y_lo);
        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_hi, v_y_hi);
        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_z_lo, v_w_lo);
        v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_z_hi, v_w_hi);

        v_xz_sum_a = vmlal_s16(v_xz_sum_a, v_x_lo, v_z_lo);
        v_xz_sum_a = vmlal_s16(v_xz_sum_a, v_x_hi, v_z_hi);

        v_w2_sum = vmlal_s16(v_w2_sum, v_w_lo, v_w_lo);
        v_w2_sum = vmlal_s16(v_w2_sum, v_w_hi, v_w_hi);
        v_y2_sum = vmlal_s16(v_y2_sum, v_y_lo, v_y_lo);
        v_y2_sum = vmlal_s16(v_y2_sum, v_y_hi, v_y_hi);

        v_w_sum = vpadalq_s16(v_w_sum, v_w);
        v_x_sum_a = vpadalq_s16(v_x_sum_a, v_y);
        v_x_sum_a = vpadalq_s16(v_x_sum_a, v_w);

        k -= 8;
        j += 8;
      }
      const int16x8_t v_l = vld1q_s16(&diff[(height - 2) * stride] + j);
      const int16x8_t v_x =
          vextq_s16(vextq_s16(vreinterpretq_s16_s32(zero), v_l, 7),
                    vreinterpretq_s16_s32(zero), 1);
      const int16x8_t v_y = vextq_s16(v_l, vreinterpretq_s16_s32(zero), 1);
      const int16x8_t v_l_2 = vld1q_s16(&diff[(height - 1) * stride] + j);
      const int16x8_t v_z =
          vextq_s16(vextq_s16(vreinterpretq_s16_s32(zero), v_l_2, 7),
                    vreinterpretq_s16_s32(zero), 1);
      const int16x8_t v_w = vextq_s16(v_l_2, vreinterpretq_s16_s32(zero), 1);

      const int16x4_t v_x_lo = vget_low_s16(v_x);
      const int16x4_t v_y_lo = vget_low_s16(v_y);
      const int16x4_t v_z_lo = vget_low_s16(v_z);
      const int16x4_t v_w_lo = vget_low_s16(v_w);
      const int16x4_t v_x_hi = vget_high_s16(v_x);
      const int16x4_t v_y_hi = vget_high_s16(v_y);
      const int16x4_t v_z_hi = vget_high_s16(v_z);
      const int16x4_t v_w_hi = vget_high_s16(v_w);

      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_lo, v_y_lo);
      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_x_hi, v_y_hi);
      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_z_lo, v_w_lo);
      v_xy_sum_a = vmlal_s16(v_xy_sum_a, v_z_hi, v_w_hi);

      v_xz_sum_a = vmlal_s16(v_xz_sum_a, v_x_lo, v_z_lo);
      v_xz_sum_a = vmlal_s16(v_xz_sum_a, v_x_hi, v_z_hi);

      v_w2_sum = vmlal_s16(v_w2_sum, v_w_lo, v_w_lo);
      v_w2_sum = vmlal_s16(v_w2_sum, v_w_hi, v_w_hi);
      v_y2_sum = vmlal_s16(v_y2_sum, v_y_lo, v_y_lo);
      v_y2_sum = vmlal_s16(v_y2_sum, v_y_hi, v_y_hi);

      v_w_sum = vpadalq_s16(v_w_sum, v_w);
      v_x_sum_a = vpadalq_s16(v_x_sum_a, v_y);
      v_x_sum_a = vpadalq_s16(v_x_sum_a, v_w);

#if AOM_ARCH_AARCH64
      xy_sum += vaddvq_s64(vpaddlq_s32(v_xy_sum_a));
      xz_sum += vaddvq_s64(vpaddlq_s32(v_xz_sum_a));
      x_sum += vaddvq_s32(v_x_sum_a);
      x_finalrow += vaddvq_s32(v_w_sum);
      int64_t y2 = vaddvq_s64(vpaddlq_s32(v_y2_sum));
      int64_t w2 = vaddvq_s64(vpaddlq_s32(v_w2_sum));
#else
      const int64x2_t v_xy_sum2 = vpaddlq_s32(v_xy_sum_a);
      xy_sum += vget_lane_s64(
          vadd_s64(vget_low_s64(v_xy_sum2), vget_high_s64(v_xy_sum2)), 0);
      const int64x2_t v_xz_sum2 = vpaddlq_s32(v_xz_sum_a);
      xz_sum += vget_lane_s64(
          vadd_s64(vget_low_s64(v_xz_sum2), vget_high_s64(v_xz_sum2)), 0);
      const int64x2_t v_x_sum2 = vpaddlq_s32(v_x_sum_a);
      x_sum += vget_lane_s64(
          vadd_s64(vget_low_s64(v_x_sum2), vget_high_s64(v_x_sum2)), 0);
      const int64x2_t v_w_sum_a = vpaddlq_s32(v_w_sum);
      x_finalrow += vget_lane_s64(
          vadd_s64(vget_low_s64(v_w_sum_a), vget_high_s64(v_w_sum_a)), 0);
      const int64x2_t v_y2_sum_a = vpaddlq_s32(v_y2_sum);
      int64_t y2 = vget_lane_s64(
          vadd_s64(vget_low_s64(v_y2_sum_a), vget_high_s64(v_y2_sum_a)), 0);
      const int64x2_t v_w2_sum_a = vpaddlq_s32(v_w2_sum);
      int64_t w2 = vget_lane_s64(
          vadd_s64(vget_low_s64(v_w2_sum_a), vget_high_s64(v_w2_sum_a)), 0);
#endif
      x2_sum += y2 + w2;
      x2_finalrow += w2;
    } else {
      for (int j = 0; j < width - 1; ++j) {
        const int16_t x = diff[(height - 2) * stride + j];
        const int16_t y = diff[(height - 2) * stride + j + 1];
        const int16_t z = diff[(height - 1) * stride + j];
        const int16_t w = diff[(height - 1) * stride + j + 1];

        // Horizontal and vertical correlations for the penultimate row:
        xy_sum += x * y;
        xz_sum += x * z;

        // Now just horizontal correlations for the final row:
        xy_sum += z * w;

        x_sum += y + w;
        x2_sum += y * y + w * w;
        x_finalrow += w;
        x2_finalrow += w * w;
      }
    }
  }

  // Do we have 2 columns remaining or just the one?
  if (width % 3 == 1) {  // Just vert corrs on the final col
    const int16_t x0 = diff[width - 1];
    x_sum += x0;
    x_finalcol += x0;
    x2_sum += x0 * x0;
    x2_finalcol += x0 * x0;
    for (int i = 0; i < height - 1; ++i) {
      const int16_t x = diff[i * stride + width - 1];
      const int16_t z = diff[(i + 1) * stride + width - 1];
      xz_sum += x * z;
      x_finalcol += z;
      x2_finalcol += z * z;
      // So the bottom-right elements don't get counted twice:
      if (i < height - (height % 3 == 1 ? 2 : 3)) {
        x_sum += z;
        x2_sum += z * z;
      }
    }
  } else {  // Two cols remaining
    const int16_t x0 = diff[width - 2];
    const int16_t y0 = diff[width - 1];
    x_sum += x0 + y0;
    x2_sum += x0 * x0 + y0 * y0;
    x_finalcol += y0;
    x2_finalcol += y0 * y0;
    for (int i = 0; i < height - 1; ++i) {
      const int16_t x = diff[i * stride + width - 2];
      const int16_t y = diff[i * stride + width - 1];
      const int16_t z = diff[(i + 1) * stride + width - 2];
      const int16_t w = diff[(i + 1) * stride + width - 1];

      // Horizontal and vertical correlations for the penultimate col:
      // Skip these on the last iteration of this loop if we also had two
      // rows remaining, otherwise the final horizontal and vertical correlation
      // get erroneously processed twice
      if (i < height - 2 || height % 3 == 1) {
        xy_sum += x * y;
        xz_sum += x * z;
      }

      x_finalcol += w;
      x2_finalcol += w * w;
      // So the bottom-right elements don't get counted twice:
      if (i < height - (height % 3 == 1 ? 2 : 3)) {
        x_sum += z + w;
        x2_sum += z * z + w * w;
      }

      // Now just vertical correlations for the final column:
      xz_sum += y * w;
    }
  }

  // Calculate the simple sums and squared-sums
  int64_t x_firstrow = 0, x_firstcol = 0;
  int64_t x2_firstrow = 0, x2_firstcol = 0;

  if (width >= 8) {
    int32x4_t v_x_firstrow = zero;
    int32x4_t v_x2_firstrow = zero;
    for (int j = 0; j < width; j += 8) {
      const int16x8_t v_diff = vld1q_s16(diff + j);
      const int16x4_t v_diff_lo = vget_low_s16(v_diff);
      const int16x4_t v_diff_hi = vget_high_s16(v_diff);
      v_x_firstrow = vpadalq_s16(v_x_firstrow, v_diff);
      v_x2_firstrow = vmlal_s16(v_x2_firstrow, v_diff_lo, v_diff_lo);
      v_x2_firstrow = vmlal_s16(v_x2_firstrow, v_diff_hi, v_diff_hi);
    }
#if AOM_ARCH_AARCH64
    x_firstrow += vaddvq_s32(v_x_firstrow);
    x2_firstrow += vaddvq_s32(v_x2_firstrow);
#else
    const int64x2_t v_x_firstrow_64 = vpaddlq_s32(v_x_firstrow);
    x_firstrow += vget_lane_s64(
        vadd_s64(vget_low_s64(v_x_firstrow_64), vget_high_s64(v_x_firstrow_64)),
        0);
    const int64x2_t v_x2_firstrow_64 = vpaddlq_s32(v_x2_firstrow);
    x2_firstrow += vget_lane_s64(vadd_s64(vget_low_s64(v_x2_firstrow_64),
                                          vget_high_s64(v_x2_firstrow_64)),
                                 0);
#endif
  } else {
    for (int j = 0; j < width; ++j) {
      x_firstrow += diff[j];
      x2_firstrow += diff[j] * diff[j];
    }
  }
  for (int i = 0; i < height; ++i) {
    x_firstcol += diff[i * stride];
    x2_firstcol += diff[i * stride] * diff[i * stride];
  }

  int64_t xhor_sum = x_sum - x_finalcol;
  int64_t xver_sum = x_sum - x_finalrow;
  int64_t y_sum = x_sum - x_firstcol;
  int64_t z_sum = x_sum - x_firstrow;
  int64_t x2hor_sum = x2_sum - x2_finalcol;
  int64_t x2ver_sum = x2_sum - x2_finalrow;
  int64_t y2_sum = x2_sum - x2_firstcol;
  int64_t z2_sum = x2_sum - x2_firstrow;

  const float num_hor = (float)(height * (width - 1));
  const float num_ver = (float)((height - 1) * width);

  const float xhor_var_n = x2hor_sum - (xhor_sum * xhor_sum) / num_hor;
  const float xver_var_n = x2ver_sum - (xver_sum * xver_sum) / num_ver;

  const float y_var_n = y2_sum - (y_sum * y_sum) / num_hor;
  const float z_var_n = z2_sum - (z_sum * z_sum) / num_ver;

  const float xy_var_n = xy_sum - (xhor_sum * y_sum) / num_hor;
  const float xz_var_n = xz_sum - (xver_sum * z_sum) / num_ver;

  if (xhor_var_n > 0 && y_var_n > 0) {
    *hcorr = xy_var_n / sqrtf(xhor_var_n * y_var_n);
    *hcorr = *hcorr < 0 ? 0 : *hcorr;
  } else {
    *hcorr = 1.0;
  }
  if (xver_var_n > 0 && z_var_n > 0) {
    *vcorr = xz_var_n / sqrtf(xver_var_n * z_var_n);
    *vcorr = *vcorr < 0 ? 0 : *vcorr;
  } else {
    *vcorr = 1.0;
  }
}
