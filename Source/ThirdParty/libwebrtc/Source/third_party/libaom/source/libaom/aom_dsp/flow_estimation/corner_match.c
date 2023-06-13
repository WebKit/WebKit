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
#include "aom_dsp/flow_estimation/flow_estimation.h"
#include "aom_dsp/flow_estimation/ransac.h"
#include "aom_dsp/pyramid.h"
#include "aom_scale/yv12config.h"

#define SEARCH_SZ 9
#define SEARCH_SZ_BY2 ((SEARCH_SZ - 1) / 2)

#define THRESHOLD_NCC 0.75

/* Compute var(frame) * MATCH_SZ_SQ over a MATCH_SZ by MATCH_SZ window of frame,
   centered at (x, y).
*/
static double compute_variance(const unsigned char *frame, int stride, int x,
                               int y) {
  int sum = 0;
  int sumsq = 0;
  int var;
  int i, j;
  for (i = 0; i < MATCH_SZ; ++i)
    for (j = 0; j < MATCH_SZ; ++j) {
      sum += frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)];
      sumsq += frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)] *
               frame[(i + y - MATCH_SZ_BY2) * stride + (j + x - MATCH_SZ_BY2)];
    }
  var = sumsq * MATCH_SZ_SQ - sum * sum;
  return (double)var;
}

/* Compute corr(frame1, frame2) * MATCH_SZ * stddev(frame1), where the
   correlation/standard deviation are taken over MATCH_SZ by MATCH_SZ windows
   of each image, centered at (x1, y1) and (x2, y2) respectively.
*/
double av1_compute_cross_correlation_c(const unsigned char *frame1, int stride1,
                                       int x1, int y1,
                                       const unsigned char *frame2, int stride2,
                                       int x2, int y2) {
  int v1, v2;
  int sum1 = 0;
  int sum2 = 0;
  int sumsq2 = 0;
  int cross = 0;
  int var2, cov;
  int i, j;
  for (i = 0; i < MATCH_SZ; ++i)
    for (j = 0; j < MATCH_SZ; ++j) {
      v1 = frame1[(i + y1 - MATCH_SZ_BY2) * stride1 + (j + x1 - MATCH_SZ_BY2)];
      v2 = frame2[(i + y2 - MATCH_SZ_BY2) * stride2 + (j + x2 - MATCH_SZ_BY2)];
      sum1 += v1;
      sum2 += v2;
      sumsq2 += v2 * v2;
      cross += v1 * v2;
    }
  var2 = sumsq2 * MATCH_SZ_SQ - sum2 * sum2;
  cov = cross * MATCH_SZ_SQ - sum1 * sum2;
  return cov / sqrt((double)var2);
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

static void improve_correspondence(const unsigned char *src,
                                   const unsigned char *ref, int width,
                                   int height, int src_stride, int ref_stride,
                                   Correspondence *correspondences,
                                   int num_correspondences) {
  int i;
  for (i = 0; i < num_correspondences; ++i) {
    int x, y, best_x = 0, best_y = 0;
    double best_match_ncc = 0.0;
    // For this algorithm, all points have integer coordinates.
    // It's a little more efficient to convert them to ints once,
    // before the inner loops
    int x0 = (int)correspondences[i].x;
    int y0 = (int)correspondences[i].y;
    int rx0 = (int)correspondences[i].rx;
    int ry0 = (int)correspondences[i].ry;
    for (y = -SEARCH_SZ_BY2; y <= SEARCH_SZ_BY2; ++y) {
      for (x = -SEARCH_SZ_BY2; x <= SEARCH_SZ_BY2; ++x) {
        double match_ncc;
        if (!is_eligible_point(rx0 + x, ry0 + y, width, height)) continue;
        if (!is_eligible_distance(x0, y0, rx0 + x, ry0 + y, width, height))
          continue;
        match_ncc = av1_compute_cross_correlation(src, src_stride, x0, y0, ref,
                                                  ref_stride, rx0 + x, ry0 + y);
        if (match_ncc > best_match_ncc) {
          best_match_ncc = match_ncc;
          best_y = y;
          best_x = x;
        }
      }
    }
    correspondences[i].rx += best_x;
    correspondences[i].ry += best_y;
  }
  for (i = 0; i < num_correspondences; ++i) {
    int x, y, best_x = 0, best_y = 0;
    double best_match_ncc = 0.0;
    int x0 = (int)correspondences[i].x;
    int y0 = (int)correspondences[i].y;
    int rx0 = (int)correspondences[i].rx;
    int ry0 = (int)correspondences[i].ry;
    for (y = -SEARCH_SZ_BY2; y <= SEARCH_SZ_BY2; ++y)
      for (x = -SEARCH_SZ_BY2; x <= SEARCH_SZ_BY2; ++x) {
        double match_ncc;
        if (!is_eligible_point(x0 + x, y0 + y, width, height)) continue;
        if (!is_eligible_distance(x0 + x, y0 + y, rx0, ry0, width, height))
          continue;
        match_ncc = av1_compute_cross_correlation(
            ref, ref_stride, rx0, ry0, src, src_stride, x0 + x, y0 + y);
        if (match_ncc > best_match_ncc) {
          best_match_ncc = match_ncc;
          best_y = y;
          best_x = x;
        }
      }
    correspondences[i].x += best_x;
    correspondences[i].y += best_y;
  }
}

int aom_determine_correspondence(const unsigned char *src,
                                 const int *src_corners, int num_src_corners,
                                 const unsigned char *ref,
                                 const int *ref_corners, int num_ref_corners,
                                 int width, int height, int src_stride,
                                 int ref_stride,
                                 Correspondence *correspondences) {
  // TODO(sarahparker) Improve this to include 2-way match
  int i, j;
  int num_correspondences = 0;
  for (i = 0; i < num_src_corners; ++i) {
    double best_match_ncc = 0.0;
    double template_norm;
    int best_match_j = -1;
    if (!is_eligible_point(src_corners[2 * i], src_corners[2 * i + 1], width,
                           height))
      continue;
    for (j = 0; j < num_ref_corners; ++j) {
      double match_ncc;
      if (!is_eligible_point(ref_corners[2 * j], ref_corners[2 * j + 1], width,
                             height))
        continue;
      if (!is_eligible_distance(src_corners[2 * i], src_corners[2 * i + 1],
                                ref_corners[2 * j], ref_corners[2 * j + 1],
                                width, height))
        continue;
      match_ncc = av1_compute_cross_correlation(
          src, src_stride, src_corners[2 * i], src_corners[2 * i + 1], ref,
          ref_stride, ref_corners[2 * j], ref_corners[2 * j + 1]);
      if (match_ncc > best_match_ncc) {
        best_match_ncc = match_ncc;
        best_match_j = j;
      }
    }
    // Note: We want to test if the best correlation is >= THRESHOLD_NCC,
    // but need to account for the normalization in
    // av1_compute_cross_correlation.
    template_norm = compute_variance(src, src_stride, src_corners[2 * i],
                                     src_corners[2 * i + 1]);
    if (best_match_ncc > THRESHOLD_NCC * sqrt(template_norm)) {
      correspondences[num_correspondences].x = src_corners[2 * i];
      correspondences[num_correspondences].y = src_corners[2 * i + 1];
      correspondences[num_correspondences].rx = ref_corners[2 * best_match_j];
      correspondences[num_correspondences].ry =
          ref_corners[2 * best_match_j + 1];
      num_correspondences++;
    }
  }
  improve_correspondence(src, ref, width, height, src_stride, ref_stride,
                         correspondences, num_correspondences);
  return num_correspondences;
}

bool av1_compute_global_motion_feature_match(
    TransformationType type, YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *ref,
    int bit_depth, MotionModel *motion_models, int num_motion_models) {
  int num_correspondences;
  Correspondence *correspondences;
  ImagePyramid *src_pyramid = src->y_pyramid;
  CornerList *src_corners = src->corners;
  ImagePyramid *ref_pyramid = ref->y_pyramid;
  CornerList *ref_corners = ref->corners;

  // Precompute information we will need about each frame
  aom_compute_pyramid(src, bit_depth, src_pyramid);
  av1_compute_corner_list(src_pyramid, src_corners);
  aom_compute_pyramid(ref, bit_depth, ref_pyramid);
  av1_compute_corner_list(ref_pyramid, ref_corners);

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
  if (!correspondences) return false;
  num_correspondences = aom_determine_correspondence(
      src_buffer, src_corners->corners, src_corners->num_corners, ref_buffer,
      ref_corners->corners, ref_corners->num_corners, src_width, src_height,
      src_stride, ref_stride, correspondences);

  bool result = ransac(correspondences, num_correspondences, type,
                       motion_models, num_motion_models);

  aom_free(correspondences);
  return result;
}
