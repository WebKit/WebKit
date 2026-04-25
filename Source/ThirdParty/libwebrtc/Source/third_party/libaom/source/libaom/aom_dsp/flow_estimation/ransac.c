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

#include <memory.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "aom_dsp/flow_estimation/ransac.h"
#include "aom_dsp/mathutils.h"
#include "aom_mem/aom_mem.h"

// TODO(rachelbarker): Remove dependence on code in av1/encoder/
#include "av1/encoder/random.h"

#define MAX_MINPTS 4
#define MINPTS_MULTIPLIER 5

#define INLIER_THRESHOLD 1.25
#define INLIER_THRESHOLD_SQUARED (INLIER_THRESHOLD * INLIER_THRESHOLD)

// Number of initial models to generate
#define NUM_TRIALS 20

// Number of times to refine the best model found
#define NUM_REFINES 5

// Flag to enable functions for finding TRANSLATION type models.
//
// These modes are not considered currently due to a spec bug (see comments
// in gm_get_motion_vector() in av1/common/mv.h). Thus we don't need to compile
// the corresponding search functions, but it is nice to keep the source around
// but disabled, for completeness.
#define ALLOW_TRANSLATION_MODELS 0

typedef struct {
  int num_inliers;
  double sse;  // Sum of squared errors of inliers
  int *inlier_indices;
} RANSAC_MOTION;

////////////////////////////////////////////////////////////////////////////////
// ransac
typedef bool (*FindTransformationFunc)(const Correspondence *points,
                                       const int *indices, int num_indices,
                                       double *params);
typedef void (*ScoreModelFunc)(const double *mat, const Correspondence *points,
                               int num_points, RANSAC_MOTION *model);

// vtable-like structure which stores all of the information needed by RANSAC
// for a particular model type
typedef struct {
  FindTransformationFunc find_transformation;
  ScoreModelFunc score_model;

  // The minimum number of points which can be passed to find_transformation
  // to generate a model.
  //
  // This should be set as small as possible. This is due to an observation
  // from section 4 of "Optimal Ransac" by A. Hast, J. Nysjö and
  // A. Marchetti (https://dspace5.zcu.cz/bitstream/11025/6869/1/Hast.pdf):
  // using the minimum possible number of points in the initial model maximizes
  // the chance that all of the selected points are inliers.
  //
  // That paper proposes a method which can deal with models which are
  // contaminated by outliers, which helps in cases where the inlier fraction
  // is low. However, for our purposes, global motion only gives significant
  // gains when the inlier fraction is high.
  //
  // So we do not use the method from this paper, but we do find that
  // minimizing the number of points used for initial model fitting helps
  // make the best use of the limited number of models we consider.
  int minpts;
} RansacModelInfo;

#if ALLOW_TRANSLATION_MODELS
static void score_translation(const double *mat, const Correspondence *points,
                              int num_points, RANSAC_MOTION *model) {
  model->num_inliers = 0;
  model->sse = 0.0;

  for (int i = 0; i < num_points; ++i) {
    const double x1 = points[i].x;
    const double y1 = points[i].y;
    const double x2 = points[i].rx;
    const double y2 = points[i].ry;

    const double proj_x = x1 + mat[0];
    const double proj_y = y1 + mat[1];

    const double dx = proj_x - x2;
    const double dy = proj_y - y2;
    const double sse = dx * dx + dy * dy;

    if (sse < INLIER_THRESHOLD_SQUARED) {
      model->inlier_indices[model->num_inliers++] = i;
      model->sse += sse;
    }
  }
}
#endif  // ALLOW_TRANSLATION_MODELS

static void score_affine(const double *mat, const Correspondence *points,
                         int num_points, RANSAC_MOTION *model) {
  model->num_inliers = 0;
  model->sse = 0.0;

  for (int i = 0; i < num_points; ++i) {
    const double x1 = points[i].x;
    const double y1 = points[i].y;
    const double x2 = points[i].rx;
    const double y2 = points[i].ry;

    const double proj_x = mat[2] * x1 + mat[3] * y1 + mat[0];
    const double proj_y = mat[4] * x1 + mat[5] * y1 + mat[1];

    const double dx = proj_x - x2;
    const double dy = proj_y - y2;
    const double sse = dx * dx + dy * dy;

    if (sse < INLIER_THRESHOLD_SQUARED) {
      model->inlier_indices[model->num_inliers++] = i;
      model->sse += sse;
    }
  }
}

#if ALLOW_TRANSLATION_MODELS
static bool find_translation(const Correspondence *points, const int *indices,
                             int num_indices, double *params) {
  double sumx = 0;
  double sumy = 0;

  for (int i = 0; i < num_indices; ++i) {
    int index = indices[i];
    const double sx = points[index].x;
    const double sy = points[index].y;
    const double dx = points[index].rx;
    const double dy = points[index].ry;

    sumx += dx - sx;
    sumy += dy - sy;
  }

  params[0] = sumx / np;
  params[1] = sumy / np;
  params[2] = 1;
  params[3] = 0;
  params[4] = 0;
  params[5] = 1;
  return true;
}
#endif  // ALLOW_TRANSLATION_MODELS

static bool find_rotzoom(const Correspondence *points, const int *indices,
                         int num_indices, double *params) {
  const int n = 4;    // Size of least-squares problem
  double mat[4 * 4];  // Accumulator for A'A
  double y[4];        // Accumulator for A'b
  double a[4];        // Single row of A
  double b;           // Single element of b

  least_squares_init(mat, y, n);
  for (int i = 0; i < num_indices; ++i) {
    int index = indices[i];
    const double sx = points[index].x;
    const double sy = points[index].y;
    const double dx = points[index].rx;
    const double dy = points[index].ry;

    a[0] = 1;
    a[1] = 0;
    a[2] = sx;
    a[3] = sy;
    b = dx;
    least_squares_accumulate(mat, y, a, b, n);

    a[0] = 0;
    a[1] = 1;
    a[2] = sy;
    a[3] = -sx;
    b = dy;
    least_squares_accumulate(mat, y, a, b, n);
  }

  // Fill in params[0] .. params[3] with output model
  if (!least_squares_solve(mat, y, params, n)) {
    return false;
  }

  // Fill in remaining parameters
  params[4] = -params[3];
  params[5] = params[2];

  return true;
}

static bool find_affine(const Correspondence *points, const int *indices,
                        int num_indices, double *params) {
  // Note: The least squares problem for affine models is 6-dimensional,
  // but it splits into two independent 3-dimensional subproblems.
  // Solving these two subproblems separately and recombining at the end
  // results in less total computation than solving the 6-dimensional
  // problem directly.
  //
  // The two subproblems correspond to all the parameters which contribute
  // to the x output of the model, and all the parameters which contribute
  // to the y output, respectively.

  const int n = 3;       // Size of each least-squares problem
  double mat[2][3 * 3];  // Accumulator for A'A
  double y[2][3];        // Accumulator for A'b
  double x[2][3];        // Output vector
  double a[2][3];        // Single row of A
  double b[2];           // Single element of b

  least_squares_init(mat[0], y[0], n);
  least_squares_init(mat[1], y[1], n);
  for (int i = 0; i < num_indices; ++i) {
    int index = indices[i];
    const double sx = points[index].x;
    const double sy = points[index].y;
    const double dx = points[index].rx;
    const double dy = points[index].ry;

    a[0][0] = 1;
    a[0][1] = sx;
    a[0][2] = sy;
    b[0] = dx;
    least_squares_accumulate(mat[0], y[0], a[0], b[0], n);

    a[1][0] = 1;
    a[1][1] = sx;
    a[1][2] = sy;
    b[1] = dy;
    least_squares_accumulate(mat[1], y[1], a[1], b[1], n);
  }

  if (!least_squares_solve(mat[0], y[0], x[0], n)) {
    return false;
  }
  if (!least_squares_solve(mat[1], y[1], x[1], n)) {
    return false;
  }

  // Rearrange least squares result to form output model
  params[0] = x[0][0];
  params[1] = x[1][0];
  params[2] = x[0][1];
  params[3] = x[0][2];
  params[4] = x[1][1];
  params[5] = x[1][2];

  return true;
}

// Return -1 if 'a' is a better motion, 1 if 'b' is better, 0 otherwise.
static int compare_motions(const void *arg_a, const void *arg_b) {
  const RANSAC_MOTION *motion_a = (RANSAC_MOTION *)arg_a;
  const RANSAC_MOTION *motion_b = (RANSAC_MOTION *)arg_b;

  if (motion_a->num_inliers > motion_b->num_inliers) return -1;
  if (motion_a->num_inliers < motion_b->num_inliers) return 1;
  if (motion_a->sse < motion_b->sse) return -1;
  if (motion_a->sse > motion_b->sse) return 1;
  return 0;
}

static bool is_better_motion(const RANSAC_MOTION *motion_a,
                             const RANSAC_MOTION *motion_b) {
  return compare_motions(motion_a, motion_b) < 0;
}

// Returns true on success, false on error
static bool ransac_internal(const Correspondence *matched_points, int npoints,
                            MotionModel *motion_models, int num_desired_motions,
                            const RansacModelInfo *model_info,
                            bool *mem_alloc_failed) {
  assert(npoints >= 0);
  int i = 0;
  int minpts = model_info->minpts;
  bool ret_val = true;

  unsigned int seed = (unsigned int)npoints;

  int indices[MAX_MINPTS] = { 0 };

  // Store information for the num_desired_motions best transformations found
  // and the worst motion among them, as well as the motion currently under
  // consideration.
  RANSAC_MOTION *motions, *worst_kept_motion = NULL;
  RANSAC_MOTION current_motion;

  // Store the parameters and the indices of the inlier points for the motion
  // currently under consideration.
  double params_this_motion[MAX_PARAMDIM];

  // Initialize output models, as a fallback in case we can't find a model
  for (i = 0; i < num_desired_motions; i++) {
    memcpy(motion_models[i].params, kIdentityParams,
           MAX_PARAMDIM * sizeof(*(motion_models[i].params)));
    motion_models[i].num_inliers = 0;
  }

  if (npoints < minpts * MINPTS_MULTIPLIER || npoints == 0) {
    return false;
  }

  int min_inliers = AOMMAX((int)(MIN_INLIER_PROB * npoints), minpts);

  motions =
      (RANSAC_MOTION *)aom_calloc(num_desired_motions, sizeof(RANSAC_MOTION));

  // Allocate one large buffer which will be carved up to store the inlier
  // indices for the current motion plus the num_desired_motions many
  // output models
  // This allows us to keep the allocation/deallocation logic simple, without
  // having to (for example) check that `motions` is non-null before allocating
  // the inlier arrays
  int *inlier_buffer = (int *)aom_malloc(sizeof(*inlier_buffer) * npoints *
                                         (num_desired_motions + 1));

  if (!(motions && inlier_buffer)) {
    ret_val = false;
    *mem_alloc_failed = true;
    goto finish_ransac;
  }

  // Once all our allocations are known-good, we can fill in our structures
  worst_kept_motion = motions;

  for (i = 0; i < num_desired_motions; ++i) {
    motions[i].inlier_indices = inlier_buffer + i * npoints;
  }
  memset(&current_motion, 0, sizeof(current_motion));
  current_motion.inlier_indices = inlier_buffer + num_desired_motions * npoints;

  for (int trial_count = 0; trial_count < NUM_TRIALS; trial_count++) {
    lcg_pick(npoints, minpts, indices, &seed);

    if (!model_info->find_transformation(matched_points, indices, minpts,
                                         params_this_motion)) {
      continue;
    }

    model_info->score_model(params_this_motion, matched_points, npoints,
                            &current_motion);

    if (current_motion.num_inliers < min_inliers) {
      // Reject models with too few inliers
      continue;
    }

    if (is_better_motion(&current_motion, worst_kept_motion)) {
      // This motion is better than the worst currently kept motion. Remember
      // the inlier points and sse. The parameters for each kept motion
      // will be recomputed later using only the inliers.
      worst_kept_motion->num_inliers = current_motion.num_inliers;
      worst_kept_motion->sse = current_motion.sse;

      // Rather than copying the (potentially many) inlier indices from
      // current_motion.inlier_indices to worst_kept_motion->inlier_indices,
      // we can swap the underlying pointers.
      //
      // This is okay because the next time current_motion.inlier_indices
      // is used will be in the next trial, where we ignore its previous
      // contents anyway. And both arrays will be deallocated together at the
      // end of this function, so there are no lifetime issues.
      int *tmp = worst_kept_motion->inlier_indices;
      worst_kept_motion->inlier_indices = current_motion.inlier_indices;
      current_motion.inlier_indices = tmp;

      // Determine the new worst kept motion and its num_inliers and sse.
      for (i = 0; i < num_desired_motions; ++i) {
        if (is_better_motion(worst_kept_motion, &motions[i])) {
          worst_kept_motion = &motions[i];
        }
      }
    }
  }

  // Sort the motions, best first.
  qsort(motions, num_desired_motions, sizeof(RANSAC_MOTION), compare_motions);

  // Refine each of the best N models using iterative estimation.
  //
  // The idea here is loosely based on the iterative method from
  // "Locally Optimized RANSAC" by O. Chum, J. Matas and Josef Kittler:
  // https://cmp.felk.cvut.cz/ftp/articles/matas/chum-dagm03.pdf
  //
  // However, we implement a simpler version than their proposal, and simply
  // refit the model repeatedly until the number of inliers stops increasing,
  // with a cap on the number of iterations to defend against edge cases which
  // only improve very slowly.
  for (i = 0; i < num_desired_motions; ++i) {
    if (motions[i].num_inliers <= 0) {
      // Output model has already been initialized to the identity model,
      // so just skip setup
      continue;
    }

    bool bad_model = false;
    for (int refine_count = 0; refine_count < NUM_REFINES; refine_count++) {
      int num_inliers = motions[i].num_inliers;
      assert(num_inliers >= min_inliers);

      if (!model_info->find_transformation(matched_points,
                                           motions[i].inlier_indices,
                                           num_inliers, params_this_motion)) {
        // In the unlikely event that this model fitting fails, we don't have a
        // good fallback. So leave this model set to the identity model
        bad_model = true;
        break;
      }

      // Score the newly generated model
      model_info->score_model(params_this_motion, matched_points, npoints,
                              &current_motion);

      // At this point, there are three possibilities:
      // 1) If we found more inliers, keep refining.
      // 2) If we found the same number of inliers but a lower SSE, we want to
      //    keep the new model, but further refinement is unlikely to gain much.
      //    So commit to this new model
      // 3) It is possible, but very unlikely, that the new model will have
      //    fewer inliers. If it does happen, we probably just lost a few
      //    borderline inliers. So treat the same as case (2).
      if (current_motion.num_inliers > motions[i].num_inliers) {
        motions[i].num_inliers = current_motion.num_inliers;
        motions[i].sse = current_motion.sse;
        int *tmp = motions[i].inlier_indices;
        motions[i].inlier_indices = current_motion.inlier_indices;
        current_motion.inlier_indices = tmp;
      } else {
        // Refined model is no better, so stop
        // This shouldn't be significantly worse than the previous model,
        // so it's fine to use the parameters in params_this_motion.
        // This saves us from having to cache the previous iteration's params.
        break;
      }
    }

    if (bad_model) continue;

    // Fill in output struct
    memcpy(motion_models[i].params, params_this_motion,
           MAX_PARAMDIM * sizeof(*motion_models[i].params));
    for (int j = 0; j < motions[i].num_inliers; j++) {
      int index = motions[i].inlier_indices[j];
      const Correspondence *corr = &matched_points[index];
      motion_models[i].inliers[2 * j + 0] = (int)rint(corr->x);
      motion_models[i].inliers[2 * j + 1] = (int)rint(corr->y);
    }
    motion_models[i].num_inliers = motions[i].num_inliers;
  }

finish_ransac:
  aom_free(inlier_buffer);
  aom_free(motions);

  return ret_val;
}

static const RansacModelInfo ransac_model_info[TRANS_TYPES] = {
  // IDENTITY
  { NULL, NULL, 0 },
// TRANSLATION
#if ALLOW_TRANSLATION_MODELS
  { find_translation, score_translation, 1 },
#else
  { NULL, NULL, 0 },
#endif
  // ROTZOOM
  { find_rotzoom, score_affine, 2 },
  // AFFINE
  { find_affine, score_affine, 3 },
};

// Returns true on success, false on error
bool ransac(const Correspondence *matched_points, int npoints,
            TransformationType type, MotionModel *motion_models,
            int num_desired_motions, bool *mem_alloc_failed) {
#if ALLOW_TRANSLATION_MODELS
  assert(type > IDENTITY && type < TRANS_TYPES);
#else
  assert(type > TRANSLATION && type < TRANS_TYPES);
#endif  // ALLOW_TRANSLATION_MODELS

  return ransac_internal(matched_points, npoints, motion_models,
                         num_desired_motions, &ransac_model_info[type],
                         mem_alloc_failed);
}
