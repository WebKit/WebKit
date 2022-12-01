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
#include "aom_scale/yv12config.h"
#include "aom_util/aom_thread.h"

#include "av1/common/mv.h"
#include "av1/common/warped_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CORNERS 4096
#define RANSAC_NUM_MOTIONS 1
#define GM_REFINEMENT_COUNT 5
#define MAX_DIRECTIONS 2

typedef enum {
  GLOBAL_MOTION_FEATURE_BASED,
  GLOBAL_MOTION_DISFLOW_BASED,
} GlobalMotionEstimationType;

unsigned char *av1_downconvert_frame(YV12_BUFFER_CONFIG *frm, int bit_depth);

typedef struct {
  double params[MAX_PARAMDIM - 1];
  int *inliers;
  int num_inliers;
} MotionModel;

// The structure holds a valid reference frame type and its temporal distance
// from the source frame.
typedef struct {
  int distance;
  MV_REFERENCE_FRAME frame;
} FrameDistPair;

typedef struct {
  // Array of structure which holds the global motion parameters for a given
  // motion model. params_by_motion[i] holds the parameters for a given motion
  // model for the ith ransac motion.
  MotionModel params_by_motion[RANSAC_NUM_MOTIONS];

  // Pointer to hold inliers from motion model.
  uint8_t *segment_map;
} GlobalMotionThreadData;

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

  // Data specific to each worker in global motion multi-threading.
  // thread_data[i] stores the thread specific data for worker 'i'.
  GlobalMotionThreadData *thread_data;

#if CONFIG_MULTITHREAD
  // Mutex lock used while dispatching jobs.
  pthread_mutex_t *mutex_;
#endif

  // Width and height for which segment_map is allocated for each thread.
  int allocated_width;
  int allocated_height;

  // Number of workers for which thread_data is allocated.
  int8_t allocated_workers;
} AV1GlobalMotionSync;

void av1_convert_model_to_params(const double *params,
                                 WarpedMotionParams *model);

// TODO(sarahparker) These need to be retuned for speed 0 and 1 to
// maximize gains from segmented error metric
static const double erroradv_tr = 0.65;
static const double erroradv_prod_tr = 20000;

int av1_is_enough_erroradvantage(double best_erroradvantage, int params_cost);

void av1_compute_feature_segmentation_map(uint8_t *segment_map, int width,
                                          int height, int *inliers,
                                          int num_inliers);

// Returns the error between the result of applying motion 'wm' to the frame
// described by 'ref' and the frame described by 'dst'.
int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int64_t best_error,
                       uint8_t *segment_map, int segment_map_stride);

// Returns the av1_warp_error between "dst" and the result of applying the
// motion params that result from fine-tuning "wm" to "ref". Note that "wm" is
// modified in place.
int64_t av1_refine_integerized_param(
    WarpedMotionParams *wm, TransformationType wmtype, int use_hbd, int bd,
    uint8_t *ref, int r_width, int r_height, int r_stride, uint8_t *dst,
    int d_width, int d_height, int d_stride, int n_refinements,
    int64_t best_frame_error, uint8_t *segment_map, int segment_map_stride,
    int64_t erroradv_threshold);

/*
  Computes "num_motions" candidate global motion parameters between two frames.
  The array "params_by_motion" should be length 8 * "num_motions". The ordering
  of each set of parameters is best described  by the homography:

        [x'     (m2 m3 m0   [x
    z .  y'  =   m4 m5 m1 *  y
         1]      m6 m7 1)    1]

  where m{i} represents the ith value in any given set of parameters.

  "num_inliers" should be length "num_motions", and will be populated with the
  number of inlier feature points for each motion. Params for which the
  num_inliers entry is 0 should be ignored by the caller.
*/
int av1_compute_global_motion(TransformationType type,
                              unsigned char *src_buffer, int src_width,
                              int src_height, int src_stride, int *src_corners,
                              int num_src_corners, YV12_BUFFER_CONFIG *ref,
                              int bit_depth,
                              GlobalMotionEstimationType gm_estimation_type,
                              int *num_inliers_by_motion,
                              MotionModel *params_by_motion, int num_motions);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_GLOBAL_MOTION_H_
