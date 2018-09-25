/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_svc_layercontext.h"
#include "vp9/encoder/vp9_extend.h"
#include "vpx_dsp/vpx_dsp_common.h"

#define SMALL_FRAME_WIDTH 32
#define SMALL_FRAME_HEIGHT 16

void vp9_init_layer_context(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int mi_rows = cpi->common.mi_rows;
  int mi_cols = cpi->common.mi_cols;
  int sl, tl, i;
  int alt_ref_idx = svc->number_spatial_layers;

  svc->spatial_layer_id = 0;
  svc->temporal_layer_id = 0;
  svc->first_spatial_layer_to_encode = 0;
  svc->rc_drop_superframe = 0;
  svc->force_zero_mode_spatial_ref = 0;
  svc->use_base_mv = 0;
  svc->use_partition_reuse = 0;
  svc->scaled_temp_is_alloc = 0;
  svc->scaled_one_half = 0;
  svc->current_superframe = 0;
  svc->non_reference_frame = 0;
  svc->skip_enhancement_layer = 0;

  for (i = 0; i < REF_FRAMES; ++i) svc->ref_frame_index[i] = -1;
  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    svc->ext_frame_flags[sl] = 0;
    svc->ext_lst_fb_idx[sl] = 0;
    svc->ext_gld_fb_idx[sl] = 1;
    svc->ext_alt_fb_idx[sl] = 2;
    svc->downsample_filter_type[sl] = BILINEAR;
    svc->downsample_filter_phase[sl] = 8;  // Set to 8 for averaging filter.
  }

  if (cpi->oxcf.error_resilient_mode == 0 && cpi->oxcf.pass == 2) {
    if (vpx_realloc_frame_buffer(&cpi->svc.empty_frame.img, SMALL_FRAME_WIDTH,
                                 SMALL_FRAME_HEIGHT, cpi->common.subsampling_x,
                                 cpi->common.subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                 cpi->common.use_highbitdepth,
#endif
                                 VP9_ENC_BORDER_IN_PIXELS,
                                 cpi->common.byte_alignment, NULL, NULL, NULL))
      vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                         "Failed to allocate empty frame for multiple frame "
                         "contexts");

    memset(cpi->svc.empty_frame.img.buffer_alloc, 0x80,
           cpi->svc.empty_frame.img.buffer_alloc_sz);
  }

  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, oxcf->ts_number_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      int i;
      lc->current_video_frame_in_layer = 0;
      lc->layer_size = 0;
      lc->frames_from_key_frame = 0;
      lc->last_frame_type = FRAME_TYPES;
      lrc->ni_av_qi = oxcf->worst_allowed_q;
      lrc->total_actual_bits = 0;
      lrc->total_target_vs_actual = 0;
      lrc->ni_tot_qi = 0;
      lrc->tot_q = 0.0;
      lrc->avg_q = 0.0;
      lrc->ni_frames = 0;
      lrc->decimation_count = 0;
      lrc->decimation_factor = 0;

      for (i = 0; i < RATE_FACTOR_LEVELS; ++i) {
        lrc->rate_correction_factors[i] = 1.0;
      }

      if (cpi->oxcf.rc_mode == VPX_CBR) {
        lc->target_bandwidth = oxcf->layer_target_bitrate[layer];
        lrc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;
        lrc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
        lrc->avg_frame_qindex[KEY_FRAME] = oxcf->worst_allowed_q;
      } else {
        lc->target_bandwidth = oxcf->layer_target_bitrate[layer];
        lrc->last_q[KEY_FRAME] = oxcf->best_allowed_q;
        lrc->last_q[INTER_FRAME] = oxcf->best_allowed_q;
        lrc->avg_frame_qindex[KEY_FRAME] =
            (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
        lrc->avg_frame_qindex[INTER_FRAME] =
            (oxcf->worst_allowed_q + oxcf->best_allowed_q) / 2;
        if (oxcf->ss_enable_auto_arf[sl])
          lc->alt_ref_idx = alt_ref_idx++;
        else
          lc->alt_ref_idx = INVALID_IDX;
        lc->gold_ref_idx = INVALID_IDX;
      }

      lrc->buffer_level =
          oxcf->starting_buffer_level_ms * lc->target_bandwidth / 1000;
      lrc->bits_off_target = lrc->buffer_level;

      // Initialize the cyclic refresh parameters. If spatial layers are used
      // (i.e., ss_number_layers > 1), these need to be updated per spatial
      // layer.
      // Cyclic refresh is only applied on base temporal layer.
      if (oxcf->ss_number_layers > 1 && tl == 0) {
        size_t last_coded_q_map_size;
        size_t consec_zero_mv_size;
        VP9_COMMON *const cm = &cpi->common;
        lc->sb_index = 0;
        CHECK_MEM_ERROR(cm, lc->map,
                        vpx_malloc(mi_rows * mi_cols * sizeof(*lc->map)));
        memset(lc->map, 0, mi_rows * mi_cols);
        last_coded_q_map_size =
            mi_rows * mi_cols * sizeof(*lc->last_coded_q_map);
        CHECK_MEM_ERROR(cm, lc->last_coded_q_map,
                        vpx_malloc(last_coded_q_map_size));
        assert(MAXQ <= 255);
        memset(lc->last_coded_q_map, MAXQ, last_coded_q_map_size);
        consec_zero_mv_size = mi_rows * mi_cols * sizeof(*lc->consec_zero_mv);
        CHECK_MEM_ERROR(cm, lc->consec_zero_mv,
                        vpx_malloc(consec_zero_mv_size));
        memset(lc->consec_zero_mv, 0, consec_zero_mv_size);
      }
    }
  }

  // Still have extra buffer for base layer golden frame
  if (!(svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) &&
      alt_ref_idx < REF_FRAMES)
    svc->layer_context[0].gold_ref_idx = alt_ref_idx;
}

// Update the layer context from a change_config() call.
void vp9_update_layer_context_change_config(VP9_COMP *const cpi,
                                            const int target_bandwidth) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const RATE_CONTROL *const rc = &cpi->rc;
  int sl, tl, layer = 0, spatial_layer_target;
  float bitrate_alloc = 1.0;

  cpi->svc.temporal_layering_mode = oxcf->temporal_layering_mode;

  if (svc->temporal_layering_mode != VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING) {
    for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
      for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
        layer = LAYER_IDS_TO_IDX(sl, tl, oxcf->ts_number_layers);
        svc->layer_context[layer].target_bandwidth =
            oxcf->layer_target_bitrate[layer];
      }

      layer = LAYER_IDS_TO_IDX(
          sl,
          ((oxcf->ts_number_layers - 1) < 0 ? 0 : (oxcf->ts_number_layers - 1)),
          oxcf->ts_number_layers);
      spatial_layer_target = svc->layer_context[layer].target_bandwidth =
          oxcf->layer_target_bitrate[layer];

      for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
        LAYER_CONTEXT *const lc =
            &svc->layer_context[sl * oxcf->ts_number_layers + tl];
        RATE_CONTROL *const lrc = &lc->rc;

        lc->spatial_layer_target_bandwidth = spatial_layer_target;
        bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
        lrc->starting_buffer_level =
            (int64_t)(rc->starting_buffer_level * bitrate_alloc);
        lrc->optimal_buffer_level =
            (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
        lrc->maximum_buffer_size =
            (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
        lrc->bits_off_target =
            VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
        lrc->buffer_level = VPXMIN(lrc->buffer_level, lrc->maximum_buffer_size);
        lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[tl];
        lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
        lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
        lrc->worst_quality = rc->worst_quality;
        lrc->best_quality = rc->best_quality;
      }
    }
  } else {
    int layer_end;

    if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
      layer_end = svc->number_temporal_layers;
    } else {
      layer_end = svc->number_spatial_layers;
    }

    for (layer = 0; layer < layer_end; ++layer) {
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;

      lc->target_bandwidth = oxcf->layer_target_bitrate[layer];

      bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
      // Update buffer-related quantities.
      lrc->starting_buffer_level =
          (int64_t)(rc->starting_buffer_level * bitrate_alloc);
      lrc->optimal_buffer_level =
          (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
      lrc->maximum_buffer_size =
          (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
      lrc->bits_off_target =
          VPXMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
      lrc->buffer_level = VPXMIN(lrc->buffer_level, lrc->maximum_buffer_size);
      // Update framerate-related quantities.
      if (svc->number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) {
        lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[layer];
      } else {
        lc->framerate = cpi->framerate;
      }
      lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
      lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
      // Update qp-related quantities.
      lrc->worst_quality = rc->worst_quality;
      lrc->best_quality = rc->best_quality;
    }
  }
}

static LAYER_CONTEXT *get_layer_context(VP9_COMP *const cpi) {
  if (is_one_pass_cbr_svc(cpi))
    return &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                       cpi->svc.number_temporal_layers +
                                   cpi->svc.temporal_layer_id];
  else
    return (cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR)
               ? &cpi->svc.layer_context[cpi->svc.temporal_layer_id]
               : &cpi->svc.layer_context[cpi->svc.spatial_layer_id];
}

void vp9_update_temporal_layer_framerate(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;
  // Index into spatial+temporal arrays.
  const int st_idx = svc->spatial_layer_id * svc->number_temporal_layers +
                     svc->temporal_layer_id;
  const int tl = svc->temporal_layer_id;

  lc->framerate = cpi->framerate / oxcf->ts_rate_decimator[tl];
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (tl == 0) {
    lc->avg_frame_size = lrc->avg_frame_bandwidth;
  } else {
    const double prev_layer_framerate =
        cpi->framerate / oxcf->ts_rate_decimator[tl - 1];
    const int prev_layer_target_bandwidth =
        oxcf->layer_target_bitrate[st_idx - 1];
    lc->avg_frame_size =
        (int)((lc->target_bandwidth - prev_layer_target_bandwidth) /
              (lc->framerate - prev_layer_framerate));
  }
}

void vp9_update_spatial_layer_framerate(VP9_COMP *const cpi, double framerate) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;

  lc->framerate = framerate;
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->min_frame_bandwidth =
      (int)(lrc->avg_frame_bandwidth * oxcf->two_pass_vbrmin_section / 100);
  lrc->max_frame_bandwidth = (int)(((int64_t)lrc->avg_frame_bandwidth *
                                    oxcf->two_pass_vbrmax_section) /
                                   100);
  vp9_rc_set_gf_interval_range(cpi, lrc);
}

void vp9_restore_layer_context(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;

  cpi->rc = lc->rc;
  cpi->twopass = lc->twopass;
  cpi->oxcf.target_bandwidth = lc->target_bandwidth;
  cpi->alt_ref_source = lc->alt_ref_source;
  // Check if it is one_pass_cbr_svc mode and lc->speed > 0 (real-time mode
  // does not use speed = 0).
  if (is_one_pass_cbr_svc(cpi) && lc->speed > 0) {
    cpi->oxcf.speed = lc->speed;
  }
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  if (cpi->svc.number_temporal_layers > 1 ||
      (cpi->svc.number_spatial_layers > 1 && !is_two_pass_svc(cpi))) {
    cpi->rc.frames_since_key = old_frame_since_key;
    cpi->rc.frames_to_key = old_frame_to_key;
  }

  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->svc.number_spatial_layers > 1 && cpi->svc.temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    signed char *temp = cr->map;
    uint8_t *temp2 = cr->last_coded_q_map;
    uint8_t *temp3 = cpi->consec_zero_mv;
    cr->map = lc->map;
    lc->map = temp;
    cr->last_coded_q_map = lc->last_coded_q_map;
    lc->last_coded_q_map = temp2;
    cpi->consec_zero_mv = lc->consec_zero_mv;
    lc->consec_zero_mv = temp3;
    cr->sb_index = lc->sb_index;
  }
}

void vp9_save_layer_context(VP9_COMP *const cpi) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);

  lc->rc = cpi->rc;
  lc->twopass = cpi->twopass;
  lc->target_bandwidth = (int)oxcf->target_bandwidth;
  lc->alt_ref_source = cpi->alt_ref_source;

  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->svc.number_spatial_layers > 1 && cpi->svc.temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    signed char *temp = lc->map;
    uint8_t *temp2 = lc->last_coded_q_map;
    uint8_t *temp3 = lc->consec_zero_mv;
    lc->map = cr->map;
    cr->map = temp;
    lc->last_coded_q_map = cr->last_coded_q_map;
    cr->last_coded_q_map = temp2;
    lc->consec_zero_mv = cpi->consec_zero_mv;
    cpi->consec_zero_mv = temp3;
    lc->sb_index = cr->sb_index;
  }
}

#if !CONFIG_REALTIME_ONLY
void vp9_init_second_pass_spatial_svc(VP9_COMP *cpi) {
  SVC *const svc = &cpi->svc;
  int i;

  for (i = 0; i < svc->number_spatial_layers; ++i) {
    TWO_PASS *const twopass = &svc->layer_context[i].twopass;

    svc->spatial_layer_id = i;
    vp9_init_second_pass(cpi);

    twopass->total_stats.spatial_layer_id = i;
    twopass->total_left_stats.spatial_layer_id = i;
  }
  svc->spatial_layer_id = 0;
}
#endif  // !CONFIG_REALTIME_ONLY

void vp9_inc_frame_in_layer(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc =
      &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                              cpi->svc.number_temporal_layers];
  ++lc->current_video_frame_in_layer;
  ++lc->frames_from_key_frame;
  if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)
    ++cpi->svc.current_superframe;
}

int vp9_is_upper_layer_key_frame(const VP9_COMP *const cpi) {
  return is_two_pass_svc(cpi) && cpi->svc.spatial_layer_id > 0 &&
         cpi->svc
             .layer_context[cpi->svc.spatial_layer_id *
                                cpi->svc.number_temporal_layers +
                            cpi->svc.temporal_layer_id]
             .is_key_frame;
}

void get_layer_resolution(const int width_org, const int height_org,
                          const int num, const int den, int *width_out,
                          int *height_out) {
  int w, h;

  if (width_out == NULL || height_out == NULL || den == 0) return;

  w = width_org * num / den;
  h = height_org * num / den;

  // make height and width even to make chrome player happy
  w += w % 2;
  h += h % 2;

  *width_out = w;
  *height_out = h;
}

// The function sets proper ref_frame_flags, buffer indices, and buffer update
// variables for temporal layering mode 3 - that does 0-2-1-2 temporal layering
// scheme.
static void set_flags_and_fb_idx_for_temporal_mode3(VP9_COMP *const cpi) {
  int frame_num_within_temporal_struct = 0;
  int spatial_id, temporal_id;
  spatial_id = cpi->svc.spatial_layer_id = cpi->svc.spatial_layer_to_encode;
  frame_num_within_temporal_struct =
      cpi->svc
          .layer_context[cpi->svc.spatial_layer_id *
                         cpi->svc.number_temporal_layers]
          .current_video_frame_in_layer %
      4;
  temporal_id = cpi->svc.temporal_layer_id =
      (frame_num_within_temporal_struct & 1)
          ? 2
          : (frame_num_within_temporal_struct >> 1);
  cpi->ext_refresh_last_frame = cpi->ext_refresh_golden_frame =
      cpi->ext_refresh_alt_ref_frame = 0;
  if (!temporal_id) {
    cpi->ext_refresh_frame_flags_pending = 1;
    cpi->ext_refresh_last_frame = 1;
    if (!spatial_id) {
      cpi->ref_frame_flags = VP9_LAST_FLAG;
    } else if (cpi->svc.layer_context[temporal_id].is_key_frame) {
      // base layer is a key frame.
      cpi->ref_frame_flags = VP9_LAST_FLAG;
      cpi->ext_refresh_last_frame = 0;
      cpi->ext_refresh_golden_frame = 1;
    } else {
      cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
    }
  } else if (temporal_id == 1) {
    cpi->ext_refresh_frame_flags_pending = 1;
    cpi->ext_refresh_alt_ref_frame = 1;
    if (!spatial_id) {
      cpi->ref_frame_flags = VP9_LAST_FLAG;
    } else {
      cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
    }
  } else {
    if (frame_num_within_temporal_struct == 1) {
      // the first tl2 picture
      if (spatial_id == cpi->svc.number_spatial_layers - 1) {  // top layer
        cpi->ext_refresh_frame_flags_pending = 1;
        if (!spatial_id)
          cpi->ref_frame_flags = VP9_LAST_FLAG;
        else
          cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
      } else if (!spatial_id) {
        cpi->ext_refresh_frame_flags_pending = 1;
        cpi->ext_refresh_alt_ref_frame = 1;
        cpi->ref_frame_flags = VP9_LAST_FLAG;
      } else if (spatial_id < cpi->svc.number_spatial_layers - 1) {
        cpi->ext_refresh_frame_flags_pending = 1;
        cpi->ext_refresh_alt_ref_frame = 1;
        cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
      }
    } else {
      //  The second tl2 picture
      if (spatial_id == cpi->svc.number_spatial_layers - 1) {  // top layer
        cpi->ext_refresh_frame_flags_pending = 1;
        if (!spatial_id)
          cpi->ref_frame_flags = VP9_LAST_FLAG;
        else
          cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
      } else if (!spatial_id) {
        cpi->ext_refresh_frame_flags_pending = 1;
        cpi->ref_frame_flags = VP9_LAST_FLAG;
        cpi->ext_refresh_alt_ref_frame = 1;
      } else {  // top layer
        cpi->ext_refresh_frame_flags_pending = 1;
        cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
        cpi->ext_refresh_alt_ref_frame = 1;
      }
    }
  }
  if (temporal_id == 0) {
    cpi->lst_fb_idx = spatial_id;
    if (spatial_id) {
      if (cpi->svc.layer_context[temporal_id].is_key_frame) {
        cpi->lst_fb_idx = spatial_id - 1;
        cpi->gld_fb_idx = spatial_id;
      } else {
        cpi->gld_fb_idx = spatial_id - 1;
      }
    } else {
      cpi->gld_fb_idx = 0;
    }
    cpi->alt_fb_idx = 0;
  } else if (temporal_id == 1) {
    cpi->lst_fb_idx = spatial_id;
    cpi->gld_fb_idx = cpi->svc.number_spatial_layers + spatial_id - 1;
    cpi->alt_fb_idx = cpi->svc.number_spatial_layers + spatial_id;
  } else if (frame_num_within_temporal_struct == 1) {
    cpi->lst_fb_idx = spatial_id;
    cpi->gld_fb_idx = cpi->svc.number_spatial_layers + spatial_id - 1;
    cpi->alt_fb_idx = cpi->svc.number_spatial_layers + spatial_id;
  } else {
    cpi->lst_fb_idx = cpi->svc.number_spatial_layers + spatial_id;
    cpi->gld_fb_idx = cpi->svc.number_spatial_layers + spatial_id - 1;
    cpi->alt_fb_idx = cpi->svc.number_spatial_layers + spatial_id;
  }
}

// The function sets proper ref_frame_flags, buffer indices, and buffer update
// variables for temporal layering mode 2 - that does 0-1-0-1 temporal layering
// scheme.
static void set_flags_and_fb_idx_for_temporal_mode2(VP9_COMP *const cpi) {
  int spatial_id, temporal_id;
  spatial_id = cpi->svc.spatial_layer_id = cpi->svc.spatial_layer_to_encode;
  temporal_id = cpi->svc.temporal_layer_id =
      cpi->svc
          .layer_context[cpi->svc.spatial_layer_id *
                         cpi->svc.number_temporal_layers]
          .current_video_frame_in_layer &
      1;
  cpi->ext_refresh_last_frame = cpi->ext_refresh_golden_frame =
      cpi->ext_refresh_alt_ref_frame = 0;
  if (!temporal_id) {
    cpi->ext_refresh_frame_flags_pending = 1;
    cpi->ext_refresh_last_frame = 1;
    if (!spatial_id) {
      cpi->ref_frame_flags = VP9_LAST_FLAG;
    } else if (cpi->svc.layer_context[temporal_id].is_key_frame) {
      // base layer is a key frame.
      cpi->ref_frame_flags = VP9_LAST_FLAG;
      cpi->ext_refresh_last_frame = 0;
      cpi->ext_refresh_golden_frame = 1;
    } else {
      cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
    }
  } else if (temporal_id == 1) {
    cpi->ext_refresh_frame_flags_pending = 1;
    cpi->ext_refresh_alt_ref_frame = 1;
    if (!spatial_id) {
      cpi->ref_frame_flags = VP9_LAST_FLAG;
    } else {
      if (spatial_id == cpi->svc.number_spatial_layers - 1)
        cpi->ext_refresh_alt_ref_frame = 0;
      cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
    }
  }

  if (temporal_id == 0) {
    cpi->lst_fb_idx = spatial_id;
    if (spatial_id) {
      if (cpi->svc.layer_context[temporal_id].is_key_frame) {
        cpi->lst_fb_idx = spatial_id - 1;
        cpi->gld_fb_idx = spatial_id;
      } else {
        cpi->gld_fb_idx = spatial_id - 1;
      }
    } else {
      cpi->gld_fb_idx = 0;
    }
    cpi->alt_fb_idx = 0;
  } else if (temporal_id == 1) {
    cpi->lst_fb_idx = spatial_id;
    cpi->gld_fb_idx = cpi->svc.number_spatial_layers + spatial_id - 1;
    cpi->alt_fb_idx = cpi->svc.number_spatial_layers + spatial_id;
  }
}

// The function sets proper ref_frame_flags, buffer indices, and buffer update
// variables for temporal layering mode 0 - that has no temporal layering.
static void set_flags_and_fb_idx_for_temporal_mode_noLayering(
    VP9_COMP *const cpi) {
  int spatial_id;
  spatial_id = cpi->svc.spatial_layer_id = cpi->svc.spatial_layer_to_encode;
  cpi->ext_refresh_last_frame = cpi->ext_refresh_golden_frame =
      cpi->ext_refresh_alt_ref_frame = 0;
  cpi->ext_refresh_frame_flags_pending = 1;
  cpi->ext_refresh_last_frame = 1;
  if (!spatial_id) {
    cpi->ref_frame_flags = VP9_LAST_FLAG;
  } else if (cpi->svc.layer_context[0].is_key_frame) {
    cpi->ref_frame_flags = VP9_LAST_FLAG;
    cpi->ext_refresh_last_frame = 0;
    cpi->ext_refresh_golden_frame = 1;
  } else {
    cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
  }
  cpi->lst_fb_idx = spatial_id;
  if (spatial_id) {
    if (cpi->svc.layer_context[0].is_key_frame) {
      cpi->lst_fb_idx = spatial_id - 1;
      cpi->gld_fb_idx = spatial_id;
    } else {
      cpi->gld_fb_idx = spatial_id - 1;
    }
  } else {
    cpi->gld_fb_idx = 0;
  }
}

int vp9_one_pass_cbr_svc_start_layer(VP9_COMP *const cpi) {
  int width = 0, height = 0;
  LAYER_CONTEXT *lc = NULL;
  cpi->svc.skip_enhancement_layer = 0;
  if (cpi->svc.number_spatial_layers > 1) {
    cpi->svc.use_base_mv = 1;
    cpi->svc.use_partition_reuse = 1;
  }
  cpi->svc.force_zero_mode_spatial_ref = 1;
  cpi->svc.mi_stride[cpi->svc.spatial_layer_id] = cpi->common.mi_stride;

  if (cpi->svc.temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_0212) {
    set_flags_and_fb_idx_for_temporal_mode3(cpi);
  } else if (cpi->svc.temporal_layering_mode ==
             VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING) {
    set_flags_and_fb_idx_for_temporal_mode_noLayering(cpi);
  } else if (cpi->svc.temporal_layering_mode ==
             VP9E_TEMPORAL_LAYERING_MODE_0101) {
    set_flags_and_fb_idx_for_temporal_mode2(cpi);
  } else if (cpi->svc.temporal_layering_mode ==
             VP9E_TEMPORAL_LAYERING_MODE_BYPASS) {
    // In the BYPASS/flexible mode, the encoder is relying on the application
    // to specify, for each spatial layer, the flags and buffer indices for the
    // layering.
    // Note that the check (cpi->ext_refresh_frame_flags_pending == 0) is
    // needed to support the case where the frame flags may be passed in via
    // vpx_codec_encode(), which can be used for the temporal-only svc case.
    // TODO(marpan): Consider adding an enc_config parameter to better handle
    // this case.
    if (cpi->ext_refresh_frame_flags_pending == 0) {
      int sl;
      cpi->svc.spatial_layer_id = cpi->svc.spatial_layer_to_encode;
      sl = cpi->svc.spatial_layer_id;
      vp9_apply_encoding_flags(cpi, cpi->svc.ext_frame_flags[sl]);
      cpi->lst_fb_idx = cpi->svc.ext_lst_fb_idx[sl];
      cpi->gld_fb_idx = cpi->svc.ext_gld_fb_idx[sl];
      cpi->alt_fb_idx = cpi->svc.ext_alt_fb_idx[sl];
    }
  }

  if (cpi->svc.spatial_layer_id == cpi->svc.first_spatial_layer_to_encode)
    cpi->svc.rc_drop_superframe = 0;

  lc = &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                   cpi->svc.number_temporal_layers +
                               cpi->svc.temporal_layer_id];

  // Setting the worst/best_quality via the encoder control: SET_SVC_PARAMETERS,
  // only for non-BYPASS mode for now.
  if (cpi->svc.temporal_layering_mode != VP9E_TEMPORAL_LAYERING_MODE_BYPASS) {
    RATE_CONTROL *const lrc = &lc->rc;
    lrc->worst_quality = vp9_quantizer_to_qindex(lc->max_q);
    lrc->best_quality = vp9_quantizer_to_qindex(lc->min_q);
  }

  get_layer_resolution(cpi->oxcf.width, cpi->oxcf.height,
                       lc->scaling_factor_num, lc->scaling_factor_den, &width,
                       &height);

  // Use Eightap_smooth for low resolutions.
  if (width * height <= 320 * 240)
    cpi->svc.downsample_filter_type[cpi->svc.spatial_layer_id] =
        EIGHTTAP_SMOOTH;
  // For scale factors > 0.75, set the phase to 0 (aligns decimated pixel
  // to source pixel).
  lc = &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                   cpi->svc.number_temporal_layers +
                               cpi->svc.temporal_layer_id];
  if (lc->scaling_factor_num > (3 * lc->scaling_factor_den) >> 2)
    cpi->svc.downsample_filter_phase[cpi->svc.spatial_layer_id] = 0;

  // The usage of use_base_mv or partition_reuse assumes down-scale of 2x2.
  // For now, turn off use of base motion vectors and partition reuse if the
  // spatial scale factors for any layers are not 2,
  // keep the case of 3 spatial layers with scale factor of 4x4 for base layer.
  // TODO(marpan): Fix this to allow for use_base_mv for scale factors != 2.
  if (cpi->svc.number_spatial_layers > 1) {
    int sl;
    for (sl = 0; sl < cpi->svc.number_spatial_layers - 1; ++sl) {
      lc = &cpi->svc.layer_context[sl * cpi->svc.number_temporal_layers +
                                   cpi->svc.temporal_layer_id];
      if ((lc->scaling_factor_num != lc->scaling_factor_den >> 1) &&
          !(lc->scaling_factor_num == lc->scaling_factor_den >> 2 && sl == 0 &&
            cpi->svc.number_spatial_layers == 3)) {
        cpi->svc.use_base_mv = 0;
        cpi->svc.use_partition_reuse = 0;
        break;
      }
    }
  }

  cpi->svc.non_reference_frame = 0;
  if (cpi->common.frame_type != KEY_FRAME && !cpi->ext_refresh_last_frame &&
      !cpi->ext_refresh_golden_frame && !cpi->ext_refresh_alt_ref_frame) {
    cpi->svc.non_reference_frame = 1;
  }

  if (vp9_set_size_literal(cpi, width, height) != 0)
    return VPX_CODEC_INVALID_PARAM;

  return 0;
}

#if CONFIG_SPATIAL_SVC
#define SMALL_FRAME_FB_IDX 7

int vp9_svc_start_frame(VP9_COMP *const cpi) {
  int width = 0, height = 0;
  LAYER_CONTEXT *lc;
  struct lookahead_entry *buf;
  int count = 1 << (cpi->svc.number_temporal_layers - 1);

  cpi->svc.spatial_layer_id = cpi->svc.spatial_layer_to_encode;
  lc = &cpi->svc.layer_context[cpi->svc.spatial_layer_id];

  cpi->svc.temporal_layer_id = 0;
  while ((lc->current_video_frame_in_layer % count) != 0) {
    ++cpi->svc.temporal_layer_id;
    count >>= 1;
  }

  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  cpi->lst_fb_idx = cpi->svc.spatial_layer_id;

  if (cpi->svc.spatial_layer_id == 0)
    cpi->gld_fb_idx =
        (lc->gold_ref_idx >= 0) ? lc->gold_ref_idx : cpi->lst_fb_idx;
  else
    cpi->gld_fb_idx = cpi->svc.spatial_layer_id - 1;

  if (lc->current_video_frame_in_layer == 0) {
    if (cpi->svc.spatial_layer_id >= 2) {
      cpi->alt_fb_idx = cpi->svc.spatial_layer_id - 2;
    } else {
      cpi->alt_fb_idx = cpi->lst_fb_idx;
      cpi->ref_frame_flags &= (~VP9_LAST_FLAG & ~VP9_ALT_FLAG);
    }
  } else {
    if (cpi->oxcf.ss_enable_auto_arf[cpi->svc.spatial_layer_id]) {
      cpi->alt_fb_idx = lc->alt_ref_idx;
      if (!lc->has_alt_frame) cpi->ref_frame_flags &= (~VP9_ALT_FLAG);
    } else {
      // Find a proper alt_fb_idx for layers that don't have alt ref frame
      if (cpi->svc.spatial_layer_id == 0) {
        cpi->alt_fb_idx = cpi->lst_fb_idx;
      } else {
        LAYER_CONTEXT *lc_lower =
            &cpi->svc.layer_context[cpi->svc.spatial_layer_id - 1];

        if (cpi->oxcf.ss_enable_auto_arf[cpi->svc.spatial_layer_id - 1] &&
            lc_lower->alt_ref_source != NULL)
          cpi->alt_fb_idx = lc_lower->alt_ref_idx;
        else if (cpi->svc.spatial_layer_id >= 2)
          cpi->alt_fb_idx = cpi->svc.spatial_layer_id - 2;
        else
          cpi->alt_fb_idx = cpi->lst_fb_idx;
      }
    }
  }

  get_layer_resolution(cpi->oxcf.width, cpi->oxcf.height,
                       lc->scaling_factor_num, lc->scaling_factor_den, &width,
                       &height);

  // Workaround for multiple frame contexts. In some frames we can't use prev_mi
  // since its previous frame could be changed during decoding time. The idea is
  // we put a empty invisible frame in front of them, then we will not use
  // prev_mi when encoding these frames.

  buf = vp9_lookahead_peek(cpi->lookahead, 0);
  if (cpi->oxcf.error_resilient_mode == 0 && cpi->oxcf.pass == 2 &&
      cpi->svc.encode_empty_frame_state == NEED_TO_ENCODE &&
      lc->rc.frames_to_key != 0 &&
      !(buf != NULL && (buf->flags & VPX_EFLAG_FORCE_KF))) {
    if ((cpi->svc.number_temporal_layers > 1 &&
         cpi->svc.temporal_layer_id < cpi->svc.number_temporal_layers - 1) ||
        (cpi->svc.number_spatial_layers > 1 &&
         cpi->svc.spatial_layer_id == 0)) {
      struct lookahead_entry *buf = vp9_lookahead_peek(cpi->lookahead, 0);

      if (buf != NULL) {
        cpi->svc.empty_frame.ts_start = buf->ts_start;
        cpi->svc.empty_frame.ts_end = buf->ts_end;
        cpi->svc.encode_empty_frame_state = ENCODING;
        cpi->common.show_frame = 0;
        cpi->ref_frame_flags = 0;
        cpi->common.frame_type = INTER_FRAME;
        cpi->lst_fb_idx = cpi->gld_fb_idx = cpi->alt_fb_idx =
            SMALL_FRAME_FB_IDX;

        if (cpi->svc.encode_intra_empty_frame != 0) cpi->common.intra_only = 1;

        width = SMALL_FRAME_WIDTH;
        height = SMALL_FRAME_HEIGHT;
      }
    }
  }

  cpi->oxcf.worst_allowed_q = vp9_quantizer_to_qindex(lc->max_q);
  cpi->oxcf.best_allowed_q = vp9_quantizer_to_qindex(lc->min_q);

  vp9_change_config(cpi, &cpi->oxcf);

  if (vp9_set_size_literal(cpi, width, height) != 0)
    return VPX_CODEC_INVALID_PARAM;

  vp9_set_high_precision_mv(cpi, 1);

  cpi->alt_ref_source = get_layer_context(cpi)->alt_ref_source;

  return 0;
}

#undef SMALL_FRAME_FB_IDX
#endif  // CONFIG_SPATIAL_SVC

struct lookahead_entry *vp9_svc_lookahead_pop(VP9_COMP *const cpi,
                                              struct lookahead_ctx *ctx,
                                              int drain) {
  struct lookahead_entry *buf = NULL;
  if (ctx->sz && (drain || ctx->sz == ctx->max_sz - MAX_PRE_FRAMES)) {
    buf = vp9_lookahead_peek(ctx, 0);
    if (buf != NULL) {
      // Only remove the buffer when pop the highest layer.
      if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1) {
        vp9_lookahead_pop(ctx, drain);
      }
    }
  }
  return buf;
}

void vp9_free_svc_cyclic_refresh(VP9_COMP *const cpi) {
  int sl, tl;
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
    for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, oxcf->ts_number_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      if (lc->map) vpx_free(lc->map);
      if (lc->last_coded_q_map) vpx_free(lc->last_coded_q_map);
      if (lc->consec_zero_mv) vpx_free(lc->consec_zero_mv);
    }
  }
}

// Reset on key frame: reset counters, references and buffer updates.
void vp9_svc_reset_key_frame(VP9_COMP *const cpi) {
  int sl, tl;
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  for (sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (tl = 0; tl < svc->number_temporal_layers; ++tl) {
      lc = &cpi->svc.layer_context[sl * svc->number_temporal_layers + tl];
      lc->current_video_frame_in_layer = 0;
      lc->frames_from_key_frame = 0;
    }
  }
  if (svc->temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_0212) {
    set_flags_and_fb_idx_for_temporal_mode3(cpi);
  } else if (svc->temporal_layering_mode ==
             VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING) {
    set_flags_and_fb_idx_for_temporal_mode_noLayering(cpi);
  } else if (svc->temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_0101) {
    set_flags_and_fb_idx_for_temporal_mode2(cpi);
  }
  vp9_update_temporal_layer_framerate(cpi);
  vp9_restore_layer_context(cpi);
}

void vp9_svc_check_reset_layer_rc_flag(VP9_COMP *const cpi) {
  SVC *svc = &cpi->svc;
  int sl, tl;
  for (sl = 0; sl < svc->number_spatial_layers; ++sl) {
    // Check for reset based on avg_frame_bandwidth for spatial layer sl.
    int layer = LAYER_IDS_TO_IDX(sl, svc->number_temporal_layers - 1,
                                 svc->number_temporal_layers);
    LAYER_CONTEXT *lc = &svc->layer_context[layer];
    RATE_CONTROL *lrc = &lc->rc;
    if (lrc->avg_frame_bandwidth > (3 * lrc->last_avg_frame_bandwidth >> 1) ||
        lrc->avg_frame_bandwidth < (lrc->last_avg_frame_bandwidth >> 1)) {
      // Reset for all temporal layers with spatial layer sl.
      for (tl = 0; tl < svc->number_temporal_layers; ++tl) {
        int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
        LAYER_CONTEXT *lc = &svc->layer_context[layer];
        RATE_CONTROL *lrc = &lc->rc;
        lrc->rc_1_frame = 0;
        lrc->rc_2_frame = 0;
        lrc->bits_off_target = lrc->optimal_buffer_level;
        lrc->buffer_level = lrc->optimal_buffer_level;
      }
    }
  }
}
