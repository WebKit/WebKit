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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/common.h"
#include "av1/common/filter.h"
#include "av1/common/mvref_common.h"
#include "av1/common/reconinter.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/reconinter_enc.h"

static INLINE void init_mv_cost_params(MV_COST_PARAMS *mv_cost_params,
                                       const MvCosts *mv_costs,
                                       const MV *ref_mv, int errorperbit,
                                       int sadperbit) {
  mv_cost_params->ref_mv = ref_mv;
  mv_cost_params->full_ref_mv = get_fullmv_from_mv(ref_mv);
  mv_cost_params->mv_cost_type = MV_COST_ENTROPY;
  mv_cost_params->error_per_bit = errorperbit;
  mv_cost_params->sad_per_bit = sadperbit;
  // For allintra encoding mode, 'mv_costs' is not allocated. Hence, the
  // population of mvjcost and mvcost are avoided. In case of IntraBC, these
  // values are populated from 'dv_costs' in av1_set_ms_to_intra_mode().
  if (mv_costs != NULL) {
    mv_cost_params->mvjcost = mv_costs->nmv_joint_cost;
    mv_cost_params->mvcost[0] = mv_costs->mv_cost_stack[0];
    mv_cost_params->mvcost[1] = mv_costs->mv_cost_stack[1];
  }
}

static INLINE void init_ms_buffers(MSBuffers *ms_buffers, const MACROBLOCK *x) {
  ms_buffers->ref = &x->e_mbd.plane[0].pre[0];
  ms_buffers->src = &x->plane[0].src;

  av1_set_ms_compound_refs(ms_buffers, NULL, NULL, 0, 0);

  ms_buffers->wsrc = x->obmc_buffer.wsrc;
  ms_buffers->obmc_mask = x->obmc_buffer.mask;
}

static AOM_INLINE SEARCH_METHODS
get_faster_search_method(SEARCH_METHODS search_method) {
  // Note on search method's accuracy:
  //  1. NSTEP
  //  2. DIAMOND
  //  3. BIGDIA \approx SQUARE
  //  4. HEX.
  //  5. FAST_HEX \approx FAST_DIAMOND
  switch (search_method) {
    case NSTEP: return DIAMOND;
    case NSTEP_8PT: return DIAMOND;
    case DIAMOND: return BIGDIA;
    case CLAMPED_DIAMOND: return BIGDIA;
    case BIGDIA: return HEX;
    case SQUARE: return HEX;
    case HEX: return FAST_HEX;
    case FAST_HEX: return FAST_HEX;
    case FAST_DIAMOND: return FAST_DIAMOND;
    case FAST_BIGDIA: return FAST_BIGDIA;
    default: assert(0 && "Invalid search method!"); return DIAMOND;
  }
}

void av1_init_obmc_buffer(OBMCBuffer *obmc_buffer) {
  obmc_buffer->wsrc = NULL;
  obmc_buffer->mask = NULL;
  obmc_buffer->above_pred = NULL;
  obmc_buffer->left_pred = NULL;
}

void av1_make_default_fullpel_ms_params(
    FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    const MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv,
    const search_site_config search_sites[NUM_DISTINCT_SEARCH_METHODS],
    int fine_search_interval) {
  const MV_SPEED_FEATURES *mv_sf = &cpi->sf.mv_sf;

  // High level params
  ms_params->bsize = bsize;
  ms_params->vfp = &cpi->ppi->fn_ptr[bsize];

  init_ms_buffers(&ms_params->ms_buffers, x);

  SEARCH_METHODS search_method = mv_sf->search_method;
  if (mv_sf->use_bsize_dependent_search_method) {
    const int min_dim = AOMMIN(block_size_wide[bsize], block_size_high[bsize]);
    if (min_dim >= 32) {
      search_method = get_faster_search_method(search_method);
    }
  }

  av1_set_mv_search_method(ms_params, search_sites, search_method);

  const int use_downsampled_sad =
      mv_sf->use_downsampled_sad && block_size_high[bsize] >= 16;
  if (use_downsampled_sad) {
    ms_params->sdf = ms_params->vfp->sdsf;
    ms_params->sdx4df = ms_params->vfp->sdsx4df;
  } else {
    ms_params->sdf = ms_params->vfp->sdf;
    ms_params->sdx4df = ms_params->vfp->sdx4df;
  }

  ms_params->mesh_patterns[0] = mv_sf->mesh_patterns;
  ms_params->mesh_patterns[1] = mv_sf->intrabc_mesh_patterns;
  ms_params->force_mesh_thresh = mv_sf->exhaustive_searches_thresh;
  ms_params->prune_mesh_search =
      (cpi->sf.mv_sf.prune_mesh_search == PRUNE_MESH_SEARCH_LVL_2) ? 1 : 0;
  ms_params->mesh_search_mv_diff_threshold = 4;
  ms_params->run_mesh_search = 0;
  ms_params->fine_search_interval = fine_search_interval;

  ms_params->is_intra_mode = 0;

  ms_params->fast_obmc_search = mv_sf->obmc_full_pixel_search_level;

  ms_params->mv_limits = x->mv_limits;
  av1_set_mv_search_range(&ms_params->mv_limits, ref_mv);

  // Mvcost params
  init_mv_cost_params(&ms_params->mv_cost_params, x->mv_costs, ref_mv,
                      x->errorperbit, x->sadperbit);
}

void av1_set_ms_to_intra_mode(FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                              const IntraBCMVCosts *dv_costs) {
  ms_params->is_intra_mode = 1;

  MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;

  mv_cost_params->mvjcost = dv_costs->joint_mv;
  mv_cost_params->mvcost[0] = dv_costs->dv_costs[0];
  mv_cost_params->mvcost[1] = dv_costs->dv_costs[1];
}

void av1_make_default_subpel_ms_params(SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                       const struct AV1_COMP *cpi,
                                       const MACROBLOCK *x, BLOCK_SIZE bsize,
                                       const MV *ref_mv, const int *cost_list) {
  const AV1_COMMON *cm = &cpi->common;
  // High level params
  ms_params->allow_hp = cm->features.allow_high_precision_mv;
  ms_params->forced_stop = cpi->sf.mv_sf.subpel_force_stop;
  ms_params->iters_per_step = cpi->sf.mv_sf.subpel_iters_per_step;
  ms_params->cost_list = cond_cost_list_const(cpi, cost_list);

  av1_set_subpel_mv_search_range(&ms_params->mv_limits, &x->mv_limits, ref_mv);

  // Mvcost params
  init_mv_cost_params(&ms_params->mv_cost_params, x->mv_costs, ref_mv,
                      x->errorperbit, x->sadperbit);

  // Subpel variance params
  ms_params->var_params.vfp = &cpi->ppi->fn_ptr[bsize];
  ms_params->var_params.subpel_search_type =
      cpi->sf.mv_sf.use_accurate_subpel_search;
  ms_params->var_params.w = block_size_wide[bsize];
  ms_params->var_params.h = block_size_high[bsize];

  // Ref and src buffers
  MSBuffers *ms_buffers = &ms_params->var_params.ms_buffers;
  init_ms_buffers(ms_buffers, x);
}

static INLINE int get_offset_from_fullmv(const FULLPEL_MV *mv, int stride) {
  return mv->row * stride + mv->col;
}

static INLINE const uint8_t *get_buf_from_fullmv(const struct buf_2d *buf,
                                                 const FULLPEL_MV *mv) {
  return &buf->buf[get_offset_from_fullmv(mv, buf->stride)];
}

void av1_set_mv_search_range(FullMvLimits *mv_limits, const MV *mv) {
  int col_min =
      GET_MV_RAWPEL(mv->col) - MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0);
  int row_min =
      GET_MV_RAWPEL(mv->row) - MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0);
  int col_max = GET_MV_RAWPEL(mv->col) + MAX_FULL_PEL_VAL;
  int row_max = GET_MV_RAWPEL(mv->row) + MAX_FULL_PEL_VAL;

  col_min = AOMMAX(col_min, GET_MV_RAWPEL(MV_LOW) + 1);
  row_min = AOMMAX(row_min, GET_MV_RAWPEL(MV_LOW) + 1);
  col_max = AOMMIN(col_max, GET_MV_RAWPEL(MV_UPP) - 1);
  row_max = AOMMIN(row_max, GET_MV_RAWPEL(MV_UPP) - 1);

  // Get intersection of UMV window and valid MV window to reduce # of checks
  // in diamond search.
  if (mv_limits->col_min < col_min) mv_limits->col_min = col_min;
  if (mv_limits->col_max > col_max) mv_limits->col_max = col_max;
  if (mv_limits->row_min < row_min) mv_limits->row_min = row_min;
  if (mv_limits->row_max > row_max) mv_limits->row_max = row_max;
}

int av1_init_search_range(int size) {
  int sr = 0;
  // Minimum search size no matter what the passed in value.
  size = AOMMAX(16, size);

  while ((size << sr) < MAX_FULL_PEL_VAL) sr++;

  sr = AOMMIN(sr, MAX_MVSEARCH_STEPS - 2);
  return sr;
}

// ============================================================================
//  Cost of motion vectors
// ============================================================================
// TODO(any): Adaptively adjust the regularization strength based on image size
// and motion activity instead of using hard-coded values. It seems like we
// roughly half the lambda for each increase in resolution
// These are multiplier used to perform regularization in motion compensation
// when x->mv_cost_type is set to MV_COST_L1.
// LOWRES
#define SSE_LAMBDA_LOWRES 2   // Used by mv_cost_err_fn
#define SAD_LAMBDA_LOWRES 32  // Used by mvsad_err_cost during full pixel search
// MIDRES
#define SSE_LAMBDA_MIDRES 0   // Used by mv_cost_err_fn
#define SAD_LAMBDA_MIDRES 15  // Used by mvsad_err_cost during full pixel search
// HDRES
#define SSE_LAMBDA_HDRES 1  // Used by mv_cost_err_fn
#define SAD_LAMBDA_HDRES 8  // Used by mvsad_err_cost during full pixel search

// Returns the rate of encoding the current motion vector based on the
// joint_cost and comp_cost. joint_costs covers the cost of transmitting
// JOINT_MV, and comp_cost covers the cost of transmitting the actual motion
// vector.
static INLINE int mv_cost(const MV *mv, const int *joint_cost,
                          const int *const comp_cost[2]) {
  return joint_cost[av1_get_mv_joint(mv)] + comp_cost[0][mv->row] +
         comp_cost[1][mv->col];
}

#define CONVERT_TO_CONST_MVCOST(ptr) ((const int *const *)(ptr))
// Returns the cost of encoding the motion vector diff := *mv - *ref. The cost
// is defined as the rate required to encode diff * weight, rounded to the
// nearest 2 ** 7.
// This is NOT used during motion compensation.
int av1_mv_bit_cost(const MV *mv, const MV *ref_mv, const int *mvjcost,
                    int *const mvcost[2], int weight) {
  const MV diff = { mv->row - ref_mv->row, mv->col - ref_mv->col };
  return ROUND_POWER_OF_TWO(
      mv_cost(&diff, mvjcost, CONVERT_TO_CONST_MVCOST(mvcost)) * weight, 7);
}

// Returns the cost of using the current mv during the motion search. This is
// used when var is used as the error metric.
#define PIXEL_TRANSFORM_ERROR_SCALE 4
static INLINE int mv_err_cost(const MV *mv, const MV *ref_mv,
                              const int *mvjcost, const int *const mvcost[2],
                              int error_per_bit, MV_COST_TYPE mv_cost_type) {
  const MV diff = { mv->row - ref_mv->row, mv->col - ref_mv->col };
  const MV abs_diff = { abs(diff.row), abs(diff.col) };

  switch (mv_cost_type) {
    case MV_COST_ENTROPY:
      if (mvcost) {
        return (int)ROUND_POWER_OF_TWO_64(
            (int64_t)mv_cost(&diff, mvjcost, mvcost) * error_per_bit,
            RDDIV_BITS + AV1_PROB_COST_SHIFT - RD_EPB_SHIFT +
                PIXEL_TRANSFORM_ERROR_SCALE);
      }
      return 0;
    case MV_COST_L1_LOWRES:
      return (SSE_LAMBDA_LOWRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_L1_MIDRES:
      return (SSE_LAMBDA_MIDRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_L1_HDRES:
      return (SSE_LAMBDA_HDRES * (abs_diff.row + abs_diff.col)) >> 3;
    case MV_COST_NONE: return 0;
    default: assert(0 && "Invalid rd_cost_type"); return 0;
  }
}

static INLINE int mv_err_cost_(const MV *mv,
                               const MV_COST_PARAMS *mv_cost_params) {
  if (mv_cost_params->mv_cost_type == MV_COST_NONE) {
    return 0;
  }
  return mv_err_cost(mv, mv_cost_params->ref_mv, mv_cost_params->mvjcost,
                     mv_cost_params->mvcost, mv_cost_params->error_per_bit,
                     mv_cost_params->mv_cost_type);
}

// Returns the cost of using the current mv during the motion search. This is
// only used during full pixel motion search when sad is used as the error
// metric
static INLINE int mvsad_err_cost(const FULLPEL_MV *mv, const FULLPEL_MV *ref_mv,
                                 const int *mvjcost, const int *const mvcost[2],
                                 int sad_per_bit, MV_COST_TYPE mv_cost_type) {
  const MV diff = { GET_MV_SUBPEL(mv->row - ref_mv->row),
                    GET_MV_SUBPEL(mv->col - ref_mv->col) };

  switch (mv_cost_type) {
    case MV_COST_ENTROPY:
      return ROUND_POWER_OF_TWO(
          (unsigned)mv_cost(&diff, mvjcost, CONVERT_TO_CONST_MVCOST(mvcost)) *
              sad_per_bit,
          AV1_PROB_COST_SHIFT);
    case MV_COST_L1_LOWRES:
      return (SAD_LAMBDA_LOWRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_L1_MIDRES:
      return (SAD_LAMBDA_MIDRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_L1_HDRES:
      return (SAD_LAMBDA_HDRES * (abs(diff.row) + abs(diff.col))) >> 3;
    case MV_COST_NONE: return 0;
    default: assert(0 && "Invalid rd_cost_type"); return 0;
  }
}

static INLINE int mvsad_err_cost_(const FULLPEL_MV *mv,
                                  const MV_COST_PARAMS *mv_cost_params) {
  return mvsad_err_cost(mv, &mv_cost_params->full_ref_mv,
                        mv_cost_params->mvjcost, mv_cost_params->mvcost,
                        mv_cost_params->sad_per_bit,
                        mv_cost_params->mv_cost_type);
}

// =============================================================================
//  Fullpixel Motion Search: Translational
// =============================================================================
#define MAX_PATTERN_SCALES 11
#define MAX_PATTERN_CANDIDATES 8  // max number of candidates per scale
#define PATTERN_CANDIDATES_REF 3  // number of refinement candidates

// Search site initialization for DIAMOND / CLAMPED_DIAMOND search methods.
// level = 0: DIAMOND, level = 1: CLAMPED_DIAMOND.
void av1_init_dsmotion_compensation(search_site_config *cfg, int stride,
                                    int level) {
  int num_search_steps = 0;
  int stage_index = MAX_MVSEARCH_STEPS - 1;

  cfg->site[stage_index][0].mv.col = cfg->site[stage_index][0].mv.row = 0;
  cfg->site[stage_index][0].offset = 0;
  cfg->stride = stride;

  // Choose the initial step size depending on level.
  const int first_step = (level > 0) ? (MAX_FIRST_STEP / 4) : MAX_FIRST_STEP;

  for (int radius = first_step; radius > 0;) {
    int num_search_pts = 8;

    const FULLPEL_MV search_site_mvs[13] = {
      { 0, 0 },           { -radius, 0 },      { radius, 0 },
      { 0, -radius },     { 0, radius },       { -radius, -radius },
      { radius, radius }, { -radius, radius }, { radius, -radius },
    };

    int i;
    for (i = 0; i <= num_search_pts; ++i) {
      search_site *const site = &cfg->site[stage_index][i];
      site->mv = search_site_mvs[i];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    // Update the search radius based on level.
    if (!level || ((stage_index < 9) && level)) radius /= 2;
    --stage_index;
    ++num_search_steps;
  }
  cfg->num_search_steps = num_search_steps;
}

void av1_init_motion_fpf(search_site_config *cfg, int stride) {
  int num_search_steps = 0;
  int stage_index = MAX_MVSEARCH_STEPS - 1;

  cfg->site[stage_index][0].mv.col = cfg->site[stage_index][0].mv.row = 0;
  cfg->site[stage_index][0].offset = 0;
  cfg->stride = stride;

  for (int radius = MAX_FIRST_STEP; radius > 0; radius /= 2) {
    // Generate offsets for 8 search sites per step.
    int tan_radius = AOMMAX((int)(0.41 * radius), 1);
    int num_search_pts = 12;
    if (radius == 1) num_search_pts = 8;

    const FULLPEL_MV search_site_mvs[13] = {
      { 0, 0 },
      { -radius, 0 },
      { radius, 0 },
      { 0, -radius },
      { 0, radius },
      { -radius, -tan_radius },
      { radius, tan_radius },
      { -tan_radius, radius },
      { tan_radius, -radius },
      { -radius, tan_radius },
      { radius, -tan_radius },
      { tan_radius, radius },
      { -tan_radius, -radius },
    };

    int i;
    for (i = 0; i <= num_search_pts; ++i) {
      search_site *const site = &cfg->site[stage_index][i];
      site->mv = search_site_mvs[i];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    --stage_index;
    ++num_search_steps;
  }
  cfg->num_search_steps = num_search_steps;
}

// Search site initialization for NSTEP / NSTEP_8PT search methods.
// level = 0: NSTEP, level = 1: NSTEP_8PT.
void av1_init_motion_compensation_nstep(search_site_config *cfg, int stride,
                                        int level) {
  int num_search_steps = 0;
  int stage_index = 0;
  cfg->stride = stride;
  int radius = 1;
  const int num_stages = (level > 0) ? 16 : 15;
  for (stage_index = 0; stage_index < num_stages; ++stage_index) {
    int tan_radius = AOMMAX((int)(0.41 * radius), 1);
    int num_search_pts = 12;
    if ((radius <= 5) || (level > 0)) {
      tan_radius = radius;
      num_search_pts = 8;
    }
    const FULLPEL_MV search_site_mvs[13] = {
      { 0, 0 },
      { -radius, 0 },
      { radius, 0 },
      { 0, -radius },
      { 0, radius },
      { -radius, -tan_radius },
      { radius, tan_radius },
      { -tan_radius, radius },
      { tan_radius, -radius },
      { -radius, tan_radius },
      { radius, -tan_radius },
      { tan_radius, radius },
      { -tan_radius, -radius },
    };

    for (int i = 0; i <= num_search_pts; ++i) {
      search_site *const site = &cfg->site[stage_index][i];
      site->mv = search_site_mvs[i];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    cfg->searches_per_step[stage_index] = num_search_pts;
    cfg->radius[stage_index] = radius;
    ++num_search_steps;
    if (stage_index < 12)
      radius = (int)AOMMAX((radius * 1.5 + 0.5), radius + 1);
  }
  cfg->num_search_steps = num_search_steps;
}

// Search site initialization for BIGDIA / FAST_BIGDIA / FAST_DIAMOND
// search methods.
void av1_init_motion_compensation_bigdia(search_site_config *cfg, int stride,
                                         int level) {
  (void)level;
  cfg->stride = stride;
  // First scale has 4-closest points, the rest have 8 points in diamond
  // shape at increasing scales
  static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = {
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };

  // BIGDIA search method candidates.
  // Note that the largest candidate step at each scale is 2^scale
  /* clang-format off */
  static const FULLPEL_MV
      site_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
          { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, 0 }, { 0, 0 },
            { 0, 0 }, { 0, 0 } },
          { { -1, -1 }, { 0, -2 }, { 1, -1 }, { 2, 0 }, { 1, 1 }, { 0, 2 },
            { -1, 1 }, { -2, 0 } },
          { { -2, -2 }, { 0, -4 }, { 2, -2 }, { 4, 0 }, { 2, 2 }, { 0, 4 },
            { -2, 2 }, { -4, 0 } },
          { { -4, -4 }, { 0, -8 }, { 4, -4 }, { 8, 0 }, { 4, 4 }, { 0, 8 },
            { -4, 4 }, { -8, 0 } },
          { { -8, -8 }, { 0, -16 }, { 8, -8 }, { 16, 0 }, { 8, 8 }, { 0, 16 },
            { -8, 8 }, { -16, 0 } },
          { { -16, -16 }, { 0, -32 }, { 16, -16 }, { 32, 0 }, { 16, 16 },
            { 0, 32 }, { -16, 16 }, { -32, 0 } },
          { { -32, -32 }, { 0, -64 }, { 32, -32 }, { 64, 0 }, { 32, 32 },
            { 0, 64 }, { -32, 32 }, { -64, 0 } },
          { { -64, -64 }, { 0, -128 }, { 64, -64 }, { 128, 0 }, { 64, 64 },
            { 0, 128 }, { -64, 64 }, { -128, 0 } },
          { { -128, -128 }, { 0, -256 }, { 128, -128 }, { 256, 0 },
            { 128, 128 }, { 0, 256 }, { -128, 128 }, { -256, 0 } },
          { { -256, -256 }, { 0, -512 }, { 256, -256 }, { 512, 0 },
            { 256, 256 }, { 0, 512 }, { -256, 256 }, { -512, 0 } },
          { { -512, -512 }, { 0, -1024 }, { 512, -512 }, { 1024, 0 },
            { 512, 512 }, { 0, 1024 }, { -512, 512 }, { -1024, 0 } },
        };

  /* clang-format on */
  int radius = 1;
  for (int i = 0; i < MAX_PATTERN_SCALES; ++i) {
    cfg->searches_per_step[i] = bigdia_num_candidates[i];
    cfg->radius[i] = radius;
    for (int j = 0; j < MAX_PATTERN_CANDIDATES; ++j) {
      search_site *const site = &cfg->site[i][j];
      site->mv = site_candidates[i][j];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    radius *= 2;
  }
  cfg->num_search_steps = MAX_PATTERN_SCALES;
}

// Search site initialization for SQUARE search method.
void av1_init_motion_compensation_square(search_site_config *cfg, int stride,
                                         int level) {
  (void)level;
  cfg->stride = stride;
  // All scales have 8 closest points in square shape.
  static const int square_num_candidates[MAX_PATTERN_SCALES] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };

  // Square search method candidates.
  // Note that the largest candidate step at each scale is 2^scale.
  /* clang-format off */
    static const FULLPEL_MV
        square_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
             { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
               { -1, 1 }, { -1, 0 } },
             { { -2, -2 }, { 0, -2 }, { 2, -2 }, { 2, 0 }, { 2, 2 }, { 0, 2 },
               { -2, 2 }, { -2, 0 } },
             { { -4, -4 }, { 0, -4 }, { 4, -4 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
               { -4, 4 }, { -4, 0 } },
             { { -8, -8 }, { 0, -8 }, { 8, -8 }, { 8, 0 }, { 8, 8 }, { 0, 8 },
               { -8, 8 }, { -8, 0 } },
             { { -16, -16 }, { 0, -16 }, { 16, -16 }, { 16, 0 }, { 16, 16 },
               { 0, 16 }, { -16, 16 }, { -16, 0 } },
             { { -32, -32 }, { 0, -32 }, { 32, -32 }, { 32, 0 }, { 32, 32 },
               { 0, 32 }, { -32, 32 }, { -32, 0 } },
             { { -64, -64 }, { 0, -64 }, { 64, -64 }, { 64, 0 }, { 64, 64 },
               { 0, 64 }, { -64, 64 }, { -64, 0 } },
             { { -128, -128 }, { 0, -128 }, { 128, -128 }, { 128, 0 },
               { 128, 128 }, { 0, 128 }, { -128, 128 }, { -128, 0 } },
             { { -256, -256 }, { 0, -256 }, { 256, -256 }, { 256, 0 },
               { 256, 256 }, { 0, 256 }, { -256, 256 }, { -256, 0 } },
             { { -512, -512 }, { 0, -512 }, { 512, -512 }, { 512, 0 },
               { 512, 512 }, { 0, 512 }, { -512, 512 }, { -512, 0 } },
             { { -1024, -1024 }, { 0, -1024 }, { 1024, -1024 }, { 1024, 0 },
               { 1024, 1024 }, { 0, 1024 }, { -1024, 1024 }, { -1024, 0 } },
    };

  /* clang-format on */
  int radius = 1;
  for (int i = 0; i < MAX_PATTERN_SCALES; ++i) {
    cfg->searches_per_step[i] = square_num_candidates[i];
    cfg->radius[i] = radius;
    for (int j = 0; j < MAX_PATTERN_CANDIDATES; ++j) {
      search_site *const site = &cfg->site[i][j];
      site->mv = square_candidates[i][j];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    radius *= 2;
  }
  cfg->num_search_steps = MAX_PATTERN_SCALES;
}

// Search site initialization for HEX / FAST_HEX search methods.
void av1_init_motion_compensation_hex(search_site_config *cfg, int stride,
                                      int level) {
  (void)level;
  cfg->stride = stride;
  // First scale has 8-closest points, the rest have 6 points in hex shape
  // at increasing scales.
  static const int hex_num_candidates[MAX_PATTERN_SCALES] = { 8, 6, 6, 6, 6, 6,
                                                              6, 6, 6, 6, 6 };
  // Note that the largest candidate step at each scale is 2^scale.
  /* clang-format off */
    static const FULLPEL_MV
        hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
        { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 },
          { -1, 1 }, { -1, 0 } },
        { { -1, -2 }, { 1, -2 }, { 2, 0 }, { 1, 2 }, { -1, 2 }, { -2, 0 } },
        { { -2, -4 }, { 2, -4 }, { 4, 0 }, { 2, 4 }, { -2, 4 }, { -4, 0 } },
        { { -4, -8 }, { 4, -8 }, { 8, 0 }, { 4, 8 }, { -4, 8 }, { -8, 0 } },
        { { -8, -16 }, { 8, -16 }, { 16, 0 }, { 8, 16 },
          { -8, 16 }, { -16, 0 } },
        { { -16, -32 }, { 16, -32 }, { 32, 0 }, { 16, 32 }, { -16, 32 },
          { -32, 0 } },
        { { -32, -64 }, { 32, -64 }, { 64, 0 }, { 32, 64 }, { -32, 64 },
          { -64, 0 } },
        { { -64, -128 }, { 64, -128 }, { 128, 0 }, { 64, 128 },
          { -64, 128 }, { -128, 0 } },
        { { -128, -256 }, { 128, -256 }, { 256, 0 }, { 128, 256 },
          { -128, 256 }, { -256, 0 } },
        { { -256, -512 }, { 256, -512 }, { 512, 0 }, { 256, 512 },
          { -256, 512 }, { -512, 0 } },
        { { -512, -1024 }, { 512, -1024 }, { 1024, 0 }, { 512, 1024 },
          { -512, 1024 }, { -1024, 0 } },
    };

  /* clang-format on */
  int radius = 1;
  for (int i = 0; i < MAX_PATTERN_SCALES; ++i) {
    cfg->searches_per_step[i] = hex_num_candidates[i];
    cfg->radius[i] = radius;
    for (int j = 0; j < hex_num_candidates[i]; ++j) {
      search_site *const site = &cfg->site[i][j];
      site->mv = hex_candidates[i][j];
      site->offset = get_offset_from_fullmv(&site->mv, stride);
    }
    radius *= 2;
  }
  cfg->num_search_steps = MAX_PATTERN_SCALES;
}

// Checks whether the mv is within range of the mv_limits
static INLINE int check_bounds(const FullMvLimits *mv_limits, int row, int col,
                               int range) {
  return ((row - range) >= mv_limits->row_min) &
         ((row + range) <= mv_limits->row_max) &
         ((col - range) >= mv_limits->col_min) &
         ((col + range) <= mv_limits->col_max);
}

static INLINE int get_mvpred_var_cost(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const FULLPEL_MV *this_mv) {
  const aom_variance_fn_ptr_t *vfp = ms_params->vfp;
  const MV sub_this_mv = get_mv_from_fullmv(this_mv);
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const uint8_t *src_buf = src->buf;
  const int src_stride = src->stride;
  const int ref_stride = ref->stride;

  unsigned unused;
  int bestsme;

  bestsme = vfp->vf(src_buf, src_stride, get_buf_from_fullmv(ref, this_mv),
                    ref_stride, &unused);

  bestsme += mv_err_cost_(&sub_this_mv, &ms_params->mv_cost_params);

  return bestsme;
}

static INLINE int get_mvpred_sad(const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                                 const struct buf_2d *const src,
                                 const uint8_t *const ref_address,
                                 const int ref_stride) {
  const uint8_t *src_buf = src->buf;
  const int src_stride = src->stride;

  return ms_params->sdf(src_buf, src_stride, ref_address, ref_stride);
}

static INLINE int get_mvpred_compound_var_cost(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const FULLPEL_MV *this_mv) {
  const aom_variance_fn_ptr_t *vfp = ms_params->vfp;
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const uint8_t *src_buf = src->buf;
  const int src_stride = src->stride;
  const int ref_stride = ref->stride;

  const uint8_t *mask = ms_params->ms_buffers.mask;
  const uint8_t *second_pred = ms_params->ms_buffers.second_pred;
  const int mask_stride = ms_params->ms_buffers.mask_stride;
  const int invert_mask = ms_params->ms_buffers.inv_mask;
  unsigned unused;
  int bestsme;

  if (mask) {
    bestsme = vfp->msvf(get_buf_from_fullmv(ref, this_mv), ref_stride, 0, 0,
                        src_buf, src_stride, second_pred, mask, mask_stride,
                        invert_mask, &unused);
  } else if (second_pred) {
    bestsme = vfp->svaf(get_buf_from_fullmv(ref, this_mv), ref_stride, 0, 0,
                        src_buf, src_stride, &unused, second_pred);
  } else {
    bestsme = vfp->vf(src_buf, src_stride, get_buf_from_fullmv(ref, this_mv),
                      ref_stride, &unused);
  }

  const MV sub_this_mv = get_mv_from_fullmv(this_mv);
  bestsme += mv_err_cost_(&sub_this_mv, &ms_params->mv_cost_params);

  return bestsme;
}

static INLINE int get_mvpred_compound_sad(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
    const struct buf_2d *const src, const uint8_t *const ref_address,
    const int ref_stride) {
  const aom_variance_fn_ptr_t *vfp = ms_params->vfp;
  const uint8_t *src_buf = src->buf;
  const int src_stride = src->stride;

  const uint8_t *mask = ms_params->ms_buffers.mask;
  const uint8_t *second_pred = ms_params->ms_buffers.second_pred;
  const int mask_stride = ms_params->ms_buffers.mask_stride;
  const int invert_mask = ms_params->ms_buffers.inv_mask;

  if (mask) {
    return vfp->msdf(src_buf, src_stride, ref_address, ref_stride, second_pred,
                     mask, mask_stride, invert_mask);
  } else if (second_pred) {
    return vfp->sdaf(src_buf, src_stride, ref_address, ref_stride, second_pred);
  } else {
    return ms_params->sdf(src_buf, src_stride, ref_address, ref_stride);
  }
}

// Calculates and returns a sad+mvcost list around an integer best pel during
// fullpixel motion search. The resulting list can be used to speed up subpel
// motion search later.
#define USE_SAD_COSTLIST 1

// calc_int_cost_list uses var to populate the costlist, which is more accurate
// than sad but slightly slower.
static AOM_FORCE_INLINE void calc_int_cost_list(
    const FULLPEL_MV best_mv, const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
    int *cost_list) {
  static const FULLPEL_MV neighbors[4] = {
    { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }
  };
  const int br = best_mv.row;
  const int bc = best_mv.col;

  cost_list[0] = get_mvpred_var_cost(ms_params, &best_mv);

  if (check_bounds(&ms_params->mv_limits, br, bc, 1)) {
    for (int i = 0; i < 4; i++) {
      const FULLPEL_MV neighbor_mv = { br + neighbors[i].row,
                                       bc + neighbors[i].col };
      cost_list[i + 1] = get_mvpred_var_cost(ms_params, &neighbor_mv);
    }
  } else {
    for (int i = 0; i < 4; i++) {
      const FULLPEL_MV neighbor_mv = { br + neighbors[i].row,
                                       bc + neighbors[i].col };
      if (!av1_is_fullmv_in_range(&ms_params->mv_limits, neighbor_mv)) {
        cost_list[i + 1] = INT_MAX;
      } else {
        cost_list[i + 1] = get_mvpred_var_cost(ms_params, &neighbor_mv);
      }
    }
  }
}

// calc_int_sad_list uses sad to populate the costlist, which is less accurate
// than var but faster.
static AOM_FORCE_INLINE void calc_int_sad_list(
    const FULLPEL_MV best_mv, const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
    int *cost_list, int costlist_has_sad) {
  static const FULLPEL_MV neighbors[4] = {
    { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }
  };
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const int ref_stride = ref->stride;
  const int br = best_mv.row;
  const int bc = best_mv.col;

  assert(av1_is_fullmv_in_range(&ms_params->mv_limits, best_mv));

  // Refresh the costlist it does not contain valid sad
  if (!costlist_has_sad) {
    cost_list[0] = get_mvpred_sad(
        ms_params, src, get_buf_from_fullmv(ref, &best_mv), ref_stride);

    if (check_bounds(&ms_params->mv_limits, br, bc, 1)) {
      for (int i = 0; i < 4; i++) {
        const FULLPEL_MV this_mv = { br + neighbors[i].row,
                                     bc + neighbors[i].col };
        cost_list[i + 1] = get_mvpred_sad(
            ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
      }
    } else {
      for (int i = 0; i < 4; i++) {
        const FULLPEL_MV this_mv = { br + neighbors[i].row,
                                     bc + neighbors[i].col };
        if (!av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv)) {
          cost_list[i + 1] = INT_MAX;
        } else {
          cost_list[i + 1] = get_mvpred_sad(
              ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
        }
      }
    }
  }

  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  cost_list[0] += mvsad_err_cost_(&best_mv, mv_cost_params);

  for (int idx = 0; idx < 4; idx++) {
    if (cost_list[idx + 1] != INT_MAX) {
      const FULLPEL_MV this_mv = { br + neighbors[idx].row,
                                   bc + neighbors[idx].col };
      cost_list[idx + 1] += mvsad_err_cost_(&this_mv, mv_cost_params);
    }
  }
}

// Computes motion vector cost and adds to the sad cost.
// Then updates the best sad and motion vectors.
// Inputs:
//   this_sad: the sad to be evaluated.
//   mv: the current motion vector.
//   mv_cost_params: a structure containing information to compute mv cost.
//   best_sad: the current best sad.
//   raw_best_sad (optional): the current best sad without calculating mv cost.
//   best_mv: the current best motion vector.
//   second_best_mv (optional): the second best motion vector up to now.
// Modifies:
//   best_sad, raw_best_sad, best_mv, second_best_mv
//   If the current sad is lower than the current best sad.
// Returns:
//   Whether the input sad (mv) is better than the current best.
static int update_mvs_and_sad(const unsigned int this_sad, const FULLPEL_MV *mv,
                              const MV_COST_PARAMS *mv_cost_params,
                              unsigned int *best_sad,
                              unsigned int *raw_best_sad, FULLPEL_MV *best_mv,
                              FULLPEL_MV *second_best_mv) {
  if (this_sad >= *best_sad) return 0;

  // Add the motion vector cost.
  const unsigned int sad = this_sad + mvsad_err_cost_(mv, mv_cost_params);
  if (sad < *best_sad) {
    if (raw_best_sad) *raw_best_sad = this_sad;
    *best_sad = sad;
    if (second_best_mv) *second_best_mv = *best_mv;
    *best_mv = *mv;
    return 1;
  }
  return 0;
}

// Calculate sad4 and update the bestmv information
// in FAST_DIAMOND search method.
static void calc_sad4_update_bestmv(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
    const MV_COST_PARAMS *mv_cost_params, FULLPEL_MV *best_mv,
    FULLPEL_MV *temp_best_mv, unsigned int *bestsad, unsigned int *raw_bestsad,
    int search_step, int *best_site, int cand_start) {
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const search_site *site = ms_params->search_sites->site[search_step];

  unsigned char const *block_offset[4];
  unsigned int sads[4];
  const uint8_t *best_address;
  const uint8_t *src_buf = src->buf;
  const int src_stride = src->stride;
  best_address = get_buf_from_fullmv(ref, temp_best_mv);
  // Loop over number of candidates.
  for (int j = 0; j < 4; j++)
    block_offset[j] = site[cand_start + j].offset + best_address;

  // 4-point sad calculation.
  ms_params->sdx4df(src_buf, src_stride, block_offset, ref->stride, sads);

  for (int j = 0; j < 4; j++) {
    const FULLPEL_MV this_mv = {
      temp_best_mv->row + site[cand_start + j].mv.row,
      temp_best_mv->col + site[cand_start + j].mv.col
    };
    const int found_better_mv = update_mvs_and_sad(
        sads[j], &this_mv, mv_cost_params, bestsad, raw_bestsad, best_mv,
        /*second_best_mv=*/NULL);
    if (found_better_mv) *best_site = cand_start + j;
  }
}

// Calculate sad and update the bestmv information
// in FAST_DIAMOND search method.
static void calc_sad_update_bestmv(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
    const MV_COST_PARAMS *mv_cost_params, FULLPEL_MV *best_mv,
    FULLPEL_MV *temp_best_mv, unsigned int *bestsad, unsigned int *raw_bestsad,
    int search_step, int *best_site, const int num_candidates, int cand_start) {
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const search_site *site = ms_params->search_sites->site[search_step];
  // Loop over number of candidates.
  for (int i = cand_start; i < num_candidates; i++) {
    const FULLPEL_MV this_mv = { temp_best_mv->row + site[i].mv.row,
                                 temp_best_mv->col + site[i].mv.col };
    if (!av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv)) continue;
    int thissad = get_mvpred_sad(
        ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref->stride);
    const int found_better_mv = update_mvs_and_sad(
        thissad, &this_mv, mv_cost_params, bestsad, raw_bestsad, best_mv,
        /*second_best_mv=*/NULL);
    if (found_better_mv) *best_site = i;
  }
}

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of
// candidates as indicated in the num_candidates and candidates arrays
// passed into this function
static int pattern_search(FULLPEL_MV start_mv,
                          const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                          int search_step, const int do_init_search,
                          int *cost_list, FULLPEL_MV *best_mv) {
  static const int search_steps[MAX_MVSEARCH_STEPS] = {
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  };
  int i, s, t;

  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const search_site_config *search_sites = ms_params->search_sites;
  const int *num_candidates = search_sites->searches_per_step;
  const int ref_stride = ref->stride;
  const int last_is_4 = num_candidates[0] == 4;
  int br, bc;
  unsigned int bestsad = UINT_MAX, raw_bestsad = UINT_MAX;
  int thissad;
  int k = -1;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  search_step = AOMMIN(search_step, MAX_MVSEARCH_STEPS - 1);
  assert(search_step >= 0);
  int best_init_s = search_steps[search_step];
  // adjust ref_mv to make sure it is within MV range
  clamp_fullmv(&start_mv, &ms_params->mv_limits);
  br = start_mv.row;
  bc = start_mv.col;
  if (cost_list != NULL) {
    cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] =
        INT_MAX;
  }
  int costlist_has_sad = 0;

  // Work out the start point for the search
  raw_bestsad = get_mvpred_sad(ms_params, src,
                               get_buf_from_fullmv(ref, &start_mv), ref_stride);
  bestsad = raw_bestsad + mvsad_err_cost_(&start_mv, mv_cost_params);

  // Search all possible scales up to the search param around the center point
  // pick the scale of the point that is best as the starting scale of
  // further steps around it.
  if (do_init_search) {
    s = best_init_s;
    best_init_s = -1;
    for (t = 0; t <= s; ++t) {
      int best_site = -1;
      FULLPEL_MV temp_best_mv;
      temp_best_mv.row = br;
      temp_best_mv.col = bc;
      if (check_bounds(&ms_params->mv_limits, br, bc, 1 << t)) {
        // Call 4-point sad for multiples of 4 candidates.
        const int no_of_4_cand_loops = num_candidates[t] >> 2;
        for (i = 0; i < no_of_4_cand_loops; i++) {
          calc_sad4_update_bestmv(ms_params, mv_cost_params, best_mv,
                                  &temp_best_mv, &bestsad, &raw_bestsad, t,
                                  &best_site, i * 4);
        }
        // Rest of the candidates
        const int remaining_cand = num_candidates[t] % 4;
        calc_sad_update_bestmv(ms_params, mv_cost_params, best_mv,
                               &temp_best_mv, &bestsad, &raw_bestsad, t,
                               &best_site, remaining_cand,
                               no_of_4_cand_loops * 4);
      } else {
        calc_sad_update_bestmv(ms_params, mv_cost_params, best_mv,
                               &temp_best_mv, &bestsad, &raw_bestsad, t,
                               &best_site, num_candidates[t], 0);
      }
      if (best_site == -1) {
        continue;
      } else {
        best_init_s = t;
        k = best_site;
      }
    }
    if (best_init_s != -1) {
      br += search_sites->site[best_init_s][k].mv.row;
      bc += search_sites->site[best_init_s][k].mv.col;
    }
  }

  // If the center point is still the best, just skip this and move to
  // the refinement step.
  if (best_init_s != -1) {
    const int last_s = (last_is_4 && cost_list != NULL);
    int best_site = -1;
    s = best_init_s;

    for (; s >= last_s; s--) {
      // No need to search all points the 1st time if initial search was used
      if (!do_init_search || s != best_init_s) {
        FULLPEL_MV temp_best_mv;
        temp_best_mv.row = br;
        temp_best_mv.col = bc;
        if (check_bounds(&ms_params->mv_limits, br, bc, 1 << s)) {
          // Call 4-point sad for multiples of 4 candidates.
          const int no_of_4_cand_loops = num_candidates[s] >> 2;
          for (i = 0; i < no_of_4_cand_loops; i++) {
            calc_sad4_update_bestmv(ms_params, mv_cost_params, best_mv,
                                    &temp_best_mv, &bestsad, &raw_bestsad, s,
                                    &best_site, i * 4);
          }
          // Rest of the candidates
          const int remaining_cand = num_candidates[s] % 4;
          calc_sad_update_bestmv(ms_params, mv_cost_params, best_mv,
                                 &temp_best_mv, &bestsad, &raw_bestsad, s,
                                 &best_site, remaining_cand,
                                 no_of_4_cand_loops * 4);
        } else {
          calc_sad_update_bestmv(ms_params, mv_cost_params, best_mv,
                                 &temp_best_mv, &bestsad, &raw_bestsad, s,
                                 &best_site, num_candidates[s], 0);
        }

        if (best_site == -1) {
          continue;
        } else {
          br += search_sites->site[s][best_site].mv.row;
          bc += search_sites->site[s][best_site].mv.col;
          k = best_site;
        }
      }

      do {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

        if (check_bounds(&ms_params->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + search_sites->site[s][next_chkpts_indices[i]].mv.row,
              bc + search_sites->site[s][next_chkpts_indices[i]].mv.col
            };
            thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + search_sites->site[s][next_chkpts_indices[i]].mv.row,
              bc + search_sites->site[s][next_chkpts_indices[i]].mv.col
            };
            if (!av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv))
              continue;
            thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += search_sites->site[s][k].mv.row;
          bc += search_sites->site[s][k].mv.col;
        }
      } while (best_site != -1);
    }

    // Note: If we enter the if below, then cost_list must be non-NULL.
    if (s == 0) {
      cost_list[0] = raw_bestsad;
      costlist_has_sad = 1;
      if (!do_init_search || s != best_init_s) {
        if (check_bounds(&ms_params->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + search_sites->site[s][i].mv.row,
                                         bc + search_sites->site[s][i].mv.col };
            cost_list[i + 1] = thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        } else {
          for (i = 0; i < num_candidates[s]; i++) {
            const FULLPEL_MV this_mv = { br + search_sites->site[s][i].mv.row,
                                         bc + search_sites->site[s][i].mv.col };
            if (!av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv))
              continue;
            cost_list[i + 1] = thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        }

        if (best_site != -1) {
          br += search_sites->site[s][best_site].mv.row;
          bc += search_sites->site[s][best_site].mv.col;
          k = best_site;
        }
      }
      while (best_site != -1) {
        int next_chkpts_indices[PATTERN_CANDIDATES_REF];
        best_site = -1;
        next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
        next_chkpts_indices[1] = k;
        next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;
        cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
        cost_list[((k + 2) % 4) + 1] = cost_list[0];
        cost_list[0] = raw_bestsad;

        if (check_bounds(&ms_params->mv_limits, br, bc, 1 << s)) {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + search_sites->site[s][next_chkpts_indices[i]].mv.row,
              bc + search_sites->site[s][next_chkpts_indices[i]].mv.col
            };
            cost_list[next_chkpts_indices[i] + 1] = thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        } else {
          for (i = 0; i < PATTERN_CANDIDATES_REF; i++) {
            const FULLPEL_MV this_mv = {
              br + search_sites->site[s][next_chkpts_indices[i]].mv.row,
              bc + search_sites->site[s][next_chkpts_indices[i]].mv.col
            };
            if (!av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv)) {
              cost_list[next_chkpts_indices[i] + 1] = INT_MAX;
              continue;
            }
            cost_list[next_chkpts_indices[i] + 1] = thissad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &this_mv), ref_stride);
            const int found_better_mv =
                update_mvs_and_sad(thissad, &this_mv, mv_cost_params, &bestsad,
                                   &raw_bestsad, best_mv,
                                   /*second_best_mv=*/NULL);
            if (found_better_mv) best_site = i;
          }
        }

        if (best_site != -1) {
          k = next_chkpts_indices[best_site];
          br += search_sites->site[s][k].mv.row;
          bc += search_sites->site[s][k].mv.col;
        }
      }
    }
  }

  best_mv->row = br;
  best_mv->col = bc;

  // Returns the one-away integer pel cost/sad around the best as follows:
  // cost_list[0]: cost/sad at the best integer pel
  // cost_list[1]: cost/sad at delta {0, -1} (left)   from the best integer pel
  // cost_list[2]: cost/sad at delta { 1, 0} (bottom) from the best integer pel
  // cost_list[3]: cost/sad at delta { 0, 1} (right)  from the best integer pel
  // cost_list[4]: cost/sad at delta {-1, 0} (top)    from the best integer pel
  if (cost_list) {
    if (USE_SAD_COSTLIST) {
      calc_int_sad_list(*best_mv, ms_params, cost_list, costlist_has_sad);
    } else {
      calc_int_cost_list(*best_mv, ms_params, cost_list);
    }
  }
  best_mv->row = br;
  best_mv->col = bc;

  const int var_cost = get_mvpred_var_cost(ms_params, best_mv);
  return var_cost;
}

// For the following foo_search, the input arguments are:
// start_mv: where we are starting our motion search
// ms_params: a collection of motion search parameters
// search_step: how many steps to skip in our motion search. For example,
//   a value 3 suggests that 3 search steps have already taken place prior to
//   this function call, so we jump directly to step 4 of the search process
// do_init_search: if on, do an initial search of all possible scales around the
//   start_mv, and then pick the best scale.
// cond_list: used to hold the cost around the best full mv so we can use it to
//   speed up subpel search later.
// best_mv: the best mv found in the motion search
static int hex_search(const FULLPEL_MV start_mv,
                      const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                      const int search_step, const int do_init_search,
                      int *cost_list, FULLPEL_MV *best_mv) {
  return pattern_search(start_mv, ms_params, search_step, do_init_search,
                        cost_list, best_mv);
}

static int bigdia_search(const FULLPEL_MV start_mv,
                         const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                         const int search_step, const int do_init_search,
                         int *cost_list, FULLPEL_MV *best_mv) {
  return pattern_search(start_mv, ms_params, search_step, do_init_search,
                        cost_list, best_mv);
}

static int square_search(const FULLPEL_MV start_mv,
                         const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                         const int search_step, const int do_init_search,
                         int *cost_list, FULLPEL_MV *best_mv) {
  return pattern_search(start_mv, ms_params, search_step, do_init_search,
                        cost_list, best_mv);
}

static int fast_hex_search(const FULLPEL_MV start_mv,
                           const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                           const int search_step, const int do_init_search,
                           int *cost_list, FULLPEL_MV *best_mv) {
  return hex_search(start_mv, ms_params,
                    AOMMAX(MAX_MVSEARCH_STEPS - 2, search_step), do_init_search,
                    cost_list, best_mv);
}

static int fast_dia_search(const FULLPEL_MV start_mv,
                           const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                           const int search_step, const int do_init_search,
                           int *cost_list, FULLPEL_MV *best_mv) {
  return bigdia_search(start_mv, ms_params,
                       AOMMAX(MAX_MVSEARCH_STEPS - 2, search_step),
                       do_init_search, cost_list, best_mv);
}

static int fast_bigdia_search(const FULLPEL_MV start_mv,
                              const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                              const int search_step, const int do_init_search,
                              int *cost_list, FULLPEL_MV *best_mv) {
  return bigdia_search(start_mv, ms_params,
                       AOMMAX(MAX_MVSEARCH_STEPS - 3, search_step),
                       do_init_search, cost_list, best_mv);
}

static int diamond_search_sad(FULLPEL_MV start_mv,
                              const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                              const int search_step, int *num00,
                              FULLPEL_MV *best_mv, FULLPEL_MV *second_best_mv) {
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;

  const int ref_stride = ref->stride;
  const uint8_t *best_address;

  const uint8_t *mask = ms_params->ms_buffers.mask;
  const uint8_t *second_pred = ms_params->ms_buffers.second_pred;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;

  const search_site_config *cfg = ms_params->search_sites;

  unsigned int bestsad = INT_MAX;
  int best_site = 0;
  int is_off_center = 0;

  clamp_fullmv(&start_mv, &ms_params->mv_limits);

  // search_step determines the length of the initial step and hence the number
  // of iterations.
  const int tot_steps = cfg->num_search_steps - search_step;

  *num00 = 0;
  *best_mv = start_mv;

  // Check the starting position
  best_address = get_buf_from_fullmv(ref, &start_mv);
  bestsad = get_mvpred_compound_sad(ms_params, src, best_address, ref_stride);
  bestsad += mvsad_err_cost_(best_mv, &ms_params->mv_cost_params);

  int next_step_size = tot_steps > 2 ? cfg->radius[tot_steps - 2] : 1;
  for (int step = tot_steps - 1; step >= 0; --step) {
    const search_site *site = cfg->site[step];
    best_site = 0;
    if (step > 0) next_step_size = cfg->radius[step - 1];

    int all_in = 1, j;
    // Trap illegal vectors
    all_in &= best_mv->row + site[1].mv.row >= ms_params->mv_limits.row_min;
    all_in &= best_mv->row + site[2].mv.row <= ms_params->mv_limits.row_max;
    all_in &= best_mv->col + site[3].mv.col >= ms_params->mv_limits.col_min;
    all_in &= best_mv->col + site[4].mv.col <= ms_params->mv_limits.col_max;

    // TODO(anyone): Implement 4 points search for msdf&sdaf
    if (all_in && !mask && !second_pred) {
      const uint8_t *src_buf = src->buf;
      const int src_stride = src->stride;
      for (int idx = 1; idx <= cfg->searches_per_step[step]; idx += 4) {
        unsigned char const *block_offset[4];
        unsigned int sads[4];

        for (j = 0; j < 4; j++)
          block_offset[j] = site[idx + j].offset + best_address;

        ms_params->sdx4df(src_buf, src_stride, block_offset, ref_stride, sads);
        for (j = 0; j < 4; j++) {
          if (sads[j] < bestsad) {
            const FULLPEL_MV this_mv = { best_mv->row + site[idx + j].mv.row,
                                         best_mv->col + site[idx + j].mv.col };
            unsigned int thissad =
                sads[j] + mvsad_err_cost_(&this_mv, mv_cost_params);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = idx + j;
            }
          }
        }
      }
    } else {
      for (int idx = 1; idx <= cfg->searches_per_step[step]; idx++) {
        const FULLPEL_MV this_mv = { best_mv->row + site[idx].mv.row,
                                     best_mv->col + site[idx].mv.col };

        if (av1_is_fullmv_in_range(&ms_params->mv_limits, this_mv)) {
          const uint8_t *const check_here = site[idx].offset + best_address;
          unsigned int thissad;

          thissad =
              get_mvpred_compound_sad(ms_params, src, check_here, ref_stride);

          if (thissad < bestsad) {
            thissad += mvsad_err_cost_(&this_mv, mv_cost_params);
            if (thissad < bestsad) {
              bestsad = thissad;
              best_site = idx;
            }
          }
        }
      }
    }

    if (best_site != 0) {
      if (second_best_mv) {
        *second_best_mv = *best_mv;
      }
      best_mv->row += site[best_site].mv.row;
      best_mv->col += site[best_site].mv.col;
      best_address += site[best_site].offset;
      is_off_center = 1;
    }

    if (is_off_center == 0) (*num00)++;

    if (best_site == 0) {
      while (next_step_size == cfg->radius[step] && step > 2) {
        ++(*num00);
        --step;
        next_step_size = cfg->radius[step - 1];
      }
    }
  }

  return bestsad;
}

/* do_refine: If last step (1-away) of n-step search doesn't pick the center
              point as the best match, we will do a final 1-away diamond
              refining search  */
static int full_pixel_diamond(const FULLPEL_MV start_mv,
                              const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                              const int step_param, int *cost_list,
                              FULLPEL_MV *best_mv, FULLPEL_MV *second_best_mv) {
  const search_site_config *cfg = ms_params->search_sites;
  int thissme, n, num00 = 0;
  int bestsme = diamond_search_sad(start_mv, ms_params, step_param, &n, best_mv,
                                   second_best_mv);

  if (bestsme < INT_MAX) {
    bestsme = get_mvpred_compound_var_cost(ms_params, best_mv);
  }

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  const int further_steps = cfg->num_search_steps - 1 - step_param;
  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      // TODO(chiyotsai@google.com): There is another bug here where the second
      // best mv gets incorrectly overwritten. Fix it later.
      FULLPEL_MV tmp_best_mv;
      thissme = diamond_search_sad(start_mv, ms_params, step_param + n, &num00,
                                   &tmp_best_mv, second_best_mv);

      if (thissme < INT_MAX) {
        thissme = get_mvpred_compound_var_cost(ms_params, &tmp_best_mv);
      }

      if (thissme < bestsme) {
        bestsme = thissme;
        *best_mv = tmp_best_mv;
      }
    }
  }

  // Return cost list.
  if (cost_list) {
    if (USE_SAD_COSTLIST) {
      const int costlist_has_sad = 0;
      calc_int_sad_list(*best_mv, ms_params, cost_list, costlist_has_sad);
    } else {
      calc_int_cost_list(*best_mv, ms_params, cost_list);
    }
  }
  return bestsme;
}

// Exhaustive motion search around a given centre position with a given
// step size.
static int exhaustive_mesh_search(FULLPEL_MV start_mv,
                                  const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                                  const int range, const int step,
                                  FULLPEL_MV *best_mv,
                                  FULLPEL_MV *second_best_mv) {
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const struct buf_2d *const src = ms_params->ms_buffers.src;
  const struct buf_2d *const ref = ms_params->ms_buffers.ref;
  const int ref_stride = ref->stride;
  unsigned int best_sad = INT_MAX;
  int r, c, i;
  int start_col, end_col, start_row, end_row;
  const int col_step = (step > 1) ? step : 4;

  assert(step >= 1);

  clamp_fullmv(&start_mv, &ms_params->mv_limits);
  *best_mv = start_mv;
  best_sad = get_mvpred_sad(ms_params, src, get_buf_from_fullmv(ref, &start_mv),
                            ref_stride);
  best_sad += mvsad_err_cost_(&start_mv, mv_cost_params);
  start_row = AOMMAX(-range, ms_params->mv_limits.row_min - start_mv.row);
  start_col = AOMMAX(-range, ms_params->mv_limits.col_min - start_mv.col);
  end_row = AOMMIN(range, ms_params->mv_limits.row_max - start_mv.row);
  end_col = AOMMIN(range, ms_params->mv_limits.col_max - start_mv.col);

  for (r = start_row; r <= end_row; r += step) {
    for (c = start_col; c <= end_col; c += col_step) {
      // Step > 1 means we are not checking every location in this pass.
      if (step > 1) {
        const FULLPEL_MV mv = { start_mv.row + r, start_mv.col + c };
        unsigned int sad = get_mvpred_sad(
            ms_params, src, get_buf_from_fullmv(ref, &mv), ref_stride);
        update_mvs_and_sad(sad, &mv, mv_cost_params, &best_sad,
                           /*raw_best_sad=*/NULL, best_mv, second_best_mv);
      } else {
        // 4 sads in a single call if we are checking every location
        if (c + 3 <= end_col) {
          unsigned int sads[4];
          const uint8_t *addrs[4];
          for (i = 0; i < 4; ++i) {
            const FULLPEL_MV mv = { start_mv.row + r, start_mv.col + c + i };
            addrs[i] = get_buf_from_fullmv(ref, &mv);
          }

          ms_params->sdx4df(src->buf, src->stride, addrs, ref_stride, sads);

          for (i = 0; i < 4; ++i) {
            if (sads[i] < best_sad) {
              const FULLPEL_MV mv = { start_mv.row + r, start_mv.col + c + i };
              update_mvs_and_sad(sads[i], &mv, mv_cost_params, &best_sad,
                                 /*raw_best_sad=*/NULL, best_mv,
                                 second_best_mv);
            }
          }
        } else {
          for (i = 0; i < end_col - c; ++i) {
            const FULLPEL_MV mv = { start_mv.row + r, start_mv.col + c + i };
            unsigned int sad = get_mvpred_sad(
                ms_params, src, get_buf_from_fullmv(ref, &mv), ref_stride);
            update_mvs_and_sad(sad, &mv, mv_cost_params, &best_sad,
                               /*raw_best_sad=*/NULL, best_mv, second_best_mv);
          }
        }
      }
    }
  }

  return best_sad;
}

// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int full_pixel_exhaustive(const FULLPEL_MV start_mv,
                                 const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                                 const struct MESH_PATTERN *const mesh_patterns,
                                 int *cost_list, FULLPEL_MV *best_mv,
                                 FULLPEL_MV *second_best_mv) {
  const int kMinRange = 7;
  const int kMaxRange = 256;
  const int kMinInterval = 1;

  int bestsme;
  int i;
  int interval = mesh_patterns[0].interval;
  int range = mesh_patterns[0].range;
  int baseline_interval_divisor;

  *best_mv = start_mv;

  // Trap illegal values for interval and range for this function.
  if ((range < kMinRange) || (range > kMaxRange) || (interval < kMinInterval) ||
      (interval > range))
    return INT_MAX;

  baseline_interval_divisor = range / interval;

  // Check size of proposed first range against magnitude of the centre
  // value used as a starting point.
  range = AOMMAX(range, (5 * AOMMAX(abs(best_mv->row), abs(best_mv->col))) / 4);
  range = AOMMIN(range, kMaxRange);
  interval = AOMMAX(interval, range / baseline_interval_divisor);
  // Use a small search step/interval for certain kind of clips.
  // For example, screen content clips with a lot of texts.
  // Large interval could lead to a false matching position, and it can't find
  // the best global candidate in following iterations due to reduced search
  // range. The solution here is to use a small search iterval in the beginning
  // and thus reduces the chance of missing the best candidate.
  if (ms_params->fine_search_interval) {
    interval = AOMMIN(interval, 4);
  }

  // initial search
  bestsme = exhaustive_mesh_search(*best_mv, ms_params, range, interval,
                                   best_mv, second_best_mv);

  if ((interval > kMinInterval) && (range > kMinRange)) {
    // Progressive searches with range and step size decreasing each time
    // till we reach a step size of 1. Then break out.
    for (i = 1; i < MAX_MESH_STEP; ++i) {
      // First pass with coarser step and longer range
      bestsme = exhaustive_mesh_search(
          *best_mv, ms_params, mesh_patterns[i].range,
          mesh_patterns[i].interval, best_mv, second_best_mv);

      if (mesh_patterns[i].interval == 1) break;
    }
  }

  if (bestsme < INT_MAX) {
    bestsme = get_mvpred_var_cost(ms_params, best_mv);
  }

  // Return cost list.
  if (cost_list) {
    if (USE_SAD_COSTLIST) {
      const int costlist_has_sad = 0;
      calc_int_sad_list(*best_mv, ms_params, cost_list, costlist_has_sad);
    } else {
      calc_int_cost_list(*best_mv, ms_params, cost_list);
    }
  }
  return bestsme;
}

// This function is called when we do joint motion search in comp_inter_inter
// mode, or when searching for one component of an ext-inter compound mode.
int av1_refining_search_8p_c(const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                             const FULLPEL_MV start_mv, FULLPEL_MV *best_mv) {
  static const search_neighbors neighbors[8] = {
    { { -1, 0 }, -1 * SEARCH_GRID_STRIDE_8P + 0 },
    { { 0, -1 }, 0 * SEARCH_GRID_STRIDE_8P - 1 },
    { { 0, 1 }, 0 * SEARCH_GRID_STRIDE_8P + 1 },
    { { 1, 0 }, 1 * SEARCH_GRID_STRIDE_8P + 0 },
    { { -1, -1 }, -1 * SEARCH_GRID_STRIDE_8P - 1 },
    { { 1, -1 }, 1 * SEARCH_GRID_STRIDE_8P - 1 },
    { { -1, 1 }, -1 * SEARCH_GRID_STRIDE_8P + 1 },
    { { 1, 1 }, 1 * SEARCH_GRID_STRIDE_8P + 1 }
  };

  uint8_t do_refine_search_grid[SEARCH_GRID_STRIDE_8P *
                                SEARCH_GRID_STRIDE_8P] = { 0 };
  int grid_center = SEARCH_GRID_CENTER_8P;
  int grid_coord = grid_center;

  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const FullMvLimits *mv_limits = &ms_params->mv_limits;
  const MSBuffers *ms_buffers = &ms_params->ms_buffers;
  const struct buf_2d *src = ms_buffers->src;
  const struct buf_2d *ref = ms_buffers->ref;
  const int ref_stride = ref->stride;

  *best_mv = start_mv;
  clamp_fullmv(best_mv, mv_limits);

  unsigned int best_sad = get_mvpred_compound_sad(
      ms_params, src, get_buf_from_fullmv(ref, best_mv), ref_stride);
  best_sad += mvsad_err_cost_(best_mv, mv_cost_params);

  do_refine_search_grid[grid_coord] = 1;

  for (int i = 0; i < SEARCH_RANGE_8P; ++i) {
    int best_site = -1;

    for (int j = 0; j < 8; ++j) {
      grid_coord = grid_center + neighbors[j].coord_offset;
      if (do_refine_search_grid[grid_coord] == 1) {
        continue;
      }
      const FULLPEL_MV mv = { best_mv->row + neighbors[j].coord.row,
                              best_mv->col + neighbors[j].coord.col };

      do_refine_search_grid[grid_coord] = 1;
      if (av1_is_fullmv_in_range(mv_limits, mv)) {
        unsigned int sad;
        sad = get_mvpred_compound_sad(
            ms_params, src, get_buf_from_fullmv(ref, &mv), ref_stride);
        if (sad < best_sad) {
          sad += mvsad_err_cost_(&mv, mv_cost_params);

          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      best_mv->row += neighbors[best_site].coord.row;
      best_mv->col += neighbors[best_site].coord.col;
      grid_center += neighbors[best_site].coord_offset;
    }
  }
  return best_sad;
}

int av1_full_pixel_search(const FULLPEL_MV start_mv,
                          const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                          const int step_param, int *cost_list,
                          FULLPEL_MV *best_mv, FULLPEL_MV *second_best_mv) {
  const BLOCK_SIZE bsize = ms_params->bsize;
  const SEARCH_METHODS search_method = ms_params->search_method;

  const int is_intra_mode = ms_params->is_intra_mode;
  int run_mesh_search = ms_params->run_mesh_search;

  int var = 0;
  MARK_MV_INVALID(best_mv);
  if (second_best_mv) {
    MARK_MV_INVALID(second_best_mv);
  }

  if (cost_list) {
    cost_list[0] = INT_MAX;
    cost_list[1] = INT_MAX;
    cost_list[2] = INT_MAX;
    cost_list[3] = INT_MAX;
    cost_list[4] = INT_MAX;
  }

  assert(ms_params->ms_buffers.ref->stride == ms_params->search_sites->stride);

  switch (search_method) {
    case FAST_BIGDIA:
      var = fast_bigdia_search(start_mv, ms_params, step_param, 0, cost_list,
                               best_mv);
      break;
    case FAST_DIAMOND:
      var = fast_dia_search(start_mv, ms_params, step_param, 0, cost_list,
                            best_mv);
      break;
    case FAST_HEX:
      var = fast_hex_search(start_mv, ms_params, step_param, 0, cost_list,
                            best_mv);
      break;
    case HEX:
      var = hex_search(start_mv, ms_params, step_param, 1, cost_list, best_mv);
      break;
    case SQUARE:
      var =
          square_search(start_mv, ms_params, step_param, 1, cost_list, best_mv);
      break;
    case BIGDIA:
      var =
          bigdia_search(start_mv, ms_params, step_param, 1, cost_list, best_mv);
      break;
    case NSTEP:
    case NSTEP_8PT:
    case DIAMOND:
    case CLAMPED_DIAMOND:
      var = full_pixel_diamond(start_mv, ms_params, step_param, cost_list,
                               best_mv, second_best_mv);
      break;
    default: assert(0 && "Invalid search method.");
  }

  // Should we allow a follow on exhaustive search?
  if (!run_mesh_search &&
      ((search_method == NSTEP) || (search_method == NSTEP_8PT))) {
    int exhaustive_thr = ms_params->force_mesh_thresh;
    exhaustive_thr >>=
        10 - (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
    // Threshold variance for an exhaustive full search.
    if (var > exhaustive_thr) run_mesh_search = 1;
  }

  // TODO(yunqing): the following is used to reduce mesh search in temporal
  // filtering. Can extend it to intrabc.
  if (!is_intra_mode && ms_params->prune_mesh_search) {
    const int full_pel_mv_diff = AOMMAX(abs(start_mv.row - best_mv->row),
                                        abs(start_mv.col - best_mv->col));
    if (full_pel_mv_diff <= ms_params->mesh_search_mv_diff_threshold) {
      run_mesh_search = 0;
    }
  }

  if (ms_params->sdf != ms_params->vfp->sdf) {
    // If we are skipping rows when we perform the motion search, we need to
    // check the quality of skipping. If it's bad, then we run mesh search with
    // skip row features off.
    // TODO(chiyotsai@google.com): Handle the case where we have a vertical
    // offset of 1 before we hit this statement to avoid having to redo
    // motion search.
    const struct buf_2d *src = ms_params->ms_buffers.src;
    const struct buf_2d *ref = ms_params->ms_buffers.ref;
    const int src_stride = src->stride;
    const int ref_stride = ref->stride;

    const uint8_t *src_address = src->buf;
    const uint8_t *best_address = get_buf_from_fullmv(ref, best_mv);
    const int sad =
        ms_params->vfp->sdf(src_address, src_stride, best_address, ref_stride);
    const int skip_sad =
        ms_params->vfp->sdsf(src_address, src_stride, best_address, ref_stride);
    // We will keep the result of skipping rows if it's good enough. Here, good
    // enough means the error is less than 1 per pixel.
    const int kSADThresh =
        1 << (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
    if (sad > kSADThresh && abs(skip_sad - sad) * 10 >= AOMMAX(sad, 1) * 9) {
      // There is a large discrepancy between skipping and not skipping, so we
      // need to redo the motion search.
      FULLPEL_MOTION_SEARCH_PARAMS new_ms_params = *ms_params;
      new_ms_params.sdf = new_ms_params.vfp->sdf;
      new_ms_params.sdx4df = new_ms_params.vfp->sdx4df;

      return av1_full_pixel_search(start_mv, &new_ms_params, step_param,
                                   cost_list, best_mv, second_best_mv);
    }
  }

  if (run_mesh_search) {
    int var_ex;
    FULLPEL_MV tmp_mv_ex;
    // Pick the mesh pattern for exhaustive search based on the toolset (intraBC
    // or non-intraBC)
    // TODO(chiyotsai@google.com):  There is a bug here where the second best mv
    // gets overwritten without actually comparing the rdcost.
    const MESH_PATTERN *const mesh_patterns =
        ms_params->mesh_patterns[is_intra_mode];
    // TODO(chiyotsai@google.com): the second best mv is not set correctly by
    // full_pixel_exhaustive, which can incorrectly override it.
    var_ex = full_pixel_exhaustive(*best_mv, ms_params, mesh_patterns,
                                   cost_list, &tmp_mv_ex, second_best_mv);
    if (var_ex < var) {
      var = var_ex;
      *best_mv = tmp_mv_ex;
    }
  }

  return var;
}

int av1_intrabc_hash_search(const AV1_COMP *cpi, const MACROBLOCKD *xd,
                            const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                            IntraBCHashInfo *intrabc_hash_info,
                            FULLPEL_MV *best_mv) {
  if (!av1_use_hash_me(cpi)) return INT_MAX;

  const BLOCK_SIZE bsize = ms_params->bsize;
  const int block_width = block_size_wide[bsize];
  const int block_height = block_size_high[bsize];

  if (block_width != block_height) return INT_MAX;

  const FullMvLimits *mv_limits = &ms_params->mv_limits;
  const MSBuffers *ms_buffer = &ms_params->ms_buffers;

  const uint8_t *src = ms_buffer->src->buf;
  const int src_stride = ms_buffer->src->stride;

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int x_pos = mi_col * MI_SIZE;
  const int y_pos = mi_row * MI_SIZE;

  uint32_t hash_value1, hash_value2;
  int best_hash_cost = INT_MAX;

  // for the hashMap
  hash_table *ref_frame_hash = &intrabc_hash_info->intrabc_hash_table;

  av1_get_block_hash_value(intrabc_hash_info, src, src_stride, block_width,
                           &hash_value1, &hash_value2, is_cur_buf_hbd(xd));

  const int count = av1_hash_table_count(ref_frame_hash, hash_value1);
  if (count <= 1) {
    return INT_MAX;
  }

  Iterator iterator = av1_hash_get_first_iterator(ref_frame_hash, hash_value1);
  for (int i = 0; i < count; i++, aom_iterator_increment(&iterator)) {
    block_hash ref_block_hash = *(block_hash *)(aom_iterator_get(&iterator));
    if (hash_value2 == ref_block_hash.hash_value2) {
      // Make sure the prediction is from valid area.
      const MV dv = { GET_MV_SUBPEL(ref_block_hash.y - y_pos),
                      GET_MV_SUBPEL(ref_block_hash.x - x_pos) };
      if (!av1_is_dv_valid(dv, &cpi->common, xd, mi_row, mi_col, bsize,
                           cpi->common.seq_params->mib_size_log2))
        continue;

      FULLPEL_MV hash_mv;
      hash_mv.col = ref_block_hash.x - x_pos;
      hash_mv.row = ref_block_hash.y - y_pos;
      if (!av1_is_fullmv_in_range(mv_limits, hash_mv)) continue;
      const int refCost = get_mvpred_var_cost(ms_params, &hash_mv);
      if (refCost < best_hash_cost) {
        best_hash_cost = refCost;
        *best_mv = hash_mv;
      }
    }
  }

  return best_hash_cost;
}

static int vector_match(int16_t *ref, int16_t *src, int bwl) {
  int best_sad = INT_MAX;
  int this_sad;
  int d;
  int center, offset = 0;
  int bw = 4 << bwl;  // redundant variable, to be changed in the experiments.
  for (d = 0; d <= bw; d += 16) {
    this_sad = aom_vector_var(&ref[d], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      offset = d;
    }
  }
  center = offset;

  for (d = -8; d <= 8; d += 16) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -4; d <= 4; d += 8) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -2; d <= 2; d += 4) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }
  offset = center;

  for (d = -1; d <= 1; d += 2) {
    int this_pos = offset + d;
    // check limit
    if (this_pos < 0 || this_pos > bw) continue;
    this_sad = aom_vector_var(&ref[this_pos], src, bwl);
    if (this_sad < best_sad) {
      best_sad = this_sad;
      center = this_pos;
    }
  }

  return (center - (bw >> 1));
}

// A special fast version of motion search used in rt mode
unsigned int av1_int_pro_motion_estimation(const AV1_COMP *cpi, MACROBLOCK *x,
                                           BLOCK_SIZE bsize, int mi_row,
                                           int mi_col, const MV *ref_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  DECLARE_ALIGNED(16, int16_t, hbuf[256]);
  DECLARE_ALIGNED(16, int16_t, vbuf[256]);
  DECLARE_ALIGNED(16, int16_t, src_hbuf[128]);
  DECLARE_ALIGNED(16, int16_t, src_vbuf[128]);
  int idx;
  const int bw = 4 << mi_size_wide_log2[bsize];
  const int bh = 4 << mi_size_high_log2[bsize];
  const int search_width = bw << 1;
  const int search_height = bh << 1;
  const int src_stride = x->plane[0].src.stride;
  const int ref_stride = xd->plane[0].pre[0].stride;
  uint8_t const *ref_buf, *src_buf;
  int_mv *best_int_mv = &xd->mi[0]->mv[0];
  unsigned int best_sad, tmp_sad, this_sad[4];
  const int norm_factor = 3 + (bw >> 5);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, mi->ref_frame[0]);
  static const MV search_pos[4] = {
    { -1, 0 },
    { 0, -1 },
    { 0, 1 },
    { 1, 0 },
  };

  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++) backup_yv12[i] = xd->plane[i].pre[0];
    av1_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL,
                         MAX_MB_PLANE);
  }

  if (xd->bd != 8) {
    unsigned int sad;
    best_int_mv->as_fullmv = kZeroFullMv;
    sad = cpi->ppi->fn_ptr[bsize].sdf(x->plane[0].src.buf, src_stride,
                                      xd->plane[0].pre[0].buf, ref_stride);

    if (scaled_ref_frame) {
      int i;
      for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
    }
    return sad;
  }

  // Set up prediction 1-D reference set
  ref_buf = xd->plane[0].pre[0].buf - (bw >> 1);
  for (idx = 0; idx < search_width; idx += 16) {
    aom_int_pro_row(&hbuf[idx], ref_buf, ref_stride, bh);
    ref_buf += 16;
  }

  ref_buf = xd->plane[0].pre[0].buf - (bh >> 1) * ref_stride;
  for (idx = 0; idx < search_height; ++idx) {
    vbuf[idx] = aom_int_pro_col(ref_buf, bw) >> norm_factor;
    ref_buf += ref_stride;
  }

  // Set up src 1-D reference set
  for (idx = 0; idx < bw; idx += 16) {
    src_buf = x->plane[0].src.buf + idx;
    aom_int_pro_row(&src_hbuf[idx], src_buf, src_stride, bh);
  }

  src_buf = x->plane[0].src.buf;
  for (idx = 0; idx < bh; ++idx) {
    src_vbuf[idx] = aom_int_pro_col(src_buf, bw) >> norm_factor;
    src_buf += src_stride;
  }

  // Find the best match per 1-D search
  best_int_mv->as_fullmv.col =
      vector_match(hbuf, src_hbuf, mi_size_wide_log2[bsize]);
  best_int_mv->as_fullmv.row =
      vector_match(vbuf, src_vbuf, mi_size_high_log2[bsize]);

  FULLPEL_MV this_mv = best_int_mv->as_fullmv;
  src_buf = x->plane[0].src.buf;
  ref_buf = get_buf_from_fullmv(&xd->plane[0].pre[0], &this_mv);
  best_sad =
      cpi->ppi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);

  {
    const uint8_t *const pos[4] = {
      ref_buf - ref_stride,
      ref_buf - 1,
      ref_buf + 1,
      ref_buf + ref_stride,
    };

    cpi->ppi->fn_ptr[bsize].sdx4df(src_buf, src_stride, pos, ref_stride,
                                   this_sad);
  }

  for (idx = 0; idx < 4; ++idx) {
    if (this_sad[idx] < best_sad) {
      best_sad = this_sad[idx];
      best_int_mv->as_fullmv.row = search_pos[idx].row + this_mv.row;
      best_int_mv->as_fullmv.col = search_pos[idx].col + this_mv.col;
    }
  }

  if (this_sad[0] < this_sad[3])
    this_mv.row -= 1;
  else
    this_mv.row += 1;

  if (this_sad[1] < this_sad[2])
    this_mv.col -= 1;
  else
    this_mv.col += 1;

  ref_buf = get_buf_from_fullmv(&xd->plane[0].pre[0], &this_mv);

  tmp_sad =
      cpi->ppi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);
  if (best_sad > tmp_sad) {
    best_int_mv->as_fullmv = this_mv;
    best_sad = tmp_sad;
  }

  convert_fullmv_to_mv(best_int_mv);

  SubpelMvLimits subpel_mv_limits;
  av1_set_subpel_mv_search_range(&subpel_mv_limits, &x->mv_limits, ref_mv);
  clamp_mv(&best_int_mv->as_mv, &subpel_mv_limits);

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++) xd->plane[i].pre[0] = backup_yv12[i];
  }

  return best_sad;
}

// =============================================================================
//  Fullpixel Motion Search: OBMC
// =============================================================================
static INLINE int get_obmc_mvpred_var(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const FULLPEL_MV *this_mv) {
  const aom_variance_fn_ptr_t *vfp = ms_params->vfp;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const MSBuffers *ms_buffers = &ms_params->ms_buffers;
  const int32_t *wsrc = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const struct buf_2d *ref_buf = ms_buffers->ref;

  const MV mv = get_mv_from_fullmv(this_mv);
  unsigned int unused;

  return vfp->ovf(get_buf_from_fullmv(ref_buf, this_mv), ref_buf->stride, wsrc,
                  mask, &unused) +
         mv_err_cost_(&mv, mv_cost_params);
}

static int obmc_refining_search_sad(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, FULLPEL_MV *best_mv) {
  const aom_variance_fn_ptr_t *fn_ptr = ms_params->vfp;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const MSBuffers *ms_buffers = &ms_params->ms_buffers;
  const int32_t *wsrc = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const struct buf_2d *ref_buf = ms_buffers->ref;
  const FULLPEL_MV neighbors[4] = { { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, 0 } };
  const int kSearchRange = 8;

  unsigned int best_sad = fn_ptr->osdf(get_buf_from_fullmv(ref_buf, best_mv),
                                       ref_buf->stride, wsrc, mask) +
                          mvsad_err_cost_(best_mv, mv_cost_params);

  for (int i = 0; i < kSearchRange; i++) {
    int best_site = -1;

    for (int j = 0; j < 4; j++) {
      const FULLPEL_MV mv = { best_mv->row + neighbors[j].row,
                              best_mv->col + neighbors[j].col };
      if (av1_is_fullmv_in_range(&ms_params->mv_limits, mv)) {
        unsigned int sad = fn_ptr->osdf(get_buf_from_fullmv(ref_buf, &mv),
                                        ref_buf->stride, wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost_(&mv, mv_cost_params);

          if (sad < best_sad) {
            best_sad = sad;
            best_site = j;
          }
        }
      }
    }

    if (best_site == -1) {
      break;
    } else {
      best_mv->row += neighbors[best_site].row;
      best_mv->col += neighbors[best_site].col;
    }
  }
  return best_sad;
}

static int obmc_diamond_search_sad(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, FULLPEL_MV start_mv,
    FULLPEL_MV *best_mv, int search_step, int *num00) {
  const aom_variance_fn_ptr_t *fn_ptr = ms_params->vfp;
  const search_site_config *cfg = ms_params->search_sites;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const MSBuffers *ms_buffers = &ms_params->ms_buffers;
  const int32_t *wsrc = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const struct buf_2d *const ref_buf = ms_buffers->ref;

  // search_step determines the length of the initial step and hence the number
  // of iterations.
  const int tot_steps = cfg->num_search_steps - search_step;
  const uint8_t *best_address, *init_ref;
  int best_sad = INT_MAX;
  int best_site = 0;

  clamp_fullmv(&start_mv, &ms_params->mv_limits);
  best_address = init_ref = get_buf_from_fullmv(ref_buf, &start_mv);
  *num00 = 0;
  *best_mv = start_mv;

  // Check the starting position
  best_sad = fn_ptr->osdf(best_address, ref_buf->stride, wsrc, mask) +
             mvsad_err_cost_(best_mv, mv_cost_params);

  for (int step = tot_steps - 1; step >= 0; --step) {
    const search_site *const site = cfg->site[step];
    best_site = 0;
    for (int idx = 1; idx <= cfg->searches_per_step[step]; ++idx) {
      const FULLPEL_MV mv = { best_mv->row + site[idx].mv.row,
                              best_mv->col + site[idx].mv.col };
      if (av1_is_fullmv_in_range(&ms_params->mv_limits, mv)) {
        int sad = fn_ptr->osdf(best_address + site[idx].offset, ref_buf->stride,
                               wsrc, mask);
        if (sad < best_sad) {
          sad += mvsad_err_cost_(&mv, mv_cost_params);

          if (sad < best_sad) {
            best_sad = sad;
            best_site = idx;
          }
        }
      }
    }

    if (best_site != 0) {
      best_mv->row += site[best_site].mv.row;
      best_mv->col += site[best_site].mv.col;
      best_address += site[best_site].offset;
    } else if (best_address == init_ref) {
      (*num00)++;
    }
  }
  return best_sad;
}

static int obmc_full_pixel_diamond(
    const FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const FULLPEL_MV start_mv,
    int step_param, FULLPEL_MV *best_mv) {
  const search_site_config *cfg = ms_params->search_sites;
  FULLPEL_MV tmp_mv;
  int thissme, n, num00 = 0;
  int bestsme =
      obmc_diamond_search_sad(ms_params, start_mv, &tmp_mv, step_param, &n);
  if (bestsme < INT_MAX) bestsme = get_obmc_mvpred_var(ms_params, &tmp_mv);
  *best_mv = tmp_mv;

  // If there won't be more n-step search, check to see if refining search is
  // needed.
  const int further_steps = cfg->num_search_steps - 1 - step_param;

  while (n < further_steps) {
    ++n;

    if (num00) {
      num00--;
    } else {
      thissme = obmc_diamond_search_sad(ms_params, start_mv, &tmp_mv,
                                        step_param + n, &num00);
      if (thissme < INT_MAX) thissme = get_obmc_mvpred_var(ms_params, &tmp_mv);

      if (thissme < bestsme) {
        bestsme = thissme;
        *best_mv = tmp_mv;
      }
    }
  }

  return bestsme;
}

int av1_obmc_full_pixel_search(const FULLPEL_MV start_mv,
                               const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                               const int step_param, FULLPEL_MV *best_mv) {
  if (!ms_params->fast_obmc_search) {
    const int bestsme =
        obmc_full_pixel_diamond(ms_params, start_mv, step_param, best_mv);
    return bestsme;
  } else {
    *best_mv = start_mv;
    clamp_fullmv(best_mv, &ms_params->mv_limits);
    int thissme = obmc_refining_search_sad(ms_params, best_mv);
    if (thissme < INT_MAX) thissme = get_obmc_mvpred_var(ms_params, best_mv);
    return thissme;
  }
}

// =============================================================================
//  Subpixel Motion Search: Translational
// =============================================================================
#define INIT_SUBPEL_STEP_SIZE (4)
/*
 * To avoid the penalty for crossing cache-line read, preload the reference
 * area in a small buffer, which is aligned to make sure there won't be crossing
 * cache-line read while reading from this buffer. This reduced the cpu
 * cycles spent on reading ref data in sub-pixel filter functions.
 * TODO: Currently, since sub-pixel search range here is -3 ~ 3, copy 22 rows x
 * 32 cols area that is enough for 16x16 macroblock. Later, for SPLITMV, we
 * could reduce the area.
 */

// Returns the subpel offset used by various subpel variance functions [m]sv[a]f
static INLINE int get_subpel_part(int x) { return x & 7; }

// Gets the address of the ref buffer at subpel location (r, c), rounded to the
// nearest fullpel precision toward - \infty
static INLINE const uint8_t *get_buf_from_mv(const struct buf_2d *buf,
                                             const MV mv) {
  const int offset = (mv.row >> 3) * buf->stride + (mv.col >> 3);
  return &buf->buf[offset];
}

// Estimates the variance of prediction residue using bilinear filter for fast
// search.
static INLINE int estimated_pref_error(
    const MV *this_mv, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    unsigned int *sse) {
  const aom_variance_fn_ptr_t *vfp = var_params->vfp;

  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const uint8_t *src = ms_buffers->src->buf;
  const uint8_t *ref = get_buf_from_mv(ms_buffers->ref, *this_mv);
  const int src_stride = ms_buffers->src->stride;
  const int ref_stride = ms_buffers->ref->stride;
  const uint8_t *second_pred = ms_buffers->second_pred;
  const uint8_t *mask = ms_buffers->mask;
  const int mask_stride = ms_buffers->mask_stride;
  const int invert_mask = ms_buffers->inv_mask;

  const int subpel_x_q3 = get_subpel_part(this_mv->col);
  const int subpel_y_q3 = get_subpel_part(this_mv->row);

  if (second_pred == NULL) {
    return vfp->svf(ref, ref_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                    sse);
  } else if (mask) {
    return vfp->msvf(ref, ref_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                     second_pred, mask, mask_stride, invert_mask, sse);
  } else {
    return vfp->svaf(ref, ref_stride, subpel_x_q3, subpel_y_q3, src, src_stride,
                     sse, second_pred);
  }
}

// Calculates the variance of prediction residue.
static int upsampled_pref_error(MACROBLOCKD *xd, const AV1_COMMON *cm,
                                const MV *this_mv,
                                const SUBPEL_SEARCH_VAR_PARAMS *var_params,
                                unsigned int *sse) {
  const aom_variance_fn_ptr_t *vfp = var_params->vfp;
  const SUBPEL_SEARCH_TYPE subpel_search_type = var_params->subpel_search_type;

  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const uint8_t *src = ms_buffers->src->buf;
  const uint8_t *ref = get_buf_from_mv(ms_buffers->ref, *this_mv);
  const int src_stride = ms_buffers->src->stride;
  const int ref_stride = ms_buffers->ref->stride;
  const uint8_t *second_pred = ms_buffers->second_pred;
  const uint8_t *mask = ms_buffers->mask;
  const int mask_stride = ms_buffers->mask_stride;
  const int invert_mask = ms_buffers->inv_mask;
  const int w = var_params->w;
  const int h = var_params->h;

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int subpel_x_q3 = get_subpel_part(this_mv->col);
  const int subpel_y_q3 = get_subpel_part(this_mv->row);

  unsigned int besterr;
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    DECLARE_ALIGNED(16, uint16_t, pred16[MAX_SB_SQUARE]);
    uint8_t *pred8 = CONVERT_TO_BYTEPTR(pred16);
    if (second_pred != NULL) {
      if (mask) {
        aom_highbd_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred8, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, mask, mask_stride,
            invert_mask, xd->bd, subpel_search_type);
      } else {
        aom_highbd_comp_avg_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred8, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, xd->bd,
            subpel_search_type);
      }
    } else {
      aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred8, w, h,
                                subpel_x_q3, subpel_y_q3, ref, ref_stride,
                                xd->bd, subpel_search_type);
    }
    besterr = vfp->vf(pred8, w, src, src_stride, sse);
  } else {
    DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
    if (second_pred != NULL) {
      if (mask) {
        aom_comp_mask_upsampled_pred(
            xd, cm, mi_row, mi_col, this_mv, pred, second_pred, w, h,
            subpel_x_q3, subpel_y_q3, ref, ref_stride, mask, mask_stride,
            invert_mask, subpel_search_type);
      } else {
        aom_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                    second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                    ref, ref_stride, subpel_search_type);
      }
    } else {
      aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h,
                         subpel_x_q3, subpel_y_q3, ref, ref_stride,
                         subpel_search_type);
    }

    besterr = vfp->vf(pred, w, src, src_stride, sse);
  }
#else
  DECLARE_ALIGNED(16, uint8_t, pred[MAX_SB_SQUARE]);
  if (second_pred != NULL) {
    if (mask) {
      aom_comp_mask_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                   second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                   ref, ref_stride, mask, mask_stride,
                                   invert_mask, subpel_search_type);
    } else {
      aom_comp_avg_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred,
                                  second_pred, w, h, subpel_x_q3, subpel_y_q3,
                                  ref, ref_stride, subpel_search_type);
    }
  } else {
    aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h, subpel_x_q3,
                       subpel_y_q3, ref, ref_stride, subpel_search_type);
  }

  besterr = vfp->vf(pred, w, src, src_stride, sse);
#endif
  return besterr;
}

// Estimates whether this_mv is better than best_mv. This function incorporates
// both prediction error and residue into account. It is suffixed "fast" because
// it uses bilinear filter to estimate the prediction.
static INLINE unsigned int check_better_fast(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV *this_mv, MV *best_mv,
    const SubpelMvLimits *mv_limits, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int *has_better_mv, int is_scaled) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    int thismse;
    if (is_scaled) {
      thismse = upsampled_pref_error(xd, cm, this_mv, var_params, &sse);
    } else {
      thismse = estimated_pref_error(this_mv, var_params, &sse);
    }
    cost = mv_err_cost_(this_mv, mv_cost_params);
    cost += thismse;

    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
      *has_better_mv |= 1;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

// Checks whether this_mv is better than best_mv. This function incorporates
// both prediction error and residue into account.
static AOM_FORCE_INLINE unsigned int check_better(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV *this_mv, MV *best_mv,
    const SubpelMvLimits *mv_limits, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int *is_better) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    int thismse;
    thismse = upsampled_pref_error(xd, cm, this_mv, var_params, &sse);
    cost = mv_err_cost_(this_mv, mv_cost_params);
    cost += thismse;
    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
      *is_better |= 1;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

static INLINE MV get_best_diag_step(int step_size, unsigned int left_cost,
                                    unsigned int right_cost,
                                    unsigned int up_cost,
                                    unsigned int down_cost) {
  const MV diag_step = { up_cost <= down_cost ? -step_size : step_size,
                         left_cost <= right_cost ? -step_size : step_size };

  return diag_step;
}

// Searches the four cardinal direction for a better mv, then follows up with a
// search in the best quadrant. This uses bilinear filter to speed up the
// calculation.
static AOM_FORCE_INLINE MV first_level_check_fast(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV this_mv, MV *best_mv,
    int hstep, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int is_scaled) {
  // Check the four cardinal directions
  const MV left_mv = { this_mv.row, this_mv.col - hstep };
  int dummy = 0;
  const unsigned int left = check_better_fast(
      xd, cm, &left_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr,
      sse1, distortion, &dummy, is_scaled);

  const MV right_mv = { this_mv.row, this_mv.col + hstep };
  const unsigned int right = check_better_fast(
      xd, cm, &right_mv, best_mv, mv_limits, var_params, mv_cost_params,
      besterr, sse1, distortion, &dummy, is_scaled);

  const MV top_mv = { this_mv.row - hstep, this_mv.col };
  const unsigned int up = check_better_fast(
      xd, cm, &top_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr,
      sse1, distortion, &dummy, is_scaled);

  const MV bottom_mv = { this_mv.row + hstep, this_mv.col };
  const unsigned int down = check_better_fast(
      xd, cm, &bottom_mv, best_mv, mv_limits, var_params, mv_cost_params,
      besterr, sse1, distortion, &dummy, is_scaled);

  const MV diag_step = get_best_diag_step(hstep, left, right, up, down);
  const MV diag_mv = { this_mv.row + diag_step.row,
                       this_mv.col + diag_step.col };

  // Check the diagonal direction with the best mv
  check_better_fast(xd, cm, &diag_mv, best_mv, mv_limits, var_params,
                    mv_cost_params, besterr, sse1, distortion, &dummy,
                    is_scaled);

  return diag_step;
}

// Performs a following up search after first_level_check_fast is called. This
// performs two extra chess pattern searches in the best quadrant.
static AOM_FORCE_INLINE void second_level_check_fast(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV this_mv, const MV diag_step,
    MV *best_mv, int hstep, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int is_scaled) {
  assert(diag_step.row == hstep || diag_step.row == -hstep);
  assert(diag_step.col == hstep || diag_step.col == -hstep);
  const int tr = this_mv.row;
  const int tc = this_mv.col;
  const int br = best_mv->row;
  const int bc = best_mv->col;
  int dummy = 0;
  if (tr != br && tc != bc) {
    assert(diag_step.col == bc - tc);
    assert(diag_step.row == br - tr);
    const MV chess_mv_1 = { br, bc + diag_step.col };
    const MV chess_mv_2 = { br + diag_step.row, bc };
    check_better_fast(xd, cm, &chess_mv_1, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);

    check_better_fast(xd, cm, &chess_mv_2, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);
  } else if (tr == br && tc != bc) {
    assert(diag_step.col == bc - tc);
    // Continue searching in the best direction
    const MV bottom_long_mv = { br + hstep, bc + diag_step.col };
    const MV top_long_mv = { br - hstep, bc + diag_step.col };
    check_better_fast(xd, cm, &bottom_long_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);
    check_better_fast(xd, cm, &top_long_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);

    // Search in the direction opposite of the best quadrant
    const MV rev_mv = { br - diag_step.row, bc };
    check_better_fast(xd, cm, &rev_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);
  } else if (tr != br && tc == bc) {
    assert(diag_step.row == br - tr);
    // Continue searching in the best direction
    const MV right_long_mv = { br + diag_step.row, bc + hstep };
    const MV left_long_mv = { br + diag_step.row, bc - hstep };
    check_better_fast(xd, cm, &right_long_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);
    check_better_fast(xd, cm, &left_long_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);

    // Search in the direction opposite of the best quadrant
    const MV rev_mv = { br, bc - diag_step.col };
    check_better_fast(xd, cm, &rev_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy,
                      is_scaled);
  }
}

// Combines first level check and second level check when applicable. This first
// searches the four cardinal directions, and perform several
// diagonal/chess-pattern searches in the best quadrant.
static AOM_FORCE_INLINE void two_level_checks_fast(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV this_mv, MV *best_mv,
    int hstep, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int iters, int is_scaled) {
  const MV diag_step = first_level_check_fast(
      xd, cm, this_mv, best_mv, hstep, mv_limits, var_params, mv_cost_params,
      besterr, sse1, distortion, is_scaled);
  if (iters > 1) {
    second_level_check_fast(xd, cm, this_mv, diag_step, best_mv, hstep,
                            mv_limits, var_params, mv_cost_params, besterr,
                            sse1, distortion, is_scaled);
  }
}

static AOM_FORCE_INLINE MV
first_level_check(MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV this_mv,
                  MV *best_mv, const int hstep, const SubpelMvLimits *mv_limits,
                  const SUBPEL_SEARCH_VAR_PARAMS *var_params,
                  const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
                  unsigned int *sse1, int *distortion) {
  int dummy = 0;
  const MV left_mv = { this_mv.row, this_mv.col - hstep };
  const MV right_mv = { this_mv.row, this_mv.col + hstep };
  const MV top_mv = { this_mv.row - hstep, this_mv.col };
  const MV bottom_mv = { this_mv.row + hstep, this_mv.col };

  const unsigned int left =
      check_better(xd, cm, &left_mv, best_mv, mv_limits, var_params,
                   mv_cost_params, besterr, sse1, distortion, &dummy);
  const unsigned int right =
      check_better(xd, cm, &right_mv, best_mv, mv_limits, var_params,
                   mv_cost_params, besterr, sse1, distortion, &dummy);
  const unsigned int up =
      check_better(xd, cm, &top_mv, best_mv, mv_limits, var_params,
                   mv_cost_params, besterr, sse1, distortion, &dummy);
  const unsigned int down =
      check_better(xd, cm, &bottom_mv, best_mv, mv_limits, var_params,
                   mv_cost_params, besterr, sse1, distortion, &dummy);

  const MV diag_step = get_best_diag_step(hstep, left, right, up, down);
  const MV diag_mv = { this_mv.row + diag_step.row,
                       this_mv.col + diag_step.col };

  // Check the diagonal direction with the best mv
  check_better(xd, cm, &diag_mv, best_mv, mv_limits, var_params, mv_cost_params,
               besterr, sse1, distortion, &dummy);

  return diag_step;
}

// A newer version of second level check that gives better quality.
// TODO(chiyotsai@google.com): evaluate this on subpel_search_types different
// from av1_find_best_sub_pixel_tree
static AOM_FORCE_INLINE void second_level_check_v2(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV this_mv, MV diag_step,
    MV *best_mv, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int is_scaled) {
  assert(best_mv->row == this_mv.row + diag_step.row ||
         best_mv->col == this_mv.col + diag_step.col);
  if (CHECK_MV_EQUAL(this_mv, *best_mv)) {
    return;
  } else if (this_mv.row == best_mv->row) {
    // Search away from diagonal step since diagonal search did not provide any
    // improvement
    diag_step.row *= -1;
  } else if (this_mv.col == best_mv->col) {
    diag_step.col *= -1;
  }

  const MV row_bias_mv = { best_mv->row + diag_step.row, best_mv->col };
  const MV col_bias_mv = { best_mv->row, best_mv->col + diag_step.col };
  const MV diag_bias_mv = { best_mv->row + diag_step.row,
                            best_mv->col + diag_step.col };
  int has_better_mv = 0;

  if (var_params->subpel_search_type != USE_2_TAPS_ORIG) {
    check_better(xd, cm, &row_bias_mv, best_mv, mv_limits, var_params,
                 mv_cost_params, besterr, sse1, distortion, &has_better_mv);
    check_better(xd, cm, &col_bias_mv, best_mv, mv_limits, var_params,
                 mv_cost_params, besterr, sse1, distortion, &has_better_mv);

    // Do an additional search if the second iteration gives a better mv
    if (has_better_mv) {
      check_better(xd, cm, &diag_bias_mv, best_mv, mv_limits, var_params,
                   mv_cost_params, besterr, sse1, distortion, &has_better_mv);
    }
  } else {
    check_better_fast(xd, cm, &row_bias_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &has_better_mv,
                      is_scaled);
    check_better_fast(xd, cm, &col_bias_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &has_better_mv,
                      is_scaled);

    // Do an additional search if the second iteration gives a better mv
    if (has_better_mv) {
      check_better_fast(xd, cm, &diag_bias_mv, best_mv, mv_limits, var_params,
                        mv_cost_params, besterr, sse1, distortion,
                        &has_better_mv, is_scaled);
    }
  }
}

// Gets the error at the beginning when the mv has fullpel precision
static unsigned int setup_center_error(
    const MACROBLOCKD *xd, const MV *bestmv,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  const aom_variance_fn_ptr_t *vfp = var_params->vfp;
  const int w = var_params->w;
  const int h = var_params->h;

  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const uint8_t *src = ms_buffers->src->buf;
  const uint8_t *y = get_buf_from_mv(ms_buffers->ref, *bestmv);
  const int src_stride = ms_buffers->src->stride;
  const int y_stride = ms_buffers->ref->stride;
  const uint8_t *second_pred = ms_buffers->second_pred;
  const uint8_t *mask = ms_buffers->mask;
  const int mask_stride = ms_buffers->mask_stride;
  const int invert_mask = ms_buffers->inv_mask;

  unsigned int besterr;

  if (second_pred != NULL) {
#if CONFIG_AV1_HIGHBITDEPTH
    if (is_cur_buf_hbd(xd)) {
      DECLARE_ALIGNED(16, uint16_t, comp_pred16[MAX_SB_SQUARE]);
      uint8_t *comp_pred = CONVERT_TO_BYTEPTR(comp_pred16);
      if (mask) {
        aom_highbd_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride,
                                  mask, mask_stride, invert_mask);
      } else {
        aom_highbd_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
      }
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    } else {
      DECLARE_ALIGNED(16, uint8_t, comp_pred[MAX_SB_SQUARE]);
      if (mask) {
        aom_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride, mask,
                           mask_stride, invert_mask);
      } else {
        aom_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
      }
      besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
    }
#else
    (void)xd;
    DECLARE_ALIGNED(16, uint8_t, comp_pred[MAX_SB_SQUARE]);
    if (mask) {
      aom_comp_mask_pred(comp_pred, second_pred, w, h, y, y_stride, mask,
                         mask_stride, invert_mask);
    } else {
      aom_comp_avg_pred(comp_pred, second_pred, w, h, y, y_stride);
    }
    besterr = vfp->vf(comp_pred, w, src, src_stride, sse1);
#endif
  } else {
    besterr = vfp->vf(y, y_stride, src, src_stride, sse1);
  }
  *distortion = besterr;
  besterr += mv_err_cost_(bestmv, mv_cost_params);
  return besterr;
}

// Gets the error at the beginning when the mv has fullpel precision
static unsigned int upsampled_setup_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV *bestmv,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  unsigned int besterr = upsampled_pref_error(xd, cm, bestmv, var_params, sse1);
  *distortion = besterr;
  besterr += mv_err_cost_(bestmv, mv_cost_params);
  return besterr;
}

static INLINE int divide_and_round(int n, int d) {
  return ((n < 0) ^ (d < 0)) ? ((n - d / 2) / d) : ((n + d / 2) / d);
}

static INLINE int is_cost_list_wellbehaved(const int *cost_list) {
  return cost_list[0] < cost_list[1] && cost_list[0] < cost_list[2] &&
         cost_list[0] < cost_list[3] && cost_list[0] < cost_list[4];
}

// Returns surface minima estimate at given precision in 1/2^n bits.
// Assume a model for the cost surface: S = A(x - x0)^2 + B(y - y0)^2 + C
// For a given set of costs S0, S1, S2, S3, S4 at points
// (y, x) = (0, 0), (0, -1), (1, 0), (0, 1) and (-1, 0) respectively,
// the solution for the location of the minima (x0, y0) is given by:
// x0 = 1/2 (S1 - S3)/(S1 + S3 - 2*S0),
// y0 = 1/2 (S4 - S2)/(S4 + S2 - 2*S0).
// The code below is an integerized version of that.
static AOM_INLINE void get_cost_surf_min(const int *cost_list, int *ir, int *ic,
                                         int bits) {
  *ic = divide_and_round((cost_list[1] - cost_list[3]) * (1 << (bits - 1)),
                         (cost_list[1] - 2 * cost_list[0] + cost_list[3]));
  *ir = divide_and_round((cost_list[4] - cost_list[2]) * (1 << (bits - 1)),
                         (cost_list[4] - 2 * cost_list[0] + cost_list[2]));
}

// Checks the list of mvs searched in the last iteration and see if we are
// repeating it. If so, return 1. Otherwise we update the last_mv_search_list
// with current_mv and return 0.
static INLINE int check_repeated_mv_and_update(int_mv *last_mv_search_list,
                                               const MV current_mv, int iter) {
  if (last_mv_search_list) {
    if (CHECK_MV_EQUAL(last_mv_search_list[iter].as_mv, current_mv)) {
      return 1;
    }

    last_mv_search_list[iter].as_mv = current_mv;
  }
  return 0;
}

static AOM_INLINE int setup_center_error_facade(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV *bestmv,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion,
    int is_scaled) {
  if (is_scaled) {
    return upsampled_setup_center_error(xd, cm, bestmv, var_params,
                                        mv_cost_params, sse1, distortion);
  } else {
    return setup_center_error(xd, bestmv, var_params, mv_cost_params, sse1,
                              distortion);
  }
}

int av1_find_best_sub_pixel_tree_pruned_more(
    MACROBLOCKD *xd, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, MV start_mv, MV *bestmv,
    int *distortion, unsigned int *sse1, int_mv *last_mv_search_list) {
  (void)cm;
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int *cost_list = ms_params->cost_list;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;

  // The iteration we are current searching for. Iter 0 corresponds to fullpel
  // mv, iter 1 to half pel, and so on
  int iter = 0;
  int hstep = INIT_SUBPEL_STEP_SIZE;  // Step size, initialized to 4/8=1/2 pel
  unsigned int besterr = INT_MAX;
  *bestmv = start_mv;

  const struct scale_factors *const sf = is_intrabc_block(xd->mi[0])
                                             ? &cm->sf_identity
                                             : xd->block_ref_scale_factors[0];
  const int is_scaled = av1_is_scaled(sf);
  besterr = setup_center_error_facade(
      xd, cm, bestmv, var_params, mv_cost_params, sse1, distortion, is_scaled);

  // If forced_stop is FULL_PEL, return.
  if (forced_stop == FULL_PEL) return besterr;

  if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
    return INT_MAX;
  }
  iter++;

  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX && is_cost_list_wellbehaved(cost_list)) {
    int ir, ic;
    get_cost_surf_min(cost_list, &ir, &ic, 1);
    if (ir != 0 || ic != 0) {
      const MV this_mv = { start_mv.row + ir * hstep,
                           start_mv.col + ic * hstep };
      int dummy = 0;
      check_better_fast(xd, cm, &this_mv, bestmv, mv_limits, var_params,
                        mv_cost_params, &besterr, sse1, distortion, &dummy,
                        is_scaled);
    }
  } else {
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel
  if (forced_stop < HALF_PEL) {
    if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
      return INT_MAX;
    }
    iter++;

    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  if (allow_hp && forced_stop == EIGHTH_PEL) {
    if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
      return INT_MAX;
    }
    iter++;

    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  return besterr;
}

int av1_find_best_sub_pixel_tree_pruned(
    MACROBLOCKD *xd, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, MV start_mv, MV *bestmv,
    int *distortion, unsigned int *sse1, int_mv *last_mv_search_list) {
  (void)cm;
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const int *cost_list = ms_params->cost_list;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;

  // The iteration we are current searching for. Iter 0 corresponds to fullpel
  // mv, iter 1 to half pel, and so on
  int iter = 0;
  int hstep = INIT_SUBPEL_STEP_SIZE;  // Step size, initialized to 4/8=1/2 pel
  unsigned int besterr = INT_MAX;
  *bestmv = start_mv;

  const struct scale_factors *const sf = is_intrabc_block(xd->mi[0])
                                             ? &cm->sf_identity
                                             : xd->block_ref_scale_factors[0];
  const int is_scaled = av1_is_scaled(sf);
  besterr = setup_center_error_facade(
      xd, cm, bestmv, var_params, mv_cost_params, sse1, distortion, is_scaled);

  // If forced_stop is FULL_PEL, return.
  if (forced_stop == FULL_PEL) return besterr;

  if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
    return INT_MAX;
  }
  iter++;

  if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX &&
      cost_list[2] != INT_MAX && cost_list[3] != INT_MAX &&
      cost_list[4] != INT_MAX) {
    const unsigned int whichdir = (cost_list[1] < cost_list[3] ? 0 : 1) +
                                  (cost_list[2] < cost_list[4] ? 0 : 2);

    const MV left_mv = { start_mv.row, start_mv.col - hstep };
    const MV right_mv = { start_mv.row, start_mv.col + hstep };
    const MV bottom_mv = { start_mv.row + hstep, start_mv.col };
    const MV top_mv = { start_mv.row - hstep, start_mv.col };

    const MV bottom_left_mv = { start_mv.row + hstep, start_mv.col - hstep };
    const MV bottom_right_mv = { start_mv.row + hstep, start_mv.col + hstep };
    const MV top_left_mv = { start_mv.row - hstep, start_mv.col - hstep };
    const MV top_right_mv = { start_mv.row - hstep, start_mv.col + hstep };

    int dummy = 0;

    switch (whichdir) {
      case 0:  // bottom left quadrant
        check_better_fast(xd, cm, &left_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &bottom_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &bottom_left_mv, bestmv, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, &dummy, is_scaled);
        break;
      case 1:  // bottom right quadrant
        check_better_fast(xd, cm, &right_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &bottom_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &bottom_right_mv, bestmv, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, &dummy, is_scaled);
        break;
      case 2:  // top left quadrant
        check_better_fast(xd, cm, &left_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &top_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &top_left_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        break;
      case 3:  // top right quadrant
        check_better_fast(xd, cm, &right_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &top_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        check_better_fast(xd, cm, &top_right_mv, bestmv, mv_limits, var_params,
                          mv_cost_params, &besterr, sse1, distortion, &dummy,
                          is_scaled);
        break;
    }
  } else {
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  // Each subsequent iteration checks at least one point in common with
  // the last iteration could be 2 ( if diag selected) 1/4 pel
  if (forced_stop < HALF_PEL) {
    if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
      return INT_MAX;
    }
    iter++;

    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  if (allow_hp && forced_stop == EIGHTH_PEL) {
    if (check_repeated_mv_and_update(last_mv_search_list, *bestmv, iter)) {
      return INT_MAX;
    }
    iter++;

    hstep >>= 1;
    start_mv = *bestmv;
    two_level_checks_fast(xd, cm, start_mv, bestmv, hstep, mv_limits,
                          var_params, mv_cost_params, &besterr, sse1,
                          distortion, iters_per_step, is_scaled);
  }

  return besterr;
}

int av1_find_best_sub_pixel_tree(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                 const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                 MV start_mv, MV *bestmv, int *distortion,
                                 unsigned int *sse1,
                                 int_mv *last_mv_search_list) {
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;
  const SUBPEL_SEARCH_TYPE subpel_search_type =
      ms_params->var_params.subpel_search_type;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;

  // How many steps to take. A round of 0 means fullpel search only, 1 means
  // half-pel, and so on.
  const int round = AOMMIN(FULL_PEL - forced_stop, 3 - !allow_hp);
  int hstep = INIT_SUBPEL_STEP_SIZE;  // Step size, initialized to 4/8=1/2 pel

  unsigned int besterr = INT_MAX;

  *bestmv = start_mv;

  const struct scale_factors *const sf = is_intrabc_block(xd->mi[0])
                                             ? &cm->sf_identity
                                             : xd->block_ref_scale_factors[0];
  const int is_scaled = av1_is_scaled(sf);

  if (subpel_search_type != USE_2_TAPS_ORIG) {
    besterr = upsampled_setup_center_error(xd, cm, bestmv, var_params,
                                           mv_cost_params, sse1, distortion);
  } else {
    besterr = setup_center_error(xd, bestmv, var_params, mv_cost_params, sse1,
                                 distortion);
  }

  // If forced_stop is FULL_PEL, return.
  if (!round) return besterr;

  for (int iter = 0; iter < round; ++iter) {
    MV iter_center_mv = *bestmv;
    if (check_repeated_mv_and_update(last_mv_search_list, iter_center_mv,
                                     iter)) {
      return INT_MAX;
    }

    MV diag_step;
    if (subpel_search_type != USE_2_TAPS_ORIG) {
      diag_step = first_level_check(xd, cm, iter_center_mv, bestmv, hstep,
                                    mv_limits, var_params, mv_cost_params,
                                    &besterr, sse1, distortion);
    } else {
      diag_step = first_level_check_fast(xd, cm, iter_center_mv, bestmv, hstep,
                                         mv_limits, var_params, mv_cost_params,
                                         &besterr, sse1, distortion, is_scaled);
    }

    // Check diagonal sub-pixel position
    if (!CHECK_MV_EQUAL(iter_center_mv, *bestmv) && iters_per_step > 1) {
      second_level_check_v2(xd, cm, iter_center_mv, diag_step, bestmv,
                            mv_limits, var_params, mv_cost_params, &besterr,
                            sse1, distortion, is_scaled);
    }

    hstep >>= 1;
  }

  return besterr;
}

// Note(yunqingwang): The following 2 functions are only used in the motion
// vector unit test, which return extreme motion vectors allowed by the MV
// limits.
// Returns the maximum MV.
int av1_return_max_sub_pixel_mv(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                MV start_mv, MV *bestmv, int *distortion,
                                unsigned int *sse1,
                                int_mv *last_mv_search_list) {
  (void)xd;
  (void)cm;
  (void)start_mv;
  (void)sse1;
  (void)distortion;
  (void)last_mv_search_list;

  const int allow_hp = ms_params->allow_hp;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;

  bestmv->row = mv_limits->row_max;
  bestmv->col = mv_limits->col_max;

  unsigned int besterr = 0;

  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}

// Returns the minimum MV.
int av1_return_min_sub_pixel_mv(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                MV start_mv, MV *bestmv, int *distortion,
                                unsigned int *sse1,
                                int_mv *last_mv_search_list) {
  (void)xd;
  (void)cm;
  (void)start_mv;
  (void)sse1;
  (void)distortion;
  (void)last_mv_search_list;

  const int allow_hp = ms_params->allow_hp;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;

  bestmv->row = mv_limits->row_min;
  bestmv->col = mv_limits->col_min;

  unsigned int besterr = 0;
  // In the sub-pel motion search, if hp is not used, then the last bit of mv
  // has to be 0.
  lower_mv_precision(bestmv, allow_hp, 0);
  return besterr;
}

#if !CONFIG_REALTIME_ONLY
// Computes the cost of the current predictor by going through the whole
// av1_enc_build_inter_predictor pipeline. This is mainly used by warped mv
// during motion_mode_rd. We are going through the whole
// av1_enc_build_inter_predictor because we might have changed the interpolation
// filter, etc before motion_mode_rd is called.
static INLINE unsigned int compute_motion_cost(
    MACROBLOCKD *xd, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, BLOCK_SIZE bsize,
    const MV *this_mv) {
  unsigned int mse;
  unsigned int sse;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                AOM_PLANE_Y, AOM_PLANE_Y);

  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;
  const MSBuffers *ms_buffers = &var_params->ms_buffers;

  const uint8_t *const src = ms_buffers->src->buf;
  const int src_stride = ms_buffers->src->stride;
  const uint8_t *const dst = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;
  const aom_variance_fn_ptr_t *vfp = ms_params->var_params.vfp;

  mse = vfp->vf(dst, dst_stride, src, src_stride, &sse);
  mse += mv_err_cost_(this_mv, &ms_params->mv_cost_params);
  return mse;
}

// Refines MV in a small range
unsigned int av1_refine_warped_mv(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                  const SUBPEL_MOTION_SEARCH_PARAMS *ms_params,
                                  BLOCK_SIZE bsize, const int *pts0,
                                  const int *pts_inref0, int total_samples) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  static const MV neighbors[8] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
                                   { 0, -2 }, { 2, 0 }, { 0, 2 }, { -2, 0 } };
  MV *best_mv = &mbmi->mv[0].as_mv;

  WarpedMotionParams best_wm_params = mbmi->wm_params;
  int best_num_proj_ref = mbmi->num_proj_ref;
  unsigned int bestmse;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;

  const int start = ms_params->allow_hp ? 0 : 4;

  // Calculate the center position's error
  assert(av1_is_subpelmv_in_range(mv_limits, *best_mv));
  bestmse = compute_motion_cost(xd, cm, ms_params, bsize, best_mv);

  // MV search
  int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE];
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  for (int ite = 0; ite < 2; ++ite) {
    int best_idx = -1;

    for (int idx = start; idx < start + 4; ++idx) {
      unsigned int thismse;

      MV this_mv = { best_mv->row + neighbors[idx].row,
                     best_mv->col + neighbors[idx].col };
      if (av1_is_subpelmv_in_range(mv_limits, this_mv)) {
        memcpy(pts, pts0, total_samples * 2 * sizeof(*pts0));
        memcpy(pts_inref, pts_inref0, total_samples * 2 * sizeof(*pts_inref0));
        if (total_samples > 1) {
          mbmi->num_proj_ref =
              av1_selectSamples(&this_mv, pts, pts_inref, total_samples, bsize);
        }

        if (!av1_find_projection(mbmi->num_proj_ref, pts, pts_inref, bsize,
                                 this_mv.row, this_mv.col, &mbmi->wm_params,
                                 mi_row, mi_col)) {
          thismse = compute_motion_cost(xd, cm, ms_params, bsize, &this_mv);

          if (thismse < bestmse) {
            best_idx = idx;
            best_wm_params = mbmi->wm_params;
            best_num_proj_ref = mbmi->num_proj_ref;
            bestmse = thismse;
          }
        }
      }
    }

    if (best_idx == -1) break;

    if (best_idx >= 0) {
      best_mv->row += neighbors[best_idx].row;
      best_mv->col += neighbors[best_idx].col;
    }
  }

  mbmi->wm_params = best_wm_params;
  mbmi->num_proj_ref = best_num_proj_ref;
  return bestmse;
}
#endif  // !CONFIG_REALTIME_ONLY
// =============================================================================
//  Subpixel Motion Search: OBMC
// =============================================================================
// Estimates the variance of prediction residue
static INLINE int estimate_obmc_pref_error(
    const MV *this_mv, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    unsigned int *sse) {
  const aom_variance_fn_ptr_t *vfp = var_params->vfp;

  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const int32_t *src = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const uint8_t *ref = get_buf_from_mv(ms_buffers->ref, *this_mv);
  const int ref_stride = ms_buffers->ref->stride;

  const int subpel_x_q3 = get_subpel_part(this_mv->col);
  const int subpel_y_q3 = get_subpel_part(this_mv->row);

  return vfp->osvf(ref, ref_stride, subpel_x_q3, subpel_y_q3, src, mask, sse);
}

// Calculates the variance of prediction residue
static int upsampled_obmc_pref_error(MACROBLOCKD *xd, const AV1_COMMON *cm,
                                     const MV *this_mv,
                                     const SUBPEL_SEARCH_VAR_PARAMS *var_params,
                                     unsigned int *sse) {
  const aom_variance_fn_ptr_t *vfp = var_params->vfp;
  const SUBPEL_SEARCH_TYPE subpel_search_type = var_params->subpel_search_type;
  const int w = var_params->w;
  const int h = var_params->h;

  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const int32_t *wsrc = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const uint8_t *ref = get_buf_from_mv(ms_buffers->ref, *this_mv);
  const int ref_stride = ms_buffers->ref->stride;

  const int subpel_x_q3 = get_subpel_part(this_mv->col);
  const int subpel_y_q3 = get_subpel_part(this_mv->row);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  unsigned int besterr;
  DECLARE_ALIGNED(16, uint8_t, pred[2 * MAX_SB_SQUARE]);
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    uint8_t *pred8 = CONVERT_TO_BYTEPTR(pred);
    aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred8, w, h,
                              subpel_x_q3, subpel_y_q3, ref, ref_stride, xd->bd,
                              subpel_search_type);
    besterr = vfp->ovf(pred8, w, wsrc, mask, sse);
  } else {
    aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h, subpel_x_q3,
                       subpel_y_q3, ref, ref_stride, subpel_search_type);

    besterr = vfp->ovf(pred, w, wsrc, mask, sse);
  }
#else
  aom_upsampled_pred(xd, cm, mi_row, mi_col, this_mv, pred, w, h, subpel_x_q3,
                     subpel_y_q3, ref, ref_stride, subpel_search_type);

  besterr = vfp->ovf(pred, w, wsrc, mask, sse);
#endif
  return besterr;
}

static unsigned int setup_obmc_center_error(
    const MV *this_mv, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  // TODO(chiyotsai@google.com): There might be a bug here where we didn't use
  // get_buf_from_mv(ref, *this_mv).
  const MSBuffers *ms_buffers = &var_params->ms_buffers;
  const int32_t *wsrc = ms_buffers->wsrc;
  const int32_t *mask = ms_buffers->obmc_mask;
  const uint8_t *ref = ms_buffers->ref->buf;
  const int ref_stride = ms_buffers->ref->stride;
  unsigned int besterr =
      var_params->vfp->ovf(ref, ref_stride, wsrc, mask, sse1);
  *distortion = besterr;
  besterr += mv_err_cost_(this_mv, mv_cost_params);
  return besterr;
}

static unsigned int upsampled_setup_obmc_center_error(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV *this_mv,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *sse1, int *distortion) {
  unsigned int besterr =
      upsampled_obmc_pref_error(xd, cm, this_mv, var_params, sse1);
  *distortion = besterr;
  besterr += mv_err_cost_(this_mv, mv_cost_params);
  return besterr;
}

// Estimates the variance of prediction residue
// TODO(chiyotsai@google.com): the cost does does not match the cost in
// mv_cost_. Investigate this later.
static INLINE int estimate_obmc_mvcost(const MV *this_mv,
                                       const MV_COST_PARAMS *mv_cost_params) {
  const MV *ref_mv = mv_cost_params->ref_mv;
  const int *mvjcost = mv_cost_params->mvjcost;
  const int *const *mvcost = mv_cost_params->mvcost;
  const int error_per_bit = mv_cost_params->error_per_bit;
  const MV_COST_TYPE mv_cost_type = mv_cost_params->mv_cost_type;
  const MV diff_mv = { GET_MV_SUBPEL(this_mv->row - ref_mv->row),
                       GET_MV_SUBPEL(this_mv->col - ref_mv->col) };

  switch (mv_cost_type) {
    case MV_COST_ENTROPY:
      return (unsigned)((mv_cost(&diff_mv, mvjcost,
                                 CONVERT_TO_CONST_MVCOST(mvcost)) *
                             error_per_bit +
                         4096) >>
                        13);
    case MV_COST_NONE: return 0;
    default:
      assert(0 && "L1 norm is not tuned for estimated obmc mvcost");
      return 0;
  }
}

// Estimates whether this_mv is better than best_mv. This function incorporates
// both prediction error and residue into account.
static INLINE unsigned int obmc_check_better_fast(
    const MV *this_mv, MV *best_mv, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int *has_better_mv) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    const int thismse = estimate_obmc_pref_error(this_mv, var_params, &sse);

    cost = estimate_obmc_mvcost(this_mv, mv_cost_params);
    cost += thismse;

    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
      *has_better_mv |= 1;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

// Estimates whether this_mv is better than best_mv. This function incorporates
// both prediction error and residue into account.
static INLINE unsigned int obmc_check_better(
    MACROBLOCKD *xd, const AV1_COMMON *cm, const MV *this_mv, MV *best_mv,
    const SubpelMvLimits *mv_limits, const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion, int *has_better_mv) {
  unsigned int cost;
  if (av1_is_subpelmv_in_range(mv_limits, *this_mv)) {
    unsigned int sse;
    const int thismse =
        upsampled_obmc_pref_error(xd, cm, this_mv, var_params, &sse);
    cost = mv_err_cost_(this_mv, mv_cost_params);

    cost += thismse;

    if (cost < *besterr) {
      *besterr = cost;
      *best_mv = *this_mv;
      *distortion = thismse;
      *sse1 = sse;
      *has_better_mv |= 1;
    }
  } else {
    cost = INT_MAX;
  }
  return cost;
}

static AOM_FORCE_INLINE MV obmc_first_level_check(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV this_mv, MV *best_mv,
    const int hstep, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion) {
  int dummy = 0;
  const MV left_mv = { this_mv.row, this_mv.col - hstep };
  const MV right_mv = { this_mv.row, this_mv.col + hstep };
  const MV top_mv = { this_mv.row - hstep, this_mv.col };
  const MV bottom_mv = { this_mv.row + hstep, this_mv.col };

  if (var_params->subpel_search_type != USE_2_TAPS_ORIG) {
    const unsigned int left =
        obmc_check_better(xd, cm, &left_mv, best_mv, mv_limits, var_params,
                          mv_cost_params, besterr, sse1, distortion, &dummy);
    const unsigned int right =
        obmc_check_better(xd, cm, &right_mv, best_mv, mv_limits, var_params,
                          mv_cost_params, besterr, sse1, distortion, &dummy);
    const unsigned int up =
        obmc_check_better(xd, cm, &top_mv, best_mv, mv_limits, var_params,
                          mv_cost_params, besterr, sse1, distortion, &dummy);
    const unsigned int down =
        obmc_check_better(xd, cm, &bottom_mv, best_mv, mv_limits, var_params,
                          mv_cost_params, besterr, sse1, distortion, &dummy);

    const MV diag_step = get_best_diag_step(hstep, left, right, up, down);
    const MV diag_mv = { this_mv.row + diag_step.row,
                         this_mv.col + diag_step.col };

    // Check the diagonal direction with the best mv
    obmc_check_better(xd, cm, &diag_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion, &dummy);

    return diag_step;
  } else {
    const unsigned int left = obmc_check_better_fast(
        &left_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr, sse1,
        distortion, &dummy);
    const unsigned int right = obmc_check_better_fast(
        &right_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr,
        sse1, distortion, &dummy);

    const unsigned int up = obmc_check_better_fast(
        &top_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr, sse1,
        distortion, &dummy);

    const unsigned int down = obmc_check_better_fast(
        &bottom_mv, best_mv, mv_limits, var_params, mv_cost_params, besterr,
        sse1, distortion, &dummy);

    const MV diag_step = get_best_diag_step(hstep, left, right, up, down);
    const MV diag_mv = { this_mv.row + diag_step.row,
                         this_mv.col + diag_step.col };

    // Check the diagonal direction with the best mv
    obmc_check_better_fast(&diag_mv, best_mv, mv_limits, var_params,
                           mv_cost_params, besterr, sse1, distortion, &dummy);

    return diag_step;
  }
}

// A newer version of second level check for obmc that gives better quality.
static AOM_FORCE_INLINE void obmc_second_level_check_v2(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, const MV this_mv, MV diag_step,
    MV *best_mv, const SubpelMvLimits *mv_limits,
    const SUBPEL_SEARCH_VAR_PARAMS *var_params,
    const MV_COST_PARAMS *mv_cost_params, unsigned int *besterr,
    unsigned int *sse1, int *distortion) {
  assert(best_mv->row == this_mv.row + diag_step.row ||
         best_mv->col == this_mv.col + diag_step.col);
  if (CHECK_MV_EQUAL(this_mv, *best_mv)) {
    return;
  } else if (this_mv.row == best_mv->row) {
    // Search away from diagonal step since diagonal search did not provide any
    // improvement
    diag_step.row *= -1;
  } else if (this_mv.col == best_mv->col) {
    diag_step.col *= -1;
  }

  const MV row_bias_mv = { best_mv->row + diag_step.row, best_mv->col };
  const MV col_bias_mv = { best_mv->row, best_mv->col + diag_step.col };
  const MV diag_bias_mv = { best_mv->row + diag_step.row,
                            best_mv->col + diag_step.col };
  int has_better_mv = 0;

  if (var_params->subpel_search_type != USE_2_TAPS_ORIG) {
    obmc_check_better(xd, cm, &row_bias_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion,
                      &has_better_mv);
    obmc_check_better(xd, cm, &col_bias_mv, best_mv, mv_limits, var_params,
                      mv_cost_params, besterr, sse1, distortion,
                      &has_better_mv);

    // Do an additional search if the second iteration gives a better mv
    if (has_better_mv) {
      obmc_check_better(xd, cm, &diag_bias_mv, best_mv, mv_limits, var_params,
                        mv_cost_params, besterr, sse1, distortion,
                        &has_better_mv);
    }
  } else {
    obmc_check_better_fast(&row_bias_mv, best_mv, mv_limits, var_params,
                           mv_cost_params, besterr, sse1, distortion,
                           &has_better_mv);
    obmc_check_better_fast(&col_bias_mv, best_mv, mv_limits, var_params,
                           mv_cost_params, besterr, sse1, distortion,
                           &has_better_mv);

    // Do an additional search if the second iteration gives a better mv
    if (has_better_mv) {
      obmc_check_better_fast(&diag_bias_mv, best_mv, mv_limits, var_params,
                             mv_cost_params, besterr, sse1, distortion,
                             &has_better_mv);
    }
  }
}

int av1_find_best_obmc_sub_pixel_tree_up(
    MACROBLOCKD *xd, const AV1_COMMON *const cm,
    const SUBPEL_MOTION_SEARCH_PARAMS *ms_params, MV start_mv, MV *bestmv,
    int *distortion, unsigned int *sse1, int_mv *last_mv_search_list) {
  (void)last_mv_search_list;
  const int allow_hp = ms_params->allow_hp;
  const int forced_stop = ms_params->forced_stop;
  const int iters_per_step = ms_params->iters_per_step;
  const MV_COST_PARAMS *mv_cost_params = &ms_params->mv_cost_params;
  const SUBPEL_SEARCH_VAR_PARAMS *var_params = &ms_params->var_params;
  const SUBPEL_SEARCH_TYPE subpel_search_type =
      ms_params->var_params.subpel_search_type;
  const SubpelMvLimits *mv_limits = &ms_params->mv_limits;

  int hstep = INIT_SUBPEL_STEP_SIZE;
  const int round = AOMMIN(FULL_PEL - forced_stop, 3 - !allow_hp);

  unsigned int besterr = INT_MAX;
  *bestmv = start_mv;

  if (subpel_search_type != USE_2_TAPS_ORIG)
    besterr = upsampled_setup_obmc_center_error(
        xd, cm, bestmv, var_params, mv_cost_params, sse1, distortion);
  else
    besterr = setup_obmc_center_error(bestmv, var_params, mv_cost_params, sse1,
                                      distortion);

  for (int iter = 0; iter < round; ++iter) {
    MV iter_center_mv = *bestmv;
    MV diag_step = obmc_first_level_check(xd, cm, iter_center_mv, bestmv, hstep,
                                          mv_limits, var_params, mv_cost_params,
                                          &besterr, sse1, distortion);

    if (!CHECK_MV_EQUAL(iter_center_mv, *bestmv) && iters_per_step > 1) {
      obmc_second_level_check_v2(xd, cm, iter_center_mv, diag_step, bestmv,
                                 mv_limits, var_params, mv_cost_params,
                                 &besterr, sse1, distortion);
    }
    hstep >>= 1;
  }

  return besterr;
}

// =============================================================================
//  Public cost function: mv_cost + pred error
// =============================================================================
int av1_get_mvpred_sse(const MV_COST_PARAMS *mv_cost_params,
                       const FULLPEL_MV best_mv,
                       const aom_variance_fn_ptr_t *vfp,
                       const struct buf_2d *src, const struct buf_2d *pre) {
  const MV mv = get_mv_from_fullmv(&best_mv);
  unsigned int sse, var;

  var = vfp->vf(src->buf, src->stride, get_buf_from_fullmv(pre, &best_mv),
                pre->stride, &sse);
  (void)var;

  return sse + mv_err_cost_(&mv, mv_cost_params);
}

static INLINE int get_mvpred_av_var(const MV_COST_PARAMS *mv_cost_params,
                                    const FULLPEL_MV best_mv,
                                    const uint8_t *second_pred,
                                    const aom_variance_fn_ptr_t *vfp,
                                    const struct buf_2d *src,
                                    const struct buf_2d *pre) {
  const MV mv = get_mv_from_fullmv(&best_mv);
  unsigned int unused;

  return vfp->svaf(get_buf_from_fullmv(pre, &best_mv), pre->stride, 0, 0,
                   src->buf, src->stride, &unused, second_pred) +
         mv_err_cost_(&mv, mv_cost_params);
}

static INLINE int get_mvpred_mask_var(
    const MV_COST_PARAMS *mv_cost_params, const FULLPEL_MV best_mv,
    const uint8_t *second_pred, const uint8_t *mask, int mask_stride,
    int invert_mask, const aom_variance_fn_ptr_t *vfp, const struct buf_2d *src,
    const struct buf_2d *pre) {
  const MV mv = get_mv_from_fullmv(&best_mv);
  unsigned int unused;

  return vfp->msvf(get_buf_from_fullmv(pre, &best_mv), pre->stride, 0, 0,
                   src->buf, src->stride, second_pred, mask, mask_stride,
                   invert_mask, &unused) +
         mv_err_cost_(&mv, mv_cost_params);
}

int av1_get_mvpred_compound_var(const MV_COST_PARAMS *mv_cost_params,
                                const FULLPEL_MV best_mv,
                                const uint8_t *second_pred, const uint8_t *mask,
                                int mask_stride, int invert_mask,
                                const aom_variance_fn_ptr_t *vfp,
                                const struct buf_2d *src,
                                const struct buf_2d *pre) {
  if (mask) {
    return get_mvpred_mask_var(mv_cost_params, best_mv, second_pred, mask,
                               mask_stride, invert_mask, vfp, src, pre);
  } else {
    return get_mvpred_av_var(mv_cost_params, best_mv, second_pred, vfp, src,
                             pre);
  }
}
