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

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/flow_estimation/corner_detect.h"
#include "aom_dsp/flow_estimation/corner_match.h"
#include "aom_dsp/flow_estimation/disflow.h"
#include "aom_dsp/flow_estimation/flow_estimation.h"
#include "aom_dsp/flow_estimation/ransac.h"
#include "aom_dsp/pyramid.h"
#include "aom_scale/yv12config.h"

#define THRESHOLD_NCC 0.75

/* Compute mean and standard deviation of pixels in a window of size
   MATCH_SZ by MATCH_SZ centered at (x, y).
   Store results into *mean and *one_over_stddev

   Note: The output of this function is scaled by MATCH_SZ, as in
   *mean = MATCH_SZ * <true mean> and
   *one_over_stddev = 1 / (MATCH_SZ * <true stddev>)

   Combined with the fact that we return 1/stddev rather than the standard
   deviation itself, this allows us to completely avoid divisions in
   aom_compute_correlation, which is much hotter than this function is.

   Returns true if this feature point is usable, false otherwise.
*/
bool aom_compute_mean_stddev_c(const unsigned char *frame, int stride, int x,
                               int y, double *mean, double *one_over_stddev) {
  int sum = 0;
  int sumsq = 0;
  for (int i = 0; i < MATCH_SZ; ++i) {
    for (int j = 0; j < MATCH_SZ; ++j) {
      sum += frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)];
      sumsq += frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)] *
               frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)];
    }
  }
  *mean = (double)sum / MATCH_SZ;
  const double variance = sumsq - (*mean) * (*mean);
  if (variance < MIN_FEATURE_VARIANCE) {
    *one_over_stddev = 0.0;
    return false;
  }
  *one_over_stddev = 1.0 / sqrt(variance);
  return true;
}

/* Compute corr(frame1, frame2) over a window of size MATCH_SZ by MATCH_SZ.
   To save on computation, the mean and (1 divided by the) standard deviation
   of the window in each frame are precomputed and passed into this function
   as arguments.
*/
double aom_compute_correlation_c(const unsigned char *frame1, int stride1,
                                 int x1, int y1, double mean1,
                                 double one_over_stddev1,
                                 const unsigned char *frame2, int stride2,
                                 int x2, int y2, double mean2,
                                 double one_over_stddev2) {
  int v1, v2;
  int cross = 0;
  for (int i = 0; i < MATCH_SZ; ++i) {
    for (int j = 0; j < MATCH_SZ; ++j) {
      v1 = frame1[(i + y1 - MATCH_SZ_BY2) * stride1 + (j + x1 - MATCH_SZ_BY2)];
      v2 = frame2[(i + y2 - MATCH_SZ_BY2) * stride2 + (j + x2 - MATCH_SZ_BY2)];
      cross += v1 * v2;
    }
  }

  // Note: In theory, the calculations here "should" be
  //   covariance = cross / N^2 - mean1 * mean2
  //   correlation = covariance / (stddev1 * stddev2).
  //
  // However, because of the scaling in aom_compute_mean_stddev, the
  // lines below actually calculate
  //   covariance * N^2 = cross - (mean1 * N) * (mean2 * N)
  //   correlation = (covariance * N^2) / ((stddev1 * N) * (stddev2 * N))
  //
  // ie. we have removed the need for a division, and still end up with the
  // correct unscaled correlation (ie, in the range [-1, +1])
  double covariance = cross - mean1 * mean2;
  double correlation = covariance * (one_over_stddev1 * one_over_stddev2);
  return correlation;
}

static int is_eligible_point(int pointx, int pointy, int width, int height) {
  return (pointx >= MATCH_SZ_BY2 && pointy >= MATCH_SZ_BY2 &&
          pointx + MATCH_SZ_BY2 < width && pointy + MATCH_SZ_BY2 < height);
}

static int is_eligible_distance(int point1x, int point1y, int point2x,
                                int point2y, int width, int height) {
  const int thresh = (width < height ? height : width) >> 4;
  return ((point1x - point2x) * (point1x - point2x) +
          (point1y - point2y) * (point1y - point2y)) <= thresh * thresh;
}

typedef struct {
  int x;
  int y;
  double mean;
  double one_over_stddev;
  int best_match_idx;
  double best_match_corr;
} PointInfo;

static int determine_correspondence(const unsigned char *src,
                                    const int *src_corners, int num_src_corners,
                                    const unsigned char *ref,
                                    const int *ref_corners, int num_ref_corners,
                                    int width, int height, int src_stride,
                                    int ref_stride,
                                    Correspondence *correspondences) {
  PointInfo *src_point_info = NULL;
  PointInfo *ref_point_info = NULL;
  int num_correspondences = 0;

  src_point_info =
      (PointInfo *)aom_calloc(num_src_corners, sizeof(*src_point_info));
  if (!src_point_info) {
    goto finished;
  }

  ref_point_info =
      (PointInfo *)aom_calloc(num_ref_corners, sizeof(*ref_point_info));
  if (!ref_point_info) {
    goto finished;
  }

  // First pass (linear):
  // Filter corner lists and compute per-patch means and standard deviations,
  // for the src and ref frames independently
  int src_point_count = 0;
  for (int i = 0; i < num_src_corners; i++) {
    int src_x = src_corners[2 * i];
    int src_y = src_corners[2 * i + 1];
    if (!is_eligible_point(src_x, src_y, width, height)) continue;

    PointInfo *point = &src_point_info[src_point_count];
    point->x = src_x;
    point->y = src_y;
    point->best_match_corr = THRESHOLD_NCC;
    if (!aom_compute_mean_stddev(src, src_stride, src_x, src_y, &point->mean,
                                 &point->one_over_stddev))
      continue;
    src_point_count++;
  }
  if (src_point_count == 0) {
    goto finished;
  }

  int ref_point_count = 0;
  for (int j = 0; j < num_ref_corners; j++) {
    int ref_x = ref_corners[2 * j];
    int ref_y = ref_corners[2 * j + 1];
    if (!is_eligible_point(ref_x, ref_y, width, height)) continue;

    PointInfo *point = &ref_point_info[ref_point_count];
    point->x = ref_x;
    point->y = ref_y;
    point->best_match_corr = THRESHOLD_NCC;
    if (!aom_compute_mean_stddev(ref, ref_stride, ref_x, ref_y, &point->mean,
                                 &point->one_over_stddev))
      continue;
    ref_point_count++;
  }
  if (ref_point_count == 0) {
    goto finished;
  }

  // Second pass (quadratic):
  // For each pair of points, compute correlation, and use this to determine
  // the best match of each corner, in both directions
  for (int i = 0; i < src_point_count; ++i) {
    PointInfo *src_point = &src_point_info[i];
    for (int j = 0; j < ref_point_count; ++j) {
      PointInfo *ref_point = &ref_point_info[j];
      if (!is_eligible_distance(src_point->x, src_point->y, ref_point->x,
                                ref_point->y, width, height))
        continue;

      double corr = aom_compute_correlation(
          src, src_stride, src_point->x, src_point->y, src_point->mean,
          src_point->one_over_stddev, ref, ref_stride, ref_point->x,
          ref_point->y, ref_point->mean, ref_point->one_over_stddev);

      if (corr > src_point->best_match_corr) {
        src_point->best_match_idx = j;
        src_point->best_match_corr = corr;
      }
      if (corr > ref_point->best_match_corr) {
        ref_point->best_match_idx = i;
        ref_point->best_match_corr = corr;
      }
    }
  }

  // Third pass (linear):
  // Scan through source corners, generating a correspondence for each corner
  // iff ref_best_match[src_best_match[i]] == i
  // Then refine the generated correspondences using optical flow
  for (int i = 0; i < src_point_count; i++) {
    PointInfo *point = &src_point_info[i];

    // Skip corners which were not matched, or which didn't find
    // a good enough match
    if (point->best_match_corr < THRESHOLD_NCC) continue;

    PointInfo *match_point = &ref_point_info[point->best_match_idx];
    if (match_point->best_match_idx == i) {
      // Refine match using optical flow and store
      const int sx = point->x;
      const int sy = point->y;
      const int rx = match_point->x;
      const int ry = match_point->y;
      double u = (double)(rx - sx);
      double v = (double)(ry - sy);

      const int patch_tl_x = sx - DISFLOW_PATCH_CENTER;
      const int patch_tl_y = sy - DISFLOW_PATCH_CENTER;

      aom_compute_flow_at_point(src, ref, patch_tl_x, patch_tl_y, width, height,
                                src_stride, &u, &v);

      Correspondence *correspondence = &correspondences[num_correspondences];
      correspondence->x = (double)sx;
      correspondence->y = (double)sy;
      correspondence->rx = (double)sx + u;
      correspondence->ry = (double)sy + v;
      num_correspondences++;
    }
  }

finished:
  aom_free(src_point_info);
  aom_free(ref_point_info);
  return num_correspondences;
}

bool av1_compute_global_motion_feature_match(
    TransformationType type, YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *ref,
    int bit_depth, int downsample_level, MotionModel *motion_models,
    int num_motion_models, bool *mem_alloc_failed) {
  int num_correspondences;
  Correspondence *correspondences;
  ImagePyramid *src_pyramid = src->y_pyramid;
  CornerList *src_corners = src->corners;
  ImagePyramid *ref_pyramid = ref->y_pyramid;
  CornerList *ref_corners = ref->corners;

  // Precompute information we will need about each frame
  if (aom_compute_pyramid(src, bit_depth, 1, src_pyramid) < 0) {
    *mem_alloc_failed = true;
    return false;
  }
  if (!av1_compute_corner_list(src, bit_depth, downsample_level, src_corners)) {
    *mem_alloc_failed = true;
    return false;
  }
  if (aom_compute_pyramid(ref, bit_depth, 1, ref_pyramid) < 0) {
    *mem_alloc_failed = true;
    return false;
  }
  if (!av1_compute_corner_list(src, bit_depth, downsample_level, ref_corners)) {
    *mem_alloc_failed = true;
    return false;
  }

  const uint8_t *src_buffer = src_pyramid->layers[0].buffer;
  const int src_width = src_pyramid->layers[0].width;
  const int src_height = src_pyramid->layers[0].height;
  const int src_stride = src_pyramid->layers[0].stride;

  const uint8_t *ref_buffer = ref_pyramid->layers[0].buffer;
  assert(ref_pyramid->layers[0].width == src_width);
  assert(ref_pyramid->layers[0].height == src_height);
  const int ref_stride = ref_pyramid->layers[0].stride;

  // find correspondences between the two images
  correspondences = (Correspondence *)aom_malloc(src_corners->num_corners *
                                                 sizeof(*correspondences));
  if (!correspondences) {
    *mem_alloc_failed = true;
    return false;
  }
  num_correspondences = determine_correspondence(
      src_buffer, src_corners->corners, src_corners->num_corners, ref_buffer,
      ref_corners->corners, ref_corners->num_corners, src_width, src_height,
      src_stride, ref_stride, correspondences);

  bool result = ransac(correspondences, num_correspondences, type,
                       motion_models, num_motion_models, mem_alloc_failed);

  aom_free(correspondences);
  return result;
}
