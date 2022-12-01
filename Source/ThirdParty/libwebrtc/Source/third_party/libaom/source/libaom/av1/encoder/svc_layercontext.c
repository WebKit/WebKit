/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_alloc.h"

static void swap_ptr(void *a, void *b) {
  void **a_p = (void **)a;
  void **b_p = (void **)b;
  void *c = *a_p;
  *a_p = *b_p;
  *b_p = c;
}

void av1_init_layer_context(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  SVC *const svc = &cpi->svc;
  int mi_rows = cpi->common.mi_params.mi_rows;
  int mi_cols = cpi->common.mi_params.mi_cols;
  svc->base_framerate = 30.0;
  svc->current_superframe = 0;
  svc->force_zero_mode_spatial_ref = 1;
  svc->num_encoded_top_layer = 0;
  svc->use_flexible_mode = 0;

  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      PRIMARY_RATE_CONTROL *const lp_rc = &lc->p_rc;
      lrc->ni_av_qi = oxcf->rc_cfg.worst_allowed_q;
      lp_rc->total_actual_bits = 0;
      lrc->ni_tot_qi = 0;
      lp_rc->tot_q = 0.0;
      lp_rc->avg_q = 0.0;
      lp_rc->ni_frames = 0;
      lrc->decimation_count = 0;
      lrc->decimation_factor = 0;
      lrc->worst_quality = av1_quantizer_to_qindex(lc->max_q);
      lrc->best_quality = av1_quantizer_to_qindex(lc->min_q);
      lrc->rtc_external_ratectrl = 0;
      for (int i = 0; i < RATE_FACTOR_LEVELS; ++i) {
        lp_rc->rate_correction_factors[i] = 1.0;
      }
      lc->target_bandwidth = lc->layer_target_bitrate;
      lp_rc->last_q[INTER_FRAME] = lrc->worst_quality;
      lp_rc->avg_frame_qindex[INTER_FRAME] = lrc->worst_quality;
      lp_rc->avg_frame_qindex[KEY_FRAME] = lrc->worst_quality;
      lp_rc->buffer_level =
          oxcf->rc_cfg.starting_buffer_level_ms * lc->target_bandwidth / 1000;
      lp_rc->bits_off_target = lp_rc->buffer_level;
      // Initialize the cyclic refresh parameters. If spatial layers are used
      // (i.e., ss_number_layers > 1), these need to be updated per spatial
      // layer. Cyclic refresh is only applied on base temporal layer.
      if (svc->number_spatial_layers > 1 && tl == 0) {
        lc->sb_index = 0;
        lc->actual_num_seg1_blocks = 0;
        lc->actual_num_seg2_blocks = 0;
        lc->counter_encode_maxq_scene_change = 0;
        if (lc->map) aom_free(lc->map);
        CHECK_MEM_ERROR(cm, lc->map,
                        aom_calloc(mi_rows * mi_cols, sizeof(*lc->map)));
      }
    }
    svc->downsample_filter_type[sl] = BILINEAR;
    svc->downsample_filter_phase[sl] = 8;
  }
  if (svc->number_spatial_layers == 3) {
    svc->downsample_filter_type[0] = EIGHTTAP_SMOOTH;
  }
  svc->ref_frame_comp[0] = 0;
  svc->ref_frame_comp[1] = 0;
  svc->ref_frame_comp[2] = 0;
}

// Update the layer context from a change_config() call.
void av1_update_layer_context_change_config(AV1_COMP *const cpi,
                                            const int64_t target_bandwidth) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  SVC *const svc = &cpi->svc;
  int layer = 0;
  int64_t spatial_layer_target = 0;
  float bitrate_alloc = 1.0;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      svc->layer_context[layer].target_bandwidth = lc->layer_target_bitrate;
    }
    spatial_layer_target = svc->layer_context[layer].target_bandwidth;
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      LAYER_CONTEXT *const lc =
          &svc->layer_context[sl * svc->number_temporal_layers + tl];
      RATE_CONTROL *const lrc = &lc->rc;
      PRIMARY_RATE_CONTROL *const lp_rc = &lc->p_rc;
      lc->spatial_layer_target_bandwidth = spatial_layer_target;
      bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
      lp_rc->starting_buffer_level =
          (int64_t)(p_rc->starting_buffer_level * bitrate_alloc);
      lp_rc->optimal_buffer_level =
          (int64_t)(p_rc->optimal_buffer_level * bitrate_alloc);
      lp_rc->maximum_buffer_size =
          (int64_t)(p_rc->maximum_buffer_size * bitrate_alloc);
      lp_rc->bits_off_target =
          AOMMIN(lp_rc->bits_off_target, lp_rc->maximum_buffer_size);
      lp_rc->buffer_level =
          AOMMIN(lp_rc->buffer_level, lp_rc->maximum_buffer_size);
      lc->framerate = cpi->framerate / lc->framerate_factor;
      lrc->avg_frame_bandwidth =
          (int)round(lc->target_bandwidth / lc->framerate);
      lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
      lrc->rtc_external_ratectrl = rc->rtc_external_ratectrl;
      lrc->worst_quality = av1_quantizer_to_qindex(lc->max_q);
      lrc->best_quality = av1_quantizer_to_qindex(lc->min_q);
    }
  }
}

/*!\brief Return layer context for current layer.
 *
 * \ingroup rate_control
 * \param[in]       cpi   Top level encoder structure
 *
 * \return LAYER_CONTEXT for current layer.
 */
static LAYER_CONTEXT *get_layer_context(AV1_COMP *const cpi) {
  return &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                     cpi->svc.number_temporal_layers +
                                 cpi->svc.temporal_layer_id];
}

void av1_update_temporal_layer_framerate(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;
  const int tl = svc->temporal_layer_id;
  lc->framerate = cpi->framerate / lc->framerate_factor;
  lrc->avg_frame_bandwidth = (int)round(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (tl == 0) {
    lc->avg_frame_size = lrc->avg_frame_bandwidth;
  } else {
    int prev_layer = svc->spatial_layer_id * svc->number_temporal_layers +
                     svc->temporal_layer_id - 1;
    LAYER_CONTEXT *const lcprev = &svc->layer_context[prev_layer];
    const double prev_layer_framerate =
        cpi->framerate / lcprev->framerate_factor;
    const int64_t prev_layer_target_bandwidth = lcprev->layer_target_bitrate;
    lc->avg_frame_size =
        (int)round((lc->target_bandwidth - prev_layer_target_bandwidth) /
                   (lc->framerate - prev_layer_framerate));
  }
}

static AOM_INLINE bool check_ref_is_low_spatial_res_super_frame(
    int ref_frame, const SVC *svc) {
  int ref_frame_idx = svc->ref_idx[ref_frame - 1];
  return svc->buffer_time_index[ref_frame_idx] == svc->current_superframe &&
         svc->buffer_spatial_layer[ref_frame_idx] <= svc->spatial_layer_id - 1;
}

void av1_restore_layer_context(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;
  // Restore layer rate control.
  cpi->rc = lc->rc;
  cpi->ppi->p_rc = lc->p_rc;
  cpi->oxcf.rc_cfg.target_bandwidth = lc->target_bandwidth;
  cpi->gf_frame_index = 0;
  cpi->mv_search_params.max_mv_magnitude = lc->max_mv_magnitude;
  if (cpi->mv_search_params.max_mv_magnitude == 0)
    cpi->mv_search_params.max_mv_magnitude = AOMMAX(cm->width, cm->height);
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  cpi->rc.frames_since_key = old_frame_since_key;
  cpi->rc.frames_to_key = old_frame_to_key;
  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      svc->number_spatial_layers > 1 && svc->temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    swap_ptr(&cr->map, &lc->map);
    cr->sb_index = lc->sb_index;
    cr->actual_num_seg1_blocks = lc->actual_num_seg1_blocks;
    cr->actual_num_seg2_blocks = lc->actual_num_seg2_blocks;
  }
  svc->skip_mvsearch_last = 0;
  svc->skip_mvsearch_gf = 0;
  svc->skip_mvsearch_altref = 0;
  // For each reference (LAST/GOLDEN) set the skip_mvsearch_last/gf frame flags.
  // This is to skip searching mv for that reference if it was last
  // refreshed (i.e., buffer slot holding that reference was refreshed) on the
  // previous spatial layer(s) at the same time (current_superframe).
  if (svc->set_ref_frame_config && svc->force_zero_mode_spatial_ref) {
    if (check_ref_is_low_spatial_res_super_frame(LAST_FRAME, svc)) {
      svc->skip_mvsearch_last = 1;
    }
    if (check_ref_is_low_spatial_res_super_frame(GOLDEN_FRAME, svc)) {
      svc->skip_mvsearch_gf = 1;
    }
    if (check_ref_is_low_spatial_res_super_frame(ALTREF_FRAME, svc)) {
      svc->skip_mvsearch_altref = 1;
    }
  }
}

void av1_save_layer_context(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  LAYER_CONTEXT *lc = get_layer_context(cpi);
  lc->rc = cpi->rc;
  lc->p_rc = cpi->ppi->p_rc;
  lc->target_bandwidth = (int)cpi->oxcf.rc_cfg.target_bandwidth;
  lc->group_index = cpi->gf_frame_index;
  lc->max_mv_magnitude = cpi->mv_search_params.max_mv_magnitude;
  if (svc->spatial_layer_id == 0) svc->base_framerate = cpi->framerate;
  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->svc.number_spatial_layers > 1 && svc->temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    signed char *temp = lc->map;
    lc->map = cr->map;
    cr->map = temp;
    lc->sb_index = cr->sb_index;
    lc->actual_num_seg1_blocks = cr->actual_num_seg1_blocks;
    lc->actual_num_seg2_blocks = cr->actual_num_seg2_blocks;
  }
  // For any buffer slot that is refreshed, update it with
  // the spatial_layer_id and the current_superframe.
  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    // All slots are refreshed on KEY.
    for (unsigned int i = 0; i < REF_FRAMES; i++) {
      svc->buffer_time_index[i] = svc->current_superframe;
      svc->buffer_spatial_layer[i] = svc->spatial_layer_id;
    }
  } else if (cpi->svc.set_ref_frame_config) {
    for (unsigned int i = 0; i < INTER_REFS_PER_FRAME; i++) {
      int ref_frame_map_idx = svc->ref_idx[i];
      if (cpi->svc.refresh[ref_frame_map_idx]) {
        svc->buffer_time_index[ref_frame_map_idx] = svc->current_superframe;
        svc->buffer_spatial_layer[ref_frame_map_idx] = svc->spatial_layer_id;
      }
    }
  }
  for (unsigned int i = 0; i < REF_FRAMES; i++) {
    if (frame_is_intra_only(cm) ||
        cm->current_frame.refresh_frame_flags & (1 << i)) {
      svc->spatial_layer_fb[i] = svc->spatial_layer_id;
      svc->temporal_layer_fb[i] = svc->temporal_layer_id;
    }
  }
  if (svc->spatial_layer_id == svc->number_spatial_layers - 1)
    svc->current_superframe++;
}

int av1_svc_primary_ref_frame(const AV1_COMP *const cpi) {
  const SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  int fb_idx = -1;
  int primary_ref_frame = PRIMARY_REF_NONE;
  // Set the primary_ref_frame to LAST_FRAME if that buffer slot for LAST
  // was last updated on a lower temporal layer (or base TL0) and for the
  // same spatial layer. For RTC patterns this allows for continued decoding
  // when set of enhancement layers are dropped (continued decoding starting
  // at next base TL0), so error_resilience can be off/0 for all layers.
  fb_idx = get_ref_frame_map_idx(cm, LAST_FRAME);
  if (svc->spatial_layer_fb[fb_idx] == svc->spatial_layer_id &&
      (svc->temporal_layer_fb[fb_idx] < svc->temporal_layer_id ||
       svc->temporal_layer_fb[fb_idx] == 0)) {
    primary_ref_frame = 0;  // LAST_FRAME
  }
  return primary_ref_frame;
}

void av1_free_svc_cyclic_refresh(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      if (lc->map) aom_free(lc->map);
    }
  }
}

void av1_svc_reset_temporal_layers(AV1_COMP *const cpi, int is_key) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      lc = &cpi->svc.layer_context[sl * svc->number_temporal_layers + tl];
      if (is_key) lc->frames_from_key_frame = 0;
    }
  }
  av1_update_temporal_layer_framerate(cpi);
  av1_restore_layer_context(cpi);
}

void av1_get_layer_resolution(const int width_org, const int height_org,
                              const int num, const int den, int *width_out,
                              int *height_out) {
  int w, h;
  if (width_out == NULL || height_out == NULL || den == 0) return;
  w = width_org * num / den;
  h = height_org * num / den;
  // Make height and width even.
  w += w % 2;
  h += h % 2;
  *width_out = w;
  *height_out = h;
}

void av1_one_pass_cbr_svc_start_layer(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  int width = 0, height = 0;
  lc = &svc->layer_context[svc->spatial_layer_id * svc->number_temporal_layers +
                           svc->temporal_layer_id];
  av1_get_layer_resolution(cpi->oxcf.frm_dim_cfg.width,
                           cpi->oxcf.frm_dim_cfg.height, lc->scaling_factor_num,
                           lc->scaling_factor_den, &width, &height);
  // Use Eightap_smooth for low resolutions.
  if (width * height <= 320 * 240)
    svc->downsample_filter_type[svc->spatial_layer_id] = EIGHTTAP_SMOOTH;

  cpi->common.width = width;
  cpi->common.height = height;
  alloc_mb_mode_info_buffers(cpi);
  av1_update_frame_size(cpi);
  if (svc->spatial_layer_id == 0) svc->high_source_sad_superframe = 0;
}

enum {
  SVC_LAST_FRAME = 0,
  SVC_LAST2_FRAME,
  SVC_LAST3_FRAME,
  SVC_GOLDEN_FRAME,
  SVC_BWDREF_FRAME,
  SVC_ALTREF2_FRAME,
  SVC_ALTREF_FRAME
};

// For fixed svc mode: fixed pattern is set based on the number of
// spatial and temporal layers, and the ksvc_fixed_mode.
void av1_set_svc_fixed_mode(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  int i;
  assert(svc->use_flexible_mode == 0);
  // Fixed SVC mode only supports at most 3 spatial or temporal layers.
  assert(svc->number_spatial_layers >= 1 && svc->number_spatial_layers <= 3 &&
         svc->number_temporal_layers >= 1 && svc->number_temporal_layers <= 3);
  svc->set_ref_frame_config = 1;
  int superframe_cnt = svc->current_superframe;
  // Set the reference map buffer idx for the 7 references:
  // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
  // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
  for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = i;
  for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->reference[i] = 0;
  for (i = 0; i < REF_FRAMES; i++) svc->refresh[i] = 0;
  // Always reference LAST, and reference GOLDEN on SL > 0.
  // For KSVC: GOLDEN reference will be removed on INTER_FRAMES later
  // when frame_type is set.
  svc->reference[SVC_LAST_FRAME] = 1;
  if (svc->spatial_layer_id > 0) svc->reference[SVC_GOLDEN_FRAME] = 1;
  if (svc->temporal_layer_id == 0) {
    // Base temporal layer.
    if (svc->spatial_layer_id == 0) {
      // Set all buffer_idx to 0. Update slot 0 (LAST).
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      svc->refresh[0] = 1;
    } else if (svc->spatial_layer_id == 1) {
      // Set buffer_idx for LAST to slot 1, GOLDEN (and all other refs) to
      // slot 0. Update slot 1 (LAST).
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      svc->ref_idx[SVC_LAST_FRAME] = 1;
      svc->refresh[1] = 1;
    } else if (svc->spatial_layer_id == 2) {
      // Set buffer_idx for LAST to slot 2, GOLDEN (and all other refs) to
      // slot 1. Update slot 2 (LAST).
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 1;
      svc->ref_idx[SVC_LAST_FRAME] = 2;
      svc->refresh[2] = 1;
    }
  } else if (svc->temporal_layer_id == 2 && (superframe_cnt - 1) % 4 == 0) {
    // First top temporal enhancement layer.
    if (svc->spatial_layer_id == 0) {
      // Reference LAST (slot 0).
      // Set GOLDEN to slot 3 and update slot 3.
      // Set all other buffer_idx to slot 0.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      if (svc->spatial_layer_id < svc->number_spatial_layers - 1) {
        svc->ref_idx[SVC_GOLDEN_FRAME] = 3;
        svc->refresh[3] = 1;
      }
    } else if (svc->spatial_layer_id == 1) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
      // GOLDEN (and all other refs) to slot 3.
      // Set LAST2 to slot 4 and Update slot 4.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 3;
      svc->ref_idx[SVC_LAST_FRAME] = 1;
      if (svc->spatial_layer_id < svc->number_spatial_layers - 1) {
        svc->ref_idx[SVC_LAST2_FRAME] = 4;
        svc->refresh[4] = 1;
      }
    } else if (svc->spatial_layer_id == 2) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
      // GOLDEN (and all other refs) to slot 4.
      // No update.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 4;
      svc->ref_idx[SVC_LAST_FRAME] = 2;
    }
  } else if (svc->temporal_layer_id == 1) {
    // Middle temporal enhancement layer.
    if (svc->spatial_layer_id == 0) {
      // Reference LAST.
      // Set all buffer_idx to 0.
      // Set GOLDEN to slot 5 and update slot 5.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      if (svc->temporal_layer_id < svc->number_temporal_layers - 1) {
        svc->ref_idx[SVC_GOLDEN_FRAME] = 5;
        svc->refresh[5] = 1;
      }
    } else if (svc->spatial_layer_id == 1) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
      // GOLDEN (and all other refs) to slot 5.
      // Set LAST3 to slot 6 and update slot 6.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 5;
      svc->ref_idx[SVC_LAST_FRAME] = 1;
      if (svc->temporal_layer_id < svc->number_temporal_layers - 1) {
        svc->ref_idx[SVC_LAST3_FRAME] = 6;
        svc->refresh[6] = 1;
      }
    } else if (svc->spatial_layer_id == 2) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
      // GOLDEN (and all other refs) to slot 6.
      // Set LAST3 to slot 7 and update slot 7.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 6;
      svc->ref_idx[SVC_LAST_FRAME] = 2;
      if (svc->temporal_layer_id < svc->number_temporal_layers - 1) {
        svc->ref_idx[SVC_LAST3_FRAME] = 7;
        svc->refresh[7] = 1;
      }
    }
  } else if (svc->temporal_layer_id == 2 && (superframe_cnt - 3) % 4 == 0) {
    // Second top temporal enhancement layer.
    if (svc->spatial_layer_id == 0) {
      // Set LAST to slot 5 and reference LAST.
      // Set GOLDEN to slot 3 and update slot 3.
      // Set all other buffer_idx to 0.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      svc->ref_idx[SVC_LAST_FRAME] = 5;
      if (svc->spatial_layer_id < svc->number_spatial_layers - 1) {
        svc->ref_idx[SVC_GOLDEN_FRAME] = 3;
        svc->refresh[3] = 1;
      }
    } else if (svc->spatial_layer_id == 1) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 6,
      // GOLDEN to slot 3. Set LAST2 to slot 4 and update slot 4.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      svc->ref_idx[SVC_LAST_FRAME] = 6;
      svc->ref_idx[SVC_GOLDEN_FRAME] = 3;
      if (svc->spatial_layer_id < svc->number_spatial_layers - 1) {
        svc->ref_idx[SVC_LAST2_FRAME] = 4;
        svc->refresh[4] = 1;
      }
    } else if (svc->spatial_layer_id == 2) {
      // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 7,
      // GOLDEN to slot 4. No update.
      for (i = 0; i < INTER_REFS_PER_FRAME; i++) svc->ref_idx[i] = 0;
      svc->ref_idx[SVC_LAST_FRAME] = 7;
      svc->ref_idx[SVC_GOLDEN_FRAME] = 4;
    }
  }
}

void av1_svc_check_reset_layer_rc_flag(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    // Check for reset based on avg_frame_bandwidth for spatial layer sl.
    int layer = LAYER_IDS_TO_IDX(sl, svc->number_temporal_layers - 1,
                                 svc->number_temporal_layers);
    LAYER_CONTEXT *lc = &svc->layer_context[layer];
    RATE_CONTROL *lrc = &lc->rc;
    if (lrc->avg_frame_bandwidth > (3 * lrc->prev_avg_frame_bandwidth >> 1) ||
        lrc->avg_frame_bandwidth < (lrc->prev_avg_frame_bandwidth >> 1)) {
      // Reset for all temporal layers with spatial layer sl.
      for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
        int layer2 = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
        LAYER_CONTEXT *lc2 = &svc->layer_context[layer2];
        RATE_CONTROL *lrc2 = &lc2->rc;
        PRIMARY_RATE_CONTROL *lp_rc2 = &lc2->p_rc;
        PRIMARY_RATE_CONTROL *const lp_rc = &lc2->p_rc;
        lrc2->rc_1_frame = 0;
        lrc2->rc_2_frame = 0;
        lp_rc2->bits_off_target = lp_rc->optimal_buffer_level;
        lp_rc2->buffer_level = lp_rc->optimal_buffer_level;
      }
    }
  }
}
