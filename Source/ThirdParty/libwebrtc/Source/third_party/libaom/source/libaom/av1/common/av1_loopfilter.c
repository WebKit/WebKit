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

#include <math.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"

enum {
  USE_SINGLE,
  USE_DUAL,
  USE_QUAD,
} UENUM1BYTE(USE_FILTER_TYPE);

static const SEG_LVL_FEATURES seg_lvl_lf_lut[MAX_MB_PLANE][2] = {
  { SEG_LVL_ALT_LF_Y_V, SEG_LVL_ALT_LF_Y_H },
  { SEG_LVL_ALT_LF_U, SEG_LVL_ALT_LF_U },
  { SEG_LVL_ALT_LF_V, SEG_LVL_ALT_LF_V }
};

static const int delta_lf_id_lut[MAX_MB_PLANE][2] = { { 0, 1 },
                                                      { 2, 2 },
                                                      { 3, 3 } };

static const int mode_lf_lut[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // INTRA_MODES
  1, 1, 0, 1,                             // INTER_MODES (GLOBALMV == 0)
  1, 1, 1, 1, 1, 1, 0, 1  // INTER_COMPOUND_MODES (GLOBAL_GLOBALMV == 0)
};

static void update_sharpness(loop_filter_info_n *lfi, int sharpness_lvl) {
  int lvl;

  // For each possible value for the loop filter fill out limits
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++) {
    // Set loop filter parameters that control sharpness.
    int block_inside_limit = lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));

    if (sharpness_lvl > 0) {
      if (block_inside_limit > (9 - sharpness_lvl))
        block_inside_limit = (9 - sharpness_lvl);
    }

    if (block_inside_limit < 1) block_inside_limit = 1;

    memset(lfi->lfthr[lvl].lim, block_inside_limit, SIMD_WIDTH);
    memset(lfi->lfthr[lvl].mblim, (2 * (lvl + 2) + block_inside_limit),
           SIMD_WIDTH);
  }
}

static uint8_t get_filter_level(const AV1_COMMON *cm,
                                const loop_filter_info_n *lfi_n,
                                const int dir_idx, int plane,
                                const MB_MODE_INFO *mbmi) {
  const int segment_id = mbmi->segment_id;
  if (cm->delta_q_info.delta_lf_present_flag) {
    int8_t delta_lf;
    if (cm->delta_q_info.delta_lf_multi) {
      const int delta_lf_idx = delta_lf_id_lut[plane][dir_idx];
      delta_lf = mbmi->delta_lf[delta_lf_idx];
    } else {
      delta_lf = mbmi->delta_lf_from_base;
    }
    int base_level;
    if (plane == 0)
      base_level = cm->lf.filter_level[dir_idx];
    else if (plane == 1)
      base_level = cm->lf.filter_level_u;
    else
      base_level = cm->lf.filter_level_v;
    int lvl_seg = clamp(delta_lf + base_level, 0, MAX_LOOP_FILTER);
    assert(plane >= 0 && plane <= 2);
    const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir_idx];
    if (segfeature_active(&cm->seg, segment_id, seg_lf_feature_id)) {
      const int data = get_segdata(&cm->seg, segment_id, seg_lf_feature_id);
      lvl_seg = clamp(lvl_seg + data, 0, MAX_LOOP_FILTER);
    }

    if (cm->lf.mode_ref_delta_enabled) {
      const int scale = 1 << (lvl_seg >> 5);
      lvl_seg += cm->lf.ref_deltas[mbmi->ref_frame[0]] * scale;
      if (mbmi->ref_frame[0] > INTRA_FRAME)
        lvl_seg += cm->lf.mode_deltas[mode_lf_lut[mbmi->mode]] * scale;
      lvl_seg = clamp(lvl_seg, 0, MAX_LOOP_FILTER);
    }
    return lvl_seg;
  } else {
    return lfi_n->lvl[plane][segment_id][dir_idx][mbmi->ref_frame[0]]
                     [mode_lf_lut[mbmi->mode]];
  }
}

void av1_loop_filter_init(AV1_COMMON *cm) {
  assert(MB_MODE_COUNT == NELEMENTS(mode_lf_lut));
  loop_filter_info_n *lfi = &cm->lf_info;
  struct loopfilter *lf = &cm->lf;
  int lvl;

  // init limits for given sharpness
  update_sharpness(lfi, lf->sharpness_level);

  // init hev threshold const vectors
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++)
    memset(lfi->lfthr[lvl].hev_thr, (lvl >> 4), SIMD_WIDTH);
}

// Update the loop filter for the current frame.
// This should be called before loop_filter_rows(),
// av1_loop_filter_frame() calls this function directly.
void av1_loop_filter_frame_init(AV1_COMMON *cm, int plane_start,
                                int plane_end) {
  int filt_lvl[MAX_MB_PLANE], filt_lvl_r[MAX_MB_PLANE];
  int plane;
  int seg_id;
  // n_shift is the multiplier for lf_deltas
  // the multiplier is 1 for when filter_lvl is between 0 and 31;
  // 2 when filter_lvl is between 32 and 63
  loop_filter_info_n *const lfi = &cm->lf_info;
  struct loopfilter *const lf = &cm->lf;
  const struct segmentation *const seg = &cm->seg;

  // update sharpness limits
  update_sharpness(lfi, lf->sharpness_level);

  filt_lvl[0] = cm->lf.filter_level[0];
  filt_lvl[1] = cm->lf.filter_level_u;
  filt_lvl[2] = cm->lf.filter_level_v;

  filt_lvl_r[0] = cm->lf.filter_level[1];
  filt_lvl_r[1] = cm->lf.filter_level_u;
  filt_lvl_r[2] = cm->lf.filter_level_v;

  assert(plane_start >= AOM_PLANE_Y);
  assert(plane_end <= MAX_MB_PLANE);

  for (plane = plane_start; plane < plane_end; plane++) {
    if (plane == 0 && !filt_lvl[0] && !filt_lvl_r[0])
      break;
    else if (plane == 1 && !filt_lvl[1])
      continue;
    else if (plane == 2 && !filt_lvl[2])
      continue;

    for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {
      for (int dir = 0; dir < 2; ++dir) {
        int lvl_seg = (dir == 0) ? filt_lvl[plane] : filt_lvl_r[plane];
        const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir];
        if (segfeature_active(seg, seg_id, seg_lf_feature_id)) {
          const int data = get_segdata(&cm->seg, seg_id, seg_lf_feature_id);
          lvl_seg = clamp(lvl_seg + data, 0, MAX_LOOP_FILTER);
        }

        if (!lf->mode_ref_delta_enabled) {
          // we could get rid of this if we assume that deltas are set to
          // zero when not in use; encoder always uses deltas
          memset(lfi->lvl[plane][seg_id][dir], lvl_seg,
                 sizeof(lfi->lvl[plane][seg_id][dir]));
        } else {
          int ref, mode;
          const int scale = 1 << (lvl_seg >> 5);
          const int intra_lvl = lvl_seg + lf->ref_deltas[INTRA_FRAME] * scale;
          lfi->lvl[plane][seg_id][dir][INTRA_FRAME][0] =
              clamp(intra_lvl, 0, MAX_LOOP_FILTER);

          for (ref = LAST_FRAME; ref < REF_FRAMES; ++ref) {
            for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
              const int inter_lvl = lvl_seg + lf->ref_deltas[ref] * scale +
                                    lf->mode_deltas[mode] * scale;
              lfi->lvl[plane][seg_id][dir][ref][mode] =
                  clamp(inter_lvl, 0, MAX_LOOP_FILTER);
            }
          }
        }
      }
    }
  }
}

static AOM_FORCE_INLINE TX_SIZE
get_transform_size(const MACROBLOCKD *const xd, const MB_MODE_INFO *const mbmi,
                   const int mi_row, const int mi_col, const int plane,
                   const int ss_x, const int ss_y) {
  assert(mbmi != NULL);
  if (xd && xd->lossless[mbmi->segment_id]) return TX_4X4;

  TX_SIZE tx_size = (plane == AOM_PLANE_Y)
                        ? mbmi->tx_size
                        : av1_get_max_uv_txsize(mbmi->bsize, ss_x, ss_y);
  assert(tx_size < TX_SIZES_ALL);
  if ((plane == AOM_PLANE_Y) && is_inter_block(mbmi) && !mbmi->skip_txfm) {
    const BLOCK_SIZE sb_type = mbmi->bsize;
    const int blk_row = mi_row & (mi_size_high[sb_type] - 1);
    const int blk_col = mi_col & (mi_size_wide[sb_type] - 1);
    const TX_SIZE mb_tx_size =
        mbmi->inter_tx_size[av1_get_txb_size_index(sb_type, blk_row, blk_col)];
    assert(mb_tx_size < TX_SIZES_ALL);
    tx_size = mb_tx_size;
  }

  return tx_size;
}

static const int tx_dim_to_filter_length[TX_SIZES] = { 4, 8, 14, 14, 14 };

// Return TX_SIZE from get_transform_size(), so it is plane and direction
// aware
static TX_SIZE set_lpf_parameters(
    AV1_DEBLOCKING_PARAMETERS *const params, const ptrdiff_t mode_step,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, const uint32_t x, const uint32_t y,
    const int plane, const struct macroblockd_plane *const plane_ptr) {
  // reset to initial values
  params->filter_length = 0;

  // no deblocking is required
  const uint32_t width = plane_ptr->dst.width;
  const uint32_t height = plane_ptr->dst.height;
  if ((width <= x) || (height <= y)) {
    // just return the smallest transform unit size
    return TX_4X4;
  }

  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  // for sub8x8 block, chroma prediction mode is obtained from the bottom/right
  // mi structure of the co-located 8x8 luma block. so for chroma plane, mi_row
  // and mi_col should map to the bottom/right mi structure, i.e, both mi_row
  // and mi_col should be odd number for chroma plane.
  const int mi_row = scale_vert | ((y << scale_vert) >> MI_SIZE_LOG2);
  const int mi_col = scale_horz | ((x << scale_horz) >> MI_SIZE_LOG2);
  MB_MODE_INFO **mi =
      cm->mi_params.mi_grid_base + mi_row * cm->mi_params.mi_stride + mi_col;
  const MB_MODE_INFO *mbmi = mi[0];
  // If current mbmi is not correctly setup, return an invalid value to stop
  // filtering. One example is that if this tile is not coded, then its mbmi
  // it not set up.
  if (mbmi == NULL) return TX_INVALID;

  const TX_SIZE ts = get_transform_size(xd, mi[0], mi_row, mi_col, plane,
                                        scale_horz, scale_vert);

  {
    const uint32_t coord = (VERT_EDGE == edge_dir) ? (x) : (y);
    const uint32_t transform_masks =
        edge_dir == VERT_EDGE ? tx_size_wide[ts] - 1 : tx_size_high[ts] - 1;
    const int32_t tu_edge = (coord & transform_masks) ? (0) : (1);

    if (!tu_edge) return ts;

    // prepare outer edge parameters. deblock the edge if it's an edge of a TU
    {
      const uint32_t curr_level =
          get_filter_level(cm, &cm->lf_info, edge_dir, plane, mbmi);
      const int curr_skipped = mbmi->skip_txfm && is_inter_block(mbmi);
      uint32_t level = curr_level;
      if (coord) {
        {
          const MB_MODE_INFO *const mi_prev = *(mi - mode_step);
          if (mi_prev == NULL) return TX_INVALID;
          const int pv_row =
              (VERT_EDGE == edge_dir) ? (mi_row) : (mi_row - (1 << scale_vert));
          const int pv_col =
              (VERT_EDGE == edge_dir) ? (mi_col - (1 << scale_horz)) : (mi_col);
          const TX_SIZE pv_ts = get_transform_size(
              xd, mi_prev, pv_row, pv_col, plane, scale_horz, scale_vert);

          const uint32_t pv_lvl =
              get_filter_level(cm, &cm->lf_info, edge_dir, plane, mi_prev);

          const int pv_skip_txfm =
              mi_prev->skip_txfm && is_inter_block(mi_prev);
          const BLOCK_SIZE bsize = get_plane_block_size(
              mbmi->bsize, plane_ptr->subsampling_x, plane_ptr->subsampling_y);
          assert(bsize < BLOCK_SIZES_ALL);
          const int prediction_masks = edge_dir == VERT_EDGE
                                           ? block_size_wide[bsize] - 1
                                           : block_size_high[bsize] - 1;
          const int32_t pu_edge = !(coord & prediction_masks);
          // if the current and the previous blocks are skipped,
          // deblock the edge if the edge belongs to a PU's edge only.
          if ((curr_level || pv_lvl) &&
              (!pv_skip_txfm || !curr_skipped || pu_edge)) {
            const int dim = (VERT_EDGE == edge_dir)
                                ? AOMMIN(tx_size_wide_unit_log2[ts],
                                         tx_size_wide_unit_log2[pv_ts])
                                : AOMMIN(tx_size_high_unit_log2[ts],
                                         tx_size_high_unit_log2[pv_ts]);
            if (plane) {
              params->filter_length = (dim == 0) ? 4 : 6;
            } else {
              assert(dim < TX_SIZES);
              assert(dim >= 0);
              params->filter_length = tx_dim_to_filter_length[dim];
            }

            // update the level if the current block is skipped,
            // but the previous one is not
            level = (curr_level) ? (curr_level) : (pv_lvl);
          }
        }
      }
      // prepare common parameters
      if (params->filter_length) {
        const loop_filter_thresh *const limits = cm->lf_info.lfthr + level;
        params->lfthr = limits;
      }
    }
  }

  return ts;
}

static const uint32_t vert_filter_length_luma[TX_SIZES_ALL][TX_SIZES_ALL] = {
  // TX_4X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X8
  {
      4, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8,
  },
  // TX_16X16
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_32X32
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_64X64
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_4X8
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X4
  {
      4, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8,
  },
  // TX_8X16
  {
      4, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8,
  },
  // TX_16X8
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_16X32
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_32X16
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_32X64
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_64X32
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_4X16
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_16X4
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_8X32
  {
      4, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8,
  },
  // TX_32X8
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_16X64
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
  // TX_64X16
  {
      4, 8, 14, 14, 14, 4, 8, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14, 14,
  },
};

static const uint32_t horz_filter_length_luma[TX_SIZES_ALL][TX_SIZES_ALL] = {
  // TX_4X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X8
  {
      4, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8,
  },
  // TX_16X16
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_32X32
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_64X64
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_4X8
  {
      4, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8,
  },
  // TX_8X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X16
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_16X8
  {
      4, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8,
  },
  // TX_16X32
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_32X16
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_32X64
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_64X32
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_4X16
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_16X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X32
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_32X8
  {
      4, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8,
  },
  // TX_16X64
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
  // TX_64X16
  {
      4, 8, 14, 14, 14, 8, 4, 14, 8, 14, 14, 14, 14, 14, 4, 14, 8, 14, 14,
  },
};

static const uint32_t vert_filter_length_chroma[TX_SIZES_ALL][TX_SIZES_ALL] = {
  // TX_4X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X8
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_16X16
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_32X32
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_64X64
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_4X8
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X4
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_8X16
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_16X8
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_16X32
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_32X16
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_32X64
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_64X32
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_4X16
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_16X4
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_8X32
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_32X8
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_16X64
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
  // TX_64X16
  {
      4, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6,
  },
};

static const uint32_t horz_filter_length_chroma[TX_SIZES_ALL][TX_SIZES_ALL] = {
  // TX_4X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X8
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_16X16
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_32X32
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_64X64
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_4X8
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_8X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X16
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_16X8
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_16X32
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_32X16
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_32X64
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_64X32
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_4X16
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_16X4
  {
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  },
  // TX_8X32
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_32X8
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_16X64
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
  // TX_64X16
  {
      4, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6,
  },
};

static AOM_FORCE_INLINE void set_one_param_for_line_luma(
    AV1_DEBLOCKING_PARAMETERS *const params, TX_SIZE *tx_size,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, uint32_t mi_col, uint32_t mi_row,
    const struct macroblockd_plane *const plane_ptr, int coord,
    bool is_first_block, TX_SIZE prev_tx_size, const ptrdiff_t mode_step,
    int *min_dim) {
  (void)plane_ptr;
  assert(mi_col << MI_SIZE_LOG2 < (uint32_t)plane_ptr->dst.width &&
         mi_row << MI_SIZE_LOG2 < (uint32_t)plane_ptr->dst.height);
  const int is_vert = edge_dir == VERT_EDGE;
  // reset to initial values
  params->filter_length = 0;

  MB_MODE_INFO **mi =
      cm->mi_params.mi_grid_base + mi_row * cm->mi_params.mi_stride + mi_col;
  const MB_MODE_INFO *mbmi = mi[0];
  assert(mbmi);

  const TX_SIZE ts =
      get_transform_size(xd, mi[0], mi_row, mi_col, AOM_PLANE_Y, 0, 0);

#ifndef NDEBUG
  const uint32_t transform_masks =
      is_vert ? tx_size_wide[ts] - 1 : tx_size_high[ts] - 1;
  const int32_t tu_edge = ((coord * MI_SIZE) & transform_masks) ? (0) : (1);
  assert(tu_edge);
#endif  // NDEBUG
  // If we are not the first block, then coord is always true, so
  // !is_first_block is technically redundant. But we are keeping it here so the
  // compiler can compile away this conditional if we pass in is_first_block :=
  // false
  bool curr_skipped = false;
  if (!is_first_block || coord) {
    const MB_MODE_INFO *const mi_prev = *(mi - mode_step);
    const int pv_row = is_vert ? mi_row : (mi_row - 1);
    const int pv_col = is_vert ? (mi_col - 1) : mi_col;
    const TX_SIZE pv_ts =
        is_first_block
            ? get_transform_size(xd, mi_prev, pv_row, pv_col, AOM_PLANE_Y, 0, 0)
            : prev_tx_size;
    if (is_first_block) {
      *min_dim = is_vert ? block_size_high[mi_prev->bsize]
                         : block_size_wide[mi_prev->bsize];
    }
    assert(mi_prev);
    uint8_t level =
        get_filter_level(cm, &cm->lf_info, edge_dir, AOM_PLANE_Y, mbmi);
    if (!level) {
      level =
          get_filter_level(cm, &cm->lf_info, edge_dir, AOM_PLANE_Y, mi_prev);
    }

    const int32_t pu_edge = mi_prev != mbmi;

    // The quad loop filter assumes that all the transform blocks within a
    // 8x16/16x8/16x16 prediction block are of the same size.
    assert(IMPLIES(
        !pu_edge && (mbmi->bsize >= BLOCK_8X16 && mbmi->bsize <= BLOCK_16X16),
        pv_ts == ts));

    if (!pu_edge) {
      curr_skipped = mbmi->skip_txfm && is_inter_block(mbmi);
    }
    if ((pu_edge || !curr_skipped) && level) {
      params->filter_length = is_vert ? vert_filter_length_luma[ts][pv_ts]
                                      : horz_filter_length_luma[ts][pv_ts];

      // prepare common parameters
      const loop_filter_thresh *const limits = cm->lf_info.lfthr + level;
      params->lfthr = limits;
    }
  }
  const int block_dim =
      is_vert ? block_size_high[mbmi->bsize] : block_size_wide[mbmi->bsize];
  *min_dim = AOMMIN(*min_dim, block_dim);

  *tx_size = ts;
}

// Similar to set_lpf_parameters, but does so one row/col at a time to reduce
// calls to \ref get_transform_size and \ref get_filter_level
static AOM_FORCE_INLINE void set_lpf_parameters_for_line_luma(
    AV1_DEBLOCKING_PARAMETERS *const params_buf, TX_SIZE *tx_buf,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, uint32_t mi_col, uint32_t mi_row,
    const struct macroblockd_plane *const plane_ptr, const uint32_t mi_range,
    const ptrdiff_t mode_step, int *min_dim) {
  const int is_vert = edge_dir == VERT_EDGE;

  AV1_DEBLOCKING_PARAMETERS *params = params_buf;
  TX_SIZE *tx_size = tx_buf;
  uint32_t *counter_ptr = is_vert ? &mi_col : &mi_row;
  TX_SIZE prev_tx_size = TX_INVALID;

  // Unroll the first iteration of the loop
  set_one_param_for_line_luma(params, tx_size, cm, xd, edge_dir, mi_col, mi_row,
                              plane_ptr, *counter_ptr, true, prev_tx_size,
                              mode_step, min_dim);

  // Advance
  int advance_units =
      is_vert ? tx_size_wide_unit[*tx_size] : tx_size_high_unit[*tx_size];
  prev_tx_size = *tx_size;
  *counter_ptr += advance_units;
  params += advance_units;
  tx_size += advance_units;

  while (*counter_ptr < mi_range) {
    set_one_param_for_line_luma(params, tx_size, cm, xd, edge_dir, mi_col,
                                mi_row, plane_ptr, *counter_ptr, false,
                                prev_tx_size, mode_step, min_dim);

    // Advance
    advance_units =
        is_vert ? tx_size_wide_unit[*tx_size] : tx_size_high_unit[*tx_size];
    prev_tx_size = *tx_size;
    *counter_ptr += advance_units;
    params += advance_units;
    tx_size += advance_units;
  }
}

static AOM_FORCE_INLINE void set_one_param_for_line_chroma(
    AV1_DEBLOCKING_PARAMETERS *const params, TX_SIZE *tx_size,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, uint32_t mi_col, uint32_t mi_row, int coord,
    bool is_first_block, TX_SIZE prev_tx_size,
    const struct macroblockd_plane *const plane_ptr, const ptrdiff_t mode_step,
    const int scale_horz, const int scale_vert, int *min_dim, int plane,
    int joint_filter_chroma) {
  const int is_vert = edge_dir == VERT_EDGE;
  (void)plane_ptr;
  assert((mi_col << MI_SIZE_LOG2) <
             (uint32_t)(plane_ptr->dst.width << scale_horz) &&
         (mi_row << MI_SIZE_LOG2) <
             (uint32_t)(plane_ptr->dst.height << scale_vert));
  // reset to initial values
  params->filter_length = 0;

  // for sub8x8 block, chroma prediction mode is obtained from the
  // bottom/right mi structure of the co-located 8x8 luma block. so for chroma
  // plane, mi_row and mi_col should map to the bottom/right mi structure,
  // i.e, both mi_row and mi_col should be odd number for chroma plane.
  mi_row |= scale_vert;
  mi_col |= scale_horz;
  MB_MODE_INFO **mi =
      cm->mi_params.mi_grid_base + mi_row * cm->mi_params.mi_stride + mi_col;
  const MB_MODE_INFO *mbmi = mi[0];
  assert(mbmi);

  const TX_SIZE ts = get_transform_size(xd, mi[0], mi_row, mi_col, plane,
                                        scale_horz, scale_vert);
  *tx_size = ts;

#ifndef NDEBUG
  const uint32_t transform_masks =
      is_vert ? tx_size_wide[ts] - 1 : tx_size_high[ts] - 1;
  const int32_t tu_edge = ((coord * MI_SIZE) & transform_masks) ? (0) : (1);
  assert(tu_edge);
#endif  // NDEBUG

  // If we are not the first block, then coord is always true, so
  // !is_first_block is technically redundant. But we are keeping it here so the
  // compiler can compile away this conditional if we pass in is_first_block :=
  // false
  bool curr_skipped = false;
  if (!is_first_block || coord) {
    const MB_MODE_INFO *const mi_prev = *(mi - mode_step);
    assert(mi_prev);
    const int pv_row = is_vert ? (mi_row) : (mi_row - (1 << scale_vert));
    const int pv_col = is_vert ? (mi_col - (1 << scale_horz)) : (mi_col);
    const TX_SIZE pv_ts =
        is_first_block ? get_transform_size(xd, mi_prev, pv_row, pv_col, plane,
                                            scale_horz, scale_vert)
                       : prev_tx_size;
    if (is_first_block) {
      *min_dim = is_vert ? tx_size_high[pv_ts] : tx_size_wide[pv_ts];
    }

    uint8_t level = get_filter_level(cm, &cm->lf_info, edge_dir, plane, mbmi);
    if (!level) {
      level = get_filter_level(cm, &cm->lf_info, edge_dir, plane, mi_prev);
    }
#ifndef NDEBUG
    if (joint_filter_chroma) {
      uint8_t v_level =
          get_filter_level(cm, &cm->lf_info, edge_dir, AOM_PLANE_V, mbmi);
      if (!v_level) {
        v_level =
            get_filter_level(cm, &cm->lf_info, edge_dir, AOM_PLANE_V, mi_prev);
      }
      assert(level == v_level);
    }
#else
    (void)joint_filter_chroma;
#endif  // NDEBUG
    const int32_t pu_edge = mi_prev != mbmi;

    if (!pu_edge) {
      curr_skipped = mbmi->skip_txfm && is_inter_block(mbmi);
    }
    // For realtime mode, u and v have the same level
    if ((!curr_skipped || pu_edge) && level) {
      params->filter_length = is_vert ? vert_filter_length_chroma[ts][pv_ts]
                                      : horz_filter_length_chroma[ts][pv_ts];

      const loop_filter_thresh *const limits = cm->lf_info.lfthr;
      params->lfthr = limits + level;
    }
  }
  const int tx_dim = is_vert ? tx_size_high[ts] : tx_size_wide[ts];
  *min_dim = AOMMIN(*min_dim, tx_dim);
}

static AOM_FORCE_INLINE void set_lpf_parameters_for_line_chroma(
    AV1_DEBLOCKING_PARAMETERS *const params_buf, TX_SIZE *tx_buf,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, uint32_t mi_col, uint32_t mi_row,
    const struct macroblockd_plane *const plane_ptr, const uint32_t mi_range,
    const ptrdiff_t mode_step, const int scale_horz, const int scale_vert,
    int *min_dim, int plane, int joint_filter_chroma) {
  const int is_vert = edge_dir == VERT_EDGE;

  AV1_DEBLOCKING_PARAMETERS *params = params_buf;
  TX_SIZE *tx_size = tx_buf;
  uint32_t *counter_ptr = is_vert ? &mi_col : &mi_row;
  const uint32_t scale = is_vert ? scale_horz : scale_vert;
  TX_SIZE prev_tx_size = TX_INVALID;

  // Unroll the first iteration of the loop
  set_one_param_for_line_chroma(params, tx_size, cm, xd, edge_dir, mi_col,
                                mi_row, *counter_ptr, true, prev_tx_size,
                                plane_ptr, mode_step, scale_horz, scale_vert,
                                min_dim, plane, joint_filter_chroma);

  // Advance
  int advance_units =
      is_vert ? tx_size_wide_unit[*tx_size] : tx_size_high_unit[*tx_size];
  prev_tx_size = *tx_size;
  *counter_ptr += advance_units << scale;
  params += advance_units;
  tx_size += advance_units;

  while (*counter_ptr < mi_range) {
    set_one_param_for_line_chroma(params, tx_size, cm, xd, edge_dir, mi_col,
                                  mi_row, *counter_ptr, false, prev_tx_size,
                                  plane_ptr, mode_step, scale_horz, scale_vert,
                                  min_dim, plane, joint_filter_chroma);

    // Advance
    advance_units =
        is_vert ? tx_size_wide_unit[*tx_size] : tx_size_high_unit[*tx_size];
    prev_tx_size = *tx_size;
    *counter_ptr += advance_units << scale;
    params += advance_units;
    tx_size += advance_units;
  }
}

static inline void filter_vert(uint8_t *dst, int dst_stride,
                               const AV1_DEBLOCKING_PARAMETERS *params,
                               const SequenceHeader *seq_params,
                               USE_FILTER_TYPE use_filter_type) {
  const loop_filter_thresh *limits = params->lfthr;
#if CONFIG_AV1_HIGHBITDEPTH
  const int use_highbitdepth = seq_params->use_highbitdepth;
  const aom_bit_depth_t bit_depth = seq_params->bit_depth;
  if (use_highbitdepth) {
    uint16_t *dst_shortptr = CONVERT_TO_SHORTPTR(dst);
    if (use_filter_type == USE_QUAD) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_vertical_4_dual(
              dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              limits->mblim, limits->lim, limits->hev_thr, limits->mblim,
              limits->lim, limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_vertical_6_dual(
              dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              limits->mblim, limits->lim, limits->hev_thr, limits->mblim,
              limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_vertical_8_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_vertical_8_dual(
              dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              limits->mblim, limits->lim, limits->hev_thr, limits->mblim,
              limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_vertical_14_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_vertical_14_dual(
              dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              limits->mblim, limits->lim, limits->hev_thr, limits->mblim,
              limits->lim, limits->hev_thr, bit_depth);
          break;
        // no filtering
        default: break;
      }
    } else if (use_filter_type == USE_DUAL) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_vertical_8_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_vertical_14_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // no filtering
        default: break;
      }
    } else {
      assert(use_filter_type == USE_SINGLE);
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4(dst_shortptr, dst_stride, limits->mblim,
                                    limits->lim, limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6(dst_shortptr, dst_stride, limits->mblim,
                                    limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_vertical_8(dst_shortptr, dst_stride, limits->mblim,
                                    limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_vertical_14(dst_shortptr, dst_stride, limits->mblim,
                                     limits->lim, limits->hev_thr, bit_depth);
          break;
        // no filtering
        default: break;
      }
    }
    return;
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH
  if (use_filter_type == USE_QUAD) {
    // Only one set of loop filter parameters (mblim, lim and hev_thr) is
    // passed as argument to quad loop filter because quad loop filter is
    // called for those cases where all the 4 set of loop filter parameters
    // are equal.
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4_quad(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6_quad(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_vertical_8_quad(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_vertical_14_quad(dst, dst_stride, limits->mblim, limits->lim,
                                 limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  } else if (use_filter_type == USE_DUAL) {
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4_dual(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6_dual(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_vertical_8_dual(dst, dst_stride, limits->mblim, limits->lim,
                                limits->hev_thr, limits->mblim, limits->lim,
                                limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_vertical_14_dual(dst, dst_stride, limits->mblim, limits->lim,
                                 limits->hev_thr, limits->mblim, limits->lim,
                                 limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  } else {
    assert(use_filter_type == USE_SINGLE);
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4(dst, dst_stride, limits->mblim, limits->lim,
                           limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6(dst, dst_stride, limits->mblim, limits->lim,
                           limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_vertical_8(dst, dst_stride, limits->mblim, limits->lim,
                           limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_vertical_14(dst, dst_stride, limits->mblim, limits->lim,
                            limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  }
#if !CONFIG_AV1_HIGHBITDEPTH
  (void)seq_params;
#endif  // !CONFIG_AV1_HIGHBITDEPTH
}

static inline void filter_vert_chroma(uint8_t *u_dst, uint8_t *v_dst,
                                      int dst_stride,
                                      const AV1_DEBLOCKING_PARAMETERS *params,
                                      const SequenceHeader *seq_params,
                                      USE_FILTER_TYPE use_filter_type) {
  const loop_filter_thresh *u_limits = params->lfthr;
  const loop_filter_thresh *v_limits = params->lfthr;
#if CONFIG_AV1_HIGHBITDEPTH
  const int use_highbitdepth = seq_params->use_highbitdepth;
  const aom_bit_depth_t bit_depth = seq_params->bit_depth;
  if (use_highbitdepth) {
    uint16_t *u_dst_shortptr = CONVERT_TO_SHORTPTR(u_dst);
    uint16_t *v_dst_shortptr = CONVERT_TO_SHORTPTR(v_dst);
    if (use_filter_type == USE_QUAD) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_4_dual(
              u_dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              u_limits->mblim, u_limits->lim, u_limits->hev_thr,
              u_limits->mblim, u_limits->lim, u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_4_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_4_dual(
              v_dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              v_limits->mblim, v_limits->lim, v_limits->hev_thr,
              v_limits->mblim, v_limits->lim, v_limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_6_dual(
              u_dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              u_limits->mblim, u_limits->lim, u_limits->hev_thr,
              u_limits->mblim, u_limits->lim, u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_6_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_6_dual(
              v_dst_shortptr + (2 * MI_SIZE * dst_stride), dst_stride,
              v_limits->mblim, v_limits->lim, v_limits->hev_thr,
              v_limits->mblim, v_limits->lim, v_limits->hev_thr, bit_depth);
          break;
        case 8:
        case 14: assert(0);
        // no filtering
        default: break;
      }
    } else if (use_filter_type == USE_DUAL) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_4_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_vertical_6_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 8:
        case 14: assert(0);
        // no filtering
        default: break;
      }
    } else {
      assert(use_filter_type == USE_SINGLE);
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_vertical_4(u_dst_shortptr, dst_stride, u_limits->mblim,
                                    u_limits->lim, u_limits->hev_thr,
                                    bit_depth);
          aom_highbd_lpf_vertical_4(v_dst_shortptr, dst_stride, v_limits->mblim,
                                    v_limits->lim, v_limits->hev_thr,
                                    bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_vertical_6(u_dst_shortptr, dst_stride, u_limits->mblim,
                                    u_limits->lim, u_limits->hev_thr,
                                    bit_depth);
          aom_highbd_lpf_vertical_6(v_dst_shortptr, dst_stride, v_limits->mblim,
                                    v_limits->lim, v_limits->hev_thr,
                                    bit_depth);
          break;
        case 8:
        case 14: assert(0); break;
        // no filtering
        default: break;
      }
    }
    return;
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH
  if (use_filter_type == USE_QUAD) {
    // Only one set of loop filter parameters (mblim, lim and hev_thr) is
    // passed as argument to quad loop filter because quad loop filter is
    // called for those cases where all the 4 set of loop filter parameters
    // are equal.
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4_quad(u_dst, dst_stride, u_limits->mblim,
                                u_limits->lim, u_limits->hev_thr);
        aom_lpf_vertical_4_quad(v_dst, dst_stride, v_limits->mblim,
                                v_limits->lim, v_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6_quad(u_dst, dst_stride, u_limits->mblim,
                                u_limits->lim, u_limits->hev_thr);
        aom_lpf_vertical_6_quad(v_dst, dst_stride, v_limits->mblim,
                                v_limits->lim, v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0);
      // no filtering
      default: break;
    }
  } else if (use_filter_type == USE_DUAL) {
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4_dual(u_dst, dst_stride, u_limits->mblim,
                                u_limits->lim, u_limits->hev_thr,
                                u_limits->mblim, u_limits->lim,
                                u_limits->hev_thr);
        aom_lpf_vertical_4_dual(v_dst, dst_stride, v_limits->mblim,
                                v_limits->lim, v_limits->hev_thr,
                                v_limits->mblim, v_limits->lim,
                                v_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6_dual(u_dst, dst_stride, u_limits->mblim,
                                u_limits->lim, u_limits->hev_thr,
                                u_limits->mblim, u_limits->lim,
                                u_limits->hev_thr);
        aom_lpf_vertical_6_dual(v_dst, dst_stride, v_limits->mblim,
                                v_limits->lim, v_limits->hev_thr,
                                v_limits->mblim, v_limits->lim,
                                v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0);
      // no filtering
      default: break;
    }
  } else {
    assert(use_filter_type == USE_SINGLE);
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_vertical_4(u_dst, dst_stride, u_limits->mblim, u_limits->lim,
                           u_limits->hev_thr);
        aom_lpf_vertical_4(v_dst, dst_stride, v_limits->mblim, v_limits->lim,
                           u_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_vertical_6(u_dst, dst_stride, u_limits->mblim, u_limits->lim,
                           u_limits->hev_thr);
        aom_lpf_vertical_6(v_dst, dst_stride, v_limits->mblim, v_limits->lim,
                           v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0); break;
      // no filtering
      default: break;
    }
  }
#if !CONFIG_AV1_HIGHBITDEPTH
  (void)seq_params;
#endif  // !CONFIG_AV1_HIGHBITDEPTH
}

void av1_filter_block_plane_vert(const AV1_COMMON *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col) {
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  const int plane_mi_rows =
      ROUND_POWER_OF_TWO(cm->mi_params.mi_rows, scale_vert);
  const int plane_mi_cols =
      ROUND_POWER_OF_TWO(cm->mi_params.mi_cols, scale_horz);
  const int y_range = AOMMIN((int)(plane_mi_rows - (mi_row >> scale_vert)),
                             (MAX_MIB_SIZE >> scale_vert));
  const int x_range = AOMMIN((int)(plane_mi_cols - (mi_col >> scale_horz)),
                             (MAX_MIB_SIZE >> scale_horz));

  for (int y = 0; y < y_range; y++) {
    uint8_t *p = dst_ptr + y * MI_SIZE * dst_stride;
    for (int x = 0; x < x_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will filter the vertical edge aligned with a 8x8 block.
      // If 4x4 transform is used, it will then filter the internal edge
      //  aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      uint32_t advance_units;
      TX_SIZE tx_size;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      tx_size =
          set_lpf_parameters(&params, ((ptrdiff_t)1 << scale_horz), cm, xd,
                             VERT_EDGE, curr_x, curr_y, plane, plane_ptr);
      if (tx_size == TX_INVALID) {
        params.filter_length = 0;
        tx_size = TX_4X4;
      }

      filter_vert(p, dst_stride, &params, cm->seq_params, USE_SINGLE);

      // advance the destination pointer
      advance_units = tx_size_wide_unit[tx_size];
      x += advance_units;
      p += advance_units * MI_SIZE;
    }
  }
}

void av1_filter_block_plane_vert_opt(
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col, AV1_DEBLOCKING_PARAMETERS *params_buf,
    TX_SIZE *tx_buf, int num_mis_in_lpf_unit_height_log2) {
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  // Ensure that mi_cols/mi_rows are calculated based on frame dimension aligned
  // to MI_SIZE.
  const int plane_mi_cols =
      CEIL_POWER_OF_TWO(plane_ptr->dst.width, MI_SIZE_LOG2);
  const int plane_mi_rows =
      CEIL_POWER_OF_TWO(plane_ptr->dst.height, MI_SIZE_LOG2);
  // Whenever 'pipeline_lpf_mt_with_enc' is enabled, height of the unit to
  // filter (i.e., y_range) is calculated based on the size of the superblock
  // used.
  const int y_range = AOMMIN((int)(plane_mi_rows - mi_row),
                             (1 << num_mis_in_lpf_unit_height_log2));
  // Width of the unit to filter (i.e., x_range) should always be calculated
  // based on maximum superblock size as this function is called for mi_col = 0,
  // MAX_MIB_SIZE, 2 * MAX_MIB_SIZE etc.
  const int x_range = AOMMIN((int)(plane_mi_cols - mi_col), MAX_MIB_SIZE);
  const ptrdiff_t mode_step = 1;
  for (int y = 0; y < y_range; y++) {
    const uint32_t curr_y = mi_row + y;
    const uint32_t x_start = mi_col;
    const uint32_t x_end = mi_col + x_range;
    int min_block_height = block_size_high[BLOCK_128X128];
    set_lpf_parameters_for_line_luma(params_buf, tx_buf, cm, xd, VERT_EDGE,
                                     x_start, curr_y, plane_ptr, x_end,
                                     mode_step, &min_block_height);

    AV1_DEBLOCKING_PARAMETERS *params = params_buf;
    TX_SIZE *tx_size = tx_buf;
    USE_FILTER_TYPE use_filter_type = USE_SINGLE;

    uint8_t *p = dst_ptr + y * MI_SIZE * dst_stride;

    if ((y & 3) == 0 && (y + 3) < y_range && min_block_height >= 16) {
      // If we are on a row which is a multiple of 4, and the minimum height is
      // 16 pixels, then the current and right 3 cols must contain the same
      // prediction block. This is because dim 16 can only happen every unit of
      // 4 mi's.
      use_filter_type = USE_QUAD;
      y += 3;
    } else if ((y + 1) < y_range && min_block_height >= 8) {
      use_filter_type = USE_DUAL;
      y += 1;
    }

    for (int x = 0; x < x_range;) {
      if (*tx_size == TX_INVALID) {
        params->filter_length = 0;
        *tx_size = TX_4X4;
      }

      filter_vert(p, dst_stride, params, cm->seq_params, use_filter_type);

      // advance the destination pointer
      const uint32_t advance_units = tx_size_wide_unit[*tx_size];
      x += advance_units;
      p += advance_units * MI_SIZE;
      params += advance_units;
      tx_size += advance_units;
    }
  }
}

void av1_filter_block_plane_vert_opt_chroma(
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col, AV1_DEBLOCKING_PARAMETERS *params_buf,
    TX_SIZE *tx_buf, int plane, bool joint_filter_chroma,
    int num_mis_in_lpf_unit_height_log2) {
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  const int dst_stride = plane_ptr->dst.stride;
  // Ensure that mi_cols/mi_rows are calculated based on frame dimension aligned
  // to MI_SIZE.
  const int mi_cols =
      ((plane_ptr->dst.width << scale_horz) + MI_SIZE - 1) >> MI_SIZE_LOG2;
  const int mi_rows =
      ((plane_ptr->dst.height << scale_vert) + MI_SIZE - 1) >> MI_SIZE_LOG2;
  const int plane_mi_rows = ROUND_POWER_OF_TWO(mi_rows, scale_vert);
  const int plane_mi_cols = ROUND_POWER_OF_TWO(mi_cols, scale_horz);
  const int y_range =
      AOMMIN((int)(plane_mi_rows - (mi_row >> scale_vert)),
             ((1 << num_mis_in_lpf_unit_height_log2) >> scale_vert));
  const int x_range = AOMMIN((int)(plane_mi_cols - (mi_col >> scale_horz)),
                             (MAX_MIB_SIZE >> scale_horz));
  const ptrdiff_t mode_step = (ptrdiff_t)1 << scale_horz;

  for (int y = 0; y < y_range; y++) {
    const uint32_t curr_y = mi_row + (y << scale_vert);
    const uint32_t x_start = mi_col + (0 << scale_horz);
    const uint32_t x_end = mi_col + (x_range << scale_horz);
    int min_height = tx_size_high[TX_64X64];
    set_lpf_parameters_for_line_chroma(params_buf, tx_buf, cm, xd, VERT_EDGE,
                                       x_start, curr_y, plane_ptr, x_end,
                                       mode_step, scale_horz, scale_vert,
                                       &min_height, plane, joint_filter_chroma);

    AV1_DEBLOCKING_PARAMETERS *params = params_buf;
    TX_SIZE *tx_size = tx_buf;
    int use_filter_type = USE_SINGLE;
    int y_inc = 0;

    if ((y & 3) == 0 && (y + 3) < y_range && min_height >= 16) {
      // If we are on a row which is a multiple of 4, and the minimum height is
      // 16 pixels, then the current and below 3 rows must contain the same tx
      // block. This is because dim 16 can only happen every unit of 4 mi's.
      use_filter_type = USE_QUAD;
      y_inc = 3;
    } else if (y % 2 == 0 && (y + 1) < y_range && min_height >= 8) {
      // If we are on an even row, and the minimum height is 8 pixels, then the
      // current and below rows must contain the same tx block. This is because
      // dim 4 can only happen every unit of 2**0, and 8 every unit of 2**1,
      // etc.
      use_filter_type = USE_DUAL;
      y_inc = 1;
    }

    for (int x = 0; x < x_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will filter the vertical edge aligned with a 8x8 block.
      // If 4x4 transform is used, it will then filter the internal edge
      //  aligned with a 4x4 block
      if (*tx_size == TX_INVALID) {
        params->filter_length = 0;
        *tx_size = TX_4X4;
      }

      const int offset = y * MI_SIZE * dst_stride + x * MI_SIZE;
      if (joint_filter_chroma) {
        uint8_t *u_dst = plane_ptr[0].dst.buf + offset;
        uint8_t *v_dst = plane_ptr[1].dst.buf + offset;
        filter_vert_chroma(u_dst, v_dst, dst_stride, params, cm->seq_params,
                           use_filter_type);
      } else {
        uint8_t *dst_ptr = plane_ptr->dst.buf + offset;
        filter_vert(dst_ptr, dst_stride, params, cm->seq_params,
                    use_filter_type);
      }

      // advance the destination pointer
      const uint32_t advance_units = tx_size_wide_unit[*tx_size];
      x += advance_units;
      params += advance_units;
      tx_size += advance_units;
    }
    y += y_inc;
  }
}

static inline void filter_horz(uint8_t *dst, int dst_stride,
                               const AV1_DEBLOCKING_PARAMETERS *params,
                               const SequenceHeader *seq_params,
                               USE_FILTER_TYPE use_filter_type) {
  const loop_filter_thresh *limits = params->lfthr;
#if CONFIG_AV1_HIGHBITDEPTH
  const int use_highbitdepth = seq_params->use_highbitdepth;
  const aom_bit_depth_t bit_depth = seq_params->bit_depth;
  if (use_highbitdepth) {
    uint16_t *dst_shortptr = CONVERT_TO_SHORTPTR(dst);
    if (use_filter_type == USE_QUAD) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_horizontal_4_dual(
              dst_shortptr + (2 * MI_SIZE), dst_stride, limits->mblim,
              limits->lim, limits->hev_thr, limits->mblim, limits->lim,
              limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_horizontal_6_dual(
              dst_shortptr + (2 * MI_SIZE), dst_stride, limits->mblim,
              limits->lim, limits->hev_thr, limits->mblim, limits->lim,
              limits->hev_thr, bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_horizontal_8_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_horizontal_8_dual(
              dst_shortptr + (2 * MI_SIZE), dst_stride, limits->mblim,
              limits->lim, limits->hev_thr, limits->mblim, limits->lim,
              limits->hev_thr, bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_horizontal_14_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          aom_highbd_lpf_horizontal_14_dual(
              dst_shortptr + (2 * MI_SIZE), dst_stride, limits->mblim,
              limits->lim, limits->hev_thr, limits->mblim, limits->lim,
              limits->hev_thr, bit_depth);
          break;
        // no filtering
        default: break;
      }
    } else if (use_filter_type == USE_DUAL) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_horizontal_8_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_horizontal_14_dual(
              dst_shortptr, dst_stride, limits->mblim, limits->lim,
              limits->hev_thr, limits->mblim, limits->lim, limits->hev_thr,
              bit_depth);
          break;
        // no filtering
        default: break;
      }
    } else {
      assert(use_filter_type == USE_SINGLE);
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4(dst_shortptr, dst_stride, limits->mblim,
                                      limits->lim, limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6(dst_shortptr, dst_stride, limits->mblim,
                                      limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 8-tap filtering
        case 8:
          aom_highbd_lpf_horizontal_8(dst_shortptr, dst_stride, limits->mblim,
                                      limits->lim, limits->hev_thr, bit_depth);
          break;
        // apply 14-tap filtering
        case 14:
          aom_highbd_lpf_horizontal_14(dst_shortptr, dst_stride, limits->mblim,
                                       limits->lim, limits->hev_thr, bit_depth);
          break;
        // no filtering
        default: break;
      }
    }
    return;
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH
  if (use_filter_type == USE_QUAD) {
    // Only one set of loop filter parameters (mblim, lim and hev_thr) is
    // passed as argument to quad loop filter because quad loop filter is
    // called for those cases where all the 4 set of loop filter parameters
    // are equal.
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4_quad(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6_quad(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_horizontal_8_quad(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_horizontal_14_quad(dst, dst_stride, limits->mblim, limits->lim,
                                   limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  } else if (use_filter_type == USE_DUAL) {
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4_dual(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6_dual(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_horizontal_8_dual(dst, dst_stride, limits->mblim, limits->lim,
                                  limits->hev_thr, limits->mblim, limits->lim,
                                  limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_horizontal_14_dual(dst, dst_stride, limits->mblim, limits->lim,
                                   limits->hev_thr, limits->mblim, limits->lim,
                                   limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  } else {
    assert(use_filter_type == USE_SINGLE);
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4(dst, dst_stride, limits->mblim, limits->lim,
                             limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6(dst, dst_stride, limits->mblim, limits->lim,
                             limits->hev_thr);
        break;
      // apply 8-tap filtering
      case 8:
        aom_lpf_horizontal_8(dst, dst_stride, limits->mblim, limits->lim,
                             limits->hev_thr);
        break;
      // apply 14-tap filtering
      case 14:
        aom_lpf_horizontal_14(dst, dst_stride, limits->mblim, limits->lim,
                              limits->hev_thr);
        break;
      // no filtering
      default: break;
    }
  }
#if !CONFIG_AV1_HIGHBITDEPTH
  (void)seq_params;
#endif  // !CONFIG_AV1_HIGHBITDEPTH
}

static inline void filter_horz_chroma(uint8_t *u_dst, uint8_t *v_dst,
                                      int dst_stride,
                                      const AV1_DEBLOCKING_PARAMETERS *params,
                                      const SequenceHeader *seq_params,
                                      USE_FILTER_TYPE use_filter_type) {
  const loop_filter_thresh *u_limits = params->lfthr;
  const loop_filter_thresh *v_limits = params->lfthr;
#if CONFIG_AV1_HIGHBITDEPTH
  const int use_highbitdepth = seq_params->use_highbitdepth;
  const aom_bit_depth_t bit_depth = seq_params->bit_depth;
  if (use_highbitdepth) {
    uint16_t *u_dst_shortptr = CONVERT_TO_SHORTPTR(u_dst);
    uint16_t *v_dst_shortptr = CONVERT_TO_SHORTPTR(v_dst);
    if (use_filter_type == USE_QUAD) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_4_dual(
              u_dst_shortptr + (2 * MI_SIZE), dst_stride, u_limits->mblim,
              u_limits->lim, u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_4_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_4_dual(
              v_dst_shortptr + (2 * MI_SIZE), dst_stride, v_limits->mblim,
              v_limits->lim, v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_6_dual(
              u_dst_shortptr + (2 * MI_SIZE), dst_stride, u_limits->mblim,
              u_limits->lim, u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_6_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_6_dual(
              v_dst_shortptr + (2 * MI_SIZE), dst_stride, v_limits->mblim,
              v_limits->lim, v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 8:
        case 14: assert(0);
        // no filtering
        default: break;
      }
    } else if (use_filter_type == USE_DUAL) {
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_4_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6_dual(
              u_dst_shortptr, dst_stride, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, u_limits->mblim, u_limits->lim,
              u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_6_dual(
              v_dst_shortptr, dst_stride, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, v_limits->mblim, v_limits->lim,
              v_limits->hev_thr, bit_depth);
          break;
        case 8:
        case 14: assert(0);
        // no filtering
        default: break;
      }
    } else {
      assert(use_filter_type == USE_SINGLE);
      switch (params->filter_length) {
        // apply 4-tap filtering
        case 4:
          aom_highbd_lpf_horizontal_4(u_dst_shortptr, dst_stride,
                                      u_limits->mblim, u_limits->lim,
                                      u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_4(v_dst_shortptr, dst_stride,
                                      v_limits->mblim, v_limits->lim,
                                      v_limits->hev_thr, bit_depth);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          aom_highbd_lpf_horizontal_6(u_dst_shortptr, dst_stride,
                                      u_limits->mblim, u_limits->lim,
                                      u_limits->hev_thr, bit_depth);
          aom_highbd_lpf_horizontal_6(v_dst_shortptr, dst_stride,
                                      v_limits->mblim, v_limits->lim,
                                      v_limits->hev_thr, bit_depth);
          break;
        case 8:
        case 14: assert(0); break;
        // no filtering
        default: break;
      }
    }
    return;
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH
  if (use_filter_type == USE_QUAD) {
    // Only one set of loop filter parameters (mblim, lim and hev_thr) is
    // passed as argument to quad loop filter because quad loop filter is
    // called for those cases where all the 4 set of loop filter parameters
    // are equal.
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4_quad(u_dst, dst_stride, u_limits->mblim,
                                  u_limits->lim, u_limits->hev_thr);
        aom_lpf_horizontal_4_quad(v_dst, dst_stride, v_limits->mblim,
                                  v_limits->lim, v_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6_quad(u_dst, dst_stride, u_limits->mblim,
                                  u_limits->lim, u_limits->hev_thr);
        aom_lpf_horizontal_6_quad(v_dst, dst_stride, v_limits->mblim,
                                  v_limits->lim, v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0);
      // no filtering
      default: break;
    }
  } else if (use_filter_type == USE_DUAL) {
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4_dual(u_dst, dst_stride, u_limits->mblim,
                                  u_limits->lim, u_limits->hev_thr,
                                  u_limits->mblim, u_limits->lim,
                                  u_limits->hev_thr);
        aom_lpf_horizontal_4_dual(v_dst, dst_stride, v_limits->mblim,
                                  v_limits->lim, v_limits->hev_thr,
                                  v_limits->mblim, v_limits->lim,
                                  v_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6_dual(u_dst, dst_stride, u_limits->mblim,
                                  u_limits->lim, u_limits->hev_thr,
                                  u_limits->mblim, u_limits->lim,
                                  u_limits->hev_thr);
        aom_lpf_horizontal_6_dual(v_dst, dst_stride, v_limits->mblim,
                                  v_limits->lim, v_limits->hev_thr,
                                  v_limits->mblim, v_limits->lim,
                                  v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0);
      // no filtering
      default: break;
    }
  } else {
    assert(use_filter_type == USE_SINGLE);
    switch (params->filter_length) {
      // apply 4-tap filtering
      case 4:
        aom_lpf_horizontal_4(u_dst, dst_stride, u_limits->mblim, u_limits->lim,
                             u_limits->hev_thr);
        aom_lpf_horizontal_4(v_dst, dst_stride, v_limits->mblim, v_limits->lim,
                             u_limits->hev_thr);
        break;
      case 6:  // apply 6-tap filter for chroma plane only
        aom_lpf_horizontal_6(u_dst, dst_stride, u_limits->mblim, u_limits->lim,
                             u_limits->hev_thr);
        aom_lpf_horizontal_6(v_dst, dst_stride, v_limits->mblim, v_limits->lim,
                             v_limits->hev_thr);
        break;
      case 8:
      case 14: assert(0); break;
      // no filtering
      default: break;
    }
  }
#if !CONFIG_AV1_HIGHBITDEPTH
  (void)seq_params;
#endif  // !CONFIG_AV1_HIGHBITDEPTH
}

void av1_filter_block_plane_horz(const AV1_COMMON *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col) {
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  const int plane_mi_rows =
      ROUND_POWER_OF_TWO(cm->mi_params.mi_rows, scale_vert);
  const int plane_mi_cols =
      ROUND_POWER_OF_TWO(cm->mi_params.mi_cols, scale_horz);
  const int y_range = AOMMIN((int)(plane_mi_rows - (mi_row >> scale_vert)),
                             (MAX_MIB_SIZE >> scale_vert));
  const int x_range = AOMMIN((int)(plane_mi_cols - (mi_col >> scale_horz)),
                             (MAX_MIB_SIZE >> scale_horz));
  for (int x = 0; x < x_range; x++) {
    uint8_t *p = dst_ptr + x * MI_SIZE;
    for (int y = 0; y < y_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will first filter the vertical edge aligned with a 8x8
      // block. If 4x4 transform is used, it will then filter the internal
      // edge aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      uint32_t advance_units;
      TX_SIZE tx_size;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      tx_size = set_lpf_parameters(
          &params, (cm->mi_params.mi_stride << scale_vert), cm, xd, HORZ_EDGE,
          curr_x, curr_y, plane, plane_ptr);
      if (tx_size == TX_INVALID) {
        params.filter_length = 0;
        tx_size = TX_4X4;
      }

      filter_horz(p, dst_stride, &params, cm->seq_params, USE_SINGLE);

      // advance the destination pointer
      advance_units = tx_size_high_unit[tx_size];
      y += advance_units;
      p += advance_units * dst_stride * MI_SIZE;
    }
  }
}

void av1_filter_block_plane_horz_opt(
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col, AV1_DEBLOCKING_PARAMETERS *params_buf,
    TX_SIZE *tx_buf, int num_mis_in_lpf_unit_height_log2) {
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  // Ensure that mi_cols/mi_rows are calculated based on frame dimension aligned
  // to MI_SIZE.
  const int plane_mi_cols =
      CEIL_POWER_OF_TWO(plane_ptr->dst.width, MI_SIZE_LOG2);
  const int plane_mi_rows =
      CEIL_POWER_OF_TWO(plane_ptr->dst.height, MI_SIZE_LOG2);
  const int y_range = AOMMIN((int)(plane_mi_rows - mi_row),
                             (1 << num_mis_in_lpf_unit_height_log2));
  const int x_range = AOMMIN((int)(plane_mi_cols - mi_col), MAX_MIB_SIZE);

  const ptrdiff_t mode_step = cm->mi_params.mi_stride;
  for (int x = 0; x < x_range; x++) {
    const uint32_t curr_x = mi_col + x;
    const uint32_t y_start = mi_row;
    const uint32_t y_end = mi_row + y_range;
    int min_block_width = block_size_high[BLOCK_128X128];
    set_lpf_parameters_for_line_luma(params_buf, tx_buf, cm, xd, HORZ_EDGE,
                                     curr_x, y_start, plane_ptr, y_end,
                                     mode_step, &min_block_width);

    AV1_DEBLOCKING_PARAMETERS *params = params_buf;
    TX_SIZE *tx_size = tx_buf;
    USE_FILTER_TYPE filter_type = USE_SINGLE;

    uint8_t *p = dst_ptr + x * MI_SIZE;

    if ((x & 3) == 0 && (x + 3) < x_range && min_block_width >= 16) {
      // If we are on a col which is a multiple of 4, and the minimum width is
      // 16 pixels, then the current and right 3 cols must contain the same
      // prediction block. This is because dim 16 can only happen every unit of
      // 4 mi's.
      filter_type = USE_QUAD;
      x += 3;
    } else if ((x + 1) < x_range && min_block_width >= 8) {
      filter_type = USE_DUAL;
      x += 1;
    }

    for (int y = 0; y < y_range;) {
      if (*tx_size == TX_INVALID) {
        params->filter_length = 0;
        *tx_size = TX_4X4;
      }

      filter_horz(p, dst_stride, params, cm->seq_params, filter_type);

      // advance the destination pointer
      const uint32_t advance_units = tx_size_high_unit[*tx_size];
      y += advance_units;
      p += advance_units * dst_stride * MI_SIZE;
      params += advance_units;
      tx_size += advance_units;
    }
  }
}

void av1_filter_block_plane_horz_opt_chroma(
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col, AV1_DEBLOCKING_PARAMETERS *params_buf,
    TX_SIZE *tx_buf, int plane, bool joint_filter_chroma,
    int num_mis_in_lpf_unit_height_log2) {
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  const int dst_stride = plane_ptr->dst.stride;
  // Ensure that mi_cols/mi_rows are calculated based on frame dimension aligned
  // to MI_SIZE.
  const int mi_cols =
      ((plane_ptr->dst.width << scale_horz) + MI_SIZE - 1) >> MI_SIZE_LOG2;
  const int mi_rows =
      ((plane_ptr->dst.height << scale_vert) + MI_SIZE - 1) >> MI_SIZE_LOG2;
  const int plane_mi_rows = ROUND_POWER_OF_TWO(mi_rows, scale_vert);
  const int plane_mi_cols = ROUND_POWER_OF_TWO(mi_cols, scale_horz);
  const int y_range =
      AOMMIN((int)(plane_mi_rows - (mi_row >> scale_vert)),
             ((1 << num_mis_in_lpf_unit_height_log2) >> scale_vert));
  const int x_range = AOMMIN((int)(plane_mi_cols - (mi_col >> scale_horz)),
                             (MAX_MIB_SIZE >> scale_horz));
  const ptrdiff_t mode_step = cm->mi_params.mi_stride << scale_vert;
  for (int x = 0; x < x_range; x++) {
    const uint32_t y_start = mi_row + (0 << scale_vert);
    const uint32_t curr_x = mi_col + (x << scale_horz);
    const uint32_t y_end = mi_row + (y_range << scale_vert);
    int min_width = tx_size_wide[TX_64X64];
    set_lpf_parameters_for_line_chroma(params_buf, tx_buf, cm, xd, HORZ_EDGE,
                                       curr_x, y_start, plane_ptr, y_end,
                                       mode_step, scale_horz, scale_vert,
                                       &min_width, plane, joint_filter_chroma);

    AV1_DEBLOCKING_PARAMETERS *params = params_buf;
    TX_SIZE *tx_size = tx_buf;
    USE_FILTER_TYPE use_filter_type = USE_SINGLE;
    int x_inc = 0;

    if ((x & 3) == 0 && (x + 3) < x_range && min_width >= 16) {
      // If we are on a col which is a multiple of 4, and the minimum width is
      // 16 pixels, then the current and right 3 cols must contain the same tx
      // block. This is because dim 16 can only happen every unit of 4 mi's.
      use_filter_type = USE_QUAD;
      x_inc = 3;
    } else if (x % 2 == 0 && (x + 1) < x_range && min_width >= 8) {
      // If we are on an even col, and the minimum width is 8 pixels, then the
      // current and left cols must contain the same tx block. This is because
      // dim 4 can only happen every unit of 2**0, and 8 every unit of 2**1,
      // etc.
      use_filter_type = USE_DUAL;
      x_inc = 1;
    }

    for (int y = 0; y < y_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will first filter the vertical edge aligned with a 8x8
      // block. If 4x4 transform is used, it will then filter the internal
      // edge aligned with a 4x4 block
      if (*tx_size == TX_INVALID) {
        params->filter_length = 0;
        *tx_size = TX_4X4;
      }

      const int offset = y * MI_SIZE * dst_stride + x * MI_SIZE;
      if (joint_filter_chroma) {
        uint8_t *u_dst = plane_ptr[0].dst.buf + offset;
        uint8_t *v_dst = plane_ptr[1].dst.buf + offset;
        filter_horz_chroma(u_dst, v_dst, dst_stride, params, cm->seq_params,
                           use_filter_type);
      } else {
        uint8_t *dst_ptr = plane_ptr->dst.buf + offset;
        filter_horz(dst_ptr, dst_stride, params, cm->seq_params,
                    use_filter_type);
      }

      // advance the destination pointer
      const int advance_units = tx_size_high_unit[*tx_size];
      y += advance_units;
      params += advance_units;
      tx_size += advance_units;
    }
    x += x_inc;
  }
}
