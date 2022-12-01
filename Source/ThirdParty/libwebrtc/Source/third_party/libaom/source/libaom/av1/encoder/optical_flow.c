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
#include <math.h>
#include <limits.h>

#include "config/aom_config.h"

#include "aom_dsp/mathutils.h"
#include "aom_mem/aom_mem.h"

#include "av1/common/av1_common_int.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/optical_flow.h"
#include "av1/encoder/sparse_linear_solver.h"
#include "av1/encoder/reconinter_enc.h"

#if CONFIG_OPTICAL_FLOW_API

void av1_init_opfl_params(OPFL_PARAMS *opfl_params) {
  opfl_params->pyramid_levels = OPFL_PYRAMID_LEVELS;
  opfl_params->warping_steps = OPFL_WARPING_STEPS;
  opfl_params->lk_params = NULL;
}

void av1_init_lk_params(LK_PARAMS *lk_params) {
  lk_params->window_size = OPFL_WINDOW_SIZE;
}

// Helper function to determine whether a frame is encoded with high bit-depth.
static INLINE int is_frame_high_bitdepth(const YV12_BUFFER_CONFIG *frame) {
  return (frame->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
}

// Helper function to determine whether optical flow method is sparse.
static INLINE int is_sparse(const OPFL_PARAMS *opfl_params) {
  return (opfl_params->flags & OPFL_FLAG_SPARSE) ? 1 : 0;
}

static void gradients_over_window(const YV12_BUFFER_CONFIG *frame,
                                  const YV12_BUFFER_CONFIG *ref_frame,
                                  const double x_coord, const double y_coord,
                                  const int window_size, const int bit_depth,
                                  double *ix, double *iy, double *it,
                                  LOCALMV *mv);

// coefficients for bilinear interpolation on unit square
static int pixel_interp(const double x, const double y, const double b00,
                        const double b01, const double b10, const double b11) {
  const int xint = (int)x;
  const int yint = (int)y;
  const double xdec = x - xint;
  const double ydec = y - yint;
  const double a = (1 - xdec) * (1 - ydec);
  const double b = xdec * (1 - ydec);
  const double c = (1 - xdec) * ydec;
  const double d = xdec * ydec;
  // if x, y are already integers, this results to b00
  int interp = (int)round(a * b00 + b * b01 + c * b10 + d * b11);
  return interp;
}

// Scharr filter to compute spatial gradient
static void spatial_gradient(const YV12_BUFFER_CONFIG *frame, const int x_coord,
                             const int y_coord, const int direction,
                             double *derivative) {
  double *filter;
  // Scharr filters
  double gx[9] = { -3, 0, 3, -10, 0, 10, -3, 0, 3 };
  double gy[9] = { -3, -10, -3, 0, 0, 0, 3, 10, 3 };
  if (direction == 0) {  // x direction
    filter = gx;
  } else {  // y direction
    filter = gy;
  }
  int idx = 0;
  double d = 0;
  for (int yy = -1; yy <= 1; yy++) {
    for (int xx = -1; xx <= 1; xx++) {
      d += filter[idx] *
           frame->y_buffer[(y_coord + yy) * frame->y_stride + (x_coord + xx)];
      idx++;
    }
  }
  // normalization scaling factor for scharr
  *derivative = d / 32.0;
}

// Determine the spatial gradient at subpixel locations
// For example, when reducing images for pyramidal LK,
// corners found in original image may be at subpixel locations.
static void gradient_interp(double *fullpel_deriv, const double x_coord,
                            const double y_coord, const int w, const int h,
                            double *derivative) {
  const int xint = (int)x_coord;
  const int yint = (int)y_coord;
  double interp;
  if (xint + 1 > w - 1 || yint + 1 > h - 1) {
    interp = fullpel_deriv[yint * w + xint];
  } else {
    interp = pixel_interp(x_coord, y_coord, fullpel_deriv[yint * w + xint],
                          fullpel_deriv[yint * w + (xint + 1)],
                          fullpel_deriv[(yint + 1) * w + xint],
                          fullpel_deriv[(yint + 1) * w + (xint + 1)]);
  }

  *derivative = interp;
}

static void temporal_gradient(const YV12_BUFFER_CONFIG *frame,
                              const YV12_BUFFER_CONFIG *frame2,
                              const double x_coord, const double y_coord,
                              const int bit_depth, double *derivative,
                              LOCALMV *mv) {
  const int w = 2;
  const int h = 2;
  uint8_t pred1[4];
  uint8_t pred2[4];

  const int y = (int)y_coord;
  const int x = (int)x_coord;
  const double ydec = y_coord - y;
  const double xdec = x_coord - x;
  const int is_intrabc = 0;  // Is intra-copied?
  const int is_high_bitdepth = is_frame_high_bitdepth(frame2);
  const int subsampling_x = 0, subsampling_y = 0;  // for y-buffer
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(MULTITAP_SHARP);
  const int plane = 0;  // y-plane
  const struct buf_2d ref_buf2 = { NULL, frame2->y_buffer, frame2->y_crop_width,
                                   frame2->y_crop_height, frame2->y_stride };
  struct scale_factors scale;
  av1_setup_scale_factors_for_frame(&scale, frame->y_crop_width,
                                    frame->y_crop_height, frame->y_crop_width,
                                    frame->y_crop_height);
  InterPredParams inter_pred_params;
  av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                        subsampling_y, bit_depth, is_high_bitdepth, is_intrabc,
                        &scale, &ref_buf2, interp_filters);
  inter_pred_params.interp_filter_params[0] =
      &av1_interp_filter_params_list[interp_filters.as_filters.x_filter];
  inter_pred_params.interp_filter_params[1] =
      &av1_interp_filter_params_list[interp_filters.as_filters.y_filter];
  inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
  MV newmv = { .row = (int16_t)round((mv->row + xdec) * 8),
               .col = (int16_t)round((mv->col + ydec) * 8) };
  av1_enc_build_one_inter_predictor(pred2, w, &newmv, &inter_pred_params);
  const struct buf_2d ref_buf1 = { NULL, frame->y_buffer, frame->y_crop_width,
                                   frame->y_crop_height, frame->y_stride };
  av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                        subsampling_y, bit_depth, is_high_bitdepth, is_intrabc,
                        &scale, &ref_buf1, interp_filters);
  inter_pred_params.interp_filter_params[0] =
      &av1_interp_filter_params_list[interp_filters.as_filters.x_filter];
  inter_pred_params.interp_filter_params[1] =
      &av1_interp_filter_params_list[interp_filters.as_filters.y_filter];
  inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
  MV zeroMV = { .row = (int16_t)round(xdec * 8),
                .col = (int16_t)round(ydec * 8) };
  av1_enc_build_one_inter_predictor(pred1, w, &zeroMV, &inter_pred_params);

  *derivative = pred2[0] - pred1[0];
}

// Numerical differentiate over window_size x window_size surrounding (x,y)
// location. Alters ix, iy, it to contain numerical partial derivatives
static void gradients_over_window(const YV12_BUFFER_CONFIG *frame,
                                  const YV12_BUFFER_CONFIG *ref_frame,
                                  const double x_coord, const double y_coord,
                                  const int window_size, const int bit_depth,
                                  double *ix, double *iy, double *it,
                                  LOCALMV *mv) {
  const double left = x_coord - window_size / 2.0;
  const double top = y_coord - window_size / 2.0;
  // gradient operators need pixel before and after (start at 1)
  const double x_start = AOMMAX(1, left);
  const double y_start = AOMMAX(1, top);
  const int frame_height = frame->y_crop_height;
  const int frame_width = frame->y_crop_width;
  double deriv_x;
  double deriv_y;
  double deriv_t;

  const double x_end = AOMMIN(x_coord + window_size / 2.0, frame_width - 2);
  const double y_end = AOMMIN(y_coord + window_size / 2.0, frame_height - 2);
  const int xs = (int)AOMMAX(1, x_start - 1);
  const int ys = (int)AOMMAX(1, y_start - 1);
  const int xe = (int)AOMMIN(x_end + 2, frame_width - 2);
  const int ye = (int)AOMMIN(y_end + 2, frame_height - 2);
  // with normalization, gradients may be double values
  double *fullpel_dx = aom_malloc((ye - ys) * (xe - xs) * sizeof(deriv_x));
  double *fullpel_dy = aom_malloc((ye - ys) * (xe - xs) * sizeof(deriv_y));
  if (!fullpel_dx || !fullpel_dy) {
    aom_free(fullpel_dx);
    aom_free(fullpel_dy);
    return;
  }

  // TODO(any): This could be more efficient in the case that x_coord
  // and y_coord are integers.. but it may look more messy.

  // calculate spatial gradients at full pixel locations
  for (int j = ys; j < ye; j++) {
    for (int i = xs; i < xe; i++) {
      spatial_gradient(frame, i, j, 0, &deriv_x);
      spatial_gradient(frame, i, j, 1, &deriv_y);
      int idx = (j - ys) * (xe - xs) + (i - xs);
      fullpel_dx[idx] = deriv_x;
      fullpel_dy[idx] = deriv_y;
    }
  }
  // compute numerical differentiation for every pixel in window
  // (this potentially includes subpixels)
  for (double j = y_start; j < y_end; j++) {
    for (double i = x_start; i < x_end; i++) {
      temporal_gradient(frame, ref_frame, i, j, bit_depth, &deriv_t, mv);
      gradient_interp(fullpel_dx, i - xs, j - ys, xe - xs, ye - ys, &deriv_x);
      gradient_interp(fullpel_dy, i - xs, j - ys, xe - xs, ye - ys, &deriv_y);
      int idx = (int)(j - top) * window_size + (int)(i - left);
      ix[idx] = deriv_x;
      iy[idx] = deriv_y;
      it[idx] = deriv_t;
    }
  }
  // TODO(any): to avoid setting deriv arrays to zero for every iteration,
  // could instead pass these two values back through function call
  // int first_idx = (int)(y_start - top) * window_size + (int)(x_start - left);
  // int width = window_size - ((int)(x_start - left) + (int)(left + window_size
  // - x_end));

  aom_free(fullpel_dx);
  aom_free(fullpel_dy);
}

// To compute eigenvalues of 2x2 matrix: Solve for lambda where
// Determinant(matrix - lambda*identity) == 0
static void eigenvalues_2x2(const double *matrix, double *eig) {
  const double a = 1;
  const double b = -1 * matrix[0] - matrix[3];
  const double c = -1 * matrix[1] * matrix[2] + matrix[0] * matrix[3];
  // quadratic formula
  const double discriminant = b * b - 4 * a * c;
  eig[0] = (-b - sqrt(discriminant)) / (2.0 * a);
  eig[1] = (-b + sqrt(discriminant)) / (2.0 * a);
  // double check that eigenvalues are ordered by magnitude
  if (fabs(eig[0]) > fabs(eig[1])) {
    double tmp = eig[0];
    eig[0] = eig[1];
    eig[1] = tmp;
  }
}

// Shi-Tomasi corner detection criteria
static double corner_score(const YV12_BUFFER_CONFIG *frame_to_filter,
                           const YV12_BUFFER_CONFIG *ref_frame, const int x,
                           const int y, double *i_x, double *i_y, double *i_t,
                           const int n, const int bit_depth) {
  double eig[2];
  LOCALMV mv = { .row = 0, .col = 0 };
  // TODO(any): technically, ref_frame and i_t are not used by corner score
  // so these could be replaced by dummy variables,
  // or change this to spatial gradient function over window only
  gradients_over_window(frame_to_filter, ref_frame, x, y, n, bit_depth, i_x,
                        i_y, i_t, &mv);
  double Mres1[1] = { 0 }, Mres2[1] = { 0 }, Mres3[1] = { 0 };
  multiply_mat(i_x, i_x, Mres1, 1, n * n, 1);
  multiply_mat(i_x, i_y, Mres2, 1, n * n, 1);
  multiply_mat(i_y, i_y, Mres3, 1, n * n, 1);
  double M[4] = { Mres1[0], Mres2[0], Mres2[0], Mres3[0] };
  eigenvalues_2x2(M, eig);
  return fabs(eig[0]);
}

// Finds corners in frame_to_filter
// For less strict requirements (i.e. more corners), decrease threshold
static int detect_corners(const YV12_BUFFER_CONFIG *frame_to_filter,
                          const YV12_BUFFER_CONFIG *ref_frame,
                          const int maxcorners, int *ref_corners,
                          const int bit_depth) {
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  // TODO(any): currently if maxcorners is decreased, then it only means
  // corners will be omited from bottom-right of image. if maxcorners
  // is actually used, then this algorithm would need to re-iterate
  // and choose threshold based on that
  assert(maxcorners == frame_height * frame_width);
  int countcorners = 0;
  const double threshold = 0.1;
  double score;
  const int n = 3;
  double i_x[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  double i_y[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  double i_t[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  const int fromedge = n;
  double max_score = corner_score(frame_to_filter, ref_frame, fromedge,
                                  fromedge, i_x, i_y, i_t, n, bit_depth);
  // rough estimate of max corner score in image
  for (int x = fromedge; x < frame_width - fromedge; x += 1) {
    for (int y = fromedge; y < frame_height - fromedge; y += frame_height / 5) {
      for (int i = 0; i < n * n; i++) {
        i_x[i] = 0;
        i_y[i] = 0;
        i_t[i] = 0;
      }
      score = corner_score(frame_to_filter, ref_frame, x, y, i_x, i_y, i_t, n,
                           bit_depth);
      if (score > max_score) {
        max_score = score;
      }
    }
  }
  // score all the points and choose corners over threshold
  for (int x = fromedge; x < frame_width - fromedge; x += 1) {
    for (int y = fromedge;
         (y < frame_height - fromedge) && countcorners < maxcorners; y += 1) {
      for (int i = 0; i < n * n; i++) {
        i_x[i] = 0;
        i_y[i] = 0;
        i_t[i] = 0;
      }
      score = corner_score(frame_to_filter, ref_frame, x, y, i_x, i_y, i_t, n,
                           bit_depth);
      if (score > threshold * max_score) {
        ref_corners[countcorners * 2] = x;
        ref_corners[countcorners * 2 + 1] = y;
        countcorners++;
      }
    }
  }
  return countcorners;
}

// weights is an nxn matrix. weights is filled with a gaussian function,
// with independent variable: distance from the center point.
static void gaussian(const double sigma, const int n, const int normalize,
                     double *weights) {
  double total_weight = 0;
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      double distance = sqrt(pow(n / 2 - i, 2) + pow(n / 2 - j, 2));
      double weight = exp(-0.5 * pow(distance / sigma, 2));
      weights[j * n + i] = weight;
      total_weight += weight;
    }
  }
  if (normalize == 1) {
    for (int j = 0; j < n; j++) {
      weights[j] = weights[j] / total_weight;
    }
  }
}

static double convolve(const double *filter, const int *img, const int size) {
  double result = 0;
  for (int i = 0; i < size; i++) {
    result += filter[i] * img[i];
  }
  return result;
}

// Applies a Gaussian low-pass smoothing filter to produce
// a corresponding lower resolution image with halved dimensions
static void reduce(uint8_t *img, int height, int width, int stride,
                   uint8_t *reduced_img) {
  const int new_width = width / 2;
  const int window_size = 5;
  const double gaussian_filter[25] = {
    1. / 256, 1.0 / 64, 3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };
  // filter is 5x5 so need prev and forward 2 pixels
  int img_section[25];
  for (int y = 0; y < height - 1; y += 2) {
    for (int x = 0; x < width - 1; x += 2) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = yy;
          int xvalue = xx;
          // copied pixels outside the boundary
          if (yvalue < 0) yvalue = 0;
          if (xvalue < 0) xvalue = 0;
          if (yvalue >= height) yvalue = height - 1;
          if (xvalue >= width) xvalue = width - 1;
          img_section[i++] = img[yvalue * stride + xvalue];
        }
      }
      reduced_img[(y / 2) * new_width + (x / 2)] = (uint8_t)convolve(
          gaussian_filter, img_section, window_size * window_size);
    }
  }
}

static int cmpfunc(const void *a, const void *b) {
  return (*(int *)a - *(int *)b);
}
static void filter_mvs(const MV_FILTER_TYPE mv_filter, const int frame_height,
                       const int frame_width, LOCALMV *localmvs, MV *mvs) {
  const int n = 5;  // window size
  // for smoothing filter
  const double gaussian_filter[25] = {
    1. / 256, 1. / 64,  3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };
  // for median filter
  int mvrows[25];
  int mvcols[25];
  if (mv_filter != MV_FILTER_NONE) {
    for (int y = 0; y < frame_height; y++) {
      for (int x = 0; x < frame_width; x++) {
        int center_idx = y * frame_width + x;
        int i = 0;
        double filtered_row = 0;
        double filtered_col = 0;
        for (int yy = y - n / 2; yy <= y + n / 2; yy++) {
          for (int xx = x - n / 2; xx <= x + n / 2; xx++) {
            int yvalue = yy;
            int xvalue = xx;
            // copied pixels outside the boundary
            if (yvalue < 0) yvalue = 0;
            if (xvalue < 0) xvalue = 0;
            if (yvalue >= frame_height) yvalue = frame_height - 1;
            if (xvalue >= frame_width) xvalue = frame_width - 1;
            int index = yvalue * frame_width + xvalue;
            if (mv_filter == MV_FILTER_SMOOTH) {
              filtered_row += mvs[index].row * gaussian_filter[i];
              filtered_col += mvs[index].col * gaussian_filter[i];
            } else if (mv_filter == MV_FILTER_MEDIAN) {
              mvrows[i] = mvs[index].row;
              mvcols[i] = mvs[index].col;
            }
            i++;
          }
        }

        MV mv = mvs[center_idx];
        if (mv_filter == MV_FILTER_SMOOTH) {
          mv.row = (int16_t)filtered_row;
          mv.col = (int16_t)filtered_col;
        } else if (mv_filter == MV_FILTER_MEDIAN) {
          qsort(mvrows, 25, sizeof(mv.row), cmpfunc);
          qsort(mvcols, 25, sizeof(mv.col), cmpfunc);
          mv.row = mvrows[25 / 2];
          mv.col = mvcols[25 / 2];
        }
        LOCALMV localmv = { .row = ((double)mv.row) / 8,
                            .col = ((double)mv.row) / 8 };
        localmvs[y * frame_width + x] = localmv;
        // if mvs array is immediately updated here, then the result may
        // propagate to other pixels.
      }
    }
    for (int i = 0; i < frame_height * frame_width; i++) {
      MV mv = { .row = (int16_t)round(8 * localmvs[i].row),
                .col = (int16_t)round(8 * localmvs[i].col) };
      mvs[i] = mv;
    }
  }
}

// Computes optical flow at a single pyramid level,
// using Lucas-Kanade algorithm.
// Modifies mvs array.
static void lucas_kanade(const YV12_BUFFER_CONFIG *from_frame,
                         const YV12_BUFFER_CONFIG *to_frame, const int level,
                         const LK_PARAMS *lk_params, const int num_ref_corners,
                         int *ref_corners, const int mv_stride,
                         const int bit_depth, LOCALMV *mvs) {
  assert(lk_params->window_size > 0 && lk_params->window_size % 2 == 0);
  const int n = lk_params->window_size;
  // algorithm is sensitive to window size
  double *i_x = (double *)aom_malloc(n * n * sizeof(*i_x));
  double *i_y = (double *)aom_malloc(n * n * sizeof(*i_y));
  double *i_t = (double *)aom_malloc(n * n * sizeof(*i_t));
  double *weights = (double *)aom_malloc(n * n * sizeof(*weights));
  if (!i_x || !i_y || !i_t || !weights) goto free_lk_buf;

  const int expand_multiplier = (int)pow(2, level);
  double sigma = 0.2 * n;
  // normalizing doesn't really affect anything since it's applied
  // to every component of M and b
  gaussian(sigma, n, 0, weights);
  for (int i = 0; i < num_ref_corners; i++) {
    const double x_coord = 1.0 * ref_corners[i * 2] / expand_multiplier;
    const double y_coord = 1.0 * ref_corners[i * 2 + 1] / expand_multiplier;
    int highres_x = ref_corners[i * 2];
    int highres_y = ref_corners[i * 2 + 1];
    int mv_idx = highres_y * (mv_stride) + highres_x;
    LOCALMV mv_old = mvs[mv_idx];
    mv_old.row = mv_old.row / expand_multiplier;
    mv_old.col = mv_old.col / expand_multiplier;
    // using this instead of memset, since it's not completely
    // clear if zero memset works on double arrays
    for (int j = 0; j < n * n; j++) {
      i_x[j] = 0;
      i_y[j] = 0;
      i_t[j] = 0;
    }
    gradients_over_window(from_frame, to_frame, x_coord, y_coord, n, bit_depth,
                          i_x, i_y, i_t, &mv_old);
    double Mres1[1] = { 0 }, Mres2[1] = { 0 }, Mres3[1] = { 0 };
    double bres1[1] = { 0 }, bres2[1] = { 0 };
    for (int j = 0; j < n * n; j++) {
      Mres1[0] += weights[j] * i_x[j] * i_x[j];
      Mres2[0] += weights[j] * i_x[j] * i_y[j];
      Mres3[0] += weights[j] * i_y[j] * i_y[j];
      bres1[0] += weights[j] * i_x[j] * i_t[j];
      bres2[0] += weights[j] * i_y[j] * i_t[j];
    }
    double M[4] = { Mres1[0], Mres2[0], Mres2[0], Mres3[0] };
    double b[2] = { -1 * bres1[0], -1 * bres2[0] };
    double eig[2] = { 1, 1 };
    eigenvalues_2x2(M, eig);
    double threshold = 0.1;
    if (fabs(eig[0]) > threshold) {
      // if M is not invertible, then displacement
      // will default to zeros
      double u[2] = { 0, 0 };
      linsolve(2, M, 2, b, u);
      int mult = 1;
      if (level != 0)
        mult = expand_multiplier;  // mv doubles when resolution doubles
      LOCALMV mv = { .row = (mult * (u[0] + mv_old.row)),
                     .col = (mult * (u[1] + mv_old.col)) };
      mvs[mv_idx] = mv;
      mvs[mv_idx] = mv;
    }
  }
free_lk_buf:
  aom_free(weights);
  aom_free(i_t);
  aom_free(i_x);
  aom_free(i_y);
}

// Warp the src_frame to warper_frame according to mvs.
// mvs point to src_frame
static void warp_back_frame(YV12_BUFFER_CONFIG *warped_frame,
                            const YV12_BUFFER_CONFIG *src_frame,
                            const LOCALMV *mvs, int mv_stride) {
  int w, h;
  const int fw = src_frame->y_crop_width;
  const int fh = src_frame->y_crop_height;
  const int src_fs = src_frame->y_stride, warped_fs = warped_frame->y_stride;
  const uint8_t *src_buf = src_frame->y_buffer;
  uint8_t *warped_buf = warped_frame->y_buffer;
  double temp;
  for (h = 0; h < fh; h++) {
    for (w = 0; w < fw; w++) {
      double cord_x = (double)w + mvs[h * mv_stride + w].col;
      double cord_y = (double)h + mvs[h * mv_stride + w].row;
      cord_x = fclamp(cord_x, 0, (double)(fw - 1));
      cord_y = fclamp(cord_y, 0, (double)(fh - 1));
      const int floorx = (int)floor(cord_x);
      const int floory = (int)floor(cord_y);
      const double fracx = cord_x - (double)floorx;
      const double fracy = cord_y - (double)floory;

      temp = 0;
      for (int hh = 0; hh < 2; hh++) {
        const double weighth = hh ? (fracy) : (1 - fracy);
        for (int ww = 0; ww < 2; ww++) {
          const double weightw = ww ? (fracx) : (1 - fracx);
          int y = floory + hh;
          int x = floorx + ww;
          y = clamp(y, 0, fh - 1);
          x = clamp(x, 0, fw - 1);
          temp += (double)src_buf[y * src_fs + x] * weightw * weighth;
        }
      }
      warped_buf[h * warped_fs + w] = (uint8_t)round(temp);
    }
  }
}

// Same as warp_back_frame, but using a better interpolation filter.
static void warp_back_frame_intp(YV12_BUFFER_CONFIG *warped_frame,
                                 const YV12_BUFFER_CONFIG *src_frame,
                                 const LOCALMV *mvs, int mv_stride) {
  int w, h;
  const int fw = src_frame->y_crop_width;
  const int fh = src_frame->y_crop_height;
  const int warped_fs = warped_frame->y_stride;
  uint8_t *warped_buf = warped_frame->y_buffer;
  const int blk = 2;
  uint8_t temp_blk[4];

  const int is_intrabc = 0;  // Is intra-copied?
  const int is_high_bitdepth = is_frame_high_bitdepth(src_frame);
  const int subsampling_x = 0, subsampling_y = 0;  // for y-buffer
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(MULTITAP_SHARP2);
  const int plane = 0;  // y-plane
  const struct buf_2d ref_buf2 = { NULL, src_frame->y_buffer,
                                   src_frame->y_crop_width,
                                   src_frame->y_crop_height,
                                   src_frame->y_stride };
  const int bit_depth = src_frame->bit_depth;
  struct scale_factors scale;
  av1_setup_scale_factors_for_frame(
      &scale, src_frame->y_crop_width, src_frame->y_crop_height,
      src_frame->y_crop_width, src_frame->y_crop_height);

  for (h = 0; h < fh; h++) {
    for (w = 0; w < fw; w++) {
      InterPredParams inter_pred_params;
      av1_init_inter_params(&inter_pred_params, blk, blk, h, w, subsampling_x,
                            subsampling_y, bit_depth, is_high_bitdepth,
                            is_intrabc, &scale, &ref_buf2, interp_filters);
      inter_pred_params.interp_filter_params[0] =
          &av1_interp_filter_params_list[interp_filters.as_filters.x_filter];
      inter_pred_params.interp_filter_params[1] =
          &av1_interp_filter_params_list[interp_filters.as_filters.y_filter];
      inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
      MV newmv = { .row = (int16_t)round((mvs[h * mv_stride + w].row) * 8),
                   .col = (int16_t)round((mvs[h * mv_stride + w].col) * 8) };
      av1_enc_build_one_inter_predictor(temp_blk, blk, &newmv,
                                        &inter_pred_params);
      warped_buf[h * warped_fs + w] = temp_blk[0];
    }
  }
}

#define DERIVATIVE_FILTER_LENGTH 7
double filter[DERIVATIVE_FILTER_LENGTH] = { -1.0 / 60, 9.0 / 60,  -45.0 / 60, 0,
                                            45.0 / 60, -9.0 / 60, 1.0 / 60 };

// Get gradient of the whole frame
static void get_frame_gradients(const YV12_BUFFER_CONFIG *from_frame,
                                const YV12_BUFFER_CONFIG *to_frame, double *ix,
                                double *iy, double *it, int grad_stride) {
  int w, h, k, idx;
  const int fw = from_frame->y_crop_width;
  const int fh = from_frame->y_crop_height;
  const int from_fs = from_frame->y_stride, to_fs = to_frame->y_stride;
  const uint8_t *from_buf = from_frame->y_buffer;
  const uint8_t *to_buf = to_frame->y_buffer;

  const int lh = DERIVATIVE_FILTER_LENGTH;
  const int hleft = (lh - 1) / 2;

  for (h = 0; h < fh; h++) {
    for (w = 0; w < fw; w++) {
      // x
      ix[h * grad_stride + w] = 0;
      for (k = 0; k < lh; k++) {
        // if we want to make this block dependent, need to extend the
        // boundaries using other initializations.
        idx = w + k - hleft;
        idx = clamp(idx, 0, fw - 1);
        ix[h * grad_stride + w] += filter[k] * 0.5 *
                                   ((double)from_buf[h * from_fs + idx] +
                                    (double)to_buf[h * to_fs + idx]);
      }
      // y
      iy[h * grad_stride + w] = 0;
      for (k = 0; k < lh; k++) {
        // if we want to make this block dependent, need to extend the
        // boundaries using other initializations.
        idx = h + k - hleft;
        idx = clamp(idx, 0, fh - 1);
        iy[h * grad_stride + w] += filter[k] * 0.5 *
                                   ((double)from_buf[idx * from_fs + w] +
                                    (double)to_buf[idx * to_fs + w]);
      }
      // t
      it[h * grad_stride + w] =
          (double)to_buf[h * to_fs + w] - (double)from_buf[h * from_fs + w];
    }
  }
}

// Solve for linear equations given by the H-S method
static void solve_horn_schunck(const double *ix, const double *iy,
                               const double *it, int grad_stride, int width,
                               int height, const LOCALMV *init_mvs,
                               int init_mv_stride, LOCALMV *mvs,
                               int mv_stride) {
  // TODO(bohanli): May just need to allocate the buffers once per optical flow
  // calculation
  int *row_pos = aom_calloc(width * height * 28, sizeof(*row_pos));
  int *col_pos = aom_calloc(width * height * 28, sizeof(*col_pos));
  double *values = aom_calloc(width * height * 28, sizeof(*values));
  double *mv_vec = aom_calloc(width * height * 2, sizeof(*mv_vec));
  double *mv_init_vec = aom_calloc(width * height * 2, sizeof(*mv_init_vec));
  double *temp_b = aom_calloc(width * height * 2, sizeof(*temp_b));
  double *b = aom_calloc(width * height * 2, sizeof(*b));
  if (!row_pos || !col_pos || !values || !mv_vec || !mv_init_vec || !temp_b ||
      !b) {
    goto free_hs_solver_buf;
  }

  // the location idx for neighboring pixels, k < 4 are the 4 direct neighbors
  const int check_locs_y[12] = { 0, 0, -1, 1, -1, -1, 1, 1, 0, 0, -2, 2 };
  const int check_locs_x[12] = { -1, 1, 0, 0, -1, 1, -1, 1, -2, 2, 0, 0 };

  int h, w, checkh, checkw, k, ret;
  const int offset = height * width;
  SPARSE_MTX A;
  int c = 0;
  const double lambda = 100;

  for (w = 0; w < width; w++) {
    for (h = 0; h < height; h++) {
      mv_init_vec[w * height + h] = init_mvs[h * init_mv_stride + w].col;
      mv_init_vec[w * height + h + offset] =
          init_mvs[h * init_mv_stride + w].row;
    }
  }

  // get matrix A
  for (w = 0; w < width; w++) {
    for (h = 0; h < height; h++) {
      int center_num_direct = 4;
      const int center_idx = w * height + h;
      if (w == 0 || w == width - 1) center_num_direct--;
      if (h == 0 || h == height - 1) center_num_direct--;
      // diagonal entry for this row from the center pixel
      double cor_w = center_num_direct * center_num_direct + center_num_direct;
      row_pos[c] = center_idx;
      col_pos[c] = center_idx;
      values[c] = lambda * cor_w;
      c++;
      row_pos[c] = center_idx + offset;
      col_pos[c] = center_idx + offset;
      values[c] = lambda * cor_w;
      c++;
      // other entries from direct neighbors
      for (k = 0; k < 4; k++) {
        checkh = h + check_locs_y[k];
        checkw = w + check_locs_x[k];
        if (checkh < 0 || checkh >= height || checkw < 0 || checkw >= width) {
          continue;
        }
        int this_idx = checkw * height + checkh;
        int this_num_direct = 4;
        if (checkw == 0 || checkw == width - 1) this_num_direct--;
        if (checkh == 0 || checkh == height - 1) this_num_direct--;
        cor_w = -center_num_direct - this_num_direct;
        row_pos[c] = center_idx;
        col_pos[c] = this_idx;
        values[c] = lambda * cor_w;
        c++;
        row_pos[c] = center_idx + offset;
        col_pos[c] = this_idx + offset;
        values[c] = lambda * cor_w;
        c++;
      }
      // entries from neighbors on the diagonal corners
      for (k = 4; k < 8; k++) {
        checkh = h + check_locs_y[k];
        checkw = w + check_locs_x[k];
        if (checkh < 0 || checkh >= height || checkw < 0 || checkw >= width) {
          continue;
        }
        int this_idx = checkw * height + checkh;
        cor_w = 2;
        row_pos[c] = center_idx;
        col_pos[c] = this_idx;
        values[c] = lambda * cor_w;
        c++;
        row_pos[c] = center_idx + offset;
        col_pos[c] = this_idx + offset;
        values[c] = lambda * cor_w;
        c++;
      }
      // entries from neighbors with dist of 2
      for (k = 8; k < 12; k++) {
        checkh = h + check_locs_y[k];
        checkw = w + check_locs_x[k];
        if (checkh < 0 || checkh >= height || checkw < 0 || checkw >= width) {
          continue;
        }
        int this_idx = checkw * height + checkh;
        cor_w = 1;
        row_pos[c] = center_idx;
        col_pos[c] = this_idx;
        values[c] = lambda * cor_w;
        c++;
        row_pos[c] = center_idx + offset;
        col_pos[c] = this_idx + offset;
        values[c] = lambda * cor_w;
        c++;
      }
    }
  }
  ret = av1_init_sparse_mtx(row_pos, col_pos, values, c, 2 * width * height,
                            2 * width * height, &A);
  if (ret < 0) goto free_hs_solver_buf;
  // subtract init mv part from b
  av1_mtx_vect_multi_left(&A, mv_init_vec, temp_b, 2 * width * height);
  for (int i = 0; i < 2 * width * height; i++) {
    b[i] = -temp_b[i];
  }
  av1_free_sparse_mtx_elems(&A);

  // add cross terms to A and modify b with ExEt / EyEt
  for (w = 0; w < width; w++) {
    for (h = 0; h < height; h++) {
      int curidx = w * height + h;
      // modify b
      b[curidx] += -ix[h * grad_stride + w] * it[h * grad_stride + w];
      b[curidx + offset] += -iy[h * grad_stride + w] * it[h * grad_stride + w];
      // add cross terms to A
      row_pos[c] = curidx;
      col_pos[c] = curidx + offset;
      values[c] = ix[h * grad_stride + w] * iy[h * grad_stride + w];
      c++;
      row_pos[c] = curidx + offset;
      col_pos[c] = curidx;
      values[c] = ix[h * grad_stride + w] * iy[h * grad_stride + w];
      c++;
    }
  }
  // Add diagonal terms to A
  for (int i = 0; i < c; i++) {
    if (row_pos[i] == col_pos[i]) {
      if (row_pos[i] < offset) {
        w = row_pos[i] / height;
        h = row_pos[i] % height;
        values[i] += pow(ix[h * grad_stride + w], 2);
      } else {
        w = (row_pos[i] - offset) / height;
        h = (row_pos[i] - offset) % height;
        values[i] += pow(iy[h * grad_stride + w], 2);
      }
    }
  }

  ret = av1_init_sparse_mtx(row_pos, col_pos, values, c, 2 * width * height,
                            2 * width * height, &A);
  if (ret < 0) goto free_hs_solver_buf;

  // solve for the mvs
  ret = av1_conjugate_gradient_sparse(&A, b, 2 * width * height, mv_vec);
  if (ret < 0) goto free_hs_solver_buf;

  // copy mvs
  for (w = 0; w < width; w++) {
    for (h = 0; h < height; h++) {
      mvs[h * mv_stride + w].col = mv_vec[w * height + h];
      mvs[h * mv_stride + w].row = mv_vec[w * height + h + offset];
    }
  }
free_hs_solver_buf:
  aom_free(row_pos);
  aom_free(col_pos);
  aom_free(values);
  aom_free(mv_vec);
  aom_free(mv_init_vec);
  aom_free(b);
  aom_free(temp_b);
  av1_free_sparse_mtx_elems(&A);
}

// Calculate optical flow from from_frame to to_frame using the H-S method.
static void horn_schunck(const YV12_BUFFER_CONFIG *from_frame,
                         const YV12_BUFFER_CONFIG *to_frame, const int level,
                         const int mv_stride, const int mv_height,
                         const int mv_width, const OPFL_PARAMS *opfl_params,
                         LOCALMV *mvs) {
  // mvs are always on level 0, here we define two new mv arrays that is of size
  // of this level.
  const int fw = from_frame->y_crop_width;
  const int fh = from_frame->y_crop_height;
  const int factor = (int)pow(2, level);
  int w, h, k, init_mv_stride;
  LOCALMV *init_mvs = NULL, *refine_mvs = NULL;
  double *ix = NULL, *iy = NULL, *it = NULL;
  YV12_BUFFER_CONFIG temp_frame;
  temp_frame.y_buffer = NULL;
  if (level == 0) {
    init_mvs = mvs;
    init_mv_stride = mv_stride;
  } else {
    init_mvs = aom_calloc(fw * fh, sizeof(*mvs));
    if (!init_mvs) goto free_hs_buf;
    init_mv_stride = fw;
    for (h = 0; h < fh; h++) {
      for (w = 0; w < fw; w++) {
        init_mvs[h * init_mv_stride + w].row =
            mvs[h * factor * mv_stride + w * factor].row / (double)factor;
        init_mvs[h * init_mv_stride + w].col =
            mvs[h * factor * mv_stride + w * factor].col / (double)factor;
      }
    }
  }
  refine_mvs = aom_calloc(fw * fh, sizeof(*mvs));
  if (!refine_mvs) goto free_hs_buf;
  // temp frame for warping
  temp_frame.y_buffer =
      (uint8_t *)aom_calloc(fh * fw, sizeof(*temp_frame.y_buffer));
  if (!temp_frame.y_buffer) goto free_hs_buf;
  temp_frame.y_crop_height = fh;
  temp_frame.y_crop_width = fw;
  temp_frame.y_stride = fw;
  // gradient buffers
  ix = aom_calloc(fw * fh, sizeof(*ix));
  iy = aom_calloc(fw * fh, sizeof(*iy));
  it = aom_calloc(fw * fh, sizeof(*it));
  if (!ix || !iy || !it) goto free_hs_buf;
  // For each warping step
  for (k = 0; k < opfl_params->warping_steps; k++) {
    // warp from_frame with init_mv
    if (level == 0) {
      warp_back_frame_intp(&temp_frame, to_frame, init_mvs, init_mv_stride);
    } else {
      warp_back_frame(&temp_frame, to_frame, init_mvs, init_mv_stride);
    }
    // calculate frame gradients
    get_frame_gradients(from_frame, &temp_frame, ix, iy, it, fw);
    // form linear equations and solve mvs
    solve_horn_schunck(ix, iy, it, fw, fw, fh, init_mvs, init_mv_stride,
                       refine_mvs, fw);
    // update init_mvs
    for (h = 0; h < fh; h++) {
      for (w = 0; w < fw; w++) {
        init_mvs[h * init_mv_stride + w].col += refine_mvs[h * fw + w].col;
        init_mvs[h * init_mv_stride + w].row += refine_mvs[h * fw + w].row;
      }
    }
  }
  // copy back the mvs if needed
  if (level != 0) {
    for (h = 0; h < mv_height; h++) {
      for (w = 0; w < mv_width; w++) {
        mvs[h * mv_stride + w].row =
            init_mvs[h / factor * init_mv_stride + w / factor].row *
            (double)factor;
        mvs[h * mv_stride + w].col =
            init_mvs[h / factor * init_mv_stride + w / factor].col *
            (double)factor;
      }
    }
  }
free_hs_buf:
  if (level != 0) aom_free(init_mvs);
  aom_free(refine_mvs);
  aom_free(temp_frame.y_buffer);
  aom_free(ix);
  aom_free(iy);
  aom_free(it);
}

// Apply optical flow iteratively at each pyramid level
static void pyramid_optical_flow(const YV12_BUFFER_CONFIG *from_frame,
                                 const YV12_BUFFER_CONFIG *to_frame,
                                 const int bit_depth,
                                 const OPFL_PARAMS *opfl_params,
                                 const OPTFLOW_METHOD method, LOCALMV *mvs) {
  assert(opfl_params->pyramid_levels > 0 &&
         opfl_params->pyramid_levels <= MAX_PYRAMID_LEVELS);
  int levels = opfl_params->pyramid_levels;
  const int frame_height = from_frame->y_crop_height;
  const int frame_width = from_frame->y_crop_width;
  if ((frame_height / pow(2.0, levels - 1) < 50 ||
       frame_height / pow(2.0, levels - 1) < 50) &&
      levels > 1)
    levels = levels - 1;
  uint8_t *images1[MAX_PYRAMID_LEVELS] = { NULL };
  uint8_t *images2[MAX_PYRAMID_LEVELS] = { NULL };
  int *ref_corners = NULL;

  images1[0] = from_frame->y_buffer;
  images2[0] = to_frame->y_buffer;
  YV12_BUFFER_CONFIG *buffers1 = aom_malloc(levels * sizeof(*buffers1));
  YV12_BUFFER_CONFIG *buffers2 = aom_malloc(levels * sizeof(*buffers2));
  if (!buffers1 || !buffers2) goto free_pyramid_buf;
  buffers1[0] = *from_frame;
  buffers2[0] = *to_frame;
  int fw = frame_width;
  int fh = frame_height;
  for (int i = 1; i < levels; i++) {
    // TODO(bohanli): may need to extend buffers for better interpolation SIMD
    images1[i] = (uint8_t *)aom_calloc(fh / 2 * fw / 2, sizeof(*images1[i]));
    images2[i] = (uint8_t *)aom_calloc(fh / 2 * fw / 2, sizeof(*images2[i]));
    if (!images1[i] || !images2[i]) goto free_pyramid_buf;
    int stride;
    if (i == 1)
      stride = from_frame->y_stride;
    else
      stride = fw;
    reduce(images1[i - 1], fh, fw, stride, images1[i]);
    reduce(images2[i - 1], fh, fw, stride, images2[i]);
    fh /= 2;
    fw /= 2;
    YV12_BUFFER_CONFIG a = { .y_buffer = images1[i],
                             .y_crop_width = fw,
                             .y_crop_height = fh,
                             .y_stride = fw };
    YV12_BUFFER_CONFIG b = { .y_buffer = images2[i],
                             .y_crop_width = fw,
                             .y_crop_height = fh,
                             .y_stride = fw };
    buffers1[i] = a;
    buffers2[i] = b;
  }
  // Compute corners for specific frame
  int num_ref_corners = 0;
  if (is_sparse(opfl_params)) {
    int maxcorners = from_frame->y_crop_width * from_frame->y_crop_height;
    ref_corners = aom_malloc(maxcorners * 2 * sizeof(*ref_corners));
    if (!ref_corners) goto free_pyramid_buf;
    num_ref_corners = detect_corners(from_frame, to_frame, maxcorners,
                                     ref_corners, bit_depth);
  }
  const int stop_level = 0;
  for (int i = levels - 1; i >= stop_level; i--) {
    if (method == LUCAS_KANADE) {
      assert(is_sparse(opfl_params));
      lucas_kanade(&buffers1[i], &buffers2[i], i, opfl_params->lk_params,
                   num_ref_corners, ref_corners, buffers1[0].y_crop_width,
                   bit_depth, mvs);
    } else if (method == HORN_SCHUNCK) {
      assert(!is_sparse(opfl_params));
      horn_schunck(&buffers1[i], &buffers2[i], i, buffers1[0].y_crop_width,
                   buffers1[0].y_crop_height, buffers1[0].y_crop_width,
                   opfl_params, mvs);
    }
  }
free_pyramid_buf:
  for (int i = 1; i < levels; i++) {
    aom_free(images1[i]);
    aom_free(images2[i]);
  }
  aom_free(ref_corners);
  aom_free(buffers1);
  aom_free(buffers2);
}
// Computes optical flow by applying algorithm at
// multiple pyramid levels of images (lower-resolution, smoothed images)
// This accounts for larger motions.
// Inputs:
//   from_frame Frame buffer.
//   to_frame: Frame buffer. MVs point from_frame -> to_frame.
//   from_frame_idx: Index of from_frame.
//   to_frame_idx: Index of to_frame. Return all zero MVs when idx are equal.
//   bit_depth:
//   opfl_params: contains algorithm-specific parameters.
//   mv_filter: MV_FILTER_NONE, MV_FILTER_SMOOTH, or MV_FILTER_MEDIAN.
//   method: LUCAS_KANADE, HORN_SCHUNCK
//   mvs: pointer to MVs. Contains initialization, and modified
//   based on optical flow. Must have
//   dimensions = from_frame->y_crop_width * from_frame->y_crop_height
void av1_optical_flow(const YV12_BUFFER_CONFIG *from_frame,
                      const YV12_BUFFER_CONFIG *to_frame,
                      const int from_frame_idx, const int to_frame_idx,
                      const int bit_depth, const OPFL_PARAMS *opfl_params,
                      const MV_FILTER_TYPE mv_filter,
                      const OPTFLOW_METHOD method, MV *mvs) {
  const int frame_height = from_frame->y_crop_height;
  const int frame_width = from_frame->y_crop_width;
  // TODO(any): deal with the case where frames are not of the same dimensions
  assert(frame_height == to_frame->y_crop_height &&
         frame_width == to_frame->y_crop_width);
  if (from_frame_idx == to_frame_idx) {
    // immediately return all zero mvs when frame indices are equal
    for (int yy = 0; yy < frame_height; yy++) {
      for (int xx = 0; xx < frame_width; xx++) {
        MV mv = { .row = 0, .col = 0 };
        mvs[yy * frame_width + xx] = mv;
      }
    }
    return;
  }

  // Initialize double mvs based on input parameter mvs array
  LOCALMV *localmvs =
      aom_malloc(frame_height * frame_width * sizeof(*localmvs));
  if (!localmvs) return;

  filter_mvs(MV_FILTER_SMOOTH, frame_height, frame_width, localmvs, mvs);

  for (int i = 0; i < frame_width * frame_height; i++) {
    MV mv = mvs[i];
    LOCALMV localmv = { .row = ((double)mv.row) / 8,
                        .col = ((double)mv.col) / 8 };
    localmvs[i] = localmv;
  }
  // Apply optical flow algorithm
  pyramid_optical_flow(from_frame, to_frame, bit_depth, opfl_params, method,
                       localmvs);

  // Update original mvs array
  for (int j = 0; j < frame_height; j++) {
    for (int i = 0; i < frame_width; i++) {
      int idx = j * frame_width + i;
      if (j + localmvs[idx].row < 0 || j + localmvs[idx].row >= frame_height ||
          i + localmvs[idx].col < 0 || i + localmvs[idx].col >= frame_width) {
        continue;
      }
      MV mv = { .row = (int16_t)round(8 * localmvs[idx].row),
                .col = (int16_t)round(8 * localmvs[idx].col) };
      mvs[idx] = mv;
    }
  }

  filter_mvs(mv_filter, frame_height, frame_width, localmvs, mvs);

  aom_free(localmvs);
}
#endif
