/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
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
#include <stdbool.h>
#include <stdio.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"

#include "av1/common/reconinter.h"
#include "av1/common/blockd.h"

#include "av1/encoder/encodeframe.h"
#include "av1/encoder/var_based_part.h"
#include "av1/encoder/reconinter_enc.h"

extern const uint8_t AV1_VAR_OFFS[];

// Possible values for the force_split variable while evaluating variance based
// partitioning.
enum {
  // Evaluate all partition types
  PART_EVAL_ALL = 0,
  // Force PARTITION_SPLIT
  PART_EVAL_ONLY_SPLIT = 1,
  // Force PARTITION_NONE
  PART_EVAL_ONLY_NONE = 2
} UENUM1BYTE(PART_EVAL_STATUS);

typedef struct {
  VPVariance *part_variances;
  VPartVar *split[4];
} variance_node;

static AOM_INLINE void tree_to_node(void *data, BLOCK_SIZE bsize,
                                    variance_node *node) {
  int i;
  node->part_variances = NULL;
  switch (bsize) {
    case BLOCK_128X128: {
      VP128x128 *vt = (VP128x128 *)data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_64X64: {
      VP64x64 *vt = (VP64x64 *)data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_32X32: {
      VP32x32 *vt = (VP32x32 *)data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_16X16: {
      VP16x16 *vt = (VP16x16 *)data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_8X8: {
      VP8x8 *vt = (VP8x8 *)data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    default: {
      VP4x4 *vt = (VP4x4 *)data;
      assert(bsize == BLOCK_4X4);
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++) node->split[i] = &vt->split[i];
      break;
    }
  }
}

// Set variance values given sum square error, sum error, count.
static AOM_INLINE void fill_variance(uint32_t s2, int32_t s, int c,
                                     VPartVar *v) {
  v->sum_square_error = s2;
  v->sum_error = s;
  v->log2_count = c;
}

static AOM_INLINE void get_variance(VPartVar *v) {
  v->variance =
      (int)(256 * (v->sum_square_error -
                   (uint32_t)(((int64_t)v->sum_error * v->sum_error) >>
                              v->log2_count)) >>
            v->log2_count);
}

static AOM_INLINE void sum_2_variances(const VPartVar *a, const VPartVar *b,
                                       VPartVar *r) {
  assert(a->log2_count == b->log2_count);
  fill_variance(a->sum_square_error + b->sum_square_error,
                a->sum_error + b->sum_error, a->log2_count + 1, r);
}

static AOM_INLINE void fill_variance_tree(void *data, BLOCK_SIZE bsize) {
  variance_node node;
  memset(&node, 0, sizeof(node));
  tree_to_node(data, bsize, &node);
  sum_2_variances(node.split[0], node.split[1], &node.part_variances->horz[0]);
  sum_2_variances(node.split[2], node.split[3], &node.part_variances->horz[1]);
  sum_2_variances(node.split[0], node.split[2], &node.part_variances->vert[0]);
  sum_2_variances(node.split[1], node.split[3], &node.part_variances->vert[1]);
  sum_2_variances(&node.part_variances->vert[0], &node.part_variances->vert[1],
                  &node.part_variances->none);
}

static AOM_INLINE void set_block_size(AV1_COMP *const cpi, int mi_row,
                                      int mi_col, BLOCK_SIZE bsize) {
  if (cpi->common.mi_params.mi_cols > mi_col &&
      cpi->common.mi_params.mi_rows > mi_row) {
    CommonModeInfoParams *mi_params = &cpi->common.mi_params;
    const int mi_grid_idx = get_mi_grid_idx(mi_params, mi_row, mi_col);
    const int mi_alloc_idx = get_alloc_mi_idx(mi_params, mi_row, mi_col);
    MB_MODE_INFO *mi = mi_params->mi_grid_base[mi_grid_idx] =
        &mi_params->mi_alloc[mi_alloc_idx];
    mi->bsize = bsize;
  }
}

static int set_vt_partitioning(AV1_COMP *cpi, MACROBLOCKD *const xd,
                               const TileInfo *const tile, void *data,
                               BLOCK_SIZE bsize, int mi_row, int mi_col,
                               int64_t threshold, BLOCK_SIZE bsize_min,
                               PART_EVAL_STATUS force_split) {
  AV1_COMMON *const cm = &cpi->common;
  variance_node vt;
  const int block_width = mi_size_wide[bsize];
  const int block_height = mi_size_high[bsize];
  int bs_width_check = block_width;
  int bs_height_check = block_height;
  int bs_width_vert_check = block_width >> 1;
  int bs_height_horiz_check = block_height >> 1;
  // On the right and bottom boundary we only need to check
  // if half the bsize fits, because boundary is extended
  // up to 64. So do this check only for sb_size = 64X64.
  if (cm->seq_params->sb_size == BLOCK_64X64) {
    if (tile->mi_col_end == cm->mi_params.mi_cols) {
      bs_width_check = (block_width >> 1) + 1;
      bs_width_vert_check = (block_width >> 2) + 1;
    }
    if (tile->mi_row_end == cm->mi_params.mi_rows) {
      bs_height_check = (block_height >> 1) + 1;
      bs_height_horiz_check = (block_height >> 2) + 1;
    }
  }

  assert(block_height == block_width);
  tree_to_node(data, bsize, &vt);

  if (mi_col + bs_width_check <= tile->mi_col_end &&
      mi_row + bs_height_check <= tile->mi_row_end &&
      force_split == PART_EVAL_ONLY_NONE) {
    set_block_size(cpi, mi_row, mi_col, bsize);
    return 1;
  }
  if (force_split == PART_EVAL_ONLY_SPLIT) return 0;

  // For bsize=bsize_min (16x16/8x8 for 8x8/4x4 downsampling), select if
  // variance is below threshold, otherwise split will be selected.
  // No check for vert/horiz split as too few samples for variance.
  if (bsize == bsize_min) {
    // Variance already computed to set the force_split.
    if (frame_is_intra_only(cm)) get_variance(&vt.part_variances->none);
    if (mi_col + bs_width_check <= tile->mi_col_end &&
        mi_row + bs_height_check <= tile->mi_row_end &&
        vt.part_variances->none.variance < threshold) {
      set_block_size(cpi, mi_row, mi_col, bsize);
      return 1;
    }
    return 0;
  } else if (bsize > bsize_min) {
    // Variance already computed to set the force_split.
    if (frame_is_intra_only(cm)) get_variance(&vt.part_variances->none);
    // For key frame: take split for bsize above 32X32 or very high variance.
    if (frame_is_intra_only(cm) &&
        (bsize > BLOCK_32X32 ||
         vt.part_variances->none.variance > (threshold << 4))) {
      return 0;
    }
    // If variance is low, take the bsize (no split).
    if (mi_col + bs_width_check <= tile->mi_col_end &&
        mi_row + bs_height_check <= tile->mi_row_end &&
        vt.part_variances->none.variance < threshold) {
      set_block_size(cpi, mi_row, mi_col, bsize);
      return 1;
    }
    // Check vertical split.
    if (mi_row + bs_height_check <= tile->mi_row_end &&
        mi_col + bs_width_vert_check <= tile->mi_col_end) {
      BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_VERT);
      get_variance(&vt.part_variances->vert[0]);
      get_variance(&vt.part_variances->vert[1]);
      if (vt.part_variances->vert[0].variance < threshold &&
          vt.part_variances->vert[1].variance < threshold &&
          get_plane_block_size(subsize, xd->plane[1].subsampling_x,
                               xd->plane[1].subsampling_y) < BLOCK_INVALID) {
        set_block_size(cpi, mi_row, mi_col, subsize);
        set_block_size(cpi, mi_row, mi_col + block_width / 2, subsize);
        return 1;
      }
    }
    // Check horizontal split.
    if (mi_col + bs_width_check <= tile->mi_col_end &&
        mi_row + bs_height_horiz_check <= tile->mi_row_end) {
      BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_HORZ);
      get_variance(&vt.part_variances->horz[0]);
      get_variance(&vt.part_variances->horz[1]);
      if (vt.part_variances->horz[0].variance < threshold &&
          vt.part_variances->horz[1].variance < threshold &&
          get_plane_block_size(subsize, xd->plane[1].subsampling_x,
                               xd->plane[1].subsampling_y) < BLOCK_INVALID) {
        set_block_size(cpi, mi_row, mi_col, subsize);
        set_block_size(cpi, mi_row + block_height / 2, mi_col, subsize);
        return 1;
      }
    }
    return 0;
  }
  return 0;
}

static AOM_INLINE int all_blks_inside(int x16_idx, int y16_idx, int pixels_wide,
                                      int pixels_high) {
  int all_inside = 1;
  for (int k = 0; k < 4; k++) {
    all_inside &= ((x16_idx + ((k & 1) << 3)) < pixels_wide);
    all_inside &= ((y16_idx + ((k >> 1) << 3)) < pixels_high);
  }
  return all_inside;
}

#if CONFIG_AV1_HIGHBITDEPTH
// TODO(yunqingwang): Perform average of four 8x8 blocks similar to lowbd
static AOM_INLINE void fill_variance_8x8avg_highbd(
    const uint8_t *s, int sp, const uint8_t *d, int dp, int x16_idx,
    int y16_idx, VP16x16 *vst, int pixels_wide, int pixels_high,
    int is_key_frame) {
  for (int k = 0; k < 4; k++) {
    const int x8_idx = x16_idx + ((k & 1) << 3);
    const int y8_idx = y16_idx + ((k >> 1) << 3);
    unsigned int sse = 0;
    int sum = 0;
    if (x8_idx < pixels_wide && y8_idx < pixels_high) {
      int s_avg;
      int d_avg = 128;
      s_avg = aom_highbd_avg_8x8(s + y8_idx * sp + x8_idx, sp);
      if (!is_key_frame)
        d_avg = aom_highbd_avg_8x8(d + y8_idx * dp + x8_idx, dp);

      sum = s_avg - d_avg;
      sse = sum * sum;
    }
    fill_variance(sse, sum, 0, &vst->split[k].part_variances.none);
  }
}
#endif

static AOM_INLINE void fill_variance_8x8avg_lowbd(const uint8_t *s, int sp,
                                                  const uint8_t *d, int dp,
                                                  int x16_idx, int y16_idx,
                                                  VP16x16 *vst, int pixels_wide,
                                                  int pixels_high,
                                                  int is_key_frame) {
  unsigned int sse[4] = { 0 };
  int sum[4] = { 0 };
  int d_avg[4] = { 128, 128, 128, 128 };
  int s_avg[4];

  if (all_blks_inside(x16_idx, y16_idx, pixels_wide, pixels_high)) {
    aom_avg_8x8_quad(s, sp, x16_idx, y16_idx, s_avg);
    if (!is_key_frame) aom_avg_8x8_quad(d, dp, x16_idx, y16_idx, d_avg);
    for (int k = 0; k < 4; k++) {
      sum[k] = s_avg[k] - d_avg[k];
      sse[k] = sum[k] * sum[k];
    }
  } else {
    for (int k = 0; k < 4; k++) {
      const int x8_idx = x16_idx + ((k & 1) << 3);
      const int y8_idx = y16_idx + ((k >> 1) << 3);
      if (x8_idx < pixels_wide && y8_idx < pixels_high) {
        s_avg[k] = aom_avg_8x8(s + y8_idx * sp + x8_idx, sp);
        if (!is_key_frame) d_avg[k] = aom_avg_8x8(d + y8_idx * dp + x8_idx, dp);
        sum[k] = s_avg[k] - d_avg[k];
        sse[k] = sum[k] * sum[k];
      }
    }
  }

  for (int k = 0; k < 4; k++) {
    fill_variance(sse[k], sum[k], 0, &vst->split[k].part_variances.none);
  }
}

// Obtain parameters required to calculate variance (such as sum, sse, etc,.)
// at 8x8 sub-block level for a given 16x16 block.
static AOM_INLINE void fill_variance_8x8avg(const uint8_t *s, int sp,
                                            const uint8_t *d, int dp,
                                            int x16_idx, int y16_idx,
                                            VP16x16 *vst, int highbd_flag,
                                            int pixels_wide, int pixels_high,
                                            int is_key_frame) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd_flag) {
    fill_variance_8x8avg_highbd(s, sp, d, dp, x16_idx, y16_idx, vst,
                                pixels_wide, pixels_high, is_key_frame);
    return;
  }
#else
  (void)highbd_flag;
#endif  // CONFIG_AV1_HIGHBITDEPTH
  fill_variance_8x8avg_lowbd(s, sp, d, dp, x16_idx, y16_idx, vst, pixels_wide,
                             pixels_high, is_key_frame);
}

static int compute_minmax_8x8(const uint8_t *s, int sp, const uint8_t *d,
                              int dp, int x16_idx, int y16_idx,
#if CONFIG_AV1_HIGHBITDEPTH
                              int highbd_flag,
#endif
                              int pixels_wide, int pixels_high) {
  int k;
  int minmax_max = 0;
  int minmax_min = 255;
  // Loop over the 4 8x8 subblocks.
  for (k = 0; k < 4; k++) {
    int x8_idx = x16_idx + ((k & 1) << 3);
    int y8_idx = y16_idx + ((k >> 1) << 3);
    int min = 0;
    int max = 0;
    if (x8_idx < pixels_wide && y8_idx < pixels_high) {
#if CONFIG_AV1_HIGHBITDEPTH
      if (highbd_flag & YV12_FLAG_HIGHBITDEPTH) {
        aom_highbd_minmax_8x8(s + y8_idx * sp + x8_idx, sp,
                              d + y8_idx * dp + x8_idx, dp, &min, &max);
      } else {
        aom_minmax_8x8(s + y8_idx * sp + x8_idx, sp, d + y8_idx * dp + x8_idx,
                       dp, &min, &max);
      }
#else
      aom_minmax_8x8(s + y8_idx * sp + x8_idx, sp, d + y8_idx * dp + x8_idx, dp,
                     &min, &max);
#endif
      if ((max - min) > minmax_max) minmax_max = (max - min);
      if ((max - min) < minmax_min) minmax_min = (max - min);
    }
  }
  return (minmax_max - minmax_min);
}

static AOM_INLINE void fill_variance_4x4avg(const uint8_t *s, int sp,
                                            const uint8_t *d, int dp,
                                            int x8_idx, int y8_idx, VP8x8 *vst,
#if CONFIG_AV1_HIGHBITDEPTH
                                            int highbd_flag,
#endif
                                            int pixels_wide, int pixels_high,
                                            int is_key_frame) {
  int k;
  for (k = 0; k < 4; k++) {
    int x4_idx = x8_idx + ((k & 1) << 2);
    int y4_idx = y8_idx + ((k >> 1) << 2);
    unsigned int sse = 0;
    int sum = 0;
    if (x4_idx < pixels_wide && y4_idx < pixels_high) {
      int s_avg;
      int d_avg = 128;
#if CONFIG_AV1_HIGHBITDEPTH
      if (highbd_flag & YV12_FLAG_HIGHBITDEPTH) {
        s_avg = aom_highbd_avg_4x4(s + y4_idx * sp + x4_idx, sp);
        if (!is_key_frame)
          d_avg = aom_highbd_avg_4x4(d + y4_idx * dp + x4_idx, dp);
      } else {
        s_avg = aom_avg_4x4(s + y4_idx * sp + x4_idx, sp);
        if (!is_key_frame) d_avg = aom_avg_4x4(d + y4_idx * dp + x4_idx, dp);
      }
#else
      s_avg = aom_avg_4x4(s + y4_idx * sp + x4_idx, sp);
      if (!is_key_frame) d_avg = aom_avg_4x4(d + y4_idx * dp + x4_idx, dp);
#endif

      sum = s_avg - d_avg;
      sse = sum * sum;
    }
    fill_variance(sse, sum, 0, &vst->split[k].part_variances.none);
  }
}

// TODO(kyslov) Bring back threshold adjustment based on content state
static int64_t scale_part_thresh_content(int64_t threshold_base, int speed,
                                         int width, int height,
                                         int non_reference_frame) {
  (void)width;
  (void)height;
  int64_t threshold = threshold_base;
  if (non_reference_frame) threshold = (3 * threshold) >> 1;
  if (speed >= 8) {
    return (5 * threshold) >> 2;
  }
  return threshold;
}

static AOM_INLINE void tune_thresh_based_on_qindex_window(
    int qindex, int th, int source_sad, int ag_idx, int64_t thresholds[]) {
  const int win = 45;
  double weight;

  if (qindex < th - win)
    weight = 1.0;
  else if (qindex > th + win)
    weight = 0.0;
  else
    weight = 1.0 - (qindex - th + win) / (2 * win);
  thresholds[1] =
      (int)((1 - weight) * (thresholds[1] << 1) + weight * thresholds[1]);
  thresholds[2] =
      (int)((1 - weight) * (thresholds[2] << 1) + weight * thresholds[2]);
  const int fac = (!ag_idx && source_sad != kLowSad) ? 1 : 2;
  thresholds[3] =
      (int)((1 - weight) * (thresholds[3] << fac) + weight * thresholds[3]);
}

static AOM_INLINE void set_vbp_thresholds(AV1_COMP *cpi, int64_t thresholds[],
                                          int q, int content_lowsumdiff,
                                          int source_sad_nonrd,
                                          int source_sad_rd, int segment_id) {
  AV1_COMMON *const cm = &cpi->common;
  const int is_key_frame = frame_is_intra_only(cm);
  const int threshold_multiplier = is_key_frame ? 120 : 1;
  const int ac_q = av1_ac_quant_QTX(q, 0, cm->seq_params->bit_depth);
  int64_t threshold_base = (int64_t)(threshold_multiplier * ac_q);
  const int current_qindex = cm->quant_params.base_qindex;
  const int threshold_left_shift = cpi->sf.rt_sf.var_part_split_threshold_shift;

  if (is_key_frame) {
    if (cpi->sf.rt_sf.force_large_partition_blocks_intra) {
      const int shift_steps =
          threshold_left_shift - (cpi->oxcf.mode == ALLINTRA ? 7 : 8);
      assert(shift_steps >= 0);
      threshold_base <<= shift_steps;
    }
    thresholds[0] = threshold_base;
    thresholds[1] = threshold_base;
    if (cm->width * cm->height < 1280 * 720) {
      thresholds[2] = threshold_base / 3;
      thresholds[3] = threshold_base >> 1;
    } else {
      int shift_val = 2;
      if (cpi->sf.rt_sf.force_large_partition_blocks_intra) {
        shift_val = 0;
      }

      thresholds[2] = threshold_base >> shift_val;
      thresholds[3] = threshold_base >> shift_val;
    }
    thresholds[4] = threshold_base << 2;
    return;
  }

  // Increase partition thresholds for noisy content. Apply it only for
  // superblocks where sumdiff is low, as we assume the sumdiff of superblock
  // whose only change is due to noise will be low (i.e, noise will average
  // out over large block).
  if (cpi->noise_estimate.enabled && content_lowsumdiff &&
      (cm->width * cm->height > 640 * 480) &&
      cm->current_frame.frame_number > 60) {
    NOISE_LEVEL noise_level =
        av1_noise_estimate_extract_level(&cpi->noise_estimate);
    if (noise_level == kHigh)
      threshold_base = (5 * threshold_base) >> 1;
    else if (noise_level == kMedium &&
             !cpi->sf.rt_sf.prefer_large_partition_blocks)
      threshold_base = (5 * threshold_base) >> 2;
  }
  // TODO(kyslov) Enable var based partition adjusment on temporal denoising
#if 0  // CONFIG_AV1_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && denoise_svc(cpi) &&
      cpi->oxcf.speed > 5 && cpi->denoiser.denoising_level >= kDenLow)
      threshold_base =
          av1_scale_part_thresh(threshold_base, cpi->denoiser.denoising_level,
                                content_state, cpi->svc.temporal_layer_id);
  else
    threshold_base =
        scale_part_thresh_content(threshold_base, cpi->oxcf.speed, cm->width,
                                  cm->height, cpi->svc.non_reference_frame);
#else
  // Increase base variance threshold based on content_state/sum_diff level.
  threshold_base =
      scale_part_thresh_content(threshold_base, cpi->oxcf.speed, cm->width,
                                cm->height, cpi->svc.non_reference_frame);
#endif
  thresholds[0] = threshold_base >> 1;
  thresholds[1] = threshold_base;
  thresholds[3] = threshold_base << threshold_left_shift;
  if (cm->width >= 1280 && cm->height >= 720)
    thresholds[3] = thresholds[3] << 1;
  if (cm->width * cm->height <= 352 * 288) {
    const int qindex_thr[5][2] = {
      { 200, 220 }, { 140, 170 }, { 120, 150 }, { 200, 210 }, { 170, 220 },
    };
    int th_idx = 0;
    if (cpi->sf.rt_sf.var_part_based_on_qidx >= 1)
      th_idx =
          (source_sad_rd <= kLowSad) ? cpi->sf.rt_sf.var_part_based_on_qidx : 0;
    if (cpi->sf.rt_sf.var_part_based_on_qidx >= 3)
      th_idx = cpi->sf.rt_sf.var_part_based_on_qidx;
    const int qindex_low_thr = qindex_thr[th_idx][0];
    const int qindex_high_thr = qindex_thr[th_idx][1];
    if (current_qindex >= qindex_high_thr) {
      threshold_base = (5 * threshold_base) >> 1;
      thresholds[1] = threshold_base >> 3;
      thresholds[2] = threshold_base << 2;
      thresholds[3] = threshold_base << 5;
    } else if (current_qindex < qindex_low_thr) {
      thresholds[1] = threshold_base >> 3;
      thresholds[2] = threshold_base >> 1;
      thresholds[3] = threshold_base << 3;
    } else {
      int64_t qi_diff_low = current_qindex - qindex_low_thr;
      int64_t qi_diff_high = qindex_high_thr - current_qindex;
      int64_t threshold_diff = qindex_high_thr - qindex_low_thr;
      int64_t threshold_base_high = (5 * threshold_base) >> 1;

      threshold_diff = threshold_diff > 0 ? threshold_diff : 1;
      threshold_base =
          (qi_diff_low * threshold_base_high + qi_diff_high * threshold_base) /
          threshold_diff;
      thresholds[1] = threshold_base >> 3;
      thresholds[2] = ((qi_diff_low * threshold_base) +
                       qi_diff_high * (threshold_base >> 1)) /
                      threshold_diff;
      thresholds[3] = ((qi_diff_low * (threshold_base << 5)) +
                       qi_diff_high * (threshold_base << 3)) /
                      threshold_diff;
    }
  } else if (cm->width < 1280 && cm->height < 720) {
    thresholds[2] = (5 * threshold_base) >> 2;
  } else if (cm->width < 1920 && cm->height < 1080) {
    thresholds[2] = threshold_base << 1;
  } else {
    thresholds[2] = (5 * threshold_base) >> 1;
  }
  // Tune thresholds less or more aggressively to prefer larger partitions
  if (cpi->sf.rt_sf.prefer_large_partition_blocks >= 4) {
    double weight;
    const int win = 20;
    if (current_qindex < QINDEX_LARGE_BLOCK_THR - win)
      weight = 1.0;
    else if (current_qindex > QINDEX_LARGE_BLOCK_THR + win)
      weight = 0.0;
    else
      weight =
          1.0 - (current_qindex - QINDEX_LARGE_BLOCK_THR + win) / (2 * win);
    if (cm->width * cm->height > 640 * 480) {
      for (int i = 0; i < 4; i++) {
        thresholds[i] <<= 1;
      }
    }
    if (cm->width * cm->height <= 352 * 288) {
      thresholds[3] = INT32_MAX;
      if (segment_id == 0) {
        thresholds[1] <<= 2;
        thresholds[2] <<= (source_sad_nonrd == kLowSad) ? 5 : 4;
      } else {
        thresholds[1] <<= 1;
        thresholds[2] <<= 3;
      }
      // Condition the increase of partition thresholds on the segment
      // and the content. Avoid the increase for superblocks which have
      // high source sad, unless the whole frame has very high motion
      // (i.e, cpi->rc.avg_source_sad is very large, in which case all blocks
      // have high source sad).
    } else if (cm->width * cm->height > 640 * 480 && segment_id == 0 &&
               (source_sad_nonrd != kHighSad ||
                cpi->rc.avg_source_sad > 50000)) {
      thresholds[0] = (3 * thresholds[0]) >> 1;
      thresholds[3] = INT32_MAX;
      if (current_qindex > QINDEX_LARGE_BLOCK_THR) {
        thresholds[1] =
            (int)((1 - weight) * (thresholds[1] << 1) + weight * thresholds[1]);
        thresholds[2] =
            (int)((1 - weight) * (thresholds[2] << 1) + weight * thresholds[2]);
      }
    } else if (current_qindex > QINDEX_LARGE_BLOCK_THR && segment_id == 0 &&
               (source_sad_nonrd != kHighSad ||
                cpi->rc.avg_source_sad > 50000)) {
      thresholds[1] =
          (int)((1 - weight) * (thresholds[1] << 2) + weight * thresholds[1]);
      thresholds[2] =
          (int)((1 - weight) * (thresholds[2] << 4) + weight * thresholds[2]);
      thresholds[3] = INT32_MAX;
    }
  } else if (cpi->sf.rt_sf.prefer_large_partition_blocks >= 2) {
    tune_thresh_based_on_qindex_window(
        current_qindex, QINDEX_LARGE_BLOCK_THR, source_sad_nonrd,
        cpi->sf.rt_sf.prefer_large_partition_blocks - 2, thresholds);
  } else if (cpi->sf.rt_sf.prefer_large_partition_blocks >= 1) {
    thresholds[3] <<= 2;
    thresholds[1] <<= (source_sad_nonrd == kLowSad) ? 1 : 0;
    thresholds[2] <<= (source_sad_nonrd == kLowSad) ? 1 : 0;
  }
  if (cpi->sf.part_sf.disable_8x8_part_based_on_qidx && (current_qindex < 128))
    thresholds[3] = INT64_MAX;
}

// Set temporal variance low flag for superblock 64x64.
// Only first 25 in the array are used in this case.
static AOM_INLINE void set_low_temp_var_flag_64x64(
    CommonModeInfoParams *mi_params, PartitionSearchInfo *part_info,
    MACROBLOCKD *xd, VP64x64 *vt, const int64_t thresholds[], int mi_col,
    int mi_row) {
  if (xd->mi[0]->bsize == BLOCK_64X64) {
    if ((vt->part_variances).none.variance < (thresholds[0] >> 1))
      part_info->variance_low[0] = 1;
  } else if (xd->mi[0]->bsize == BLOCK_64X32) {
    for (int i = 0; i < 2; i++) {
      if (vt->part_variances.horz[i].variance < (thresholds[0] >> 2))
        part_info->variance_low[i + 1] = 1;
    }
  } else if (xd->mi[0]->bsize == BLOCK_32X64) {
    for (int i = 0; i < 2; i++) {
      if (vt->part_variances.vert[i].variance < (thresholds[0] >> 2))
        part_info->variance_low[i + 3] = 1;
    }
  } else {
    static const int idx[4][2] = { { 0, 0 }, { 0, 8 }, { 8, 0 }, { 8, 8 } };
    for (int i = 0; i < 4; i++) {
      const int idx_str =
          mi_params->mi_stride * (mi_row + idx[i][0]) + mi_col + idx[i][1];
      MB_MODE_INFO **this_mi = mi_params->mi_grid_base + idx_str;

      if (mi_params->mi_cols <= mi_col + idx[i][1] ||
          mi_params->mi_rows <= mi_row + idx[i][0])
        continue;

      if (*this_mi == NULL) continue;

      if ((*this_mi)->bsize == BLOCK_32X32) {
        int64_t threshold_32x32 = (5 * thresholds[1]) >> 3;
        if (vt->split[i].part_variances.none.variance < threshold_32x32)
          part_info->variance_low[i + 5] = 1;
      } else {
        // For 32x16 and 16x32 blocks, the flag is set on each 16x16 block
        // inside.
        if ((*this_mi)->bsize == BLOCK_16X16 ||
            (*this_mi)->bsize == BLOCK_32X16 ||
            (*this_mi)->bsize == BLOCK_16X32) {
          for (int j = 0; j < 4; j++) {
            if (vt->split[i].split[j].part_variances.none.variance <
                (thresholds[2] >> 8))
              part_info->variance_low[(i << 2) + j + 9] = 1;
          }
        }
      }
    }
  }
}

static AOM_INLINE void set_low_temp_var_flag_128x128(
    CommonModeInfoParams *mi_params, PartitionSearchInfo *part_info,
    MACROBLOCKD *xd, VP128x128 *vt, const int64_t thresholds[], int mi_col,
    int mi_row) {
  if (xd->mi[0]->bsize == BLOCK_128X128) {
    if (vt->part_variances.none.variance < (thresholds[0] >> 1))
      part_info->variance_low[0] = 1;
  } else if (xd->mi[0]->bsize == BLOCK_128X64) {
    for (int i = 0; i < 2; i++) {
      if (vt->part_variances.horz[i].variance < (thresholds[0] >> 2))
        part_info->variance_low[i + 1] = 1;
    }
  } else if (xd->mi[0]->bsize == BLOCK_64X128) {
    for (int i = 0; i < 2; i++) {
      if (vt->part_variances.vert[i].variance < (thresholds[0] >> 2))
        part_info->variance_low[i + 3] = 1;
    }
  } else {
    static const int idx64[4][2] = {
      { 0, 0 }, { 0, 16 }, { 16, 0 }, { 16, 16 }
    };
    static const int idx32[4][2] = { { 0, 0 }, { 0, 8 }, { 8, 0 }, { 8, 8 } };
    for (int i = 0; i < 4; i++) {
      const int idx_str =
          mi_params->mi_stride * (mi_row + idx64[i][0]) + mi_col + idx64[i][1];
      MB_MODE_INFO **mi_64 = mi_params->mi_grid_base + idx_str;
      if (*mi_64 == NULL) continue;
      if (mi_params->mi_cols <= mi_col + idx64[i][1] ||
          mi_params->mi_rows <= mi_row + idx64[i][0])
        continue;
      const int64_t threshold_64x64 = (5 * thresholds[1]) >> 3;
      if ((*mi_64)->bsize == BLOCK_64X64) {
        if (vt->split[i].part_variances.none.variance < threshold_64x64)
          part_info->variance_low[5 + i] = 1;
      } else if ((*mi_64)->bsize == BLOCK_64X32) {
        for (int j = 0; j < 2; j++)
          if (vt->split[i].part_variances.horz[j].variance <
              (threshold_64x64 >> 1))
            part_info->variance_low[9 + (i << 1) + j] = 1;
      } else if ((*mi_64)->bsize == BLOCK_32X64) {
        for (int j = 0; j < 2; j++)
          if (vt->split[i].part_variances.vert[j].variance <
              (threshold_64x64 >> 1))
            part_info->variance_low[17 + (i << 1) + j] = 1;
      } else {
        for (int k = 0; k < 4; k++) {
          const int idx_str1 = mi_params->mi_stride * idx32[k][0] + idx32[k][1];
          MB_MODE_INFO **mi_32 = mi_params->mi_grid_base + idx_str + idx_str1;
          if (*mi_32 == NULL) continue;

          if (mi_params->mi_cols <= mi_col + idx64[i][1] + idx32[k][1] ||
              mi_params->mi_rows <= mi_row + idx64[i][0] + idx32[k][0])
            continue;
          const int64_t threshold_32x32 = (5 * thresholds[2]) >> 3;
          if ((*mi_32)->bsize == BLOCK_32X32) {
            if (vt->split[i].split[k].part_variances.none.variance <
                threshold_32x32)
              part_info->variance_low[25 + (i << 2) + k] = 1;
          } else {
            // For 32x16 and 16x32 blocks, the flag is set on each 16x16 block
            // inside.
            if ((*mi_32)->bsize == BLOCK_16X16 ||
                (*mi_32)->bsize == BLOCK_32X16 ||
                (*mi_32)->bsize == BLOCK_16X32) {
              for (int j = 0; j < 4; j++) {
                if (vt->split[i]
                        .split[k]
                        .split[j]
                        .part_variances.none.variance < (thresholds[3] >> 8))
                  part_info->variance_low[41 + (i << 4) + (k << 2) + j] = 1;
              }
            }
          }
        }
      }
    }
  }
}

static AOM_INLINE void set_low_temp_var_flag(
    AV1_COMP *cpi, PartitionSearchInfo *part_info, MACROBLOCKD *xd,
    VP128x128 *vt, int64_t thresholds[], MV_REFERENCE_FRAME ref_frame_partition,
    int mi_col, int mi_row) {
  AV1_COMMON *const cm = &cpi->common;
  // Check temporal variance for bsize >= 16x16, if LAST_FRAME was selected.
  // If the temporal variance is small set the flag
  // variance_low for the block. The variance threshold can be adjusted, the
  // higher the more aggressive.
  if (ref_frame_partition == LAST_FRAME) {
    const int is_small_sb = (cm->seq_params->sb_size == BLOCK_64X64);
    if (is_small_sb)
      set_low_temp_var_flag_64x64(&cm->mi_params, part_info, xd,
                                  &(vt->split[0]), thresholds, mi_col, mi_row);
    else
      set_low_temp_var_flag_128x128(&cm->mi_params, part_info, xd, vt,
                                    thresholds, mi_col, mi_row);
  }
}

static const int pos_shift_16x16[4][4] = {
  { 9, 10, 13, 14 }, { 11, 12, 15, 16 }, { 17, 18, 21, 22 }, { 19, 20, 23, 24 }
};

int av1_get_force_skip_low_temp_var_small_sb(const uint8_t *variance_low,
                                             int mi_row, int mi_col,
                                             BLOCK_SIZE bsize) {
  // Relative indices of MB inside the superblock.
  const int mi_x = mi_row & 0xF;
  const int mi_y = mi_col & 0xF;
  // Relative indices of 16x16 block inside the superblock.
  const int i = mi_x >> 2;
  const int j = mi_y >> 2;
  int force_skip_low_temp_var = 0;
  // Set force_skip_low_temp_var based on the block size and block offset.
  switch (bsize) {
    case BLOCK_64X64: force_skip_low_temp_var = variance_low[0]; break;
    case BLOCK_64X32:
      if (!mi_y && !mi_x) {
        force_skip_low_temp_var = variance_low[1];
      } else if (!mi_y && mi_x) {
        force_skip_low_temp_var = variance_low[2];
      }
      break;
    case BLOCK_32X64:
      if (!mi_y && !mi_x) {
        force_skip_low_temp_var = variance_low[3];
      } else if (mi_y && !mi_x) {
        force_skip_low_temp_var = variance_low[4];
      }
      break;
    case BLOCK_32X32:
      if (!mi_y && !mi_x) {
        force_skip_low_temp_var = variance_low[5];
      } else if (mi_y && !mi_x) {
        force_skip_low_temp_var = variance_low[6];
      } else if (!mi_y && mi_x) {
        force_skip_low_temp_var = variance_low[7];
      } else if (mi_y && mi_x) {
        force_skip_low_temp_var = variance_low[8];
      }
      break;
    case BLOCK_32X16:
    case BLOCK_16X32:
    case BLOCK_16X16:
      force_skip_low_temp_var = variance_low[pos_shift_16x16[i][j]];
      break;
    default: break;
  }

  return force_skip_low_temp_var;
}

int av1_get_force_skip_low_temp_var(const uint8_t *variance_low, int mi_row,
                                    int mi_col, BLOCK_SIZE bsize) {
  int force_skip_low_temp_var = 0;
  int x, y;
  x = (mi_col & 0x1F) >> 4;
  // y = (mi_row & 0x1F) >> 4;
  // const int idx64 = (y << 1) + x;
  y = (mi_row & 0x17) >> 3;
  const int idx64 = y + x;

  x = (mi_col & 0xF) >> 3;
  // y = (mi_row & 0xF) >> 3;
  // const int idx32 = (y << 1) + x;
  y = (mi_row & 0xB) >> 2;
  const int idx32 = y + x;

  x = (mi_col & 0x7) >> 2;
  // y = (mi_row & 0x7) >> 2;
  // const int idx16 = (y << 1) + x;
  y = (mi_row & 0x5) >> 1;
  const int idx16 = y + x;
  // Set force_skip_low_temp_var based on the block size and block offset.
  switch (bsize) {
    case BLOCK_128X128: force_skip_low_temp_var = variance_low[0]; break;
    case BLOCK_128X64:
      assert((mi_col & 0x1F) == 0);
      force_skip_low_temp_var = variance_low[1 + ((mi_row & 0x1F) != 0)];
      break;
    case BLOCK_64X128:
      assert((mi_row & 0x1F) == 0);
      force_skip_low_temp_var = variance_low[3 + ((mi_col & 0x1F) != 0)];
      break;
    case BLOCK_64X64:
      // Location of this 64x64 block inside the 128x128 superblock
      force_skip_low_temp_var = variance_low[5 + idx64];
      break;
    case BLOCK_64X32:
      x = (mi_col & 0x1F) >> 4;
      y = (mi_row & 0x1F) >> 3;
      /*
      .---------------.---------------.
      | x=0,y=0,idx=0 | x=0,y=0,idx=2 |
      :---------------+---------------:
      | x=0,y=1,idx=1 | x=1,y=1,idx=3 |
      :---------------+---------------:
      | x=0,y=2,idx=4 | x=1,y=2,idx=6 |
      :---------------+---------------:
      | x=0,y=3,idx=5 | x=1,y=3,idx=7 |
      '---------------'---------------'
      */
      const int idx64x32 = (x << 1) + (y % 2) + ((y >> 1) << 2);
      force_skip_low_temp_var = variance_low[9 + idx64x32];
      break;
    case BLOCK_32X64:
      x = (mi_col & 0x1F) >> 3;
      y = (mi_row & 0x1F) >> 4;
      const int idx32x64 = (y << 2) + x;
      force_skip_low_temp_var = variance_low[17 + idx32x64];
      break;
    case BLOCK_32X32:
      force_skip_low_temp_var = variance_low[25 + (idx64 << 2) + idx32];
      break;
    case BLOCK_32X16:
    case BLOCK_16X32:
    case BLOCK_16X16:
      force_skip_low_temp_var =
          variance_low[41 + (idx64 << 4) + (idx32 << 2) + idx16];
      break;
    default: break;
  }
  return force_skip_low_temp_var;
}

void av1_set_variance_partition_thresholds(AV1_COMP *cpi, int q,
                                           int content_lowsumdiff) {
  SPEED_FEATURES *const sf = &cpi->sf;
  if (sf->part_sf.partition_search_type != VAR_BASED_PARTITION) {
    return;
  } else {
    set_vbp_thresholds(cpi, cpi->vbp_info.thresholds, q, content_lowsumdiff, 0,
                       0, 0);
    // The threshold below is not changed locally.
    cpi->vbp_info.threshold_minmax = 15 + (q >> 3);
  }
}

static AOM_INLINE void chroma_check(AV1_COMP *cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, unsigned int y_sad,
                                    int is_key_frame, int zero_motion,
                                    unsigned int *uv_sad) {
  int i;
  MACROBLOCKD *xd = &x->e_mbd;
  int shift = 3;
  if (is_key_frame || cpi->oxcf.tool_cfg.enable_monochrome) return;

  if (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN &&
      cpi->rc.high_source_sad)
    shift = 5;

  MB_MODE_INFO *mi = xd->mi[0];
  const AV1_COMMON *const cm = &cpi->common;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_yv12_buf(cm, LAST_FRAME);
  const struct scale_factors *const sf =
      get_ref_scale_factors_const(cm, LAST_FRAME);
  struct buf_2d dst;

  for (i = 1; i <= 2; ++i) {
    struct macroblock_plane *p = &x->plane[i];
    struct macroblockd_plane *pd = &xd->plane[i];
    const BLOCK_SIZE bs =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);

    if (bs != BLOCK_INVALID) {
      if (zero_motion) {
        if (mi->ref_frame[0] == LAST_FRAME) {
          uv_sad[i - 1] = cpi->ppi->fn_ptr[bs].sdf(
              p->src.buf, p->src.stride, pd->pre[0].buf, pd->pre[0].stride);
        } else {
          uint8_t *src = (i == 1) ? yv12->u_buffer : yv12->v_buffer;
          setup_pred_plane(&dst, xd->mi[0]->bsize, src, yv12->uv_crop_width,
                           yv12->uv_crop_height, yv12->uv_stride, xd->mi_row,
                           xd->mi_col, sf, xd->plane[i].subsampling_x,
                           xd->plane[i].subsampling_y);

          uv_sad[i - 1] = cpi->ppi->fn_ptr[bs].sdf(p->src.buf, p->src.stride,
                                                   dst.buf, dst.stride);
        }
      } else
        uv_sad[i - 1] = cpi->ppi->fn_ptr[bs].sdf(p->src.buf, p->src.stride,
                                                 pd->dst.buf, pd->dst.stride);
    }

    if (uv_sad[i - 1] > (y_sad >> 1))
      x->color_sensitivity_sb[i - 1] = 1;
    else if (uv_sad[i - 1] < (y_sad >> shift))
      x->color_sensitivity_sb[i - 1] = 0;
    // Borderline case: to be refined at coding block level in nonrd_pickmode,
    // for coding block size < sb_size.
    else
      x->color_sensitivity_sb[i - 1] = 2;
  }
}

static void fill_variance_tree_leaves(
    AV1_COMP *cpi, MACROBLOCK *x, VP128x128 *vt, VP16x16 *vt2,
    PART_EVAL_STATUS *force_split, int avg_16x16[][4], int maxvar_16x16[][4],
    int minvar_16x16[][4], int *variance4x4downsample, int64_t *thresholds,
    uint8_t *src, int src_stride, const uint8_t *dst, int dst_stride) {
  AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const int is_key_frame = frame_is_intra_only(cm);
  const int is_small_sb = (cm->seq_params->sb_size == BLOCK_64X64);
  const int num_64x64_blocks = is_small_sb ? 1 : 4;
  // TODO(kyslov) Bring back compute_minmax_variance with content type detection
  const int compute_minmax_variance = 0;
  const int segment_id = xd->mi[0]->segment_id;
  int pixels_wide = 128, pixels_high = 128;

  if (is_small_sb) {
    pixels_wide = 64;
    pixels_high = 64;
  }
  if (xd->mb_to_right_edge < 0) pixels_wide += (xd->mb_to_right_edge >> 3);
  if (xd->mb_to_bottom_edge < 0) pixels_high += (xd->mb_to_bottom_edge >> 3);
  for (int m = 0; m < num_64x64_blocks; m++) {
    const int x64_idx = ((m & 1) << 6);
    const int y64_idx = ((m >> 1) << 6);
    const int m2 = m << 2;
    force_split[m + 1] = PART_EVAL_ALL;

    for (int i = 0; i < 4; i++) {
      const int x32_idx = x64_idx + ((i & 1) << 5);
      const int y32_idx = y64_idx + ((i >> 1) << 5);
      const int i2 = (m2 + i) << 2;
      force_split[5 + m2 + i] = PART_EVAL_ALL;
      avg_16x16[m][i] = 0;
      maxvar_16x16[m][i] = 0;
      minvar_16x16[m][i] = INT_MAX;
      for (int j = 0; j < 4; j++) {
        const int x16_idx = x32_idx + ((j & 1) << 4);
        const int y16_idx = y32_idx + ((j >> 1) << 4);
        const int split_index = 21 + i2 + j;
        VP16x16 *vst = &vt->split[m].split[i].split[j];
        force_split[split_index] = PART_EVAL_ALL;
        variance4x4downsample[i2 + j] = 0;
        if (!is_key_frame) {
          fill_variance_8x8avg(src, src_stride, dst, dst_stride, x16_idx,
                               y16_idx, vst, is_cur_buf_hbd(xd), pixels_wide,
                               pixels_high, is_key_frame);

          fill_variance_tree(&vt->split[m].split[i].split[j], BLOCK_16X16);
          get_variance(&vt->split[m].split[i].split[j].part_variances.none);
          avg_16x16[m][i] +=
              vt->split[m].split[i].split[j].part_variances.none.variance;
          if (vt->split[m].split[i].split[j].part_variances.none.variance <
              minvar_16x16[m][i])
            minvar_16x16[m][i] =
                vt->split[m].split[i].split[j].part_variances.none.variance;
          if (vt->split[m].split[i].split[j].part_variances.none.variance >
              maxvar_16x16[m][i])
            maxvar_16x16[m][i] =
                vt->split[m].split[i].split[j].part_variances.none.variance;
          if (vt->split[m].split[i].split[j].part_variances.none.variance >
              thresholds[3]) {
            // 16X16 variance is above threshold for split, so force split to
            // 8x8 for this 16x16 block (this also forces splits for upper
            // levels).
            force_split[split_index] = PART_EVAL_ONLY_SPLIT;
            force_split[5 + m2 + i] = PART_EVAL_ONLY_SPLIT;
            force_split[m + 1] = PART_EVAL_ONLY_SPLIT;
            force_split[0] = PART_EVAL_ONLY_SPLIT;
          } else if (!cyclic_refresh_segment_id_boosted(segment_id) &&
                     compute_minmax_variance &&
                     vt->split[m]
                             .split[i]
                             .split[j]
                             .part_variances.none.variance > thresholds[2]) {
            // We have some nominal amount of 16x16 variance (based on average),
            // compute the minmax over the 8x8 sub-blocks, and if above
            // threshold, force split to 8x8 block for this 16x16 block.
            int minmax = compute_minmax_8x8(src, src_stride, dst, dst_stride,
                                            x16_idx, y16_idx,
#if CONFIG_AV1_HIGHBITDEPTH
                                            xd->cur_buf->flags,
#endif
                                            pixels_wide, pixels_high);
            int thresh_minmax = (int)cpi->vbp_info.threshold_minmax;
            if (minmax > thresh_minmax) {
              force_split[split_index] = PART_EVAL_ONLY_SPLIT;
              force_split[5 + m2 + i] = PART_EVAL_ONLY_SPLIT;
              force_split[m + 1] = PART_EVAL_ONLY_SPLIT;
              force_split[0] = PART_EVAL_ONLY_SPLIT;
            }
          }
        }
        if (is_key_frame) {
          force_split[split_index] = PART_EVAL_ALL;
          // Go down to 4x4 down-sampling for variance.
          variance4x4downsample[i2 + j] = 1;
          for (int k = 0; k < 4; k++) {
            int x8_idx = x16_idx + ((k & 1) << 3);
            int y8_idx = y16_idx + ((k >> 1) << 3);
            VP8x8 *vst2 = is_key_frame ? &vst->split[k] : &vt2[i2 + j].split[k];
            fill_variance_4x4avg(src, src_stride, dst, dst_stride, x8_idx,
                                 y8_idx, vst2,
#if CONFIG_AV1_HIGHBITDEPTH
                                 xd->cur_buf->flags,
#endif
                                 pixels_wide, pixels_high, is_key_frame);
          }
        }
      }
    }
  }
}

static void setup_planes(AV1_COMP *cpi, MACROBLOCK *x, unsigned int *y_sad,
                         unsigned int *y_sad_g, unsigned int *y_sad_last,
                         MV_REFERENCE_FRAME *ref_frame_partition, int mi_row,
                         int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const int num_planes = av1_num_planes(cm);
  const int is_small_sb = (cm->seq_params->sb_size == BLOCK_64X64);
  BLOCK_SIZE bsize = is_small_sb ? BLOCK_64X64 : BLOCK_128X128;
  // TODO(kyslov): we are assuming that the ref is LAST_FRAME! Check if it
  // is!!
  MB_MODE_INFO *mi = xd->mi[0];
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_yv12_buf(cm, LAST_FRAME);
  assert(yv12 != NULL);
  const YV12_BUFFER_CONFIG *yv12_g = NULL;

  // For non-SVC GOLDEN is another temporal reference. Check if it should be
  // used as reference for partitioning.
  if (!cpi->ppi->use_svc && (cpi->ref_frame_flags & AOM_GOLD_FLAG) &&
      x->content_state_sb.source_sad_nonrd != kZeroSad) {
    yv12_g = get_ref_frame_yv12_buf(cm, GOLDEN_FRAME);
    if (yv12_g && yv12_g != yv12) {
      av1_setup_pre_planes(xd, 0, yv12_g, mi_row, mi_col,
                           get_ref_scale_factors(cm, GOLDEN_FRAME), num_planes);
      *y_sad_g = cpi->ppi->fn_ptr[bsize].sdf(
          x->plane[0].src.buf, x->plane[0].src.stride, xd->plane[0].pre[0].buf,
          xd->plane[0].pre[0].stride);
    }
  }

  av1_setup_pre_planes(xd, 0, yv12, mi_row, mi_col,
                       get_ref_scale_factors(cm, LAST_FRAME), num_planes);
  mi->ref_frame[0] = LAST_FRAME;
  mi->ref_frame[1] = NONE_FRAME;
  mi->bsize = cm->seq_params->sb_size;
  mi->mv[0].as_int = 0;
  mi->interp_filters = av1_broadcast_interp_filter(BILINEAR);
  if (cpi->sf.rt_sf.estimate_motion_for_var_based_partition) {
    if (xd->mb_to_right_edge >= 0 && xd->mb_to_bottom_edge >= 0) {
      const MV dummy_mv = { 0, 0 };
      *y_sad = av1_int_pro_motion_estimation(cpi, x, cm->seq_params->sb_size,
                                             mi_row, mi_col, &dummy_mv);
    }
  }
  if (*y_sad == UINT_MAX) {
    *y_sad = cpi->ppi->fn_ptr[bsize].sdf(
        x->plane[0].src.buf, x->plane[0].src.stride, xd->plane[0].pre[0].buf,
        xd->plane[0].pre[0].stride);
  }
  *y_sad_last = *y_sad;

  // Pick the ref frame for partitioning, use golden frame only if its
  // lower sad.
  if (*y_sad_g < 0.9 * *y_sad) {
    av1_setup_pre_planes(xd, 0, yv12_g, mi_row, mi_col,
                         get_ref_scale_factors(cm, GOLDEN_FRAME), num_planes);
    mi->ref_frame[0] = GOLDEN_FRAME;
    mi->mv[0].as_int = 0;
    *y_sad = *y_sad_g;
    *ref_frame_partition = GOLDEN_FRAME;
    x->nonrd_prune_ref_frame_search = 0;
  } else {
    *ref_frame_partition = LAST_FRAME;
    x->nonrd_prune_ref_frame_search =
        cpi->sf.rt_sf.nonrd_prune_ref_frame_search;
  }

  // Only calculate the predictor for non-zero MV.
  if (mi->mv[0].as_int != 0) {
    set_ref_ptrs(cm, xd, mi->ref_frame[0], mi->ref_frame[1]);
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL,
                                  cm->seq_params->sb_size, AOM_PLANE_Y,
                                  AOM_PLANE_V);
  }
}

// Decides whether to split or merge a 16x16 partition block in variance based
// partitioning based on the 8x8 sub-block variances.
static AOM_INLINE PART_EVAL_STATUS get_part_eval_based_on_sub_blk_var(
    VP16x16 *var_16x16_info, int64_t threshold16) {
  int max_8x8_var = 0, min_8x8_var = INT_MAX;
  for (int k = 0; k < 4; k++) {
    get_variance(&var_16x16_info->split[k].part_variances.none);
    int this_8x8_var = var_16x16_info->split[k].part_variances.none.variance;
    max_8x8_var = AOMMAX(this_8x8_var, max_8x8_var);
    min_8x8_var = AOMMIN(this_8x8_var, min_8x8_var);
  }
  // If the difference between maximum and minimum sub-block variances is high,
  // then only evaluate PARTITION_SPLIT for the 16x16 block. Otherwise, evaluate
  // only PARTITION_NONE. The shift factor for threshold16 has been derived
  // empirically.
  return ((max_8x8_var - min_8x8_var) > (threshold16 << 2))
             ? PART_EVAL_ONLY_SPLIT
             : PART_EVAL_ONLY_NONE;
}

int av1_choose_var_based_partitioning(AV1_COMP *cpi, const TileInfo *const tile,
                                      ThreadData *td, MACROBLOCK *x, int mi_row,
                                      int mi_col) {
#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, choose_var_based_partitioning_time);
#endif
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const int64_t *const vbp_thresholds = cpi->vbp_info.thresholds;

  int i, j, k, m;
  VP128x128 *vt;
  VP16x16 *vt2 = NULL;
  PART_EVAL_STATUS force_split[85];
  int avg_64x64;
  int max_var_32x32[4];
  int min_var_32x32[4];
  int var_32x32;
  int var_64x64;
  int min_var_64x64 = INT_MAX;
  int max_var_64x64 = 0;
  int avg_16x16[4][4];
  int maxvar_16x16[4][4];
  int minvar_16x16[4][4];
  int64_t threshold_4x4avg;
  uint8_t *s;
  const uint8_t *d;
  int sp;
  int dp;
  unsigned int uv_sad[2];
  NOISE_LEVEL noise_level = kLow;
  int zero_motion = 1;

  int is_key_frame =
      (frame_is_intra_only(cm) ||
       (cpi->ppi->use_svc &&
        cpi->svc.layer_context[cpi->svc.temporal_layer_id].is_key_frame));

  assert(cm->seq_params->sb_size == BLOCK_64X64 ||
         cm->seq_params->sb_size == BLOCK_128X128);
  const int is_small_sb = (cm->seq_params->sb_size == BLOCK_64X64);
  const int num_64x64_blocks = is_small_sb ? 1 : 4;

  unsigned int y_sad = UINT_MAX;
  unsigned int y_sad_g = UINT_MAX;
  unsigned int y_sad_last = UINT_MAX;
  BLOCK_SIZE bsize = is_small_sb ? BLOCK_64X64 : BLOCK_128X128;

  // Ref frame used in partitioning.
  MV_REFERENCE_FRAME ref_frame_partition = LAST_FRAME;

  CHECK_MEM_ERROR(cm, vt, aom_malloc(sizeof(*vt)));

  vt->split = td->vt64x64;

  int64_t thresholds[5] = { vbp_thresholds[0], vbp_thresholds[1],
                            vbp_thresholds[2], vbp_thresholds[3],
                            vbp_thresholds[4] };

  const int low_res = (cm->width <= 352 && cm->height <= 288);
  int variance4x4downsample[64];
  const int segment_id = xd->mi[0]->segment_id;

  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ && cm->seg.enabled &&
      cyclic_refresh_segment_id_boosted(segment_id)) {
    const int q =
        av1_get_qindex(&cm->seg, segment_id, cm->quant_params.base_qindex);
    set_vbp_thresholds(cpi, thresholds, q, x->content_state_sb.low_sumdiff,
                       x->content_state_sb.source_sad_nonrd,
                       x->content_state_sb.source_sad_rd, 1);
  } else {
    set_vbp_thresholds(cpi, thresholds, cm->quant_params.base_qindex,
                       x->content_state_sb.low_sumdiff,
                       x->content_state_sb.source_sad_nonrd,
                       x->content_state_sb.source_sad_rd, 0);
  }

  // For non keyframes, disable 4x4 average for low resolution when speed = 8
  threshold_4x4avg = INT64_MAX;

  s = x->plane[0].src.buf;
  sp = x->plane[0].src.stride;

  // Index for force_split: 0 for 64x64, 1-4 for 32x32 blocks,
  // 5-20 for the 16x16 blocks.
  force_split[0] = PART_EVAL_ALL;
  memset(x->part_search_info.variance_low, 0,
         sizeof(x->part_search_info.variance_low));

  // Check if LAST frame is NULL or if the resolution of LAST is
  // different than the current frame resolution, and if so, treat this frame
  // as a key frame, for the purpose of the superblock partitioning.
  // LAST == NULL can happen in cases where enhancement spatial layers are
  // enabled dyanmically and the only reference is the spatial(GOLDEN).
  // TODO(marpan): Check se of scaled references for the different resoln.
  if (!frame_is_intra_only(cm)) {
    const YV12_BUFFER_CONFIG *const ref =
        get_ref_frame_yv12_buf(cm, LAST_FRAME);
    if (ref == NULL || ref->y_crop_height != cm->height ||
        ref->y_crop_width != cm->width) {
      is_key_frame = 1;
    }
  }

  if (!is_key_frame) {
    setup_planes(cpi, x, &y_sad, &y_sad_g, &y_sad_last, &ref_frame_partition,
                 mi_row, mi_col);

    MB_MODE_INFO *mi = xd->mi[0];
    // Use reference SB directly for zero mv.
    if (mi->mv[0].as_int != 0) {
      d = xd->plane[0].dst.buf;
      dp = xd->plane[0].dst.stride;
      zero_motion = 0;
    } else {
      d = xd->plane[0].pre[0].buf;
      dp = xd->plane[0].pre[0].stride;
    }
  } else {
    d = AV1_VAR_OFFS;
    dp = 0;
  }

  uv_sad[0] = 0;
  uv_sad[1] = 0;
  chroma_check(cpi, x, bsize, y_sad_last, is_key_frame, zero_motion, uv_sad);

  x->force_zeromv_skip = 0;
  const unsigned int thresh_exit_part =
      (cm->seq_params->sb_size == BLOCK_64X64) ? 5000 : 10000;
  // If the superblock is completely static (zero source sad) and
  // the y_sad (relative to LAST ref) is very small, take the sb_size partition
  // and exit, and force zeromv_last skip mode for nonrd_pickmode.
  // Only do this when the cyclic refresh is applied, and only on the base
  // segment (so the QP-boosted segment can still contnue cleaning/ramping
  // up the quality). Condition on color uv_sad is also added.
  if (!is_key_frame && cpi->sf.rt_sf.part_early_exit_zeromv &&
      cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->cyclic_refresh->apply_cyclic_refresh &&
      segment_id == CR_SEGMENT_ID_BASE &&
      x->content_state_sb.source_sad_nonrd == kZeroSad &&
      ref_frame_partition == LAST_FRAME && xd->mi[0]->mv[0].as_int == 0 &&
      y_sad < thresh_exit_part && uv_sad[0]<(3 * thresh_exit_part)>> 2 &&
      uv_sad[1]<(3 * thresh_exit_part)>> 2) {
    const int block_width = mi_size_wide[cm->seq_params->sb_size];
    const int block_height = mi_size_high[cm->seq_params->sb_size];
    if (mi_col + block_width <= tile->mi_col_end &&
        mi_row + block_height <= tile->mi_row_end) {
      set_block_size(cpi, mi_row, mi_col, bsize);
      x->force_zeromv_skip = 1;
      if (vt2) aom_free(vt2);
      if (vt) aom_free(vt);
      return 0;
    }
  }

  if (cpi->noise_estimate.enabled)
    noise_level = av1_noise_estimate_extract_level(&cpi->noise_estimate);

  if (low_res && threshold_4x4avg < INT64_MAX)
    CHECK_MEM_ERROR(cm, vt2, aom_malloc(sizeof(*vt2)));
  // Fill in the entire tree of 8x8 (or 4x4 under some conditions) variances
  // for splits.
  fill_variance_tree_leaves(cpi, x, vt, vt2, force_split, avg_16x16,
                            maxvar_16x16, minvar_16x16, variance4x4downsample,
                            thresholds, s, sp, d, dp);

  avg_64x64 = 0;
  for (m = 0; m < num_64x64_blocks; ++m) {
    max_var_32x32[m] = 0;
    min_var_32x32[m] = INT_MAX;
    const int m2 = m << 2;
    for (i = 0; i < 4; i++) {
      const int i2 = (m2 + i) << 2;
      for (j = 0; j < 4; j++) {
        const int split_index = 21 + i2 + j;
        if (variance4x4downsample[i2 + j] == 1) {
          VP16x16 *vtemp =
              (!is_key_frame) ? &vt2[i2 + j] : &vt->split[m].split[i].split[j];
          for (k = 0; k < 4; k++)
            fill_variance_tree(&vtemp->split[k], BLOCK_8X8);
          fill_variance_tree(vtemp, BLOCK_16X16);
          // If variance of this 16x16 block is above the threshold, force block
          // to split. This also forces a split on the upper levels.
          get_variance(&vtemp->part_variances.none);
          if (vtemp->part_variances.none.variance > thresholds[3]) {
            force_split[split_index] =
                cpi->sf.rt_sf.vbp_prune_16x16_split_using_min_max_sub_blk_var
                    ? get_part_eval_based_on_sub_blk_var(vtemp, thresholds[3])
                    : PART_EVAL_ONLY_SPLIT;
            force_split[5 + m2 + i] = PART_EVAL_ONLY_SPLIT;
            force_split[m + 1] = PART_EVAL_ONLY_SPLIT;
            force_split[0] = PART_EVAL_ONLY_SPLIT;
          }
        }
      }
      fill_variance_tree(&vt->split[m].split[i], BLOCK_32X32);
      // If variance of this 32x32 block is above the threshold, or if its above
      // (some threshold of) the average variance over the sub-16x16 blocks,
      // then force this block to split. This also forces a split on the upper
      // (64x64) level.
      if (force_split[5 + m2 + i] == PART_EVAL_ALL) {
        get_variance(&vt->split[m].split[i].part_variances.none);
        var_32x32 = vt->split[m].split[i].part_variances.none.variance;
        max_var_32x32[m] = AOMMAX(var_32x32, max_var_32x32[m]);
        min_var_32x32[m] = AOMMIN(var_32x32, min_var_32x32[m]);
        if (vt->split[m].split[i].part_variances.none.variance >
                thresholds[2] ||
            (!is_key_frame &&
             vt->split[m].split[i].part_variances.none.variance >
                 (thresholds[2] >> 1) &&
             vt->split[m].split[i].part_variances.none.variance >
                 (avg_16x16[m][i] >> 1))) {
          force_split[5 + m2 + i] = PART_EVAL_ONLY_SPLIT;
          force_split[m + 1] = PART_EVAL_ONLY_SPLIT;
          force_split[0] = PART_EVAL_ONLY_SPLIT;
        } else if (!is_key_frame && (cm->width * cm->height <= 640 * 360) &&
                   (((maxvar_16x16[m][i] - minvar_16x16[m][i]) >
                         (thresholds[2] >> 1) &&
                     maxvar_16x16[m][i] > thresholds[2]) ||
                    (cpi->sf.rt_sf.prefer_large_partition_blocks &&
                     x->content_state_sb.source_sad_nonrd > kLowSad &&
                     cpi->rc.frame_source_sad < 20000 &&
                     maxvar_16x16[m][i] > (thresholds[2] >> 4) &&
                     maxvar_16x16[m][i] > (minvar_16x16[m][i] << 2)))) {
          force_split[5 + m2 + i] = PART_EVAL_ONLY_SPLIT;
          force_split[m + 1] = PART_EVAL_ONLY_SPLIT;
          force_split[0] = PART_EVAL_ONLY_SPLIT;
        }
      }
    }
    if (force_split[1 + m] == PART_EVAL_ALL) {
      fill_variance_tree(&vt->split[m], BLOCK_64X64);
      get_variance(&vt->split[m].part_variances.none);
      var_64x64 = vt->split[m].part_variances.none.variance;
      max_var_64x64 = AOMMAX(var_64x64, max_var_64x64);
      min_var_64x64 = AOMMIN(var_64x64, min_var_64x64);
      // If the difference of the max-min variances of sub-blocks or max
      // variance of a sub-block is above some threshold of then force this
      // block to split. Only checking this for noise level >= medium, if
      // encoder is in SVC or if we already forced large blocks.

      if (!is_key_frame &&
          (max_var_32x32[m] - min_var_32x32[m]) > 3 * (thresholds[1] >> 3) &&
          max_var_32x32[m] > thresholds[1] >> 1 &&
          (noise_level >= kMedium || cpi->ppi->use_svc ||
           cpi->sf.rt_sf.prefer_large_partition_blocks)) {
        force_split[1 + m] = PART_EVAL_ONLY_SPLIT;
        force_split[0] = PART_EVAL_ONLY_SPLIT;
      }
      avg_64x64 += var_64x64;
    }
    if (is_small_sb) force_split[0] = PART_EVAL_ONLY_SPLIT;
  }

  if (force_split[0] == PART_EVAL_ALL) {
    fill_variance_tree(vt, BLOCK_128X128);
    get_variance(&vt->part_variances.none);
    if (!is_key_frame &&
        vt->part_variances.none.variance > (9 * avg_64x64) >> 5)
      force_split[0] = PART_EVAL_ONLY_SPLIT;

    if (!is_key_frame &&
        (max_var_64x64 - min_var_64x64) > 3 * (thresholds[0] >> 3) &&
        max_var_64x64 > thresholds[0] >> 1)
      force_split[0] = PART_EVAL_ONLY_SPLIT;
  }

  if (mi_col + 32 > tile->mi_col_end || mi_row + 32 > tile->mi_row_end ||
      !set_vt_partitioning(cpi, xd, tile, vt, BLOCK_128X128, mi_row, mi_col,
                           thresholds[0], BLOCK_16X16, force_split[0])) {
    for (m = 0; m < num_64x64_blocks; ++m) {
      const int x64_idx = ((m & 1) << 4);
      const int y64_idx = ((m >> 1) << 4);
      const int m2 = m << 2;

      // Now go through the entire structure, splitting every block size until
      // we get to one that's got a variance lower than our threshold.
      if (!set_vt_partitioning(cpi, xd, tile, &vt->split[m], BLOCK_64X64,
                               mi_row + y64_idx, mi_col + x64_idx,
                               thresholds[1], BLOCK_16X16,
                               force_split[1 + m])) {
        for (i = 0; i < 4; ++i) {
          const int x32_idx = ((i & 1) << 3);
          const int y32_idx = ((i >> 1) << 3);
          const int i2 = (m2 + i) << 2;
          if (!set_vt_partitioning(cpi, xd, tile, &vt->split[m].split[i],
                                   BLOCK_32X32, (mi_row + y64_idx + y32_idx),
                                   (mi_col + x64_idx + x32_idx), thresholds[2],
                                   BLOCK_16X16, force_split[5 + m2 + i])) {
            for (j = 0; j < 4; ++j) {
              const int x16_idx = ((j & 1) << 2);
              const int y16_idx = ((j >> 1) << 2);
              const int split_index = 21 + i2 + j;
              // For inter frames: if variance4x4downsample[] == 1 for this
              // 16x16 block, then the variance is based on 4x4 down-sampling,
              // so use vt2 in set_vt_partioning(), otherwise use vt.
              VP16x16 *vtemp =
                  (!is_key_frame && variance4x4downsample[i2 + j] == 1)
                      ? &vt2[i2 + j]
                      : &vt->split[m].split[i].split[j];
              if (!set_vt_partitioning(cpi, xd, tile, vtemp, BLOCK_16X16,
                                       mi_row + y64_idx + y32_idx + y16_idx,
                                       mi_col + x64_idx + x32_idx + x16_idx,
                                       thresholds[3], BLOCK_8X8,
                                       force_split[split_index])) {
                for (k = 0; k < 4; ++k) {
                  const int x8_idx = (k & 1) << 1;
                  const int y8_idx = (k >> 1) << 1;
                  set_block_size(
                      cpi, (mi_row + y64_idx + y32_idx + y16_idx + y8_idx),
                      (mi_col + x64_idx + x32_idx + x16_idx + x8_idx),
                      BLOCK_8X8);
                }
              }
            }
          }
        }
      }
    }
  }

  if (cpi->sf.rt_sf.short_circuit_low_temp_var) {
    set_low_temp_var_flag(cpi, &x->part_search_info, xd, vt, thresholds,
                          ref_frame_partition, mi_col, mi_row);
  }

  if (vt2) aom_free(vt2);
  if (vt) aom_free(vt);
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, choose_var_based_partitioning_time);
#endif
  return 0;
}
