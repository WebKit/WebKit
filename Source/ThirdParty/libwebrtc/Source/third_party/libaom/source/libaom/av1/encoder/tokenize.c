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
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "aom_mem/aom_mem.h"

#include "av1/common/entropy.h"
#include "av1/common/pred_common.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"

static inline int av1_fast_palette_color_index_context_on_edge(
    const uint8_t *color_map, int stride, int r, int c, int *color_idx) {
  const bool has_left = (c - 1 >= 0);
  const bool has_above = (r - 1 >= 0);
  assert(r > 0 || c > 0);
  assert(has_above ^ has_left);
  assert(color_idx);
  (void)has_left;

  const uint8_t color_neighbor = has_above
                                     ? color_map[(r - 1) * stride + (c - 0)]
                                     : color_map[(r - 0) * stride + (c - 1)];
  // If the neighbor color has higher index than current color index, then we
  // move up by 1.
  const uint8_t current_color = *color_idx = color_map[r * stride + c];
  if (color_neighbor > current_color) {
    (*color_idx)++;
  } else if (color_neighbor == current_color) {
    *color_idx = 0;
  }

  // Get hash value of context.
  // The non-diagonal neighbors get a weight of 2.
  const uint8_t color_score = 2;
  const uint8_t hash_multiplier = 1;
  const uint8_t color_index_ctx_hash = color_score * hash_multiplier;

  // Lookup context from hash.
  const int color_index_ctx =
      av1_palette_color_index_context_lookup[color_index_ctx_hash];
  assert(color_index_ctx == 0);
  (void)color_index_ctx;
  return 0;
}

#define SWAP(i, j)                           \
  do {                                       \
    const uint8_t tmp_score = score_rank[i]; \
    const uint8_t tmp_color = color_rank[i]; \
    score_rank[i] = score_rank[j];           \
    color_rank[i] = color_rank[j];           \
    score_rank[j] = tmp_score;               \
    color_rank[j] = tmp_color;               \
  } while (0)
#define INVALID_COLOR_IDX (UINT8_MAX)

// A faster version of av1_get_palette_color_index_context used by the encoder
// exploiting the fact that the encoder does not need to maintain a color order.
static inline int av1_fast_palette_color_index_context(const uint8_t *color_map,
                                                       int stride, int r, int c,
                                                       int *color_idx) {
  assert(r > 0 || c > 0);

  const bool has_above = (r - 1 >= 0);
  const bool has_left = (c - 1 >= 0);
  assert(has_above || has_left);
  if (has_above ^ has_left) {
    return av1_fast_palette_color_index_context_on_edge(color_map, stride, r, c,
                                                        color_idx);
  }

  // This goes in the order of left, top, and top-left. This has the advantage
  // that unless anything here are not distinct or invalid, this will already
  // be in sorted order. Furthermore, if either of the first two is
  // invalid, we know the last one is also invalid.
  uint8_t color_neighbors[NUM_PALETTE_NEIGHBORS];
  color_neighbors[0] = color_map[(r - 0) * stride + (c - 1)];
  color_neighbors[1] = color_map[(r - 1) * stride + (c - 0)];
  color_neighbors[2] = color_map[(r - 1) * stride + (c - 1)];

  // Aggregate duplicated values.
  // Since our array is so small, using a couple if statements is faster
  uint8_t scores[NUM_PALETTE_NEIGHBORS] = { 2, 2, 1 };
  uint8_t num_invalid_colors = 0;
  if (color_neighbors[0] == color_neighbors[1]) {
    scores[0] += scores[1];
    color_neighbors[1] = INVALID_COLOR_IDX;
    num_invalid_colors += 1;

    if (color_neighbors[0] == color_neighbors[2]) {
      scores[0] += scores[2];
      num_invalid_colors += 1;
    }
  } else if (color_neighbors[0] == color_neighbors[2]) {
    scores[0] += scores[2];
    num_invalid_colors += 1;
  } else if (color_neighbors[1] == color_neighbors[2]) {
    scores[1] += scores[2];
    num_invalid_colors += 1;
  }

  const uint8_t num_valid_colors = NUM_PALETTE_NEIGHBORS - num_invalid_colors;

  uint8_t *color_rank = color_neighbors;
  uint8_t *score_rank = scores;

  // Sort everything
  if (num_valid_colors > 1) {
    if (color_neighbors[1] == INVALID_COLOR_IDX) {
      scores[1] = scores[2];
      color_neighbors[1] = color_neighbors[2];
    }

    // We need to swap the first two elements if they have the same score but
    // the color indices are not in the right order
    if (score_rank[0] < score_rank[1] ||
        (score_rank[0] == score_rank[1] && color_rank[0] > color_rank[1])) {
      SWAP(0, 1);
    }
    if (num_valid_colors > 2) {
      if (score_rank[0] < score_rank[2]) {
        SWAP(0, 2);
      }
      if (score_rank[1] < score_rank[2]) {
        SWAP(1, 2);
      }
    }
  }

  // If any of the neighbor colors has higher index than current color index,
  // then we move up by 1 unless the current color is the same as one of the
  // neighbors.
  const uint8_t current_color = *color_idx = color_map[r * stride + c];
  for (int idx = 0; idx < num_valid_colors; idx++) {
    if (color_rank[idx] > current_color) {
      (*color_idx)++;
    } else if (color_rank[idx] == current_color) {
      *color_idx = idx;
      break;
    }
  }

  // Get hash value of context.
  uint8_t color_index_ctx_hash = 0;
  static const uint8_t hash_multipliers[NUM_PALETTE_NEIGHBORS] = { 1, 2, 2 };
  for (int idx = 0; idx < num_valid_colors; ++idx) {
    color_index_ctx_hash += score_rank[idx] * hash_multipliers[idx];
  }
  assert(color_index_ctx_hash > 0);
  assert(color_index_ctx_hash <= MAX_COLOR_CONTEXT_HASH);

  // Lookup context from hash.
  const int color_index_ctx = 9 - color_index_ctx_hash;
  assert(color_index_ctx ==
         av1_palette_color_index_context_lookup[color_index_ctx_hash]);
  assert(color_index_ctx >= 0);
  assert(color_index_ctx < PALETTE_COLOR_INDEX_CONTEXTS);
  return color_index_ctx;
}
#undef INVALID_COLOR_IDX
#undef SWAP

static int cost_and_tokenize_map(Av1ColorMapParam *param, TokenExtra **t,
                                 int plane, int calc_rate, int allow_update_cdf,
                                 FRAME_COUNTS *counts) {
  const uint8_t *const color_map = param->color_map;
  MapCdf map_cdf = param->map_cdf;
  ColorCost color_cost = param->color_cost;
  const int plane_block_width = param->plane_width;
  const int rows = param->rows;
  const int cols = param->cols;
  const int n = param->n_colors;
  const int palette_size_idx = n - PALETTE_MIN_SIZE;
  int this_rate = 0;

  (void)plane;
  (void)counts;

  for (int k = 1; k < rows + cols - 1; ++k) {
    for (int j = AOMMIN(k, cols - 1); j >= AOMMAX(0, k - rows + 1); --j) {
      int i = k - j;
      int color_new_idx;
      const int color_ctx = av1_fast_palette_color_index_context(
          color_map, plane_block_width, i, j, &color_new_idx);
      assert(color_new_idx >= 0 && color_new_idx < n);
      if (calc_rate) {
        this_rate += color_cost[palette_size_idx][color_ctx][color_new_idx];
      } else {
        (*t)->token = color_new_idx;
        (*t)->color_ctx = color_ctx;
        ++(*t);
        if (allow_update_cdf)
          update_cdf(map_cdf[palette_size_idx][color_ctx], color_new_idx, n);
#if CONFIG_ENTROPY_STATS
        if (plane) {
          ++counts->palette_uv_color_index[palette_size_idx][color_ctx]
                                          [color_new_idx];
        } else {
          ++counts->palette_y_color_index[palette_size_idx][color_ctx]
                                         [color_new_idx];
        }
#endif
      }
    }
  }
  if (calc_rate) return this_rate;
  return 0;
}

static void get_palette_params(const MACROBLOCK *const x, int plane,
                               BLOCK_SIZE bsize, Av1ColorMapParam *params) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  params->color_map = xd->plane[plane].color_index_map;
  params->map_cdf = plane ? xd->tile_ctx->palette_uv_color_index_cdf
                          : xd->tile_ctx->palette_y_color_index_cdf;
  params->color_cost = plane ? x->mode_costs.palette_uv_color_cost
                             : x->mode_costs.palette_y_color_cost;
  params->n_colors = pmi->palette_size[plane];
  av1_get_block_dimensions(bsize, plane, xd, &params->plane_width, NULL,
                           &params->rows, &params->cols);
}

// TODO(any): Remove this function
static void get_color_map_params(const MACROBLOCK *const x, int plane,
                                 BLOCK_SIZE bsize, TX_SIZE tx_size,
                                 COLOR_MAP_TYPE type,
                                 Av1ColorMapParam *params) {
  (void)tx_size;
  memset(params, 0, sizeof(*params));
  switch (type) {
    case PALETTE_MAP: get_palette_params(x, plane, bsize, params); break;
    default: assert(0 && "Invalid color map type"); return;
  }
}

int av1_cost_color_map(const MACROBLOCK *const x, int plane, BLOCK_SIZE bsize,
                       TX_SIZE tx_size, COLOR_MAP_TYPE type) {
  assert(plane == 0 || plane == 1);
  Av1ColorMapParam color_map_params;
  get_color_map_params(x, plane, bsize, tx_size, type, &color_map_params);
  return cost_and_tokenize_map(&color_map_params, NULL, plane, 1, 0, NULL);
}

void av1_tokenize_color_map(const MACROBLOCK *const x, int plane,
                            TokenExtra **t, BLOCK_SIZE bsize, TX_SIZE tx_size,
                            COLOR_MAP_TYPE type, int allow_update_cdf,
                            FRAME_COUNTS *counts) {
  assert(plane == 0 || plane == 1);
  Av1ColorMapParam color_map_params;
  get_color_map_params(x, plane, bsize, tx_size, type, &color_map_params);
  // The first color index does not use context or entropy.
  (*t)->token = color_map_params.color_map[0];
  (*t)->color_ctx = -1;
  ++(*t);
  cost_and_tokenize_map(&color_map_params, t, plane, 0, allow_update_cdf,
                        counts);
}

static void tokenize_vartx(ThreadData *td, TX_SIZE tx_size,
                           BLOCK_SIZE plane_bsize, int blk_row, int blk_col,
                           int block, int plane, void *arg) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  const TX_SIZE plane_tx_size =
      plane ? av1_get_max_uv_txsize(mbmi->bsize, pd->subsampling_x,
                                    pd->subsampling_y)
            : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row,
                                                         blk_col)];

  if (tx_size == plane_tx_size || plane) {
    plane_bsize =
        get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);

    struct tokenize_b_args *args = arg;
    if (args->allow_update_cdf)
      av1_update_and_record_txb_context(plane, block, blk_row, blk_col,
                                        plane_bsize, tx_size, arg);
    else
      av1_record_txb_context(plane, block, blk_row, blk_col, plane_bsize,
                             tx_size, arg);

  } else {
    // Half the block size in transform block unit.
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int step = bsw * bsh;
    const int row_end =
        AOMMIN(tx_size_high_unit[tx_size], max_blocks_high - blk_row);
    const int col_end =
        AOMMIN(tx_size_wide_unit[tx_size], max_blocks_wide - blk_col);

    assert(bsw > 0 && bsh > 0);

    for (int row = 0; row < row_end; row += bsh) {
      const int offsetr = blk_row + row;
      for (int col = 0; col < col_end; col += bsw) {
        const int offsetc = blk_col + col;

        tokenize_vartx(td, sub_txs, plane_bsize, offsetr, offsetc, block, plane,
                       arg);
        block += step;
      }
    }
  }
}

void av1_tokenize_sb_vartx(const AV1_COMP *cpi, ThreadData *td,
                           RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                           uint8_t allow_update_cdf) {
  assert(bsize < BLOCK_SIZES_ALL);
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  if (mi_row >= cm->mi_params.mi_rows || mi_col >= cm->mi_params.mi_cols)
    return;

  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct tokenize_b_args arg = { cpi, td, 0, allow_update_cdf, dry_run };

  if (mbmi->skip_txfm) {
    av1_reset_entropy_context(xd, bsize, num_planes);
    return;
  }

  for (int plane = 0; plane < num_planes; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const int ss_x = pd->subsampling_x;
    const int ss_y = pd->subsampling_y;
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, ss_x, ss_y);
    assert(plane_bsize < BLOCK_SIZES_ALL);
    const int mi_width = mi_size_wide[plane_bsize];
    const int mi_height = mi_size_high[plane_bsize];
    const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, plane);
    const BLOCK_SIZE txb_size = txsize_to_bsize[max_tx_size];
    const int bw = mi_size_wide[txb_size];
    const int bh = mi_size_high[txb_size];
    int block = 0;
    const int step =
        tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];

    const BLOCK_SIZE max_unit_bsize =
        get_plane_block_size(BLOCK_64X64, ss_x, ss_y);
    int mu_blocks_wide = mi_size_wide[max_unit_bsize];
    int mu_blocks_high = mi_size_high[max_unit_bsize];

    mu_blocks_wide = AOMMIN(mi_width, mu_blocks_wide);
    mu_blocks_high = AOMMIN(mi_height, mu_blocks_high);

    for (int idy = 0; idy < mi_height; idy += mu_blocks_high) {
      for (int idx = 0; idx < mi_width; idx += mu_blocks_wide) {
        const int unit_height = AOMMIN(mu_blocks_high + idy, mi_height);
        const int unit_width = AOMMIN(mu_blocks_wide + idx, mi_width);
        for (int blk_row = idy; blk_row < unit_height; blk_row += bh) {
          for (int blk_col = idx; blk_col < unit_width; blk_col += bw) {
            tokenize_vartx(td, max_tx_size, plane_bsize, blk_row, blk_col,
                           block, plane, &arg);
            block += step;
          }
        }
      }
    }
  }
  if (rate) *rate += arg.this_rate;
}
