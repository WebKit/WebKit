/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
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

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

constexpr size_t kNumFrames = 450;

const int kTemporalId3Layer[4] = { 0, 2, 1, 2 };
const int kTemporalId2Layer[2] = { 0, 1 };
const int kTemporalRateAllocation3Layer[3] = { 50, 70, 100 };
const int kTemporalRateAllocation2Layer[2] = { 60, 100 };
const int kSpatialLayerBitrate[3] = { 200, 500, 900 };

// Parameter: aq mode: 0 and 3
class RcInterfaceTest : public ::libaom_test::EncoderTest,
                        public ::libaom_test::CodecTestWithParam<int> {
 public:
  RcInterfaceTest()
      : EncoderTest(GET_PARAM(0)), aq_mode_(GET_PARAM(1)), key_interval_(3000),
        encoder_exit_(false), layer_frame_cnt_(0), superframe_cnt_(0),
        frame_cnt_(0), dynamic_temporal_layers_(false),
        dynamic_spatial_layers_(false), num_drops_(0), max_consec_drop_ms_(0),
        frame_drop_thresh_(0) {
    memset(&svc_params_, 0, sizeof(svc_params_));
    memset(&layer_id_, 0, sizeof(layer_id_));
  }

  ~RcInterfaceTest() override = default;

 protected:
  void SetUp() override { InitializeConfig(::libaom_test::kRealTime); }

  int GetNumSpatialLayers() override { return rc_cfg_.ss_number_layers; }

  void PreEncodeFrameHook(libaom_test::VideoSource *video,
                          libaom_test::Encoder *encoder) override {
    int key_int = key_interval_;
    const int use_svc =
        rc_cfg_.ss_number_layers > 1 || rc_cfg_.ts_number_layers > 1;
    encoder->Control(AV1E_SET_RTC_EXTERNAL_RC, 1);
    if (video->frame() == 0 && layer_frame_cnt_ == 0) {
      encoder->Control(AOME_SET_CPUUSED, 7);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_ENABLE_ORDER_HINT, 0);
      if (rc_cfg_.is_screen) {
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      } else {
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      }
      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT,
                       rc_cfg_.max_intra_bitrate_pct);
      if (use_svc) encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      encoder->Control(AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR,
                       max_consec_drop_ms_);
    }
    // SVC specific settings
    if (use_svc) {
      frame_params_.spatial_layer_id =
          layer_frame_cnt_ % rc_cfg_.ss_number_layers;
      if (rc_cfg_.ts_number_layers == 3)
        frame_params_.temporal_layer_id =
            kTemporalId3Layer[superframe_cnt_ % 4];
      else if (rc_cfg_.ts_number_layers == 2)
        frame_params_.temporal_layer_id =
            kTemporalId2Layer[superframe_cnt_ % 2];
      else
        frame_params_.temporal_layer_id = 0;
      layer_id_.spatial_layer_id = frame_params_.spatial_layer_id;
      layer_id_.temporal_layer_id = frame_params_.temporal_layer_id;
      encoder->Control(AV1E_SET_SVC_LAYER_ID, &layer_id_);
      key_int = key_interval_ * rc_cfg_.ss_number_layers;
    }
    frame_params_.frame_type =
        layer_frame_cnt_ % key_int == 0 ? aom::kKeyFrame : aom::kInterFrame;
    encoder_exit_ = video->frame() == kNumFrames;
    frame_flags_ = 0;

    if (dynamic_temporal_layers_) {
      if (superframe_cnt_ == 100 && layer_id_.spatial_layer_id == 0) {
        // Go down to 2 temporal layers.
        SetConfigSvc(3, 2);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      } else if (superframe_cnt_ == 200 && layer_id_.spatial_layer_id == 0) {
        // Go down to 1 temporal layer.
        SetConfigSvc(3, 1);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      } else if (superframe_cnt_ == 300 && layer_id_.spatial_layer_id == 0) {
        // Go back up to 3 temporal layers.
        SetConfigSvc(3, 3);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      }
    } else if (dynamic_spatial_layers_) {
      // In this example the #spatial layers is modified on the fly,
      // so we go from (120p,240p,480p) to (240p,480p), etc.
      if (superframe_cnt_ == 100 && layer_id_.spatial_layer_id == 0) {
        // Change to 2 spatial layers (240p, 480p).
        SetConfigSvc(2, 3);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      } else if (superframe_cnt_ == 200 && layer_id_.spatial_layer_id == 0) {
        // Change to 1 spatial layer (480p).
        SetConfigSvc(1, 3);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      } else if (superframe_cnt_ == 300 && layer_id_.spatial_layer_id == 0) {
        // Go back to 3 spatial layers (120p, 240p, 480p).
        SetConfigSvc(3, 3);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
        // In the fixed SVC mode (which is what is used in this test):
        // Key frame is required here on SL0 since 120p will try to predict
        // from LAST which was the 480p, so decoder will throw an error
        // (reference must be smaller than 4x4). In the flexible mode
        // (not used here) we can set the frame flags to predict off the 2x2
        // reference instead,
        frame_flags_ = AOM_EFLAG_FORCE_KF;
        frame_params_.frame_type = aom::kKeyFrame;
        ASSERT_TRUE(rc_api_->UpdateRateControl(rc_cfg_));
      }
    }
    // TODO(marpan): Add dynamic spatial layers based on 0 layer bitrate.
    // That is actual usage in SW where configuration (#spatial, #temporal)
    // layers is fixed, but top layer is dropped or re-enabled based on
    // bitrate. This requires external RC to handle dropped (zero-size) frames.
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    if (encoder_exit_) {
      return;
    }
    layer_frame_cnt_++;
    frame_cnt_++;
    if (layer_id_.spatial_layer_id == rc_cfg_.ss_number_layers - 1)
      superframe_cnt_++;
    int qp;
    encoder->Control(AOME_GET_LAST_QUANTIZER, &qp);
    if (rc_api_->ComputeQP(frame_params_) == aom::FrameDropDecision::kOk) {
      ASSERT_EQ(rc_api_->GetQP(), qp) << "at frame " << frame_cnt_ - 1;
      int encoder_lpf_level;
      encoder->Control(AOME_GET_LOOPFILTER_LEVEL, &encoder_lpf_level);
      aom::AV1LoopfilterLevel loopfilter_level = rc_api_->GetLoopfilterLevel();
      ASSERT_EQ(loopfilter_level.filter_level[0], encoder_lpf_level);
      aom::AV1CdefInfo cdef_level = rc_api_->GetCdefInfo();
      int cdef_y_strengths[16];
      encoder->Control(AV1E_GET_LUMA_CDEF_STRENGTH, cdef_y_strengths);
      ASSERT_EQ(cdef_level.cdef_strength_y, cdef_y_strengths[0]);
    } else {
      num_drops_++;
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (layer_id_.spatial_layer_id == 0)
      rc_api_->PostEncodeUpdate(pkt->data.frame.sz - 2);
    else
      rc_api_->PostEncodeUpdate(pkt->data.frame.sz);
  }

  void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) override {
    (void)img1;
    (void)img2;
  }

  void RunOneLayer() {
    key_interval_ = 10000;
    SetConfig();
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunOneLayerScreen() {
    key_interval_ = 10000;
    SetConfig();
    rc_cfg_.is_screen = true;
    rc_cfg_.width = 352;
    rc_cfg_.height = 288;
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 140);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunOneLayerDropFramesCBR() {
    key_interval_ = 10000;
    max_consec_drop_ms_ = 250;
    frame_drop_thresh_ = 30;
    SetConfig();
    rc_cfg_.target_bandwidth = 100;
    cfg_.rc_target_bitrate = 100;
    rc_cfg_.max_quantizer = 50;
    cfg_.rc_max_quantizer = 50;
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    // Check that some frames were dropped, otherwise test has no value.
    ASSERT_GE(num_drops_, 1);
  }

  void RunOneLayerPeriodicKey() {
    key_interval_ = 100;
    SetConfig();
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunSvc() {
    key_interval_ = 10000;
    SetConfigSvc(3, 3);
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunSvcPeriodicKey() {
    key_interval_ = 100;
    SetConfigSvc(3, 3);
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunSvcDynamicTemporal() {
    dynamic_temporal_layers_ = true;
    key_interval_ = 10000;
    SetConfigSvc(3, 3);
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunSvcDynamicSpatial() {
    dynamic_spatial_layers_ = true;
    key_interval_ = 10000;
    SetConfigSvc(3, 3);
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480, 30,
                                         1, 0, kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  void SetConfig() {
    rc_cfg_.width = 640;
    rc_cfg_.height = 480;
    rc_cfg_.max_quantizer = 52;
    rc_cfg_.min_quantizer = 2;
    rc_cfg_.target_bandwidth = 1000;
    rc_cfg_.buf_initial_sz = 600;
    rc_cfg_.buf_optimal_sz = 600;
    rc_cfg_.buf_sz = 1000;
    rc_cfg_.undershoot_pct = 50;
    rc_cfg_.overshoot_pct = 50;
    rc_cfg_.max_intra_bitrate_pct = 1000;
    rc_cfg_.framerate = 30.0;
    rc_cfg_.ss_number_layers = 1;
    rc_cfg_.ts_number_layers = 1;
    rc_cfg_.scaling_factor_num[0] = 1;
    rc_cfg_.scaling_factor_den[0] = 1;
    rc_cfg_.layer_target_bitrate[0] = 1000;
    rc_cfg_.max_quantizers[0] = 52;
    rc_cfg_.min_quantizers[0] = 2;
    rc_cfg_.aq_mode = aq_mode_;
    rc_cfg_.frame_drop_thresh = frame_drop_thresh_;
    rc_cfg_.max_consec_drop_ms = max_consec_drop_ms_;

    // Encoder settings for ground truth.
    cfg_.g_w = 640;
    cfg_.g_h = 480;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_buf_initial_sz = 600;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 52;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    cfg_.rc_target_bitrate = 1000;
    cfg_.kf_min_dist = key_interval_;
    cfg_.kf_max_dist = key_interval_;
    cfg_.rc_dropframe_thresh = frame_drop_thresh_;
  }

  void SetConfigSvc(int number_spatial_layers, int number_temporal_layers) {
    rc_cfg_.width = 640;
    rc_cfg_.height = 480;
    rc_cfg_.max_quantizer = 56;
    rc_cfg_.min_quantizer = 2;
    rc_cfg_.buf_initial_sz = 600;
    rc_cfg_.buf_optimal_sz = 600;
    rc_cfg_.buf_sz = 1000;
    rc_cfg_.undershoot_pct = 50;
    rc_cfg_.overshoot_pct = 50;
    rc_cfg_.max_intra_bitrate_pct = 1000;
    rc_cfg_.framerate = 30.0;
    rc_cfg_.aq_mode = aq_mode_;
    rc_cfg_.ss_number_layers = number_spatial_layers;
    rc_cfg_.ts_number_layers = number_temporal_layers;

    // Encoder settings for ground truth.
    cfg_.g_w = 640;
    cfg_.g_h = 480;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_buf_initial_sz = 600;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_undershoot_pct = 50;
    cfg_.g_threads = 1;
    cfg_.kf_min_dist = key_interval_;
    cfg_.kf_max_dist = key_interval_;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 30;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    svc_params_.number_spatial_layers = number_spatial_layers;
    svc_params_.number_temporal_layers = number_temporal_layers;

    // Scale factors.
    if (number_spatial_layers == 3) {
      rc_cfg_.scaling_factor_num[0] = 1;
      rc_cfg_.scaling_factor_den[0] = 4;
      rc_cfg_.scaling_factor_num[1] = 2;
      rc_cfg_.scaling_factor_den[1] = 4;
      rc_cfg_.scaling_factor_num[2] = 4;
      rc_cfg_.scaling_factor_den[2] = 4;
      svc_params_.scaling_factor_num[0] = 1;
      svc_params_.scaling_factor_den[0] = 4;
      svc_params_.scaling_factor_num[1] = 2;
      svc_params_.scaling_factor_den[1] = 4;
      svc_params_.scaling_factor_num[2] = 4;
      svc_params_.scaling_factor_den[2] = 4;
    } else if (number_spatial_layers == 2) {
      rc_cfg_.scaling_factor_num[0] = 1;
      rc_cfg_.scaling_factor_den[0] = 2;
      rc_cfg_.scaling_factor_num[1] = 2;
      rc_cfg_.scaling_factor_den[1] = 2;
      svc_params_.scaling_factor_num[0] = 1;
      svc_params_.scaling_factor_den[0] = 2;
      svc_params_.scaling_factor_num[1] = 2;
      svc_params_.scaling_factor_den[1] = 2;
    } else if (number_spatial_layers == 1) {
      rc_cfg_.scaling_factor_num[0] = 1;
      rc_cfg_.scaling_factor_den[0] = 1;
      svc_params_.scaling_factor_num[0] = 1;
      svc_params_.scaling_factor_den[0] = 1;
    }

    // TS rate decimator.
    if (number_temporal_layers == 3) {
      rc_cfg_.ts_rate_decimator[0] = 4;
      rc_cfg_.ts_rate_decimator[1] = 2;
      rc_cfg_.ts_rate_decimator[2] = 1;
      svc_params_.framerate_factor[0] = 4;
      svc_params_.framerate_factor[1] = 2;
      svc_params_.framerate_factor[2] = 1;
    } else if (number_temporal_layers == 2) {
      rc_cfg_.ts_rate_decimator[0] = 2;
      rc_cfg_.ts_rate_decimator[1] = 1;
      svc_params_.framerate_factor[0] = 2;
      svc_params_.framerate_factor[1] = 1;
    } else if (number_temporal_layers == 1) {
      rc_cfg_.ts_rate_decimator[0] = 1;
      svc_params_.framerate_factor[0] = 1;
    }

    // Bitate.
    rc_cfg_.target_bandwidth = 0;
    cfg_.rc_target_bitrate = 0;
    for (int sl = 0; sl < number_spatial_layers; sl++) {
      int spatial_bitrate = 0;
      if (number_spatial_layers <= 3)
        spatial_bitrate = kSpatialLayerBitrate[sl];
      for (int tl = 0; tl < number_temporal_layers; tl++) {
        int layer = sl * number_temporal_layers + tl;
        if (number_temporal_layers == 3) {
          rc_cfg_.layer_target_bitrate[layer] =
              kTemporalRateAllocation3Layer[tl] * spatial_bitrate / 100;
          svc_params_.layer_target_bitrate[layer] =
              kTemporalRateAllocation3Layer[tl] * spatial_bitrate / 100;
        } else if (number_temporal_layers == 2) {
          rc_cfg_.layer_target_bitrate[layer] =
              kTemporalRateAllocation2Layer[tl] * spatial_bitrate / 100;
          svc_params_.layer_target_bitrate[layer] =
              kTemporalRateAllocation2Layer[tl] * spatial_bitrate / 100;
        } else if (number_temporal_layers == 1) {
          rc_cfg_.layer_target_bitrate[layer] = spatial_bitrate;
          svc_params_.layer_target_bitrate[layer] = spatial_bitrate;
        }
      }
      rc_cfg_.target_bandwidth += spatial_bitrate;
      cfg_.rc_target_bitrate += spatial_bitrate;
    }

    // Layer min/max quantizer.
    for (int sl = 0; sl < number_spatial_layers; ++sl) {
      for (int tl = 0; tl < number_temporal_layers; ++tl) {
        const int i = sl * number_temporal_layers + tl;
        rc_cfg_.max_quantizers[i] = rc_cfg_.max_quantizer;
        rc_cfg_.min_quantizers[i] = rc_cfg_.min_quantizer;
        svc_params_.max_quantizers[i] = cfg_.rc_max_quantizer;
        svc_params_.min_quantizers[i] = cfg_.rc_min_quantizer;
      }
    }
  }

  std::unique_ptr<aom::AV1RateControlRTC> rc_api_;
  aom::AV1RateControlRtcConfig rc_cfg_;
  int aq_mode_;
  int key_interval_;
  aom::AV1FrameParamsRTC frame_params_;
  bool encoder_exit_;
  aom_svc_params_t svc_params_;
  aom_svc_layer_id_t layer_id_;
  int layer_frame_cnt_;
  int superframe_cnt_;
  int frame_cnt_;
  bool dynamic_temporal_layers_;
  bool dynamic_spatial_layers_;
  int num_drops_;
  int max_consec_drop_ms_;
  int frame_drop_thresh_;
};

TEST_P(RcInterfaceTest, OneLayer) { RunOneLayer(); }

TEST_P(RcInterfaceTest, OneLayerDropFramesCBR) { RunOneLayerDropFramesCBR(); }

TEST_P(RcInterfaceTest, OneLayerPeriodicKey) { RunOneLayerPeriodicKey(); }

TEST_P(RcInterfaceTest, OneLayerScreen) { RunOneLayerScreen(); }

TEST_P(RcInterfaceTest, Svc) { RunSvc(); }

TEST_P(RcInterfaceTest, SvcPeriodicKey) { RunSvcPeriodicKey(); }

TEST_P(RcInterfaceTest, SvcDynamicTemporal) { RunSvcDynamicTemporal(); }

TEST_P(RcInterfaceTest, SvcDynamicSpatial) { RunSvcDynamicSpatial(); }

AV1_INSTANTIATE_TEST_SUITE(RcInterfaceTest, ::testing::Values(0, 3));

}  // namespace
