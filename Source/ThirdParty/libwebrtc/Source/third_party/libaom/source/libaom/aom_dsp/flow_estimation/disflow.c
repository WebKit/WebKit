/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Dense Inverse Search flow algorithm
// Paper: https://arxiv.org/abs/1603.03590

#include <assert.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/flow_estimation/corner_detect.h"
#include "aom_dsp/flow_estimation/disflow.h"
#include "aom_dsp/flow_estimation/ransac.h"
#include "aom_dsp/pyramid.h"
#include "aom_mem/aom_mem.h"

#include "config/aom_dsp_rtcd.h"

// Amount to downsample the flow field by.
// e.g., DOWNSAMPLE_SHIFT = 2 (DOWNSAMPLE_FACTOR == 4) means we calculate
// one flow point for each 4x4 pixel region of the frame
// Must be a power of 2
#define DOWNSAMPLE_SHIFT 3
#define DOWNSAMPLE_FACTOR (1 << DOWNSAMPLE_SHIFT)

// Filters used when upscaling the flow field from one pyramid level
// to another. See upscale_flow_component for details on kernel selection
#define FLOW_UPSCALE_TAPS 4

// Number of outermost flow field entries (on each edge) which can't be
// computed, because the patch they correspond to extends outside of the
// frame
// The border is (DISFLOW_PATCH_SIZE >> 1) pixels, which is
// (DISFLOW_PATCH_SIZE >> 1) >> DOWNSAMPLE_SHIFT many flow field entries
#define FLOW_BORDER_INNER ((DISFLOW_PATCH_SIZE >> 1) >> DOWNSAMPLE_SHIFT)

// Number of extra padding entries on each side of the flow field.
// These samples are added so that we do not need to apply clamping when
// interpolating or upsampling the flow field
#define FLOW_BORDER_OUTER (FLOW_UPSCALE_TAPS / 2)

// When downsampling the flow field, each flow field entry covers a square
// region of pixels in the image pyramid. This value is equal to the position
// of the center of that region, as an offset from the top/left edge.
//
// Note: Using ((DOWNSAMPLE_FACTOR - 1) / 2) is equivalent to the more
// natural expression ((DOWNSAMPLE_FACTOR / 2) - 1),
// unless DOWNSAMPLE_FACTOR == 1 (ie, no downsampling), in which case
// this gives the correct offset of 0 instead of -1.
#define UPSAMPLE_CENTER_OFFSET ((DOWNSAMPLE_FACTOR - 1) / 2)

static double flow_upscale_filter[2][FLOW_UPSCALE_TAPS] = {
  // Cubic interpolation kernels for phase=0.75 and phase=0.25, respectively
  { -3 / 128., 29 / 128., 111 / 128., -9 / 128. },
  { -9 / 128., 111 / 128., 29 / 128., -3 / 128. }
};

static INLINE void get_cubic_kernel_dbl(double x, double kernel[4]) {
  // Check that the fractional position is in range.
  //
  // Note: x is calculated from, e.g., `u_frac = u - floor(u)`.
  // Mathematically, this implies that 0 <= x < 1. However, in practice it is
  // possible to have x == 1 due to floating point rounding. This is fine,
  // and we still interpolate correctly if we allow x = 1.
  assert(0 <= x && x <= 1);

  double x2 = x * x;
  double x3 = x2 * x;
  kernel[0] = -0.5 * x + x2 - 0.5 * x3;
  kernel[1] = 1.0 - 2.5 * x2 + 1.5 * x3;
  kernel[2] = 0.5 * x + 2.0 * x2 - 1.5 * x3;
  kernel[3] = -0.5 * x2 + 0.5 * x3;
}

static INLINE void get_cubic_kernel_int(double x, int kernel[4]) {
  double kernel_dbl[4];
  get_cubic_kernel_dbl(x, kernel_dbl);

  kernel[0] = (int)rint(kernel_dbl[0] * (1 << DISFLOW_INTERP_BITS));
  kernel[1] = (int)rint(kernel_dbl[1] * (1 << DISFLOW_INTERP_BITS));
  kernel[2] = (int)rint(kernel_dbl[2] * (1 << DISFLOW_INTERP_BITS));
  kernel[3] = (int)rint(kernel_dbl[3] * (1 << DISFLOW_INTERP_BITS));
}

static INLINE double get_cubic_value_dbl(const double *p,
                                         const double kernel[4]) {
  return kernel[0] * p[0] + kernel[1] * p[1] + kernel[2] * p[2] +
         kernel[3] * p[3];
}

static INLINE int get_cubic_value_int(const int *p, const int kernel[4]) {
  return kernel[0] * p[0] + kernel[1] * p[1] + kernel[2] * p[2] +
         kernel[3] * p[3];
}

static INLINE double bicubic_interp_one(const double *arr, int stride,
                                        const double h_kernel[4],
                                        const double v_kernel[4]) {
  double tmp[1 * 4];

  // Horizontal convolution
  for (int i = -1; i < 3; ++i) {
    tmp[i + 1] = get_cubic_value_dbl(&arr[i * stride - 1], h_kernel);
  }

  // Vertical convolution
  return get_cubic_value_dbl(tmp, v_kernel);
}

static int determine_disflow_correspondence(const ImagePyramid *src_pyr,
                                            const ImagePyramid *ref_pyr,
                                            CornerList *corners,
                                            const FlowField *flow,
                                            Correspondence *correspondences) {
  const int width = flow->width;
  const int height = flow->height;
  const int stride = flow->stride;

  int num_correspondences = 0;
  for (int i = 0; i < corners->num_corners; ++i) {
    const int x0 = corners->corners[2 * i];
    const int y0 = corners->corners[2 * i + 1];

    // Offset points, to compensate for the fact that (say) a flow field entry
    // at horizontal index i, is nominally associated with the pixel at
    // horizontal coordinate (i << DOWNSAMPLE_FACTOR) + UPSAMPLE_CENTER_OFFSET
    // This offset must be applied before we split the coordinate into integer
    // and fractional parts, in order for the interpolation to be correct.
    const int x = x0 - UPSAMPLE_CENTER_OFFSET;
    const int y = y0 - UPSAMPLE_CENTER_OFFSET;

    // Split the pixel coordinates into integer flow field coordinates and
    // an offset for interpolation
    const int flow_x = x >> DOWNSAMPLE_SHIFT;
    const double flow_sub_x =
        (x & (DOWNSAMPLE_FACTOR - 1)) / (double)DOWNSAMPLE_FACTOR;
    const int flow_y = y >> DOWNSAMPLE_SHIFT;
    const double flow_sub_y =
        (y & (DOWNSAMPLE_FACTOR - 1)) / (double)DOWNSAMPLE_FACTOR;

    // Exclude points which would sample from the outer border of the flow
    // field, as this would give lower-quality results.
    //
    // Note: As we never read from the border region at pyramid level 0, we
    // can skip filling it in. If the conditions here are removed, or any
    // other logic is added which reads from this border region, then
    // compute_flow_field() will need to be modified to call
    // fill_flow_field_borders() at pyramid level 0 to set up the correct
    // border data.
    if (flow_x < 1 || (flow_x + 2) >= width) continue;
    if (flow_y < 1 || (flow_y + 2) >= height) continue;

    double h_kernel[4];
    double v_kernel[4];
    get_cubic_kernel_dbl(flow_sub_x, h_kernel);
    get_cubic_kernel_dbl(flow_sub_y, v_kernel);

    double flow_u = bicubic_interp_one(&flow->u[flow_y * stride + flow_x],
                                       stride, h_kernel, v_kernel);
    double flow_v = bicubic_interp_one(&flow->v[flow_y * stride + flow_x],
                                       stride, h_kernel, v_kernel);

    // Refine the interpolated flow vector one last time
    const int patch_tl_x = x0 - DISFLOW_PATCH_CENTER;
    const int patch_tl_y = y0 - DISFLOW_PATCH_CENTER;
    aom_compute_flow_at_point(
        src_pyr->layers[0].buffer, ref_pyr->layers[0].buffer, patch_tl_x,
        patch_tl_y, src_pyr->layers[0].width, src_pyr->layers[0].height,
        src_pyr->layers[0].stride, &flow_u, &flow_v);

    // Use original points (without offsets) when filling in correspondence
    // array
    correspondences[num_correspondences].x = x0;
    correspondences[num_correspondences].y = y0;
    correspondences[num_correspondences].rx = x0 + flow_u;
    correspondences[num_correspondences].ry = y0 + flow_v;
    num_correspondences++;
  }
  return num_correspondences;
}

// Compare two regions of width x height pixels, one rooted at position
// (x, y) in src and the other at (x + u, y + v) in ref.
// This function returns the sum of squared pixel differences between
// the two regions.
static INLINE void compute_flow_vector(const uint8_t *src, const uint8_t *ref,
                                       int width, int height, int stride, int x,
                                       int y, double u, double v,
                                       const int16_t *dx, const int16_t *dy,
                                       int *b) {
  memset(b, 0, 2 * sizeof(*b));

  // Split offset into integer and fractional parts, and compute cubic
  // interpolation kernels
  const int u_int = (int)floor(u);
  const int v_int = (int)floor(v);
  const double u_frac = u - floor(u);
  const double v_frac = v - floor(v);

  int h_kernel[4];
  int v_kernel[4];
  get_cubic_kernel_int(u_frac, h_kernel);
  get_cubic_kernel_int(v_frac, v_kernel);

  // Storage for intermediate values between the two convolution directions
  int tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 3)];
  int *tmp = tmp_ + DISFLOW_PATCH_SIZE;  // Offset by one row

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
  for (int i = -1; i < DISFLOW_PATCH_SIZE + 2; ++i) {
    const int y_w = y0 + i;
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
      tmp[i * DISFLOW_PATCH_SIZE + j] = ROUND_POWER_OF_TWO(
          get_cubic_value_int(arr, h_kernel), DISFLOW_INTERP_BITS - 6);
    }
  }

  // Vertical convolution
  for (int i = 0; i < DISFLOW_PATCH_SIZE; ++i) {
    for (int j = 0; j < DISFLOW_PATCH_SIZE; ++j) {
      const int *p = &tmp[i * DISFLOW_PATCH_SIZE + j];
      const int arr[4] = { p[-DISFLOW_PATCH_SIZE], p[0], p[DISFLOW_PATCH_SIZE],
                           p[2 * DISFLOW_PATCH_SIZE] };
      const int result = get_cubic_value_int(arr, v_kernel);

      // Apply kernel and round.
      // This time, we have to round off the 6 extra bits which were kept
      // earlier, but we also want to keep DISFLOW_DERIV_SCALE_LOG2 extra bits
      // of precision to match the scale of the dx and dy arrays.
      const int round_bits = DISFLOW_INTERP_BITS + 6 - DISFLOW_DERIV_SCALE_LOG2;
      const int warped = ROUND_POWER_OF_TWO(result, round_bits);
      const int src_px = src[(x + j) + (y + i) * stride] << 3;
      const int dt = warped - src_px;
      b[0] += dx[i * DISFLOW_PATCH_SIZE + j] * dt;
      b[1] += dy[i * DISFLOW_PATCH_SIZE + j] * dt;
    }
  }
}

static INLINE void sobel_filter(const uint8_t *src, int src_stride,
                                int16_t *dst, int dst_stride, int dir) {
  int16_t tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 2)];
  int16_t *tmp = tmp_ + DISFLOW_PATCH_SIZE;

  // Sobel filter kernel
  // This must have an overall scale factor equal to DISFLOW_DERIV_SCALE,
  // in order to produce correctly scaled outputs.
  // To work out the scale factor, we multiply two factors:
  //
  // * For the derivative filter (sobel_a), comparing our filter
  //    image[x - 1] - image[x + 1]
  //   to the standard form
  //    d/dx image[x] = image[x+1] - image[x]
  //   tells us that we're actually calculating -2 * d/dx image[2]
  //
  // * For the smoothing filter (sobel_b), all coefficients are positive
  //   so the scale factor is just the sum of the coefficients
  //
  // Thus we need to make sure that DISFLOW_DERIV_SCALE = 2 * sum(sobel_b)
  // (and take care of the - sign from sobel_a elsewhere)
  static const int16_t sobel_a[3] = { 1, 0, -1 };
  static const int16_t sobel_b[3] = { 1, 2, 1 };
  const int taps = 3;

  // horizontal filter
  const int16_t *h_kernel = dir ? sobel_a : sobel_b;

  for (int y = -1; y < DISFLOW_PATCH_SIZE + 1; ++y) {
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += h_kernel[k] * src[y * src_stride + (x + k - 1)];
      }
      tmp[y * DISFLOW_PATCH_SIZE + x] = sum;
    }
  }

  // vertical filter
  const int16_t *v_kernel = dir ? sobel_b : sobel_a;

  for (int y = 0; y < DISFLOW_PATCH_SIZE; ++y) {
    for (int x = 0; x < DISFLOW_PATCH_SIZE; ++x) {
      int sum = 0;
      for (int k = 0; k < taps; ++k) {
        sum += v_kernel[k] * tmp[(y + k - 1) * DISFLOW_PATCH_SIZE + x];
      }
      dst[y * dst_stride + x] = sum;
    }
  }
}

// Computes the components of the system of equations used to solve for
// a flow vector.
//
// The flow equations are a least-squares system, derived as follows:
//
// For each pixel in the patch, we calculate the current error `dt`,
// and the x and y gradients `dx` and `dy` of the source patch.
// This means that, to first order, the squared error for this pixel is
//
//    (dt + u * dx + v * dy)^2
//
// where (u, v) are the incremental changes to the flow vector.
//
// We then want to find the values of u and v which minimize the sum
// of the squared error across all pixels. Conveniently, this fits exactly
// into the form of a least squares problem, with one equation
//
//   u * dx + v * dy = -dt
//
// for each pixel.
//
// Summing across all pixels in a square window of size DISFLOW_PATCH_SIZE,
// and absorbing the - sign elsewhere, this results in the least squares system
//
//   M = |sum(dx * dx)  sum(dx * dy)|
//       |sum(dx * dy)  sum(dy * dy)|
//
//   b = |sum(dx * dt)|
//       |sum(dy * dt)|
static INLINE void compute_flow_matrix(const int16_t *dx, int dx_stride,
                                       const int16_t *dy, int dy_stride,
                                       double *M) {
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
  // We follow the standard regularization method of adding `k * I` before
  // inverting. This ensures that the matrix will be invertible.
  //
  // Setting the regularization strength k to 1 seems to work well here, as
  // typical values coming from the other equations are very large (1e5 to
  // 1e6, with an upper limit of around 6e7, at the time of writing).
  // It also preserves the property that all matrix values are whole numbers,
  // which is convenient for integerized SIMD implementation.
  tmp[0] += 1;
  tmp[3] += 1;

  tmp[2] = tmp[1];

  M[0] = (double)tmp[0];
  M[1] = (double)tmp[1];
  M[2] = (double)tmp[2];
  M[3] = (double)tmp[3];
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

void aom_compute_flow_at_point_c(const uint8_t *src, const uint8_t *ref, int x,
                                 int y, int width, int height, int stride,
                                 double *u, double *v) {
  double M[4];
  double M_inv[4];
  int b[2];
  int16_t dx[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];
  int16_t dy[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];

  // Compute gradients within this patch
  const uint8_t *src_patch = &src[y * stride + x];
  sobel_filter(src_patch, stride, dx, DISFLOW_PATCH_SIZE, 1);
  sobel_filter(src_patch, stride, dy, DISFLOW_PATCH_SIZE, 0);

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

static void fill_flow_field_borders(double *flow, int width, int height,
                                    int stride) {
  // Calculate the bounds of the rectangle which was filled in by
  // compute_flow_field() before calling this function.
  // These indices are inclusive on both ends.
  const int left_index = FLOW_BORDER_INNER;
  const int right_index = (width - FLOW_BORDER_INNER - 1);
  const int top_index = FLOW_BORDER_INNER;
  const int bottom_index = (height - FLOW_BORDER_INNER - 1);

  // Left area
  for (int i = top_index; i <= bottom_index; i += 1) {
    double *row = flow + i * stride;
    const double left = row[left_index];
    for (int j = -FLOW_BORDER_OUTER; j < left_index; j++) {
      row[j] = left;
    }
  }

  // Right area
  for (int i = top_index; i <= bottom_index; i += 1) {
    double *row = flow + i * stride;
    const double right = row[right_index];
    for (int j = right_index + 1; j < width + FLOW_BORDER_OUTER; j++) {
      row[j] = right;
    }
  }

  // Top area
  const double *top_row = flow + top_index * stride - FLOW_BORDER_OUTER;
  for (int i = -FLOW_BORDER_OUTER; i < top_index; i++) {
    double *row = flow + i * stride - FLOW_BORDER_OUTER;
    size_t length = width + 2 * FLOW_BORDER_OUTER;
    memcpy(row, top_row, length * sizeof(*row));
  }

  // Bottom area
  const double *bottom_row = flow + bottom_index * stride - FLOW_BORDER_OUTER;
  for (int i = bottom_index + 1; i < height + FLOW_BORDER_OUTER; i++) {
    double *row = flow + i * stride - FLOW_BORDER_OUTER;
    size_t length = width + 2 * FLOW_BORDER_OUTER;
    memcpy(row, bottom_row, length * sizeof(*row));
  }
}

// Upscale one component of the flow field, from a size of
// cur_width x cur_height to a size of (2*cur_width) x (2*cur_height), storing
// the result back into the same buffer. This function also scales the flow
// vector by 2, so that when we move to the next pyramid level down, the implied
// motion vector is the same.
//
// The temporary buffer tmpbuf must be large enough to hold an intermediate
// array of size stride * cur_height, *plus* FLOW_BORDER_OUTER rows above and
// below. In other words, indices from -FLOW_BORDER_OUTER * stride to
// (cur_height + FLOW_BORDER_OUTER) * stride - 1 must be valid.
//
// Note that the same stride is used for u before and after upscaling
// and for the temporary buffer, for simplicity.
//
// A note on phasing:
//
// The flow fields at two adjacent pyramid levels are offset from each other,
// and we need to account for this in the construction of the interpolation
// kernels.
//
// Consider an 8x8 pixel patch at pyramid level n. This is split into four
// patches at pyramid level n-1. Bringing these patches back up to pyramid level
// n, each sub-patch covers 4x4 pixels, and between them they cover the same
// 8x8 region.
//
// Therefore, at pyramid level n, two adjacent patches look like this:
//
//    + - - - - - - - + - - - - - - - +
//    |               |               |
//    |   x       x   |   x       x   |
//    |               |               |
//    |       #       |       #       |
//    |               |               |
//    |   x       x   |   x       x   |
//    |               |               |
//    + - - - - - - - + - - - - - - - +
//
// where # marks the center of a patch at pyramid level n (the input to this
// function), and x marks the center of a patch at pyramid level n-1 (the output
// of this function).
//
// By counting pixels (marked by +, -, and |), we can see that the flow vectors
// at pyramid level n-1 are offset relative to the flow vectors at pyramid
// level n, by 1/4 of the larger (input) patch size. Therefore, our
// interpolation kernels need to have phases of 0.25 and 0.75.
//
// In addition, in order to handle the frame edges correctly, we need to
// generate one output vector to the left and one to the right of each input
// vector, even though these must be interpolated using different source points.
static void upscale_flow_component(double *flow, int cur_width, int cur_height,
                                   int stride, double *tmpbuf) {
  const int half_len = FLOW_UPSCALE_TAPS / 2;

  // Check that the outer border is large enough to avoid needing to clamp
  // the source locations
  assert(half_len <= FLOW_BORDER_OUTER);

  // Horizontal upscale and multiply by 2
  for (int i = 0; i < cur_height; i++) {
    for (int j = 0; j < cur_width; j++) {
      double left = 0;
      for (int k = -half_len; k < half_len; k++) {
        left +=
            flow[i * stride + (j + k)] * flow_upscale_filter[0][k + half_len];
      }
      tmpbuf[i * stride + (2 * j + 0)] = 2.0 * left;

      // Right output pixel is 0.25 units to the right of the input pixel
      double right = 0;
      for (int k = -(half_len - 1); k < (half_len + 1); k++) {
        right += flow[i * stride + (j + k)] *
                 flow_upscale_filter[1][k + (half_len - 1)];
      }
      tmpbuf[i * stride + (2 * j + 1)] = 2.0 * right;
    }
  }

  // Fill in top and bottom borders of tmpbuf
  const double *top_row = &tmpbuf[0];
  for (int i = -FLOW_BORDER_OUTER; i < 0; i++) {
    double *row = &tmpbuf[i * stride];
    memcpy(row, top_row, 2 * cur_width * sizeof(*row));
  }

  const double *bottom_row = &tmpbuf[(cur_height - 1) * stride];
  for (int i = cur_height; i < cur_height + FLOW_BORDER_OUTER; i++) {
    double *row = &tmpbuf[i * stride];
    memcpy(row, bottom_row, 2 * cur_width * sizeof(*row));
  }

  // Vertical upscale
  int upscaled_width = cur_width * 2;
  for (int i = 0; i < cur_height; i++) {
    for (int j = 0; j < upscaled_width; j++) {
      double top = 0;
      for (int k = -half_len; k < half_len; k++) {
        top +=
            tmpbuf[(i + k) * stride + j] * flow_upscale_filter[0][k + half_len];
      }
      flow[(2 * i) * stride + j] = top;

      double bottom = 0;
      for (int k = -(half_len - 1); k < (half_len + 1); k++) {
        bottom += tmpbuf[(i + k) * stride + j] *
                  flow_upscale_filter[1][k + (half_len - 1)];
      }
      flow[(2 * i + 1) * stride + j] = bottom;
    }
  }
}

// make sure flow_u and flow_v start at 0
static bool compute_flow_field(const ImagePyramid *src_pyr,
                               const ImagePyramid *ref_pyr, int n_levels,
                               FlowField *flow) {
  bool mem_status = true;

  double *flow_u = flow->u;
  double *flow_v = flow->v;

  double *tmpbuf0;
  double *tmpbuf;

  if (n_levels < 2) {
    // tmpbuf not needed
    tmpbuf0 = NULL;
    tmpbuf = NULL;
  } else {
    // This line must match the calculation of cur_flow_height below
    const int layer1_height = src_pyr->layers[1].height >> DOWNSAMPLE_SHIFT;

    const size_t tmpbuf_size =
        (layer1_height + 2 * FLOW_BORDER_OUTER) * flow->stride;
    tmpbuf0 = aom_malloc(tmpbuf_size * sizeof(*tmpbuf0));
    if (!tmpbuf0) {
      mem_status = false;
      goto free_tmpbuf;
    }
    tmpbuf = tmpbuf0 + FLOW_BORDER_OUTER * flow->stride;
  }

  // Compute flow field from coarsest to finest level of the pyramid
  //
  // Note: We stop after refining pyramid level 1 and interpolating it to
  // generate an initial flow field at level 0. We do *not* refine the dense
  // flow field at level 0. Instead, we wait until we have generated
  // correspondences by interpolating this flow field, and then refine the
  // correspondences themselves. This is both faster and gives better output
  // compared to refining the flow field at level 0 and then interpolating.
  for (int level = n_levels - 1; level >= 1; --level) {
    const PyramidLayer *cur_layer = &src_pyr->layers[level];
    const int cur_width = cur_layer->width;
    const int cur_height = cur_layer->height;
    const int cur_stride = cur_layer->stride;

    const uint8_t *src_buffer = cur_layer->buffer;
    const uint8_t *ref_buffer = ref_pyr->layers[level].buffer;

    const int cur_flow_width = cur_width >> DOWNSAMPLE_SHIFT;
    const int cur_flow_height = cur_height >> DOWNSAMPLE_SHIFT;
    const int cur_flow_stride = flow->stride;

    for (int i = FLOW_BORDER_INNER; i < cur_flow_height - FLOW_BORDER_INNER;
         i += 1) {
      for (int j = FLOW_BORDER_INNER; j < cur_flow_width - FLOW_BORDER_INNER;
           j += 1) {
        const int flow_field_idx = i * cur_flow_stride + j;

        // Calculate the position of a patch of size DISFLOW_PATCH_SIZE pixels,
        // which is centered on the region covered by this flow field entry
        const int patch_center_x =
            (j << DOWNSAMPLE_SHIFT) + UPSAMPLE_CENTER_OFFSET;  // In pixels
        const int patch_center_y =
            (i << DOWNSAMPLE_SHIFT) + UPSAMPLE_CENTER_OFFSET;  // In pixels
        const int patch_tl_x = patch_center_x - DISFLOW_PATCH_CENTER;
        const int patch_tl_y = patch_center_y - DISFLOW_PATCH_CENTER;
        assert(patch_tl_x >= 0);
        assert(patch_tl_y >= 0);

        aom_compute_flow_at_point(src_buffer, ref_buffer, patch_tl_x,
                                  patch_tl_y, cur_width, cur_height, cur_stride,
                                  &flow_u[flow_field_idx],
                                  &flow_v[flow_field_idx]);
      }
    }

    // Fill in the areas which we haven't explicitly computed, with copies
    // of the outermost values which we did compute
    fill_flow_field_borders(flow_u, cur_flow_width, cur_flow_height,
                            cur_flow_stride);
    fill_flow_field_borders(flow_v, cur_flow_width, cur_flow_height,
                            cur_flow_stride);

    if (level > 0) {
      const int upscale_flow_width = cur_flow_width << 1;
      const int upscale_flow_height = cur_flow_height << 1;
      const int upscale_stride = flow->stride;

      upscale_flow_component(flow_u, cur_flow_width, cur_flow_height,
                             cur_flow_stride, tmpbuf);
      upscale_flow_component(flow_v, cur_flow_width, cur_flow_height,
                             cur_flow_stride, tmpbuf);

      // If we didn't fill in the rightmost column or bottommost row during
      // upsampling (in order to keep the ratio to exactly 2), fill them
      // in here by copying the next closest column/row
      const PyramidLayer *next_layer = &src_pyr->layers[level - 1];
      const int next_flow_width = next_layer->width >> DOWNSAMPLE_SHIFT;
      const int next_flow_height = next_layer->height >> DOWNSAMPLE_SHIFT;

      // Rightmost column
      if (next_flow_width > upscale_flow_width) {
        assert(next_flow_width == upscale_flow_width + 1);
        for (int i = 0; i < upscale_flow_height; i++) {
          const int index = i * upscale_stride + upscale_flow_width;
          flow_u[index] = flow_u[index - 1];
          flow_v[index] = flow_v[index - 1];
        }
      }

      // Bottommost row
      if (next_flow_height > upscale_flow_height) {
        assert(next_flow_height == upscale_flow_height + 1);
        for (int j = 0; j < next_flow_width; j++) {
          const int index = upscale_flow_height * upscale_stride + j;
          flow_u[index] = flow_u[index - upscale_stride];
          flow_v[index] = flow_v[index - upscale_stride];
        }
      }
    }
  }

free_tmpbuf:
  aom_free(tmpbuf0);
  return mem_status;
}

static FlowField *alloc_flow_field(int frame_width, int frame_height) {
  FlowField *flow = (FlowField *)aom_malloc(sizeof(FlowField));
  if (flow == NULL) return NULL;

  // Calculate the size of the bottom (largest) layer of the flow pyramid
  flow->width = frame_width >> DOWNSAMPLE_SHIFT;
  flow->height = frame_height >> DOWNSAMPLE_SHIFT;
  flow->stride = flow->width + 2 * FLOW_BORDER_OUTER;

  const size_t flow_size =
      flow->stride * (size_t)(flow->height + 2 * FLOW_BORDER_OUTER);

  flow->buf0 = aom_calloc(2 * flow_size, sizeof(*flow->buf0));
  if (!flow->buf0) {
    aom_free(flow);
    return NULL;
  }

  flow->u = flow->buf0 + FLOW_BORDER_OUTER * flow->stride + FLOW_BORDER_OUTER;
  flow->v = flow->u + flow_size;

  return flow;
}

static void free_flow_field(FlowField *flow) {
  aom_free(flow->buf0);
  aom_free(flow);
}

// Compute flow field between `src` and `ref`, and then use that flow to
// compute a global motion model relating the two frames.
//
// Following the convention in flow_estimation.h, the flow vectors are computed
// at fixed points in `src` and point to the corresponding locations in `ref`,
// regardless of the temporal ordering of the frames.
bool av1_compute_global_motion_disflow(
    TransformationType type, YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *ref,
    int bit_depth, int downsample_level, MotionModel *motion_models,
    int num_motion_models, bool *mem_alloc_failed) {
  // Precompute information we will need about each frame
  ImagePyramid *src_pyramid = src->y_pyramid;
  CornerList *src_corners = src->corners;
  ImagePyramid *ref_pyramid = ref->y_pyramid;

  const int src_layers =
      aom_compute_pyramid(src, bit_depth, DISFLOW_PYRAMID_LEVELS, src_pyramid);
  const int ref_layers =
      aom_compute_pyramid(ref, bit_depth, DISFLOW_PYRAMID_LEVELS, ref_pyramid);

  if (src_layers < 0 || ref_layers < 0) {
    *mem_alloc_failed = true;
    return false;
  }
  if (!av1_compute_corner_list(src, bit_depth, downsample_level, src_corners)) {
    *mem_alloc_failed = true;
    return false;
  }

  assert(src_layers == ref_layers);

  const int src_width = src_pyramid->layers[0].width;
  const int src_height = src_pyramid->layers[0].height;
  assert(ref_pyramid->layers[0].width == src_width);
  assert(ref_pyramid->layers[0].height == src_height);

  FlowField *flow = alloc_flow_field(src_width, src_height);
  if (!flow) {
    *mem_alloc_failed = true;
    return false;
  }

  if (!compute_flow_field(src_pyramid, ref_pyramid, src_layers, flow)) {
    *mem_alloc_failed = true;
    free_flow_field(flow);
    return false;
  }

  // find correspondences between the two images using the flow field
  Correspondence *correspondences =
      aom_malloc(src_corners->num_corners * sizeof(*correspondences));
  if (!correspondences) {
    *mem_alloc_failed = true;
    free_flow_field(flow);
    return false;
  }

  const int num_correspondences = determine_disflow_correspondence(
      src_pyramid, ref_pyramid, src_corners, flow, correspondences);

  bool result = ransac(correspondences, num_correspondences, type,
                       motion_models, num_motion_models, mem_alloc_failed);

  aom_free(correspondences);
  free_flow_field(flow);
  return result;
}
