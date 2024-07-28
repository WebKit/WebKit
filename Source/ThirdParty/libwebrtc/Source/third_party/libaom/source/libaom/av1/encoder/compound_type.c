/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/pred_common.h"
#include "av1/encoder/compound_type.h"
#include "av1/encoder/encoder_alloc.h"
#include "av1/encoder/model_rd.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/rdopt_utils.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/tx_search.h"

typedef int64_t (*pick_interinter_mask_type)(
    const AV1_COMP *const cpi, MACROBLOCK *x, const BLOCK_SIZE bsize,
    const uint8_t *const p0, const uint8_t *const p1,
    const int16_t *const residual1, const int16_t *const diff10,
    uint64_t *best_sse);

// Checks if characteristics of search match
static INLINE int is_comp_rd_match(const AV1_COMP *const cpi,
                                   const MACROBLOCK *const x,
                                   const COMP_RD_STATS *st,
                                   const MB_MODE_INFO *const mi,
                                   int32_t *comp_rate, int64_t *comp_dist,
                                   int32_t *comp_model_rate,
                                   int64_t *comp_model_dist, int *comp_rs2) {
  // TODO(ranjit): Ensure that compound type search use regular filter always
  // and check if following check can be removed
  // Check if interp filter matches with previous case
  if (st->filter.as_int != mi->interp_filters.as_int) return 0;

  const MACROBLOCKD *const xd = &x->e_mbd;
  // Match MV and reference indices
  for (int i = 0; i < 2; ++i) {
    if ((st->ref_frames[i] != mi->ref_frame[i]) ||
        (st->mv[i].as_int != mi->mv[i].as_int)) {
      return 0;
    }
    const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[i]];
    if (is_global_mv_block(mi, wm->wmtype) != st->is_global[i]) return 0;
  }

  int reuse_data[COMPOUND_TYPES] = { 1, 1, 0, 0 };
  // For compound wedge, reuse data if newmv search is disabled when NEWMV is
  // present or if NEWMV is not present in either of the directions
  if ((!have_newmv_in_inter_mode(mi->mode) &&
       !have_newmv_in_inter_mode(st->mode)) ||
      (cpi->sf.inter_sf.disable_interinter_wedge_newmv_search))
    reuse_data[COMPOUND_WEDGE] = 1;
  // For compound diffwtd, reuse data if fast search is enabled (no newmv search
  // when NEWMV is present) or if NEWMV is not present in either of the
  // directions
  if (cpi->sf.inter_sf.enable_fast_compound_mode_search ||
      (!have_newmv_in_inter_mode(mi->mode) &&
       !have_newmv_in_inter_mode(st->mode)))
    reuse_data[COMPOUND_DIFFWTD] = 1;

  // Store the stats for the different compound types
  for (int comp_type = COMPOUND_AVERAGE; comp_type < COMPOUND_TYPES;
       comp_type++) {
    if (reuse_data[comp_type]) {
      comp_rate[comp_type] = st->rate[comp_type];
      comp_dist[comp_type] = st->dist[comp_type];
      comp_model_rate[comp_type] = st->model_rate[comp_type];
      comp_model_dist[comp_type] = st->model_dist[comp_type];
      comp_rs2[comp_type] = st->comp_rs2[comp_type];
    }
  }
  return 1;
}

// Checks if similar compound type search case is accounted earlier
// If found, returns relevant rd data
static INLINE int find_comp_rd_in_stats(const AV1_COMP *const cpi,
                                        const MACROBLOCK *x,
                                        const MB_MODE_INFO *const mbmi,
                                        int32_t *comp_rate, int64_t *comp_dist,
                                        int32_t *comp_model_rate,
                                        int64_t *comp_model_dist, int *comp_rs2,
                                        int *match_index) {
  for (int j = 0; j < x->comp_rd_stats_idx; ++j) {
    if (is_comp_rd_match(cpi, x, &x->comp_rd_stats[j], mbmi, comp_rate,
                         comp_dist, comp_model_rate, comp_model_dist,
                         comp_rs2)) {
      *match_index = j;
      return 1;
    }
  }
  return 0;  // no match result found
}

static INLINE bool enable_wedge_search(
    MACROBLOCK *const x, const unsigned int disable_wedge_var_thresh) {
  // Enable wedge search if source variance and edge strength are above
  // the thresholds.
  return x->source_variance > disable_wedge_var_thresh;
}

static INLINE bool enable_wedge_interinter_search(MACROBLOCK *const x,
                                                  const AV1_COMP *const cpi) {
  return enable_wedge_search(
             x, cpi->sf.inter_sf.disable_interinter_wedge_var_thresh) &&
         cpi->oxcf.comp_type_cfg.enable_interinter_wedge;
}

static INLINE bool enable_wedge_interintra_search(MACROBLOCK *const x,
                                                  const AV1_COMP *const cpi) {
  return enable_wedge_search(
             x, cpi->sf.inter_sf.disable_interintra_wedge_var_thresh) &&
         cpi->oxcf.comp_type_cfg.enable_interintra_wedge;
}

static int8_t estimate_wedge_sign(const AV1_COMP *cpi, const MACROBLOCK *x,
                                  const BLOCK_SIZE bsize, const uint8_t *pred0,
                                  int stride0, const uint8_t *pred1,
                                  int stride1) {
  static const BLOCK_SIZE split_qtr[BLOCK_SIZES_ALL] = {
    //                            4X4
    BLOCK_INVALID,
    // 4X8,        8X4,           8X8
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4,
    // 8X16,       16X8,          16X16
    BLOCK_4X8, BLOCK_8X4, BLOCK_8X8,
    // 16X32,      32X16,         32X32
    BLOCK_8X16, BLOCK_16X8, BLOCK_16X16,
    // 32X64,      64X32,         64X64
    BLOCK_16X32, BLOCK_32X16, BLOCK_32X32,
    // 64x128,     128x64,        128x128
    BLOCK_32X64, BLOCK_64X32, BLOCK_64X64,
    // 4X16,       16X4,          8X32
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
    // 32X8,       16X64,         64X16
    BLOCK_16X4, BLOCK_8X32, BLOCK_32X8
  };
  const struct macroblock_plane *const p = &x->plane[0];
  const uint8_t *src = p->src.buf;
  int src_stride = p->src.stride;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int bw_by2 = bw >> 1;
  const int bh_by2 = bh >> 1;
  uint32_t esq[2][2];
  int64_t tl, br;

  const BLOCK_SIZE f_index = split_qtr[bsize];
  assert(f_index != BLOCK_INVALID);

  if (is_cur_buf_hbd(&x->e_mbd)) {
    pred0 = CONVERT_TO_BYTEPTR(pred0);
    pred1 = CONVERT_TO_BYTEPTR(pred1);
  }

  // Residual variance computation over relevant quandrants in order to
  // find TL + BR, TL = sum(1st,2nd,3rd) quadrants of (pred0 - pred1),
  // BR = sum(2nd,3rd,4th) quadrants of (pred1 - pred0)
  // The 2nd and 3rd quadrants cancel out in TL + BR
  // Hence TL + BR = 1st quadrant of (pred0-pred1) + 4th of (pred1-pred0)
  // TODO(nithya): Sign estimation assumes 45 degrees (1st and 4th quadrants)
  // for all codebooks; experiment with other quadrant combinations for
  // 0, 90 and 135 degrees also.
  cpi->ppi->fn_ptr[f_index].vf(src, src_stride, pred0, stride0, &esq[0][0]);
  cpi->ppi->fn_ptr[f_index].vf(src + bh_by2 * src_stride + bw_by2, src_stride,
                               pred0 + bh_by2 * stride0 + bw_by2, stride0,
                               &esq[0][1]);
  cpi->ppi->fn_ptr[f_index].vf(src, src_stride, pred1, stride1, &esq[1][0]);
  cpi->ppi->fn_ptr[f_index].vf(src + bh_by2 * src_stride + bw_by2, src_stride,
                               pred1 + bh_by2 * stride1 + bw_by2, stride0,
                               &esq[1][1]);

  tl = ((int64_t)esq[0][0]) - ((int64_t)esq[1][0]);
  br = ((int64_t)esq[1][1]) - ((int64_t)esq[0][1]);
  return (tl + br > 0);
}

// Choose the best wedge index and sign
static int64_t pick_wedge(const AV1_COMP *const cpi, const MACROBLOCK *const x,
                          const BLOCK_SIZE bsize, const uint8_t *const p0,
                          const int16_t *const residual1,
                          const int16_t *const diff10,
                          int8_t *const best_wedge_sign,
                          int8_t *const best_wedge_index, uint64_t *best_sse) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  assert(N >= 64);
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int8_t wedge_index;
  int8_t wedge_sign;
  const int8_t wedge_types = get_wedge_types_lookup(bsize);
  const uint8_t *mask;
  uint64_t sse;
  const int hbd = is_cur_buf_hbd(xd);
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;

  DECLARE_ALIGNED(32, int16_t, residual0[MAX_SB_SQUARE]);  // src - pred0
#if CONFIG_AV1_HIGHBITDEPTH
  if (hbd) {
    aom_highbd_subtract_block(bh, bw, residual0, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p0), bw);
  } else {
    aom_subtract_block(bh, bw, residual0, bw, src->buf, src->stride, p0, bw);
  }
#else
  (void)hbd;
  aom_subtract_block(bh, bw, residual0, bw, src->buf, src->stride, p0, bw);
#endif

  int64_t sign_limit = ((int64_t)aom_sum_squares_i16(residual0, N) -
                        (int64_t)aom_sum_squares_i16(residual1, N)) *
                       (1 << WEDGE_WEIGHT_BITS) / 2;
  int16_t *ds = residual0;

  av1_wedge_compute_delta_squares(ds, residual0, residual1, N);

  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, 0, bsize);

    wedge_sign = av1_wedge_sign_from_residuals(ds, mask, N, sign_limit);

    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    sse = av1_wedge_sse_from_residuals(residual1, diff10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_sse_fn[MODELRD_TYPE_MASKED_COMPOUND](cpi, x, bsize, 0, sse, N,
                                                  &rate, &dist);
    // int rate2;
    // int64_t dist2;
    // model_rd_with_curvfit(cpi, x, bsize, 0, sse, N, &rate2, &dist2);
    // printf("sse %"PRId64": leagacy: %d %"PRId64", curvfit %d %"PRId64"\n",
    // sse, rate, dist, rate2, dist2); dist = dist2;
    // rate = rate2;

    rate += x->mode_costs.wedge_idx_cost[bsize][wedge_index];
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      *best_wedge_sign = wedge_sign;
      best_rd = rd;
      *best_sse = sse;
    }
  }

  return best_rd -
         RDCOST(x->rdmult,
                x->mode_costs.wedge_idx_cost[bsize][*best_wedge_index], 0);
}

// Choose the best wedge index the specified sign
static int64_t pick_wedge_fixed_sign(
    const AV1_COMP *const cpi, const MACROBLOCK *const x,
    const BLOCK_SIZE bsize, const int16_t *const residual1,
    const int16_t *const diff10, const int8_t wedge_sign,
    int8_t *const best_wedge_index, uint64_t *best_sse) {
  const MACROBLOCKD *const xd = &x->e_mbd;

  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = bw * bh;
  assert(N >= 64);
  int rate;
  int64_t dist;
  int64_t rd, best_rd = INT64_MAX;
  int8_t wedge_index;
  const int8_t wedge_types = get_wedge_types_lookup(bsize);
  const uint8_t *mask;
  uint64_t sse;
  const int hbd = is_cur_buf_hbd(xd);
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
  for (wedge_index = 0; wedge_index < wedge_types; ++wedge_index) {
    mask = av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
    sse = av1_wedge_sse_from_residuals(residual1, diff10, mask, N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_sse_fn[MODELRD_TYPE_MASKED_COMPOUND](cpi, x, bsize, 0, sse, N,
                                                  &rate, &dist);
    rate += x->mode_costs.wedge_idx_cost[bsize][wedge_index];
    rd = RDCOST(x->rdmult, rate, dist);

    if (rd < best_rd) {
      *best_wedge_index = wedge_index;
      best_rd = rd;
      *best_sse = sse;
    }
  }
  return best_rd -
         RDCOST(x->rdmult,
                x->mode_costs.wedge_idx_cost[bsize][*best_wedge_index], 0);
}

static int64_t pick_interinter_wedge(
    const AV1_COMP *const cpi, MACROBLOCK *const x, const BLOCK_SIZE bsize,
    const uint8_t *const p0, const uint8_t *const p1,
    const int16_t *const residual1, const int16_t *const diff10,
    uint64_t *best_sse) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int bw = block_size_wide[bsize];

  int64_t rd;
  int8_t wedge_index = -1;
  int8_t wedge_sign = 0;

  assert(is_interinter_compound_used(COMPOUND_WEDGE, bsize));
  assert(cpi->common.seq_params->enable_masked_compound);

  if (cpi->sf.inter_sf.fast_wedge_sign_estimate) {
    wedge_sign = estimate_wedge_sign(cpi, x, bsize, p0, bw, p1, bw);
    rd = pick_wedge_fixed_sign(cpi, x, bsize, residual1, diff10, wedge_sign,
                               &wedge_index, best_sse);
  } else {
    rd = pick_wedge(cpi, x, bsize, p0, residual1, diff10, &wedge_sign,
                    &wedge_index, best_sse);
  }

  mbmi->interinter_comp.wedge_sign = wedge_sign;
  mbmi->interinter_comp.wedge_index = wedge_index;
  return rd;
}

static int64_t pick_interinter_seg(const AV1_COMP *const cpi,
                                   MACROBLOCK *const x, const BLOCK_SIZE bsize,
                                   const uint8_t *const p0,
                                   const uint8_t *const p1,
                                   const int16_t *const residual1,
                                   const int16_t *const diff10,
                                   uint64_t *best_sse) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int N = 1 << num_pels_log2_lookup[bsize];
  int rate;
  int64_t dist;
  DIFFWTD_MASK_TYPE cur_mask_type;
  int64_t best_rd = INT64_MAX;
  DIFFWTD_MASK_TYPE best_mask_type = 0;
  const int hbd = is_cur_buf_hbd(xd);
  const int bd_round = hbd ? (xd->bd - 8) * 2 : 0;
  DECLARE_ALIGNED(16, uint8_t, seg_mask[2 * MAX_SB_SQUARE]);
  uint8_t *tmp_mask[2] = { xd->seg_mask, seg_mask };
  // try each mask type and its inverse
  for (cur_mask_type = 0; cur_mask_type < DIFFWTD_MASK_TYPES; cur_mask_type++) {
    // build mask and inverse
#if CONFIG_AV1_HIGHBITDEPTH
    if (hbd)
      av1_build_compound_diffwtd_mask_highbd(
          tmp_mask[cur_mask_type], cur_mask_type, CONVERT_TO_BYTEPTR(p0), bw,
          CONVERT_TO_BYTEPTR(p1), bw, bh, bw, xd->bd);
    else
      av1_build_compound_diffwtd_mask(tmp_mask[cur_mask_type], cur_mask_type,
                                      p0, bw, p1, bw, bh, bw);
#else
    (void)hbd;
    av1_build_compound_diffwtd_mask(tmp_mask[cur_mask_type], cur_mask_type, p0,
                                    bw, p1, bw, bh, bw);
#endif  // CONFIG_AV1_HIGHBITDEPTH

    // compute rd for mask
    uint64_t sse = av1_wedge_sse_from_residuals(residual1, diff10,
                                                tmp_mask[cur_mask_type], N);
    sse = ROUND_POWER_OF_TWO(sse, bd_round);

    model_rd_sse_fn[MODELRD_TYPE_MASKED_COMPOUND](cpi, x, bsize, 0, sse, N,
                                                  &rate, &dist);
    const int64_t rd0 = RDCOST(x->rdmult, rate, dist);

    if (rd0 < best_rd) {
      best_mask_type = cur_mask_type;
      best_rd = rd0;
      *best_sse = sse;
    }
  }
  mbmi->interinter_comp.mask_type = best_mask_type;
  if (best_mask_type == DIFFWTD_38_INV) {
    memcpy(xd->seg_mask, seg_mask, N * 2);
  }
  return best_rd;
}

static int64_t pick_interintra_wedge(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const BLOCK_SIZE bsize,
                                     const uint8_t *const p0,
                                     const uint8_t *const p1) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(av1_is_wedge_used(bsize));
  assert(cpi->common.seq_params->enable_interintra_compound);

  const struct buf_2d *const src = &x->plane[0].src;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  DECLARE_ALIGNED(32, int16_t, residual1[MAX_SB_SQUARE]);  // src - pred1
  DECLARE_ALIGNED(32, int16_t, diff10[MAX_SB_SQUARE]);     // pred1 - pred0
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    aom_highbd_subtract_block(bh, bw, residual1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(p1), bw);
    aom_highbd_subtract_block(bh, bw, diff10, bw, CONVERT_TO_BYTEPTR(p1), bw,
                              CONVERT_TO_BYTEPTR(p0), bw);
  } else {
    aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, p1, bw);
    aom_subtract_block(bh, bw, diff10, bw, p1, bw, p0, bw);
  }
#else
  aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, p1, bw);
  aom_subtract_block(bh, bw, diff10, bw, p1, bw, p0, bw);
#endif
  int8_t wedge_index = -1;
  uint64_t sse;
  int64_t rd = pick_wedge_fixed_sign(cpi, x, bsize, residual1, diff10, 0,
                                     &wedge_index, &sse);

  mbmi->interintra_wedge_index = wedge_index;
  return rd;
}

static AOM_INLINE void get_inter_predictors_masked_compound(
    MACROBLOCK *x, const BLOCK_SIZE bsize, uint8_t **preds0, uint8_t **preds1,
    int16_t *residual1, int16_t *diff10, int *strides) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  // get inter predictors to use for masked compound modes
  av1_build_inter_predictors_for_planes_single_buf(xd, bsize, 0, 0, 0, preds0,
                                                   strides);
  av1_build_inter_predictors_for_planes_single_buf(xd, bsize, 0, 0, 1, preds1,
                                                   strides);
  const struct buf_2d *const src = &x->plane[0].src;
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_cur_buf_hbd(xd)) {
    aom_highbd_subtract_block(bh, bw, residual1, bw, src->buf, src->stride,
                              CONVERT_TO_BYTEPTR(*preds1), bw);
    aom_highbd_subtract_block(bh, bw, diff10, bw, CONVERT_TO_BYTEPTR(*preds1),
                              bw, CONVERT_TO_BYTEPTR(*preds0), bw);
  } else {
    aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, *preds1,
                       bw);
    aom_subtract_block(bh, bw, diff10, bw, *preds1, bw, *preds0, bw);
  }
#else
  aom_subtract_block(bh, bw, residual1, bw, src->buf, src->stride, *preds1, bw);
  aom_subtract_block(bh, bw, diff10, bw, *preds1, bw, *preds0, bw);
#endif
}

// Computes the rd cost for the given interintra mode and updates the best
static INLINE void compute_best_interintra_mode(
    const AV1_COMP *const cpi, MB_MODE_INFO *mbmi, MACROBLOCKD *xd,
    MACROBLOCK *const x, const int *const interintra_mode_cost,
    const BUFFER_SET *orig_dst, uint8_t *intrapred, const uint8_t *tmp_buf,
    INTERINTRA_MODE *best_interintra_mode, int64_t *best_interintra_rd,
    INTERINTRA_MODE interintra_mode, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  int rate;
  uint8_t skip_txfm_sb;
  int64_t dist, skip_sse_sb;
  const int bw = block_size_wide[bsize];
  mbmi->interintra_mode = interintra_mode;
  int rmode = interintra_mode_cost[interintra_mode];
  av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                            intrapred, bw);
  av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
  model_rd_sb_fn[MODELRD_TYPE_INTERINTRA](cpi, bsize, x, xd, 0, 0, &rate, &dist,
                                          &skip_txfm_sb, &skip_sse_sb, NULL,
                                          NULL, NULL);
  int64_t rd = RDCOST(x->rdmult, rate + rmode, dist);
  if (rd < *best_interintra_rd) {
    *best_interintra_rd = rd;
    *best_interintra_mode = mbmi->interintra_mode;
  }
}

static int64_t estimate_yrd_for_sb(const AV1_COMP *const cpi, BLOCK_SIZE bs,
                                   MACROBLOCK *x, int64_t ref_best_rd,
                                   RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  if (ref_best_rd < 0) return INT64_MAX;
  av1_subtract_plane(x, bs, 0);
  const int64_t rd = av1_estimate_txfm_yrd(cpi, x, rd_stats, ref_best_rd, bs,
                                           max_txsize_rect_lookup[bs]);
  if (rd != INT64_MAX) {
    const int skip_ctx = av1_get_skip_txfm_context(xd);
    if (rd_stats->skip_txfm) {
      const int s1 = x->mode_costs.skip_txfm_cost[skip_ctx][1];
      rd_stats->rate = s1;
    } else {
      const int s0 = x->mode_costs.skip_txfm_cost[skip_ctx][0];
      rd_stats->rate += s0;
    }
  }
  return rd;
}

// Computes the rd_threshold for smooth interintra rd search.
static AOM_INLINE int64_t compute_rd_thresh(MACROBLOCK *const x,
                                            int total_mode_rate,
                                            int64_t ref_best_rd) {
  const int64_t rd_thresh = get_rd_thresh_from_best_rd(
      ref_best_rd, (1 << INTER_INTRA_RD_THRESH_SHIFT),
      INTER_INTRA_RD_THRESH_SCALE);
  const int64_t mode_rd = RDCOST(x->rdmult, total_mode_rate, 0);
  return (rd_thresh - mode_rd);
}

// Computes the best wedge interintra mode
static AOM_INLINE int64_t compute_best_wedge_interintra(
    const AV1_COMP *const cpi, MB_MODE_INFO *mbmi, MACROBLOCKD *xd,
    MACROBLOCK *const x, const int *const interintra_mode_cost,
    const BUFFER_SET *orig_dst, uint8_t *intrapred_, uint8_t *tmp_buf_,
    int *best_mode, int *best_wedge_index, BLOCK_SIZE bsize) {
  const AV1_COMMON *const cm = &cpi->common;
  const int bw = block_size_wide[bsize];
  int64_t best_interintra_rd_wedge = INT64_MAX;
  int64_t best_total_rd = INT64_MAX;
  uint8_t *intrapred = get_buf_by_bd(xd, intrapred_);
  for (INTERINTRA_MODE mode = 0; mode < INTERINTRA_MODES; ++mode) {
    mbmi->interintra_mode = mode;
    av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                              intrapred, bw);
    int64_t rd = pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);
    const int rate_overhead =
        interintra_mode_cost[mode] +
        x->mode_costs.wedge_idx_cost[bsize][mbmi->interintra_wedge_index];
    const int64_t total_rd = rd + RDCOST(x->rdmult, rate_overhead, 0);
    if (total_rd < best_total_rd) {
      best_total_rd = total_rd;
      best_interintra_rd_wedge = rd;
      *best_mode = mbmi->interintra_mode;
      *best_wedge_index = mbmi->interintra_wedge_index;
    }
  }
  return best_interintra_rd_wedge;
}

static int handle_smooth_inter_intra_mode(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    MB_MODE_INFO *mbmi, int64_t ref_best_rd, int *rate_mv,
    INTERINTRA_MODE *best_interintra_mode, int64_t *best_rd,
    int *best_mode_rate, const BUFFER_SET *orig_dst, uint8_t *tmp_buf,
    uint8_t *intrapred, HandleInterModeArgs *args) {
  MACROBLOCKD *xd = &x->e_mbd;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const interintra_mode_cost =
      mode_costs->interintra_mode_cost[size_group_lookup[bsize]];
  const AV1_COMMON *const cm = &cpi->common;
  const int bw = block_size_wide[bsize];

  mbmi->use_wedge_interintra = 0;

  if (cpi->sf.inter_sf.reuse_inter_intra_mode == 0 ||
      *best_interintra_mode == INTERINTRA_MODES) {
    int64_t best_interintra_rd = INT64_MAX;
    for (INTERINTRA_MODE cur_mode = 0; cur_mode < INTERINTRA_MODES;
         ++cur_mode) {
      if ((!cpi->oxcf.intra_mode_cfg.enable_smooth_intra ||
           cpi->sf.intra_sf.disable_smooth_intra) &&
          cur_mode == II_SMOOTH_PRED)
        continue;
      compute_best_interintra_mode(
          cpi, mbmi, xd, x, interintra_mode_cost, orig_dst, intrapred, tmp_buf,
          best_interintra_mode, &best_interintra_rd, cur_mode, bsize);
    }
    args->inter_intra_mode[mbmi->ref_frame[0]] = *best_interintra_mode;
  }
  assert(IMPLIES(!cpi->oxcf.comp_type_cfg.enable_smooth_interintra,
                 *best_interintra_mode != II_SMOOTH_PRED));
  // Recompute prediction if required
  bool interintra_mode_reuse = cpi->sf.inter_sf.reuse_inter_intra_mode ||
                               *best_interintra_mode != INTERINTRA_MODES;
  if (interintra_mode_reuse || *best_interintra_mode != INTERINTRA_MODES - 1) {
    mbmi->interintra_mode = *best_interintra_mode;
    av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                              intrapred, bw);
    av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
  }

  // Compute rd cost for best smooth_interintra
  RD_STATS rd_stats;
  const int is_wedge_used = av1_is_wedge_used(bsize);
  const int rmode =
      interintra_mode_cost[*best_interintra_mode] +
      (is_wedge_used ? mode_costs->wedge_interintra_cost[bsize][0] : 0);
  const int total_mode_rate = rmode + *rate_mv;
  const int64_t rd_thresh = compute_rd_thresh(x, total_mode_rate, ref_best_rd);
  int64_t rd = estimate_yrd_for_sb(cpi, bsize, x, rd_thresh, &rd_stats);
  if (rd != INT64_MAX) {
    rd = RDCOST(x->rdmult, total_mode_rate + rd_stats.rate, rd_stats.dist);
  } else {
    return IGNORE_MODE;
  }
  *best_rd = rd;
  *best_mode_rate = rmode;
  // Return early if best rd not good enough
  if (ref_best_rd < INT64_MAX &&
      (*best_rd >> INTER_INTRA_RD_THRESH_SHIFT) * INTER_INTRA_RD_THRESH_SCALE >
          ref_best_rd) {
    return IGNORE_MODE;
  }
  return 0;
}

static int handle_wedge_inter_intra_mode(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    MB_MODE_INFO *mbmi, int *rate_mv, INTERINTRA_MODE *best_interintra_mode,
    int64_t *best_rd, const BUFFER_SET *orig_dst, uint8_t *tmp_buf_,
    uint8_t *tmp_buf, uint8_t *intrapred_, uint8_t *intrapred,
    HandleInterModeArgs *args, int *tmp_rate_mv, int *rate_overhead,
    int_mv *tmp_mv, int64_t best_rd_no_wedge) {
  MACROBLOCKD *xd = &x->e_mbd;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const interintra_mode_cost =
      mode_costs->interintra_mode_cost[size_group_lookup[bsize]];
  const AV1_COMMON *const cm = &cpi->common;
  const int bw = block_size_wide[bsize];
  const int try_smooth_interintra =
      cpi->oxcf.comp_type_cfg.enable_smooth_interintra;

  mbmi->use_wedge_interintra = 1;

  if (!cpi->sf.inter_sf.fast_interintra_wedge_search) {
    // Exhaustive search of all wedge and mode combinations.
    int best_mode = 0;
    int best_wedge_index = 0;
    *best_rd = compute_best_wedge_interintra(
        cpi, mbmi, xd, x, interintra_mode_cost, orig_dst, intrapred_, tmp_buf_,
        &best_mode, &best_wedge_index, bsize);
    mbmi->interintra_mode = best_mode;
    mbmi->interintra_wedge_index = best_wedge_index;
    if (best_mode != INTERINTRA_MODES - 1) {
      av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                intrapred, bw);
    }
  } else if (!try_smooth_interintra) {
    if (*best_interintra_mode == INTERINTRA_MODES) {
      mbmi->interintra_mode = INTERINTRA_MODES - 1;
      *best_interintra_mode = INTERINTRA_MODES - 1;
      av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                intrapred, bw);
      // Pick wedge mask based on INTERINTRA_MODES - 1
      *best_rd = pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);
      // Find the best interintra mode for the chosen wedge mask
      for (INTERINTRA_MODE cur_mode = 0; cur_mode < INTERINTRA_MODES;
           ++cur_mode) {
        compute_best_interintra_mode(
            cpi, mbmi, xd, x, interintra_mode_cost, orig_dst, intrapred,
            tmp_buf, best_interintra_mode, best_rd, cur_mode, bsize);
      }
      args->inter_intra_mode[mbmi->ref_frame[0]] = *best_interintra_mode;
      mbmi->interintra_mode = *best_interintra_mode;

      // Recompute prediction if required
      if (*best_interintra_mode != INTERINTRA_MODES - 1) {
        av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                  intrapred, bw);
      }
    } else {
      // Pick wedge mask for the best interintra mode (reused)
      mbmi->interintra_mode = *best_interintra_mode;
      av1_build_intra_predictors_for_interintra(cm, xd, bsize, 0, orig_dst,
                                                intrapred, bw);
      *best_rd = pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);
    }
  } else {
    // Pick wedge mask for the best interintra mode from smooth_interintra
    *best_rd = pick_interintra_wedge(cpi, x, bsize, intrapred_, tmp_buf_);
  }

  *rate_overhead =
      interintra_mode_cost[mbmi->interintra_mode] +
      mode_costs->wedge_idx_cost[bsize][mbmi->interintra_wedge_index] +
      mode_costs->wedge_interintra_cost[bsize][1];
  *best_rd += RDCOST(x->rdmult, *rate_overhead + *rate_mv, 0);

  int64_t rd = INT64_MAX;
  const int_mv mv0 = mbmi->mv[0];
  // Refine motion vector for NEWMV case.
  if (have_newmv_in_inter_mode(mbmi->mode)) {
    int rate_sum;
    uint8_t skip_txfm_sb;
    int64_t dist_sum, skip_sse_sb;
    // get negative of mask
    const uint8_t *mask =
        av1_get_contiguous_soft_mask(mbmi->interintra_wedge_index, 1, bsize);
    av1_compound_single_motion_search(cpi, x, bsize, &tmp_mv->as_mv, intrapred,
                                      mask, bw, tmp_rate_mv, 0);
    if (mbmi->mv[0].as_int != tmp_mv->as_int) {
      mbmi->mv[0].as_int = tmp_mv->as_int;
      // Set ref_frame[1] to NONE_FRAME temporarily so that the intra
      // predictor is not calculated again in av1_enc_build_inter_predictor().
      mbmi->ref_frame[1] = NONE_FRAME;
      const int mi_row = xd->mi_row;
      const int mi_col = xd->mi_col;
      av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                    AOM_PLANE_Y, AOM_PLANE_Y);
      mbmi->ref_frame[1] = INTRA_FRAME;
      av1_combine_interintra(xd, bsize, 0, xd->plane[AOM_PLANE_Y].dst.buf,
                             xd->plane[AOM_PLANE_Y].dst.stride, intrapred, bw);
      model_rd_sb_fn[MODELRD_TYPE_MASKED_COMPOUND](
          cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum, &skip_txfm_sb,
          &skip_sse_sb, NULL, NULL, NULL);
      rd =
          RDCOST(x->rdmult, *tmp_rate_mv + *rate_overhead + rate_sum, dist_sum);
    }
  }
  if (rd >= *best_rd) {
    tmp_mv->as_int = mv0.as_int;
    *tmp_rate_mv = *rate_mv;
    av1_combine_interintra(xd, bsize, 0, tmp_buf, bw, intrapred, bw);
  }
  // Evaluate closer to true rd
  RD_STATS rd_stats;
  const int64_t mode_rd = RDCOST(x->rdmult, *rate_overhead + *tmp_rate_mv, 0);
  const int64_t tmp_rd_thresh = best_rd_no_wedge - mode_rd;
  rd = estimate_yrd_for_sb(cpi, bsize, x, tmp_rd_thresh, &rd_stats);
  if (rd != INT64_MAX) {
    rd = RDCOST(x->rdmult, *rate_overhead + *tmp_rate_mv + rd_stats.rate,
                rd_stats.dist);
  } else {
    if (*best_rd == INT64_MAX) return IGNORE_MODE;
  }
  *best_rd = rd;
  return 0;
}

int av1_handle_inter_intra_mode(const AV1_COMP *const cpi, MACROBLOCK *const x,
                                BLOCK_SIZE bsize, MB_MODE_INFO *mbmi,
                                HandleInterModeArgs *args, int64_t ref_best_rd,
                                int *rate_mv, int *tmp_rate2,
                                const BUFFER_SET *orig_dst) {
  const int try_smooth_interintra =
      cpi->oxcf.comp_type_cfg.enable_smooth_interintra;

  const int is_wedge_used = av1_is_wedge_used(bsize);
  const int try_wedge_interintra =
      is_wedge_used && enable_wedge_interintra_search(x, cpi);

  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const int bw = block_size_wide[bsize];
  DECLARE_ALIGNED(16, uint8_t, tmp_buf_[2 * MAX_INTERINTRA_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, intrapred_[2 * MAX_INTERINTRA_SB_SQUARE]);
  uint8_t *tmp_buf = get_buf_by_bd(xd, tmp_buf_);
  uint8_t *intrapred = get_buf_by_bd(xd, intrapred_);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  // Single reference inter prediction
  mbmi->ref_frame[1] = NONE_FRAME;
  xd->plane[0].dst.buf = tmp_buf;
  xd->plane[0].dst.stride = bw;
  av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                AOM_PLANE_Y, AOM_PLANE_Y);
  const int num_planes = av1_num_planes(cm);

  // Restore the buffers for intra prediction
  restore_dst_buf(xd, *orig_dst, num_planes);
  mbmi->ref_frame[1] = INTRA_FRAME;
  INTERINTRA_MODE best_interintra_mode =
      args->inter_intra_mode[mbmi->ref_frame[0]];

  // Compute smooth_interintra
  int64_t best_interintra_rd_nowedge = INT64_MAX;
  int best_mode_rate = INT_MAX;
  if (try_smooth_interintra) {
    int ret = handle_smooth_inter_intra_mode(
        cpi, x, bsize, mbmi, ref_best_rd, rate_mv, &best_interintra_mode,
        &best_interintra_rd_nowedge, &best_mode_rate, orig_dst, tmp_buf,
        intrapred, args);
    if (ret == IGNORE_MODE) {
      return IGNORE_MODE;
    }
  }

  // Compute wedge interintra
  int64_t best_interintra_rd_wedge = INT64_MAX;
  const int_mv mv0 = mbmi->mv[0];
  int_mv tmp_mv = mv0;
  int tmp_rate_mv = 0;
  int rate_overhead = 0;
  if (try_wedge_interintra) {
    int ret = handle_wedge_inter_intra_mode(
        cpi, x, bsize, mbmi, rate_mv, &best_interintra_mode,
        &best_interintra_rd_wedge, orig_dst, tmp_buf_, tmp_buf, intrapred_,
        intrapred, args, &tmp_rate_mv, &rate_overhead, &tmp_mv,
        best_interintra_rd_nowedge);
    if (ret == IGNORE_MODE) {
      return IGNORE_MODE;
    }
  }

  if (best_interintra_rd_nowedge == INT64_MAX &&
      best_interintra_rd_wedge == INT64_MAX) {
    return IGNORE_MODE;
  }
  if (best_interintra_rd_wedge < best_interintra_rd_nowedge) {
    mbmi->mv[0].as_int = tmp_mv.as_int;
    *tmp_rate2 += tmp_rate_mv - *rate_mv;
    *rate_mv = tmp_rate_mv;
    best_mode_rate = rate_overhead;
  } else if (try_smooth_interintra && try_wedge_interintra) {
    // If smooth was best, but we over-wrote the values when evaluating the
    // wedge mode, we need to recompute the smooth values.
    mbmi->use_wedge_interintra = 0;
    mbmi->interintra_mode = best_interintra_mode;
    mbmi->mv[0].as_int = mv0.as_int;
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                  AOM_PLANE_Y, AOM_PLANE_Y);
  }
  *tmp_rate2 += best_mode_rate;

  if (num_planes > 1) {
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                  AOM_PLANE_U, num_planes - 1);
  }
  return 0;
}

// Computes the valid compound_types to be evaluated
static INLINE int compute_valid_comp_types(MACROBLOCK *x,
                                           const AV1_COMP *const cpi,
                                           BLOCK_SIZE bsize,
                                           int masked_compound_used,
                                           int mode_search_mask,
                                           COMPOUND_TYPE *valid_comp_types) {
  const AV1_COMMON *cm = &cpi->common;
  int valid_type_count = 0;
  int comp_type, valid_check;
  int8_t enable_masked_type[MASKED_COMPOUND_TYPES] = { 0, 0 };

  const int try_average_comp = (mode_search_mask & (1 << COMPOUND_AVERAGE));
  const int try_distwtd_comp =
      ((mode_search_mask & (1 << COMPOUND_DISTWTD)) &&
       cm->seq_params->order_hint_info.enable_dist_wtd_comp == 1 &&
       cpi->sf.inter_sf.use_dist_wtd_comp_flag != DIST_WTD_COMP_DISABLED);

  // Check if COMPOUND_AVERAGE and COMPOUND_DISTWTD are valid cases
  for (comp_type = COMPOUND_AVERAGE; comp_type <= COMPOUND_DISTWTD;
       comp_type++) {
    valid_check =
        (comp_type == COMPOUND_AVERAGE) ? try_average_comp : try_distwtd_comp;
    if (valid_check && is_interinter_compound_used(comp_type, bsize))
      valid_comp_types[valid_type_count++] = comp_type;
  }
  // Check if COMPOUND_WEDGE and COMPOUND_DIFFWTD are valid cases
  if (masked_compound_used) {
    // enable_masked_type[0] corresponds to COMPOUND_WEDGE
    // enable_masked_type[1] corresponds to COMPOUND_DIFFWTD
    enable_masked_type[0] = enable_wedge_interinter_search(x, cpi);
    enable_masked_type[1] = cpi->oxcf.comp_type_cfg.enable_diff_wtd_comp;
    for (comp_type = COMPOUND_WEDGE; comp_type <= COMPOUND_DIFFWTD;
         comp_type++) {
      if ((mode_search_mask & (1 << comp_type)) &&
          is_interinter_compound_used(comp_type, bsize) &&
          enable_masked_type[comp_type - COMPOUND_WEDGE])
        valid_comp_types[valid_type_count++] = comp_type;
    }
  }
  return valid_type_count;
}

// Calculates the cost for compound type mask
static INLINE void calc_masked_type_cost(
    const ModeCosts *mode_costs, BLOCK_SIZE bsize, int comp_group_idx_ctx,
    int comp_index_ctx, int masked_compound_used, int *masked_type_cost) {
  av1_zero_array(masked_type_cost, COMPOUND_TYPES);
  // Account for group index cost when wedge and/or diffwtd prediction are
  // enabled
  if (masked_compound_used) {
    // Compound group index of average and distwtd is 0
    // Compound group index of wedge and diffwtd is 1
    masked_type_cost[COMPOUND_AVERAGE] +=
        mode_costs->comp_group_idx_cost[comp_group_idx_ctx][0];
    masked_type_cost[COMPOUND_DISTWTD] += masked_type_cost[COMPOUND_AVERAGE];
    masked_type_cost[COMPOUND_WEDGE] +=
        mode_costs->comp_group_idx_cost[comp_group_idx_ctx][1];
    masked_type_cost[COMPOUND_DIFFWTD] += masked_type_cost[COMPOUND_WEDGE];
  }

  // Compute the cost to signal compound index/type
  masked_type_cost[COMPOUND_AVERAGE] +=
      mode_costs->comp_idx_cost[comp_index_ctx][1];
  masked_type_cost[COMPOUND_DISTWTD] +=
      mode_costs->comp_idx_cost[comp_index_ctx][0];
  masked_type_cost[COMPOUND_WEDGE] += mode_costs->compound_type_cost[bsize][0];
  masked_type_cost[COMPOUND_DIFFWTD] +=
      mode_costs->compound_type_cost[bsize][1];
}

// Updates mbmi structure with the relevant compound type info
static INLINE void update_mbmi_for_compound_type(MB_MODE_INFO *mbmi,
                                                 COMPOUND_TYPE cur_type) {
  mbmi->interinter_comp.type = cur_type;
  mbmi->comp_group_idx = (cur_type >= COMPOUND_WEDGE);
  mbmi->compound_idx = (cur_type != COMPOUND_DISTWTD);
}

// When match is found, populate the compound type data
// and calculate the rd cost using the stored stats and
// update the mbmi appropriately.
static INLINE int populate_reuse_comp_type_data(
    const MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BEST_COMP_TYPE_STATS *best_type_stats, int_mv *cur_mv, int32_t *comp_rate,
    int64_t *comp_dist, int *comp_rs2, int *rate_mv, int64_t *rd,
    int match_index) {
  const int winner_comp_type =
      x->comp_rd_stats[match_index].interinter_comp.type;
  if (comp_rate[winner_comp_type] == INT_MAX)
    return best_type_stats->best_compmode_interinter_cost;
  update_mbmi_for_compound_type(mbmi, winner_comp_type);
  mbmi->interinter_comp = x->comp_rd_stats[match_index].interinter_comp;
  *rd = RDCOST(
      x->rdmult,
      comp_rs2[winner_comp_type] + *rate_mv + comp_rate[winner_comp_type],
      comp_dist[winner_comp_type]);
  mbmi->mv[0].as_int = cur_mv[0].as_int;
  mbmi->mv[1].as_int = cur_mv[1].as_int;
  return comp_rs2[winner_comp_type];
}

// Updates rd cost and relevant compound type data for the best compound type
static INLINE void update_best_info(const MB_MODE_INFO *const mbmi, int64_t *rd,
                                    BEST_COMP_TYPE_STATS *best_type_stats,
                                    int64_t best_rd_cur,
                                    int64_t comp_model_rd_cur, int rs2) {
  *rd = best_rd_cur;
  best_type_stats->comp_best_model_rd = comp_model_rd_cur;
  best_type_stats->best_compound_data = mbmi->interinter_comp;
  best_type_stats->best_compmode_interinter_cost = rs2;
}

// Updates best_mv for masked compound types
static INLINE void update_mask_best_mv(const MB_MODE_INFO *const mbmi,
                                       int_mv *best_mv, int *best_tmp_rate_mv,
                                       int tmp_rate_mv) {
  *best_tmp_rate_mv = tmp_rate_mv;
  best_mv[0].as_int = mbmi->mv[0].as_int;
  best_mv[1].as_int = mbmi->mv[1].as_int;
}

static INLINE void save_comp_rd_search_stat(
    MACROBLOCK *x, const MB_MODE_INFO *const mbmi, const int32_t *comp_rate,
    const int64_t *comp_dist, const int32_t *comp_model_rate,
    const int64_t *comp_model_dist, const int_mv *cur_mv, const int *comp_rs2) {
  const int offset = x->comp_rd_stats_idx;
  if (offset < MAX_COMP_RD_STATS) {
    COMP_RD_STATS *const rd_stats = x->comp_rd_stats + offset;
    memcpy(rd_stats->rate, comp_rate, sizeof(rd_stats->rate));
    memcpy(rd_stats->dist, comp_dist, sizeof(rd_stats->dist));
    memcpy(rd_stats->model_rate, comp_model_rate, sizeof(rd_stats->model_rate));
    memcpy(rd_stats->model_dist, comp_model_dist, sizeof(rd_stats->model_dist));
    memcpy(rd_stats->comp_rs2, comp_rs2, sizeof(rd_stats->comp_rs2));
    memcpy(rd_stats->mv, cur_mv, sizeof(rd_stats->mv));
    memcpy(rd_stats->ref_frames, mbmi->ref_frame, sizeof(rd_stats->ref_frames));
    rd_stats->mode = mbmi->mode;
    rd_stats->filter = mbmi->interp_filters;
    rd_stats->ref_mv_idx = mbmi->ref_mv_idx;
    const MACROBLOCKD *const xd = &x->e_mbd;
    for (int i = 0; i < 2; ++i) {
      const WarpedMotionParams *const wm =
          &xd->global_motion[mbmi->ref_frame[i]];
      rd_stats->is_global[i] = is_global_mv_block(mbmi, wm->wmtype);
    }
    memcpy(&rd_stats->interinter_comp, &mbmi->interinter_comp,
           sizeof(rd_stats->interinter_comp));
    ++x->comp_rd_stats_idx;
  }
}

static INLINE int get_interinter_compound_mask_rate(
    const ModeCosts *const mode_costs, const MB_MODE_INFO *const mbmi) {
  const COMPOUND_TYPE compound_type = mbmi->interinter_comp.type;
  // This function will be called only for COMPOUND_WEDGE and COMPOUND_DIFFWTD
  if (compound_type == COMPOUND_WEDGE) {
    return av1_is_wedge_used(mbmi->bsize)
               ? av1_cost_literal(1) +
                     mode_costs
                         ->wedge_idx_cost[mbmi->bsize]
                                         [mbmi->interinter_comp.wedge_index]
               : 0;
  } else {
    assert(compound_type == COMPOUND_DIFFWTD);
    return av1_cost_literal(1);
  }
}

// Takes a backup of rate, distortion and model_rd for future reuse
static INLINE void backup_stats(COMPOUND_TYPE cur_type, int32_t *comp_rate,
                                int64_t *comp_dist, int32_t *comp_model_rate,
                                int64_t *comp_model_dist, int rate_sum,
                                int64_t dist_sum, RD_STATS *rd_stats,
                                int *comp_rs2, int rs2) {
  comp_rate[cur_type] = rd_stats->rate;
  comp_dist[cur_type] = rd_stats->dist;
  comp_model_rate[cur_type] = rate_sum;
  comp_model_dist[cur_type] = dist_sum;
  comp_rs2[cur_type] = rs2;
}

static INLINE int save_mask_search_results(const PREDICTION_MODE this_mode,
                                           const int reuse_level) {
  if (reuse_level || (this_mode == NEW_NEWMV))
    return 1;
  else
    return 0;
}

static INLINE int prune_mode_by_skip_rd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, MACROBLOCKD *xd,
                                        const BLOCK_SIZE bsize,
                                        int64_t ref_skip_rd, int mode_rate) {
  int eval_txfm = 1;
  const int txfm_rd_gate_level =
      get_txfm_rd_gate_level(cpi->common.seq_params->enable_masked_compound,
                             cpi->sf.inter_sf.txfm_rd_gate_level, bsize,
                             TX_SEARCH_COMP_TYPE_MODE, /*eval_motion_mode=*/0);
  // Check if the mode is good enough based on skip rd
  if (txfm_rd_gate_level) {
    int64_t sse_y = compute_sse_plane(x, xd, PLANE_TYPE_Y, bsize);
    int64_t skip_rd = RDCOST(x->rdmult, mode_rate, (sse_y << 4));
    eval_txfm =
        check_txfm_eval(x, bsize, ref_skip_rd, skip_rd, txfm_rd_gate_level, 1);
  }
  return eval_txfm;
}

static int64_t masked_compound_type_rd(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const BLOCK_SIZE bsize, const PREDICTION_MODE this_mode, int *rs2,
    int rate_mv, const BUFFER_SET *ctx, int *out_rate_mv, uint8_t **preds0,
    uint8_t **preds1, int16_t *residual1, int16_t *diff10, int *strides,
    int mode_rate, int64_t rd_thresh, int *calc_pred_masked_compound,
    int32_t *comp_rate, int64_t *comp_dist, int32_t *comp_model_rate,
    int64_t *comp_model_dist, const int64_t comp_best_model_rd,
    int64_t *const comp_model_rd_cur, int *comp_rs2, int64_t ref_skip_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int64_t best_rd_cur = INT64_MAX;
  int64_t rd = INT64_MAX;
  const COMPOUND_TYPE compound_type = mbmi->interinter_comp.type;
  // This function will be called only for COMPOUND_WEDGE and COMPOUND_DIFFWTD
  assert(compound_type == COMPOUND_WEDGE || compound_type == COMPOUND_DIFFWTD);
  int rate_sum;
  uint8_t tmp_skip_txfm_sb;
  int64_t dist_sum, tmp_skip_sse_sb;
  pick_interinter_mask_type pick_interinter_mask[2] = { pick_interinter_wedge,
                                                        pick_interinter_seg };

  // TODO(any): Save pred and mask calculation as well into records. However
  // this may increase memory requirements as compound segment mask needs to be
  // stored in each record.
  if (*calc_pred_masked_compound) {
    get_inter_predictors_masked_compound(x, bsize, preds0, preds1, residual1,
                                         diff10, strides);
    *calc_pred_masked_compound = 0;
  }
  if (compound_type == COMPOUND_WEDGE) {
    unsigned int sse;
    if (is_cur_buf_hbd(xd))
      (void)cpi->ppi->fn_ptr[bsize].vf(CONVERT_TO_BYTEPTR(*preds0), *strides,
                                       CONVERT_TO_BYTEPTR(*preds1), *strides,
                                       &sse);
    else
      (void)cpi->ppi->fn_ptr[bsize].vf(*preds0, *strides, *preds1, *strides,
                                       &sse);
    const unsigned int mse =
        ROUND_POWER_OF_TWO(sse, num_pels_log2_lookup[bsize]);
    // If two predictors are very similar, skip wedge compound mode search
    if (mse < 8 || (!have_newmv_in_inter_mode(this_mode) && mse < 64)) {
      *comp_model_rd_cur = INT64_MAX;
      return INT64_MAX;
    }
  }
  // Function pointer to pick the appropriate mask
  // compound_type == COMPOUND_WEDGE, calls pick_interinter_wedge()
  // compound_type == COMPOUND_DIFFWTD, calls pick_interinter_seg()
  uint64_t cur_sse = UINT64_MAX;
  best_rd_cur = pick_interinter_mask[compound_type - COMPOUND_WEDGE](
      cpi, x, bsize, *preds0, *preds1, residual1, diff10, &cur_sse);
  *rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);
  best_rd_cur += RDCOST(x->rdmult, *rs2 + rate_mv, 0);
  assert(cur_sse != UINT64_MAX);
  int64_t skip_rd_cur = RDCOST(x->rdmult, *rs2 + rate_mv, (cur_sse << 4));

  // Although the true rate_mv might be different after motion search, but it
  // is unlikely to be the best mode considering the transform rd cost and other
  // mode overhead cost
  int64_t mode_rd = RDCOST(x->rdmult, *rs2 + mode_rate, 0);
  if (mode_rd > rd_thresh) {
    *comp_model_rd_cur = INT64_MAX;
    return INT64_MAX;
  }

  // Check if the mode is good enough based on skip rd
  // TODO(nithya): Handle wedge_newmv_search if extending for lower speed
  // setting
  const int txfm_rd_gate_level =
      get_txfm_rd_gate_level(cm->seq_params->enable_masked_compound,
                             cpi->sf.inter_sf.txfm_rd_gate_level, bsize,
                             TX_SEARCH_COMP_TYPE_MODE, /*eval_motion_mode=*/0);
  if (txfm_rd_gate_level) {
    int eval_txfm = check_txfm_eval(x, bsize, ref_skip_rd, skip_rd_cur,
                                    txfm_rd_gate_level, 1);
    if (!eval_txfm) {
      *comp_model_rd_cur = INT64_MAX;
      return INT64_MAX;
    }
  }

  // Compute cost if matching record not found, else, reuse data
  if (comp_rate[compound_type] == INT_MAX) {
    // Check whether new MV search for wedge is to be done
    int wedge_newmv_search =
        have_newmv_in_inter_mode(this_mode) &&
        (compound_type == COMPOUND_WEDGE) &&
        (!cpi->sf.inter_sf.disable_interinter_wedge_newmv_search);

    // Search for new MV if needed and build predictor
    if (wedge_newmv_search) {
      *out_rate_mv = av1_interinter_compound_motion_search(cpi, x, cur_mv,
                                                           bsize, this_mode);
      const int mi_row = xd->mi_row;
      const int mi_col = xd->mi_col;
      av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, ctx, bsize,
                                    AOM_PLANE_Y, AOM_PLANE_Y);
    } else {
      *out_rate_mv = rate_mv;
      av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0, strides,
                                               preds1, strides);
    }
    // Get the RD cost from model RD
    model_rd_sb_fn[MODELRD_TYPE_MASKED_COMPOUND](
        cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum, &tmp_skip_txfm_sb,
        &tmp_skip_sse_sb, NULL, NULL, NULL);
    rd = RDCOST(x->rdmult, *rs2 + *out_rate_mv + rate_sum, dist_sum);
    *comp_model_rd_cur = rd;
    // Override with best if current is worse than best for new MV
    if (wedge_newmv_search) {
      if (rd >= best_rd_cur) {
        mbmi->mv[0].as_int = cur_mv[0].as_int;
        mbmi->mv[1].as_int = cur_mv[1].as_int;
        *out_rate_mv = rate_mv;
        av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0,
                                                 strides, preds1, strides);
        *comp_model_rd_cur = best_rd_cur;
      }
    }
    if (cpi->sf.inter_sf.prune_comp_type_by_model_rd &&
        (*comp_model_rd_cur > comp_best_model_rd) &&
        comp_best_model_rd != INT64_MAX) {
      *comp_model_rd_cur = INT64_MAX;
      return INT64_MAX;
    }
    // Compute RD cost for the current type
    RD_STATS rd_stats;
    const int64_t tmp_mode_rd = RDCOST(x->rdmult, *rs2 + *out_rate_mv, 0);
    const int64_t tmp_rd_thresh = rd_thresh - tmp_mode_rd;
    rd = estimate_yrd_for_sb(cpi, bsize, x, tmp_rd_thresh, &rd_stats);
    if (rd != INT64_MAX) {
      rd =
          RDCOST(x->rdmult, *rs2 + *out_rate_mv + rd_stats.rate, rd_stats.dist);
      // Backup rate and distortion for future reuse
      backup_stats(compound_type, comp_rate, comp_dist, comp_model_rate,
                   comp_model_dist, rate_sum, dist_sum, &rd_stats, comp_rs2,
                   *rs2);
    }
  } else {
    // Reuse data as matching record is found
    assert(comp_dist[compound_type] != INT64_MAX);
    // When disable_interinter_wedge_newmv_search is set, motion refinement is
    // disabled. Hence rate and distortion can be reused in this case as well
    assert(IMPLIES((have_newmv_in_inter_mode(this_mode) &&
                    (compound_type == COMPOUND_WEDGE)),
                   cpi->sf.inter_sf.disable_interinter_wedge_newmv_search));
    assert(mbmi->mv[0].as_int == cur_mv[0].as_int);
    assert(mbmi->mv[1].as_int == cur_mv[1].as_int);
    *out_rate_mv = rate_mv;
    // Calculate RD cost based on stored stats
    rd = RDCOST(x->rdmult, *rs2 + *out_rate_mv + comp_rate[compound_type],
                comp_dist[compound_type]);
    // Recalculate model rdcost with the updated rate
    *comp_model_rd_cur =
        RDCOST(x->rdmult, *rs2 + *out_rate_mv + comp_model_rate[compound_type],
               comp_model_dist[compound_type]);
  }
  return rd;
}

// scaling values to be used for gating wedge/compound segment based on best
// approximate rd
static int comp_type_rd_threshold_mul[3] = { 1, 11, 12 };
static int comp_type_rd_threshold_div[3] = { 3, 16, 16 };

int av1_compound_type_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                         HandleInterModeArgs *args, BLOCK_SIZE bsize,
                         int_mv *cur_mv, int mode_search_mask,
                         int masked_compound_used, const BUFFER_SET *orig_dst,
                         const BUFFER_SET *tmp_dst,
                         const CompoundTypeRdBuffers *buffers, int *rate_mv,
                         int64_t *rd, RD_STATS *rd_stats, int64_t ref_best_rd,
                         int64_t ref_skip_rd, int *is_luma_interp_done,
                         int64_t rd_thresh) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const PREDICTION_MODE this_mode = mbmi->mode;
  int ref_frame = av1_ref_frame_type(mbmi->ref_frame);
  const int bw = block_size_wide[bsize];
  int rs2;
  int_mv best_mv[2];
  int best_tmp_rate_mv = *rate_mv;
  BEST_COMP_TYPE_STATS best_type_stats;
  // Initializing BEST_COMP_TYPE_STATS
  best_type_stats.best_compound_data.type = COMPOUND_AVERAGE;
  best_type_stats.best_compmode_interinter_cost = 0;
  best_type_stats.comp_best_model_rd = INT64_MAX;

  uint8_t *preds0[1] = { buffers->pred0 };
  uint8_t *preds1[1] = { buffers->pred1 };
  int strides[1] = { bw };
  int tmp_rate_mv;
  COMPOUND_TYPE cur_type;
  // Local array to store the mask cost for different compound types
  int masked_type_cost[COMPOUND_TYPES];

  int calc_pred_masked_compound = 1;
  int64_t comp_dist[COMPOUND_TYPES] = { INT64_MAX, INT64_MAX, INT64_MAX,
                                        INT64_MAX };
  int32_t comp_rate[COMPOUND_TYPES] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };
  int comp_rs2[COMPOUND_TYPES] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };
  int32_t comp_model_rate[COMPOUND_TYPES] = { INT_MAX, INT_MAX, INT_MAX,
                                              INT_MAX };
  int64_t comp_model_dist[COMPOUND_TYPES] = { INT64_MAX, INT64_MAX, INT64_MAX,
                                              INT64_MAX };
  int match_index = 0;
  const int match_found =
      find_comp_rd_in_stats(cpi, x, mbmi, comp_rate, comp_dist, comp_model_rate,
                            comp_model_dist, comp_rs2, &match_index);
  best_mv[0].as_int = cur_mv[0].as_int;
  best_mv[1].as_int = cur_mv[1].as_int;
  *rd = INT64_MAX;

  // Local array to store the valid compound types to be evaluated in the core
  // loop
  COMPOUND_TYPE valid_comp_types[COMPOUND_TYPES] = {
    COMPOUND_AVERAGE, COMPOUND_DISTWTD, COMPOUND_WEDGE, COMPOUND_DIFFWTD
  };
  int valid_type_count = 0;
  // compute_valid_comp_types() returns the number of valid compound types to be
  // evaluated and populates the same in the local array valid_comp_types[].
  // It also sets the flag 'try_average_and_distwtd_comp'
  valid_type_count = compute_valid_comp_types(
      x, cpi, bsize, masked_compound_used, mode_search_mask, valid_comp_types);

  // The following context indices are independent of compound type
  const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
  const int comp_index_ctx = get_comp_index_context(cm, xd);

  // Populates masked_type_cost local array for the 4 compound types
  calc_masked_type_cost(&x->mode_costs, bsize, comp_group_idx_ctx,
                        comp_index_ctx, masked_compound_used, masked_type_cost);

  int64_t comp_model_rd_cur = INT64_MAX;
  int64_t best_rd_cur = ref_best_rd;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  // If the match is found, calculate the rd cost using the
  // stored stats and update the mbmi appropriately.
  if (match_found && cpi->sf.inter_sf.reuse_compound_type_decision) {
    return populate_reuse_comp_type_data(x, mbmi, &best_type_stats, cur_mv,
                                         comp_rate, comp_dist, comp_rs2,
                                         rate_mv, rd, match_index);
  }

  // If COMPOUND_AVERAGE is not valid, use the spare buffer
  if (valid_comp_types[0] != COMPOUND_AVERAGE) restore_dst_buf(xd, *tmp_dst, 1);

  // Loop over valid compound types
  for (int i = 0; i < valid_type_count; i++) {
    cur_type = valid_comp_types[i];

    if (args->cmp_mode[ref_frame] == COMPOUND_AVERAGE) {
      if (cur_type == COMPOUND_WEDGE) continue;
    }

    comp_model_rd_cur = INT64_MAX;
    tmp_rate_mv = *rate_mv;
    best_rd_cur = INT64_MAX;
    ref_best_rd = AOMMIN(ref_best_rd, *rd);
    update_mbmi_for_compound_type(mbmi, cur_type);
    rs2 = masked_type_cost[cur_type];

    int64_t mode_rd = RDCOST(x->rdmult, rs2 + rd_stats->rate, 0);
    if (mode_rd >= ref_best_rd) continue;

    // Derive the flags to indicate enabling/disabling of MV refinement process.
    const int enable_fast_compound_mode_search =
        cpi->sf.inter_sf.enable_fast_compound_mode_search;
    const bool skip_mv_refinement_for_avg_distwtd =
        enable_fast_compound_mode_search == 3 ||
        (enable_fast_compound_mode_search == 2 && (this_mode != NEW_NEWMV));
    const bool skip_mv_refinement_for_diffwtd =
        (!enable_fast_compound_mode_search && cur_type == COMPOUND_DIFFWTD);

    // Case COMPOUND_AVERAGE and COMPOUND_DISTWTD
    if (cur_type < COMPOUND_WEDGE) {
      if (skip_mv_refinement_for_avg_distwtd) {
        int rate_sum;
        uint8_t tmp_skip_txfm_sb;
        int64_t dist_sum, tmp_skip_sse_sb;

        // Reuse data if matching record is found
        if (comp_rate[cur_type] == INT_MAX) {
          av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                        AOM_PLANE_Y, AOM_PLANE_Y);
          if (cur_type == COMPOUND_AVERAGE) *is_luma_interp_done = 1;
          // Compute RD cost for the current type
          RD_STATS est_rd_stats;
          const int64_t tmp_rd_thresh = AOMMIN(*rd, rd_thresh) - mode_rd;
          int64_t est_rd = INT64_MAX;
          int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                                rs2 + *rate_mv);
          // Evaluate further if skip rd is low enough
          if (eval_txfm) {
            est_rd = estimate_yrd_for_sb(cpi, bsize, x, tmp_rd_thresh,
                                         &est_rd_stats);
          }
          if (est_rd != INT64_MAX) {
            best_rd_cur = RDCOST(x->rdmult, rs2 + *rate_mv + est_rd_stats.rate,
                                 est_rd_stats.dist);
            model_rd_sb_fn[MODELRD_TYPE_MASKED_COMPOUND](
                cpi, bsize, x, xd, 0, 0, &rate_sum, &dist_sum,
                &tmp_skip_txfm_sb, &tmp_skip_sse_sb, NULL, NULL, NULL);
            comp_model_rd_cur =
                RDCOST(x->rdmult, rs2 + *rate_mv + rate_sum, dist_sum);
            // Backup rate and distortion for future reuse
            backup_stats(cur_type, comp_rate, comp_dist, comp_model_rate,
                         comp_model_dist, rate_sum, dist_sum, &est_rd_stats,
                         comp_rs2, rs2);
          }
        } else {
          // Calculate RD cost based on stored stats
          assert(comp_dist[cur_type] != INT64_MAX);
          best_rd_cur = RDCOST(x->rdmult, rs2 + *rate_mv + comp_rate[cur_type],
                               comp_dist[cur_type]);
          // Recalculate model rdcost with the updated rate
          comp_model_rd_cur =
              RDCOST(x->rdmult, rs2 + *rate_mv + comp_model_rate[cur_type],
                     comp_model_dist[cur_type]);
        }
      } else {
        tmp_rate_mv = *rate_mv;
        if (have_newmv_in_inter_mode(this_mode)) {
          InterPredParams inter_pred_params;
          av1_dist_wtd_comp_weight_assign(
              &cpi->common, mbmi, &inter_pred_params.conv_params.fwd_offset,
              &inter_pred_params.conv_params.bck_offset,
              &inter_pred_params.conv_params.use_dist_wtd_comp_avg, 1);
          int mask_value = inter_pred_params.conv_params.fwd_offset * 4;
          memset(xd->seg_mask, mask_value,
                 sizeof(xd->seg_mask[0]) * 2 * MAX_SB_SQUARE);
          tmp_rate_mv = av1_interinter_compound_motion_search(cpi, x, cur_mv,
                                                              bsize, this_mode);
        }
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                      AOM_PLANE_Y, AOM_PLANE_Y);
        if (cur_type == COMPOUND_AVERAGE) *is_luma_interp_done = 1;

        int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                              rs2 + *rate_mv);
        if (eval_txfm) {
          RD_STATS est_rd_stats;
          estimate_yrd_for_sb(cpi, bsize, x, INT64_MAX, &est_rd_stats);

          best_rd_cur = RDCOST(x->rdmult, rs2 + tmp_rate_mv + est_rd_stats.rate,
                               est_rd_stats.dist);
        }
      }

      // use spare buffer for following compound type try
      if (cur_type == COMPOUND_AVERAGE) restore_dst_buf(xd, *tmp_dst, 1);
    } else if (cur_type == COMPOUND_WEDGE) {
      int best_mask_index = 0;
      int best_wedge_sign = 0;
      int_mv tmp_mv[2] = { mbmi->mv[0], mbmi->mv[1] };
      int best_rs2 = 0;
      int best_rate_mv = *rate_mv;
      int wedge_mask_size = get_wedge_types_lookup(bsize);
      int need_mask_search = args->wedge_index == -1;
      int wedge_newmv_search =
          have_newmv_in_inter_mode(this_mode) &&
          !cpi->sf.inter_sf.disable_interinter_wedge_newmv_search;

      if (need_mask_search && !wedge_newmv_search) {
        // short cut repeated single reference block build
        av1_build_inter_predictors_for_planes_single_buf(xd, bsize, 0, 0, 0,
                                                         preds0, strides);
        av1_build_inter_predictors_for_planes_single_buf(xd, bsize, 0, 0, 1,
                                                         preds1, strides);
      }

      for (int wedge_mask = 0; wedge_mask < wedge_mask_size && need_mask_search;
           ++wedge_mask) {
        for (int wedge_sign = 0; wedge_sign < 2; ++wedge_sign) {
          tmp_rate_mv = *rate_mv;
          mbmi->interinter_comp.wedge_index = wedge_mask;
          mbmi->interinter_comp.wedge_sign = wedge_sign;
          rs2 = masked_type_cost[cur_type];
          rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);

          mode_rd = RDCOST(x->rdmult, rs2 + rd_stats->rate, 0);
          if (mode_rd >= ref_best_rd / 2) continue;

          if (wedge_newmv_search) {
            tmp_rate_mv = av1_interinter_compound_motion_search(
                cpi, x, cur_mv, bsize, this_mode);
            av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst,
                                          bsize, AOM_PLANE_Y, AOM_PLANE_Y);
          } else {
            av1_build_wedge_inter_predictor_from_buf(xd, bsize, 0, 0, preds0,
                                                     strides, preds1, strides);
          }

          RD_STATS est_rd_stats;
          int64_t this_rd_cur = INT64_MAX;
          int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                                rs2 + *rate_mv);
          if (eval_txfm) {
            this_rd_cur = estimate_yrd_for_sb(
                cpi, bsize, x, AOMMIN(best_rd_cur, ref_best_rd), &est_rd_stats);
          }
          if (this_rd_cur < INT64_MAX) {
            this_rd_cur =
                RDCOST(x->rdmult, rs2 + tmp_rate_mv + est_rd_stats.rate,
                       est_rd_stats.dist);
          }
          if (this_rd_cur < best_rd_cur) {
            best_mask_index = wedge_mask;
            best_wedge_sign = wedge_sign;
            best_rd_cur = this_rd_cur;
            tmp_mv[0] = mbmi->mv[0];
            tmp_mv[1] = mbmi->mv[1];
            best_rate_mv = tmp_rate_mv;
            best_rs2 = rs2;
          }
        }
        // Consider the asymmetric partitions for oblique angle only if the
        // corresponding symmetric partition is the best so far.
        // Note: For horizontal and vertical types, both symmetric and
        // asymmetric partitions are always considered.
        if (cpi->sf.inter_sf.enable_fast_wedge_mask_search) {
          // The first 4 entries in wedge_codebook_16_heqw/hltw/hgtw[16]
          // correspond to symmetric partitions of the 4 oblique angles, the
          // next 4 entries correspond to the vertical/horizontal
          // symmetric/asymmetric partitions and the last 8 entries correspond
          // to the asymmetric partitions of oblique types.
          const int idx_before_asym_oblique = 7;
          const int last_oblique_sym_idx = 3;
          if (wedge_mask == idx_before_asym_oblique) {
            if (best_mask_index > last_oblique_sym_idx) {
              break;
            } else {
              // Asymmetric (Index-1) map for the corresponding oblique masks.
              // WEDGE_OBLIQUE27: sym - 0, asym - 8, 9
              // WEDGE_OBLIQUE63: sym - 1, asym - 12, 13
              // WEDGE_OBLIQUE117: sym - 2, asym - 14, 15
              // WEDGE_OBLIQUE153: sym - 3, asym - 10, 11
              const int asym_mask_idx[4] = { 7, 11, 13, 9 };
              wedge_mask = asym_mask_idx[best_mask_index];
              wedge_mask_size = wedge_mask + 3;
            }
          }
        }
      }

      if (need_mask_search) {
        if (save_mask_search_results(
                this_mode, cpi->sf.inter_sf.reuse_mask_search_results)) {
          args->wedge_index = best_mask_index;
          args->wedge_sign = best_wedge_sign;
        }
      } else {
        mbmi->interinter_comp.wedge_index = args->wedge_index;
        mbmi->interinter_comp.wedge_sign = args->wedge_sign;
        rs2 = masked_type_cost[cur_type];
        rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);

        if (wedge_newmv_search) {
          tmp_rate_mv = av1_interinter_compound_motion_search(cpi, x, cur_mv,
                                                              bsize, this_mode);
        }

        best_mask_index = args->wedge_index;
        best_wedge_sign = args->wedge_sign;
        tmp_mv[0] = mbmi->mv[0];
        tmp_mv[1] = mbmi->mv[1];
        best_rate_mv = tmp_rate_mv;
        best_rs2 = masked_type_cost[cur_type];
        best_rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                      AOM_PLANE_Y, AOM_PLANE_Y);
        int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                              best_rs2 + *rate_mv);
        if (eval_txfm) {
          RD_STATS est_rd_stats;
          estimate_yrd_for_sb(cpi, bsize, x, INT64_MAX, &est_rd_stats);
          best_rd_cur =
              RDCOST(x->rdmult, best_rs2 + tmp_rate_mv + est_rd_stats.rate,
                     est_rd_stats.dist);
        }
      }

      mbmi->interinter_comp.wedge_index = best_mask_index;
      mbmi->interinter_comp.wedge_sign = best_wedge_sign;
      mbmi->mv[0] = tmp_mv[0];
      mbmi->mv[1] = tmp_mv[1];
      tmp_rate_mv = best_rate_mv;
      rs2 = best_rs2;
    } else if (skip_mv_refinement_for_diffwtd) {
      int_mv tmp_mv[2];
      int best_mask_index = 0;
      rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);

      int need_mask_search = args->diffwtd_index == -1;

      for (int mask_index = 0; mask_index < 2 && need_mask_search;
           ++mask_index) {
        tmp_rate_mv = *rate_mv;
        mbmi->interinter_comp.mask_type = mask_index;
        if (have_newmv_in_inter_mode(this_mode)) {
          // hard coded number for diff wtd
          int mask_value = mask_index == 0 ? 38 : 26;
          memset(xd->seg_mask, mask_value,
                 sizeof(xd->seg_mask[0]) * 2 * MAX_SB_SQUARE);
          tmp_rate_mv = av1_interinter_compound_motion_search(cpi, x, cur_mv,
                                                              bsize, this_mode);
        }
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                      AOM_PLANE_Y, AOM_PLANE_Y);
        RD_STATS est_rd_stats;
        int64_t this_rd_cur = INT64_MAX;
        int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                              rs2 + *rate_mv);
        if (eval_txfm) {
          this_rd_cur =
              estimate_yrd_for_sb(cpi, bsize, x, ref_best_rd, &est_rd_stats);
        }
        if (this_rd_cur < INT64_MAX) {
          this_rd_cur = RDCOST(x->rdmult, rs2 + tmp_rate_mv + est_rd_stats.rate,
                               est_rd_stats.dist);
        }

        if (this_rd_cur < best_rd_cur) {
          best_rd_cur = this_rd_cur;
          best_mask_index = mbmi->interinter_comp.mask_type;
          tmp_mv[0] = mbmi->mv[0];
          tmp_mv[1] = mbmi->mv[1];
        }
      }

      if (need_mask_search) {
        if (save_mask_search_results(this_mode, 0))
          args->diffwtd_index = best_mask_index;
      } else {
        mbmi->interinter_comp.mask_type = args->diffwtd_index;
        rs2 = masked_type_cost[cur_type];
        rs2 += get_interinter_compound_mask_rate(&x->mode_costs, mbmi);

        int mask_value = mbmi->interinter_comp.mask_type == 0 ? 38 : 26;
        memset(xd->seg_mask, mask_value,
               sizeof(xd->seg_mask[0]) * 2 * MAX_SB_SQUARE);

        if (have_newmv_in_inter_mode(this_mode)) {
          tmp_rate_mv = av1_interinter_compound_motion_search(cpi, x, cur_mv,
                                                              bsize, this_mode);
        }
        best_mask_index = mbmi->interinter_comp.mask_type;
        tmp_mv[0] = mbmi->mv[0];
        tmp_mv[1] = mbmi->mv[1];
        av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, orig_dst, bsize,
                                      AOM_PLANE_Y, AOM_PLANE_Y);
        RD_STATS est_rd_stats;
        int64_t this_rd_cur = INT64_MAX;
        int eval_txfm = prune_mode_by_skip_rd(cpi, x, xd, bsize, ref_skip_rd,
                                              rs2 + *rate_mv);
        if (eval_txfm) {
          this_rd_cur =
              estimate_yrd_for_sb(cpi, bsize, x, ref_best_rd, &est_rd_stats);
        }
        if (this_rd_cur < INT64_MAX) {
          best_rd_cur = RDCOST(x->rdmult, rs2 + tmp_rate_mv + est_rd_stats.rate,
                               est_rd_stats.dist);
        }
      }

      mbmi->interinter_comp.mask_type = best_mask_index;
      mbmi->mv[0] = tmp_mv[0];
      mbmi->mv[1] = tmp_mv[1];
    } else {
      // Handle masked compound types
      bool eval_masked_comp_type = true;
      if (*rd != INT64_MAX) {
        // Factors to control gating of compound type selection based on best
        // approximate rd so far
        const int max_comp_type_rd_threshold_mul =
            comp_type_rd_threshold_mul[cpi->sf.inter_sf
                                           .prune_comp_type_by_comp_avg];
        const int max_comp_type_rd_threshold_div =
            comp_type_rd_threshold_div[cpi->sf.inter_sf
                                           .prune_comp_type_by_comp_avg];
        // Evaluate COMPOUND_WEDGE / COMPOUND_DIFFWTD if approximated cost is
        // within threshold
        const int64_t approx_rd = ((*rd / max_comp_type_rd_threshold_div) *
                                   max_comp_type_rd_threshold_mul);
        if (approx_rd >= ref_best_rd) eval_masked_comp_type = false;
      }

      if (eval_masked_comp_type) {
        const int64_t tmp_rd_thresh = AOMMIN(*rd, rd_thresh);
        best_rd_cur = masked_compound_type_rd(
            cpi, x, cur_mv, bsize, this_mode, &rs2, *rate_mv, orig_dst,
            &tmp_rate_mv, preds0, preds1, buffers->residual1, buffers->diff10,
            strides, rd_stats->rate, tmp_rd_thresh, &calc_pred_masked_compound,
            comp_rate, comp_dist, comp_model_rate, comp_model_dist,
            best_type_stats.comp_best_model_rd, &comp_model_rd_cur, comp_rs2,
            ref_skip_rd);
      }
    }

    // Update stats for best compound type
    if (best_rd_cur < *rd) {
      update_best_info(mbmi, rd, &best_type_stats, best_rd_cur,
                       comp_model_rd_cur, rs2);
      if (have_newmv_in_inter_mode(this_mode))
        update_mask_best_mv(mbmi, best_mv, &best_tmp_rate_mv, tmp_rate_mv);
    }
    // reset to original mvs for next iteration
    mbmi->mv[0].as_int = cur_mv[0].as_int;
    mbmi->mv[1].as_int = cur_mv[1].as_int;
  }

  mbmi->comp_group_idx =
      (best_type_stats.best_compound_data.type < COMPOUND_WEDGE) ? 0 : 1;
  mbmi->compound_idx =
      !(best_type_stats.best_compound_data.type == COMPOUND_DISTWTD);
  mbmi->interinter_comp = best_type_stats.best_compound_data;

  if (have_newmv_in_inter_mode(this_mode)) {
    mbmi->mv[0].as_int = best_mv[0].as_int;
    mbmi->mv[1].as_int = best_mv[1].as_int;
    rd_stats->rate += best_tmp_rate_mv - *rate_mv;
    *rate_mv = best_tmp_rate_mv;
  }

  if (this_mode == NEW_NEWMV)
    args->cmp_mode[ref_frame] = mbmi->interinter_comp.type;

  restore_dst_buf(xd, *orig_dst, 1);
  if (!match_found)
    save_comp_rd_search_stat(x, mbmi, comp_rate, comp_dist, comp_model_rate,
                             comp_model_dist, cur_mv, comp_rs2);
  return best_type_stats.best_compmode_interinter_cost;
}
