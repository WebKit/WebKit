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

#include "av1/common/pred_common.h"
#include "av1/common/seg_common.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"
#include "aom_dsp/aom_dsp_common.h"

CYCLIC_REFRESH *av1_cyclic_refresh_alloc(int mi_rows, int mi_cols) {
  CYCLIC_REFRESH *const cr = aom_calloc(1, sizeof(*cr));
  if (cr == NULL) return NULL;

  cr->map = aom_calloc(mi_rows * mi_cols, sizeof(*cr->map));
  cr->counter_encode_maxq_scene_change = 0;
  if (cr->map == NULL) {
    av1_cyclic_refresh_free(cr);
    return NULL;
  }
  return cr;
}

void av1_cyclic_refresh_free(CYCLIC_REFRESH *cr) {
  if (cr != NULL) {
    aom_free(cr->map);
    aom_free(cr);
  }
}

// Check if this coding block, of size bsize, should be considered for refresh
// (lower-qp coding). Decision can be based on various factors, such as
// size of the coding block (i.e., below min_block size rejected), coding
// mode, and rate/distortion.
static int candidate_refresh_aq(const CYCLIC_REFRESH *cr,
                                const MB_MODE_INFO *mbmi, int64_t rate,
                                int64_t dist, int bsize, int noise_level) {
  MV mv = mbmi->mv[0].as_mv;
  int is_compound = has_second_ref(mbmi);
  // Reject the block for lower-qp coding for non-compound mode if
  // projected distortion is above the threshold, and any of the following
  // is true:
  // 1) mode uses large mv
  // 2) mode is an intra-mode
  // Otherwise accept for refresh.
  if (!is_compound && dist > cr->thresh_dist_sb &&
      (mv.row > cr->motion_thresh || mv.row < -cr->motion_thresh ||
       mv.col > cr->motion_thresh || mv.col < -cr->motion_thresh ||
       !is_inter_block(mbmi)))
    return CR_SEGMENT_ID_BASE;
  else if ((is_compound && noise_level < kMedium) ||
           (bsize >= BLOCK_16X16 && rate < cr->thresh_rate_sb &&
            is_inter_block(mbmi) && mbmi->mv[0].as_int == 0 &&
            cr->rate_boost_fac > 10))
    // More aggressive delta-q for bigger blocks with zero motion.
    return CR_SEGMENT_ID_BOOST2;
  else
    return CR_SEGMENT_ID_BOOST1;
}

// Compute delta-q for the segment.
static int compute_deltaq(const AV1_COMP *cpi, int q, double rate_factor) {
  const CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const RATE_CONTROL *const rc = &cpi->rc;
  int deltaq = av1_compute_qdelta_by_rate(
      rc, cpi->common.current_frame.frame_type, q, rate_factor,
      cpi->is_screen_content_type, cpi->common.seq_params->bit_depth);
  if ((-deltaq) > cr->max_qdelta_perc * q / 100) {
    deltaq = -cr->max_qdelta_perc * q / 100;
  }
  return deltaq;
}

int av1_cyclic_refresh_estimate_bits_at_q(const AV1_COMP *cpi,
                                          double correction_factor) {
  const AV1_COMMON *const cm = &cpi->common;
  const FRAME_TYPE frame_type = cm->current_frame.frame_type;
  const int base_qindex = cm->quant_params.base_qindex;
  const int bit_depth = cm->seq_params->bit_depth;
  const CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const int mbs = cm->mi_params.MBs;
  const int num4x4bl = mbs << 4;
  // Weight for non-base segments: use actual number of blocks refreshed in
  // previous/just encoded frame. Note number of blocks here is in 4x4 units.
  double weight_segment1 = (double)cr->actual_num_seg1_blocks / num4x4bl;
  double weight_segment2 = (double)cr->actual_num_seg2_blocks / num4x4bl;
  if (cpi->rc.rtc_external_ratectrl) {
    weight_segment1 = (double)(cr->percent_refresh * cm->mi_params.mi_rows *
                               cm->mi_params.mi_cols / 100) /
                      num4x4bl;
    weight_segment2 = 0;
  }
  // Take segment weighted average for estimated bits.
  const int estimated_bits =
      (int)((1.0 - weight_segment1 - weight_segment2) *
                av1_estimate_bits_at_q(frame_type, base_qindex, mbs,
                                       correction_factor, bit_depth,
                                       cpi->is_screen_content_type) +
            weight_segment1 * av1_estimate_bits_at_q(
                                  frame_type, base_qindex + cr->qindex_delta[1],
                                  mbs, correction_factor, bit_depth,
                                  cpi->is_screen_content_type) +
            weight_segment2 * av1_estimate_bits_at_q(
                                  frame_type, base_qindex + cr->qindex_delta[2],
                                  mbs, correction_factor, bit_depth,
                                  cpi->is_screen_content_type));
  return estimated_bits;
}

int av1_cyclic_refresh_rc_bits_per_mb(const AV1_COMP *cpi, int i,
                                      double correction_factor) {
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  int bits_per_mb;
  int num4x4bl = cm->mi_params.MBs << 4;
  // Weight for segment prior to encoding: take the average of the target
  // number for the frame to be encoded and the actual from the previous frame.
  double weight_segment =
      (double)((cr->target_num_seg_blocks + cr->actual_num_seg1_blocks +
                cr->actual_num_seg2_blocks) >>
               1) /
      num4x4bl;
  if (cpi->rc.rtc_external_ratectrl) {
    weight_segment = (double)((cr->target_num_seg_blocks +
                               cr->percent_refresh * cm->mi_params.mi_rows *
                                   cm->mi_params.mi_cols / 100) >>
                              1) /
                     num4x4bl;
  }
  // Compute delta-q corresponding to qindex i.
  int deltaq = compute_deltaq(cpi, i, cr->rate_ratio_qdelta);
  // Take segment weighted average for bits per mb.
  bits_per_mb =
      (int)((1.0 - weight_segment) *
                av1_rc_bits_per_mb(cm->current_frame.frame_type, i,
                                   correction_factor, cm->seq_params->bit_depth,
                                   cpi->is_screen_content_type) +
            weight_segment * av1_rc_bits_per_mb(cm->current_frame.frame_type,
                                                i + deltaq, correction_factor,
                                                cm->seq_params->bit_depth,
                                                cpi->is_screen_content_type));
  return bits_per_mb;
}

void av1_cyclic_reset_segment_skip(const AV1_COMP *cpi, MACROBLOCK *const x,
                                   int mi_row, int mi_col, BLOCK_SIZE bsize) {
  int cdf_num;
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int prev_segment_id = mbmi->segment_id;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_params.mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_params.mi_rows - mi_row, bh);

  assert(cm->seg.enabled);

  if (!cr->skip_over4x4) {
    mbmi->segment_id =
        av1_get_spatial_seg_pred(cm, xd, &cdf_num, cr->skip_over4x4);
    if (prev_segment_id != mbmi->segment_id) {
      const int block_index = mi_row * cm->mi_params.mi_cols + mi_col;
      for (int mi_y = 0; mi_y < ymis; mi_y++) {
        for (int mi_x = 0; mi_x < xmis; mi_x++) {
          const int map_offset =
              block_index + mi_y * cm->mi_params.mi_cols + mi_x;
          cr->map[map_offset] = 0;
          cpi->enc_seg.map[map_offset] = mbmi->segment_id;
          cm->cur_frame->seg_map[map_offset] = mbmi->segment_id;
        }
      }
    }
  }
  if (cyclic_refresh_segment_id(prev_segment_id) == CR_SEGMENT_ID_BOOST1)
    x->actual_num_seg1_blocks -= xmis * ymis;
  else if (cyclic_refresh_segment_id(prev_segment_id) == CR_SEGMENT_ID_BOOST2)
    x->actual_num_seg2_blocks -= xmis * ymis;
}

void av1_cyclic_refresh_update_segment(const AV1_COMP *cpi, MACROBLOCK *const x,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int64_t rate, int64_t dist, int skip,
                                       RUN_TYPE dry_run) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_params.mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_params.mi_rows - mi_row, bh);
  const int block_index = mi_row * cm->mi_params.mi_cols + mi_col;
  int noise_level = 0;
  if (cpi->noise_estimate.enabled) noise_level = cpi->noise_estimate.level;
  const int refresh_this_block =
      candidate_refresh_aq(cr, mbmi, rate, dist, bsize, noise_level);
  int sh = cpi->cyclic_refresh->skip_over4x4 ? 2 : 1;
  // Default is to not update the refresh map.
  int new_map_value = cr->map[block_index];

  // If this block is labeled for refresh, check if we should reset the
  // segment_id.
  if (cyclic_refresh_segment_id_boosted(mbmi->segment_id)) {
    mbmi->segment_id = refresh_this_block;
    // Reset segment_id if will be skipped.
    if (skip) mbmi->segment_id = CR_SEGMENT_ID_BASE;
  }

  // Update the cyclic refresh map, to be used for setting segmentation map
  // for the next frame. If the block  will be refreshed this frame, mark it
  // as clean. The magnitude of the -ve influences how long before we consider
  // it for refresh again.
  if (cyclic_refresh_segment_id_boosted(mbmi->segment_id)) {
    new_map_value = -cr->time_for_refresh;
  } else if (refresh_this_block) {
    // Else if it is accepted as candidate for refresh, and has not already
    // been refreshed (marked as 1) then mark it as a candidate for cleanup
    // for future time (marked as 0), otherwise don't update it.
    if (cr->map[block_index] == 1) new_map_value = 0;
  } else {
    // Leave it marked as block that is not candidate for refresh.
    new_map_value = 1;
  }

  // Update entries in the cyclic refresh map with new_map_value, and
  // copy mbmi->segment_id into global segmentation map.
  if (sh == 1) {
    for (int mi_y = 0; mi_y < ymis; mi_y += sh) {
      const int map_offset = block_index + mi_y * cm->mi_params.mi_cols;
      memset(&cr->map[map_offset], new_map_value, xmis);
      memset(&cpi->enc_seg.map[map_offset], mbmi->segment_id, xmis);
      memset(&cm->cur_frame->seg_map[map_offset], mbmi->segment_id, xmis);
    }
  } else {
    for (int mi_y = 0; mi_y < ymis; mi_y += sh) {
      for (int mi_x = 0; mi_x < xmis; mi_x += sh) {
        const int map_offset =
            block_index + mi_y * cm->mi_params.mi_cols + mi_x;
        cr->map[map_offset] = new_map_value;
        cpi->enc_seg.map[map_offset] = mbmi->segment_id;
        cm->cur_frame->seg_map[map_offset] = mbmi->segment_id;
      }
    }
  }
  // Accumulate cyclic refresh update counters.
  if (!dry_run) {
    if (cyclic_refresh_segment_id(mbmi->segment_id) == CR_SEGMENT_ID_BOOST1)
      x->actual_num_seg1_blocks += xmis * ymis;
    else if (cyclic_refresh_segment_id(mbmi->segment_id) ==
             CR_SEGMENT_ID_BOOST2)
      x->actual_num_seg2_blocks += xmis * ymis;
  }
}

// Initializes counters used for cyclic refresh.
void av1_init_cyclic_refresh_counters(MACROBLOCK *const x) {
  x->actual_num_seg1_blocks = 0;
  x->actual_num_seg2_blocks = 0;
}

// Accumulate cyclic refresh counters.
void av1_accumulate_cyclic_refresh_counters(
    CYCLIC_REFRESH *const cyclic_refresh, const MACROBLOCK *const x) {
  cyclic_refresh->actual_num_seg1_blocks += x->actual_num_seg1_blocks;
  cyclic_refresh->actual_num_seg2_blocks += x->actual_num_seg2_blocks;
}

void av1_cyclic_refresh_set_golden_update(AV1_COMP *const cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  // Set minimum gf_interval for GF update to a multiple of the refresh period,
  // with some max limit. Depending on past encoding stats, GF flag may be
  // reset and update may not occur until next baseline_gf_interval.
  const int gf_length_mult[2] = { 8, 4 };
  if (cr->percent_refresh > 0)
    p_rc->baseline_gf_interval =
        AOMMIN(gf_length_mult[cpi->sf.rt_sf.gf_length_lvl] *
                   (100 / cr->percent_refresh),
               MAX_GF_INTERVAL_RT);
  else
    p_rc->baseline_gf_interval = FIXED_GF_INTERVAL_RT;
  if (rc->avg_frame_low_motion && rc->avg_frame_low_motion < 40)
    p_rc->baseline_gf_interval = 16;
}

// Update the segmentation map, and related quantities: cyclic refresh map,
// refresh sb_index, and target number of blocks to be refreshed.
// The map is set to either 0/CR_SEGMENT_ID_BASE (no refresh) or to
// 1/CR_SEGMENT_ID_BOOST1 (refresh) for each superblock.
// Blocks labeled as BOOST1 may later get set to BOOST2 (during the
// encoding of the superblock).
static void cyclic_refresh_update_map(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  unsigned char *const seg_map = cpi->enc_seg.map;
  int i, block_count, bl_index, sb_rows, sb_cols, sbs_in_frame;
  int xmis, ymis, x, y;
  uint64_t sb_sad = 0;
  uint64_t thresh_sad_low = 0;
  uint64_t thresh_sad = INT64_MAX;
  memset(seg_map, CR_SEGMENT_ID_BASE, mi_params->mi_rows * mi_params->mi_cols);
  sb_cols = (mi_params->mi_cols + cm->seq_params->mib_size - 1) /
            cm->seq_params->mib_size;
  sb_rows = (mi_params->mi_rows + cm->seq_params->mib_size - 1) /
            cm->seq_params->mib_size;
  sbs_in_frame = sb_cols * sb_rows;
  // Number of target blocks to get the q delta (segment 1).
  block_count =
      cr->percent_refresh * mi_params->mi_rows * mi_params->mi_cols / 100;
  // Set the segmentation map: cycle through the superblocks, starting at
  // cr->mb_index, and stopping when either block_count blocks have been found
  // to be refreshed, or we have passed through whole frame.
  if (cr->sb_index >= sbs_in_frame) cr->sb_index = 0;
  assert(cr->sb_index < sbs_in_frame);
  i = cr->sb_index;
  cr->target_num_seg_blocks = 0;
  do {
    int sum_map = 0;
    // Get the mi_row/mi_col corresponding to superblock index i.
    int sb_row_index = (i / sb_cols);
    int sb_col_index = i - sb_row_index * sb_cols;
    int mi_row = sb_row_index * cm->seq_params->mib_size;
    int mi_col = sb_col_index * cm->seq_params->mib_size;
    assert(mi_row >= 0 && mi_row < mi_params->mi_rows);
    assert(mi_col >= 0 && mi_col < mi_params->mi_cols);
    bl_index = mi_row * mi_params->mi_cols + mi_col;
    // Loop through all MI blocks in superblock and update map.
    xmis = AOMMIN(mi_params->mi_cols - mi_col, cm->seq_params->mib_size);
    ymis = AOMMIN(mi_params->mi_rows - mi_row, cm->seq_params->mib_size);
    if (cpi->sf.rt_sf.sad_based_comp_prune && cr->use_block_sad_scene_det &&
        cpi->rc.frames_since_key > 30 &&
        cr->counter_encode_maxq_scene_change > 30 &&
        cpi->src_sad_blk_64x64 != NULL) {
      sb_sad = cpi->src_sad_blk_64x64[sb_col_index + sb_cols * sb_row_index];
      int scale = (cm->width * cm->height < 640 * 360) ? 6 : 8;
      int scale_low = 2;
      thresh_sad = (scale * 64 * 64);
      thresh_sad_low = (scale_low * 64 * 64);
    }
    // cr_map only needed at 8x8 blocks.
    for (y = 0; y < ymis; y += 2) {
      for (x = 0; x < xmis; x += 2) {
        const int bl_index2 = bl_index + y * mi_params->mi_cols + x;
        // If the block is as a candidate for clean up then mark it
        // for possible boost/refresh (segment 1). The segment id may get
        // reset to 0 later if block gets coded anything other than low motion.
        // If the block_sad (sb_sad) is very low label it for refresh anyway.
        if (cr->map[bl_index2] == 0 || sb_sad < thresh_sad_low) {
          sum_map += 4;
        } else if (cr->map[bl_index2] < 0) {
          cr->map[bl_index2]++;
        }
      }
    }
    // Enforce constant segment over superblock.
    // If segment is at least half of superblock, set to 1.
    // Enforce that block sad (sb_sad) is not too high.
    if (sum_map >= (xmis * ymis) >> 1 && sb_sad < thresh_sad) {
      for (y = 0; y < ymis; y++)
        for (x = 0; x < xmis; x++) {
          seg_map[bl_index + y * mi_params->mi_cols + x] = CR_SEGMENT_ID_BOOST1;
        }
      cr->target_num_seg_blocks += xmis * ymis;
    }
    i++;
    if (i == sbs_in_frame) {
      i = 0;
    }
  } while (cr->target_num_seg_blocks < block_count && i != cr->sb_index);
  cr->sb_index = i;
  if (cr->target_num_seg_blocks == 0) {
    // Disable segmentation, seg_map is already set to 0 above.
    av1_disable_segmentation(&cm->seg);
  }
}

// Set cyclic refresh parameters.
void av1_cyclic_refresh_update_parameters(AV1_COMP *const cpi) {
  // TODO(marpan): Parameters need to be tuned.
  const RATE_CONTROL *const rc = &cpi->rc;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  int num4x4bl = cm->mi_params.MBs << 4;
  int target_refresh = 0;
  double weight_segment_target = 0;
  double weight_segment = 0;
  int qp_thresh = AOMMIN(20, rc->best_quality << 1);
  if (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN)
    qp_thresh = AOMMIN(35, rc->best_quality << 1);
  int qp_max_thresh = 118 * MAXQ >> 7;
  const int scene_change_detected =
      cpi->rc.high_source_sad ||
      (cpi->ppi->use_svc && cpi->svc.high_source_sad_superframe);
  // Although this segment feature for RTC is only used for
  // blocks >= 8X8, for more efficient coding of the seg map
  // cur_frame->seg_map needs to set at 4x4 along with the
  // function av1_cyclic_reset_segment_skip(). Skipping over
  // 4x4 will therefore have small bdrate loss (~0.2%), so
  // we use it only for speed > 9 for now.
  // Also if loop-filter deltas is applied via segment, then
  // we need to set cr->skip_over4x4 = 1.
  cr->skip_over4x4 = (cpi->oxcf.speed > 9) ? 1 : 0;
  cr->apply_cyclic_refresh = 1;
  if (frame_is_intra_only(cm) || is_lossless_requested(&cpi->oxcf.rc_cfg) ||
      scene_change_detected || cpi->svc.temporal_layer_id > 0 ||
      p_rc->avg_frame_qindex[INTER_FRAME] < qp_thresh ||
      (cpi->svc.number_spatial_layers > 1 &&
       cpi->svc.layer_context[cpi->svc.temporal_layer_id].is_key_frame) ||
      (rc->frames_since_key > 20 &&
       p_rc->avg_frame_qindex[INTER_FRAME] > qp_max_thresh) ||
      (rc->avg_frame_low_motion && rc->avg_frame_low_motion < 30 &&
       rc->frames_since_key > 40)) {
    cr->apply_cyclic_refresh = 0;
    return;
  }
  cr->percent_refresh = 10;
  // Increase the amount of refresh for #temporal_layers > 2, and for some
  // frames after scene change that is encoded at high Q.
  if (cpi->svc.number_temporal_layers > 2)
    cr->percent_refresh = 15;
  else if (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN &&
           cr->counter_encode_maxq_scene_change < 20)
    cr->percent_refresh = 15;
  cr->max_qdelta_perc = 60;
  cr->time_for_refresh = 0;
  cr->use_block_sad_scene_det =
      (cpi->oxcf.tune_cfg.content != AOM_CONTENT_SCREEN &&
       cm->seq_params->sb_size == BLOCK_64X64)
          ? 1
          : 0;
  cr->motion_thresh = 32;
  cr->rate_boost_fac =
      (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN) ? 10 : 15;
  // Use larger delta-qp (increase rate_ratio_qdelta) for first few (~4)
  // periods of the refresh cycle, after a key frame.
  // Account for larger interval on base layer for temporal layers.
  if (cr->percent_refresh > 0 &&
      rc->frames_since_key <
          (4 * cpi->svc.number_temporal_layers) * (100 / cr->percent_refresh)) {
    cr->rate_ratio_qdelta = 3.0;
  } else {
    cr->rate_ratio_qdelta = 2.0;
  }
  // Adjust some parameters for low resolutions.
  if (cm->width * cm->height <= 352 * 288) {
    if (rc->avg_frame_bandwidth < 3000) {
      cr->motion_thresh = 16;
      cr->rate_boost_fac = 13;
    } else {
      cr->max_qdelta_perc = 50;
      cr->rate_ratio_qdelta = AOMMAX(cr->rate_ratio_qdelta, 2.0);
    }
  }
  if (cpi->oxcf.rc_cfg.mode == AOM_VBR) {
    // To be adjusted for VBR mode, e.g., based on gf period and boost.
    // For now use smaller qp-delta (than CBR), no second boosted seg, and
    // turn-off (no refresh) on golden refresh (since it's already boosted).
    cr->percent_refresh = 10;
    cr->rate_ratio_qdelta = 1.5;
    cr->rate_boost_fac = 10;
    if (cpi->refresh_frame.golden_frame) {
      cr->percent_refresh = 0;
      cr->rate_ratio_qdelta = 1.0;
    }
  }
  // Weight for segment prior to encoding: take the average of the target
  // number for the frame to be encoded and the actual from the previous frame.
  // Use the target if its less. To be used for setting the base qp for the
  // frame in av1_rc_regulate_q.
  target_refresh =
      cr->percent_refresh * cm->mi_params.mi_rows * cm->mi_params.mi_cols / 100;
  weight_segment_target = (double)(target_refresh) / num4x4bl;
  weight_segment = (double)((target_refresh + cr->actual_num_seg1_blocks +
                             cr->actual_num_seg2_blocks) >>
                            1) /
                   num4x4bl;
  if (weight_segment_target < 7 * weight_segment / 8)
    weight_segment = weight_segment_target;
  cr->weight_segment = weight_segment;
  if (rc->rtc_external_ratectrl) {
    cr->actual_num_seg1_blocks = cr->percent_refresh * cm->mi_params.mi_rows *
                                 cm->mi_params.mi_cols / 100;
    cr->actual_num_seg2_blocks = 0;
    cr->weight_segment = (double)(cr->actual_num_seg1_blocks) / num4x4bl;
  }
}

// Setup cyclic background refresh: set delta q and segmentation map.
void av1_cyclic_refresh_setup(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  struct segmentation *const seg = &cm->seg;
  const int scene_change_detected =
      cpi->rc.high_source_sad ||
      (cpi->ppi->use_svc && cpi->svc.high_source_sad_superframe);
  const int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);
  if (resolution_change) av1_cyclic_refresh_reset_resize(cpi);
  if (!cr->apply_cyclic_refresh) {
    // Set segmentation map to 0 and disable.
    unsigned char *const seg_map = cpi->enc_seg.map;
    memset(seg_map, 0, cm->mi_params.mi_rows * cm->mi_params.mi_cols);
    av1_disable_segmentation(&cm->seg);
    if (cm->current_frame.frame_type == KEY_FRAME || scene_change_detected) {
      cr->sb_index = 0;
      cr->counter_encode_maxq_scene_change = 0;
    }
    return;
  } else {
    cr->counter_encode_maxq_scene_change++;
    const double q = av1_convert_qindex_to_q(cm->quant_params.base_qindex,
                                             cm->seq_params->bit_depth);
    // Set rate threshold to some multiple (set to 2 for now) of the target
    // rate (target is given by sb64_target_rate and scaled by 256).
    cr->thresh_rate_sb = ((int64_t)(rc->sb64_target_rate) << 8) << 2;
    // Distortion threshold, quadratic in Q, scale factor to be adjusted.
    // q will not exceed 457, so (q * q) is within 32bit; see:
    // av1_convert_qindex_to_q(), av1_ac_quant(), ac_qlookup*[].
    cr->thresh_dist_sb = ((int64_t)(q * q)) << 2;
    // For low-resoln or lower speeds, the rate/dist thresholds need to be
    // tuned/updated.
    if (cpi->oxcf.speed <= 7 || (cm->width * cm->height < 640 * 360)) {
      cr->thresh_dist_sb = 0;
      cr->thresh_rate_sb = INT64_MAX;
    }
    // Set up segmentation.
    // Clear down the segment map.
    av1_enable_segmentation(&cm->seg);
    av1_clearall_segfeatures(seg);

    // Note: setting temporal_update has no effect, as the seg-map coding method
    // (temporal or spatial) is determined in
    // av1_choose_segmap_coding_method(),
    // based on the coding cost of each method. For error_resilient mode on the
    // last_frame_seg_map is set to 0, so if temporal coding is used, it is
    // relative to 0 previous map.
    // seg->temporal_update = 0;

    // Segment BASE "Q" feature is disabled so it defaults to the baseline Q.
    av1_disable_segfeature(seg, CR_SEGMENT_ID_BASE, SEG_LVL_ALT_Q);
    // Use segment BOOST1 for in-frame Q adjustment.
    av1_enable_segfeature(seg, CR_SEGMENT_ID_BOOST1, SEG_LVL_ALT_Q);
    // Use segment BOOST2 for more aggressive in-frame Q adjustment.
    av1_enable_segfeature(seg, CR_SEGMENT_ID_BOOST2, SEG_LVL_ALT_Q);

    // Set the q delta for segment BOOST1.
    const CommonQuantParams *const quant_params = &cm->quant_params;
    int qindex_delta =
        compute_deltaq(cpi, quant_params->base_qindex, cr->rate_ratio_qdelta);
    cr->qindex_delta[1] = qindex_delta;

    // Compute rd-mult for segment BOOST1.
    const int qindex2 = clamp(
        quant_params->base_qindex + quant_params->y_dc_delta_q + qindex_delta,
        0, MAXQ);
    cr->rdmult = av1_compute_rd_mult(cpi, qindex2);

    av1_set_segdata(seg, CR_SEGMENT_ID_BOOST1, SEG_LVL_ALT_Q, qindex_delta);

    // Set a more aggressive (higher) q delta for segment BOOST2.
    qindex_delta = compute_deltaq(
        cpi, quant_params->base_qindex,
        AOMMIN(CR_MAX_RATE_TARGET_RATIO,
               0.1 * cr->rate_boost_fac * cr->rate_ratio_qdelta));
    cr->qindex_delta[2] = qindex_delta;
    av1_set_segdata(seg, CR_SEGMENT_ID_BOOST2, SEG_LVL_ALT_Q, qindex_delta);

    // Update the segmentation and refresh map.
    cyclic_refresh_update_map(cpi);
  }
}

int av1_cyclic_refresh_get_rdmult(const CYCLIC_REFRESH *cr) {
  return cr->rdmult;
}

void av1_cyclic_refresh_reset_resize(AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  memset(cr->map, 0, cm->mi_params.mi_rows * cm->mi_params.mi_cols);
  cr->sb_index = 0;
  cpi->refresh_frame.golden_frame = true;
  cr->apply_cyclic_refresh = 0;
  cr->counter_encode_maxq_scene_change = 0;
}

int av1_cyclic_refresh_disable_lf_cdef(AV1_COMP *const cpi) {
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  // TODO(marpan): Tune these conditons, add QP dependence.
  if (cpi->rc.frames_since_key > 30 && cr->percent_refresh > 0 &&
      cr->counter_encode_maxq_scene_change > 300 / cr->percent_refresh &&
      cpi->rc.frame_source_sad < 1000)
    return 1;
  return 0;
}
