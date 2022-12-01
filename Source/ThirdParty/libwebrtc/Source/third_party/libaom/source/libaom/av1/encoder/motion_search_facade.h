/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_MOTION_SEARCH_H_
#define AOM_AV1_ENCODER_MOTION_SEARCH_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO(any): rename this struct to something else. There is already another
// struct called inter_modes_info, which makes this terribly confusing.
typedef struct {
  int drl_cost;
  int_mv full_search_mv;
  int full_mv_rate;
  int full_mv_bestsme;
  int skip;
} inter_mode_info;

struct HandleInterModeArgs;
void av1_single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                              BLOCK_SIZE bsize, int ref_idx, int *rate_mv,
                              int search_range, inter_mode_info *mode_info,
                              int_mv *best_mv,
                              struct HandleInterModeArgs *const args);

int av1_joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, int_mv *cur_mv,
                            const uint8_t *mask, int mask_stride, int *rate_mv,
                            int allow_second_mv);

int av1_interinter_compound_motion_search(const AV1_COMP *const cpi,
                                          MACROBLOCK *x,
                                          const int_mv *const cur_mv,
                                          const BLOCK_SIZE bsize,
                                          const PREDICTION_MODE this_mode);

int av1_compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *cur_mv,
    const uint8_t *mask, int mask_stride, int *rate_mv, int ref_idx);

int av1_compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, MV *this_mv,
                                      const uint8_t *second_pred,
                                      const uint8_t *mask, int mask_stride,
                                      int *rate_mv, int ref_idx);

// Performs a motion search in SIMPLE_TRANSLATION mode using reference frame
// ref. Note that this sets the offset of mbmi, so we will need to reset it
// after calling this function.
int_mv av1_simple_motion_search(struct AV1_COMP *const cpi, MACROBLOCK *x,
                                int mi_row, int mi_col, BLOCK_SIZE bsize,
                                int ref, FULLPEL_MV start_mv, int num_planes,
                                int use_subpixel);

// Performs a simple motion search to calculate the sse and var of the residue
int_mv av1_simple_motion_sse_var(struct AV1_COMP *cpi, MACROBLOCK *x,
                                 int mi_row, int mi_col, BLOCK_SIZE bsize,
                                 const FULLPEL_MV start_mv, int use_subpixel,
                                 unsigned int *sse, unsigned int *var);

static AOM_INLINE const search_site_config *av1_get_search_site_config(
    search_site_config *ss_cfg_buf,
    const MotionVectorSearchParams *mv_search_params,
    SEARCH_METHODS search_method, const int ref_stride) {
  if (ref_stride == mv_search_params->search_site_cfg[SS_CFG_SRC]->stride) {
    return mv_search_params->search_site_cfg[SS_CFG_SRC];
  } else if (ref_stride ==
             mv_search_params->search_site_cfg[SS_CFG_LOOKAHEAD]->stride) {
    return mv_search_params->search_site_cfg[SS_CFG_LOOKAHEAD];
  }

  if (ref_stride != ss_cfg_buf[search_method].stride) {
    const int level =
        search_method == NSTEP_8PT || search_method == CLAMPED_DIAMOND;
    search_method = search_method_lookup[search_method];
    av1_init_motion_compensation[search_method](&ss_cfg_buf[search_method],
                                                ref_stride, level);
  }

  return ss_cfg_buf;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_MOTION_SEARCH_H_
