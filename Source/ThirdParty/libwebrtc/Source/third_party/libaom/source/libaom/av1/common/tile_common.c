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

#include <stdbool.h>

#include "av1/common/av1_common_int.h"
#include "av1/common/resize.h"
#include "av1/common/tile_common.h"
#include "aom_dsp/aom_dsp_common.h"

void av1_tile_init(TileInfo *tile, const AV1_COMMON *cm, int row, int col) {
  av1_tile_set_row(tile, cm, row);
  av1_tile_set_col(tile, cm, col);
}

// Find smallest k>=0 such that (blk_size << k) >= target
static int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}

void av1_get_tile_limits(AV1_COMMON *const cm) {
  const SequenceHeader *const seq_params = cm->seq_params;
  CommonTileParams *const tiles = &cm->tiles;
  const int sb_cols =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, seq_params->mib_size_log2);
  const int sb_rows =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_rows, seq_params->mib_size_log2);

  const int sb_size_log2 = seq_params->mib_size_log2 + MI_SIZE_LOG2;
  tiles->max_width_sb = MAX_TILE_WIDTH >> sb_size_log2;

#if CONFIG_CWG_C013
  bool use_level_7_above = false;
  for (int i = 0; i < seq_params->operating_points_cnt_minus_1 + 1; i++) {
    if (seq_params->seq_level_idx[i] >= SEQ_LEVEL_7_0 &&
        seq_params->seq_level_idx[i] <= SEQ_LEVEL_8_3) {
      // Currently it is assumed that levels 7.x and 8.x are either used for all
      // operating points, or none of them.
      if (i != 0 && !use_level_7_above) {
        aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Either all the operating points are levels 7.x or "
                           "8.x, or none of them are.");
      }
      use_level_7_above = true;
    }
  }
  const int max_tile_area_sb =
      (use_level_7_above ? MAX_TILE_AREA_LEVEL_7_AND_ABOVE : MAX_TILE_AREA) >>
      (2 * sb_size_log2);
#else
  const int max_tile_area_sb = MAX_TILE_AREA >> (2 * sb_size_log2);
#endif

  tiles->min_log2_cols = tile_log2(tiles->max_width_sb, sb_cols);
  tiles->max_log2_cols = tile_log2(1, AOMMIN(sb_cols, MAX_TILE_COLS));
  tiles->max_log2_rows = tile_log2(1, AOMMIN(sb_rows, MAX_TILE_ROWS));
  tiles->min_log2 = tile_log2(max_tile_area_sb, sb_cols * sb_rows);
  tiles->min_log2 = AOMMAX(tiles->min_log2, tiles->min_log2_cols);
}

void av1_calculate_tile_cols(const SequenceHeader *const seq_params,
                             int cm_mi_rows, int cm_mi_cols,
                             CommonTileParams *const tiles) {
  int sb_cols = CEIL_POWER_OF_TWO(cm_mi_cols, seq_params->mib_size_log2);
  int sb_rows = CEIL_POWER_OF_TWO(cm_mi_rows, seq_params->mib_size_log2);
  int i;

  // This will be overridden if there is at least two columns of tiles
  // (otherwise there is no inner tile width)
  tiles->min_inner_width = -1;

  if (tiles->uniform_spacing) {
    int start_sb;
    int size_sb = CEIL_POWER_OF_TWO(sb_cols, tiles->log2_cols);
    assert(size_sb > 0);
    for (i = 0, start_sb = 0; start_sb < sb_cols; i++) {
      tiles->col_start_sb[i] = start_sb;
      start_sb += size_sb;
    }
    tiles->cols = i;
    tiles->col_start_sb[i] = sb_cols;
    tiles->min_log2_rows = AOMMAX(tiles->min_log2 - tiles->log2_cols, 0);
    tiles->max_height_sb = sb_rows >> tiles->min_log2_rows;

    tiles->width = size_sb << seq_params->mib_size_log2;
    tiles->width = AOMMIN(tiles->width, cm_mi_cols);
    if (tiles->cols > 1) {
      tiles->min_inner_width = tiles->width;
    }
  } else {
    int max_tile_area_sb = (sb_rows * sb_cols);
    int widest_tile_sb = 1;
    int narrowest_inner_tile_sb = 65536;
    tiles->log2_cols = tile_log2(1, tiles->cols);
    for (i = 0; i < tiles->cols; i++) {
      int size_sb = tiles->col_start_sb[i + 1] - tiles->col_start_sb[i];
      widest_tile_sb = AOMMAX(widest_tile_sb, size_sb);
      // ignore the rightmost tile in frame for determining the narrowest
      if (i < tiles->cols - 1)
        narrowest_inner_tile_sb = AOMMIN(narrowest_inner_tile_sb, size_sb);
    }
    if (tiles->min_log2) {
      max_tile_area_sb >>= (tiles->min_log2 + 1);
    }
    tiles->max_height_sb = AOMMAX(max_tile_area_sb / widest_tile_sb, 1);
    if (tiles->cols > 1) {
      tiles->min_inner_width = narrowest_inner_tile_sb
                               << seq_params->mib_size_log2;
    }
  }
}

void av1_calculate_tile_rows(const SequenceHeader *const seq_params,
                             int cm_mi_rows, CommonTileParams *const tiles) {
  int sb_rows = CEIL_POWER_OF_TWO(cm_mi_rows, seq_params->mib_size_log2);
  int start_sb, size_sb, i;

  if (tiles->uniform_spacing) {
    size_sb = CEIL_POWER_OF_TWO(sb_rows, tiles->log2_rows);
    assert(size_sb > 0);
    for (i = 0, start_sb = 0; start_sb < sb_rows; i++) {
      tiles->row_start_sb[i] = start_sb;
      start_sb += size_sb;
    }
    tiles->rows = i;
    tiles->row_start_sb[i] = sb_rows;

    tiles->height = size_sb << seq_params->mib_size_log2;
    tiles->height = AOMMIN(tiles->height, cm_mi_rows);
  } else {
    tiles->log2_rows = tile_log2(1, tiles->rows);
  }
}

void av1_tile_set_row(TileInfo *tile, const AV1_COMMON *cm, int row) {
  assert(row < cm->tiles.rows);
  int mi_row_start = cm->tiles.row_start_sb[row]
                     << cm->seq_params->mib_size_log2;
  int mi_row_end = cm->tiles.row_start_sb[row + 1]
                   << cm->seq_params->mib_size_log2;
  tile->tile_row = row;
  tile->mi_row_start = mi_row_start;
  tile->mi_row_end = AOMMIN(mi_row_end, cm->mi_params.mi_rows);
  assert(tile->mi_row_end > tile->mi_row_start);
}

void av1_tile_set_col(TileInfo *tile, const AV1_COMMON *cm, int col) {
  assert(col < cm->tiles.cols);
  int mi_col_start = cm->tiles.col_start_sb[col]
                     << cm->seq_params->mib_size_log2;
  int mi_col_end = cm->tiles.col_start_sb[col + 1]
                   << cm->seq_params->mib_size_log2;
  tile->tile_col = col;
  tile->mi_col_start = mi_col_start;
  tile->mi_col_end = AOMMIN(mi_col_end, cm->mi_params.mi_cols);
  assert(tile->mi_col_end > tile->mi_col_start);
}

int av1_get_sb_rows_in_tile(const AV1_COMMON *cm, const TileInfo *tile) {
  return CEIL_POWER_OF_TWO(tile->mi_row_end - tile->mi_row_start,
                           cm->seq_params->mib_size_log2);
}

int av1_get_sb_cols_in_tile(const AV1_COMMON *cm, const TileInfo *tile) {
  return CEIL_POWER_OF_TWO(tile->mi_col_end - tile->mi_col_start,
                           cm->seq_params->mib_size_log2);
}

PixelRect av1_get_tile_rect(const TileInfo *tile_info, const AV1_COMMON *cm,
                            int is_uv) {
  PixelRect r;

  // Calculate position in the Y plane
  r.left = tile_info->mi_col_start * MI_SIZE;
  r.right = tile_info->mi_col_end * MI_SIZE;
  r.top = tile_info->mi_row_start * MI_SIZE;
  r.bottom = tile_info->mi_row_end * MI_SIZE;

  // If upscaling is enabled, the tile limits need scaling to match the
  // upscaled frame where the restoration units live. To do this, scale up the
  // top-left and bottom-right of the tile.
  if (av1_superres_scaled(cm)) {
    av1_calculate_unscaled_superres_size(&r.left, &r.top,
                                         cm->superres_scale_denominator);
    av1_calculate_unscaled_superres_size(&r.right, &r.bottom,
                                         cm->superres_scale_denominator);
  }

  const int frame_w = cm->superres_upscaled_width;
  const int frame_h = cm->superres_upscaled_height;

  // Make sure we don't fall off the bottom-right of the frame.
  r.right = AOMMIN(r.right, frame_w);
  r.bottom = AOMMIN(r.bottom, frame_h);

  // Convert to coordinates in the appropriate plane
  const int ss_x = is_uv && cm->seq_params->subsampling_x;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;

  r.left = ROUND_POWER_OF_TWO(r.left, ss_x);
  r.right = ROUND_POWER_OF_TWO(r.right, ss_x);
  r.top = ROUND_POWER_OF_TWO(r.top, ss_y);
  r.bottom = ROUND_POWER_OF_TWO(r.bottom, ss_y);

  return r;
}

void av1_get_uniform_tile_size(const AV1_COMMON *cm, int *w, int *h) {
  const CommonTileParams *const tiles = &cm->tiles;
  if (tiles->uniform_spacing) {
    *w = tiles->width;
    *h = tiles->height;
  } else {
    for (int i = 0; i < tiles->cols; ++i) {
      const int tile_width_sb =
          tiles->col_start_sb[i + 1] - tiles->col_start_sb[i];
      const int tile_w = tile_width_sb * cm->seq_params->mib_size;
      assert(i == 0 || tile_w == *w);  // ensure all tiles have same dimension
      *w = tile_w;
    }

    for (int i = 0; i < tiles->rows; ++i) {
      const int tile_height_sb =
          tiles->row_start_sb[i + 1] - tiles->row_start_sb[i];
      const int tile_h = tile_height_sb * cm->seq_params->mib_size;
      assert(i == 0 || tile_h == *h);  // ensure all tiles have same dimension
      *h = tile_h;
    }
  }
}

int av1_is_min_tile_width_satisfied(const AV1_COMMON *cm) {
  // Disable check if there is a single tile col in the frame
  if (cm->tiles.cols == 1) return 1;

  return ((cm->tiles.min_inner_width << MI_SIZE_LOG2) >=
          (64 << av1_superres_scaled(cm)));
}
