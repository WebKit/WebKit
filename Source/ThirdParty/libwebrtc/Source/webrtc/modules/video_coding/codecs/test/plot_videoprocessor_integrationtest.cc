/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

namespace webrtc {
namespace test {

namespace {
// Codec settings.
const int kBitrates[] = {30, 50, 100, 200, 300, 500, 1000};
const int kFps[] = {30};
const bool kErrorConcealmentOn = false;
const bool kDenoisingOn = false;
const bool kFrameDropperOn = true;
const bool kSpatialResizeOn = false;
const bool kResilienceOn = false;
const VideoCodecType kVideoCodecType[] = {kVideoCodecVP8};
const bool kHwCodec = false;
const bool kUseSingleCore = true;

// Test settings.
const bool kBatchMode = true;

// Packet loss probability [0.0, 1.0].
const float kPacketLoss = 0.0f;

const VisualizationParams kVisualizationParams = {
    false,  // save_source_y4m
    false,  // save_encoded_ivf
    false,  // save_decoded_y4m
};

const bool kVerboseLogging = true;
}  // namespace

// Tests for plotting statistics from logs.
class PlotVideoProcessorIntegrationTest
    : public VideoProcessorIntegrationTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<int, int, VideoCodecType>> {
 protected:
  PlotVideoProcessorIntegrationTest()
      : bitrate_(::testing::get<0>(GetParam())),
        framerate_(::testing::get<1>(GetParam())),
        codec_type_(::testing::get<2>(GetParam())) {}

  virtual ~PlotVideoProcessorIntegrationTest() {}

  void RunTest(int width, int height, const std::string& filename) {
    // Bitrate and frame rate profile.
    RateProfile rate_profile;
    SetRateProfile(&rate_profile,
                   0,  // update_index
                   bitrate_, framerate_,
                   0);  // frame_index_rate_update
    rate_profile.frame_index_rate_update[1] = kNumFramesLong + 1;
    rate_profile.num_frames = kNumFramesLong;

    // Codec/network settings.
    CodecParams process_settings;
    SetCodecParams(
        &process_settings, codec_type_, kHwCodec, kUseSingleCore, kPacketLoss,
        -1,  // key_frame_interval
        1,   // num_temporal_layers
        kErrorConcealmentOn, kDenoisingOn, kFrameDropperOn, kSpatialResizeOn,
        kResilienceOn, width, height, filename, kVerboseLogging, kBatchMode);

    // Use default thresholds for quality (PSNR and SSIM).
    QualityThresholds quality_thresholds;

    // Use very loose thresholds for rate control, so even poor HW codecs will
    // pass the requirements.
    RateControlThresholds rc_thresholds[1];
    // clang-format off
    SetRateControlThresholds(
      rc_thresholds,
      0,                   // update_index
      kNumFramesLong + 1,  // max_num_dropped_frames
      10000,               // max_key_frame_size_mismatch
      10000,               // max_delta_frame_size_mismatch
      10000,               // max_encoding_rate_mismatch
      kNumFramesLong + 1,  // max_time_hit_target
      0,                   // num_spatial_resizes
      1);                  // num_key_frames
    // clang-format on

    ProcessFramesAndVerify(quality_thresholds, rate_profile, process_settings,
                           rc_thresholds, &kVisualizationParams);
  }

  const int bitrate_;
  const int framerate_;
  const VideoCodecType codec_type_;
};

INSTANTIATE_TEST_CASE_P(
    CodecSettings,
    PlotVideoProcessorIntegrationTest,
    ::testing::Combine(::testing::ValuesIn(kBitrates),
                       ::testing::ValuesIn(kFps),
                       ::testing::ValuesIn(kVideoCodecType)));

TEST_P(PlotVideoProcessorIntegrationTest, Process128x96) {
  RunTest(128, 96, "foreman_128x96");
}

TEST_P(PlotVideoProcessorIntegrationTest, Process160x120) {
  RunTest(160, 120, "foreman_160x120");
}

TEST_P(PlotVideoProcessorIntegrationTest, Process176x144) {
  RunTest(176, 144, "foreman_176x144");
}

TEST_P(PlotVideoProcessorIntegrationTest, Process320x240) {
  RunTest(320, 240, "foreman_320x240");
}

TEST_P(PlotVideoProcessorIntegrationTest, Process352x288) {
  RunTest(352, 288, "foreman_cif");
}

}  // namespace test
}  // namespace webrtc
