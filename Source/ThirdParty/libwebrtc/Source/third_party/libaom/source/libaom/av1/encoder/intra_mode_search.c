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

#include "av1/common/av1_common_int.h"
#include "av1/common/reconintra.h"

#include "av1/encoder/intra_mode_search.h"
#include "av1/encoder/intra_mode_search_utils.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/tx_search.h"

// Even though there are 7 delta angles, this macro is set to 9 to facilitate
// the rd threshold check to prune -3 and 3 delta angles.
#define SIZE_OF_ANGLE_DELTA_RD_COST_ARRAY (2 * MAX_ANGLE_DELTA + 3)

// The order for evaluating delta angles while processing the luma directional
// intra modes. Currently, this order of evaluation is applicable only when
// speed feature prune_luma_odd_delta_angles_in_intra is enabled. In this case,
// even angles are evaluated first in order to facilitate the pruning of odd
// delta angles based on the rd costs of the neighboring delta angles.
static const int8_t luma_delta_angles_order[2 * MAX_ANGLE_DELTA] = {
  -2, 2, -3, -1, 1, 3,
};

/*!\cond */
static const PREDICTION_MODE intra_rd_search_mode_order[INTRA_MODES] = {
  DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
  SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D203_PRED,   D157_PRED,
  D67_PRED,      D113_PRED,     D45_PRED,
};

static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D203_PRED,  UV_D157_PRED,     UV_D67_PRED,
  UV_D113_PRED,   UV_D45_PRED,
};

// The bitmask corresponds to the filter intra modes as defined in enums.h
// FILTER_INTRA_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding filter intra mode. The table
// av1_derived_filter_intra_mode_used_flag is used when speed feature
// prune_filter_intra_level is 1. The evaluated filter intra modes are union
// of the following:
// 1) FILTER_DC_PRED
// 2) mode that corresponds to best mode so far of DC_PRED, V_PRED, H_PRED,
// D157_PRED and PAETH_PRED. (Eg: FILTER_V_PRED if best mode so far is V_PRED).
static const uint8_t av1_derived_filter_intra_mode_used_flag[INTRA_MODES] = {
  0x01,  // DC_PRED:           0000 0001
  0x03,  // V_PRED:            0000 0011
  0x05,  // H_PRED:            0000 0101
  0x01,  // D45_PRED:          0000 0001
  0x01,  // D135_PRED:         0000 0001
  0x01,  // D113_PRED:         0000 0001
  0x09,  // D157_PRED:         0000 1001
  0x01,  // D203_PRED:         0000 0001
  0x01,  // D67_PRED:          0000 0001
  0x01,  // SMOOTH_PRED:       0000 0001
  0x01,  // SMOOTH_V_PRED:     0000 0001
  0x01,  // SMOOTH_H_PRED:     0000 0001
  0x11   // PAETH_PRED:        0001 0001
};

// The bitmask corresponds to the chroma intra modes as defined in enums.h
// UV_PREDICTION_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding chroma intra mode. The table
// av1_derived_chroma_intra_mode_used_flag is used when speed feature
// prune_chroma_modes_using_luma_winner is enabled. The evaluated chroma
// intra modes are union of the following:
// 1) UV_DC_PRED
// 2) UV_SMOOTH_PRED
// 3) UV_CFL_PRED
// 4) mode that corresponds to luma intra mode winner (Eg : UV_V_PRED if luma
// intra mode winner is V_PRED).
static const uint16_t av1_derived_chroma_intra_mode_used_flag[INTRA_MODES] = {
  0x2201,  // DC_PRED:           0010 0010 0000 0001
  0x2203,  // V_PRED:            0010 0010 0000 0011
  0x2205,  // H_PRED:            0010 0010 0000 0101
  0x2209,  // D45_PRED:          0010 0010 0000 1001
  0x2211,  // D135_PRED:         0010 0010 0001 0001
  0x2221,  // D113_PRED:         0010 0010 0010 0001
  0x2241,  // D157_PRED:         0010 0010 0100 0001
  0x2281,  // D203_PRED:         0010 0010 1000 0001
  0x2301,  // D67_PRED:          0010 0011 0000 0001
  0x2201,  // SMOOTH_PRED:       0010 0010 0000 0001
  0x2601,  // SMOOTH_V_PRED:     0010 0110 0000 0001
  0x2a01,  // SMOOTH_H_PRED:     0010 1010 0000 0001
  0x3201   // PAETH_PRED:        0011 0010 0000 0001
};

DECLARE_ALIGNED(16, static const uint8_t, all_zeros[MAX_SB_SIZE]) = { 0 };
DECLARE_ALIGNED(16, static const uint16_t,
                highbd_all_zeros[MAX_SB_SIZE]) = { 0 };

int av1_calc_normalized_variance(aom_variance_fn_t vf, const uint8_t *const buf,
                                 const int stride, const int is_hbd) {
  unsigned int sse;

  if (is_hbd)
    return vf(buf, stride, CONVERT_TO_BYTEPTR(highbd_all_zeros), 0, &sse);
  else
    return vf(buf, stride, all_zeros, 0, &sse);
}

// Computes average of log(1 + variance) across 4x4 sub-blocks for source and
// reconstructed blocks.
static void compute_avg_log_variance(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     const BLOCK_SIZE bs,
                                     double *avg_log_src_variance,
                                     double *avg_log_recon_variance) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const BLOCK_SIZE sb_size = cpi->common.seq_params->sb_size;
  const int mi_row_in_sb = x->e_mbd.mi_row & (mi_size_high[sb_size] - 1);
  const int mi_col_in_sb = x->e_mbd.mi_col & (mi_size_wide[sb_size] - 1);
  const int right_overflow =
      (xd->mb_to_right_edge < 0) ? ((-xd->mb_to_right_edge) >> 3) : 0;
  const int bottom_overflow =
      (xd->mb_to_bottom_edge < 0) ? ((-xd->mb_to_bottom_edge) >> 3) : 0;
  const int bw = (MI_SIZE * mi_size_wide[bs] - right_overflow);
  const int bh = (MI_SIZE * mi_size_high[bs] - bottom_overflow);
  const int is_hbd = is_cur_buf_hbd(xd);

  for (int i = 0; i < bh; i += MI_SIZE) {
    const int r = mi_row_in_sb + (i >> MI_SIZE_LOG2);
    for (int j = 0; j < bw; j += MI_SIZE) {
      const int c = mi_col_in_sb + (j >> MI_SIZE_LOG2);
      const int mi_offset = r * mi_size_wide[sb_size] + c;
      Block4x4VarInfo *block_4x4_var_info =
          &x->src_var_info_of_4x4_sub_blocks[mi_offset];
      int src_var = block_4x4_var_info->var;
      double log_src_var = block_4x4_var_info->log_var;
      // Compute average of log(1 + variance) for the source block from 4x4
      // sub-block variance values. Calculate and store 4x4 sub-block variance
      // and log(1 + variance), if the values present in
      // src_var_of_4x4_sub_blocks are invalid. Reuse the same if it is readily
      // available with valid values.
      if (src_var < 0) {
        src_var = av1_calc_normalized_variance(
            cpi->ppi->fn_ptr[BLOCK_4X4].vf,
            x->plane[0].src.buf + i * x->plane[0].src.stride + j,
            x->plane[0].src.stride, is_hbd);
        block_4x4_var_info->var = src_var;
        log_src_var = log(1.0 + src_var / 16.0);
        block_4x4_var_info->log_var = log_src_var;
      } else {
        // When source variance is already calculated and available for
        // retrieval, check if log(1 + variance) is also available. If it is
        // available, then retrieve from buffer. Else, calculate the same and
        // store to the buffer.
        if (log_src_var < 0) {
          log_src_var = log(1.0 + src_var / 16.0);
          block_4x4_var_info->log_var = log_src_var;
        }
      }
      *avg_log_src_variance += log_src_var;

      const int recon_var = av1_calc_normalized_variance(
          cpi->ppi->fn_ptr[BLOCK_4X4].vf,
          xd->plane[0].dst.buf + i * xd->plane[0].dst.stride + j,
          xd->plane[0].dst.stride, is_hbd);
      *avg_log_recon_variance += log(1.0 + recon_var / 16.0);
    }
  }

  const int blocks = (bw * bh) / 16;
  *avg_log_src_variance /= (double)blocks;
  *avg_log_recon_variance /= (double)blocks;
}

// Returns a factor to be applied to the RD value based on how well the
// reconstructed block variance matches the source variance.
static double intra_rd_variance_factor(const AV1_COMP *cpi, MACROBLOCK *x,
                                       BLOCK_SIZE bs) {
  double threshold = INTRA_RD_VAR_THRESH(cpi->oxcf.speed);
  // For non-positive threshold values, the comparison of source and
  // reconstructed variances with threshold evaluates to false
  // (src_var < threshold/rec_var < threshold) as these metrics are greater than
  // than 0. Hence further calculations are skipped.
  if (threshold <= 0) return 1.0;

  double variance_rd_factor = 1.0;
  double avg_log_src_variance = 0.0;
  double avg_log_recon_variance = 0.0;
  double var_diff = 0.0;

  compute_avg_log_variance(cpi, x, bs, &avg_log_src_variance,
                           &avg_log_recon_variance);

  // Dont allow 0 to prevent / 0 below.
  avg_log_src_variance += 0.000001;
  avg_log_recon_variance += 0.000001;

  if (avg_log_src_variance >= avg_log_recon_variance) {
    var_diff = (avg_log_src_variance - avg_log_recon_variance);
    if ((var_diff > 0.5) && (avg_log_recon_variance < threshold)) {
      variance_rd_factor = 1.0 + ((var_diff * 2) / avg_log_src_variance);
    }
  } else {
    var_diff = (avg_log_recon_variance - avg_log_src_variance);
    if ((var_diff > 0.5) && (avg_log_src_variance < threshold)) {
      variance_rd_factor = 1.0 + (var_diff / (2 * avg_log_src_variance));
    }
  }

  // Limit adjustment;
  variance_rd_factor = AOMMIN(3.0, variance_rd_factor);

  return variance_rd_factor;
}
/*!\endcond */

/*!\brief Search for the best filter_intra mode when coding intra frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \return Returns 1 if a new filter_intra mode is selected; 0 otherwise.
 */
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    PREDICTION_MODE best_mode_so_far,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    PICK_MODE_CONTEXT *ctx) {
  // Skip the evaluation of filter intra modes.
  if (cpi->sf.intra_sf.prune_filter_intra_level == 2) return 0;

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  // Skip the evaluation of filter-intra if cached MB_MODE_INFO does not have
  // filter-intra as winner.
  if (x->use_mb_mode_cache &&
      !x->mb_mode_cache->filter_intra_mode_info.use_filter_intra)
    return 0;

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int64_t this_rd;
    RD_STATS tokenonly_rd_stats;
    mbmi->filter_intra_mode_info.filter_intra_mode = mode;

    if ((cpi->sf.intra_sf.prune_filter_intra_level == 1) &&
        !(av1_derived_filter_intra_mode_used_flag[best_mode_so_far] &
          (1 << mode)))
      continue;

    // Skip the evaluation of modes that do not match with the winner mode in
    // x->mb_mode_cache.
    if (x->use_mb_mode_cache &&
        mode != x->mb_mode_cache->filter_intra_mode_info.filter_intra_mode)
      continue;

    if (model_intra_yrd_and_prune(cpi, x, bsize, best_model_rd)) {
      continue;
    }
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                      *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    const int this_rate =
        tokenonly_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    // Visual quality adjustment based on recon vs source variance.
    if ((cpi->oxcf.mode == ALLINTRA) && (this_rd != INT64_MAX)) {
      this_rd = (int64_t)(this_rd * intra_rd_variance_factor(cpi, x, bsize));
    }

    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mbmi->tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      memcpy(ctx->blk_skip, x->txfm_search_info.blk_skip,
             sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->mode = DC_PRED;
    mbmi->tx_size = best_tx_size;
    mbmi->filter_intra_mode_info = filter_intra_mode_info;
    av1_copy_array(ctx->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
    return 1;
  } else {
    return 0;
  }
}

void av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                      int *val_count, int *num_colors) {
  const int max_pix_val = 1 << 8;
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int this_val = src[r * stride + c];
      assert(this_val < max_pix_val);
      ++val_count[this_val];
    }
  }
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  *num_colors = n;
}

void av1_count_colors_highbd(const uint8_t *src8, int stride, int rows,
                             int cols, int bit_depth, int *val_count,
                             int *bin_val_count, int *num_color_bins,
                             int *num_colors) {
  assert(bit_depth <= 12);
  const int max_bin_val = 1 << 8;
  const int max_pix_val = 1 << bit_depth;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memset(bin_val_count, 0, max_bin_val * sizeof(val_count[0]));
  if (val_count != NULL)
    memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      /*
       * Down-convert the pixels to 8-bit domain before counting.
       * This provides consistency of behavior for palette search
       * between lbd and hbd encodes. This down-converted pixels
       * are only used for calculating the threshold (n).
       */
      const int this_val = ((src[r * stride + c]) >> (bit_depth - 8));
      assert(this_val < max_bin_val);
      if (this_val >= max_bin_val) continue;
      ++bin_val_count[this_val];
      if (val_count != NULL) ++val_count[(src[r * stride + c])];
    }
  }
  int n = 0;
  // Count the colors based on 8-bit domain used to gate the palette path
  for (int i = 0; i < max_bin_val; ++i) {
    if (bin_val_count[i]) ++n;
  }
  *num_color_bins = n;

  // Count the actual hbd colors used to create top_colors
  n = 0;
  if (val_count != NULL) {
    for (int i = 0; i < max_pix_val; ++i) {
      if (val_count[i]) ++n;
    }
    *num_colors = n;
  }
}

void set_y_mode_and_delta_angle(const int mode_idx, MB_MODE_INFO *const mbmi,
                                int reorder_delta_angle_eval) {
  if (mode_idx < INTRA_MODE_END) {
    mbmi->mode = intra_rd_search_mode_order[mode_idx];
    mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  } else {
    mbmi->mode = (mode_idx - INTRA_MODE_END) / (MAX_ANGLE_DELTA * 2) + V_PRED;
    int delta_angle_eval_idx =
        (mode_idx - INTRA_MODE_END) % (MAX_ANGLE_DELTA * 2);
    if (reorder_delta_angle_eval) {
      mbmi->angle_delta[PLANE_TYPE_Y] =
          luma_delta_angles_order[delta_angle_eval_idx];
    } else {
      mbmi->angle_delta[PLANE_TYPE_Y] =
          (delta_angle_eval_idx < 3 ? (delta_angle_eval_idx - 3)
                                    : (delta_angle_eval_idx - 2));
    }
  }
}

static AOM_INLINE int get_model_rd_index_for_pruning(
    const MACROBLOCK *const x,
    const INTRA_MODE_SPEED_FEATURES *const intra_sf) {
  const int top_intra_model_count_allowed =
      intra_sf->top_intra_model_count_allowed;
  if (!intra_sf->adapt_top_model_rd_count_using_neighbors)
    return top_intra_model_count_allowed - 1;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const PREDICTION_MODE mode = xd->mi[0]->mode;
  int model_rd_index_for_pruning = top_intra_model_count_allowed - 1;
  int is_left_mode_neq_cur_mode = 0, is_above_mode_neq_cur_mode = 0;
  if (xd->left_available)
    is_left_mode_neq_cur_mode = xd->left_mbmi->mode != mode;
  if (xd->up_available)
    is_above_mode_neq_cur_mode = xd->above_mbmi->mode != mode;
  // The pruning of luma intra modes is made more aggressive at lower quantizers
  // and vice versa. The value for model_rd_index_for_pruning is derived as
  // follows.
  // qidx 0 to 127: Reduce the index of a candidate used for comparison only if
  // the current mode does not match either of the available neighboring modes.
  // qidx 128 to 255: Reduce the index of a candidate used for comparison only
  // if the current mode does not match both the available neighboring modes.
  if (x->qindex <= 127) {
    if (is_left_mode_neq_cur_mode || is_above_mode_neq_cur_mode)
      model_rd_index_for_pruning = AOMMAX(model_rd_index_for_pruning - 1, 0);
  } else {
    if (is_left_mode_neq_cur_mode && is_above_mode_neq_cur_mode)
      model_rd_index_for_pruning = AOMMAX(model_rd_index_for_pruning - 1, 0);
  }
  return model_rd_index_for_pruning;
}

int prune_intra_y_mode(int64_t this_model_rd, int64_t *best_model_rd,
                       int64_t top_intra_model_rd[], int max_model_cnt_allowed,
                       int model_rd_index_for_pruning) {
  const double thresh_best = 1.50;
  const double thresh_top = 1.00;
  for (int i = 0; i < max_model_cnt_allowed; i++) {
    if (this_model_rd < top_intra_model_rd[i]) {
      for (int j = max_model_cnt_allowed - 1; j > i; j--) {
        top_intra_model_rd[j] = top_intra_model_rd[j - 1];
      }
      top_intra_model_rd[i] = this_model_rd;
      break;
    }
  }
  if (top_intra_model_rd[model_rd_index_for_pruning] != INT64_MAX &&
      this_model_rd >
          thresh_top * top_intra_model_rd[model_rd_index_for_pruning])
    return 1;

  if (this_model_rd != INT64_MAX &&
      this_model_rd > thresh_best * (*best_model_rd))
    return 1;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  return 0;
}

// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, int64_t best_rd_in, int *rate, RD_STATS *rd_stats,
    int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  assert(!is_inter_block(mbmi));
  int this_rate;
  int64_t this_rd;
  RD_STATS tokenonly_rd_stats;

  if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  this_rate = tokenonly_rd_stats.rate +
              intra_mode_info_cost_uv(cpi, x, mbmi, bsize, rate_overhead);
  this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip_txfm = tokenonly_rd_stats.skip_txfm;
  }
  return this_rd;
}

/*!\brief Search for the best angle delta for chroma prediction
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Given a chroma directional intra prediction mode, this function will try to
 * estimate the best delta_angle.
 *
 * \returns Return if there is a new mode with smaller rdcost than best_rd.
 */
static int rd_pick_intra_angle_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd, int *rate,
                                    RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int i, angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];

  rd_stats->rate = INT_MAX;
  rd_stats->skip_txfm = 0;
  rd_stats->dist = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead,
                                              best_rd_in, rate, rd_stats,
                                              &best_angle_delta, &best_rd);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                      rate, rd_stats, &best_angle_delta,
                                      &best_rd);
      }
    }
  }

  mbmi->angle_delta[PLANE_TYPE_UV] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}

#define PLANE_SIGN_TO_JOINT_SIGN(plane, a, b) \
  (plane == CFL_PRED_U ? a * CFL_SIGNS + b - 1 : b * CFL_SIGNS + a - 1)

static void cfl_idx_to_sign_and_alpha(int cfl_idx, CFL_SIGN_TYPE *cfl_sign,
                                      int *cfl_alpha) {
  int cfl_linear_idx = cfl_idx - CFL_INDEX_ZERO;
  if (cfl_linear_idx == 0) {
    *cfl_sign = CFL_SIGN_ZERO;
    *cfl_alpha = 0;
  } else {
    *cfl_sign = cfl_linear_idx > 0 ? CFL_SIGN_POS : CFL_SIGN_NEG;
    *cfl_alpha = abs(cfl_linear_idx) - 1;
  }
}

static int64_t cfl_compute_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              int plane, TX_SIZE tx_size,
                              BLOCK_SIZE plane_bsize, int cfl_idx,
                              int fast_mode, RD_STATS *rd_stats) {
  assert(IMPLIES(fast_mode, rd_stats == NULL));
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int cfl_plane = get_cfl_pred_type(plane);
  CFL_SIGN_TYPE cfl_sign;
  int cfl_alpha;
  cfl_idx_to_sign_and_alpha(cfl_idx, &cfl_sign, &cfl_alpha);
  // We conly build CFL for a given plane, the other plane's sign is dummy
  int dummy_sign = CFL_SIGN_NEG;
  const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  mbmi->cfl_alpha_signs =
      PLANE_SIGN_TO_JOINT_SIGN(cfl_plane, cfl_sign, dummy_sign);
  mbmi->cfl_alpha_idx = (cfl_alpha << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha;
  int64_t cfl_cost;
  if (fast_mode) {
    cfl_cost =
        intra_model_rd(cm, x, plane, plane_bsize, tx_size, /*use_hadamard=*/0);
  } else {
    av1_init_rd_stats(rd_stats);
    av1_txfm_rd_in_plane(x, cpi, rd_stats, INT64_MAX, 0, plane, plane_bsize,
                         tx_size, FTXS_NONE, 0);
    av1_rd_cost_update(x->rdmult, rd_stats);
    cfl_cost = rd_stats->rdcost;
  }
  mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
  mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;
  return cfl_cost;
}

static const int cfl_dir_ls[2] = { 1, -1 };

// If cfl_search_range is CFL_MAGS_SIZE, return zero. Otherwise return the index
// of the best alpha found using intra_model_rd().
static int cfl_pick_plane_parameter(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int plane, TX_SIZE tx_size,
                                    int cfl_search_range) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);

  if (cfl_search_range == CFL_MAGS_SIZE) return CFL_INDEX_ZERO;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->uv_mode == UV_CFL_PRED);
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);

  int est_best_cfl_idx = CFL_INDEX_ZERO;
  int fast_mode = 1;
  int start_cfl_idx = CFL_INDEX_ZERO;
  int64_t best_cfl_cost = cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize,
                                         start_cfl_idx, fast_mode, NULL);
  for (int si = 0; si < 2; ++si) {
    const int dir = cfl_dir_ls[si];
    for (int i = 1; i < CFL_MAGS_SIZE; ++i) {
      int cfl_idx = start_cfl_idx + dir * i;
      if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
      int64_t cfl_cost = cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize,
                                        cfl_idx, fast_mode, NULL);
      if (cfl_cost < best_cfl_cost) {
        best_cfl_cost = cfl_cost;
        est_best_cfl_idx = cfl_idx;
      } else {
        break;
      }
    }
  }
  return est_best_cfl_idx;
}

static void cfl_pick_plane_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              int plane, TX_SIZE tx_size, int cfl_search_range,
                              RD_STATS cfl_rd_arr[CFL_MAGS_SIZE],
                              int est_best_cfl_idx) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->uv_mode == UV_CFL_PRED);
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);

  for (int cfl_idx = 0; cfl_idx < CFL_MAGS_SIZE; ++cfl_idx) {
    av1_invalid_rd_stats(&cfl_rd_arr[cfl_idx]);
  }

  int fast_mode = 0;
  int start_cfl_idx = est_best_cfl_idx;
  cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize, start_cfl_idx, fast_mode,
                 &cfl_rd_arr[start_cfl_idx]);

  if (cfl_search_range == 1) return;

  for (int si = 0; si < 2; ++si) {
    const int dir = cfl_dir_ls[si];
    for (int i = 1; i < cfl_search_range; ++i) {
      int cfl_idx = start_cfl_idx + dir * i;
      if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
      cfl_compute_rd(cpi, x, plane, tx_size, plane_bsize, cfl_idx, fast_mode,
                     &cfl_rd_arr[cfl_idx]);
    }
  }
}

/*!\brief Pick the optimal parameters for Chroma to Luma (CFL) component
 *
 * \ingroup intra_mode_search
 * \callergraph
 *
 * This function will use DCT_DCT followed by computing SATD (sum of absolute
 * transformed differences) to estimate the RD score and find the best possible
 * CFL parameter.
 *
 * Then the function will apply a full RD search near the best possible CFL
 * parameter to find the best actual CFL parameter.
 *
 * Side effect:
 * We use ths buffers in x->plane[] and xd->plane[] as throw-away buffers for RD
 * search.
 *
 * \param[in] x                Encoder prediction block structure.
 * \param[in] cpi              Top-level encoder instance structure.
 * \param[in] tx_size          Transform size.
 * \param[in] ref_best_rd      Reference best RD.
 * \param[in] cfl_search_range The search range of full RD search near the
 *                             estimated best CFL parameter.
 *
 * \param[out]   best_rd_stats          RD stats of the best CFL parameter
 * \param[out]   best_cfl_alpha_idx     Best CFL alpha index
 * \param[out]   best_cfl_alpha_signs   Best CFL joint signs
 *
 */
static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             TX_SIZE tx_size, int64_t ref_best_rd,
                             int cfl_search_range, RD_STATS *best_rd_stats,
                             uint8_t *best_cfl_alpha_idx,
                             int8_t *best_cfl_alpha_signs) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  const ModeCosts *mode_costs = &x->mode_costs;
  RD_STATS cfl_rd_arr_u[CFL_MAGS_SIZE];
  RD_STATS cfl_rd_arr_v[CFL_MAGS_SIZE];
  MACROBLOCKD *const xd = &x->e_mbd;
  int est_best_cfl_idx_u, est_best_cfl_idx_v;

  av1_invalid_rd_stats(best_rd_stats);

  // As the dc pred data is same for different values of alpha, enable the
  // caching of dc pred data.
  xd->cfl.use_dc_pred_cache = 1;
  // Evaluate alpha parameter of each chroma plane.
  est_best_cfl_idx_u =
      cfl_pick_plane_parameter(cpi, x, 1, tx_size, cfl_search_range);
  est_best_cfl_idx_v =
      cfl_pick_plane_parameter(cpi, x, 2, tx_size, cfl_search_range);

  // For cfl_search_range=1, further refinement of alpha is not enabled. Hence
  // CfL index=0 for both the chroma planes implies invalid CfL mode.
  if (cfl_search_range == 1 && est_best_cfl_idx_u == CFL_INDEX_ZERO &&
      est_best_cfl_idx_v == CFL_INDEX_ZERO) {
    // Set invalid CfL parameters here as CfL mode is invalid.
    *best_cfl_alpha_idx = 0;
    *best_cfl_alpha_signs = 0;

    // Clear the following flags to avoid the unintentional usage of cached dc
    // pred data.
    xd->cfl.use_dc_pred_cache = 0;
    xd->cfl.dc_pred_is_cached[0] = 0;
    xd->cfl.dc_pred_is_cached[1] = 0;
    return 0;
  }

  // Compute the rd cost of each chroma plane using the alpha parameters which
  // were already evaluated.
  cfl_pick_plane_rd(cpi, x, 1, tx_size, cfl_search_range, cfl_rd_arr_u,
                    est_best_cfl_idx_u);
  cfl_pick_plane_rd(cpi, x, 2, tx_size, cfl_search_range, cfl_rd_arr_v,
                    est_best_cfl_idx_v);

  // Clear the following flags to avoid the unintentional usage of cached dc
  // pred data.
  xd->cfl.use_dc_pred_cache = 0;
  xd->cfl.dc_pred_is_cached[0] = 0;
  xd->cfl.dc_pred_is_cached[1] = 0;

  for (int ui = 0; ui < CFL_MAGS_SIZE; ++ui) {
    if (cfl_rd_arr_u[ui].rate == INT_MAX) continue;
    int cfl_alpha_u;
    CFL_SIGN_TYPE cfl_sign_u;
    cfl_idx_to_sign_and_alpha(ui, &cfl_sign_u, &cfl_alpha_u);
    for (int vi = 0; vi < CFL_MAGS_SIZE; ++vi) {
      if (cfl_rd_arr_v[vi].rate == INT_MAX) continue;
      int cfl_alpha_v;
      CFL_SIGN_TYPE cfl_sign_v;
      cfl_idx_to_sign_and_alpha(vi, &cfl_sign_v, &cfl_alpha_v);
      // cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO is not a
      // valid parameter for CFL
      if (cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO) continue;
      int joint_sign = cfl_sign_u * CFL_SIGNS + cfl_sign_v - 1;
      RD_STATS rd_stats = cfl_rd_arr_u[ui];
      av1_merge_rd_stats(&rd_stats, &cfl_rd_arr_v[vi]);
      if (rd_stats.rate != INT_MAX) {
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_U][cfl_alpha_u];
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_V][cfl_alpha_v];
      }
      av1_rd_cost_update(x->rdmult, &rd_stats);
      if (rd_stats.rdcost < best_rd_stats->rdcost) {
        *best_rd_stats = rd_stats;
        *best_cfl_alpha_idx =
            (cfl_alpha_u << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha_v;
        *best_cfl_alpha_signs = joint_sign;
      }
    }
  }
  if (best_rd_stats->rdcost >= ref_best_rd) {
    av1_invalid_rd_stats(best_rd_stats);
    // Set invalid CFL parameters here since the rdcost is not better than
    // ref_best_rd.
    *best_cfl_alpha_idx = 0;
    *best_cfl_alpha_signs = 0;
    return 0;
  }
  return 1;
}

static bool should_prune_chroma_smooth_pred_based_on_source_variance(
    const AV1_COMP *cpi, const MACROBLOCK *x, BLOCK_SIZE bsize) {
  if (!cpi->sf.intra_sf.prune_smooth_intra_mode_for_chroma) return false;

  // If the source variance of both chroma planes is less than 20 (empirically
  // derived), prune UV_SMOOTH_PRED.
  for (int i = AOM_PLANE_U; i < av1_num_planes(&cpi->common); i++) {
    const unsigned int variance = av1_get_perpixel_variance_facade(
        cpi, &x->e_mbd, &x->plane[i].src, bsize, i);
    if (variance >= 20) return false;
  }
  return true;
}

int64_t av1_rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, int *skippable,
                                    BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  MB_MODE_INFO best_mbmi = *mbmi;
  int64_t best_rd = INT64_MAX, this_rd;
  const ModeCosts *mode_costs = &x->mode_costs;
  const IntraModeCfg *const intra_mode_cfg = &cpi->oxcf.intra_mode_cfg;

  init_sbuv_mode(mbmi);

  // Return if the current block does not correspond to a chroma block.
  if (!xd->is_chroma_ref) {
    *rate = 0;
    *rate_tokenonly = 0;
    *distortion = 0;
    *skippable = 1;
    return INT64_MAX;
  }

  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl.store_y = store_cfl_required_rdo(cm, x);
  if (xd->cfl.store_y) {
    // Restore reconstructed luma values.
    // TODO(chiyotsai@google.com): right now we are re-computing the txfm in
    // this function everytime we search through uv modes. There is some
    // potential speed up here if we cache the result to avoid redundant
    // computation.
    av1_encode_intra_block_plane(cpi, x, mbmi->bsize, AOM_PLANE_Y,
                                 DRY_RUN_NORMAL,
                                 cpi->optimize_seg_arr[mbmi->segment_id]);
    xd->cfl.store_y = 0;
  }
  IntraModeSearchState intra_search_state;
  init_intra_mode_search_state(&intra_search_state);

  // Search through all non-palette modes.
  for (int mode_idx = 0; mode_idx < UV_INTRA_MODES; ++mode_idx) {
    int this_rate;
    RD_STATS tokenonly_rd_stats;
    UV_PREDICTION_MODE mode = uv_rd_search_mode_order[mode_idx];
    const int is_diagonal_mode = av1_is_diagonal_mode(get_uv_mode(mode));
    const int is_directional_mode = av1_is_directional_mode(get_uv_mode(mode));

    if (is_diagonal_mode && !cpi->oxcf.intra_mode_cfg.enable_diagonal_intra)
      continue;
    if (is_directional_mode &&
        !cpi->oxcf.intra_mode_cfg.enable_directional_intra)
      continue;

    if (!(cpi->sf.intra_sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]] &
          (1 << mode)))
      continue;
    if (!intra_mode_cfg->enable_smooth_intra && mode >= UV_SMOOTH_PRED &&
        mode <= UV_SMOOTH_H_PRED)
      continue;

    if (!intra_mode_cfg->enable_paeth_intra && mode == UV_PAETH_PRED) continue;

    assert(mbmi->mode < INTRA_MODES);
    if (cpi->sf.intra_sf.prune_chroma_modes_using_luma_winner &&
        !(av1_derived_chroma_intra_mode_used_flag[mbmi->mode] & (1 << mode)))
      continue;

    mbmi->uv_mode = mode;

    // Init variables for cfl and angle delta
    const SPEED_FEATURES *sf = &cpi->sf;
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    if (mode == UV_CFL_PRED) {
      if (!is_cfl_allowed(xd) || !intra_mode_cfg->enable_cfl_intra) continue;
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      if (!cfl_rd_pick_alpha(x, cpi, uv_tx_size, best_rd,
                             sf->intra_sf.cfl_search_range, &tokenonly_rd_stats,
                             &mbmi->cfl_alpha_idx, &mbmi->cfl_alpha_signs)) {
        continue;
      }
    } else if (is_directional_mode && av1_use_angle_delta(mbmi->bsize) &&
               intra_mode_cfg->enable_angle_delta) {
      if (sf->intra_sf.chroma_intra_pruning_with_hog &&
          !intra_search_state.dir_mode_skip_mask_ready) {
        static const float thresh[2][4] = {
          { -1.2f, 0.0f, 0.0f, 1.2f },    // Interframe
          { -1.2f, -1.2f, -0.6f, 0.4f },  // Intraframe
        };
        const int is_chroma = 1;
        const int is_intra_frame = frame_is_intra_only(cm);
        prune_intra_mode_with_hog(
            x, bsize, cm->seq_params->sb_size,
            thresh[is_intra_frame]
                  [sf->intra_sf.chroma_intra_pruning_with_hog - 1],
            intra_search_state.directional_mode_skip_mask, is_chroma);
        intra_search_state.dir_mode_skip_mask_ready = 1;
      }
      if (intra_search_state.directional_mode_skip_mask[mode]) {
        continue;
      }

      // Search through angle delta
      const int rate_overhead =
          mode_costs->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, rate_overhead, best_rd,
                                    &this_rate, &tokenonly_rd_stats))
        continue;
    } else {
      if (mode == UV_SMOOTH_PRED &&
          should_prune_chroma_smooth_pred_based_on_source_variance(cpi, x,
                                                                   bsize))
        continue;

      // Predict directly if we don't need to search for angle delta.
      if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
    }
    const int mode_cost =
        mode_costs->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][mode];
    this_rate = tokenonly_rd_stats.rate +
                intra_mode_info_cost_uv(cpi, x, mbmi, bsize, mode_cost);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
    }
  }

  // Search palette mode
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        mbmi->bsize);
  if (try_palette) {
    uint8_t *best_palette_color_map = x->palette_buffer->best_palette_color_map;
    av1_rd_pick_palette_intra_sbuv(
        cpi, x,
        mode_costs
            ->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][UV_DC_PRED],
        best_palette_color_map, &best_mbmi, &best_rd, rate, rate_tokenonly,
        distortion, skippable);
  }

  *mbmi = best_mbmi;
  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

// Searches palette mode for luma channel in inter frame.
int av1_search_palette_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            PICK_MODE_CONTEXT *ctx, RD_STATS *this_rd_cost,
                            int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate2 = 0;
  int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd;
  int skippable = 0;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const intra_mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  av1_zero(pmi->palette_size);

  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  av1_rd_pick_palette_intra_sby(cpi, x, bsize, intra_mode_cost[DC_PRED],
                                &best_mbmi_palette, best_palette_color_map,
                                &best_rd_palette, &rd_stats_y.rate, NULL,
                                &rd_stats_y.dist, &rd_stats_y.skip_txfm, NULL,
                                ctx, best_blk_skip, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return skippable;
  }

  memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * bsize_to_num_blk(bsize));
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  skippable = rd_stats_y.skip_txfm;
  distortion2 = rd_stats_y.dist;
  rate2 = rd_stats_y.rate + ref_frame_cost;
  if (num_planes > 1) {
    if (intra_search_state->rate_uv_intra == INT_MAX) {
      // We have not found any good uv mode yet, so we need to search for it.
      TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
      av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                                  &intra_search_state->rate_uv_tokenonly,
                                  &intra_search_state->dist_uvs,
                                  &intra_search_state->skip_uvs, bsize, uv_tx);
      intra_search_state->mode_uv = mbmi->uv_mode;
      intra_search_state->pmi_uv = *pmi;
      intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    }

    // We have found at least one good uv mode before, so copy and paste it
    // over.
    mbmi->uv_mode = intra_search_state->mode_uv;
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;
    skippable = skippable && intra_search_state->skip_uvs;
    distortion2 += intra_search_state->dist_uvs;
    rate2 += intra_search_state->rate_uv_intra;
  }

  if (skippable) {
    rate2 -= rd_stats_y.rate;
    if (num_planes > 1) rate2 -= intra_search_state->rate_uv_tokenonly;
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][1];
  } else {
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rate2, distortion2);
  this_rd_cost->rate = rate2;
  this_rd_cost->dist = distortion2;
  this_rd_cost->rdcost = this_rd;
  return skippable;
}

void av1_search_palette_mode_luma(const AV1_COMP *cpi, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                                  PICK_MODE_CONTEXT *ctx,
                                  RD_STATS *this_rd_cost, int64_t best_rd) {
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t best_rd_palette = best_rd, this_rd;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const intra_mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  av1_zero(pmi->palette_size);

  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  av1_rd_pick_palette_intra_sby(cpi, x, bsize, intra_mode_cost[DC_PRED],
                                &best_mbmi_palette, best_palette_color_map,
                                &best_rd_palette, &rd_stats_y.rate, NULL,
                                &rd_stats_y.dist, &rd_stats_y.skip_txfm, NULL,
                                ctx, best_blk_skip, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return;
  }

  memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
         sizeof(best_blk_skip[0]) * bsize_to_num_blk(bsize));
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  rd_stats_y.rate += ref_frame_cost;

  if (rd_stats_y.skip_txfm) {
    rd_stats_y.rate =
        ref_frame_cost +
        mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][1];
  } else {
    rd_stats_y.rate +=
        mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rd_stats_y.rate, rd_stats_y.dist);
  this_rd_cost->rate = rd_stats_y.rate;
  this_rd_cost->dist = rd_stats_y.dist;
  this_rd_cost->rdcost = this_rd;
  this_rd_cost->skip_txfm = rd_stats_y.skip_txfm;
}

/*!\brief Get the intra prediction by searching through tx_type and tx_size.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Currently this function is only used in the intra frame code path for
 * winner-mode processing.
 *
 * \return Returns whether the current mode is an improvement over best_rd.
 */
static AOM_INLINE int intra_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, const int *bmode_costs,
                                      int64_t *best_rd, int *rate,
                                      int *rate_tokenonly, int64_t *distortion,
                                      int *skippable, MB_MODE_INFO *best_mbmi,
                                      PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  RD_STATS rd_stats;
  // In order to improve txfm search avoid rd based breakouts during winner
  // mode evaluation. Hence passing ref_best_rd as a maximum value
  av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats, bsize, INT64_MAX);
  if (rd_stats.rate == INT_MAX) return 0;
  int this_rate_tokenonly = rd_stats.rate;
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
    // av1_pick_uniform_tx_size_type_yrd above includes the cost of the tx_size
    // in the tokenonly rate, but for intra blocks, tx_size is always coded
    // (prediction granularity), so we account for it in the full rate,
    // not the tokenonly rate.
    this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  const int this_rate =
      rd_stats.rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode], 0);
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_mbmi = *mbmi;
    *best_rd = this_rd;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = rd_stats.dist;
    *skippable = rd_stats.skip_txfm;
    av1_copy_array(ctx->blk_skip, x->txfm_search_info.blk_skip,
                   ctx->num_4x4_blk);
    av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    return 1;
  }
  return 0;
}

/*!\brief Search for the best filter_intra mode when coding inter frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \return Returns nothing, but updates the mbmi and rd_stats.
 */
static INLINE void handle_filter_intra_mode(const AV1_COMP *cpi, MACROBLOCK *x,
                                            BLOCK_SIZE bsize,
                                            const PICK_MODE_CONTEXT *ctx,
                                            RD_STATS *rd_stats_y, int mode_cost,
                                            int64_t best_rd,
                                            int64_t best_rd_so_far) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->mode == DC_PRED &&
         av1_filter_intra_allowed_bsize(&cpi->common, bsize));

  RD_STATS rd_stats_y_fi;
  int filter_intra_selected_flag = 0;
  TX_SIZE best_tx_size = mbmi->tx_size;
  FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
  uint8_t best_blk_skip[MAX_MIB_SIZE * MAX_MIB_SIZE];
  memcpy(best_blk_skip, x->txfm_search_info.blk_skip,
         sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED; fi_mode < FILTER_INTRA_MODES;
       ++fi_mode) {
    mbmi->filter_intra_mode_info.filter_intra_mode = fi_mode;
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats_y_fi, bsize, best_rd);
    if (rd_stats_y_fi.rate == INT_MAX) continue;
    const int this_rate_tmp =
        rd_stats_y_fi.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
    const int64_t this_rd_tmp =
        RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

    if (this_rd_tmp != INT64_MAX && this_rd_tmp / 2 > best_rd) {
      break;
    }
    if (this_rd_tmp < best_rd_so_far) {
      best_tx_size = mbmi->tx_size;
      av1_copy_array(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      memcpy(best_blk_skip, x->txfm_search_info.blk_skip,
             sizeof(best_blk_skip[0]) * ctx->num_4x4_blk);
      best_fi_mode = fi_mode;
      *rd_stats_y = rd_stats_y_fi;
      filter_intra_selected_flag = 1;
      best_rd_so_far = this_rd_tmp;
    }
  }

  mbmi->tx_size = best_tx_size;
  av1_copy_array(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(x->txfm_search_info.blk_skip, best_blk_skip,
         sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);

  if (filter_intra_selected_flag) {
    mbmi->filter_intra_mode_info.use_filter_intra = 1;
    mbmi->filter_intra_mode_info.filter_intra_mode = best_fi_mode;
  } else {
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
  }
}

// Evaluate a given luma intra-mode in inter frames.
int av1_handle_intra_y_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            const PICK_MODE_CONTEXT *ctx, RD_STATS *rd_stats_y,
                            int64_t best_rd, int *mode_cost_y, int64_t *rd_y,
                            int64_t *best_model_rd,
                            int64_t top_intra_model_rd[]) {
  const AV1_COMMON *cm = &cpi->common;
  const INTRA_MODE_SPEED_FEATURES *const intra_sf = &cpi->sf.intra_sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);
  const PREDICTION_MODE mode = mbmi->mode;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]][mode] + ref_frame_cost;
  const int skip_ctx = av1_get_skip_txfm_context(xd);

  int known_rate = mode_cost;
  const int intra_cost_penalty = av1_get_intra_cost_penalty(
      cm->quant_params.base_qindex, cm->quant_params.y_dc_delta_q,
      cm->seq_params->bit_depth);

  if (mode != DC_PRED && mode != PAETH_PRED) known_rate += intra_cost_penalty;
  known_rate += AOMMIN(mode_costs->skip_txfm_cost[skip_ctx][0],
                       mode_costs->skip_txfm_cost[skip_ctx][1]);
  const int64_t known_rd = RDCOST(x->rdmult, known_rate, 0);
  if (known_rd > best_rd) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  const int is_directional_mode = av1_is_directional_mode(mode);
  if (is_directional_mode && av1_use_angle_delta(bsize) &&
      cpi->oxcf.intra_mode_cfg.enable_angle_delta) {
    if (intra_sf->intra_pruning_with_hog &&
        !intra_search_state->dir_mode_skip_mask_ready) {
      const float thresh[4] = { -1.2f, 0.0f, 0.0f, 1.2f };
      const int is_chroma = 0;
      prune_intra_mode_with_hog(x, bsize, cm->seq_params->sb_size,
                                thresh[intra_sf->intra_pruning_with_hog - 1],
                                intra_search_state->directional_mode_skip_mask,
                                is_chroma);
      intra_search_state->dir_mode_skip_mask_ready = 1;
    }
    if (intra_search_state->directional_mode_skip_mask[mode]) return 0;
  }
  const TX_SIZE tx_size = AOMMIN(TX_32X32, max_txsize_lookup[bsize]);
  const int64_t this_model_rd =
      intra_model_rd(&cpi->common, x, 0, bsize, tx_size, /*use_hadamard=*/1);

  const int model_rd_index_for_pruning =
      get_model_rd_index_for_pruning(x, intra_sf);

  if (prune_intra_y_mode(this_model_rd, best_model_rd, top_intra_model_rd,
                         intra_sf->top_intra_model_count_allowed,
                         model_rd_index_for_pruning))
    return 0;
  av1_init_rd_stats(rd_stats_y);
  av1_pick_uniform_tx_size_type_yrd(cpi, x, rd_stats_y, bsize, best_rd);

  // Pick filter intra modes.
  if (mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
    int try_filter_intra = 1;
    int64_t best_rd_so_far = INT64_MAX;
    if (rd_stats_y->rate != INT_MAX) {
      // best_rd_so_far is the rdcost of DC_PRED without using filter_intra.
      // Later, in filter intra search, best_rd_so_far is used for comparison.
      mbmi->filter_intra_mode_info.use_filter_intra = 0;
      const int tmp_rate =
          rd_stats_y->rate +
          intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
      best_rd_so_far = RDCOST(x->rdmult, tmp_rate, rd_stats_y->dist);
      try_filter_intra = (best_rd_so_far / 2) <= best_rd;
    } else if (intra_sf->skip_filter_intra_in_inter_frames >= 1) {
      // As rd cost of luma intra dc mode is more than best_rd (i.e.,
      // rd_stats_y->rate = INT_MAX), skip the evaluation of filter intra modes.
      try_filter_intra = 0;
    }

    if (try_filter_intra) {
      handle_filter_intra_mode(cpi, x, bsize, ctx, rd_stats_y, mode_cost,
                               best_rd, best_rd_so_far);
    }
  }

  if (rd_stats_y->rate == INT_MAX) return 0;

  *mode_cost_y = intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
  const int rate_y = rd_stats_y->skip_txfm
                         ? mode_costs->skip_txfm_cost[skip_ctx][1]
                         : rd_stats_y->rate;
  *rd_y = RDCOST(x->rdmult, rate_y + *mode_cost_y, rd_stats_y->dist);
  if (best_rd < (INT64_MAX / 2) && *rd_y > (best_rd + (best_rd >> 2))) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  return 1;
}

int av1_search_intra_uv_modes_in_interframe(
    IntraModeSearchState *intra_search_state, const AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, RD_STATS *rd_stats,
    const RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv, int64_t best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);

  // TODO(chiyotsai@google.com): Consolidate the chroma search code here with
  // the one in av1_search_palette_mode.
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cm->features.allow_screen_content_tools, mbmi->bsize);

  assert(intra_search_state->rate_uv_intra == INT_MAX);
  if (intra_search_state->rate_uv_intra == INT_MAX) {
    // If no good uv-predictor had been found, search for it.
    const TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
    av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                                &intra_search_state->rate_uv_tokenonly,
                                &intra_search_state->dist_uvs,
                                &intra_search_state->skip_uvs, bsize, uv_tx);
    intra_search_state->mode_uv = mbmi->uv_mode;
    if (try_palette) intra_search_state->pmi_uv = *pmi;
    intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];

    const int uv_rate = intra_search_state->rate_uv_tokenonly;
    const int64_t uv_dist = intra_search_state->dist_uvs;
    const int64_t uv_rd = RDCOST(x->rdmult, uv_rate, uv_dist);
    if (uv_rd > best_rd) {
      // If there is no good intra uv-mode available, we can skip all intra
      // modes.
      intra_search_state->skip_intra_modes = 1;
      return 0;
    }
  }

  // If we are here, then the encoder has found at least one good intra uv
  // predictor, so we can directly copy its statistics over.
  // TODO(any): the stats here is not right if the best uv mode is CFL but the
  // best y mode is palette.
  rd_stats_uv->rate = intra_search_state->rate_uv_tokenonly;
  rd_stats_uv->dist = intra_search_state->dist_uvs;
  rd_stats_uv->skip_txfm = intra_search_state->skip_uvs;
  rd_stats->skip_txfm = rd_stats_y->skip_txfm && rd_stats_uv->skip_txfm;
  mbmi->uv_mode = intra_search_state->mode_uv;
  if (try_palette) {
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
           intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
           2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
  }
  mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;

  return 1;
}

// Checks if odd delta angles can be pruned based on rdcosts of even delta
// angles of the corresponding directional mode.
static AOM_INLINE int prune_luma_odd_delta_angles_using_rd_cost(
    const MB_MODE_INFO *const mbmi, const int64_t *const intra_modes_rd_cost,
    int64_t best_rd, int prune_luma_odd_delta_angles_in_intra) {
  const int luma_delta_angle = mbmi->angle_delta[PLANE_TYPE_Y];
  if (!prune_luma_odd_delta_angles_in_intra ||
      !av1_is_directional_mode(mbmi->mode) || !(abs(luma_delta_angle) & 1) ||
      best_rd == INT64_MAX)
    return 0;

  const int64_t rd_thresh = best_rd + (best_rd >> 3);

  // Neighbour rdcosts are considered for pruning of odd delta angles as
  // mentioned below:
  // Delta angle      Delta angle rdcost
  // to be pruned     to be considered
  //    -3                   -2
  //    -1                -2, 0
  //     1                 0, 2
  //     3                    2
  return intra_modes_rd_cost[luma_delta_angle + MAX_ANGLE_DELTA] > rd_thresh &&
         intra_modes_rd_cost[luma_delta_angle + MAX_ANGLE_DELTA + 2] >
             rd_thresh;
}

// Finds the best non-intrabc mode on an intra frame.
int64_t av1_rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int *rate, int *rate_tokenonly,
                                   int64_t *distortion, int *skippable,
                                   BLOCK_SIZE bsize, int64_t best_rd,
                                   PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  int64_t best_model_rd = INT64_MAX;
  int is_directional_mode;
  uint8_t directional_mode_skip_mask[INTRA_MODES] = { 0 };
  // Flag to check rd of any intra mode is better than best_rd passed to this
  // function
  int beat_best_rd = 0;
  const int *bmode_costs;
  const IntraModeCfg *const intra_mode_cfg = &cpi->oxcf.intra_mode_cfg;
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        mbmi->bsize);
  uint8_t *best_palette_color_map =
      try_palette ? x->palette_buffer->best_palette_color_map : NULL;
  const MB_MODE_INFO *above_mi = xd->above_mbmi;
  const MB_MODE_INFO *left_mi = xd->left_mbmi;
  const PREDICTION_MODE A = av1_above_block_mode(above_mi);
  const PREDICTION_MODE L = av1_left_block_mode(left_mi);
  const int above_ctx = intra_mode_context[A];
  const int left_ctx = intra_mode_context[L];
  bmode_costs = x->mode_costs.y_mode_costs[above_ctx][left_ctx];

  mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  const INTRA_MODE_SPEED_FEATURES *const intra_sf = &cpi->sf.intra_sf;
  if (intra_sf->intra_pruning_with_hog) {
    // Less aggressive thresholds are used here than those used in inter frame
    // encoding in av1_handle_intra_y_mode() because we want key frames/intra
    // frames to have higher quality.
    const float thresh[4] = { -1.2f, -1.2f, -0.6f, 0.4f };
    const int is_chroma = 0;
    prune_intra_mode_with_hog(x, bsize, cpi->common.seq_params->sb_size,
                              thresh[intra_sf->intra_pruning_with_hog - 1],
                              directional_mode_skip_mask, is_chroma);
  }
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  pmi->palette_size[0] = 0;

  // Set params for mode evaluation
  set_mode_eval_params(cpi, x, MODE_EVAL);

  MB_MODE_INFO best_mbmi = *mbmi;
  const int max_winner_mode_count =
      winner_mode_count_allowed[cpi->sf.winner_mode_sf.multi_winner_mode_type];
  zero_winner_mode_stats(bsize, max_winner_mode_count, x->winner_mode_stats);
  x->winner_mode_count = 0;

  // Searches the intra-modes except for intrabc, palette, and filter_intra.
  int64_t top_intra_model_rd[TOP_INTRA_MODEL_COUNT];
  for (int i = 0; i < TOP_INTRA_MODEL_COUNT; i++) {
    top_intra_model_rd[i] = INT64_MAX;
  }

  // Initialize the rdcost corresponding to all the directional and
  // non-directional intra modes.
  // 1. For directional modes, it stores the rdcost values for delta angles -4,
  // -3, ..., 3, 4.
  // 2. The rdcost value for luma_delta_angle is stored at index
  // luma_delta_angle + MAX_ANGLE_DELTA + 1.
  // 3. The rdcost values for fictitious/nonexistent luma_delta_angle -4 and 4
  // (array indices 0 and 8) are always set to INT64_MAX (the initial value).
  int64_t intra_modes_rd_cost[INTRA_MODE_END]
                             [SIZE_OF_ANGLE_DELTA_RD_COST_ARRAY];
  for (int i = 0; i < INTRA_MODE_END; i++) {
    for (int j = 0; j < SIZE_OF_ANGLE_DELTA_RD_COST_ARRAY; j++) {
      intra_modes_rd_cost[i][j] = INT64_MAX;
    }
  }

  for (int mode_idx = INTRA_MODE_START; mode_idx < LUMA_MODE_COUNT;
       ++mode_idx) {
    set_y_mode_and_delta_angle(mode_idx, mbmi,
                               intra_sf->prune_luma_odd_delta_angles_in_intra);
    RD_STATS this_rd_stats;
    int this_rate, this_rate_tokenonly, s;
    int is_diagonal_mode;
    int64_t this_distortion, this_rd;
    const int luma_delta_angle = mbmi->angle_delta[PLANE_TYPE_Y];

    is_diagonal_mode = av1_is_diagonal_mode(mbmi->mode);
    if (is_diagonal_mode && !intra_mode_cfg->enable_diagonal_intra) continue;
    if (av1_is_directional_mode(mbmi->mode) &&
        !intra_mode_cfg->enable_directional_intra)
      continue;

    // The smooth prediction mode appears to be more frequently picked
    // than horizontal / vertical smooth prediction modes. Hence treat
    // them differently in speed features.
    if ((!intra_mode_cfg->enable_smooth_intra ||
         intra_sf->disable_smooth_intra) &&
        (mbmi->mode == SMOOTH_H_PRED || mbmi->mode == SMOOTH_V_PRED))
      continue;
    if (!intra_mode_cfg->enable_smooth_intra && mbmi->mode == SMOOTH_PRED)
      continue;

    // The functionality of filter intra modes and smooth prediction
    // overlap. Hence smooth prediction is pruned only if all the
    // filter intra modes are enabled.
    if (intra_sf->disable_smooth_intra &&
        intra_sf->prune_filter_intra_level == 0 && mbmi->mode == SMOOTH_PRED)
      continue;
    if (!intra_mode_cfg->enable_paeth_intra && mbmi->mode == PAETH_PRED)
      continue;

    // Skip the evaluation of modes that do not match with the winner mode in
    // x->mb_mode_cache.
    if (x->use_mb_mode_cache && mbmi->mode != x->mb_mode_cache->mode) continue;

    is_directional_mode = av1_is_directional_mode(mbmi->mode);
    if (is_directional_mode && directional_mode_skip_mask[mbmi->mode]) continue;
    if (is_directional_mode &&
        !(av1_use_angle_delta(bsize) && intra_mode_cfg->enable_angle_delta) &&
        luma_delta_angle != 0)
      continue;

    // Use intra_y_mode_mask speed feature to skip intra mode evaluation.
    if (!(intra_sf->intra_y_mode_mask[max_txsize_lookup[bsize]] &
          (1 << mbmi->mode)))
      continue;

    if (prune_luma_odd_delta_angles_using_rd_cost(
            mbmi, intra_modes_rd_cost[mbmi->mode], best_rd,
            intra_sf->prune_luma_odd_delta_angles_in_intra))
      continue;

    const TX_SIZE tx_size = AOMMIN(TX_32X32, max_txsize_lookup[bsize]);
    const int64_t this_model_rd =
        intra_model_rd(&cpi->common, x, 0, bsize, tx_size, /*use_hadamard=*/1);

    const int model_rd_index_for_pruning =
        get_model_rd_index_for_pruning(x, intra_sf);

    if (prune_intra_y_mode(this_model_rd, &best_model_rd, top_intra_model_rd,
                           intra_sf->top_intra_model_count_allowed,
                           model_rd_index_for_pruning))
      continue;

    // Builds the actual prediction. The prediction from
    // model_intra_yrd_and_prune was just an estimation that did not take into
    // account the effect of txfm pipeline, so we need to redo it for real
    // here.
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    this_rate_tokenonly = this_rd_stats.rate;
    this_distortion = this_rd_stats.dist;
    s = this_rd_stats.skip_txfm;

    if (this_rate_tokenonly == INT_MAX) continue;

    if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
      // av1_pick_uniform_tx_size_type_yrd above includes the cost of the
      // tx_size in the tokenonly rate, but for intra blocks, tx_size is always
      // coded (prediction granularity), so we account for it in the full rate,
      // not the tokenonly rate.
      this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
    }
    this_rate =
        this_rd_stats.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode], 0);
    this_rd = RDCOST(x->rdmult, this_rate, this_distortion);

    // Visual quality adjustment based on recon vs source variance.
    if ((cpi->oxcf.mode == ALLINTRA) && (this_rd != INT64_MAX)) {
      this_rd = (int64_t)(this_rd * intra_rd_variance_factor(cpi, x, bsize));
    }

    intra_modes_rd_cost[mbmi->mode][luma_delta_angle + MAX_ANGLE_DELTA + 1] =
        this_rd;

    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        &cpi->common, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
    if (this_rd < best_rd) {
      best_mbmi = *mbmi;
      best_rd = this_rd;
      // Setting beat_best_rd flag because current mode rd is better than
      // best_rd passed to this function
      beat_best_rd = 1;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_distortion;
      *skippable = s;
      memcpy(ctx->blk_skip, x->txfm_search_info.blk_skip,
             sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
      av1_copy_array(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    }
  }

  // Searches palette
  if (try_palette) {
    av1_rd_pick_palette_intra_sby(
        cpi, x, bsize, bmode_costs[DC_PRED], &best_mbmi, best_palette_color_map,
        &best_rd, rate, rate_tokenonly, distortion, skippable, &beat_best_rd,
        ctx, ctx->blk_skip, ctx->tx_type_map);
  }

  // Searches filter_intra
  if (beat_best_rd && av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 best_mbmi.mode, &best_rd, &best_model_rd,
                                 ctx)) {
      best_mbmi = *mbmi;
    }
  }

  // No mode is identified with less rd value than best_rd passed to this
  // function. In such cases winner mode processing is not necessary and return
  // best_rd as INT64_MAX to indicate best mode is not identified
  if (!beat_best_rd) return INT64_MAX;

  // In multi-winner mode processing, perform tx search for few best modes
  // identified during mode evaluation. Winner mode processing uses best tx
  // configuration for tx search.
  if (cpi->sf.winner_mode_sf.multi_winner_mode_type) {
    int best_mode_idx = 0;
    int block_width, block_height;
    uint8_t *color_map_dst = xd->plane[PLANE_TYPE_Y].color_index_map;
    av1_get_block_dimensions(bsize, AOM_PLANE_Y, xd, &block_width,
                             &block_height, NULL, NULL);

    for (int mode_idx = 0; mode_idx < x->winner_mode_count; mode_idx++) {
      *mbmi = x->winner_mode_stats[mode_idx].mbmi;
      if (is_winner_mode_processing_enabled(cpi, x, mbmi, 0)) {
        // Restore color_map of palette mode before winner mode processing
        if (mbmi->palette_mode_info.palette_size[0] > 0) {
          uint8_t *color_map_src =
              x->winner_mode_stats[mode_idx].color_index_map;
          memcpy(color_map_dst, color_map_src,
                 block_width * block_height * sizeof(*color_map_src));
        }
        // Set params for winner mode evaluation
        set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);

        // Winner mode processing
        // If previous searches use only the default tx type/no R-D optimization
        // of quantized coeffs, do an extra search for the best tx type/better
        // R-D optimization of quantized coeffs
        if (intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                            rate_tokenonly, distortion, skippable, &best_mbmi,
                            ctx))
          best_mode_idx = mode_idx;
      }
    }
    // Copy color_map of palette mode for final winner mode
    if (best_mbmi.palette_mode_info.palette_size[0] > 0) {
      uint8_t *color_map_src =
          x->winner_mode_stats[best_mode_idx].color_index_map;
      memcpy(color_map_dst, color_map_src,
             block_width * block_height * sizeof(*color_map_src));
    }
  } else {
    // If previous searches use only the default tx type/no R-D optimization of
    // quantized coeffs, do an extra search for the best tx type/better R-D
    // optimization of quantized coeffs
    if (is_winner_mode_processing_enabled(cpi, x, mbmi, 0)) {
      // Set params for winner mode evaluation
      set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
      *mbmi = best_mbmi;
      intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                      rate_tokenonly, distortion, skippable, &best_mbmi, ctx);
    }
  }
  *mbmi = best_mbmi;
  av1_copy_array(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
  return best_rd;
}
