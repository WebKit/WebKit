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

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/encodeframe.h"
#include "av1/common/seg_common.h"
#include "av1/encoder/segmentation.h"
#include "aom_dsp/aom_dsp_common.h"

#define AQ_C_SEGMENTS 5
#define DEFAULT_AQ2_SEG 3  // Neutral Q segment
#define AQ_C_STRENGTHS 3
static const double aq_c_q_adj_factor[AQ_C_STRENGTHS][AQ_C_SEGMENTS] = {
  { 1.75, 1.25, 1.05, 1.00, 0.90 },
  { 2.00, 1.50, 1.15, 1.00, 0.85 },
  { 2.50, 1.75, 1.25, 1.00, 0.80 }
};
static const double aq_c_transitions[AQ_C_STRENGTHS][AQ_C_SEGMENTS] = {
  { 0.15, 0.30, 0.55, 2.00, 100.0 },
  { 0.20, 0.40, 0.65, 2.00, 100.0 },
  { 0.25, 0.50, 0.75, 2.00, 100.0 }
};
static const double aq_c_var_thresholds[AQ_C_STRENGTHS][AQ_C_SEGMENTS] = {
  { -4.0, -3.0, -2.0, 100.00, 100.0 },
  { -3.5, -2.5, -1.5, 100.00, 100.0 },
  { -3.0, -2.0, -1.0, 100.00, 100.0 }
};

static int get_aq_c_strength(int q_index, aom_bit_depth_t bit_depth) {
  // Approximate base quatizer (truncated to int)
  const int base_quant = av1_ac_quant_QTX(q_index, 0, bit_depth) / 4;
  return (base_quant > 10) + (base_quant > 25);
}

static bool is_frame_aq_enabled(const AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  const RefreshFrameInfo *const refresh_frame = &cpi->refresh_frame;

  return frame_is_intra_only(cm) || cm->features.error_resilient_mode ||
         refresh_frame->alt_ref_frame ||
         (refresh_frame->golden_frame && !cpi->rc.is_src_frame_alt_ref);
}

// Segmentation only makes sense if the target bits per SB is above a threshold.
// Below this the overheads will usually outweigh any benefit.
static bool is_sb_aq_enabled(const AV1_COMP *const cpi) {
  return cpi->rc.sb64_target_rate >= 256;
}

void av1_setup_in_frame_q_adj(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int base_qindex = cm->quant_params.base_qindex;
  struct segmentation *const seg = &cm->seg;
  const int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);

  // Make SURE use of floating point in this function is safe.

  if (resolution_change) {
    memset(cpi->enc_seg.map, 0, cm->mi_params.mi_rows * cm->mi_params.mi_cols);
    av1_clearall_segfeatures(seg);
    av1_disable_segmentation(seg);
    return;
  }

  if (is_frame_aq_enabled(cpi)) {
    int segment;
    const int aq_strength =
        get_aq_c_strength(base_qindex, cm->seq_params->bit_depth);

    // Clear down the segment map.
    memset(cpi->enc_seg.map, DEFAULT_AQ2_SEG,
           cm->mi_params.mi_rows * cm->mi_params.mi_cols);

    av1_clearall_segfeatures(seg);

    if (!is_sb_aq_enabled(cpi)) {
      av1_disable_segmentation(seg);
      return;
    }

    av1_enable_segmentation(seg);

    // Default segment "Q" feature is disabled so it defaults to the baseline Q.
    av1_disable_segfeature(seg, DEFAULT_AQ2_SEG, SEG_LVL_ALT_Q);

    // Use some of the segments for in frame Q adjustment.
    for (segment = 0; segment < AQ_C_SEGMENTS; ++segment) {
      int qindex_delta;

      if (segment == DEFAULT_AQ2_SEG) continue;

      qindex_delta = av1_compute_qdelta_by_rate(
          cpi, cm->current_frame.frame_type, base_qindex,
          aq_c_q_adj_factor[aq_strength][segment]);

      // For AQ complexity mode, we dont allow Q0 in a segment if the base
      // Q is not 0. Q0 (lossless) implies 4x4 only and in AQ mode 2 a segment
      // Q delta is sometimes applied without going back around the rd loop.
      // This could lead to an illegal combination of partition size and q.
      if ((base_qindex != 0) && ((base_qindex + qindex_delta) == 0)) {
        qindex_delta = -base_qindex + 1;
      }
      if ((base_qindex + qindex_delta) > 0) {
        av1_enable_segfeature(seg, segment, SEG_LVL_ALT_Q);
        av1_set_segdata(seg, segment, SEG_LVL_ALT_Q, qindex_delta);
      }
    }
  }
}

#define DEFAULT_LV_THRESH 10.0
#define MIN_DEFAULT_LV_THRESH 8.0
// Select a segment for the current block.
// The choice of segment for a block depends on the ratio of the projected
// bits for the block vs a target average and its spatial complexity.
void av1_caq_select_segment(const AV1_COMP *cpi, MACROBLOCK *mb, BLOCK_SIZE bs,
                            int mi_row, int mi_col, int projected_rate) {
  if ((!is_frame_aq_enabled(cpi)) || (!is_sb_aq_enabled(cpi))) return;
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);

  const int mi_offset = mi_row * cm->mi_params.mi_cols + mi_col;
  const int xmis = AOMMIN(cm->mi_params.mi_cols - mi_col, mi_size_wide[bs]);
  const int ymis = AOMMIN(cm->mi_params.mi_rows - mi_row, mi_size_high[bs]);
  int i;
  unsigned char segment;

  // Rate depends on fraction of a SB64 in frame (xmis * ymis / bw * bh).
  // It is converted to bits << AV1_PROB_COST_SHIFT units.
  const int64_t num = (int64_t)(cpi->rc.sb64_target_rate * xmis * ymis)
                      << AV1_PROB_COST_SHIFT;
  const int denom = cm->seq_params->mib_size * cm->seq_params->mib_size;
  const int target_rate = (int)(num / denom);
  double logvar;
  double low_var_thresh;
  const int aq_strength = get_aq_c_strength(cm->quant_params.base_qindex,
                                            cm->seq_params->bit_depth);

  low_var_thresh =
      (is_stat_consumption_stage_twopass(cpi))
          ? AOMMAX(exp(cpi->twopass_frame.mb_av_energy), MIN_DEFAULT_LV_THRESH)
          : DEFAULT_LV_THRESH;

  av1_setup_src_planes(mb, cpi->source, mi_row, mi_col, num_planes, bs);
  logvar = av1_log_block_var(cpi, mb, bs);

  segment = AQ_C_SEGMENTS - 1;  // Just in case no break out below.
  for (i = 0; i < AQ_C_SEGMENTS; ++i) {
    // Test rate against a threshold value and variance against a threshold.
    // Increasing segment number (higher variance and complexity) = higher Q.
    if ((projected_rate < target_rate * aq_c_transitions[aq_strength][i]) &&
        (logvar < (low_var_thresh + aq_c_var_thresholds[aq_strength][i]))) {
      segment = i;
      break;
    }
  }

  // Fill in the entires in the segment map corresponding to this SB64.
  const int mi_stride = cm->mi_params.mi_cols;
  set_segment_id(cpi->enc_seg.map, mi_offset, xmis, ymis, mi_stride, segment);
}
