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

#include <assert.h>

#include "av1/common/av1_loopfilter.h"
#include "av1/common/blockd.h"
#include "av1/common/seg_common.h"
#include "av1/common/quant_common.h"

static const int seg_feature_data_signed[SEG_LVL_MAX] = {
  1, 1, 1, 1, 1, 0, 0, 0
};

static const int seg_feature_data_max[SEG_LVL_MAX] = { MAXQ,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       MAX_LOOP_FILTER,
                                                       7,
                                                       0,
                                                       0 };

// These functions provide access to new segment level features.
// Eventually these function may be "optimized out" but for the moment,
// the coding mechanism is still subject to change so these provide a
// convenient single point of change.

void av1_clearall_segfeatures(struct segmentation *seg) {
  av1_zero(seg->feature_data);
  av1_zero(seg->feature_mask);
}

void av1_calculate_segdata(struct segmentation *seg) {
  seg->segid_preskip = 0;
  seg->last_active_segid = 0;
  for (int i = 0; i < MAX_SEGMENTS; i++) {
    for (int j = 0; j < SEG_LVL_MAX; j++) {
      if (seg->feature_mask[i] & (1 << j)) {
        seg->segid_preskip |= (j >= SEG_LVL_REF_FRAME);
        seg->last_active_segid = i;
      }
    }
  }
}

void av1_enable_segfeature(struct segmentation *seg, int segment_id,
                           SEG_LVL_FEATURES feature_id) {
  seg->feature_mask[segment_id] |= 1 << feature_id;
}

int av1_seg_feature_data_max(SEG_LVL_FEATURES feature_id) {
  return seg_feature_data_max[feature_id];
}

int av1_is_segfeature_signed(SEG_LVL_FEATURES feature_id) {
  return seg_feature_data_signed[feature_id];
}

// The 'seg_data' given for each segment can be either deltas (from the default
// value chosen for the frame) or absolute values.
//
// Valid range for abs values is (0-127 for MB_LVL_ALT_Q), (0-63 for
// SEGMENT_ALT_LF)
// Valid range for delta values are (+/-127 for MB_LVL_ALT_Q), (+/-63 for
// SEGMENT_ALT_LF)
//
// abs_delta = SEGMENT_DELTADATA (deltas) abs_delta = SEGMENT_ABSDATA (use
// the absolute values given).

void av1_set_segdata(struct segmentation *seg, int segment_id,
                     SEG_LVL_FEATURES feature_id, int seg_data) {
  if (seg_data < 0) {
    assert(seg_feature_data_signed[feature_id]);
    assert(-seg_data <= seg_feature_data_max[feature_id]);
  } else {
    assert(seg_data <= seg_feature_data_max[feature_id]);
  }

  seg->feature_data[segment_id][feature_id] = seg_data;
}

// TBD? Functions to read and write segment data with range / validity checking
