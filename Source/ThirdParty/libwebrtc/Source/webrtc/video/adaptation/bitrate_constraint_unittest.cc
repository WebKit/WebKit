/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/bitrate_constraint.h"

#include <utility>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const VideoSourceRestrictions k360p{/*max_pixels_per_frame=*/640 * 360,
                                    /*target_pixels_per_frame=*/640 * 360,
                                    /*max_frame_rate=*/30};
const VideoSourceRestrictions k720p{/*max_pixels_per_frame=*/1280 * 720,
                                    /*target_pixels_per_frame=*/1280 * 720,
                                    /*max_frame_rate=*/30};

void FillCodecConfig(VideoCodec* video_codec,
                     VideoEncoderConfig* encoder_config,
                     int width_px,
                     int height_px,
                     std::vector<bool> active_flags) {
  size_t num_layers = active_flags.size();
  video_codec->codecType = kVideoCodecVP8;
  video_codec->numberOfSimulcastStreams = num_layers;

  encoder_config->number_of_streams = num_layers;
  encoder_config->simulcast_layers.resize(num_layers);

  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    int layer_width_px = width_px >> (num_layers - 1 - layer_idx);
    int layer_height_px = height_px >> (num_layers - 1 - layer_idx);

    video_codec->simulcastStream[layer_idx].active = active_flags[layer_idx];
    video_codec->simulcastStream[layer_idx].width = layer_width_px;
    video_codec->simulcastStream[layer_idx].height = layer_height_px;

    encoder_config->simulcast_layers[layer_idx].active =
        active_flags[layer_idx];
    encoder_config->simulcast_layers[layer_idx].width = layer_width_px;
    encoder_config->simulcast_layers[layer_idx].height = layer_height_px;
  }
}

constexpr int kStartBitrateBps720p = 1000000;

VideoEncoder::EncoderInfo MakeEncoderInfo() {
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.resolution_bitrate_limits = {
      {640 * 360, 500000, 0, 5000000},
      {1280 * 720, kStartBitrateBps720p, 0, 5000000},
      {1920 * 1080, 2000000, 0, 5000000}};
  return encoder_info;
}

}  // namespace

class BitrateConstraintTest : public ::testing::Test {
 public:
  BitrateConstraintTest()
      : frame_rate_provider_(), input_state_provider_(&frame_rate_provider_) {}

 protected:
  void OnEncoderSettingsUpdated(int width_px,
                                int height_px,
                                std::vector<bool> active_flags) {
    VideoCodec video_codec;
    VideoEncoderConfig encoder_config;
    FillCodecConfig(&video_codec, &encoder_config, width_px, height_px,
                    active_flags);

    EncoderSettings encoder_settings(MakeEncoderInfo(),
                                     std::move(encoder_config), video_codec);
    bitrate_constraint_.OnEncoderSettingsUpdated(encoder_settings);
    input_state_provider_.OnEncoderSettingsChanged(encoder_settings);
  }

  FakeFrameRateProvider frame_rate_provider_;
  VideoStreamInputStateProvider input_state_provider_;
  BitrateConstraint bitrate_constraint_;
};

TEST_F(BitrateConstraintTest, AdaptUpAllowedAtSinglecastIfBitrateIsEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{true});

  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p);

  EXPECT_TRUE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest,
       AdaptUpDisallowedAtSinglecastIfBitrateIsNotEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{true});

  // 1 bps less than needed for 720p.
  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p - 1);

  EXPECT_FALSE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest,
       AdaptUpAllowedAtSinglecastUpperLayerActiveIfBitrateIsEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{false, true});

  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p);

  EXPECT_TRUE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest,
       AdaptUpDisallowedAtSinglecastUpperLayerActiveIfBitrateIsNotEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{false, true});

  // 1 bps less than needed for 720p.
  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p - 1);

  EXPECT_FALSE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest,
       AdaptUpAllowedAtSinglecastLowestLayerActiveIfBitrateIsNotEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{true, false});

  // 1 bps less than needed for 720p.
  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p - 1);

  EXPECT_TRUE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest, AdaptUpAllowedAtSimulcastIfBitrateIsNotEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{true, true});

  // 1 bps less than needed for 720p.
  bitrate_constraint_.OnEncoderTargetBitrateUpdated(kStartBitrateBps720p - 1);

  EXPECT_TRUE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k720p));
}

TEST_F(BitrateConstraintTest,
       AdaptUpInFpsAllowedAtNoResolutionIncreaseIfBitrateIsNotEnough) {
  OnEncoderSettingsUpdated(/*width_px=*/640, /*height_px=*/360,
                           /*active_flags=*/{true});

  bitrate_constraint_.OnEncoderTargetBitrateUpdated(1);

  EXPECT_TRUE(bitrate_constraint_.IsAdaptationUpAllowed(
      input_state_provider_.InputState(),
      /*restrictions_before=*/k360p,
      /*restrictions_after=*/k360p));
}

}  // namespace webrtc
