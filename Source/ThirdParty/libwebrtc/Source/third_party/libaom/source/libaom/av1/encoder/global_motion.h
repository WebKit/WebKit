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

#ifndef AOM_AV1_ENCODER_GLOBAL_MOTION_H_
#define AOM_AV1_ENCODER_GLOBAL_MOTION_H_

#include "aom/aom_integer.h"
#include "aom_dsp/flow_estimation/flow_estimation.h"
#include "aom_scale/yv12config.h"
#include "aom_util/aom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RANSAC_NUM_MOTIONS 1
#define GM_MAX_REFINEMENT_STEPS 5
#define MAX_DIRECTIONS 2

// The structure holds a valid reference frame type and its temporal distance
// from the source frame.
typedef struct {
  int distance;
  MV_REFERENCE_FRAME frame;
} FrameDistPair;

typedef struct {
  // Array of structure which holds the global motion parameters for a given
  // motion model. motion_models[i] holds the parameters for a given motion
  // model for the ith ransac motion.
  MotionModel motion_models[RANSAC_NUM_MOTIONS];

  // Pointer to hold inliers from motion model.
  uint8_t *segment_map;
} GlobalMotionData;

typedef struct {
  // Holds the mapping of each thread to past/future direction.
  // thread_id_to_dir[i] indicates the direction id (past - 0/future - 1)
  // assigned to the ith thread.
  int8_t thread_id_to_dir[MAX_NUM_THREADS];

  // A flag which holds the early exit status based on the speed feature
  // 'prune_ref_frame_for_gm_search'. early_exit[i] will be set if the speed
  // feature based early exit happens in the direction 'i'.
  int8_t early_exit[MAX_DIRECTIONS];

  // Counter for the next reference frame to be processed.
  // next_frame_to_process[i] will hold the count of next reference frame to be
  // processed in the direction 'i'.
  int8_t next_frame_to_process[MAX_DIRECTIONS];
} JobInfo;

typedef struct {
  // Data related to assigning jobs for global motion multi-threading.
  JobInfo job_info;

#if CONFIG_MULTITHREAD
  // Mutex lock used while dispatching jobs.
  pthread_mutex_t *mutex_;
#endif

  // Initialized to false, set to true by the worker thread that encounters an
  // error in order to abort the processing of other worker threads.
  bool gm_mt_exit;
} AV1GlobalMotionSync;

void av1_convert_model_to_params(const double *params,
                                 WarpedMotionParams *model);

// Criteria for accepting a global motion model
static const double erroradv_tr = 0.65;
static const double erroradv_prod_tr = 20000;

// Early exit threshold for global motion refinement
// This is set slightly higher than erroradv_tr, as a compromise between
// two factors:
//
// 1) By rejecting un-promising models early, we can reduce the encode time
//    spent trying to refine them
//
// 2) When we refine a model, its error may decrease to below the acceptance
//    threshold even if the model is initially above the threshold
static const double erroradv_early_tr = 0.70;

int av1_is_enough_erroradvantage(double best_erroradvantage, int params_cost);

void av1_compute_feature_segmentation_map(uint8_t *segment_map, int width,
                                          int height, int *inliers,
                                          int num_inliers);

extern const int error_measure_lut[513];

static INLINE int error_measure(int err) {
  return error_measure_lut[256 + err];
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE int highbd_error_measure(int err, int bd) {
  const int b = bd - 8;
  const int bmask = (1 << b) - 1;
  const int v = (1 << b);

  // Split error into two parts and do an interpolated table lookup
  // To compute the table index and interpolation value, we want to calculate
  // the quotient and remainder of err / 2^b. But it is very important that
  // the division must round down, and the remainder must be positive,
  // ie. in the range [0, 2^b).
  //
  // In C, the >> and & operators do what we want, but the / and % operators
  // give the wrong results for negative inputs. So we must use >> and & here.
  //
  // For example, if bd == 10 and err == -5, compare the results:
  //       (-5) >> 2 = -2, (-5) & 3 =  3
  //   vs. (-5) / 4  = -1, (-5) % 4 = -1
  const int e1 = err >> b;
  const int e2 = err & bmask;
  return error_measure_lut[256 + e1] * (v - e2) +
         error_measure_lut[257 + e1] * e2;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Returns the error between the frame described by 'ref' and the frame
// described by 'dst'.
int64_t av1_frame_error(int use_hbd, int bd, const uint8_t *ref, int stride,
                        uint8_t *dst, int p_width, int p_height, int p_stride);

int64_t av1_segmented_frame_error(int use_hbd, int bd, const uint8_t *ref,
                                  int ref_stride, uint8_t *dst, int dst_stride,
                                  int p_width, int p_height,
                                  uint8_t *segment_map, int segment_map_stride);

// Returns the error between the result of applying motion 'wm' to the frame
// described by 'ref' and the frame described by 'dst'.
int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int ref_width, int ref_height,
                       int ref_stride, uint8_t *dst, int dst_stride, int p_col,
                       int p_row, int p_width, int p_height, int subsampling_x,
                       int subsampling_y, int64_t best_error,
                       uint8_t *segment_map, int segment_map_stride);

// Returns the av1_warp_error between "dst" and the result of applying the
// motion params that result from fine-tuning "wm" to "ref". Note that "wm" is
// modified in place.
int64_t av1_refine_integerized_param(
    WarpedMotionParams *wm, TransformationType wmtype, int use_hbd, int bd,
    uint8_t *ref, int r_width, int r_height, int r_stride, uint8_t *dst,
    int d_width, int d_height, int d_stride, int n_refinements,
    int64_t ref_frame_error, uint8_t *segment_map, int segment_map_stride);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_GLOBAL_MOTION_H_
