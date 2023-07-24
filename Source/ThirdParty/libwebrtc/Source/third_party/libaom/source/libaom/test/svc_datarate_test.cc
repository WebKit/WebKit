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

#include <vector>
#include "config/aom_config.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/datarate_test.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_codec.h"
#include "av1/common/enums.h"
#include "av1/encoder/encoder.h"

namespace datarate_test {
namespace {

struct FrameInfo {
  FrameInfo(aom_codec_pts_t _pts, unsigned int _w, unsigned int _h)
      : pts(_pts), w(_w), h(_h) {}

  aom_codec_pts_t pts;
  unsigned int w;
  unsigned int h;
};

class DatarateTestSVC
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 unsigned int, int>,
      public DatarateTest {
 public:
  DatarateTestSVC() : DatarateTest(GET_PARAM(0)) {
    set_cpu_used_ = GET_PARAM(2);
    aq_mode_ = GET_PARAM(3);
  }

 protected:
  virtual void SetUp() {
    InitializeConfig(GET_PARAM(1));
    ResetModel();
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    frame_info_list_.push_back(FrameInfo(pts, img.d_w, img.d_h));
    ++decoded_nframes_;
  }

  std::vector<FrameInfo> frame_info_list_;

  virtual int GetNumSpatialLayers() { return number_spatial_layers_; }

  virtual void ResetModel() {
    DatarateTest::ResetModel();
    layer_frame_cnt_ = 0;
    superframe_cnt_ = 0;
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 1;
    for (int i = 0; i < AOM_MAX_LAYERS; i++) {
      target_layer_bitrate_[i] = 0;
      effective_datarate_tl[i] = 0.0;
    }
    memset(&layer_id_, 0, sizeof(aom_svc_layer_id_t));
    memset(&svc_params_, 0, sizeof(aom_svc_params_t));
    memset(&ref_frame_config_, 0, sizeof(aom_svc_ref_frame_config_t));
    memset(&ref_frame_comp_pred_, 0, sizeof(aom_svc_ref_frame_comp_pred_t));
    drop_frames_ = 0;
    for (int i = 0; i < 1000; i++) drop_frames_list_[i] = 1000;
    decoded_nframes_ = 0;
    mismatch_nframes_ = 0;
    mismatch_psnr_ = 0.0;
    set_frame_level_er_ = 0;
    multi_ref_ = 0;
    use_fixed_mode_svc_ = 0;
    comp_pred_ = 0;
    dynamic_enable_disable_mode_ = 0;
    intra_only_ = 0;
    frame_to_start_decoding_ = 0;
    layer_to_decode_ = 0;
    frame_sync_ = 0;
    current_video_frame_ = 0;
    screen_mode_ = 0;
    rps_mode_ = 0;
    rps_recovery_frame_ = 0;
    user_define_frame_qp_ = 0;
    set_speed_per_layer_ = false;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    int spatial_layer_id = 0;
    current_video_frame_ = video->frame();
    // video->frame() is called every superframe, so we should condition
    // this on layer_frame_cnt_ = 0, so we only do this once on the very
    // first frame.
    if (video->frame() == 0 && layer_frame_cnt_ == 0) {
      initialize_svc(number_temporal_layers_, number_spatial_layers_,
                     &svc_params_);
      if (dynamic_enable_disable_mode_ == 1) {
        svc_params_.layer_target_bitrate[2] = 0;
        cfg_.rc_target_bitrate -= target_layer_bitrate_[2];
      }
      encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      // TODO(aomedia:3032): Configure KSVC in fixed mode.
      encoder->Control(AV1E_SET_ENABLE_ORDER_HINT, 0);
      encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
      encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
      if (cfg_.g_threads > 1) {
        encoder->Control(AV1E_SET_TILE_COLUMNS, cfg_.g_threads >> 1);
        encoder->Control(AV1E_SET_ROW_MT, 1);
      }
      if (screen_mode_) {
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      }
    }
    if (number_spatial_layers_ == 2) {
      spatial_layer_id = (layer_frame_cnt_ % 2 == 0) ? 0 : 1;
    } else if (number_spatial_layers_ == 3) {
      spatial_layer_id = (layer_frame_cnt_ % 3 == 0)         ? 0
                         : ((layer_frame_cnt_ - 1) % 3 == 0) ? 1
                                                             : 2;
    }
    // Set the reference/update flags, layer_id, and reference_map
    // buffer index.
    frame_flags_ = set_layer_pattern(
        video->frame(), &layer_id_, &ref_frame_config_, &ref_frame_comp_pred_,
        spatial_layer_id, multi_ref_, comp_pred_,
        (video->frame() % cfg_.kf_max_dist) == 0, dynamic_enable_disable_mode_,
        rps_mode_, rps_recovery_frame_);
    if (intra_only_ == 1 && frame_sync_ > 0) {
      // Set an Intra-only frame on SL0 at frame_sync_.
      // In order to allow decoding to start on SL0 in mid-sequence we need to
      // set and refresh all the slots used on SL0 stream, which is 0 and 3
      // for this test pattern. The other slots (1, 2, 4, 5) are used for the
      // SL > 0 layers and these slotes are not refreshed on frame_sync_, so
      // temporal prediction for the top layers can continue.
      if (spatial_layer_id == 0 && video->frame() == frame_sync_) {
        ref_frame_config_.ref_idx[0] = 0;
        ref_frame_config_.ref_idx[3] = 3;
        ref_frame_config_.refresh[0] = 1;
        ref_frame_config_.refresh[3] = 1;
        for (int i = 0; i < INTER_REFS_PER_FRAME; i++)
          ref_frame_config_.reference[i] = 0;
      }
    }
    if (intra_only_ && video->frame() == 50 && spatial_layer_id == 1) {
      // Force an intra_only frame here, for SL1.
      for (int i = 0; i < INTER_REFS_PER_FRAME; i++)
        ref_frame_config_.reference[i] = 0;
    }
    encoder->Control(AV1E_SET_SVC_LAYER_ID, &layer_id_);
    // The SET_SVC_REF_FRAME_CONFIG and AV1E_SET_SVC_REF_FRAME_COMP_PRED api is
    // for the flexible SVC mode (i.e., use_fixed_mode_svc == 0).
    if (!use_fixed_mode_svc_) {
      encoder->Control(AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config_);
      encoder->Control(AV1E_SET_SVC_REF_FRAME_COMP_PRED, &ref_frame_comp_pred_);
    }
    if (set_speed_per_layer_) {
      int speed_per_layer = 10;
      if (layer_id_.spatial_layer_id == 0) {
        // For for base SL0,TL0: use the speed the test loops over.
        if (layer_id_.temporal_layer_id == 1) speed_per_layer = 7;
        if (layer_id_.temporal_layer_id == 2) speed_per_layer = 8;
      } else if (layer_id_.spatial_layer_id == 1) {
        if (layer_id_.temporal_layer_id == 0) speed_per_layer = 7;
        if (layer_id_.temporal_layer_id == 1) speed_per_layer = 8;
        if (layer_id_.temporal_layer_id == 2) speed_per_layer = 9;
      } else if (layer_id_.spatial_layer_id == 2) {
        if (layer_id_.temporal_layer_id == 0) speed_per_layer = 8;
        if (layer_id_.temporal_layer_id == 1) speed_per_layer = 9;
        if (layer_id_.temporal_layer_id == 2) speed_per_layer = 10;
      }
      encoder->Control(AOME_SET_CPUUSED, speed_per_layer);
    }
    if (set_frame_level_er_) {
      int mode =
          (layer_id_.spatial_layer_id > 0 || layer_id_.temporal_layer_id > 0);
      encoder->Control(AV1E_SET_ERROR_RESILIENT_MODE, mode);
    }
    if (dynamic_enable_disable_mode_ == 1) {
      if (layer_frame_cnt_ == 300 && spatial_layer_id == 0) {
        // Enable: set top spatial layer bitrate back to non-zero.
        svc_params_.layer_target_bitrate[2] = target_layer_bitrate_[2];
        cfg_.rc_target_bitrate += target_layer_bitrate_[2];
        encoder->Config(&cfg_);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      }
    } else if (dynamic_enable_disable_mode_ == 2) {
      if (layer_frame_cnt_ == 300 && spatial_layer_id == 0) {
        // Disable top spatial layer mid-stream.
        svc_params_.layer_target_bitrate[2] = 0;
        cfg_.rc_target_bitrate -= target_layer_bitrate_[2];
        encoder->Config(&cfg_);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      } else if (layer_frame_cnt_ == 600 && spatial_layer_id == 0) {
        // Enable top spatial layer mid-stream.
        svc_params_.layer_target_bitrate[2] = target_layer_bitrate_[2];
        cfg_.rc_target_bitrate += target_layer_bitrate_[2];
        encoder->Config(&cfg_);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      }
    }
    layer_frame_cnt_++;
    DatarateTest::PreEncodeFrameHook(video, encoder);

    if (user_define_frame_qp_) {
      frame_qp_ = rnd_.PseudoUniform(63);
      encoder->Control(AV1E_SET_QUANTIZER_ONE_PASS, frame_qp_);
    }
  }

  virtual void PostEncodeFrameHook(::libaom_test::Encoder *encoder) {
    int num_operating_points;
    encoder->Control(AV1E_GET_NUM_OPERATING_POINTS, &num_operating_points);
    ASSERT_EQ(num_operating_points,
              number_temporal_layers_ * number_spatial_layers_);

    if (user_define_frame_qp_) {
      if (current_video_frame_ >= static_cast<unsigned int>(total_frame_))
        return;
      int qp;
      encoder->Control(AOME_GET_LAST_QUANTIZER_64, &qp);
      ASSERT_EQ(qp, frame_qp_);
    }
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;
    // Update the layer cumulative  bitrate.
    for (int i = layer_id_.temporal_layer_id; i < number_temporal_layers_;
         i++) {
      int layer = layer_id_.spatial_layer_id * number_temporal_layers_ + i;
      effective_datarate_tl[layer] += 1.0 * frame_size_in_bits;
    }
    if (layer_id_.spatial_layer_id == number_spatial_layers_ - 1) {
      last_pts_ = pkt->data.frame.pts;
      superframe_cnt_++;
    }
  }

  virtual void EndPassHook() {
    duration_ = ((last_pts_ + 1) * timebase_);
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      effective_datarate_tl[i] = (effective_datarate_tl[i] / 1000) / duration_;
    }
  }

  virtual bool DoDecode() const {
    if (drop_frames_ > 0) {
      for (unsigned int i = 0; i < drop_frames_; ++i) {
        if (drop_frames_list_[i] == (unsigned int)superframe_cnt_) {
          std::cout << "             Skipping decoding frame: "
                    << drop_frames_list_[i] << "\n";
          return 0;
        }
      }
    } else if (intra_only_ == 1) {
      // Only start decoding at frames_to_start_decoding_.
      if (current_video_frame_ < frame_to_start_decoding_) return 0;
      // Only decode base layer for 3SL, for layer_to_decode_ = 0.
      if (layer_to_decode_ == 0 && frame_sync_ > 0 &&
          (layer_frame_cnt_ - 1) % 3 != 0)
        return 0;
    }
    return 1;
  }

  virtual void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }
  unsigned int GetDecodedFrames() { return decoded_nframes_; }

  static void ref_config_rps(aom_svc_ref_frame_config_t *ref_frame_config,
                             int frame_cnt, int rps_recovery_frame) {
    // Pattern of 3 references with (ALTREF and GOLDEN) trailing
    // LAST by 4 and 8 frame, with some switching logic to
    // only predict from longer-term reference.
    int last_idx = 0;
    int last_idx_refresh = 0;
    int gld_idx = 0;
    int alt_ref_idx = 0;
    const int lag_alt = 4;
    const int lag_gld = 8;
    const int sh = 8;  // slots 0 - 7.
    // Moving index slot for last: 0 - (sh - 1)
    if (frame_cnt > 1) last_idx = (frame_cnt - 1) % sh;
    // Moving index for refresh of last: one ahead for next frame.
    last_idx_refresh = frame_cnt % sh;
    // Moving index for gld_ref, lag behind current by lag_gld
    if (frame_cnt > lag_gld) gld_idx = (frame_cnt - lag_gld) % sh;
    // Moving index for alt_ref, lag behind LAST by lag_alt frames.
    if (frame_cnt > lag_alt) alt_ref_idx = (frame_cnt - lag_alt) % sh;
    // Set the ref_idx.
    // Default all references (7) to slot for last.
    // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
    // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
    for (int i = 0; i < INTER_REFS_PER_FRAME; i++)
      ref_frame_config->ref_idx[i] = last_idx;
    // Set the ref_idx for the relevant references.
    ref_frame_config->ref_idx[0] = last_idx;
    ref_frame_config->ref_idx[1] = last_idx_refresh;
    ref_frame_config->ref_idx[3] = gld_idx;
    ref_frame_config->ref_idx[6] = alt_ref_idx;
    // Refresh this slot, which will become LAST on next frame.
    ref_frame_config->refresh[last_idx_refresh] = 1;
    // Reference LAST, ALTREF, and GOLDEN
    ref_frame_config->reference[0] = 1;
    ref_frame_config->reference[6] = 1;
    ref_frame_config->reference[3] = 1;
    if (frame_cnt == rps_recovery_frame) {
      // Switch to only reference GOLDEN at recovery_frame.
      ref_frame_config->reference[0] = 0;
      ref_frame_config->reference[6] = 0;
      ref_frame_config->reference[3] = 1;
    } else if (frame_cnt > rps_recovery_frame &&
               frame_cnt < rps_recovery_frame + 8) {
      // Go back to predicting from LAST, and after
      // 8 frames (GOLDEN is 8 frames aways) go back
      // to predicting off GOLDEN and ALTREF.
      ref_frame_config->reference[0] = 1;
      ref_frame_config->reference[6] = 0;
      ref_frame_config->reference[3] = 0;
    }
  }

  // Layer pattern configuration.
  virtual int set_layer_pattern(
      int frame_cnt, aom_svc_layer_id_t *layer_id,
      aom_svc_ref_frame_config_t *ref_frame_config,
      aom_svc_ref_frame_comp_pred_t *ref_frame_comp_pred, int spatial_layer,
      int multi_ref, int comp_pred, int is_key_frame,
      int dynamic_enable_disable_mode, int rps_mode, int rps_recovery_frame) {
    int lag_index = 0;
    int base_count = frame_cnt >> 2;
    layer_id->spatial_layer_id = spatial_layer;
    // Set the reference map buffer idx for the 7 references:
    // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
    // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
    for (int i = 0; i < INTER_REFS_PER_FRAME; i++) {
      ref_frame_config->ref_idx[i] = i;
      ref_frame_config->reference[i] = 0;
    }
    for (int i = 0; i < REF_FRAMES; i++) ref_frame_config->refresh[i] = 0;
    if (comp_pred) {
      ref_frame_comp_pred->use_comp_pred[0] = 1;  // GOLDEN_LAST
      ref_frame_comp_pred->use_comp_pred[1] = 1;  // LAST2_LAST
      ref_frame_comp_pred->use_comp_pred[2] = 1;  // ALTREF_LAST
    }
    // Set layer_flags to 0 when using ref_frame_config->reference.
    int layer_flags = 0;
    // Always reference LAST.
    ref_frame_config->reference[0] = 1;
    if (number_temporal_layers_ == 1 && number_spatial_layers_ == 1) {
      ref_frame_config->refresh[0] = 1;
      if (rps_mode)
        ref_config_rps(ref_frame_config, frame_cnt, rps_recovery_frame);
    }
    if (number_temporal_layers_ == 2 && number_spatial_layers_ == 1) {
      // 2-temporal layer.
      //    1    3    5
      //  0    2    4
      // Keep golden fixed at slot 3.
      int base_count = frame_cnt >> 1;
      ref_frame_config->ref_idx[3] = 3;
      // Cyclically refresh slots 5, 6, 7, for lag alt ref.
      int lag_index = 5;
      if (base_count > 0) {
        lag_index = 5 + (base_count % 3);
        if (frame_cnt % 2 != 0) lag_index = 5 + ((base_count + 1) % 3);
      }
      // Set the altref slot to lag_index.
      ref_frame_config->ref_idx[6] = lag_index;
      if (frame_cnt % 2 == 0) {
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[0] = 1;
        // Refresh lag_index slot, needed for lagging golen.
        ref_frame_config->refresh[lag_index] = 1;
        // Refresh GOLDEN every x base layer frames.
        if (base_count % 32 == 0) ref_frame_config->refresh[3] = 1;
      } else {
        layer_id->temporal_layer_id = 1;
        // No updates on layer 1, reference LAST (TL0).
        ref_frame_config->reference[0] = 1;
      }
      // Always reference golden and altref on TL0.
      if (layer_id->temporal_layer_id == 0) {
        ref_frame_config->reference[3] = 1;
        ref_frame_config->reference[6] = 1;
      }
    }
    if (number_temporal_layers_ == 3 && number_spatial_layers_ == 1) {
      // 3-layer:
      //   1    3   5    7
      //     2        6
      // 0        4        8
      if (multi_ref) {
        // Keep golden fixed at slot 3.
        ref_frame_config->ref_idx[3] = 3;
        // Cyclically refresh slots 4, 5, 6, 7, for lag altref.
        lag_index = 4 + (base_count % 4);
        // Set the altref slot to lag_index.
        ref_frame_config->ref_idx[6] = lag_index;
      }
      if (frame_cnt % 4 == 0) {
        // Base layer.
        layer_id->temporal_layer_id = 0;
        // Update LAST on layer 0, reference LAST and GF.
        ref_frame_config->refresh[0] = 1;
        ref_frame_config->reference[3] = 1;
        if (multi_ref) {
          // Refresh GOLDEN every x ~10 base layer frames.
          if (base_count % 10 == 0) ref_frame_config->refresh[3] = 1;
          // Refresh lag_index slot, needed for lagging altref.
          ref_frame_config->refresh[lag_index] = 1;
        }
      } else if ((frame_cnt - 1) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // First top layer: no updates, only reference LAST (TL0).
      } else if ((frame_cnt - 2) % 4 == 0) {
        layer_id->temporal_layer_id = 1;
        // Middle layer (TL1): update LAST2, only reference LAST (TL0).
        ref_frame_config->refresh[1] = 1;
      } else if ((frame_cnt - 3) % 4 == 0) {
        layer_id->temporal_layer_id = 2;
        // Second top layer: no updates, only reference LAST.
        // Set buffer idx for LAST to slot 1, since that was the slot
        // updated in previous frame. So LAST is TL1 frame.
        ref_frame_config->ref_idx[0] = 1;
        ref_frame_config->ref_idx[1] = 0;
      }
      if (multi_ref) {
        // Every frame can reference GOLDEN AND ALTREF.
        ref_frame_config->reference[3] = 1;
        ref_frame_config->reference[6] = 1;
      }
    } else if (number_temporal_layers_ == 1 && number_spatial_layers_ == 2) {
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST. Keep LAST and GOLDEN in slots 0 and 3.
        ref_frame_config->ref_idx[0] = 0;
        ref_frame_config->ref_idx[3] = 3;
        ref_frame_config->refresh[0] = 1;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 3
        // and GOLDEN to slot 0. Update slot 3 (LAST).
        ref_frame_config->ref_idx[0] = 3;
        ref_frame_config->ref_idx[3] = 0;
        ref_frame_config->refresh[3] = 1;
      }
      // Reference GOLDEN.
      if (layer_id->spatial_layer_id > 0) ref_frame_config->reference[3] = 1;
    } else if (number_temporal_layers_ == 1 && number_spatial_layers_ == 3) {
      // 3 spatial layers, 1 temporal.
      // Note for this case , we set the buffer idx for all references to be
      // either LAST or GOLDEN, which are always valid references, since decoder
      // will check if any of the 7 references is valid scale in
      // valid_ref_frame_size().
      layer_id->temporal_layer_id = 0;
      if (layer_id->spatial_layer_id == 0) {
        // Reference LAST, update LAST. Set all other buffer_idx to 0.
        for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->refresh[0] = 1;
      } else if (layer_id->spatial_layer_id == 1) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1
        // and GOLDEN (and all other refs) to slot 0.
        // Update slot 1 (LAST).
        for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
        ref_frame_config->ref_idx[0] = 1;
        ref_frame_config->refresh[1] = 1;
      } else if (layer_id->spatial_layer_id == 2) {
        // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2
        // and GOLDEN (and all other refs) to slot 1.
        // Update slot 2 (LAST).
        for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 1;
        ref_frame_config->ref_idx[0] = 2;
        ref_frame_config->refresh[2] = 1;
        if (multi_ref) {
          ref_frame_config->ref_idx[6] = 7;
          ref_frame_config->reference[6] = 1;
          if (base_count % 10 == 0) ref_frame_config->refresh[7] = 1;
        }
      }
      // Reference GOLDEN.
      if (layer_id->spatial_layer_id > 0) ref_frame_config->reference[3] = 1;
    } else if (number_temporal_layers_ == 3 && number_spatial_layers_ == 3) {
      // 3 spatial and 3 temporal layer.
      // Overlap in the buffer slot updates: the slots 3 and 4 updated by
      // first TL2 are reused for update in TL1 superframe.
      if (superframe_cnt_ % 4 == 0) {
        // Base temporal layer.
        layer_id->temporal_layer_id = 0;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST, update LAST.
          // Set all buffer_idx to 0.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->refresh[0] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 0.
          // Update slot 1 (LAST).
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->refresh[1] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 1.
          // Update slot 2 (LAST).
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 1;
          ref_frame_config->ref_idx[0] = 2;
          ref_frame_config->refresh[2] = 1;
        }
      } else if ((superframe_cnt_ - 1) % 4 == 0) {
        // First top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST (slot 0).
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to slot 0.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 3.
          // Set LAST2 to slot 4 and Update slot 4.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->ref_idx[1] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 4.
          // No update.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 4;
          ref_frame_config->ref_idx[0] = 2;
        }
      } else if ((superframe_cnt_ - 2) % 4 == 0) {
        // Middle temporal enhancement layer.
        layer_id->temporal_layer_id = 1;
        if (layer_id->spatial_layer_id == 0) {
          // Reference LAST.
          // Set all buffer_idx to 0.
          // Set GOLDEN to slot 3 and update slot 3.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 1,
          // GOLDEN (and all other refs) to slot 3.
          // Set LAST2 to slot 4 and update slot 4.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 3;
          ref_frame_config->ref_idx[0] = 1;
          ref_frame_config->ref_idx[2] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 2,
          // GOLDEN (and all other refs) to slot 4.
          // Set LAST2 to slot 5 and update slot 5.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 4;
          ref_frame_config->ref_idx[0] = 2;
          ref_frame_config->ref_idx[2] = 5;
          ref_frame_config->refresh[5] = 1;
        }
      } else if ((superframe_cnt_ - 3) % 4 == 0) {
        // Second top temporal enhancement layer.
        layer_id->temporal_layer_id = 2;
        if (layer_id->spatial_layer_id == 0) {
          // Set LAST to slot 3 and reference LAST.
          // Set GOLDEN to slot 3 and update slot 3.
          // Set all other buffer_idx to 0.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 3;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->refresh[3] = 1;
        } else if (layer_id->spatial_layer_id == 1) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 4,
          // GOLDEN to slot 3. Set LAST2 to slot 4 and update slot 4.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 4;
          ref_frame_config->ref_idx[3] = 3;
          ref_frame_config->ref_idx[1] = 4;
          ref_frame_config->refresh[4] = 1;
        } else if (layer_id->spatial_layer_id == 2) {
          // Reference LAST and GOLDEN. Set buffer_idx for LAST to slot 5,
          // GOLDEN to slot 4. No update.
          for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = 0;
          ref_frame_config->ref_idx[0] = 5;
          ref_frame_config->ref_idx[3] = 4;
        }
      }
      if (layer_id->spatial_layer_id > 0) {
        // Always reference GOLDEN (inter-layer prediction).
        ref_frame_config->reference[3] = 1;
        if (is_key_frame && layer_id->spatial_layer_id > 0) {
          // On superframes whose base is key: remove LAST since GOLDEN
          // is used as reference.
          ref_frame_config->reference[0] = 0;
        }
      }
      // Allow for top spatial layer to use additional temporal reference.
      // Additional reference is only updated on base temporal layer, every
      // 10 TL0 frames here.
      if (multi_ref && layer_id->spatial_layer_id == 2) {
        ref_frame_config->ref_idx[6] = 7;
        if (!is_key_frame) ref_frame_config->reference[6] = 1;
        if (base_count % 10 == 0 && layer_id->temporal_layer_id == 0)
          ref_frame_config->refresh[7] = 1;
      }
    }
    // If the top spatial layer is first-time encoded in mid-sequence
    // (i.e., dynamic_enable_disable_mode = 1), then don't predict from LAST,
    // since it will have been last updated on first key frame (SL0) and so
    // be different resolution from SL2.
    if (dynamic_enable_disable_mode == 1 &&
        layer_id->spatial_layer_id == number_spatial_layers_ - 1)
      ref_frame_config->reference[0] = 0;
    return layer_flags;
  }

  virtual void initialize_svc(int number_temporal_layers,
                              int number_spatial_layers,
                              aom_svc_params *svc_params) {
    svc_params->number_spatial_layers = number_spatial_layers;
    svc_params->number_temporal_layers = number_temporal_layers;
    for (int i = 0; i < number_temporal_layers * number_spatial_layers; ++i) {
      svc_params->max_quantizers[i] = 60;
      svc_params->min_quantizers[i] = 2;
      svc_params->layer_target_bitrate[i] = target_layer_bitrate_[i];
    }
    // Do at most 3 spatial or temporal layers here.
    svc_params->framerate_factor[0] = 1;
    if (number_temporal_layers == 2) {
      svc_params->framerate_factor[0] = 2;
      svc_params->framerate_factor[1] = 1;
    } else if (number_temporal_layers == 3) {
      svc_params->framerate_factor[0] = 4;
      svc_params->framerate_factor[1] = 2;
      svc_params->framerate_factor[2] = 1;
    }
    svc_params->scaling_factor_num[0] = 1;
    svc_params->scaling_factor_den[0] = 1;
    if (number_spatial_layers == 2) {
      svc_params->scaling_factor_num[0] = 1;
      svc_params->scaling_factor_den[0] = 2;
      svc_params->scaling_factor_num[1] = 1;
      svc_params->scaling_factor_den[1] = 1;
    } else if (number_spatial_layers == 3) {
      svc_params->scaling_factor_num[0] = 1;
      svc_params->scaling_factor_den[0] = 4;
      svc_params->scaling_factor_num[1] = 1;
      svc_params->scaling_factor_den[1] = 2;
      svc_params->scaling_factor_num[2] = 1;
      svc_params->scaling_factor_den[2] = 1;
    }
  }

  virtual void BasicRateTargetingSVC3TL1SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 1;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Top temporal layers are non_reference, so exlcude them from
    // mismatch count, since loopfilter/cdef is not applied for these on
    // encoder side, but is always applied on decoder.
    // This means 150 = #frames(300) - #TL2_frames(150).
    EXPECT_EQ((int)GetMismatchFrames(), 150);
  }

  virtual void SetFrameQpSVC3TL1SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 1;

    user_define_frame_qp_ = 1;
    total_frame_ = 300;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  virtual void SetFrameQpSVC3TL3SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    user_define_frame_qp_ = 1;
    total_frame_ = 300;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  virtual void BasicRateTargetingSVC3TL1SLScreenTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, 60);

    const int bitrate_array[2] = { 800, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    screen_mode_ = 1;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 1;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.50)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.7)
          << " The datarate for the file is greater than target by too much!";
    }
    // Top temporal layers are non_reference, so exlcude them from
    // mismatch count, since loopfilter/cdef is not applied for these on
    // encoder side, but is always applied on decoder.
    // This means 30 = #frames(60) - #TL2_frames(30).
    // We use LE for screen since loopfilter level can become very small
    // or zero and then the frame is not a mismatch.
    EXPECT_LE((int)GetMismatchFrames(), 30);
  }

  virtual void BasicRateTargetingSVC1TL3SLScreenTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);

    const int bitrate_array[2] = { 800, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    screen_mode_ = 1;
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 3;
    target_layer_bitrate_[0] = 30 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 60 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.50)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.5)
          << " The datarate for the file is greater than target by too much!";
    }
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC1TL1SLScreenScCutsMotionTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);

    const int bitrate_array[2] = { 200, 500 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    screen_mode_ = 1;
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 1;
    target_layer_bitrate_[0] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.40)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.7)
          << " The datarate for the file is greater than target by too much!";
    }
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL1SLResizeTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.rc_resize_mode = RESIZE_DYNAMIC;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);
    cfg_.g_w = 640;
    cfg_.g_h = 480;
    const int bitrate_array[2] = { 80, 90 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    unsigned int last_w = cfg_.g_w;
    unsigned int last_h = cfg_.g_h;
    int resize_down_count = 0;
    for (std::vector<FrameInfo>::const_iterator info = frame_info_list_.begin();
         info != frame_info_list_.end(); ++info) {
      if (info->w != last_w || info->h != last_h) {
        // Verify that resize down occurs.
        ASSERT_LT(info->w, last_w);
        ASSERT_LT(info->h, last_h);
        last_w = info->w;
        last_h = info->h;
        resize_down_count++;
      }
    }
    // Must be at least one resize down.
    ASSERT_GE(resize_down_count, 1);
  }

  virtual void BasicRateTargetingSVC1TL2SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 300, 600 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 2;
    target_layer_bitrate_[0] = 2 * cfg_.rc_target_bitrate / 4;
    target_layer_bitrate_[1] = 2 * cfg_.rc_target_bitrate / 4;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLIntraStartDecodeBaseMidSeq() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    intra_only_ = 1;
    frame_sync_ = 20;
    frame_to_start_decoding_ = frame_sync_;
    layer_to_decode_ = 0;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    // Only check datarate on SL0 - this is layer that is decoded starting at
    // frame_to_start_decoding_.
    for (int i = 0; i < number_temporal_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.50)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Only base spatial layer is decoded and there are no non-referenece
    // frames on S0, so #mismatch must be 0.
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL3SLIntraMidSeqDecodeAll() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    intra_only_ = 1;
    frame_sync_ = 20;
    frame_to_start_decoding_ = 0;
    layer_to_decode_ = 3;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.585)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // All 3 spatial layers are decoded, starting at frame 0, so there are
    // and there 300/2 = 150 non-reference frames, so mismatch is 150.
    EXPECT_EQ((int)GetMismatchFrames(), 150);
  }

  virtual void BasicRateTargetingSVC1TL2SLIntraOnlyTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 300, 600 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    intra_only_ = 1;
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 2;
    target_layer_bitrate_[0] = 2 * cfg_.rc_target_bitrate / 4;
    target_layer_bitrate_[1] = 2 * cfg_.rc_target_bitrate / 4;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC1TL3SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 3;
    target_layer_bitrate_[0] = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[1] = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[2] = 4 * cfg_.rc_target_bitrate / 8;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC1TL3SLMultiRefTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    multi_ref_ = 1;
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 3;
    target_layer_bitrate_[0] = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[1] = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[2] = 4 * cfg_.rc_target_bitrate / 8;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.50)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLHDTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingFixedModeSVC3TL3SLHDTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    use_fixed_mode_svc_ = 1;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLMultiThreadSpeedPerLayerTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.g_threads = 2;
    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);
    cfg_.g_w = 640;
    cfg_.g_h = 480;
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    set_speed_per_layer_ = true;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLHDMultiThread2Test() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.g_threads = 2;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLHDMultiThread4Test() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.g_threads = 4;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLHDMultiRefTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    multi_ref_ = 1;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.45)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL3SLKfTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.kf_mode = AOM_KF_AUTO;
    cfg_.kf_min_dist = cfg_.kf_max_dist = 100;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.55)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.4)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargeting444SVC3TL3SLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.g_profile = 1;

    ::libaom_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 140);

    const int bitrate_array[2] = { 600, 1200 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.70)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC3TL1SLMultiRefDropAllEnhTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    // error_resilient can set to off/0, since for SVC the context update
    // is done per-layer.
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    multi_ref_ = 1;
    // Drop TL1 and TL2: #frames(300) - #TL0.
    drop_frames_ = 300 - 300 / 4;
    int n = 0;
    for (int i = 0; i < 300; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
      }
    }
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL1SLDropAllEnhTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    // error_resilient can set to off/0, since for SVC the context update
    // is done per-layer.
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    // Drop TL1 and TL2: #frames(300) - #TL0.
    drop_frames_ = 300 - 300 / 4;
    int n = 0;
    for (int i = 0; i < 300; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
      }
    }
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL1SLDropTL2EnhTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    // error_resilient for sequence can be off/0, since dropped frames (TL2)
    // are non-reference frames.
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    // Drop TL2: #frames(300) - (#TL0 + #TL1).
    drop_frames_ = 300 - 300 / 2;
    int n = 0;
    for (int i = 0; i < 300; i++) {
      if (i % 2 != 0) {
        drop_frames_list_[n] = i;
        n++;
      }
    }
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL1SLDropAllEnhFrameERTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    // Set error_resilience at frame level, with codec control,
    // on/1 for enahancement layers and off/0 for base layer frames.
    set_frame_level_er_ = 1;

    // Drop TL1 and TL2: #frames(300) - #TL0.
    drop_frames_ = 300 - 300 / 4;
    int n = 0;
    for (int i = 0; i < 300; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
      }
    }
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  virtual void BasicRateTargetingSVC3TL1SLDropSetEnhFrameERTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    // Set error_resilience at frame level, with codec control,
    // on/1 for enahancement layers and off/0 for base layer frames.
    set_frame_level_er_ = 1;

    // Drop TL1 and TL2: for part of sequence. Start at first TL2 at
    // frame 101, and end at second T2 at frame 199. Frame 200 is TL0,
    // so we can continue decoding without mismatch (since LAST is the
    // only reference and error_resilient = 1 on TL1/TL2 frames).
    int n = 0;
    int num_nonref = 300 / 2;
    for (int i = 101; i < 200; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
        if (i % 2 != 0) num_nonref -= 1;
      }
    }
    drop_frames_ = n;
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), num_nonref);
  }

  virtual void BasicRateTargetingSVC2TL1SLDropSetEnhER0Test() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();

    // Set error_resilience off.
    cfg_.g_error_resilient = 0;

    // Drop TL1: for part of sequence. Start at first TL1 at
    // frame 101, and end at frame 199. Frame 200 is TL0,
    // so we can continue decoding without mismatch (since LAST is the
    // only reference).
    int n = 0;
    int num_nonref = 300 / 2;
    for (int i = 101; i < 200; i++) {
      if (i % 2 != 0) {
        drop_frames_list_[n] = i;
        n++;
        if (i % 2 != 0) num_nonref -= 1;
      }
    }
    drop_frames_ = n;
    number_temporal_layers_ = 2;
    target_layer_bitrate_[0] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), num_nonref);
  }

  virtual void BasicRateTargetingSVC3TL1SLDropSetEnhER0Test() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();

    // Set error_resilience off.
    cfg_.g_error_resilient = 0;

    // Drop TL1 and TL2: for part of sequence. Start at first TL2 at
    // frame 101, and end at second T2 at frame 199. Frame 200 is TL0,
    // so we can continue decoding without mismatch (since LAST is the
    // only reference).
    int n = 0;
    int num_nonref = 300 / 2;
    for (int i = 101; i < 200; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
        if (i % 2 != 0) num_nonref -= 1;
      }
    }
    drop_frames_ = n;
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), num_nonref);
  }

  virtual void BasicRateTargetingSVC3TL3SLDropSetEnhER0Test() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 200, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    // Set error_resilience off.
    cfg_.g_error_resilient = 0;
    // Drop TL1 and TL2: for part of sequence. Start at first TL2 at
    // frame 101, and end at second T2 at frame 199. Frame 200 is TL0,
    // so we can continue decoding without mismatch (since LAST is the
    // only reference).
    // Drop here means drop whole superframe.
    int n = 0;
    int num_nonref = 300 / 2;
    for (int i = 101; i < 200; i++) {
      if (i % 4 != 0) {
        drop_frames_list_[n] = i;
        n++;
        if (i % 2 != 0) num_nonref -= 1;
      }
    }
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 3;
    multi_ref_ = 1;
    drop_frames_ = n * number_spatial_layers_;
    // SL0
    const int bitrate_sl0 = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[0] = 50 * bitrate_sl0 / 100;
    target_layer_bitrate_[1] = 70 * bitrate_sl0 / 100;
    target_layer_bitrate_[2] = bitrate_sl0;
    // SL1
    const int bitrate_sl1 = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[3] = 50 * bitrate_sl1 / 100;
    target_layer_bitrate_[4] = 70 * bitrate_sl1 / 100;
    target_layer_bitrate_[5] = bitrate_sl1;
    // SL2
    const int bitrate_sl2 = 4 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[6] = 50 * bitrate_sl2 / 100;
    target_layer_bitrate_[7] = 70 * bitrate_sl2 / 100;
    target_layer_bitrate_[8] = bitrate_sl2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 * number_spatial_layers_ - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), num_nonref);
  }

  virtual void BasicRateTargetingSVC3TL1SLMultiRefCompoundTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);
    cfg_.g_w = 640;
    cfg_.g_h = 480;
    const int bitrate_array[2] = { 400, 800 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    multi_ref_ = 1;
    comp_pred_ = 1;
    number_temporal_layers_ = 3;
    number_spatial_layers_ = 1;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC1TL3SLDynEnablTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, 400);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 3;
    target_layer_bitrate_[0] = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[1] = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[2] = 4 * cfg_.rc_target_bitrate / 8;
    dynamic_enable_disable_mode_ = 1;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    // No need to check RC on top layer which is disabled part of the time.
    for (int i = 0; i < number_spatial_layers_ - 1; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingSVC1TL3SLDynDisEnablTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 500, 1000 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 1;
    number_spatial_layers_ = 3;
    target_layer_bitrate_[0] = 1 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[1] = 3 * cfg_.rc_target_bitrate / 8;
    target_layer_bitrate_[2] = 4 * cfg_.rc_target_bitrate / 8;
    dynamic_enable_disable_mode_ = 2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    // No need to check RC on top layer which is disabled part of the time.
    for (int i = 0; i < number_spatial_layers_ - 1; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.38)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  virtual void BasicRateTargetingRPS1TL1SLDropFramesTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 100, 300 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    rps_mode_ = 1;
    rps_recovery_frame_ = 100;
    cfg_.g_error_resilient = 0;
    // Drop x frames before the recovery frames (where the reference
    // is switched to an older reference (golden or altref).
    // GOLDEN is 8 frames behind (for the rps pattern example) so we can't
    // drop more than 8 frames recovery frame, so choose x = 7.
    int n = 0;
    for (int i = rps_recovery_frame_ - 7; i < rps_recovery_frame_; i++) {
      drop_frames_list_[n] = i;
      n++;
    }
    drop_frames_ = n;
    number_spatial_layers_ = 1;
    number_temporal_layers_ = 1;
    target_layer_bitrate_[0] = cfg_.rc_target_bitrate;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_ * number_spatial_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.60)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.60)
          << " The datarate for the file is greater than target by too much!";
    }
    // Test that no mismatches have been found.
    std::cout << "          Decoded frames: " << GetDecodedFrames() << "\n";
    std::cout << "          Mismatch frames: " << GetMismatchFrames() << "\n";
    EXPECT_EQ(300 - GetDecodedFrames(), drop_frames_);
    EXPECT_EQ((int)GetMismatchFrames(), 0);
  }

  int layer_frame_cnt_;
  int superframe_cnt_;
  int number_temporal_layers_;
  int number_spatial_layers_;
  // Allow for up to 3 temporal layers.
  int target_layer_bitrate_[AOM_MAX_LAYERS];
  aom_svc_params_t svc_params_;
  aom_svc_ref_frame_config_t ref_frame_config_;
  aom_svc_ref_frame_comp_pred_t ref_frame_comp_pred_;
  aom_svc_layer_id_t layer_id_;
  double effective_datarate_tl[AOM_MAX_LAYERS];
  unsigned int drop_frames_;
  unsigned int drop_frames_list_[1000];
  unsigned int mismatch_nframes_;
  unsigned int decoded_nframes_;
  double mismatch_psnr_;
  int set_frame_level_er_;
  int multi_ref_;
  int use_fixed_mode_svc_;
  int comp_pred_;
  int dynamic_enable_disable_mode_;
  int intra_only_;
  unsigned int frame_to_start_decoding_;
  unsigned int layer_to_decode_;
  unsigned int frame_sync_;
  unsigned int current_video_frame_;
  int screen_mode_;
  int rps_mode_;
  int rps_recovery_frame_;

  int user_define_frame_qp_;
  int frame_qp_;
  int total_frame_;
  bool set_speed_per_layer_;
  libaom_test::ACMRandom rnd_;
};

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SL) {
  BasicRateTargetingSVC3TL1SLTest();
}

TEST_P(DatarateTestSVC, SetFrameQpSVC3TL1SL) { SetFrameQpSVC3TL1SLTest(); }

TEST_P(DatarateTestSVC, SetFrameQpSVC3TL3SL) { SetFrameQpSVC3TL3SLTest(); }

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial
// for screen mode.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLScreen) {
  BasicRateTargetingSVC3TL1SLScreenTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 1 temporal
// for screen mode.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL3SLScreen) {
  BasicRateTargetingSVC1TL3SLScreenTest();
}

// Check basic rate targeting for CBR, for 1 temporal layer, 1 spatial
// for screen mode, with source with many scene cuts and motion.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL1SLScreenScCutsMotion) {
  BasicRateTargetingSVC1TL1SLScreenScCutsMotionTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial,
// with dynamic resize on. Encode at very low bitrate and check that
// there is at least one resize (down) event.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLResize) {
  BasicRateTargetingSVC3TL1SLResizeTest();
}

// Check basic rate targeting for CBR, for 2 spatial layers, 1 temporal.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL2SL) {
  BasicRateTargetingSVC1TL2SLTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 3 temporal,
// with Intra-only frame inserted in the stream. Verify that we can start
// decoding the SL0 stream at the intra_only frame in mid-sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLIntraStartDecodeBaseMidSeq) {
  BasicRateTargetingSVC3TL3SLIntraStartDecodeBaseMidSeq();
}

// Check basic rate targeting for CBR, for 3spatial layers, 3 temporal,
// with Intra-only frame inserted in the stream. Verify that we can
// decode all frames and layers with no mismatch.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLIntraMidSeqDecodeAll) {
  BasicRateTargetingSVC3TL3SLIntraMidSeqDecodeAll();
}

// Check basic rate targeting for CBR, for 2 spatial layers, 1 temporal,
// with Intra-only frame inserted in the stream.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL2SLIntraOnly) {
  BasicRateTargetingSVC1TL2SLIntraOnlyTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 1 temporal.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL3SL) {
  BasicRateTargetingSVC1TL3SLTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 1 temporal,
// with additional temporal reference for top spatial layer.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL3SLMultiRef) {
  BasicRateTargetingSVC1TL3SLMultiRefTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SL) {
  BasicRateTargetingSVC3TL3SLTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLHD) {
  BasicRateTargetingSVC3TL3SLHDTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for fixed mode SVC.
TEST_P(DatarateTestSVC, BasicRateTargetingFixedModeSVC3TL3SLHD) {
  BasicRateTargetingFixedModeSVC3TL3SLHDTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for 2 threads, 2 tile_columns, row-mt enabled, and different speed
// per layer.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLMultiThreadSpeedPerLayer) {
  BasicRateTargetingSVC3TL3SLMultiThreadSpeedPerLayerTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for 2 threads, 2 tile_columns, row-mt enabled.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLHDMultiThread2) {
  BasicRateTargetingSVC3TL3SLHDMultiThread2Test();
}
// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for 4 threads, 4 tile_columns, row-mt enabled.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLHDMultiThread4) {
  BasicRateTargetingSVC3TL3SLHDMultiThread4Test();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// with additional temporal reference for top spatial layer.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLHDMultiRef) {
  BasicRateTargetingSVC3TL3SLHDMultiRefTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for auto key frame mode with short key frame period.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLKf) {
  BasicRateTargetingSVC3TL3SLKfTest();
}

// Check basic rate targeting for CBR, for 3 spatial, 3 temporal layers,
// for 4:4:4 input.
#if defined(CONFIG_MAX_DECODE_PROFILE) && CONFIG_MAX_DECODE_PROFILE < 1
TEST_P(DatarateTestSVC, DISABLED_BasicRateTargeting444SVC3TL3SL) {
#else
TEST_P(DatarateTestSVC, BasicRateTargeting444SVC3TL3SL) {
#endif
  BasicRateTargeting444SVC3TL3SLTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping of all enhancement layers (TL 1 and TL2). Check that the base
// layer (TL0) can still be decodeable (with no mismatch) with the
// error_resilient flag set to 0. This test used the pattern with multiple
// references (last, golden, and altref), updated on base layer.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLMultiRefDropAllEnh) {
  BasicRateTargetingSVC3TL1SLMultiRefDropAllEnhTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping of all enhancement layers (TL 1 and TL2). Check that the base
// layer (TL0) can still be decodeable (with no mismatch) with the
// error_resilient flag set to 0.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLDropAllEnh) {
  BasicRateTargetingSVC3TL1SLDropAllEnhTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping of the TL2 enhancement layer, which are non-reference
// (droppble) frames. For the base layer (TL0) and TL1 to still be decodeable
// (with no mismatch), the error_resilient_flag may be off (set to 0),
// since TL2 are non-reference frames.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLDropTL2Enh) {
  BasicRateTargetingSVC3TL1SLDropTL2EnhTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping of all enhancement layers (TL 1 and TL2). Test that the
// error_resilient flag can be set at frame level, with on/1 on
// enhancement layers and off/0 on base layer.
// This allows for successful decoding after dropping enhancement layer frames.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLDropAllEnhFrameER) {
  BasicRateTargetingSVC3TL1SLDropAllEnhFrameERTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping set of enhancement layers (TL 1 and TL2) in middle of sequence.
// Test that the error_resilient flag can be set at frame level, with on/1 on
// enhancement layers and off/0 on base layer.
// This allows for successful decoding after dropping a set enhancement layer
// frames in the sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLDropSetEnhFrameER) {
  BasicRateTargetingSVC3TL1SLDropSetEnhFrameERTest();
}

// Check basic rate targeting for CBR, for 2 temporal layers, 1 spatial layer,
// with dropping set of enhancement layers (TL 1) in middle of sequence.
// Test that the error_resilient flag can be 0/off for all frames.
// This allows for successful decoding after dropping a set enhancement layer
// frames in the sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC2TL1SLDropSetEnhER0) {
  BasicRateTargetingSVC2TL1SLDropSetEnhER0Test();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with dropping set of enhancement layers (TL 1 and TL2) in middle of sequence.
// Test that the error_resilient flag can be 0/off for all frames.
// This allows for successful decoding after dropping a set enhancement layer
// frames in the sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLDropSetEnhER0) {
  BasicRateTargetingSVC3TL1SLDropSetEnhER0Test();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 3 spatial layers,
// with dropping set of enhancement layers (superframe TL 1 and TL2) in middle
// of sequence. Test that the error_resilient flag can be 0/off for all frames.
// This allows for successful decoding after dropping a set enhancement layer
// frames in the sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL3SLDropSetEnhER0) {
  BasicRateTargetingSVC3TL3SLDropSetEnhER0Test();
}

// Check basic rate targeting for CBR, for 3 temporal layers, 1 spatial layer,
// with compound prediction on, for pattern with two additional refereces
// (golden and altref), both updated on base TLO frames.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLMultiRefCompound) {
  BasicRateTargetingSVC3TL1SLMultiRefCompoundTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 1 temporal,
// with the top spatial layer starting disabled (0 bitrate) and then
// dynamically enabled after x frames with nonzero bitrate.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL3SLDynEnabl) {
  BasicRateTargetingSVC1TL3SLDynEnablTest();
}

// Check basic rate targeting for CBR, for 3 spatial layers, 1 temporal,
// with the top spatial layer dynamically disabled snd enabled during the
// middle of the sequence.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL3SLDynDisEnabl) {
  BasicRateTargetingSVC1TL3SLDynDisEnablTest();
}

// Check basic rate targeting and encoder/decodermismatch, for RPS
// with 1 layer. A number of consecutive frames are lost midway in
// sequence, and encoder resorts to a longer term reference to recovery
// and continue decoding successfully.
TEST_P(DatarateTestSVC, BasicRateTargetingRPS1TL1SLDropFrames) {
  BasicRateTargetingRPS1TL1SLDropFramesTest();
}

AV1_INSTANTIATE_TEST_SUITE(DatarateTestSVC,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(7, 12), ::testing::Values(0, 3),
                           ::testing::Values(0, 1));

}  // namespace
}  // namespace datarate_test
