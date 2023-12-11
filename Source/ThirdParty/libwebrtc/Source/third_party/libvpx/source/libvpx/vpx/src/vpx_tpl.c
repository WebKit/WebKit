/*
 *  Copyright (c) 2023 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "vpx/vpx_codec.h"
#include "vpx/vpx_tpl.h"
#include "vpx_mem/vpx_mem.h"

#define CHECK_FPRINTF_ERROR(expr) \
  do {                            \
    if (expr < 0) {               \
      return VPX_CODEC_ERROR;     \
    }                             \
  } while (0)

#define CHECK_FSCANF_ERROR(expr, expected_value) \
  do {                                           \
    if (expr != expected_value) {                \
      return VPX_CODEC_ERROR;                    \
    }                                            \
  } while (0)

vpx_codec_err_t vpx_write_tpl_gop_stats(FILE *tpl_file,
                                        const VpxTplGopStats *tpl_gop_stats) {
  int i;
  if (tpl_file == NULL || tpl_gop_stats == NULL) return VPX_CODEC_INVALID_PARAM;
  CHECK_FPRINTF_ERROR(fprintf(tpl_file, "%d\n", tpl_gop_stats->size));

  for (i = 0; i < tpl_gop_stats->size; i++) {
    VpxTplFrameStats frame_stats = tpl_gop_stats->frame_stats_list[i];
    const int num_blocks = frame_stats.num_blocks;
    int block;
    CHECK_FPRINTF_ERROR(fprintf(tpl_file, "%d %d %d\n", frame_stats.frame_width,
                                frame_stats.frame_height, num_blocks));
    for (block = 0; block < num_blocks; block++) {
      VpxTplBlockStats block_stats = frame_stats.block_stats_list[block];
      CHECK_FPRINTF_ERROR(
          fprintf(tpl_file,
                  "%" PRId64 " %" PRId64 " %" PRId16 " %" PRId16 " %" PRId64
                  " %" PRId64 " %d\n",
                  block_stats.inter_cost, block_stats.intra_cost,
                  block_stats.mv_c, block_stats.mv_r, block_stats.recrf_dist,
                  block_stats.recrf_rate, block_stats.ref_frame_index));
    }
  }

  return VPX_CODEC_OK;
}

vpx_codec_err_t vpx_read_tpl_gop_stats(FILE *tpl_file,
                                       VpxTplGopStats *tpl_gop_stats) {
  int i, frame_list_size;
  if (tpl_file == NULL || tpl_gop_stats == NULL) return VPX_CODEC_INVALID_PARAM;
  CHECK_FSCANF_ERROR(fscanf(tpl_file, "%d\n", &frame_list_size), 1);
  tpl_gop_stats->size = frame_list_size;
  tpl_gop_stats->frame_stats_list = (VpxTplFrameStats *)vpx_calloc(
      frame_list_size, sizeof(tpl_gop_stats->frame_stats_list[0]));
  if (tpl_gop_stats->frame_stats_list == NULL) {
    return VPX_CODEC_MEM_ERROR;
  }
  for (i = 0; i < frame_list_size; i++) {
    VpxTplFrameStats *frame_stats = &tpl_gop_stats->frame_stats_list[i];
    int num_blocks, width, height, block;
    CHECK_FSCANF_ERROR(
        fscanf(tpl_file, "%d %d %d\n", &width, &height, &num_blocks), 3);
    frame_stats->num_blocks = num_blocks;
    frame_stats->frame_width = width;
    frame_stats->frame_height = height;
    frame_stats->block_stats_list = (VpxTplBlockStats *)vpx_calloc(
        num_blocks, sizeof(frame_stats->block_stats_list[0]));
    if (frame_stats->block_stats_list == NULL) {
      vpx_free_tpl_gop_stats(tpl_gop_stats);
      return VPX_CODEC_MEM_ERROR;
    }
    for (block = 0; block < num_blocks; block++) {
      VpxTplBlockStats *block_stats = &frame_stats->block_stats_list[block];
      CHECK_FSCANF_ERROR(
          fscanf(tpl_file,
                 "%" SCNd64 " %" SCNd64 " %" SCNd16 " %" SCNd16 " %" SCNd64
                 " %" SCNd64 " %d\n",
                 &block_stats->inter_cost, &block_stats->intra_cost,
                 &block_stats->mv_c, &block_stats->mv_r,
                 &block_stats->recrf_dist, &block_stats->recrf_rate,
                 &block_stats->ref_frame_index),
          7);
    }
  }

  return VPX_CODEC_OK;
}

void vpx_free_tpl_gop_stats(VpxTplGopStats *tpl_gop_stats) {
  int frame;
  if (tpl_gop_stats == NULL) return;
  for (frame = 0; frame < tpl_gop_stats->size; frame++) {
    vpx_free(tpl_gop_stats->frame_stats_list[frame].block_stats_list);
  }
  vpx_free(tpl_gop_stats->frame_stats_list);
}
