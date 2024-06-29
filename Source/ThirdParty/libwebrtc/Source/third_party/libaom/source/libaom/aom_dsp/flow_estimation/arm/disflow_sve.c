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

#include "aom_dsp/flow_estimation/disflow.h"

#include <arm_neon.h>
#include <arm_sve.h>
#include <math.h>

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/flow_estimation/arm/disflow_neon.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

DECLARE_ALIGNED(16, static const uint16_t, kDeinterleaveTbl[8]) = {
  0, 2, 4, 6, 1, 3, 5, 7,
};

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
  const int16x4_t h_kernel_s16 = vmovn_s32(vld1q_s32(h_kernel));
  const int16x8_t h_filter = vcombine_s16(h_kernel_s16, vdup_n_s16(0));
  const uint16x8_t idx = vld1q_u16(kDeinterleaveTbl);

  for (int i = 0; i < DISFLOW_PATCH_SIZE + 3; ++i) {
    svuint16_t r0 = svld1ub_u16(svptrue_b16(), ref_start + i * stride + 0);
    svuint16_t r1 = svld1ub_u16(svptrue_b16(), ref_start + i * stride + 1);
    svuint16_t r2 = svld1ub_u16(svptrue_b16(), ref_start + i * stride + 2);
    svuint16_t r3 = svld1ub_u16(svptrue_b16(), ref_start + i * stride + 3);

    int16x8_t s0 = vreinterpretq_s16_u16(svget_neonq_u16(r0));
    int16x8_t s1 = vreinterpretq_s16_u16(svget_neonq_u16(r1));
    int16x8_t s2 = vreinterpretq_s16_u16(svget_neonq_u16(r2));
    int16x8_t s3 = vreinterpretq_s16_u16(svget_neonq_u16(r3));

    int64x2_t sum04 = aom_svdot_lane_s16(vdupq_n_s64(0), s0, h_filter, 0);
    int64x2_t sum15 = aom_svdot_lane_s16(vdupq_n_s64(0), s1, h_filter, 0);
    int64x2_t sum26 = aom_svdot_lane_s16(vdupq_n_s64(0), s2, h_filter, 0);
    int64x2_t sum37 = aom_svdot_lane_s16(vdupq_n_s64(0), s3, h_filter, 0);

    int32x4_t res0 = vcombine_s32(vmovn_s64(sum04), vmovn_s64(sum15));
    int32x4_t res1 = vcombine_s32(vmovn_s64(sum26), vmovn_s64(sum37));

    // 6 is the maximum allowable number of extra bits which will avoid
    // the intermediate values overflowing an int16_t. The most extreme
    // intermediate value occurs when:
    // * The input pixels are [0, 255, 255, 0]
    // * u_frac = 0.5
    // In this case, the un-scaled output is 255 * 1.125 = 286.875.
    // As an integer with 6 fractional bits, that is 18360, which fits
    // in an int16_t. But with 7 fractional bits it would be 36720,
    // which is too large.
    int16x8_t res = vcombine_s16(vrshrn_n_s32(res0, DISFLOW_INTERP_BITS - 6),
                                 vrshrn_n_s32(res1, DISFLOW_INTERP_BITS - 6));

    res = aom_tbl_s16(res, idx);

    vst1q_s16(tmp_ + i * DISFLOW_PATCH_SIZE, res);
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
  int64x2_t sum[3] = { vdupq_n_s64(0), vdupq_n_s64(0), vdupq_n_s64(0) };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    int16x8_t x = vld1q_s16(dx + i * dx_stride);
    int16x8_t y = vld1q_s16(dy + i * dy_stride);

    sum[0] = aom_sdotq_s16(sum[0], x, x);
    sum[1] = aom_sdotq_s16(sum[1], x, y);
    sum[2] = aom_sdotq_s16(sum[2], y, y);
  }

  sum[0] = vpaddq_s64(sum[0], sum[1]);
  sum[2] = vpaddq_s64(sum[1], sum[2]);
  int32x4_t res = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[2]));

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
  int64x2_t b_s64[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  for (int i = 0; i < DISFLOW_PATCH_SIZE; i++) {
    int16x8_t dx16 = vld1q_s16(dx + i * dx_stride);
    int16x8_t dy16 = vld1q_s16(dy + i * dy_stride);
    int16x8_t dt16 = vld1q_s16(dt + i * dt_stride);

    b_s64[0] = aom_sdotq_s16(b_s64[0], dx16, dt16);
    b_s64[1] = aom_sdotq_s16(b_s64[1], dy16, dt16);
  }

  b_s64[0] = vpaddq_s64(b_s64[0], b_s64[1]);
  vst1_s32(b, vmovn_s64(b_s64[0]));
}

void aom_compute_flow_at_point_sve(const uint8_t *src, const uint8_t *ref,
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
