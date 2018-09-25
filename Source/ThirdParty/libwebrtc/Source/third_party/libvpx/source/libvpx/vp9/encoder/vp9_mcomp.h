/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_MCOMP_H_
#define VP9_ENCODER_VP9_MCOMP_H_

#include "vp9/encoder/vp9_block.h"
#include "vpx_dsp/variance.h"

#ifdef __cplusplus
extern "C" {
#endif

// The maximum number of steps in a step search given the largest
// allowed initial step
#define MAX_MVSEARCH_STEPS 11
// Max full pel mv specified in the unit of full pixel
// Enable the use of motion vector in range [-1023, 1023].
#define MAX_FULL_PEL_VAL ((1 << (MAX_MVSEARCH_STEPS - 1)) - 1)
// Maximum size of the first step in full pel units
#define MAX_FIRST_STEP (1 << (MAX_MVSEARCH_STEPS - 1))
// Allowed motion vector pixel distance outside image border
// for Block_16x16
#define BORDER_MV_PIXELS_B16 (16 + VP9_INTERP_EXTEND)

typedef struct search_site_config {
  // motion search sites
  MV ss_mv[8 * MAX_MVSEARCH_STEPS];        // Motion vector
  intptr_t ss_os[8 * MAX_MVSEARCH_STEPS];  // Offset
  int searches_per_step;
  int total_steps;
} search_site_config;

void vp9_init_dsmotion_compensation(search_site_config *cfg, int stride);
void vp9_init3smotion_compensation(search_site_config *cfg, int stride);

void vp9_set_mv_search_range(MvLimits *mv_limits, const MV *mv);
int vp9_mv_bit_cost(const MV *mv, const MV *ref, const int *mvjcost,
                    int *mvcost[2], int weight);

// Utility to compute variance + MV rate cost for a given MV
int vp9_get_mvpred_var(const MACROBLOCK *x, const MV *best_mv,
                       const MV *center_mv, const vp9_variance_fn_ptr_t *vfp,
                       int use_mvcost);
int vp9_get_mvpred_av_var(const MACROBLOCK *x, const MV *best_mv,
                          const MV *center_mv, const uint8_t *second_pred,
                          const vp9_variance_fn_ptr_t *vfp, int use_mvcost);

struct VP9_COMP;
struct SPEED_FEATURES;

int vp9_init_search_range(int size);

int vp9_refining_search_sad(const struct macroblock *x, struct mv *ref_mv,
                            int sad_per_bit, int distance,
                            const struct vp9_variance_vtable *fn_ptr,
                            const struct mv *center_mv);

// Perform integral projection based motion estimation.
unsigned int vp9_int_pro_motion_estimation(const struct VP9_COMP *cpi,
                                           MACROBLOCK *x, BLOCK_SIZE bsize,
                                           int mi_row, int mi_col);

typedef uint32_t(fractional_mv_step_fp)(
    const MACROBLOCK *x, MV *bestmv, const MV *ref_mv, int allow_hp,
    int error_per_bit, const vp9_variance_fn_ptr_t *vfp,
    int forced_stop,  // 0 - full, 1 - qtr only, 2 - half only
    int iters_per_step, int *cost_list, int *mvjcost, int *mvcost[2],
    uint32_t *distortion, uint32_t *sse1, const uint8_t *second_pred, int w,
    int h);

extern fractional_mv_step_fp vp9_find_best_sub_pixel_tree;
extern fractional_mv_step_fp vp9_find_best_sub_pixel_tree_pruned;
extern fractional_mv_step_fp vp9_find_best_sub_pixel_tree_pruned_more;
extern fractional_mv_step_fp vp9_find_best_sub_pixel_tree_pruned_evenmore;
extern fractional_mv_step_fp vp9_skip_sub_pixel_tree;
extern fractional_mv_step_fp vp9_return_max_sub_pixel_mv;
extern fractional_mv_step_fp vp9_return_min_sub_pixel_mv;

typedef int (*vp9_full_search_fn_t)(const MACROBLOCK *x, const MV *ref_mv,
                                    int sad_per_bit, int distance,
                                    const vp9_variance_fn_ptr_t *fn_ptr,
                                    const MV *center_mv, MV *best_mv);

typedef int (*vp9_refining_search_fn_t)(const MACROBLOCK *x, MV *ref_mv,
                                        int sad_per_bit, int distance,
                                        const vp9_variance_fn_ptr_t *fn_ptr,
                                        const MV *center_mv);

typedef int (*vp9_diamond_search_fn_t)(
    const MACROBLOCK *x, const search_site_config *cfg, MV *ref_mv, MV *best_mv,
    int search_param, int sad_per_bit, int *num00,
    const vp9_variance_fn_ptr_t *fn_ptr, const MV *center_mv);

int vp9_refining_search_8p_c(const MACROBLOCK *x, MV *ref_mv, int error_per_bit,
                             int search_range,
                             const vp9_variance_fn_ptr_t *fn_ptr,
                             const MV *center_mv, const uint8_t *second_pred);

struct VP9_COMP;

int vp9_full_pixel_search(struct VP9_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                          MV *mvp_full, int step_param, int search_method,
                          int error_per_bit, int *cost_list, const MV *ref_mv,
                          MV *tmp_mv, int var_max, int rd);

void vp9_set_subpel_mv_search_range(MvLimits *subpel_mv_limits,
                                    const MvLimits *umv_window_limits,
                                    const MV *ref_mv);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_MCOMP_H_
