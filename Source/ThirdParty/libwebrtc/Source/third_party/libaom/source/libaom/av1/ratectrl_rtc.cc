/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/ratectrl_rtc.h"

#include <memory>
#include <new>

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"
#include "aom_mem/aom_mem.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_utils.h"
#include "av1/encoder/pickcdef.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rc_utils.h"
#include "av1/encoder/svc_layercontext.h"

namespace aom {

AV1RateControlRtcConfig::AV1RateControlRtcConfig() {
  width = 1280;
  height = 720;
  max_quantizer = 63;
  min_quantizer = 2;
  target_bandwidth = 1000;
  buf_initial_sz = 600;
  buf_optimal_sz = 600;
  buf_sz = 1000;
  undershoot_pct = overshoot_pct = 50;
  max_intra_bitrate_pct = 50;
  max_inter_bitrate_pct = 0;
  framerate = 30.0;
  ss_number_layers = 1;
  ts_number_layers = 1;
  aq_mode = 0;
  layer_target_bitrate[0] = static_cast<int>(target_bandwidth);
  ts_rate_decimator[0] = 1;
  av1_zero(max_quantizers);
  av1_zero(min_quantizers);
  av1_zero(scaling_factor_den);
  av1_zero(scaling_factor_num);
  av1_zero(layer_target_bitrate);
  av1_zero(ts_rate_decimator);
  scaling_factor_num[0] = 1;
  scaling_factor_den[0] = 1;
  max_quantizers[0] = max_quantizer;
  min_quantizers[0] = min_quantizer;
}

std::unique_ptr<AV1RateControlRTC> AV1RateControlRTC::Create(
    const AV1RateControlRtcConfig &cfg) {
  std::unique_ptr<AV1RateControlRTC> rc_api(new (std::nothrow)
                                                AV1RateControlRTC());
  if (!rc_api) return nullptr;
  rc_api->cpi_ = static_cast<AV1_COMP *>(aom_memalign(32, sizeof(*cpi_)));
  if (!rc_api->cpi_) return nullptr;
  av1_zero(*rc_api->cpi_);
  rc_api->cpi_->ppi =
      static_cast<AV1_PRIMARY *>(aom_memalign(32, sizeof(AV1_PRIMARY)));
  if (!rc_api->cpi_->ppi) return nullptr;
  av1_zero(*rc_api->cpi_->ppi);
  rc_api->cpi_->common.seq_params = &rc_api->cpi_->ppi->seq_params;
  av1_zero(*rc_api->cpi_->common.seq_params);
  if (!rc_api->InitRateControl(cfg)) return nullptr;
  if (cfg.aq_mode) {
    AV1_COMP *const cpi = rc_api->cpi_;
    cpi->enc_seg.map = static_cast<uint8_t *>(aom_calloc(
        cpi->common.mi_params.mi_rows * cpi->common.mi_params.mi_cols,
        sizeof(*cpi->enc_seg.map)));
    if (!cpi->enc_seg.map) return nullptr;
    cpi->cyclic_refresh = av1_cyclic_refresh_alloc(
        cpi->common.mi_params.mi_rows, cpi->common.mi_params.mi_cols);
    if (!cpi->cyclic_refresh) return nullptr;
  }
  return rc_api;
}

AV1RateControlRTC::~AV1RateControlRTC() {
  if (cpi_) {
    if (cpi_->svc.number_spatial_layers > 1 ||
        cpi_->svc.number_temporal_layers > 1) {
      for (int sl = 0; sl < cpi_->svc.number_spatial_layers; sl++) {
        for (int tl = 0; tl < cpi_->svc.number_temporal_layers; tl++) {
          int layer =
              LAYER_IDS_TO_IDX(sl, tl, cpi_->svc.number_temporal_layers);
          LAYER_CONTEXT *const lc = &cpi_->svc.layer_context[layer];
          aom_free(lc->map);
        }
      }
    }
    aom_free(cpi_->svc.layer_context);
    cpi_->svc.layer_context = nullptr;

    if (cpi_->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) {
      aom_free(cpi_->enc_seg.map);
      cpi_->enc_seg.map = nullptr;
      av1_cyclic_refresh_free(cpi_->cyclic_refresh);
    }
    aom_free(cpi_->ppi);
    aom_free(cpi_);
  }
}

bool AV1RateControlRTC::InitRateControl(const AV1RateControlRtcConfig &rc_cfg) {
  AV1_COMMON *cm = &cpi_->common;
  AV1EncoderConfig *oxcf = &cpi_->oxcf;
  RATE_CONTROL *const rc = &cpi_->rc;
  cm->seq_params->profile = PROFILE_0;
  cm->seq_params->bit_depth = AOM_BITS_8;
  cm->show_frame = 1;
  oxcf->profile = cm->seq_params->profile;
  oxcf->mode = REALTIME;
  oxcf->rc_cfg.mode = AOM_CBR;
  oxcf->pass = AOM_RC_ONE_PASS;
  oxcf->q_cfg.aq_mode = rc_cfg.aq_mode ? CYCLIC_REFRESH_AQ : NO_AQ;
  oxcf->tune_cfg.content = AOM_CONTENT_DEFAULT;
  oxcf->rc_cfg.drop_frames_water_mark = 0;
  oxcf->tool_cfg.bit_depth = AOM_BITS_8;
  oxcf->tool_cfg.superblock_size = AOM_SUPERBLOCK_SIZE_DYNAMIC;
  oxcf->algo_cfg.loopfilter_control = LOOPFILTER_ALL;
  cm->current_frame.frame_number = 0;
  cpi_->ppi->p_rc.kf_boost = DEFAULT_KF_BOOST_RT;
  for (auto &lvl_idx : oxcf->target_seq_level_idx) lvl_idx = SEQ_LEVEL_MAX;

  memcpy(cpi_->ppi->level_params.target_seq_level_idx,
         oxcf->target_seq_level_idx, sizeof(oxcf->target_seq_level_idx));
  if (!UpdateRateControl(rc_cfg)) return false;
  set_sb_size(cm->seq_params,
              av1_select_sb_size(oxcf, cm->width, cm->height,
                                 cpi_->svc.number_spatial_layers));
  cpi_->ppi->use_svc = cpi_->svc.number_spatial_layers > 1 ||
                       cpi_->svc.number_temporal_layers > 1;
  av1_primary_rc_init(oxcf, &cpi_->ppi->p_rc);
  rc->rc_1_frame = 0;
  rc->rc_2_frame = 0;
  av1_rc_init_minq_luts();
  av1_rc_init(oxcf, rc);
  // Enable external rate control.
  cpi_->rc.rtc_external_ratectrl = 1;
  cpi_->sf.rt_sf.use_nonrd_pick_mode = 1;
  return true;
}

bool AV1RateControlRTC::UpdateRateControl(
    const AV1RateControlRtcConfig &rc_cfg) {
  if (rc_cfg.ss_number_layers < 1 ||
      rc_cfg.ss_number_layers > AOM_MAX_SS_LAYERS ||
      rc_cfg.ts_number_layers < 1 ||
      rc_cfg.ts_number_layers > AOM_MAX_TS_LAYERS) {
    return false;
  }
  const int num_layers = rc_cfg.ss_number_layers * rc_cfg.ts_number_layers;
  if (num_layers > 1 && !av1_alloc_layer_context(cpi_, num_layers)) {
    return false;
  }
  AV1_COMMON *cm = &cpi_->common;
  AV1EncoderConfig *oxcf = &cpi_->oxcf;
  RATE_CONTROL *const rc = &cpi_->rc;
  initial_width_ = rc_cfg.width;
  initial_height_ = rc_cfg.height;
  cm->width = rc_cfg.width;
  cm->height = rc_cfg.height;
  oxcf->frm_dim_cfg.width = rc_cfg.width;
  oxcf->frm_dim_cfg.height = rc_cfg.height;
  oxcf->rc_cfg.worst_allowed_q = av1_quantizer_to_qindex(rc_cfg.max_quantizer);
  oxcf->rc_cfg.best_allowed_q = av1_quantizer_to_qindex(rc_cfg.min_quantizer);
  rc->worst_quality = oxcf->rc_cfg.worst_allowed_q;
  rc->best_quality = oxcf->rc_cfg.best_allowed_q;
  oxcf->input_cfg.init_framerate = rc_cfg.framerate;
  oxcf->rc_cfg.target_bandwidth = rc_cfg.target_bandwidth > INT64_MAX / 1000
                                      ? INT64_MAX
                                      : 1000 * rc_cfg.target_bandwidth;
  oxcf->rc_cfg.starting_buffer_level_ms = rc_cfg.buf_initial_sz;
  oxcf->rc_cfg.optimal_buffer_level_ms = rc_cfg.buf_optimal_sz;
  oxcf->rc_cfg.maximum_buffer_size_ms = rc_cfg.buf_sz;
  oxcf->rc_cfg.under_shoot_pct = rc_cfg.undershoot_pct;
  oxcf->rc_cfg.over_shoot_pct = rc_cfg.overshoot_pct;
  oxcf->rc_cfg.max_intra_bitrate_pct = rc_cfg.max_intra_bitrate_pct;
  oxcf->rc_cfg.max_inter_bitrate_pct = rc_cfg.max_inter_bitrate_pct;
  cpi_->framerate = rc_cfg.framerate;
  cpi_->svc.number_spatial_layers = rc_cfg.ss_number_layers;
  cpi_->svc.number_temporal_layers = rc_cfg.ts_number_layers;
  set_primary_rc_buffer_sizes(oxcf, cpi_->ppi);
  enc_set_mb_mi(&cm->mi_params, cm->width, cm->height, BLOCK_8X8);
  av1_new_framerate(cpi_, cpi_->framerate);
  if (cpi_->svc.number_temporal_layers > 1 ||
      cpi_->svc.number_spatial_layers > 1) {
    int64_t target_bandwidth_svc = 0;
    for (int sl = 0; sl < cpi_->svc.number_spatial_layers; ++sl) {
      for (int tl = 0; tl < cpi_->svc.number_temporal_layers; ++tl) {
        const int layer =
            LAYER_IDS_TO_IDX(sl, tl, cpi_->svc.number_temporal_layers);
        LAYER_CONTEXT *lc = &cpi_->svc.layer_context[layer];
        RATE_CONTROL *const lrc = &lc->rc;
        lc->layer_target_bitrate = 1000 * rc_cfg.layer_target_bitrate[layer];
        lc->max_q = rc_cfg.max_quantizers[layer];
        lc->min_q = rc_cfg.min_quantizers[layer];
        lrc->worst_quality =
            av1_quantizer_to_qindex(rc_cfg.max_quantizers[layer]);
        lrc->best_quality =
            av1_quantizer_to_qindex(rc_cfg.min_quantizers[layer]);
        lc->scaling_factor_num = rc_cfg.scaling_factor_num[sl];
        lc->scaling_factor_den = rc_cfg.scaling_factor_den[sl];
        lc->framerate_factor = rc_cfg.ts_rate_decimator[tl];
        if (tl == cpi_->svc.number_temporal_layers - 1)
          target_bandwidth_svc += lc->layer_target_bitrate;
      }
    }

    if (cm->current_frame.frame_number == 0) av1_init_layer_context(cpi_);
    // This is needed to initialize external RC flag in layer context structure.
    cpi_->rc.rtc_external_ratectrl = 1;
    av1_update_layer_context_change_config(cpi_, target_bandwidth_svc);
  }
  check_reset_rc_flag(cpi_);
  return true;
}

void AV1RateControlRTC::ComputeQP(const AV1FrameParamsRTC &frame_params) {
  AV1_COMMON *const cm = &cpi_->common;
  int width, height;
  GF_GROUP *const gf_group = &cpi_->ppi->gf_group;
  cpi_->svc.spatial_layer_id = frame_params.spatial_layer_id;
  cpi_->svc.temporal_layer_id = frame_params.temporal_layer_id;
  if (cpi_->svc.number_spatial_layers > 1) {
    const int layer = LAYER_IDS_TO_IDX(cpi_->svc.spatial_layer_id,
                                       cpi_->svc.temporal_layer_id,
                                       cpi_->svc.number_temporal_layers);
    LAYER_CONTEXT *lc = &cpi_->svc.layer_context[layer];
    av1_get_layer_resolution(initial_width_, initial_height_,
                             lc->scaling_factor_num, lc->scaling_factor_den,
                             &width, &height);
    cm->width = width;
    cm->height = height;
  }
  enc_set_mb_mi(&cm->mi_params, cm->width, cm->height, BLOCK_8X8);
  cm->current_frame.frame_type = frame_params.frame_type;
  cpi_->refresh_frame.golden_frame =
      (cm->current_frame.frame_type == KEY_FRAME) ? 1 : 0;
  cpi_->sf.rt_sf.use_nonrd_pick_mode = 1;

  if (frame_params.frame_type == kKeyFrame) {
    gf_group->update_type[cpi_->gf_frame_index] = KF_UPDATE;
    gf_group->frame_type[cpi_->gf_frame_index] = KEY_FRAME;
    gf_group->refbuf_state[cpi_->gf_frame_index] = REFBUF_RESET;
    if (cpi_->ppi->use_svc) {
      const int layer = LAYER_IDS_TO_IDX(cpi_->svc.spatial_layer_id,
                                         cpi_->svc.temporal_layer_id,
                                         cpi_->svc.number_temporal_layers);
      if (cm->current_frame.frame_number > 0)
        av1_svc_reset_temporal_layers(cpi_, 1);
      cpi_->svc.layer_context[layer].is_key_frame = 1;
    }
  } else {
    gf_group->update_type[cpi_->gf_frame_index] = LF_UPDATE;
    gf_group->frame_type[cpi_->gf_frame_index] = INTER_FRAME;
    gf_group->refbuf_state[cpi_->gf_frame_index] = REFBUF_UPDATE;
    if (cpi_->ppi->use_svc) {
      const int layer = LAYER_IDS_TO_IDX(cpi_->svc.spatial_layer_id,
                                         cpi_->svc.temporal_layer_id,
                                         cpi_->svc.number_temporal_layers);
      cpi_->svc.layer_context[layer].is_key_frame = 0;
    }
  }
  if (cpi_->svc.spatial_layer_id == cpi_->svc.number_spatial_layers - 1)
    cpi_->rc.frames_since_key++;
  if (cpi_->svc.number_spatial_layers > 1 ||
      cpi_->svc.number_temporal_layers > 1) {
    av1_update_temporal_layer_framerate(cpi_);
    av1_restore_layer_context(cpi_);
  }
  int target = 0;
  if (cpi_->oxcf.rc_cfg.mode == AOM_CBR) {
    if (cpi_->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ)
      av1_cyclic_refresh_update_parameters(cpi_);
    if (frame_is_intra_only(cm)) {
      target = av1_calc_iframe_target_size_one_pass_cbr(cpi_);
      cpi_->common.current_frame.frame_number = 0;
    } else {
      target = av1_calc_pframe_target_size_one_pass_cbr(
          cpi_, gf_group->update_type[cpi_->gf_frame_index]);
    }
  }
  av1_rc_set_frame_target(cpi_, target, cm->width, cm->height);

  int bottom_index, top_index;
  cpi_->common.quant_params.base_qindex =
      av1_rc_pick_q_and_bounds(cpi_, cm->width, cm->height,
                               cpi_->gf_frame_index, &bottom_index, &top_index);

  if (cpi_->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ)
    av1_cyclic_refresh_setup(cpi_);
}

int AV1RateControlRTC::GetQP() const {
  return cpi_->common.quant_params.base_qindex;
}

AV1LoopfilterLevel AV1RateControlRTC::GetLoopfilterLevel() const {
  av1_pick_filter_level(nullptr, cpi_, LPF_PICK_FROM_Q);
  AV1LoopfilterLevel lpf_level;
  lpf_level.filter_level[0] = cpi_->common.lf.filter_level[0];
  lpf_level.filter_level[1] = cpi_->common.lf.filter_level[1];
  lpf_level.filter_level_u = cpi_->common.lf.filter_level_u;
  lpf_level.filter_level_v = cpi_->common.lf.filter_level_v;

  return lpf_level;
}

AV1CdefInfo AV1RateControlRTC::GetCdefInfo() const {
  av1_pick_cdef_from_qp(&cpi_->common, 0, 0);
  AV1CdefInfo cdef_level;
  cdef_level.cdef_strength_y = cpi_->common.cdef_info.cdef_strengths[0];
  cdef_level.cdef_strength_uv = cpi_->common.cdef_info.cdef_uv_strengths[0];
  cdef_level.damping = cpi_->common.cdef_info.cdef_damping;

  return cdef_level;
}

bool AV1RateControlRTC::GetSegmentationData(
    AV1SegmentationData *segmentation_data) const {
  if (cpi_->oxcf.q_cfg.aq_mode == 0) {
    return false;
  }
  segmentation_data->segmentation_map = cpi_->enc_seg.map;
  segmentation_data->segmentation_map_size =
      cpi_->common.mi_params.mi_rows * cpi_->common.mi_params.mi_cols;
  segmentation_data->delta_q = cpi_->cyclic_refresh->qindex_delta;
  segmentation_data->delta_q_size = 3u;
  return true;
}

void AV1RateControlRTC::PostEncodeUpdate(uint64_t encoded_frame_size) {
  cpi_->common.current_frame.frame_number++;
  if (cpi_->svc.spatial_layer_id == cpi_->svc.number_spatial_layers - 1)
    cpi_->svc.prev_number_spatial_layers = cpi_->svc.number_spatial_layers;
  av1_rc_postencode_update(cpi_, encoded_frame_size);
  if (cpi_->svc.number_spatial_layers > 1 ||
      cpi_->svc.number_temporal_layers > 1)
    av1_save_layer_context(cpi_);
}

}  // namespace aom
