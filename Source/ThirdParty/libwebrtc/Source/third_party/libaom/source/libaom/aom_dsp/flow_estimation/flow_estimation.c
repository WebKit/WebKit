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

#include <assert.h>

#include "aom_dsp/flow_estimation/corner_detect.h"
#include "aom_dsp/flow_estimation/corner_match.h"
#include "aom_dsp/flow_estimation/disflow.h"
#include "aom_dsp/flow_estimation/flow_estimation.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

// For each global motion method, how many pyramid levels should we allocate?
// Note that this is a maximum, and fewer levels will be allocated if the frame
// is not large enough to need all of the specified levels
const int global_motion_pyr_levels[GLOBAL_MOTION_METHODS] = {
  1,   // GLOBAL_MOTION_METHOD_FEATURE_MATCH
  16,  // GLOBAL_MOTION_METHOD_DISFLOW
};

// clang-format off
const double kIdentityParams[MAX_PARAMDIM] = {
  0.0, 0.0, 1.0, 0.0, 0.0, 1.0
};
// clang-format on

// Compute a global motion model between the given source and ref frames.
//
// As is standard for video codecs, the resulting model maps from (x, y)
// coordinates in `src` to the corresponding points in `ref`, regardless
// of the temporal order of the two frames.
//
// Returns true if global motion estimation succeeded, false if not.
// The output models should only be used if this function succeeds.
bool aom_compute_global_motion(TransformationType type, YV12_BUFFER_CONFIG *src,
                               YV12_BUFFER_CONFIG *ref, int bit_depth,
                               GlobalMotionMethod gm_method,
                               MotionModel *motion_models,
                               int num_motion_models) {
  switch (gm_method) {
    case GLOBAL_MOTION_METHOD_FEATURE_MATCH:
      return av1_compute_global_motion_feature_match(
          type, src, ref, bit_depth, motion_models, num_motion_models);
    case GLOBAL_MOTION_METHOD_DISFLOW:
      return av1_compute_global_motion_disflow(
          type, src, ref, bit_depth, motion_models, num_motion_models);
    default: assert(0 && "Unknown global motion estimation type");
  }
  return 0;
}
