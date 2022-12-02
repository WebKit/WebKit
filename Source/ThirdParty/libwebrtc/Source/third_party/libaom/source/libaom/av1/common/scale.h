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

#ifndef AOM_AV1_COMMON_SCALE_H_
#define AOM_AV1_COMMON_SCALE_H_

#include "av1/common/convolve.h"
#include "av1/common/mv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCALE_NUMERATOR 8

#define REF_SCALE_SHIFT 14
#define REF_NO_SCALE (1 << REF_SCALE_SHIFT)
#define REF_INVALID_SCALE -1

struct scale_factors {
  int x_scale_fp;  // horizontal fixed point scale factor
  int y_scale_fp;  // vertical fixed point scale factor
  int x_step_q4;
  int y_step_q4;

  int (*scale_value_x)(int val, const struct scale_factors *sf);
  int (*scale_value_y)(int val, const struct scale_factors *sf);
};

MV32 av1_scale_mv(const MV *mv, int x, int y, const struct scale_factors *sf);

void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h);

static INLINE int av1_is_valid_scale(const struct scale_factors *sf) {
  assert(sf != NULL);
  return sf->x_scale_fp != REF_INVALID_SCALE &&
         sf->y_scale_fp != REF_INVALID_SCALE;
}

static INLINE int av1_is_scaled(const struct scale_factors *sf) {
  assert(sf != NULL);
  return av1_is_valid_scale(sf) &&
         (sf->x_scale_fp != REF_NO_SCALE || sf->y_scale_fp != REF_NO_SCALE);
}

static INLINE int valid_ref_frame_size(int ref_width, int ref_height,
                                       int this_width, int this_height) {
  return 2 * this_width >= ref_width && 2 * this_height >= ref_height &&
         this_width <= 16 * ref_width && this_height <= 16 * ref_height;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_SCALE_H_
