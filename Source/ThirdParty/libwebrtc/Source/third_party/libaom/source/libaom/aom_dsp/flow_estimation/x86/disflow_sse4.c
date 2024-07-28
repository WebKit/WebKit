/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>
#include <smmintrin.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/flow_estimation/disflow.h"
#include "aom_dsp/x86/synonyms.h"

#include "config/aom_dsp_rtcd.h"

#if DISFLOW_PATCH_SIZE != 8
#error "Need to change disflow_sse4.c if DISFLOW_PATCH_SIZE != 8"
#endif

// Compute horizontal and vertical kernels and return them packed into a
// register. The coefficient ordering is:
//   h0, h1, v0, v1, h2, h3, v2, v3
// This is chosen because it takes less work than fully separating the kernels,
// but it is separated enough that we can pick out each coefficient pair in the
// main compute_flow_at_point function
static INLINE __m128i compute_cubic_kernels(double u, double v) {
  const __m128d x = _mm_set_pd(v, u);

  const __m128d x2 = _mm_mul_pd(x, x);
  const __m128d x3 = _mm_mul_pd(x2, x);

  // Macro to multiply a value v by a constant coefficient c
#define MULC(c, v) _mm_mul_pd(_mm_set1_pd(c), v)

  // Compute floating-point kernel
  // Note: To ensure results are bit-identical to the C code, we need to perform
  // exactly the same sequence of operations here as in the C code.
  __m128d k0 = _mm_sub_pd(_mm_add_pd(MULC(-0.5, x), x2), MULC(0.5, x3));
  __m128d k1 =
      _mm_add_pd(_mm_sub_pd(_mm_set1_pd(1.0), MULC(2.5, x2)), MULC(1.5, x3));
  __m128d k2 =
      _mm_sub_pd(_mm_add_pd(MULC(0.5, x), MULC(2.0, x2)), MULC(1.5, x3));
  __m128d k3 = _mm_add_pd(MULC(-0.5, x2), MULC(0.5, x3));
#undef MULC

  // Integerize
  __m128d prec = _mm_set1_pd((double)(1 << DISFLOW_INTERP_BITS));

  k0 = _mm_round_pd(_mm_mul_pd(k0, prec),
                    _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
  k1 = _mm_round_pd(_mm_mul_pd(k1, prec),
                    _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
  k2 = _mm_round_pd(_mm_mul_pd(k2, prec),
                    _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
  k3 = _mm_round_pd(_mm_mul_pd(k3, prec),
                    _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);

  const __m128i c0 = _mm_cvtpd_epi32(k0);
  const __m128i c1 = _mm_cvtpd_epi32(k1);
  const __m128i c2 = _mm_cvtpd_epi32(k2);
  const __m128i c3 = _mm_cvtpd_epi32(k3);

  // Rearrange results and convert down to 16 bits, giving the target output
  // ordering
  const __m128i c01 = _mm_unpacklo_epi32(c0, c1);
  const __m128i c23 = _mm_unpacklo_epi32(c2, c3);
  return _mm_packs_epi32(c01, c23);
}

// Compare two regions of width x height pixels, one rooted at position
// (x, y) in src and the other at (x + u, y + v) in ref.
// This function returns the sum of squared pixel differences between
// the two regions.
//
// TODO(rachelbarker): Test speed/quality impact of using bilinear interpolation
// instad of bicubic interpolation
static INLINE void compute_flow_vector(const uint8_t *src, const uint8_t *ref,
                                       int width, int height, int stride, int x,
                                       int y, double u, double v,
                                       const int16_t *dx, const int16_t *dy,
                                       int *b) {
  // This function is written to do 8x8 convolutions only
  assert(DISFLOW_PATCH_SIZE == 8);

  // Accumulate 4 32-bit partial sums for each element of b
  // These will be flattened at the end.
  __m128i b0_acc = _mm_setzero_si128();
  __m128i b1_acc = _mm_setzero_si128();

  // Split offset into integer and fractional parts, and compute cubic
  // interpolation kernels
  const int u_int = (int)floor(u);
  const int v_int = (int)floor(v);
  const double u_frac = u - floor(u);
  const double v_frac = v - floor(v);

  const __m128i kernels = compute_cubic_kernels(u_frac, v_frac);

  // Storage for intermediate values between the two convolution directions
  DECLARE_ALIGNED(16, int16_t,
                  tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 3)]);
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
  __m128i h_kernel_01 = _mm_set1_epi32(_mm_extract_epi32(kernels, 0));
  __m128i h_kernel_23 = _mm_set1_epi32(_mm_extract_epi32(kernels, 2));

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
  }

  // Vertical convolution
  const int round_bits = DISFLOW_INTERP_BITS + 6 - DISFLOW_DERIV_SCALE_LOG2;
  __m128i round_const_v = _mm_set1_epi32(1 << (round_bits - 1));

  __m128i v_kernel_01 = _mm_set1_epi32(_mm_extract_epi32(kernels, 1));
  __m128i v_kernel_23 = _mm_set1_epi32(_mm_extract_epi32(kernels, 3));

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
    __m128i dt = _mm_sub_epi16(warped, src_pixels);

    // Load 8 elements each of dx and dt, to pair with the 8 elements of dt
    // that we have just computed. Then compute 8 partial sums of dx * dt
    // and dy * dt, implicitly sum to give 4 partial sums of each, and
    // accumulate.
    __m128i dx_row = _mm_loadu_si128((__m128i *)&dx[i * DISFLOW_PATCH_SIZE]);
    __m128i dy_row = _mm_loadu_si128((__m128i *)&dy[i * DISFLOW_PATCH_SIZE]);
    b0_acc = _mm_add_epi32(b0_acc, _mm_madd_epi16(dx_row, dt));
    b1_acc = _mm_add_epi32(b1_acc, _mm_madd_epi16(dy_row, dt));
  }

  // Flatten the two sets of partial sums to find the final value of b
  // We need to set b[0] = sum(b0_acc), b[1] = sum(b1_acc).
  // We need to do 6 additions in total; a `hadd` instruction can take care
  // of four of them, leaving two scalar additions.
  __m128i partial_sum = _mm_hadd_epi32(b0_acc, b1_acc);
  b[0] = _mm_extract_epi32(partial_sum, 0) + _mm_extract_epi32(partial_sum, 1);
  b[1] = _mm_extract_epi32(partial_sum, 2) + _mm_extract_epi32(partial_sum, 3);
}

// Compute the x and y gradients of the source patch in a single pass,
// and store into dx and dy respectively.
static INLINE void sobel_filter(const uint8_t *src, int src_stride, int16_t *dx,
                                int16_t *dy) {
  // Loop setup: Load the first two rows (of 10 input rows) and apply
  // the horizontal parts of the two filters
  __m128i row_m1 = _mm_loadu_si128((__m128i *)(src - src_stride - 1));
  __m128i row_m1_a = _mm_cvtepu8_epi16(row_m1);
  __m128i row_m1_b = _mm_cvtepu8_epi16(_mm_srli_si128(row_m1, 1));
  __m128i row_m1_c = _mm_cvtepu8_epi16(_mm_srli_si128(row_m1, 2));

  __m128i row_m1_hsmooth = _mm_add_epi16(_mm_add_epi16(row_m1_a, row_m1_c),
                                         _mm_slli_epi16(row_m1_b, 1));
  __m128i row_m1_hdiff = _mm_sub_epi16(row_m1_a, row_m1_c);

  __m128i row = _mm_loadu_si128((__m128i *)(src - 1));
  __m128i row_a = _mm_cvtepu8_epi16(row);
  __m128i row_b = _mm_cvtepu8_epi16(_mm_srli_si128(row, 1));
  __m128i row_c = _mm_cvtepu8_epi16(_mm_srli_si128(row, 2));

  __m128i row_hsmooth =
      _mm_add_epi16(_mm_add_epi16(row_a, row_c), _mm_slli_epi16(row_b, 1));
  __m128i row_hdiff = _mm_sub_epi16(row_a, row_c);

  // Main loop: For each of the 8 output rows:
  // * Load row i+1 and apply both horizontal filters
  // * Apply vertical filters and store results
  // * Shift rows for next iteration
  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    // Load row i+1 and apply both horizontal filters
    const __m128i row_p1 =
        _mm_loadu_si128((__m128i *)(src + (i + 1) * src_stride - 1));
    const __m128i row_p1_a = _mm_cvtepu8_epi16(row_p1);
    const __m128i row_p1_b = _mm_cvtepu8_epi16(_mm_srli_si128(row_p1, 1));
    const __m128i row_p1_c = _mm_cvtepu8_epi16(_mm_srli_si128(row_p1, 2));

    const __m128i row_p1_hsmooth = _mm_add_epi16(
        _mm_add_epi16(row_p1_a, row_p1_c), _mm_slli_epi16(row_p1_b, 1));
    const __m128i row_p1_hdiff = _mm_sub_epi16(row_p1_a, row_p1_c);

    // Apply vertical filters and store results
    // dx = vertical smooth(horizontal diff(input))
    // dy = vertical diff(horizontal smooth(input))
    const __m128i dx_row =
        _mm_add_epi16(_mm_add_epi16(row_m1_hdiff, row_p1_hdiff),
                      _mm_slli_epi16(row_hdiff, 1));
    const __m128i dy_row = _mm_sub_epi16(row_m1_hsmooth, row_p1_hsmooth);

    _mm_storeu_si128((__m128i *)(dx + i * DISFLOW_PATCH_SIZE), dx_row);
    _mm_storeu_si128((__m128i *)(dy + i * DISFLOW_PATCH_SIZE), dy_row);

    // Shift rows for next iteration
    // This allows a lot of work to be reused, reducing the number of
    // horizontal filtering operations from 2*3*8 = 48 to 2*10 = 20
    row_m1_hsmooth = row_hsmooth;
    row_m1_hdiff = row_hdiff;
    row_hsmooth = row_p1_hsmooth;
    row_hdiff = row_p1_hdiff;
  }
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
  DECLARE_ALIGNED(16, double, M[4]);
  DECLARE_ALIGNED(16, double, M_inv[4]);
  DECLARE_ALIGNED(16, int16_t, dx[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE]);
  DECLARE_ALIGNED(16, int16_t, dy[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE]);
  int b[2];

  // Compute gradients within this patch
  const uint8_t *src_patch = &src[y * stride + x];
  sobel_filter(src_patch, stride, dx, dy);

  compute_flow_matrix(dx, DISFLOW_PATCH_SIZE, dy, DISFLOW_PATCH_SIZE, M);
  invert_2x2(M, M_inv);

  for (int itr = 0; itr < DISFLOW_MAX_ITR; itr++) {
    compute_flow_vector(src, ref, width, height, stride, x, y, *u, *v, dx, dy,
                        b);

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
