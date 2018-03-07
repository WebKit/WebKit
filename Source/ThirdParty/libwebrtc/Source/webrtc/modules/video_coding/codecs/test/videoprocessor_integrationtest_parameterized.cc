/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

// Loop variables.
const int kBitrates[] = {500};
const VideoCodecType kVideoCodecType[] = {kVideoCodecVP8};
const bool kHwCodec[] = {false};

// Codec settings.
const bool kResilienceOn = false;
const int kNumTemporalLayers = 1;
const bool kDenoisingOn = false;
const bool kErrorConcealmentOn = false;
const bool kSpatialResizeOn = false;
const bool kFrameDropperOn = false;

// Test settings.
const bool kUseSingleCore = false;
const bool kMeasureCpu = false;
const VisualizationParams kVisualizationParams = {
    false,  // save_encoded_ivf
    false,  // save_decoded_y4m
};

const int kNumFrames = 30;

}  // namespace

// Tests for plotting statistics from logs.
class VideoProcessorIntegrationTestParameterized
    : public VideoProcessorIntegrationTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<int, VideoCodecType, bool>> {
 protected:
  VideoProcessorIntegrationTestParameterized()
      : bitrate_(::testing::get<0>(GetParam())),
        codec_type_(::testing::get<1>(GetParam())),
        hw_codec_(::testing::get<2>(GetParam())) {}
  ~VideoProcessorIntegrationTestParameterized() override = default;

  void RunTest(int width,
               int height,
               int framerate,
               const std::string& filename) {
    config_.filename = filename;
    config_.input_filename = ResourcePath(filename, "yuv");
    config_.output_filename =
        TempFilename(OutputPath(), "plot_videoprocessor_integrationtest");
    config_.use_single_core = kUseSingleCore;
    config_.measure_cpu = kMeasureCpu;
    config_.hw_encoder = hw_codec_;
    config_.hw_decoder = hw_codec_;
    config_.num_frames = kNumFrames;
    config_.SetCodecSettings(codec_type_, kNumTemporalLayers,
                             kErrorConcealmentOn, kDenoisingOn, kFrameDropperOn,
                             kSpatialResizeOn, kResilienceOn, width, height);

    std::vector<RateProfile> rate_profiles = {
        {bitrate_, framerate, kNumFrames + 1}};

    ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr, nullptr,
                                &kVisualizationParams);
  }

  const int bitrate_;
  const VideoCodecType codec_type_;
  const bool hw_codec_;
};

INSTANTIATE_TEST_CASE_P(CodecSettings,
                        VideoProcessorIntegrationTestParameterized,
                        ::testing::Combine(::testing::ValuesIn(kBitrates),
                                           ::testing::ValuesIn(kVideoCodecType),
                                           ::testing::ValuesIn(kHwCodec)));

TEST_P(VideoProcessorIntegrationTestParameterized, Process_128x96_30fps) {
  RunTest(128, 96, 30, "foreman_128x96");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Process_160x120_30fps) {
  RunTest(160, 120, 30, "foreman_160x120");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Process_176x144_30fps) {
  RunTest(176, 144, 30, "foreman_176x144");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Process_320x240_30fps) {
  RunTest(320, 240, 30, "foreman_320x240");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Process_352x288_30fps) {
  RunTest(352, 288, 30, "foreman_cif");
}

}  // namespace test
}  // namespace webrtc
