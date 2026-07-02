/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <smmintrin.h>
#include "aom_dsp/x86/synonyms.h"

#include "config/av1_rtcd.h"
#include "av1/encoder/rdopt.h"

// Process horizontal and vertical correlations in a 4x4 block of pixels.
// We actually use the 4x4 pixels to calculate correlations corresponding to
// the top-left 3x3 pixels, so this function must be called with 1x1 overlap,
// moving the window along/down by 3 pixels at a time.
static inline void horver_correlation_4x4(const int16_t *diff, int stride,
                                          __m128i *xy_sum_32,
                                          __m128i *xz_sum_32, __m128i *x_sum_32,
                                          __m128i *x2_sum_32) {
  // Pixels in this 4x4   [ a b c d ]
  // are referred to as:  [ e f g h ]
  //                      [ i j k l ]
  //                      [ m n o p ]

  const __m128i pixelsa = xx_loadu_2x64(&diff[0 * stride], &diff[2 * stride]);
  const __m128i pixelsb = xx_loadu_2x64(&diff[1 * stride], &diff[3 * stride]);
  // pixelsa = [d c b a l k j i] as i16
  // pixelsb = [h g f e p o n m] as i16

  const __m128i slli_a = _mm_slli_epi64(pixelsa, 16);
  const __m128i slli_b = _mm_slli_epi64(pixelsb, 16);
  // slli_a = [c b a 0 k j i 0] as i16
  // slli_b = [g f e 0 o n m 0] as i16

  const __m128i xy_madd_a = _mm_madd_epi16(pixelsa, slli_a);
  const __m128i xy_madd_b = _mm_madd_epi16(pixelsb, slli_b);
  // xy_madd_a = [bc+cd ab jk+kl ij] as i32
  // xy_madd_b = [fg+gh ef no+op mn] as i32

  const __m128i xy32 = _mm_hadd_epi32(xy_madd_b, xy_madd_a);
  // xy32 = [ab+bc+cd ij+jk+kl ef+fg+gh mn+no+op] as i32
  *xy_sum_32 = _mm_add_epi32(*xy_sum_32, xy32);

  const __m128i xz_madd_a = _mm_madd_epi16(slli_a, slli_b);
  // xz_madd_a = [bf+cg ae jn+ko im] i32

  const __m128i swap_b = _mm_srli_si128(slli_b, 8);
  // swap_b = [0 0 0 0 g f e 0] as i16
  const __m128i xz_madd_b = _mm_madd_epi16(slli_a, swap_b);
  // xz_madd_b = [0 0 gk+fj ei] i32

  const __m128i xz32 = _mm_hadd_epi32(xz_madd_b, xz_madd_a);
  // xz32 = [ae+bf+cg im+jn+ko 0 ei+fj+gk] i32
  *xz_sum_32 = _mm_add_epi32(*xz_sum_32, xz32);

  // Now calculate the straight sums, x_sum += a+b+c+e+f+g+i+j+k
  // (sum up every element in slli_a and swap_b)
  const __m128i sum_slli_a = _mm_hadd_epi16(slli_a, slli_a);
  const __m128i sum_slli_a32 = _mm_cvtepi16_epi32(sum_slli_a);
  // sum_slli_a32 = [c+b a k+j i] as i32
  const __m128i swap_b32 = _mm_cvtepi16_epi32(swap_b);
  // swap_b32 = [g f e 0] as i32
  *x_sum_32 = _mm_add_epi32(*x_sum_32, sum_slli_a32);
  *x_sum_32 = _mm_add_epi32(*x_sum_32, swap_b32);
  // sum = [c+b+g a+f k+j+e i] as i32

  // Also sum their squares
  const __m128i slli_a_2 = _mm_madd_epi16(slli_a, slli_a);
  const __m128i swap_b_2 = _mm_madd_epi16(swap_b, swap_b);
  // slli_a_2 = [c2+b2 a2 k2+j2 i2]
  // swap_b_2 = [0 0 g2+f2 e2]
  const __m128i sum2 = _mm_hadd_epi32(slli_a_2, swap_b_2);
  // sum2 = [0 g2+f2+e2 c2+b2+a2 k2+j2+i2]
  *x2_sum_32 = _mm_add_epi32(*x2_sum_32, sum2);
}

void av1_get_horver_correlation_full_sse4_1(const int16_t *diff, int stride,
                                            int width, int height, float *hcorr,
                                            float *vcorr) {
  // The following notation is used:
  // x - current pixel
  // y - right neighbour pixel
  // z - below neighbour pixel
  // w - down-right neighbour pixel
  int64_t xy_sum = 0, xz_sum = 0;
  int64_t x_sum = 0, x2_sum = 0;

  // Process horizontal and vertical correlations through the body in 4x4
  // blocks.  This excludes the final row and column and possibly one extra
  // column depending how 3 divides into width and height
  int32_t xy_tmp[4] = { 0 }, xz_tmp[4] = { 0 };
  int32_t x_tmp[4] = { 0 }, x2_tmp[4] = { 0 };
  __m128i xy_sum_32 = _mm_setzero_si128();
  __m128i xz_sum_32 = _mm_setzero_si128();
  __m128i x_sum_32 = _mm_setzero_si128();
  __m128i x2_sum_32 = _mm_setzero_si128();
  for (int i = 0; i <= height - 4; i += 3) {
    for (int j = 0; j <= width - 4; j += 3) {
      horver_correlation_4x4(&diff[i * stride + j], stride, &xy_sum_32,
                             &xz_sum_32, &x_sum_32, &x2_sum_32);
    }
    xx_storeu_128(xy_tmp, xy_sum_32);
    xx_storeu_128(xz_tmp, xz_sum_32);
    xx_storeu_128(x_tmp, x_sum_32);
    xx_storeu_128(x2_tmp, x2_sum_32);
    xy_sum += (int64_t)xy_tmp[3] + xy_tmp[2] + xy_tmp[1];
    xz_sum += (int64_t)xz_tmp[3] + xz_tmp[2] + xz_tmp[0];
    x_sum += (int64_t)x_tmp[3] + x_tmp[2] + x_tmp[1] + x_tmp[0];
    x2_sum += (int64_t)x2_tmp[2] + x2_tmp[1] + x2_tmp[0];
    xy_sum_32 = _mm_setzero_si128();
    xz_sum_32 = _mm_setzero_si128();
    x_sum_32 = _mm_setzero_si128();
    x2_sum_32 = _mm_setzero_si128();
  }

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
    for (int j = 0; j < width - 1; ++j) {
      const int16_t x = diff[(height - 1) * stride + j];
      const int16_t y = diff[(height - 1) * stride + j + 1];
      xy_sum += x * y;
      x_sum += y;
      x2_sum += y * y;
      x_finalrow += y;
      x2_finalrow += y * y;
    }
  } else {  // Two rows remaining to do
    const int16_t x0 = diff[(height - 2) * stride];
    const int16_t z0 = diff[(height - 1) * stride];
    x_sum += x0 + z0;
    x2_sum += x0 * x0 + z0 * z0;
    x_finalrow += z0;
    x2_finalrow += z0 * z0;
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

  for (int j = 0; j < width; ++j) {
    x_firstrow += diff[j];
    x2_firstrow += diff[j] * diff[j];
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
