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

#include "config/aom_config.h"

#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/decoder/detokenize.h"

#define ACCT_STR __func__

#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/idct.h"

static void decode_color_map_tokens(Av1ColorMapParam *param, aom_reader *r) {
  uint8_t color_order[PALETTE_MAX_SIZE];
  const int n = param->n_colors;
  uint8_t *const color_map = param->color_map;
  MapCdf color_map_cdf = param->map_cdf;
  int plane_block_width = param->plane_width;
  int plane_block_height = param->plane_height;
  int rows = param->rows;
  int cols = param->cols;

  // The first color index.
  color_map[0] = av1_read_uniform(r, n);
  assert(color_map[0] < n);

  // Run wavefront on the palette map index decoding.
  for (int i = 1; i < rows + cols - 1; ++i) {
    for (int j = AOMMIN(i, cols - 1); j >= AOMMAX(0, i - rows + 1); --j) {
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, (i - j), j, n, color_order, NULL);
      const int color_idx = aom_read_symbol(
          r, color_map_cdf[n - PALETTE_MIN_SIZE][color_ctx], n, ACCT_STR);
      assert(color_idx >= 0 && color_idx < n);
      color_map[(i - j) * plane_block_width + j] = color_order[color_idx];
    }
  }
  // Copy last column to extra columns.
  if (cols < plane_block_width) {
    for (int i = 0; i < rows; ++i) {
      memset(color_map + i * plane_block_width + cols,
             color_map[i * plane_block_width + cols - 1],
             (plane_block_width - cols));
    }
  }
  // Copy last row to extra rows.
  for (int i = rows; i < plane_block_height; ++i) {
    memcpy(color_map + i * plane_block_width,
           color_map + (rows - 1) * plane_block_width, plane_block_width);
  }
}

void av1_decode_palette_tokens(MACROBLOCKD *const xd, int plane,
                               aom_reader *r) {
  assert(plane == 0 || plane == 1);
  Av1ColorMapParam params;
  params.color_map =
      xd->plane[plane].color_index_map + xd->color_index_map_offset[plane];
  params.map_cdf = plane ? xd->tile_ctx->palette_uv_color_index_cdf
                         : xd->tile_ctx->palette_y_color_index_cdf;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  params.n_colors = mbmi->palette_mode_info.palette_size[plane];
  av1_get_block_dimensions(mbmi->bsize, plane, xd, &params.plane_width,
                           &params.plane_height, &params.rows, &params.cols);
  decode_color_map_tokens(&params, r);
}
