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

#include <math.h>
#include <stdlib.h>

#include "av1/common/pred_common.h"

#include "av1/encoder/block.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/intra_mode_search.h"
#include "av1/encoder/intra_mode_search_utils.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/random.h"
#include "av1/encoder/rdopt_utils.h"
#include "av1/encoder/tx_search.h"

#define AV1_K_MEANS_DIM 1
#include "av1/encoder/k_means_template.h"
#undef AV1_K_MEANS_DIM
#define AV1_K_MEANS_DIM 2
#include "av1/encoder/k_means_template.h"
#undef AV1_K_MEANS_DIM

static int int16_comparer(const void *a, const void *b) {
  return (*(int16_t *)a - *(int16_t *)b);
}

int av1_remove_duplicates(int16_t *centroids, int num_centroids) {
  int num_unique;  // number of unique centroids
  int i;
  qsort(centroids, num_centroids, sizeof(*centroids), int16_comparer);
  // Remove duplicates.
  num_unique = 1;
  for (i = 1; i < num_centroids; ++i) {
    if (centroids[i] != centroids[i - 1]) {  // found a new unique centroid
      centroids[num_unique++] = centroids[i];
    }
  }
  return num_unique;
}

static int delta_encode_cost(const int *colors, int num, int bit_depth,
                             int min_val) {
  if (num <= 0) return 0;
  int bits_cost = bit_depth;
  if (num == 1) return bits_cost;
  bits_cost += 2;
  int max_delta = 0;
  int deltas[PALETTE_MAX_SIZE];
  const int min_bits = bit_depth - 3;
  for (int i = 1; i < num; ++i) {
    const int delta = colors[i] - colors[i - 1];
    deltas[i - 1] = delta;
    assert(delta >= min_val);
    if (delta > max_delta) max_delta = delta;
  }
  int bits_per_delta = AOMMAX(av1_ceil_log2(max_delta + 1 - min_val), min_bits);
  assert(bits_per_delta <= bit_depth);
  int range = (1 << bit_depth) - colors[0] - min_val;
  for (int i = 0; i < num - 1; ++i) {
    bits_cost += bits_per_delta;
    range -= deltas[i];
    bits_per_delta = AOMMIN(bits_per_delta, av1_ceil_log2(range));
  }
  return bits_cost;
}

int av1_index_color_cache(const uint16_t *color_cache, int n_cache,
                          const uint16_t *colors, int n_colors,
                          uint8_t *cache_color_found, int *out_cache_colors) {
  if (n_cache <= 0) {
    for (int i = 0; i < n_colors; ++i) out_cache_colors[i] = colors[i];
    return n_colors;
  }
  memset(cache_color_found, 0, n_cache * sizeof(*cache_color_found));
  int n_in_cache = 0;
  int in_cache_flags[PALETTE_MAX_SIZE];
  memset(in_cache_flags, 0, sizeof(in_cache_flags));
  for (int i = 0; i < n_cache && n_in_cache < n_colors; ++i) {
    for (int j = 0; j < n_colors; ++j) {
      if (colors[j] == color_cache[i]) {
        in_cache_flags[j] = 1;
        cache_color_found[i] = 1;
        ++n_in_cache;
        break;
      }
    }
  }
  int j = 0;
  for (int i = 0; i < n_colors; ++i)
    if (!in_cache_flags[i]) out_cache_colors[j++] = colors[i];
  assert(j == n_colors - n_in_cache);
  return j;
}

int av1_get_palette_delta_bits_v(const PALETTE_MODE_INFO *const pmi,
                                 int bit_depth, int *zero_count,
                                 int *min_bits) {
  const int n = pmi->palette_size[1];
  const int max_val = 1 << bit_depth;
  int max_d = 0;
  *min_bits = bit_depth - 4;
  *zero_count = 0;
  for (int i = 1; i < n; ++i) {
    const int delta = pmi->palette_colors[2 * PALETTE_MAX_SIZE + i] -
                      pmi->palette_colors[2 * PALETTE_MAX_SIZE + i - 1];
    const int v = abs(delta);
    const int d = AOMMIN(v, max_val - v);
    if (d > max_d) max_d = d;
    if (d == 0) ++(*zero_count);
  }
  return AOMMAX(av1_ceil_log2(max_d + 1), *min_bits);
}

int av1_palette_color_cost_y(const PALETTE_MODE_INFO *const pmi,
                             const uint16_t *color_cache, int n_cache,
                             int bit_depth) {
  const int n = pmi->palette_size[0];
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache =
      av1_index_color_cache(color_cache, n_cache, pmi->palette_colors, n,
                            cache_color_found, out_cache_colors);
  const int total_bits =
      n_cache + delta_encode_cost(out_cache_colors, n_out_cache, bit_depth, 1);
  return av1_cost_literal(total_bits);
}

int av1_palette_color_cost_uv(const PALETTE_MODE_INFO *const pmi,
                              const uint16_t *color_cache, int n_cache,
                              int bit_depth) {
  const int n = pmi->palette_size[1];
  int total_bits = 0;
  // U channel palette color cost.
  int out_cache_colors[PALETTE_MAX_SIZE];
  uint8_t cache_color_found[2 * PALETTE_MAX_SIZE];
  const int n_out_cache = av1_index_color_cache(
      color_cache, n_cache, pmi->palette_colors + PALETTE_MAX_SIZE, n,
      cache_color_found, out_cache_colors);
  total_bits +=
      n_cache + delta_encode_cost(out_cache_colors, n_out_cache, bit_depth, 0);

  // V channel palette color cost.
  int zero_count = 0, min_bits_v = 0;
  const int bits_v =
      av1_get_palette_delta_bits_v(pmi, bit_depth, &zero_count, &min_bits_v);
  const int bits_using_delta =
      2 + bit_depth + (bits_v + 1) * (n - 1) - zero_count;
  const int bits_using_raw = bit_depth * n;
  total_bits += 1 + AOMMIN(bits_using_delta, bits_using_raw);
  return av1_cost_literal(total_bits);
}

// Extends 'color_map' array from 'orig_width x orig_height' to 'new_width x
// new_height'. Extra rows and columns are filled in by copying last valid
// row/column.
static AOM_INLINE void extend_palette_color_map(uint8_t *const color_map,
                                                int orig_width, int orig_height,
                                                int new_width, int new_height) {
  int j;
  assert(new_width >= orig_width);
  assert(new_height >= orig_height);
  if (new_width == orig_width && new_height == orig_height) return;

  for (j = orig_height - 1; j >= 0; --j) {
    memmove(color_map + j * new_width, color_map + j * orig_width, orig_width);
    // Copy last column to extra columns.
    memset(color_map + j * new_width + orig_width,
           color_map[j * new_width + orig_width - 1], new_width - orig_width);
  }
  // Copy last row to extra rows.
  for (j = orig_height; j < new_height; ++j) {
    memcpy(color_map + j * new_width, color_map + (orig_height - 1) * new_width,
           new_width);
  }
}

// Bias toward using colors in the cache.
// TODO(huisu): Try other schemes to improve compression.
static AOM_INLINE void optimize_palette_colors(uint16_t *color_cache,
                                               int n_cache, int n_colors,
                                               int stride, int16_t *centroids,
                                               int bit_depth) {
  if (n_cache <= 0) return;
  for (int i = 0; i < n_colors * stride; i += stride) {
    int min_diff = abs((int)centroids[i] - (int)color_cache[0]);
    int idx = 0;
    for (int j = 1; j < n_cache; ++j) {
      const int this_diff = abs((int)centroids[i] - (int)color_cache[j]);
      if (this_diff < min_diff) {
        min_diff = this_diff;
        idx = j;
      }
    }
    const int min_threshold = 4 << (bit_depth - 8);
    if (min_diff <= min_threshold) centroids[i] = color_cache[idx];
  }
}

/*!\brief Calculate the luma palette cost from a given color palette
 *
 * \ingroup palette_mode_search
 * \callergraph
 * Given the base colors as specified in centroids[], calculate the RD cost
 * of palette mode.
 */
static AOM_INLINE void palette_rd_y(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int16_t *data, int16_t *centroids,
    int n, uint16_t *color_cache, int n_cache, bool do_header_rd_based_gating,
    MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map, int64_t *best_rd,
    int *rate, int *rate_tokenonly, int64_t *distortion, uint8_t *skippable,
    int *beat_best_rd, PICK_MODE_CONTEXT *ctx, uint8_t *blk_skip,
    uint8_t *tx_type_map, int *beat_best_palette_rd,
    bool *do_header_rd_based_breakout, int discount_color_cost) {
  if (do_header_rd_based_breakout != NULL) *do_header_rd_based_breakout = false;
  optimize_palette_colors(color_cache, n_cache, n, 1, centroids,
                          cpi->common.seq_params->bit_depth);
  const int num_unique_colors = av1_remove_duplicates(centroids, n);
  if (num_unique_colors < PALETTE_MIN_SIZE) {
    // Too few unique colors to create a palette. And DC_PRED will work
    // well for that case anyway. So skip.
    return;
  }
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  if (cpi->common.seq_params->use_highbitdepth) {
    for (int i = 0; i < num_unique_colors; ++i) {
      pmi->palette_colors[i] = clip_pixel_highbd(
          (int)centroids[i], cpi->common.seq_params->bit_depth);
    }
  } else {
    for (int i = 0; i < num_unique_colors; ++i) {
      pmi->palette_colors[i] = clip_pixel(centroids[i]);
    }
  }
  pmi->palette_size[0] = num_unique_colors;
  MACROBLOCKD *const xd = &x->e_mbd;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);
  av1_calc_indices(data, centroids, color_map, rows * cols, num_unique_colors,
                   1);
  extend_palette_color_map(color_map, cols, rows, block_width, block_height);

  RD_STATS tokenonly_rd_stats;
  int this_rate;

  if (do_header_rd_based_gating) {
    assert(do_header_rd_based_breakout != NULL);
    const int palette_mode_rate = intra_mode_info_cost_y(
        cpi, x, mbmi, bsize, dc_mode_cost, discount_color_cost);
    const int64_t header_rd = RDCOST(x->rdmult, palette_mode_rate, 0);
    // Less aggressive pruning when prune_luma_palette_size_search_level == 1.
    const int header_rd_shift =
        (cpi->sf.intra_sf.prune_luma_palette_size_search_level == 1) ? 1 : 0;
    // Terminate further palette_size search, if the header cost corresponding
    // to lower palette_size is more than *best_rd << header_rd_shift. This
    // logic is implemented with a right shift in the LHS to prevent a possible
    // overflow with the left shift in RHS.
    if ((header_rd >> header_rd_shift) > *best_rd) {
      *do_header_rd_based_breakout = true;
      return;
    }
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                      *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) return;
    this_rate = tokenonly_rd_stats.rate + palette_mode_rate;
  } else {
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                      *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) return;
    this_rate = tokenonly_rd_stats.rate +
                intra_mode_info_cost_y(cpi, x, mbmi, bsize, dc_mode_cost,
                                       discount_color_cost);
  }

  int64_t this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
    tokenonly_rd_stats.rate -= tx_size_cost(x, bsize, mbmi->tx_size);
  }
  // Collect mode stats for multiwinner mode processing
  const int txfm_search_done = 1;
  store_winner_mode_stats(
      &cpi->common, x, mbmi, NULL, NULL, NULL, THR_DC, color_map, bsize,
      this_rd, cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    // Setting beat_best_rd flag because current mode rd is better than best_rd.
    // This flag need to be updated only for palette evaluation in key frames
    if (beat_best_rd) *beat_best_rd = 1;
    memcpy(best_palette_color_map, color_map,
           block_width * block_height * sizeof(color_map[0]));
    *best_mbmi = *mbmi;
    memcpy(blk_skip, x->txfm_search_info.blk_skip,
           sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
    av1_copy_array(tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    if (rate) *rate = this_rate;
    if (rate_tokenonly) *rate_tokenonly = tokenonly_rd_stats.rate;
    if (distortion) *distortion = tokenonly_rd_stats.dist;
    if (skippable) *skippable = tokenonly_rd_stats.skip_txfm;
    if (beat_best_palette_rd) *beat_best_palette_rd = 1;
  }
}

static AOM_INLINE int is_iter_over(int curr_idx, int end_idx, int step_size) {
  assert(step_size != 0);
  return (step_size > 0) ? curr_idx >= end_idx : curr_idx <= end_idx;
}

// Performs count-based palette search with number of colors in interval
// [start_n, end_n) with step size step_size. If step_size < 0, then end_n can
// be less than start_n. Saves the last numbers searched in last_n_searched and
// returns the best number of colors found.
static AOM_INLINE int perform_top_color_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int16_t *data,
    int16_t *top_colors, int start_n, int end_n, int step_size,
    bool do_header_rd_based_gating, int *last_n_searched, uint16_t *color_cache,
    int n_cache, MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map,
    int64_t *best_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    uint8_t *skippable, int *beat_best_rd, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip, uint8_t *tx_type_map, int discount_color_cost) {
  int16_t centroids[PALETTE_MAX_SIZE];
  int n = start_n;
  int top_color_winner = end_n;
  /* clang-format off */
  assert(IMPLIES(step_size < 0, start_n > end_n));
  /* clang-format on */
  assert(IMPLIES(step_size > 0, start_n < end_n));
  while (!is_iter_over(n, end_n, step_size)) {
    int beat_best_palette_rd = 0;
    bool do_header_rd_based_breakout = false;
    memcpy(centroids, top_colors, n * sizeof(top_colors[0]));
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, do_header_rd_based_gating, best_mbmi,
                 best_palette_color_map, best_rd, rate, rate_tokenonly,
                 distortion, skippable, beat_best_rd, ctx, best_blk_skip,
                 tx_type_map, &beat_best_palette_rd,
                 &do_header_rd_based_breakout, discount_color_cost);
    *last_n_searched = n;
    if (do_header_rd_based_breakout) {
      // Terminate palette_size search by setting last_n_searched to end_n.
      *last_n_searched = end_n;
      break;
    }
    if (beat_best_palette_rd) {
      top_color_winner = n;
    } else if (cpi->sf.intra_sf.prune_palette_search_level == 2) {
      // At search level 2, we return immediately if we don't see an improvement
      return top_color_winner;
    }
    n += step_size;
  }
  return top_color_winner;
}

// Performs k-means based palette search with number of colors in interval
// [start_n, end_n) with step size step_size. If step_size < 0, then end_n can
// be less than start_n. Saves the last numbers searched in last_n_searched and
// returns the best number of colors found.
static AOM_INLINE int perform_k_means_palette_search(
    const AV1_COMP *const cpi, MACROBLOCK *x, MB_MODE_INFO *mbmi,
    BLOCK_SIZE bsize, int dc_mode_cost, const int16_t *data, int lower_bound,
    int upper_bound, int start_n, int end_n, int step_size,
    bool do_header_rd_based_gating, int *last_n_searched, uint16_t *color_cache,
    int n_cache, MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map,
    int64_t *best_rd, int *rate, int *rate_tokenonly, int64_t *distortion,
    uint8_t *skippable, int *beat_best_rd, PICK_MODE_CONTEXT *ctx,
    uint8_t *best_blk_skip, uint8_t *tx_type_map, uint8_t *color_map,
    int data_points, int discount_color_cost) {
  int16_t centroids[PALETTE_MAX_SIZE];
  const int max_itr = 50;
  int n = start_n;
  int top_color_winner = end_n;
  /* clang-format off */
  assert(IMPLIES(step_size < 0, start_n > end_n));
  /* clang-format on */
  assert(IMPLIES(step_size > 0, start_n < end_n));
  while (!is_iter_over(n, end_n, step_size)) {
    int beat_best_palette_rd = 0;
    bool do_header_rd_based_breakout = false;
    for (int i = 0; i < n; ++i) {
      centroids[i] =
          lower_bound + (2 * i + 1) * (upper_bound - lower_bound) / n / 2;
    }
    av1_k_means(data, centroids, color_map, data_points, n, 1, max_itr);
    palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, n,
                 color_cache, n_cache, do_header_rd_based_gating, best_mbmi,
                 best_palette_color_map, best_rd, rate, rate_tokenonly,
                 distortion, skippable, beat_best_rd, ctx, best_blk_skip,
                 tx_type_map, &beat_best_palette_rd,
                 &do_header_rd_based_breakout, discount_color_cost);
    *last_n_searched = n;
    if (do_header_rd_based_breakout) {
      // Terminate palette_size search by setting last_n_searched to end_n.
      *last_n_searched = end_n;
      break;
    }
    if (beat_best_palette_rd) {
      top_color_winner = n;
    } else if (cpi->sf.intra_sf.prune_palette_search_level == 2) {
      // At search level 2, we return immediately if we don't see an improvement
      return top_color_winner;
    }
    n += step_size;
  }
  return top_color_winner;
}

// Sets the parameters to search the current number of colors +- 1
static AOM_INLINE void set_stage2_params(int *min_n, int *max_n, int *step_size,
                                         int winner, int end_n) {
  // Set min to winner - 1 unless we are already at the border, then we set it
  // to winner + 1
  *min_n = (winner == PALETTE_MIN_SIZE) ? (PALETTE_MIN_SIZE + 1)
                                        : AOMMAX(winner - 1, PALETTE_MIN_SIZE);
  // Set max to winner + 1 unless we are already at the border, then we set it
  // to winner - 1
  *max_n =
      (winner == end_n) ? (winner - 1) : AOMMIN(winner + 1, PALETTE_MAX_SIZE);

  // Set the step size to max_n - min_n so we only search those two values.
  // If max_n == min_n, then set step_size to 1 to avoid infinite loop later.
  *step_size = AOMMAX(1, *max_n - *min_n);
}

static AOM_INLINE void fill_data_and_get_bounds(const uint8_t *src,
                                                const int src_stride,
                                                const int rows, const int cols,
                                                const int is_high_bitdepth,
                                                int16_t *data, int *lower_bound,
                                                int *upper_bound) {
  if (is_high_bitdepth) {
    const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);
    *lower_bound = *upper_bound = src_ptr[0];
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        const int val = src_ptr[c];
        data[c] = (int16_t)val;
        *lower_bound = AOMMIN(*lower_bound, val);
        *upper_bound = AOMMAX(*upper_bound, val);
      }
      src_ptr += src_stride;
      data += cols;
    }
    return;
  }

  // low bit depth
  *lower_bound = *upper_bound = src[0];
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int val = src[c];
      data[c] = (int16_t)val;
      *lower_bound = AOMMIN(*lower_bound, val);
      *upper_bound = AOMMAX(*upper_bound, val);
    }
    src += src_stride;
    data += cols;
  }
}

/*! \brief Colors are sorted by their count: the higher the better.
 */
struct ColorCount {
  //! Color index in the histogram.
  int index;
  //! Histogram count.
  int count;
};

static int color_count_comp(const void *c1, const void *c2) {
  const struct ColorCount *color_count1 = (const struct ColorCount *)c1;
  const struct ColorCount *color_count2 = (const struct ColorCount *)c2;
  if (color_count1->count > color_count2->count) return -1;
  if (color_count1->count < color_count2->count) return 1;
  if (color_count1->index < color_count2->index) return -1;
  return 1;
}

static void find_top_colors(const int *const count_buf, int bit_depth,
                            int n_colors, int16_t *top_colors) {
  // Top color array, serving as a priority queue if more than n_colors are
  // found.
  struct ColorCount top_color_counts[PALETTE_MAX_SIZE] = { { 0 } };
  int n_color_count = 0;
  for (int i = 0; i < (1 << bit_depth); ++i) {
    if (count_buf[i] > 0) {
      if (n_color_count < n_colors) {
        // Keep adding to the top colors.
        top_color_counts[n_color_count].index = i;
        top_color_counts[n_color_count].count = count_buf[i];
        ++n_color_count;
        if (n_color_count == n_colors) {
          qsort(top_color_counts, n_colors, sizeof(top_color_counts[0]),
                color_count_comp);
        }
      } else {
        // Check the worst in the sorted top.
        if (count_buf[i] > top_color_counts[n_colors - 1].count) {
          int j = n_colors - 1;
          // Move up to the best one.
          while (j >= 1 && count_buf[i] > top_color_counts[j - 1].count) --j;
          memmove(top_color_counts + j + 1, top_color_counts + j,
                  (n_colors - j - 1) * sizeof(top_color_counts[0]));
          top_color_counts[j].index = i;
          top_color_counts[j].count = count_buf[i];
        }
      }
    }
  }
  assert(n_color_count == n_colors);

  for (int i = 0; i < n_colors; ++i) {
    top_colors[i] = top_color_counts[i].index;
  }
}

void av1_rd_pick_palette_intra_sby(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int dc_mode_cost,
    MB_MODE_INFO *best_mbmi, uint8_t *best_palette_color_map, int64_t *best_rd,
    int *rate, int *rate_tokenonly, int64_t *distortion, uint8_t *skippable,
    int *beat_best_rd, PICK_MODE_CONTEXT *ctx, uint8_t *best_blk_skip,
    uint8_t *tx_type_map) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                           bsize));
  assert(PALETTE_MAX_SIZE == 8);
  assert(PALETTE_MIN_SIZE == 2);

  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const src = x->plane[0].src.buf;
  int block_width, block_height, rows, cols;
  av1_get_block_dimensions(bsize, 0, xd, &block_width, &block_height, &rows,
                           &cols);
  const SequenceHeader *const seq_params = cpi->common.seq_params;
  const int is_hbd = seq_params->use_highbitdepth;
  const int bit_depth = seq_params->bit_depth;
  const int discount_color_cost = cpi->sf.rt_sf.use_nonrd_pick_mode;
  int unused;

  int count_buf[1 << 12];  // Maximum (1 << 12) color levels.
  int colors, colors_threshold = 0;
  if (is_hbd) {
    int count_buf_8bit[1 << 8];  // Maximum (1 << 8) bins for hbd path.
    av1_count_colors_highbd(src, src_stride, rows, cols, bit_depth, count_buf,
                            count_buf_8bit, &colors_threshold, &colors);
  } else {
    av1_count_colors(src, src_stride, rows, cols, count_buf, &colors);
    colors_threshold = colors;
  }

  uint8_t *const color_map = xd->plane[0].color_index_map;
  int color_thresh_palette = 64;
  // Allow for larger color_threshold for palette search, based on color,
  // scene_change, and block source variance.
  // Since palette is Y based, only allow larger threshold if block
  // color_dist is below threshold.
  if (cpi->sf.rt_sf.use_nonrd_pick_mode &&
      cpi->sf.rt_sf.increase_color_thresh_palette && cpi->rc.high_source_sad &&
      x->source_variance > 50) {
    int64_t norm_color_dist = 0;
    if (x->color_sensitivity[0] || x->color_sensitivity[1]) {
      norm_color_dist = x->min_dist_inter_uv >>
                        (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
      if (x->color_sensitivity[0] && x->color_sensitivity[1])
        norm_color_dist = norm_color_dist >> 1;
    }
    if (norm_color_dist < 8000) color_thresh_palette += 20;
  }
  if (colors_threshold > 1 && colors_threshold <= color_thresh_palette) {
    int16_t *const data = x->palette_buffer->kmeans_data_buf;
    int16_t centroids[PALETTE_MAX_SIZE];
    int lower_bound, upper_bound;
    fill_data_and_get_bounds(src, src_stride, rows, cols, is_hbd, data,
                             &lower_bound, &upper_bound);

    mbmi->mode = DC_PRED;
    mbmi->filter_intra_mode_info.use_filter_intra = 0;

    uint16_t color_cache[2 * PALETTE_MAX_SIZE];
    const int n_cache = av1_get_palette_cache(xd, 0, color_cache);

    // Find the dominant colors, stored in top_colors[].
    int16_t top_colors[PALETTE_MAX_SIZE] = { 0 };
    find_top_colors(count_buf, bit_depth, AOMMIN(colors, PALETTE_MAX_SIZE),
                    top_colors);

    // The following are the approaches used for header rdcost based gating
    // for early termination for different values of prune_palette_search_level.
    // 0: Pruning based on header rdcost for ascending order palette_size
    // search.
    // 1: When colors > PALETTE_MIN_SIZE, enabled only for coarse palette_size
    // search and for finer search do_header_rd_based_gating parameter is
    // explicitly passed as 'false'.
    // 2: Enabled only for ascending order palette_size search and for
    // descending order search do_header_rd_based_gating parameter is explicitly
    // passed as 'false'.
    const bool do_header_rd_based_gating =
        cpi->sf.intra_sf.prune_luma_palette_size_search_level != 0;

    // TODO(huisu@google.com): Try to avoid duplicate computation in cases
    // where the dominant colors and the k-means results are similar.
    if ((cpi->sf.intra_sf.prune_palette_search_level == 1) &&
        (colors > PALETTE_MIN_SIZE)) {
      // Start index and step size below are chosen to evaluate unique
      // candidates in neighbor search, in case a winner candidate is found in
      // coarse search. Example,
      // 1) 8 colors (end_n = 8): 2,3,4,5,6,7,8. start_n is chosen as 2 and step
      // size is chosen as 3. Therefore, coarse search will evaluate 2, 5 and 8.
      // If winner is found at 5, then 4 and 6 are evaluated. Similarly, for 2
      // (3) and 8 (7).
      // 2) 7 colors (end_n = 7): 2,3,4,5,6,7. If start_n is chosen as 2 (same
      // as for 8 colors) then step size should also be 2, to cover all
      // candidates. Coarse search will evaluate 2, 4 and 6. If winner is either
      // 2 or 4, 3 will be evaluated. Instead, if start_n=3 and step_size=3,
      // coarse search will evaluate 3 and 6. For the winner, unique neighbors
      // (3: 2,4 or 6: 5,7) would be evaluated.

      // Start index for coarse palette search for dominant colors and k-means
      const uint8_t start_n_lookup_table[PALETTE_MAX_SIZE + 1] = { 0, 0, 0,
                                                                   3, 3, 2,
                                                                   3, 3, 2 };
      // Step size for coarse palette search for dominant colors and k-means
      const uint8_t step_size_lookup_table[PALETTE_MAX_SIZE + 1] = { 0, 0, 0,
                                                                     3, 3, 3,
                                                                     3, 3, 3 };

      // Choose the start index and step size for coarse search based on number
      // of colors
      const int max_n = AOMMIN(colors, PALETTE_MAX_SIZE);
      const int min_n = start_n_lookup_table[max_n];
      const int step_size = step_size_lookup_table[max_n];
      assert(min_n >= PALETTE_MIN_SIZE);
      // Perform top color coarse palette search to find the winner candidate
      const int top_color_winner = perform_top_color_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, min_n, max_n + 1,
          step_size, do_header_rd_based_gating, &unused, color_cache, n_cache,
          best_mbmi, best_palette_color_map, best_rd, rate, rate_tokenonly,
          distortion, skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
          discount_color_cost);
      // Evaluate neighbors for the winner color (if winner is found) in the
      // above coarse search for dominant colors
      if (top_color_winner <= max_n) {
        int stage2_min_n, stage2_max_n, stage2_step_size;
        set_stage2_params(&stage2_min_n, &stage2_max_n, &stage2_step_size,
                          top_color_winner, max_n);
        // perform finer search for the winner candidate
        perform_top_color_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, stage2_min_n,
            stage2_max_n + 1, stage2_step_size,
            /*do_header_rd_based_gating=*/false, &unused, color_cache, n_cache,
            best_mbmi, best_palette_color_map, best_rd, rate, rate_tokenonly,
            distortion, skippable, beat_best_rd, ctx, best_blk_skip,
            tx_type_map, discount_color_cost);
      }
      // K-means clustering.
      // Perform k-means coarse palette search to find the winner candidate
      const int k_means_winner = perform_k_means_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, lower_bound, upper_bound,
          min_n, max_n + 1, step_size, do_header_rd_based_gating, &unused,
          color_cache, n_cache, best_mbmi, best_palette_color_map, best_rd,
          rate, rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
          best_blk_skip, tx_type_map, color_map, rows * cols,
          discount_color_cost);
      // Evaluate neighbors for the winner color (if winner is found) in the
      // above coarse search for k-means
      if (k_means_winner <= max_n) {
        int start_n_stage2, end_n_stage2, step_size_stage2;
        set_stage2_params(&start_n_stage2, &end_n_stage2, &step_size_stage2,
                          k_means_winner, max_n);
        // perform finer search for the winner candidate
        perform_k_means_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, lower_bound, upper_bound,
            start_n_stage2, end_n_stage2 + 1, step_size_stage2,
            /*do_header_rd_based_gating=*/false, &unused, color_cache, n_cache,
            best_mbmi, best_palette_color_map, best_rd, rate, rate_tokenonly,
            distortion, skippable, beat_best_rd, ctx, best_blk_skip,
            tx_type_map, color_map, rows * cols, discount_color_cost);
      }
    } else {
      const int max_n = AOMMIN(colors, PALETTE_MAX_SIZE),
                min_n = PALETTE_MIN_SIZE;
      // Perform top color palette search in ascending order
      int last_n_searched = min_n;
      perform_top_color_palette_search(
          cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, min_n, max_n + 1,
          1, do_header_rd_based_gating, &last_n_searched, color_cache, n_cache,
          best_mbmi, best_palette_color_map, best_rd, rate, rate_tokenonly,
          distortion, skippable, beat_best_rd, ctx, best_blk_skip, tx_type_map,
          discount_color_cost);
      if (last_n_searched < max_n) {
        // Search in descending order until we get to the previous best
        perform_top_color_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, top_colors, max_n,
            last_n_searched, -1, /*do_header_rd_based_gating=*/false, &unused,
            color_cache, n_cache, best_mbmi, best_palette_color_map, best_rd,
            rate, rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
            best_blk_skip, tx_type_map, discount_color_cost);
      }
      // K-means clustering.
      if (colors == PALETTE_MIN_SIZE) {
        // Special case: These colors automatically become the centroids.
        assert(colors == 2);
        centroids[0] = lower_bound;
        centroids[1] = upper_bound;
        palette_rd_y(cpi, x, mbmi, bsize, dc_mode_cost, data, centroids, colors,
                     color_cache, n_cache, /*do_header_rd_based_gating=*/false,
                     best_mbmi, best_palette_color_map, best_rd, rate,
                     rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
                     best_blk_skip, tx_type_map, NULL, NULL,
                     discount_color_cost);
      } else {
        // Perform k-means palette search in ascending order
        last_n_searched = min_n;
        perform_k_means_palette_search(
            cpi, x, mbmi, bsize, dc_mode_cost, data, lower_bound, upper_bound,
            min_n, max_n + 1, 1, do_header_rd_based_gating, &last_n_searched,
            color_cache, n_cache, best_mbmi, best_palette_color_map, best_rd,
            rate, rate_tokenonly, distortion, skippable, beat_best_rd, ctx,
            best_blk_skip, tx_type_map, color_map, rows * cols,
            discount_color_cost);
        if (last_n_searched < max_n) {
          // Search in descending order until we get to the previous best
          perform_k_means_palette_search(
              cpi, x, mbmi, bsize, dc_mode_cost, data, lower_bound, upper_bound,
              max_n, last_n_searched, -1, /*do_header_rd_based_gating=*/false,
              &unused, color_cache, n_cache, best_mbmi, best_palette_color_map,
              best_rd, rate, rate_tokenonly, distortion, skippable,
              beat_best_rd, ctx, best_blk_skip, tx_type_map, color_map,
              rows * cols, discount_color_cost);
        }
      }
    }
  }

  if (best_mbmi->palette_mode_info.palette_size[0] > 0) {
    memcpy(color_map, best_palette_color_map,
           block_width * block_height * sizeof(best_palette_color_map[0]));
    // Gather the stats to determine whether to use screen content tools in
    // function av1_determine_sc_tools_with_encoding().
    x->palette_pixels += (block_width * block_height);
  }
  *mbmi = *best_mbmi;
}

void av1_rd_pick_palette_intra_sbuv(const AV1_COMP *cpi, MACROBLOCK *x,
                                    int dc_mode_cost,
                                    uint8_t *best_palette_color_map,
                                    MB_MODE_INFO *const best_mbmi,
                                    int64_t *best_rd, int *rate,
                                    int *rate_tokenonly, int64_t *distortion,
                                    uint8_t *skippable) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  assert(av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                           mbmi->bsize));
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->bsize;
  const SequenceHeader *const seq_params = cpi->common.seq_params;
  int this_rate;
  int64_t this_rd;
  int colors_u, colors_v;
  int colors_threshold_u = 0, colors_threshold_v = 0, colors_threshold = 0;
  const int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  uint8_t *const color_map = xd->plane[1].color_index_map;
  RD_STATS tokenonly_rd_stats;
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);

  mbmi->uv_mode = UV_DC_PRED;
  if (seq_params->use_highbitdepth) {
    int count_buf[1 << 12];      // Maximum (1 << 12) color levels.
    int count_buf_8bit[1 << 8];  // Maximum (1 << 8) bins for hbd path.
    av1_count_colors_highbd(src_u, src_stride, rows, cols,
                            seq_params->bit_depth, count_buf, count_buf_8bit,
                            &colors_threshold_u, &colors_u);
    av1_count_colors_highbd(src_v, src_stride, rows, cols,
                            seq_params->bit_depth, count_buf, count_buf_8bit,
                            &colors_threshold_v, &colors_v);
  } else {
    int count_buf[1 << 8];
    av1_count_colors(src_u, src_stride, rows, cols, count_buf, &colors_u);
    av1_count_colors(src_v, src_stride, rows, cols, count_buf, &colors_v);
    colors_threshold_u = colors_u;
    colors_threshold_v = colors_v;
  }

  uint16_t color_cache[2 * PALETTE_MAX_SIZE];
  const int n_cache = av1_get_palette_cache(xd, 1, color_cache);

  colors_threshold = colors_threshold_u > colors_threshold_v
                         ? colors_threshold_u
                         : colors_threshold_v;
  if (colors_threshold > 1 && colors_threshold <= 64) {
    int r, c, n, i, j;
    const int max_itr = 50;
    int lb_u, ub_u, val_u;
    int lb_v, ub_v, val_v;
    int16_t *const data = x->palette_buffer->kmeans_data_buf;
    int16_t centroids[2 * PALETTE_MAX_SIZE];

    uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src_u);
    uint16_t *src_v16 = CONVERT_TO_SHORTPTR(src_v);
    if (seq_params->use_highbitdepth) {
      lb_u = src_u16[0];
      ub_u = src_u16[0];
      lb_v = src_v16[0];
      ub_v = src_v16[0];
    } else {
      lb_u = src_u[0];
      ub_u = src_u[0];
      lb_v = src_v[0];
      ub_v = src_v[0];
    }

    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) {
        if (seq_params->use_highbitdepth) {
          val_u = src_u16[r * src_stride + c];
          val_v = src_v16[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        } else {
          val_u = src_u[r * src_stride + c];
          val_v = src_v[r * src_stride + c];
          data[(r * cols + c) * 2] = val_u;
          data[(r * cols + c) * 2 + 1] = val_v;
        }
        if (val_u < lb_u)
          lb_u = val_u;
        else if (val_u > ub_u)
          ub_u = val_u;
        if (val_v < lb_v)
          lb_v = val_v;
        else if (val_v > ub_v)
          ub_v = val_v;
      }
    }

    const int colors = colors_u > colors_v ? colors_u : colors_v;
    const int max_colors =
        colors > PALETTE_MAX_SIZE ? PALETTE_MAX_SIZE : colors;
    for (n = PALETTE_MIN_SIZE; n <= max_colors; ++n) {
      for (i = 0; i < n; ++i) {
        centroids[i * 2] = lb_u + (2 * i + 1) * (ub_u - lb_u) / n / 2;
        centroids[i * 2 + 1] = lb_v + (2 * i + 1) * (ub_v - lb_v) / n / 2;
      }
      av1_k_means(data, centroids, color_map, rows * cols, n, 2, max_itr);
      optimize_palette_colors(color_cache, n_cache, n, 2, centroids,
                              cpi->common.seq_params->bit_depth);
      // Sort the U channel colors in ascending order.
      for (i = 0; i < 2 * (n - 1); i += 2) {
        int min_idx = i;
        int min_val = centroids[i];
        for (j = i + 2; j < 2 * n; j += 2)
          if (centroids[j] < min_val) min_val = centroids[j], min_idx = j;
        if (min_idx != i) {
          int temp_u = centroids[i], temp_v = centroids[i + 1];
          centroids[i] = centroids[min_idx];
          centroids[i + 1] = centroids[min_idx + 1];
          centroids[min_idx] = temp_u, centroids[min_idx + 1] = temp_v;
        }
      }
      av1_calc_indices(data, centroids, color_map, rows * cols, n, 2);
      extend_palette_color_map(color_map, cols, rows, plane_block_width,
                               plane_block_height);
      pmi->palette_size[1] = n;
      for (i = 1; i < 3; ++i) {
        for (j = 0; j < n; ++j) {
          if (seq_params->use_highbitdepth)
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] = clip_pixel_highbd(
                (int)centroids[j * 2 + i - 1], seq_params->bit_depth);
          else
            pmi->palette_colors[i * PALETTE_MAX_SIZE + j] =
                clip_pixel((int)centroids[j * 2 + i - 1]);
        }
      }

      if (cpi->sf.intra_sf.early_term_chroma_palette_size_search) {
        const int palette_mode_rate =
            intra_mode_info_cost_uv(cpi, x, mbmi, bsize, dc_mode_cost);
        const int64_t header_rd = RDCOST(x->rdmult, palette_mode_rate, 0);
        // Terminate further palette_size search, if header cost corresponding
        // to lower palette_size is more than the best_rd.
        if (header_rd >= *best_rd) break;
        av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
        if (tokenonly_rd_stats.rate == INT_MAX) continue;
        this_rate = tokenonly_rd_stats.rate + palette_mode_rate;
      } else {
        av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, *best_rd);
        if (tokenonly_rd_stats.rate == INT_MAX) continue;
        this_rate = tokenonly_rd_stats.rate +
                    intra_mode_info_cost_uv(cpi, x, mbmi, bsize, dc_mode_cost);
      }

      this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
      if (this_rd < *best_rd) {
        *best_rd = this_rd;
        *best_mbmi = *mbmi;
        memcpy(best_palette_color_map, color_map,
               plane_block_width * plane_block_height *
                   sizeof(best_palette_color_map[0]));
        *rate = this_rate;
        *distortion = tokenonly_rd_stats.dist;
        *rate_tokenonly = tokenonly_rd_stats.rate;
        *skippable = tokenonly_rd_stats.skip_txfm;
      }
    }
  }
  if (best_mbmi->palette_mode_info.palette_size[1] > 0) {
    memcpy(color_map, best_palette_color_map,
           plane_block_width * plane_block_height *
               sizeof(best_palette_color_map[0]));
  }
}

void av1_restore_uv_color_map(const AV1_COMP *cpi, MACROBLOCK *x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const BLOCK_SIZE bsize = mbmi->bsize;
  int src_stride = x->plane[1].src.stride;
  const uint8_t *const src_u = x->plane[1].src.buf;
  const uint8_t *const src_v = x->plane[2].src.buf;
  int16_t *const data = x->palette_buffer->kmeans_data_buf;
  int16_t centroids[2 * PALETTE_MAX_SIZE];
  uint8_t *const color_map = xd->plane[1].color_index_map;
  int r, c;
  const uint16_t *const src_u16 = CONVERT_TO_SHORTPTR(src_u);
  const uint16_t *const src_v16 = CONVERT_TO_SHORTPTR(src_v);
  int plane_block_width, plane_block_height, rows, cols;
  av1_get_block_dimensions(bsize, 1, xd, &plane_block_width,
                           &plane_block_height, &rows, &cols);

  for (r = 0; r < rows; ++r) {
    for (c = 0; c < cols; ++c) {
      if (cpi->common.seq_params->use_highbitdepth) {
        data[(r * cols + c) * 2] = src_u16[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v16[r * src_stride + c];
      } else {
        data[(r * cols + c) * 2] = src_u[r * src_stride + c];
        data[(r * cols + c) * 2 + 1] = src_v[r * src_stride + c];
      }
    }
  }

  for (r = 1; r < 3; ++r) {
    for (c = 0; c < pmi->palette_size[1]; ++c) {
      centroids[c * 2 + r - 1] = pmi->palette_colors[r * PALETTE_MAX_SIZE + c];
    }
  }

  av1_calc_indices(data, centroids, color_map, rows * cols,
                   pmi->palette_size[1], 2);
  extend_palette_color_map(color_map, cols, rows, plane_block_width,
                           plane_block_height);
}
