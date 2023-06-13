/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include "config/aom_dsp_rtcd.h"

#include "av1/encoder/global_motion.h"

#include "av1/common/convolve.h"
#include "av1/common/warped_motion.h"

#include "av1/encoder/segmentation.h"

#define MIN_TRANS_THRESH (1 * GM_TRANS_DECODE_FACTOR)

// Border over which to compute the global motion
#define ERRORADV_BORDER 0

int av1_is_enough_erroradvantage(double best_erroradvantage, int params_cost) {
  return best_erroradvantage < erroradv_tr &&
         best_erroradvantage * params_cost < erroradv_prod_tr;
}

static void convert_to_params(const double *params, int32_t *model) {
  int i;
  model[0] = (int32_t)floor(params[0] * (1 << GM_TRANS_PREC_BITS) + 0.5);
  model[1] = (int32_t)floor(params[1] * (1 << GM_TRANS_PREC_BITS) + 0.5);
  model[0] = (int32_t)clamp(model[0], GM_TRANS_MIN, GM_TRANS_MAX) *
             GM_TRANS_DECODE_FACTOR;
  model[1] = (int32_t)clamp(model[1], GM_TRANS_MIN, GM_TRANS_MAX) *
             GM_TRANS_DECODE_FACTOR;

  for (i = 2; i < 6; ++i) {
    const int diag_value = ((i == 2 || i == 5) ? (1 << GM_ALPHA_PREC_BITS) : 0);
    model[i] = (int32_t)floor(params[i] * (1 << GM_ALPHA_PREC_BITS) + 0.5);
    model[i] =
        (int32_t)clamp(model[i] - diag_value, GM_ALPHA_MIN, GM_ALPHA_MAX);
    model[i] = (model[i] + diag_value) * GM_ALPHA_DECODE_FACTOR;
  }
}

void av1_convert_model_to_params(const double *params,
                                 WarpedMotionParams *model) {
  convert_to_params(params, model->wmmat);
  model->wmtype = get_wmtype(model);
  model->invalid = 0;
}

// Adds some offset to a global motion parameter and handles
// all of the necessary precision shifts, clamping, and
// zero-centering.
static int32_t add_param_offset(int param_index, int32_t param_value,
                                int32_t offset) {
  const int scale_vals[2] = { GM_TRANS_PREC_DIFF, GM_ALPHA_PREC_DIFF };
  const int clamp_vals[2] = { GM_TRANS_MAX, GM_ALPHA_MAX };
  // type of param: 0 - translation, 1 - affine
  const int param_type = (param_index < 2 ? 0 : 1);
  const int is_one_centered = (param_index == 2 || param_index == 5);

  // Make parameter zero-centered and offset the shift that was done to make
  // it compatible with the warped model
  param_value = (param_value - (is_one_centered << WARPEDMODEL_PREC_BITS)) >>
                scale_vals[param_type];
  // Add desired offset to the rescaled/zero-centered parameter
  param_value += offset;
  // Clamp the parameter so it does not overflow the number of bits allotted
  // to it in the bitstream
  param_value = (int32_t)clamp(param_value, -clamp_vals[param_type],
                               clamp_vals[param_type]);
  // Rescale the parameter to WARPEDMODEL_PRECISION_BITS so it is compatible
  // with the warped motion library
  param_value *= (1 << scale_vals[param_type]);

  // Undo the zero-centering step if necessary
  return param_value + (is_one_centered << WARPEDMODEL_PREC_BITS);
}

static void force_wmtype(WarpedMotionParams *wm, TransformationType wmtype) {
  switch (wmtype) {
    case IDENTITY:
      wm->wmmat[0] = 0;
      wm->wmmat[1] = 0;
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      wm->wmmat[2] = 1 << WARPEDMODEL_PREC_BITS;
      wm->wmmat[3] = 0;
      AOM_FALLTHROUGH_INTENDED;
    case ROTZOOM:
      wm->wmmat[4] = -wm->wmmat[3];
      wm->wmmat[5] = wm->wmmat[2];
      AOM_FALLTHROUGH_INTENDED;
    case AFFINE: break;
    default: assert(0);
  }
  wm->wmtype = wmtype;
}

#if CONFIG_AV1_HIGHBITDEPTH
static int64_t highbd_warp_error(
    WarpedMotionParams *wm, const uint16_t *const ref, int width, int height,
    int stride, const uint16_t *const dst, int p_col, int p_row, int p_width,
    int p_height, int p_stride, int subsampling_x, int subsampling_y, int bd,
    int64_t best_error, uint8_t *segment_map, int segment_map_stride) {
  int64_t gm_sumerr = 0;
  const int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  const int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  uint16_t tmp[WARP_ERROR_BLOCK * WARP_ERROR_BLOCK];

  ConvolveParams conv_params = get_conv_params(0, 0, bd);
  conv_params.use_dist_wtd_comp_avg = 0;
  for (int i = p_row; i < p_row + p_height; i += WARP_ERROR_BLOCK) {
    for (int j = p_col; j < p_col + p_width; j += WARP_ERROR_BLOCK) {
      int seg_x = j >> WARP_ERROR_BLOCK_LOG;
      int seg_y = i >> WARP_ERROR_BLOCK_LOG;
      // Only compute the error if this block contains inliers from the motion
      // model
      if (!segment_map[seg_y * segment_map_stride + seg_x]) continue;
      // avoid warping extra 8x8 blocks in the padded region of the frame
      // when p_width and p_height are not multiples of WARP_ERROR_BLOCK
      const int warp_w = AOMMIN(error_bsize_w, p_col + p_width - j);
      const int warp_h = AOMMIN(error_bsize_h, p_row + p_height - i);
      highbd_warp_plane(wm, ref, width, height, stride, tmp, j, i, warp_w,
                        warp_h, WARP_ERROR_BLOCK, subsampling_x, subsampling_y,
                        bd, &conv_params);
      gm_sumerr += av1_calc_highbd_frame_error(tmp, WARP_ERROR_BLOCK,
                                               dst + j + i * p_stride, warp_w,
                                               warp_h, p_stride, bd);
      if (gm_sumerr > best_error) return INT64_MAX;
    }
  }
  return gm_sumerr;
}
#endif

static int64_t warp_error(WarpedMotionParams *wm, const uint8_t *const ref,
                          int width, int height, int stride,
                          const uint8_t *const dst, int p_col, int p_row,
                          int p_width, int p_height, int p_stride,
                          int subsampling_x, int subsampling_y,
                          int64_t best_error, uint8_t *segment_map,
                          int segment_map_stride) {
  int64_t gm_sumerr = 0;
  int warp_w, warp_h;
  const int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  const int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  DECLARE_ALIGNED(16, uint8_t, tmp[WARP_ERROR_BLOCK * WARP_ERROR_BLOCK]);
  ConvolveParams conv_params = get_conv_params(0, 0, 8);
  conv_params.use_dist_wtd_comp_avg = 0;

  for (int i = p_row; i < p_row + p_height; i += WARP_ERROR_BLOCK) {
    for (int j = p_col; j < p_col + p_width; j += WARP_ERROR_BLOCK) {
      int seg_x = j >> WARP_ERROR_BLOCK_LOG;
      int seg_y = i >> WARP_ERROR_BLOCK_LOG;
      // Only compute the error if this block contains inliers from the motion
      // model
      if (!segment_map[seg_y * segment_map_stride + seg_x]) continue;
      // avoid warping extra 8x8 blocks in the padded region of the frame
      // when p_width and p_height are not multiples of WARP_ERROR_BLOCK
      warp_w = AOMMIN(error_bsize_w, p_col + p_width - j);
      warp_h = AOMMIN(error_bsize_h, p_row + p_height - i);
      warp_plane(wm, ref, width, height, stride, tmp, j, i, warp_w, warp_h,
                 WARP_ERROR_BLOCK, subsampling_x, subsampling_y, &conv_params);

      gm_sumerr +=
          av1_calc_frame_error(tmp, WARP_ERROR_BLOCK, dst + j + i * p_stride,
                               warp_w, warp_h, p_stride);
      if (gm_sumerr > best_error) return INT64_MAX;
    }
  }
  return gm_sumerr;
}

int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int64_t best_error,
                       uint8_t *segment_map, int segment_map_stride) {
  force_wmtype(wm, wm->wmtype);
  assert(wm->wmtype <= AFFINE);
  if (!av1_get_shear_params(wm)) return INT64_MAX;
#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd)
    return highbd_warp_error(wm, CONVERT_TO_SHORTPTR(ref), width, height,
                             stride, CONVERT_TO_SHORTPTR(dst), p_col, p_row,
                             p_width, p_height, p_stride, subsampling_x,
                             subsampling_y, bd, best_error, segment_map,
                             segment_map_stride);
#endif
  (void)use_hbd;
  (void)bd;
  return warp_error(wm, ref, width, height, stride, dst, p_col, p_row, p_width,
                    p_height, p_stride, subsampling_x, subsampling_y,
                    best_error, segment_map, segment_map_stride);
}

// Factors used to calculate the thresholds for av1_warp_error
static double thresh_factors[GM_MAX_REFINEMENT_STEPS] = { 1.25, 1.20, 1.15,
                                                          1.10, 1.05 };

static INLINE int64_t calc_approx_erroradv_threshold(
    double scaling_factor, int64_t erroradv_threshold) {
  return erroradv_threshold <
                 (int64_t)(((double)INT64_MAX / scaling_factor) + 0.5)
             ? (int64_t)(scaling_factor * erroradv_threshold + 0.5)
             : INT64_MAX;
}

int64_t av1_refine_integerized_param(
    WarpedMotionParams *wm, TransformationType wmtype, int use_hbd, int bd,
    uint8_t *ref, int r_width, int r_height, int r_stride, uint8_t *dst,
    int d_width, int d_height, int d_stride, int n_refinements,
    int64_t best_frame_error, uint8_t *segment_map, int segment_map_stride,
    int64_t erroradv_threshold) {
  static const int max_trans_model_params[TRANS_TYPES] = { 0, 2, 4, 6 };
  const int border = ERRORADV_BORDER;
  int i = 0, p;
  int n_params = max_trans_model_params[wmtype];
  int32_t *param_mat = wm->wmmat;
  int64_t step_error, best_error;
  int32_t step;
  int32_t *param;
  int32_t curr_param;
  int32_t best_param;

  force_wmtype(wm, wmtype);
  best_error =
      av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                     dst + border * d_stride + border, border, border,
                     d_width - 2 * border, d_height - 2 * border, d_stride, 0,
                     0, best_frame_error, segment_map, segment_map_stride);

  if (n_refinements == 0) {
    wm->wmtype = get_wmtype(wm);
    return best_error;
  }

  best_error = AOMMIN(best_error, best_frame_error);
  step = 1 << (n_refinements - 1);
  for (i = 0; i < n_refinements; i++, step >>= 1) {
    int64_t error_adv_thresh =
        calc_approx_erroradv_threshold(thresh_factors[i], erroradv_threshold);
    for (p = 0; p < n_params; ++p) {
      int step_dir = 0;
      // Skip searches for parameters that are forced to be 0
      param = param_mat + p;
      curr_param = *param;
      best_param = curr_param;
      // look to the left
      *param = add_param_offset(p, curr_param, -step);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, border, border,
                         d_width - 2 * border, d_height - 2 * border, d_stride,
                         0, 0, AOMMIN(best_error, error_adv_thresh),
                         segment_map, segment_map_stride);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = -1;
      }

      // look to the right
      *param = add_param_offset(p, curr_param, step);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, border, border,
                         d_width - 2 * border, d_height - 2 * border, d_stride,
                         0, 0, AOMMIN(best_error, error_adv_thresh),
                         segment_map, segment_map_stride);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = 1;
      }
      *param = best_param;

      // look to the direction chosen above repeatedly until error increases
      // for the biggest step size
      while (step_dir) {
        *param = add_param_offset(p, best_param, step * step_dir);
        step_error =
            av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                           dst + border * d_stride + border, border, border,
                           d_width - 2 * border, d_height - 2 * border,
                           d_stride, 0, 0, AOMMIN(best_error, error_adv_thresh),
                           segment_map, segment_map_stride);
        if (step_error < best_error) {
          best_error = step_error;
          best_param = *param;
        } else {
          *param = best_param;
          step_dir = 0;
        }
      }
    }
  }
  force_wmtype(wm, wmtype);
  wm->wmtype = get_wmtype(wm);
  return best_error;
}

#define FEAT_COUNT_TR 3
#define SEG_COUNT_TR 48
void av1_compute_feature_segmentation_map(uint8_t *segment_map, int width,
                                          int height, int *inliers,
                                          int num_inliers) {
  int seg_count = 0;
  memset(segment_map, 0, sizeof(*segment_map) * width * height);

  for (int i = 0; i < num_inliers; i++) {
    int x = inliers[i * 2];
    int y = inliers[i * 2 + 1];
    int seg_x = x >> WARP_ERROR_BLOCK_LOG;
    int seg_y = y >> WARP_ERROR_BLOCK_LOG;
    segment_map[seg_y * width + seg_x] += 1;
  }

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      uint8_t feat_count = segment_map[i * width + j];
      segment_map[i * width + j] = (feat_count >= FEAT_COUNT_TR);
      seg_count += (segment_map[i * width + j]);
    }
  }

  // If this motion does not make up a large enough portion of the frame,
  // use the unsegmented version of the error metric
  if (seg_count < SEG_COUNT_TR)
    memset(segment_map, 1, width * height * sizeof(*segment_map));
}
