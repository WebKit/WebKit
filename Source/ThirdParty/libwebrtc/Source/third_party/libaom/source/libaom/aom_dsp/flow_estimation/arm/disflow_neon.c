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

#include "aom_dsp/flow_estimation/disflow.h"

#include <arm_neon.h>
#include <math.h>

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static INLINE void get_cubic_kernel_dbl(double x, double *kernel) {
  assert(0 <= x && x < 1);
  double x2 = x * x;
  double x3 = x2 * x;
  kernel[0] = -0.5 * x + x2 - 0.5 * x3;
  kernel[1] = 1.0 - 2.5 * x2 + 1.5 * x3;
  kernel[2] = 0.5 * x + 2.0 * x2 - 1.5 * x3;
  kernel[3] = -0.5 * x2 + 0.5 * x3;
}

static INLINE void get_cubic_kernel_int(double x, int *kernel) {
  double kernel_dbl[4];
  get_cubic_kernel_dbl(x, kernel_dbl);

  kernel[0] = (int)rint(kernel_dbl[0] * (1 << DISFLOW_INTERP_BITS));
  kernel[1] = (int)rint(kernel_dbl[1] * (1 << DISFLOW_INTERP_BITS));
  kernel[2] = (int)rint(kernel_dbl[2] * (1 << DISFLOW_INTERP_BITS));
  kernel[3] = (int)rint(kernel_dbl[3] * (1 << DISFLOW_INTERP_BITS));
}

// Compare two regions of width x height pixels, one rooted at position
// (x, y) in src and the other at (x + u, y + v) in ref.
// This function returns the sum of squared pixel differences between
// the two regions.
static INLINE void compute_flow_error(const uint8_t *src, const uint8_t *ref,
                                      int width, int height, int stride, int x,
                                      int y, double u, double v, int16_t *dt) {
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

  int16_t tmp_[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 3)];

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

  // Horizontal convolution.
  const uint8_t *ref_start = ref + (y0 - 1) * stride + (x0 - 1);
  int16x4_t h_filter = vmovn_s32(vld1q_s32(h_kernel));

  for (int i = 0; i < DISFLOW_PATCH_SIZE + 3; ++i) {
    uint8x16_t r = vld1q_u8(ref_start + i * stride);
    uint16x8_t r0 = vmovl_u8(vget_low_u8(r));
    uint16x8_t r1 = vmovl_u8(vget_high_u8(r));

    int16x8_t s0 = vreinterpretq_s16_u16(r0);
    int16x8_t s1 = vreinterpretq_s16_u16(vextq_u16(r0, r1, 1));
    int16x8_t s2 = vreinterpretq_s16_u16(vextq_u16(r0, r1, 2));
    int16x8_t s3 = vreinterpretq_s16_u16(vextq_u16(r0, r1, 3));

    int32x4_t sum_lo = vmull_lane_s16(vget_low_s16(s0), h_filter, 0);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s1), h_filter, 1);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s2), h_filter, 2);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(s3), h_filter, 3);

    int32x4_t sum_hi = vmull_lane_s16(vget_high_s16(s0), h_filter, 0);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s1), h_filter, 1);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s2), h_filter, 2);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(s3), h_filter, 3);

    // 6 is the maximum allowable number of extra bits which will avoid
    // the intermediate values overflowing an int16_t. The most extreme
    // intermediate value occurs when:
    // * The input pixels are [0, 255, 255, 0]
    // * u_frac = 0.5
    // In this case, the un-scaled output is 255 * 1.125 = 286.875.
    // As an integer with 6 fractional bits, that is 18360, which fits
    // in an int16_t. But with 7 fractional bits it would be 36720,
    // which is too large.

    int16x8_t sum = vcombine_s16(vrshrn_n_s32(sum_lo, DISFLOW_INTERP_BITS - 6),
                                 vrshrn_n_s32(sum_hi, DISFLOW_INTERP_BITS - 6));
    vst1q_s16(tmp_ + i * DISFLOW_PATCH_SIZE, sum);
  }

  // Vertical convolution.
  int16x4_t v_filter = vmovn_s32(vld1q_s32(v_kernel));
  int16_t *tmp_start = tmp_ + DISFLOW_PATCH_SIZE;

  for (int i = 0; i < DISFLOW_PATCH_SIZE; ++i) {
    int16x8_t t0 = vld1q_s16(tmp_start + (i - 1) * DISFLOW_PATCH_SIZE);
    int16x8_t t1 = vld1q_s16(tmp_start + i * DISFLOW_PATCH_SIZE);
    int16x8_t t2 = vld1q_s16(tmp_start + (i + 1) * DISFLOW_PATCH_SIZE);
    int16x8_t t3 = vld1q_s16(tmp_start + (i + 2) * DISFLOW_PATCH_SIZE);

    int32x4_t sum_lo = vmull_lane_s16(vget_low_s16(t0), v_filter, 0);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(t1), v_filter, 1);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(t2), v_filter, 2);
    sum_lo = vmlal_lane_s16(sum_lo, vget_low_s16(t3), v_filter, 3);

    int32x4_t sum_hi = vmull_lane_s16(vget_high_s16(t0), v_filter, 0);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(t1), v_filter, 1);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(t2), v_filter, 2);
    sum_hi = vmlal_lane_s16(sum_hi, vget_high_s16(t3), v_filter, 3);

    uint8x8_t s = vld1_u8(src + (i + y) * stride + x);
    int16x8_t s_s16 = vreinterpretq_s16_u16(vshll_n_u8(s, 3));

    // This time, we have to round off the 6 extra bits which were kept
    // earlier, but we also want to keep DISFLOW_DERIV_SCALE_LOG2 extra bits
    // of precision to match the scale of the dx and dy arrays.
    sum_lo = vrshrq_n_s32(sum_lo,
                          DISFLOW_INTERP_BITS + 6 - DISFLOW_DERIV_SCALE_LOG2);
    sum_hi = vrshrq_n_s32(sum_hi,
                          DISFLOW_INTERP_BITS + 6 - DISFLOW_DERIV_SCALE_LOG2);
    int32x4_t err_lo = vsubw_s16(sum_lo, vget_low_s16(s_s16));
    int32x4_t err_hi = vsubw_s16(sum_hi, vget_high_s16(s_s16));
    vst1q_s16(dt + i * DISFLOW_PATCH_SIZE,
              vcombine_s16(vmovn_s32(err_lo), vmovn_s32(err_hi)));
  }
}

static INLINE void sobel_filter_x(const uint8_t *src, int src_stride,
                                  int16_t *dst, int dst_stride) {
  int16_t tmp[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 2)];

  // Horizontal filter, using kernel {1, 0, -1}.
  const uint8_t *src_start = src - 1 * src_stride - 1;

  for (int i = 0; i < DISFLOW_PATCH_SIZE + 2; i++) {
    uint8x16_t s = vld1q_u8(src_start + i * src_stride);
    uint8x8_t s0 = vget_low_u8(s);
    uint8x8_t s2 = vget_low_u8(vextq_u8(s, s, 2));

    // Given that the kernel is {1, 0, -1} the convolution is a simple
    // subtraction.
    int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(s0, s2));

    vst1q_s16(tmp + i * DISFLOW_PATCH_SIZE, diff);
  }

  // Vertical filter, using kernel {1, 2, 1}.
  // This kernel can be split into two 2-taps kernels of value {1, 1}.
  // That way we need only 3 add operations to perform the convolution, one of
  // which can be reused for the next line.
  int16x8_t s0 = vld1q_s16(tmp);
  int16x8_t s1 = vld1q_s16(tmp + DISFLOW_PATCH_SIZE);
  int16x8_t sum01 = vaddq_s16(s0, s1);
  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    int16x8_t s2 = vld1q_s16(tmp + (i + 2) * DISFLOW_PATCH_SIZE);

    int16x8_t sum12 = vaddq_s16(s1, s2);
    int16x8_t sum = vaddq_s16(sum01, sum12);

    vst1q_s16(dst + i * dst_stride, sum);

    sum01 = sum12;
    s1 = s2;
  }
}

static INLINE void sobel_filter_y(const uint8_t *src, int src_stride,
                                  int16_t *dst, int dst_stride) {
  int16_t tmp[DISFLOW_PATCH_SIZE * (DISFLOW_PATCH_SIZE + 2)];

  // Horizontal filter, using kernel {1, 2, 1}.
  // This kernel can be split into two 2-taps kernels of value {1, 1}.
  // That way we need only 3 add operations to perform the convolution.
  const uint8_t *src_start = src - 1 * src_stride - 1;

  for (int i = 0; i < DISFLOW_PATCH_SIZE + 2; i++) {
    uint8x16_t s = vld1q_u8(src_start + i * src_stride);
    uint8x8_t s0 = vget_low_u8(s);
    uint8x8_t s1 = vget_low_u8(vextq_u8(s, s, 1));
    uint8x8_t s2 = vget_low_u8(vextq_u8(s, s, 2));

    uint16x8_t sum01 = vaddl_u8(s0, s1);
    uint16x8_t sum12 = vaddl_u8(s1, s2);
    uint16x8_t sum = vaddq_u16(sum01, sum12);

    vst1q_s16(tmp + i * DISFLOW_PATCH_SIZE, vreinterpretq_s16_u16(sum));
  }

  // Vertical filter, using kernel {1, 0, -1}.
  // Load the whole block at once to avoid redundant loads during convolution.
  int16x8_t t[10];
  load_s16_8x10(tmp, DISFLOW_PATCH_SIZE, &t[0], &t[1], &t[2], &t[3], &t[4],
                &t[5], &t[6], &t[7], &t[8], &t[9]);

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    // Given that the kernel is {1, 0, -1} the convolution is a simple
    // subtraction.
    int16x8_t diff = vsubq_s16(t[i], t[i + 2]);

    vst1q_s16(dst + i * dst_stride, diff);
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
                                       double *M_inv) {
  int32x4_t sum[4] = { vdupq_n_s32(0), vdupq_n_s32(0), vdupq_n_s32(0),
                       vdupq_n_s32(0) };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    int16x8_t x = vld1q_s16(dx + i * dx_stride);
    int16x8_t y = vld1q_s16(dy + i * dy_stride);
    sum[0] = vmlal_s16(sum[0], vget_low_s16(x), vget_low_s16(x));
    sum[0] = vmlal_s16(sum[0], vget_high_s16(x), vget_high_s16(x));

    sum[1] = vmlal_s16(sum[1], vget_low_s16(x), vget_low_s16(y));
    sum[1] = vmlal_s16(sum[1], vget_high_s16(x), vget_high_s16(y));

    sum[3] = vmlal_s16(sum[3], vget_low_s16(y), vget_low_s16(y));
    sum[3] = vmlal_s16(sum[3], vget_high_s16(y), vget_high_s16(y));
  }
  sum[2] = sum[1];

  int32x4_t res = horizontal_add_4d_s32x4(sum);

  // Apply regularization
  // We follow the standard regularization method of adding `k * I` before
  // inverting. This ensures that the matrix will be invertible.
  //
  // Setting the regularization strength k to 1 seems to work well here, as
  // typical values coming from the other equations are very large (1e5 to
  // 1e6, with an upper limit of around 6e7, at the time of writing).
  // It also preserves the property that all matrix values are whole numbers,
  // which is convenient for integerized SIMD implementation.

  double M0 = (double)vgetq_lane_s32(res, 0) + 1;
  double M1 = (double)vgetq_lane_s32(res, 1);
  double M2 = (double)vgetq_lane_s32(res, 2);
  double M3 = (double)vgetq_lane_s32(res, 3) + 1;

  // Invert matrix M.
  double det = (M0 * M3) - (M1 * M2);
  assert(det >= 1);
  const double det_inv = 1 / det;

  M_inv[0] = M3 * det_inv;
  M_inv[1] = -M1 * det_inv;
  M_inv[2] = -M2 * det_inv;
  M_inv[3] = M0 * det_inv;
}

static INLINE void compute_flow_vector(const int16_t *dx, int dx_stride,
                                       const int16_t *dy, int dy_stride,
                                       const int16_t *dt, int dt_stride,
                                       int *b) {
  int32x4_t b_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    int16x8_t dx16 = vld1q_s16(dx + i * dx_stride);
    int16x8_t dy16 = vld1q_s16(dy + i * dy_stride);
    int16x8_t dt16 = vld1q_s16(dt + i * dt_stride);

    b_s32[0] = vmlal_s16(b_s32[0], vget_low_s16(dx16), vget_low_s16(dt16));
    b_s32[0] = vmlal_s16(b_s32[0], vget_high_s16(dx16), vget_high_s16(dt16));

    b_s32[1] = vmlal_s16(b_s32[1], vget_low_s16(dy16), vget_low_s16(dt16));
    b_s32[1] = vmlal_s16(b_s32[1], vget_high_s16(dy16), vget_high_s16(dt16));
  }

  int32x4_t b_red = horizontal_add_2d_s32(b_s32[0], b_s32[1]);
  vst1_s32(b, add_pairwise_s32x4(b_red));
}

void aom_compute_flow_at_point_neon(const uint8_t *src, const uint8_t *ref,
                                    int x, int y, int width, int height,
                                    int stride, double *u, double *v) {
  double M_inv[4];
  int b[2];
  int16_t dt[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];
  int16_t dx[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];
  int16_t dy[DISFLOW_PATCH_SIZE * DISFLOW_PATCH_SIZE];

  // Compute gradients within this patch
  const uint8_t *src_patch = &src[y * stride + x];
  sobel_filter_x(src_patch, stride, dx, DISFLOW_PATCH_SIZE);
  sobel_filter_y(src_patch, stride, dy, DISFLOW_PATCH_SIZE);

  compute_flow_matrix(dx, DISFLOW_PATCH_SIZE, dy, DISFLOW_PATCH_SIZE, M_inv);

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
