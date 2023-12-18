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

/* clang-format off */
// Error metric used for global motion evaluation.
// For 8-bit input, the pixel error used to index this table will always
// be between -255 and +255. But for 10- and 12-bit input, we use interpolation
// which means that we need to support indices of -256 and +256 as well.
// Therefore, the table is offset so that logical index 0 corresponds to
// error_measure_lut[256].
const int error_measure_lut[513] = {
    // pow 0.7
    16384, 16384, 16339, 16294, 16249, 16204, 16158, 16113,
    16068, 16022, 15977, 15932, 15886, 15840, 15795, 15749,
    15703, 15657, 15612, 15566, 15520, 15474, 15427, 15381,
    15335, 15289, 15242, 15196, 15149, 15103, 15056, 15010,
    14963, 14916, 14869, 14822, 14775, 14728, 14681, 14634,
    14587, 14539, 14492, 14445, 14397, 14350, 14302, 14254,
    14206, 14159, 14111, 14063, 14015, 13967, 13918, 13870,
    13822, 13773, 13725, 13676, 13628, 13579, 13530, 13481,
    13432, 13383, 13334, 13285, 13236, 13187, 13137, 13088,
    13038, 12988, 12939, 12889, 12839, 12789, 12739, 12689,
    12639, 12588, 12538, 12487, 12437, 12386, 12335, 12285,
    12234, 12183, 12132, 12080, 12029, 11978, 11926, 11875,
    11823, 11771, 11719, 11667, 11615, 11563, 11511, 11458,
    11406, 11353, 11301, 11248, 11195, 11142, 11089, 11036,
    10982, 10929, 10875, 10822, 10768, 10714, 10660, 10606,
    10552, 10497, 10443, 10388, 10333, 10279, 10224, 10168,
    10113, 10058, 10002,  9947,  9891,  9835,  9779,  9723,
     9666,  9610,  9553,  9497,  9440,  9383,  9326,  9268,
     9211,  9153,  9095,  9037,  8979,  8921,  8862,  8804,
     8745,  8686,  8627,  8568,  8508,  8449,  8389,  8329,
     8269,  8208,  8148,  8087,  8026,  7965,  7903,  7842,
     7780,  7718,  7656,  7593,  7531,  7468,  7405,  7341,
     7278,  7214,  7150,  7086,  7021,  6956,  6891,  6826,
     6760,  6695,  6628,  6562,  6495,  6428,  6361,  6293,
     6225,  6157,  6089,  6020,  5950,  5881,  5811,  5741,
     5670,  5599,  5527,  5456,  5383,  5311,  5237,  5164,
     5090,  5015,  4941,  4865,  4789,  4713,  4636,  4558,
     4480,  4401,  4322,  4242,  4162,  4080,  3998,  3916,
     3832,  3748,  3663,  3577,  3490,  3402,  3314,  3224,
     3133,  3041,  2948,  2854,  2758,  2661,  2562,  2461,
     2359,  2255,  2148,  2040,  1929,  1815,  1698,  1577,
     1452,  1323,  1187,  1045,   894,   731,   550,   339,
        0,   339,   550,   731,   894,  1045,  1187,  1323,
     1452,  1577,  1698,  1815,  1929,  2040,  2148,  2255,
     2359,  2461,  2562,  2661,  2758,  2854,  2948,  3041,
     3133,  3224,  3314,  3402,  3490,  3577,  3663,  3748,
     3832,  3916,  3998,  4080,  4162,  4242,  4322,  4401,
     4480,  4558,  4636,  4713,  4789,  4865,  4941,  5015,
     5090,  5164,  5237,  5311,  5383,  5456,  5527,  5599,
     5670,  5741,  5811,  5881,  5950,  6020,  6089,  6157,
     6225,  6293,  6361,  6428,  6495,  6562,  6628,  6695,
     6760,  6826,  6891,  6956,  7021,  7086,  7150,  7214,
     7278,  7341,  7405,  7468,  7531,  7593,  7656,  7718,
     7780,  7842,  7903,  7965,  8026,  8087,  8148,  8208,
     8269,  8329,  8389,  8449,  8508,  8568,  8627,  8686,
     8745,  8804,  8862,  8921,  8979,  9037,  9095,  9153,
     9211,  9268,  9326,  9383,  9440,  9497,  9553,  9610,
     9666,  9723,  9779,  9835,  9891,  9947, 10002, 10058,
    10113, 10168, 10224, 10279, 10333, 10388, 10443, 10497,
    10552, 10606, 10660, 10714, 10768, 10822, 10875, 10929,
    10982, 11036, 11089, 11142, 11195, 11248, 11301, 11353,
    11406, 11458, 11511, 11563, 11615, 11667, 11719, 11771,
    11823, 11875, 11926, 11978, 12029, 12080, 12132, 12183,
    12234, 12285, 12335, 12386, 12437, 12487, 12538, 12588,
    12639, 12689, 12739, 12789, 12839, 12889, 12939, 12988,
    13038, 13088, 13137, 13187, 13236, 13285, 13334, 13383,
    13432, 13481, 13530, 13579, 13628, 13676, 13725, 13773,
    13822, 13870, 13918, 13967, 14015, 14063, 14111, 14159,
    14206, 14254, 14302, 14350, 14397, 14445, 14492, 14539,
    14587, 14634, 14681, 14728, 14775, 14822, 14869, 14916,
    14963, 15010, 15056, 15103, 15149, 15196, 15242, 15289,
    15335, 15381, 15427, 15474, 15520, 15566, 15612, 15657,
    15703, 15749, 15795, 15840, 15886, 15932, 15977, 16022,
    16068, 16113, 16158, 16204, 16249, 16294, 16339, 16384,
    16384,
};
/* clang-format on */

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
int64_t av1_calc_highbd_frame_error_c(const uint16_t *const ref, int ref_stride,
                                      const uint16_t *const dst, int dst_stride,
                                      int p_width, int p_height, int bd) {
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; ++i) {
    for (int j = 0; j < p_width; ++j) {
      sum_error += highbd_error_measure(
          dst[j + i * dst_stride] - ref[j + i * ref_stride], bd);
    }
  }
  return sum_error;
}

static int64_t highbd_segmented_frame_error(
    const uint16_t *const ref, int ref_stride, const uint16_t *const dst,
    int dst_stride, int p_width, int p_height, int bd, uint8_t *segment_map,
    int segment_map_stride) {
  int patch_w, patch_h;
  const int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  const int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; i += WARP_ERROR_BLOCK) {
    for (int j = 0; j < p_width; j += WARP_ERROR_BLOCK) {
      int seg_x = j >> WARP_ERROR_BLOCK_LOG;
      int seg_y = i >> WARP_ERROR_BLOCK_LOG;
      // Only compute the error if this block contains inliers from the motion
      // model
      if (!segment_map[seg_y * segment_map_stride + seg_x]) continue;

      // avoid computing error into the frame padding
      patch_w = AOMMIN(error_bsize_w, p_width - j);
      patch_h = AOMMIN(error_bsize_h, p_height - i);
      sum_error += av1_calc_highbd_frame_error(
          ref + j + i * ref_stride, ref_stride, dst + j + i * dst_stride,
          dst_stride, patch_w, patch_h, bd);
    }
  }
  return sum_error;
}

static int64_t highbd_warp_error(WarpedMotionParams *wm,
                                 const uint16_t *const ref, int ref_width,
                                 int ref_height, int ref_stride,
                                 const uint16_t *const dst, int dst_stride,
                                 int p_col, int p_row, int p_width,
                                 int p_height, int subsampling_x,
                                 int subsampling_y, int bd, int64_t best_error,
                                 uint8_t *segment_map, int segment_map_stride) {
  int64_t gm_sumerr = 0;
  const int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  const int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  DECLARE_ALIGNED(32, uint16_t, tmp[WARP_ERROR_BLOCK * WARP_ERROR_BLOCK]);

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
      const int warp_w = AOMMIN(error_bsize_w, p_col + ref_width - j);
      const int warp_h = AOMMIN(error_bsize_h, p_row + ref_height - i);
      highbd_warp_plane(wm, ref, ref_width, ref_height, ref_stride, tmp, j, i,
                        warp_w, warp_h, WARP_ERROR_BLOCK, subsampling_x,
                        subsampling_y, bd, &conv_params);
      gm_sumerr += av1_calc_highbd_frame_error(tmp, WARP_ERROR_BLOCK,
                                               dst + j + i * dst_stride,
                                               dst_stride, warp_w, warp_h, bd);
      if (gm_sumerr > best_error) return INT64_MAX;
    }
  }
  return gm_sumerr;
}
#endif

int64_t av1_calc_frame_error_c(const uint8_t *const ref, int ref_stride,
                               const uint8_t *const dst, int dst_stride,
                               int p_width, int p_height) {
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; ++i) {
    for (int j = 0; j < p_width; ++j) {
      sum_error += (int64_t)error_measure(dst[j + i * dst_stride] -
                                          ref[j + i * ref_stride]);
    }
  }
  return sum_error;
}

static int64_t segmented_frame_error(const uint8_t *const ref, int ref_stride,
                                     const uint8_t *const dst, int dst_stride,
                                     int p_width, int p_height,
                                     uint8_t *segment_map,
                                     int segment_map_stride) {
  int patch_w, patch_h;
  const int error_bsize_w = AOMMIN(p_width, WARP_ERROR_BLOCK);
  const int error_bsize_h = AOMMIN(p_height, WARP_ERROR_BLOCK);
  int64_t sum_error = 0;
  for (int i = 0; i < p_height; i += WARP_ERROR_BLOCK) {
    for (int j = 0; j < p_width; j += WARP_ERROR_BLOCK) {
      int seg_x = j >> WARP_ERROR_BLOCK_LOG;
      int seg_y = i >> WARP_ERROR_BLOCK_LOG;
      // Only compute the error if this block contains inliers from the motion
      // model
      if (!segment_map[seg_y * segment_map_stride + seg_x]) continue;

      // avoid computing error into the frame padding
      patch_w = AOMMIN(error_bsize_w, p_width - j);
      patch_h = AOMMIN(error_bsize_h, p_height - i);
      sum_error += av1_calc_frame_error(ref + j + i * ref_stride, ref_stride,
                                        dst + j + i * dst_stride, dst_stride,
                                        patch_w, patch_h);
    }
  }
  return sum_error;
}

static int64_t warp_error(WarpedMotionParams *wm, const uint8_t *const ref,
                          int ref_width, int ref_height, int ref_stride,
                          const uint8_t *const dst, int dst_stride, int p_col,
                          int p_row, int p_width, int p_height,
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
      warp_w = AOMMIN(error_bsize_w, p_col + ref_width - j);
      warp_h = AOMMIN(error_bsize_h, p_row + ref_height - i);
      warp_plane(wm, ref, ref_width, ref_height, ref_stride, tmp, j, i, warp_w,
                 warp_h, WARP_ERROR_BLOCK, subsampling_x, subsampling_y,
                 &conv_params);

      gm_sumerr +=
          av1_calc_frame_error(tmp, WARP_ERROR_BLOCK, dst + j + i * dst_stride,
                               dst_stride, warp_w, warp_h);
      if (gm_sumerr > best_error) return INT64_MAX;
    }
  }
  return gm_sumerr;
}

int64_t av1_frame_error(int use_hbd, int bd, const uint8_t *ref, int ref_stride,
                        uint8_t *dst, int dst_stride, int p_width,
                        int p_height) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd) {
    return av1_calc_highbd_frame_error(CONVERT_TO_SHORTPTR(ref), ref_stride,
                                       CONVERT_TO_SHORTPTR(dst), dst_stride,
                                       p_width, p_height, bd);
  }
#endif
  (void)use_hbd;
  (void)bd;
  return av1_calc_frame_error(ref, ref_stride, dst, dst_stride, p_width,
                              p_height);
}

int64_t av1_segmented_frame_error(int use_hbd, int bd, const uint8_t *ref,
                                  int ref_stride, uint8_t *dst, int dst_stride,
                                  int p_width, int p_height,
                                  uint8_t *segment_map,
                                  int segment_map_stride) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd) {
    return highbd_segmented_frame_error(
        CONVERT_TO_SHORTPTR(ref), ref_stride, CONVERT_TO_SHORTPTR(dst),
        dst_stride, p_width, p_height, bd, segment_map, segment_map_stride);
  }
#endif
  (void)use_hbd;
  (void)bd;
  return segmented_frame_error(ref, ref_stride, dst, dst_stride, p_width,
                               p_height, segment_map, segment_map_stride);
}

int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int ref_width, int ref_height,
                       int ref_stride, uint8_t *dst, int dst_stride, int p_col,
                       int p_row, int p_width, int p_height, int subsampling_x,
                       int subsampling_y, int64_t best_error,
                       uint8_t *segment_map, int segment_map_stride) {
  if (!av1_get_shear_params(wm)) return INT64_MAX;
#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd)
    return highbd_warp_error(wm, CONVERT_TO_SHORTPTR(ref), ref_width,
                             ref_height, ref_stride, CONVERT_TO_SHORTPTR(dst),
                             dst_stride, p_col, p_row, p_width, p_height,
                             subsampling_x, subsampling_y, bd, best_error,
                             segment_map, segment_map_stride);
#endif
  (void)use_hbd;
  (void)bd;
  return warp_error(wm, ref, ref_width, ref_height, ref_stride, dst, dst_stride,
                    p_col, p_row, p_width, p_height, subsampling_x,
                    subsampling_y, best_error, segment_map, segment_map_stride);
}

int64_t av1_refine_integerized_param(
    WarpedMotionParams *wm, TransformationType wmtype, int use_hbd, int bd,
    uint8_t *ref, int r_width, int r_height, int r_stride, uint8_t *dst,
    int d_width, int d_height, int d_stride, int n_refinements,
    int64_t ref_frame_error, uint8_t *segment_map, int segment_map_stride) {
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
  wm->wmtype = get_wmtype(wm);

  if (n_refinements == 0) {
    // Compute the maximum error value that will be accepted, so that
    // av1_warp_error can terminate early if it proves the model will not
    // be accepted.
    int64_t selection_threshold = (int64_t)lrint(ref_frame_error * erroradv_tr);
    return av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                          dst + border * d_stride + border, d_stride, border,
                          border, d_width - 2 * border, d_height - 2 * border,
                          0, 0, selection_threshold, segment_map,
                          segment_map_stride);
  }

  // When refining, use a slightly higher threshold for the initial error
  // calculation - see comment above erroradv_early_tr for why.
  int64_t selection_threshold =
      (int64_t)lrint(ref_frame_error * erroradv_early_tr);
  best_error =
      av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                     dst + border * d_stride + border, d_stride, border, border,
                     d_width - 2 * border, d_height - 2 * border, 0, 0,
                     selection_threshold, segment_map, segment_map_stride);

  if (best_error > selection_threshold) {
    return INT64_MAX;
  }

  step = 1 << (n_refinements - 1);
  for (i = 0; i < n_refinements; i++, step >>= 1) {
    for (p = 0; p < n_params; ++p) {
      int step_dir = 0;
      param = param_mat + p;
      curr_param = *param;
      best_param = curr_param;
      // look to the left
      // Note: We have to use force_wmtype() to keep the proper symmetry for
      // ROTZOOM type models
      *param = add_param_offset(p, curr_param, -step);
      force_wmtype(wm, wmtype);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, d_stride, border,
                         border, d_width - 2 * border, d_height - 2 * border, 0,
                         0, best_error, segment_map, segment_map_stride);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = -1;
      }

      // look to the right
      *param = add_param_offset(p, curr_param, step);
      force_wmtype(wm, wmtype);
      step_error =
          av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                         dst + border * d_stride + border, d_stride, border,
                         border, d_width - 2 * border, d_height - 2 * border, 0,
                         0, best_error, segment_map, segment_map_stride);
      if (step_error < best_error) {
        best_error = step_error;
        best_param = *param;
        step_dir = 1;
      }

      // look to the direction chosen above repeatedly until error increases
      // for the biggest step size
      while (step_dir) {
        *param = add_param_offset(p, best_param, step * step_dir);
        force_wmtype(wm, wmtype);
        step_error =
            av1_warp_error(wm, use_hbd, bd, ref, r_width, r_height, r_stride,
                           dst + border * d_stride + border, d_stride, border,
                           border, d_width - 2 * border, d_height - 2 * border,
                           0, 0, best_error, segment_map, segment_map_stride);
        if (step_error < best_error) {
          best_error = step_error;
          best_param = *param;
        } else {
          step_dir = 0;
        }
      }

      // Restore best parameter value so far
      *param = best_param;
      force_wmtype(wm, wmtype);
    }
  }

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
