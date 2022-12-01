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

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

constexpr size_t kNumFrames = 250;

constexpr int kTemporalId[4] = { 0, 2, 1, 2 };

// Parameter: aq mode: 0 and 3
class RcInterfaceTest : public ::libaom_test::EncoderTest,
                        public ::libaom_test::CodecTestWithParam<int> {
 public:
  RcInterfaceTest()
      : EncoderTest(GET_PARAM(0)), aq_mode_(GET_PARAM(1)), key_interval_(3000),
        encoder_exit_(false), layer_frame_cnt_(0) {
    memset(&svc_params_, 0, sizeof(svc_params_));
    memset(&layer_id_, 0, sizeof(layer_id_));
  }

  ~RcInterfaceTest() override {}

 protected:
  void SetUp() override { InitializeConfig(::libaom_test::kRealTime); }

  int GetNumSpatialLayers() override { return rc_cfg_.ss_number_layers; }

  void PreEncodeFrameHook(libaom_test::VideoSource *video,
                          libaom_test::Encoder *encoder) override {
    const int use_svc =
        rc_cfg_.ss_number_layers > 1 || rc_cfg_.ts_number_layers > 1;
    encoder->Control(AV1E_SET_RTC_EXTERNAL_RC, 1);
    if (video->frame() == 0 && layer_frame_cnt_ == 0) {
      encoder->Control(AOME_SET_CPUUSED, 7);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      if (use_svc) encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
    }
    // SVC specific settings
    if (use_svc) {
      frame_params_.spatial_layer_id =
          layer_frame_cnt_ % rc_cfg_.ss_number_layers;
      frame_params_.temporal_layer_id = kTemporalId[video->frame() % 4];
      layer_id_.spatial_layer_id = frame_params_.spatial_layer_id;
      layer_id_.temporal_layer_id = frame_params_.temporal_layer_id;
      encoder->Control(AV1E_SET_SVC_LAYER_ID, &layer_id_);
    }
    frame_params_.frame_type = layer_frame_cnt_ % key_interval_ == 0
                                   ? aom::kKeyFrame
                                   : aom::kInterFrame;
    if (!use_svc && frame_params_.frame_type == aom::kInterFrame) {
      // Disable golden frame update.
      frame_flags_ |= AOM_EFLAG_NO_UPD_GF;
      frame_flags_ |= AOM_EFLAG_NO_UPD_ARF;
    }
    encoder_exit_ = video->frame() == kNumFrames;
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    if (encoder_exit_) {
      return;
    }
    layer_frame_cnt_++;
    int qp;
    encoder->Control(AOME_GET_LAST_QUANTIZER, &qp);
    rc_api_->ComputeQP(frame_params_);
    ASSERT_EQ(rc_api_->GetQP(), qp);
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
    SetConfig();
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0,
                                        kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  void RunSvc() {
    SetConfigSvc();
    rc_api_ = aom::AV1RateControlRTC::Create(rc_cfg_);
    frame_params_.spatial_layer_id = 0;
    frame_params_.temporal_layer_id = 0;

    ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0,
                                        kNumFrames);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  void SetConfig() {
    rc_cfg_.width = 1280;
    rc_cfg_.height = 720;
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

    // Encoder settings for ground truth.
    cfg_.g_w = 1280;
    cfg_.g_h = 720;
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
  }

  void SetConfigSvc() {
    rc_cfg_.width = 1280;
    rc_cfg_.height = 720;
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
    rc_cfg_.ss_number_layers = 3;
    rc_cfg_.ts_number_layers = 3;
    rc_cfg_.aq_mode = aq_mode_;

    rc_cfg_.scaling_factor_num[0] = 1;
    rc_cfg_.scaling_factor_den[0] = 4;
    rc_cfg_.scaling_factor_num[1] = 2;
    rc_cfg_.scaling_factor_den[1] = 4;
    rc_cfg_.scaling_factor_num[2] = 4;
    rc_cfg_.scaling_factor_den[2] = 4;

    rc_cfg_.ts_rate_decimator[0] = 4;
    rc_cfg_.ts_rate_decimator[1] = 2;
    rc_cfg_.ts_rate_decimator[2] = 1;

    rc_cfg_.layer_target_bitrate[0] = 100;
    rc_cfg_.layer_target_bitrate[1] = 140;
    rc_cfg_.layer_target_bitrate[2] = 200;
    rc_cfg_.layer_target_bitrate[3] = 250;
    rc_cfg_.layer_target_bitrate[4] = 350;
    rc_cfg_.layer_target_bitrate[5] = 500;
    rc_cfg_.layer_target_bitrate[6] = 450;
    rc_cfg_.layer_target_bitrate[7] = 630;
    rc_cfg_.layer_target_bitrate[8] = 900;

    for (int sl = 0; sl < rc_cfg_.ss_number_layers; ++sl) {
      for (int tl = 0; tl < rc_cfg_.ts_number_layers; ++tl) {
        const int i = sl * rc_cfg_.ts_number_layers + tl;
        rc_cfg_.max_quantizers[i] = 56;
        rc_cfg_.min_quantizers[i] = 2;
      }
    }

    // Encoder settings for ground truth.
    svc_params_.number_spatial_layers = 3;
    svc_params_.number_temporal_layers = 3;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 30;
    svc_params_.scaling_factor_num[0] = 72;
    svc_params_.scaling_factor_den[0] = 288;
    svc_params_.scaling_factor_num[1] = 144;
    svc_params_.scaling_factor_den[1] = 288;
    svc_params_.scaling_factor_num[2] = 288;
    svc_params_.scaling_factor_den[2] = 288;
    for (int i = 0; i < AOM_MAX_LAYERS; ++i) {
      svc_params_.max_quantizers[i] = 56;
      svc_params_.min_quantizers[i] = 2;
    }
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_error_resilient = 0;
    // 3 temporal layers
    svc_params_.framerate_factor[0] = 4;
    svc_params_.framerate_factor[1] = 2;
    svc_params_.framerate_factor[2] = 1;

    cfg_.rc_buf_initial_sz = 600;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.g_threads = 1;
    cfg_.kf_min_dist = key_interval_;
    cfg_.kf_max_dist = key_interval_;
    cfg_.rc_target_bitrate = 1000;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_undershoot_pct = 50;

    svc_params_.layer_target_bitrate[0] = 100;
    svc_params_.layer_target_bitrate[1] = 140;
    svc_params_.layer_target_bitrate[2] = 200;
    svc_params_.layer_target_bitrate[3] = 250;
    svc_params_.layer_target_bitrate[4] = 350;
    svc_params_.layer_target_bitrate[5] = 500;
    svc_params_.layer_target_bitrate[6] = 450;
    svc_params_.layer_target_bitrate[7] = 630;
    svc_params_.layer_target_bitrate[8] = 900;
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
};

TEST_P(RcInterfaceTest, OneLayer) { RunOneLayer(); }

TEST_P(RcInterfaceTest, Svc) { RunSvc(); }

AV1_INSTANTIATE_TEST_SUITE(RcInterfaceTest, ::testing::Values(0, 3));

}  // namespace
