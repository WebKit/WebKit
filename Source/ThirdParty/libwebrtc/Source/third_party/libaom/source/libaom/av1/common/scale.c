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

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "av1/common/filter.h"
#include "av1/common/scale.h"
#include "aom_dsp/aom_filter.h"

static int get_fixed_point_scale_factor(int other_size, int this_size) {
  // Calculate scaling factor once for each reference frame
  // and use fixed point scaling factors in decoding and encoding routines.
  // Hardware implementations can calculate scale factor in device driver
  // and use multiplication and shifting on hardware instead of division.
  return ((other_size << REF_SCALE_SHIFT) + this_size / 2) / this_size;
}

// Given the fixed point scale, calculate coarse point scale.
static int fixed_point_scale_to_coarse_point_scale(int scale_fp) {
  return ROUND_POWER_OF_TWO(scale_fp, REF_SCALE_SHIFT - SCALE_SUBPEL_BITS);
}

// Note: x and y are integer precision, mvq4 is q4 precision.
MV32 av1_scale_mv(const MV *mvq4, int x, int y,
                  const struct scale_factors *sf) {
  const int x_off_q4 = av1_scaled_x(x << SUBPEL_BITS, sf);
  const int y_off_q4 = av1_scaled_y(y << SUBPEL_BITS, sf);
  const MV32 res = {
    av1_scaled_y((y << SUBPEL_BITS) + mvq4->row, sf) - y_off_q4,
    av1_scaled_x((x << SUBPEL_BITS) + mvq4->col, sf) - x_off_q4
  };
  return res;
}

void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h) {
  if (!valid_ref_frame_size(other_w, other_h, this_w, this_h)) {
    sf->x_scale_fp = REF_INVALID_SCALE;
    sf->y_scale_fp = REF_INVALID_SCALE;
    return;
  }

  sf->x_scale_fp = get_fixed_point_scale_factor(other_w, this_w);
  sf->y_scale_fp = get_fixed_point_scale_factor(other_h, this_h);

  sf->x_step_q4 = fixed_point_scale_to_coarse_point_scale(sf->x_scale_fp);
  sf->y_step_q4 = fixed_point_scale_to_coarse_point_scale(sf->y_scale_fp);
}
