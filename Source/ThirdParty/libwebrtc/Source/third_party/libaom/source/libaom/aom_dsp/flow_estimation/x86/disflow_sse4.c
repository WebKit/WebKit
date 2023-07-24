/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at aomedia.org/license/software-license/bsd-3-c-c/.  If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#include <assert.h>
#include <math.h>
#include <smmintrin.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/flow_estimation/disflow.h"
#include "aom_dsp/x86/synonyms.h"

#include "config/aom_dsp_rtcd.h"

// Internal cross-check against C code
// If you set this to 1 and compile in debug mode, then the outputs of the two
// convolution stages will be checked against the plain C version of the code,
// and an assertion will be fired if the results differ.
#define CHECK_RESULTS 1

// Note: Max sum(+ve coefficients) = 1.125 * scale
static INLINE void get_cubic_kernel_dbl(double x, double *kernel) {
  assert(0 <= x && x < 1);
  double x2 = x * x;
  double x3 = x2 * x;
  kernel[0] = -0.5 * x + x2 - 0.5 * x3;
  kernel[1] = 1.0 - 2.5 * x2 + 1.5 * x3;
  kernel[2] = 0.5 * x + 2.0 * x2 - 1.5 * x3;
  kernel[3] = -0.5 * x2 + 0.5 * x3;
}

static INLINE void get_cubic_kernel_int(double x, int16_t *kernel) {
  double kernel_dbl[4];
  get_cubic_kernel_dbl(x, kernel_dbl);

  kernel[0] = (int16_t)rint(kernel_dbl[0] * (1 << DISFLOW_INTERP_BITS));
  kernel[1] = (int16_t)rint(kernel_dbl[1] * (1 << DISFLOW_INTERP_BITS));
  kernel[2] = (int16_t)rint(kernel_dbl[2] * (1 << DISFLOW_INTERP_BITS));
  kernel[3] = (int16_t)rint(kernel_dbl[3] * (1 << DISFLOW_INTERP_BITS));
}

#if CHECK_RESULTS
static INLINE int get_cubic_value_int(const int *p, const int16_t *kernel) {
  return kernel[0] * p[0] + kernel[1] * p[1] + kernel[2] * p[2] +
         kernel[3] * p[3];
}
#endif  // CHECK_RESULTS

// Compare two regions of width x height pixels, one rooted at position
// (x, y) in src and the other at (x + u, y + v) in ref.
// This function returns the sum of squared pixel differences between
// the two regions.
//
// TODO(rachelbarker): Test speed/quality impact of using bilinear interpolation
// instad of bicubic interpolation
static INLINE void compute_flow_error(const uint8_t *src, const uint8_t *ref,
                                      int width, int height, int stride, int x,
                                      int y, double u, double v, int16_t *dt) {
  // This function is written to do 8x8 convolutions only
  assert(DISFLOW_PATCH_SIZE == 8);

  // Split offset into integer and fractional parts, and compute cubic
  // interpolation kernels
  const int u_int = (int)floor(u);
  const int v_int = (int)floor(v);
  const double u_frac = u - floor(u);
  const double v_frac = v - floor(v);

  int16_t h_kernel[4];
  int16_t v_kernel[4];
  get_cubic_kernel_int(u_frac, h_kernel);
  get_cubic_kernel_int(v_frac, v_kernel);

  // Storage for intermediate values between the two convolution directions
  int16_t tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 3)];
  int16_t *tmp = tmp_ + DISFLOW_PATCH_SIZE;  // Offset by one row

  // Clamp coordinates so that all pixels we fetch will remain within the
  // allocated border region, but allow them to go far enough out that
  // the border pixels' values do not change.
  // Since we are calculating an 8x8 block, the bottom-right pixel
  // in the block has coordinates (x0 + 7, y0 + 7). Then, the cubic
  // interpolation has 4 taps, meaning that the output of pixel
  // (x_w, y_w) depends on the pixels in the range
  // ([x_w - 1, x_w + 2], [y_w - 1, y_w + 2]).
  //
  // Thus the most extreme coordinates which will be fetched are
  // (x0 - 1, y0 - 1) and (x0 + 9, y0 + 9).
  const int x0 = clamp(x + u_int, -9, width);
  const int y0 = clamp(y + v_int, -9, height);

  // Horizontal convolution

  // Prepare the kernel vectors
  // We split the kernel into two vectors with kernel indices:
  // 0, 1, 0, 1, 0, 1, 0, 1, and
  // 2, 3, 2, 3, 2, 3, 2, 3
  __m128i h_kernel_01 = xx_set2_epi16(h_kernel[0], h_kernel[1]);
  __m128i h_kernel_23 = xx_set2_epi16(h_kernel[2], h_kernel[3]);

  __m128i round_const_h = _mm_set1_epi32(1 << (DISFLOW_INTERP_BITS - 6 - 1));

  for (int i = -1; i < DISFLOW_PATCH_SIZE + 2; ++i) {
    const int y_w = y0 + i;
    const uint8_t *ref_row = &ref[y_w * stride + (x0 - 1)];
    int16_t *tmp_row = &tmp[i * DISFLOW_PATCH_SIZE];

    // Load this row of pixels.
    // For an 8x8 patch, we need to load the 8 image pixels + 3 extras,
    // for a total of 11 pixels. Here we load 16 pixels, but only use
    // the first 11.
    __m128i row = _mm_loadu_si128((__m128i *)ref_row);

    // Expand pixels to int16s
    __m128i px_0to7_i16 = _mm_cvtepu8_epi16(row);
    __m128i px_4to10_i16 = _mm_cvtepu8_epi16(_mm_srli_si128(row, 4));

    // Relevant multiply instruction
    // This multiplies pointwise, then sums in pairs.
    //_mm_madd_epi16();

    // Compute first four outputs
    // input pixels 0, 1, 1, 2, 2, 3, 3, 4
    // * kernel     0, 1, 0, 1, 0, 1, 0, 1
    __m128i px0 =
        _mm_unpacklo_epi16(px_0to7_i16, _mm_srli_si128(px_0to7_i16, 2));
    // input pixels 2, 3, 3, 4, 4, 5, 5, 6
    // * kernel     2, 3, 2, 3, 2, 3, 2, 3
    __m128i px1 = _mm_unpacklo_epi16(_mm_srli_si128(px_0to7_i16, 4),
                                     _mm_srli_si128(px_0to7_i16, 6));
    // Convolve with kernel and sum 2x2 boxes to form first 4 outputs
    __m128i sum0 = _mm_add_epi32(_mm_madd_epi16(px0, h_kernel_01),
                                 _mm_madd_epi16(px1, h_kernel_23));

    __m128i out0 = _mm_srai_epi32(_mm_add_epi32(sum0, round_const_h),
                                  DISFLOW_INTERP_BITS - 6);

    // Compute second four outputs
    __m128i px2 =
        _mm_unpacklo_epi16(px_4to10_i16, _mm_srli_si128(px_4to10_i16, 2));
    __m128i px3 = _mm_unpacklo_epi16(_mm_srli_si128(px_4to10_i16, 4),
                                     _mm_srli_si128(px_4to10_i16, 6));
    __m128i sum1 = _mm_add_epi32(_mm_madd_epi16(px2, h_kernel_01),
                                 _mm_madd_epi16(px3, h_kernel_23));

    // Round by just enough bits that the result is
    // guaranteed to fit into an i16. Then the next stage can use 16 x 16 -> 32
    // bit multiplies, which should be a fair bit faster than 32 x 32 -> 32
    // as it does now
    // This means shifting down so we have 6 extra bits, for a maximum value
    // of +18360, which can occur if u_frac == 0.5 and the input pixels are
    // {0, 255, 255, 0}.
    __m128i out1 = _mm_srai_epi32(_mm_add_epi32(sum1, round_const_h),
                                  DISFLOW_INTERP_BITS - 6);

    _mm_storeu_si128((__m128i *)tmp_row, _mm_packs_epi32(out0, out1));

#if CHECK_RESULTS && !defined(NDEBUG)
    // Cross-check
    for (int j = 0; j < DISFLOW_PATCH_SIZE; ++j) {
      const int x_w = x0 + j;
      int arr[4];

      arr[0] = (int)ref[y_w * stride + (x_w - 1)];
      arr[1] = (int)ref[y_w * stride + (x_w + 0)];
      arr[2] = (int)ref[y_w * stride + (x_w + 1)];
      arr[3] = (int)ref[y_w * stride + (x_w + 2)];

      // Apply kernel and round, keeping 6 extra bits of precision.
      //
      // 6 is the maximum allowable number of extra bits which will avoid
      // the intermediate values overflowing an int16_t. The most extreme
      // intermediate value occurs when:
      // * The input pixels are [0, 255, 255, 0]
      // * u_frac = 0.5
      // In this case, the un-scaled output is 255 * 1.125 = 286.875.
      // As an integer with 6 fractional bits, that is 18360, which fits
      // in an int16_t. But with 7 fractional bits it would be 36720,
      // which is too large.
      const int c_value = ROUND_POWER_OF_TWO(get_cubic_value_int(arr, h_kernel),
                                             DISFLOW_INTERP_BITS - 6);
      (void)c_value;  // Suppress warnings
      assert(tmp_row[j] == c_value);
    }
#endif  // CHECK_RESULTS
  }

  // Vertical convolution
  const int round_bits = DISFLOW_INTERP_BITS + 6 - DISFLOW_DERIV_SCALE_LOG2;
  __m128i round_const_v = _mm_set1_epi32(1 << (round_bits - 1));

  __m128i v_kernel_01 = xx_set2_epi16(v_kernel[0], v_kernel[1]);
  __m128i v_kernel_23 = xx_set2_epi16(v_kernel[2], v_kernel[3]);

  for (int i = 0; i < DISFLOW_PATCH_SIZE; ++i) {
    int16_t *tmp_row = &tmp[i * DISFLOW_PATCH_SIZE];

    // Load 4 rows of 8 x 16-bit values
    __m128i px0 = _mm_loadu_si128((__m128i *)(tmp_row - DISFLOW_PATCH_SIZE));
    __m128i px1 = _mm_loadu_si128((__m128i *)tmp_row);
    __m128i px2 = _mm_loadu_si128((__m128i *)(tmp_row + DISFLOW_PATCH_SIZE));
    __m128i px3 =
        _mm_loadu_si128((__m128i *)(tmp_row + 2 * DISFLOW_PATCH_SIZE));

    // We want to calculate px0 * v_kernel[0] + px1 * v_kernel[1] + ... ,
    // but each multiply expands its output to 32 bits. So we need to be
    // a little clever about how we do this
    __m128i sum0 = _mm_add_epi32(
        _mm_madd_epi16(_mm_unpacklo_epi16(px0, px1), v_kernel_01),
        _mm_madd_epi16(_mm_unpacklo_epi16(px2, px3), v_kernel_23));
    __m128i sum1 = _mm_add_epi32(
        _mm_madd_epi16(_mm_unpackhi_epi16(px0, px1), v_kernel_01),
        _mm_madd_epi16(_mm_unpackhi_epi16(px2, px3), v_kernel_23));

    __m128i sum0_rounded =
        _mm_srai_epi32(_mm_add_epi32(sum0, round_const_v), round_bits);
    __m128i sum1_rounded =
        _mm_srai_epi32(_mm_add_epi32(sum1, round_const_v), round_bits);

    __m128i warped = _mm_packs_epi32(sum0_rounded, sum1_rounded);
    __m128i src_pixels_u8 =
        _mm_loadl_epi64((__m128i *)&src[(y + i) * stride + x]);
    __m128i src_pixels = _mm_slli_epi16(_mm_cvtepu8_epi16(src_pixels_u8), 3);

    // Calculate delta from the target patch
    __m128i err = _mm_sub_epi16(warped, src_pixels);
    _mm_storeu_si128((__m128i *)&dt[i * DISFLOW_PATCH_SIZE], err);

#if CHECK_RESULTS
    for (int j = 0; j < DISFLOW_PATCH_SIZE; ++j) {
      int16_t *p = &tmp[i * DISFLOW_PATCH_SIZE + j];
      int arr[4] = { p[-DISFLOW_PATCH_SIZE], p[0], p[DISFLOW_PATCH_SIZE],
                     p[2 * DISFLOW_PATCH_SIZE] };
      const int result = get_cubic_value_int(arr, v_kernel);

      // Apply kernel and round.
      // This time, we have to round off the 6 extra bits which were kept
      // earlier, but we also want to keep DISFLOW_DERIV_SCALE_LOG2 extra bits
      // of precision to match the scale of the dx and dy arrays.
      const int c_warped = ROUND_POWER_OF_TWO(result, round_bits);
      const int c_src_px = src[(x + j) + (y + i) * stride] << 3;
      const int c_err = c_warped - c_src_px;
      (void)c_err;
      assert(dt[i * DISFLOW_PATCH_SIZE + j] == c_err);
    }
#endif  // CHECK_RESULTS
  }
}

static INLINE void sobel_filter_x(const uint8_t *src, int src_stride,
                                  int16_t *dst, int dst_stride) {
  int16_t tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 2)];
  int16_t *tmp = tmp_ + DISFLOW_PATCH_SIZE;
  const int taps = 3;

  // Horizontal filter
  // As the kernel is simply {1, 0, -1}, we implement this as simply
  //  out[x] = image[x-1] - image[x+1]
  // rather than doing a "proper" convolution operation
  for (int y = -1; y < DISFLOW_PATCH_SIZE + 1; ++y) {
    const uint8_t *src_row = src + y * src_stride;
    int16_t *tmp_row = tmp + y * DISFLOW_PATCH_SIZE;

    // Load pixels and expand to 16 bits
    __m128i row = _mm_loadu_si128((__m128i *)(src_row - 1));
    __m128i px0 = _mm_cvtepu8_epi16(row);
    __m128i px2 = _mm_cvtepu8_epi16(_mm_srli_si128(row, 2));

    __m128i out = _mm_sub_epi16(px0, px2);

    // Store to intermediate array
    _mm_storeu_si128((__m128i *)tmp_row, out);

#if CHECK_RESULTS
    // Cross-check
    static const int16_t h_kernel[3] = { 1, 0, -1 };
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += h_kernel[k] * src_row[x + k - 1];
      }
      (void)sum;
      assert(tmp_row[x] == sum);
    }
#endif  // CHECK_RESULTS
  }

  // Vertical filter
  // Here the kernel is {1, 2, 1}, which can be implemented
  // with simple sums rather than multiplies and adds.
  // In order to minimize dependency chains, we evaluate in the order
  // (image[y - 1] + image[y + 1]) + (image[y] << 1)
  // This way, the first addition and the shift can happen in parallel
  for (int y = 0; y < DISFLOW_PATCH_SIZE; ++y) {
    const int16_t *tmp_row = tmp + y * DISFLOW_PATCH_SIZE;
    int16_t *dst_row = dst + y * dst_stride;

    __m128i px0 = _mm_loadu_si128((__m128i *)(tmp_row - DISFLOW_PATCH_SIZE));
    __m128i px1 = _mm_loadu_si128((__m128i *)tmp_row);
    __m128i px2 = _mm_loadu_si128((__m128i *)(tmp_row + DISFLOW_PATCH_SIZE));

    __m128i out =
        _mm_add_epi16(_mm_add_epi16(px0, px2), _mm_slli_epi16(px1, 1));

    _mm_storeu_si128((__m128i *)dst_row, out);

#if CHECK_RESULTS
    static const int16_t v_kernel[3] = { 1, 2, 1 };
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += v_kernel[k] * tmp[(y + k - 1) * DISFLOW_PATCH_SIZE + x];
      }
      (void)sum;
      assert(dst_row[x] == sum);
    }
#endif  // CHECK_RESULTS
  }
}

static INLINE void sobel_filter_y(const uint8_t *src, int src_stride,
                                  int16_t *dst, int dst_stride) {
  int16_t tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 2)];
  int16_t *tmp = tmp_ + DISFLOW_PATCH_SIZE;
  const int taps = 3;

  // Horizontal filter
  // Here the kernel is {1, 2, 1}, which can be implemented
  // with simple sums rather than multiplies and adds.
  // In order to minimize dependency chains, we evaluate in the order
  // (image[y - 1] + image[y + 1]) + (image[y] << 1)
  // This way, the first addition and the shift can happen in parallel
  for (int y = -1; y < DISFLOW_PATCH_SIZE + 1; ++y) {
    const uint8_t *src_row = src + y * src_stride;
    int16_t *tmp_row = tmp + y * DISFLOW_PATCH_SIZE;

    // Load pixels and expand to 16 bits
    __m128i row = _mm_loadu_si128((__m128i *)(src_row - 1));
    __m128i px0 = _mm_cvtepu8_epi16(row);
    __m128i px1 = _mm_cvtepu8_epi16(_mm_srli_si128(row, 1));
    __m128i px2 = _mm_cvtepu8_epi16(_mm_srli_si128(row, 2));

    __m128i out =
        _mm_add_epi16(_mm_add_epi16(px0, px2), _mm_slli_epi16(px1, 1));

    // Store to intermediate array
    _mm_storeu_si128((__m128i *)tmp_row, out);

#if CHECK_RESULTS
    // Cross-check
    static const int16_t h_kernel[3] = { 1, 2, 1 };
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += h_kernel[k] * src_row[x + k - 1];
      }
      (void)sum;
      assert(tmp_row[x] == sum);
    }
#endif  // CHECK_RESULTS
  }

  // Vertical filter
  // As the kernel is simply {1, 0, -1}, we implement this as simply
  //  out[x] = image[x-1] - image[x+1]
  // rather than doing a "proper" convolution operation
  for (int y = 0; y < DISFLOW_PATCH_SIZE; ++y) {
    const int16_t *tmp_row = tmp + y * DISFLOW_PATCH_SIZE;
    int16_t *dst_row = dst + y * dst_stride;

    __m128i px0 = _mm_loadu_si128((__m128i *)(tmp_row - DISFLOW_PATCH_SIZE));
    __m128i px2 = _mm_loadu_si128((__m128i *)(tmp_row + DISFLOW_PATCH_SIZE));

    __m128i out = _mm_sub_epi16(px0, px2);

    _mm_storeu_si128((__m128i *)dst_row, out);

#if CHECK_RESULTS
    static const int16_t v_kernel[3] = { 1, 0, -1 };
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += v_kernel[k] * tmp[(y + k - 1) * DISFLOW_PATCH_SIZE + x];
      }
      (void)sum;
      assert(dst_row[x] == sum);
    }
#endif  // CHECK_RESULTS
  }
}

static INLINE void compute_flow_vector(const int16_t *dx, int dx_stride,
                                       const int16_t *dy, int dy_stride,
                                       const int16_t *dt, int dt_stride,
                                       int *b) {
  __m128i b0_acc = _mm_setzero_si128();
  __m128i b1_acc = _mm_setzero_si128();

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    // Need to load 8 values of dx, 8 of dy, 8 of dt, which conveniently
    // works out to one register each. Then just calculate dx * dt, dy * dt,
    // and (implicitly) sum horizontally in pairs.
    // This gives four 32-bit partial sums for each of b[0] and b[1],
    // which can be accumulated and summed at the end.
    __m128i dx_row = _mm_loadu_si128((__m128i *)&dx[i * dx_stride]);
    __m128i dy_row = _mm_loadu_si128((__m128i *)&dy[i * dy_stride]);
    __m128i dt_row = _mm_loadu_si128((__m128i *)&dt[i * dt_stride]);

    b0_acc = _mm_add_epi32(b0_acc, _mm_madd_epi16(dx_row, dt_row));
    b1_acc = _mm_add_epi32(b1_acc, _mm_madd_epi16(dy_row, dt_row));
  }

  // We need to set b[0] = sum(b0_acc), b[1] = sum(b1_acc).
  // We might as well use a `hadd` instruction to do 4 of the additions
  // needed here. Then that just leaves two more additions, which can be
  // done in scalar code
  __m128i partial_sum = _mm_hadd_epi32(b0_acc, b1_acc);
  b[0] = _mm_extract_epi32(partial_sum, 0) + _mm_extract_epi32(partial_sum, 1);
  b[1] = _mm_extract_epi32(partial_sum, 2) + _mm_extract_epi32(partial_sum, 3);

#if CHECK_RESULTS
  int c_result[2] = { 0 };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    for (int j = 0; j < DISFLOW_PATCH_SIZE; j++) {
      c_result[0] += dx[i * dx_stride + j] * dt[i * dt_stride + j];
      c_result[1] += dy[i * dy_stride + j] * dt[i * dt_stride + j];
    }
  }

  assert(b[0] == c_result[0]);
  assert(b[1] == c_result[1]);
#endif  // CHECK_RESULTS
}

static INLINE void compute_flow_matrix(const int16_t *dx, int dx_stride,
                                       const int16_t *dy, int dy_stride,
                                       double *M) {
  __m128i acc[4] = { 0 };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    __m128i dx_row = _mm_loadu_si128((__m128i *)&dx[i * dx_stride]);
    __m128i dy_row = _mm_loadu_si128((__m128i *)&dy[i * dy_stride]);

    acc[0] = _mm_add_epi32(acc[0], _mm_madd_epi16(dx_row, dx_row));
    acc[1] = _mm_add_epi32(acc[1], _mm_madd_epi16(dx_row, dy_row));
    // Don't compute acc[2], as it should be equal to acc[1]
    acc[3] = _mm_add_epi32(acc[3], _mm_madd_epi16(dy_row, dy_row));
  }

  // Condense sums
  __m128i partial_sum_0 = _mm_hadd_epi32(acc[0], acc[1]);
  __m128i partial_sum_1 = _mm_hadd_epi32(acc[1], acc[3]);
  __m128i result = _mm_hadd_epi32(partial_sum_0, partial_sum_1);

  // Apply regularization
  // We follow the standard regularization method of adding `k * I` before
  // inverting. This ensures that the matrix will be invertible.
  //
  // Setting the regularization strength k to 1 seems to work well here, as
  // typical values coming from the other equations are very large (1e5 to
  // 1e6, with an upper limit of around 6e7, at the time of writing).
  // It also preserves the property that all matrix values are whole numbers,
  // which is convenient for integerized SIMD implementation.
  result = _mm_add_epi32(result, _mm_set_epi32(1, 0, 0, 1));

#if CHECK_RESULTS
  int tmp[4] = { 0 };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    for (int j = 0; j < DISFLOW_PATCH_SIZE; j++) {
      tmp[0] += dx[i * dx_stride + j] * dx[i * dx_stride + j];
      tmp[1] += dx[i * dx_stride + j] * dy[i * dy_stride + j];
      // Don't compute tmp[2], as it should be equal to tmp[1]
      tmp[3] += dy[i * dy_stride + j] * dy[i * dy_stride + j];
    }
  }

  // Apply regularization
  tmp[0] += 1;
  tmp[3] += 1;

  tmp[2] = tmp[1];

  assert(tmp[0] == _mm_extract_epi32(result, 0));
  assert(tmp[1] == _mm_extract_epi32(result, 1));
  assert(tmp[2] == _mm_extract_epi32(result, 2));
  assert(tmp[3] == _mm_extract_epi32(result, 3));
#endif  // CHECK_RESULTS

  // Convert results to doubles and store
  _mm_storeu_pd(M, _mm_cvtepi32_pd(result));
  _mm_storeu_pd(M + 2, _mm_cvtepi32_pd(_mm_srli_si128(result, 8)));
}

// Try to invert the matrix M
// Note: Due to the nature of how a least-squares matrix is constructed, all of
// the eigenvalues will be >= 0, and therefore det M >= 0 as well.
// The regularization term `+ k * I` further ensures that det M >= k^2.
// As mentioned in compute_flow_matrix(), here we use k = 1, so det M >= 1.
// So we don't have to worry about non-invertible matrices here.
static INLINE void invert_2x2(const double *M, double *M_inv) {
  double det = (M[0] * M[3]) - (M[1] * M[2]);
  assert(det >= 1);
  const double det_inv = 1 / det;

  M_inv[0] = M[3] * det_inv;
  M_inv[1] = -M[1] * det_inv;
  M_inv[2] = -M[2] * det_inv;
  M_inv[3] = M[0] * det_inv;
}

void aom_compute_flow_at_point_sse4_1(const uint8_t *src, const uint8_t *ref,
                                      int x, int y, int width, int height,
                                      int stride, double *u, double *v) {
  double M[4];
  double M_inv[4];
  int b[2];
  int16_t dt[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];
  int16_t dx[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];
  int16_t dy[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];

  // Compute gradients within this patch
  const uint8_t *src_patch = &src[y * stride + x];
  sobel_filter_x(src_patch, stride, dx, DISFLOW_PATCH_SIZE);
  sobel_filter_y(src_patch, stride, dy, DISFLOW_PATCH_SIZE);

  compute_flow_matrix(dx, DISFLOW_PATCH_SIZE, dy, DISFLOW_PATCH_SIZE, M);
  invert_2x2(M, M_inv);

  for (int itr = 0; itr < DISFLOW_MAX_ITR; itr++) {
    compute_flow_error(src, ref, width, height, stride, x, y, *u, *v, dt);
    compute_flow_vector(dx, DISFLOW_PATCH_SIZE, dy, DISFLOW_PATCH_SIZE, dt,
                        DISFLOW_PATCH_SIZE, b);

    // Solve flow equations to find a better estimate for the flow vector
    // at this point
    const double step_u = M_inv[0] * b[0] + M_inv[1] * b[1];
    const double step_v = M_inv[2] * b[0] + M_inv[3] * b[1];
    *u += fclamp(step_u * DISFLOW_STEP_SIZE, -2, 2);
    *v += fclamp(step_v * DISFLOW_STEP_SIZE, -2, 2);

    if (fabs(step_u) + fabs(step_v) < DISFLOW_STEP_SIZE_THRESOLD) {
      // Stop iteration when we're close to convergence
      break;
    }
  }
}
