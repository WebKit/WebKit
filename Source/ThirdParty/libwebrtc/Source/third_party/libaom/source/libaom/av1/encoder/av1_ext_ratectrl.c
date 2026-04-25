/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom/aom_ext_ratectrl.h"
#include "av1/encoder/av1_ext_ratectrl.h"

aom_codec_err_t av1_extrc_init(AOM_EXT_RATECTRL *ext_ratectrl) {
  if (ext_ratectrl == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  av1_zero(*ext_ratectrl);
  return AOM_CODEC_OK;
}

aom_codec_err_t av1_extrc_create(aom_rc_funcs_t funcs,
                                 aom_rc_config_t ratectrl_config,
                                 AOM_EXT_RATECTRL *ext_ratectrl) {
  aom_rc_status_t rc_status;
  aom_rc_firstpass_stats_t *rc_firstpass_stats;
  if (ext_ratectrl == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  av1_extrc_delete(ext_ratectrl);
  ext_ratectrl->funcs = funcs;
  ext_ratectrl->ratectrl_config = ratectrl_config;
  rc_status = ext_ratectrl->funcs.create_model(ext_ratectrl->funcs.priv,
                                               &ext_ratectrl->ratectrl_config,
                                               &ext_ratectrl->model);
  if (rc_status == AOM_RC_ERROR) {
    return AOM_CODEC_ERROR;
  }
  rc_firstpass_stats = &ext_ratectrl->rc_firstpass_stats;
  rc_firstpass_stats->num_frames = ratectrl_config.show_frame_count;
  rc_firstpass_stats->frame_stats =
      aom_malloc(sizeof(*rc_firstpass_stats->frame_stats) *
                 rc_firstpass_stats->num_frames);
  if (rc_firstpass_stats->frame_stats == NULL) {
    return AOM_CODEC_MEM_ERROR;
  }

  ext_ratectrl->ready = 1;
  return AOM_CODEC_OK;
}

static void gen_rc_firstpass_stats(const FIRSTPASS_STATS *stats,
                                   aom_rc_frame_stats_t *rc_frame_stats) {
  rc_frame_stats->frame = stats->frame;
  rc_frame_stats->weight = stats->weight;
  rc_frame_stats->intra_error = stats->intra_error;
  rc_frame_stats->coded_error = stats->coded_error;
  rc_frame_stats->sr_coded_error = stats->sr_coded_error;
  rc_frame_stats->frame_avg_wavelet_energy = stats->frame_avg_wavelet_energy;
  rc_frame_stats->pcnt_inter = stats->pcnt_inter;
  rc_frame_stats->pcnt_motion = stats->pcnt_motion;
  rc_frame_stats->pcnt_second_ref = stats->pcnt_second_ref;
  rc_frame_stats->pcnt_neutral = stats->pcnt_neutral;
  rc_frame_stats->intra_skip_pct = stats->intra_skip_pct;
  rc_frame_stats->inactive_zone_rows = stats->inactive_zone_rows;
  rc_frame_stats->inactive_zone_cols = stats->inactive_zone_cols;
  rc_frame_stats->MVr = stats->MVr;
  rc_frame_stats->mvr_abs = stats->mvr_abs;
  rc_frame_stats->MVc = stats->MVc;
  rc_frame_stats->mvc_abs = stats->mvc_abs;
  rc_frame_stats->MVrv = stats->MVrv;
  rc_frame_stats->MVcv = stats->MVcv;
  rc_frame_stats->mv_in_out_count = stats->mv_in_out_count;
  rc_frame_stats->duration = stats->duration;
  rc_frame_stats->count = stats->count;
  rc_frame_stats->new_mv_count = stats->new_mv_count;
  rc_frame_stats->raw_error_stdev = stats->raw_error_stdev;
  rc_frame_stats->is_flash = stats->is_flash;
  rc_frame_stats->noise_var = stats->noise_var;
  rc_frame_stats->cor_coeff = stats->cor_coeff;
  rc_frame_stats->log_intra_error = stats->log_intra_error;
  rc_frame_stats->log_coded_error = stats->log_coded_error;
}

aom_codec_err_t av1_extrc_send_firstpass_stats(
    AOM_EXT_RATECTRL *ext_ratectrl, const FIRSTPASS_INFO *first_pass_info) {
  assert(first_pass_info != NULL);
  assert(ext_ratectrl != NULL);
  if (ext_ratectrl->ready) {
    aom_rc_status_t rc_status;
    aom_rc_firstpass_stats_t *rc_firstpass_stats =
        &ext_ratectrl->rc_firstpass_stats;
    assert(rc_firstpass_stats->num_frames == first_pass_info->stats_buf_size);
    for (int i = 0; i < rc_firstpass_stats->num_frames; ++i) {
      gen_rc_firstpass_stats(&first_pass_info->stats_buf[i],
                             &rc_firstpass_stats->frame_stats[i]);
    }
    rc_status = ext_ratectrl->funcs.send_firstpass_stats(ext_ratectrl->model,
                                                         rc_firstpass_stats);
    if (rc_status == AOM_RC_ERROR) {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_OK;
}

aom_codec_err_t av1_extrc_delete(AOM_EXT_RATECTRL *ext_ratectrl) {
  if (ext_ratectrl == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (ext_ratectrl->ready) {
    aom_rc_status_t rc_status =
        ext_ratectrl->funcs.delete_model(ext_ratectrl->model);
    if (rc_status == AOM_RC_ERROR) {
      return AOM_CODEC_ERROR;
    }
    aom_free(ext_ratectrl->rc_firstpass_stats.frame_stats);
  }
  return av1_extrc_init(ext_ratectrl);
}
