/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/test_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

using ::testing::ElementsAre;

namespace webrtc {
namespace test {

namespace {
const int kNumTemporalLayers = 2;
}  // namespace

TEST(TestConfig, NumberOfCoresWithUseSingleCore) {
  TestConfig config;
  config.use_single_core = true;
  EXPECT_EQ(1, config.NumberOfCores());
}

TEST(TestConfig, NumberOfCoresWithoutUseSingleCore) {
  TestConfig config;
  config.use_single_core = false;
  EXPECT_GE(config.NumberOfCores(), 1);
}

TEST(TestConfig, NumberOfTemporalLayersIsOne) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecH264, &config.codec_settings);
  EXPECT_EQ(1, config.NumberOfTemporalLayers());
}

TEST(TestConfig, NumberOfTemporalLayers_Vp8) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.codec_settings.VP8()->numberOfTemporalLayers = kNumTemporalLayers;
  EXPECT_EQ(kNumTemporalLayers, config.NumberOfTemporalLayers());
}

TEST(TestConfig, NumberOfTemporalLayers_Vp9) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP9, &config.codec_settings);
  config.codec_settings.VP9()->numberOfTemporalLayers = kNumTemporalLayers;
  EXPECT_EQ(kNumTemporalLayers, config.NumberOfTemporalLayers());
}

TEST(TestConfig, TemporalLayersForFrame_OneLayer) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.codec_settings.VP8()->numberOfTemporalLayers = 1;
  EXPECT_EQ(0, config.TemporalLayerForFrame(0));
  EXPECT_EQ(0, config.TemporalLayerForFrame(1));
  EXPECT_EQ(0, config.TemporalLayerForFrame(2));
}

TEST(TestConfig, TemporalLayersForFrame_TwoLayers) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.codec_settings.VP8()->numberOfTemporalLayers = 2;
  EXPECT_EQ(0, config.TemporalLayerForFrame(0));
  EXPECT_EQ(1, config.TemporalLayerForFrame(1));
  EXPECT_EQ(0, config.TemporalLayerForFrame(2));
  EXPECT_EQ(1, config.TemporalLayerForFrame(3));
}

TEST(TestConfig, TemporalLayersForFrame_ThreeLayers) {
  TestConfig config;
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.codec_settings.VP8()->numberOfTemporalLayers = 3;
  EXPECT_EQ(0, config.TemporalLayerForFrame(0));
  EXPECT_EQ(2, config.TemporalLayerForFrame(1));
  EXPECT_EQ(1, config.TemporalLayerForFrame(2));
  EXPECT_EQ(2, config.TemporalLayerForFrame(3));
  EXPECT_EQ(0, config.TemporalLayerForFrame(4));
  EXPECT_EQ(2, config.TemporalLayerForFrame(5));
  EXPECT_EQ(1, config.TemporalLayerForFrame(6));
  EXPECT_EQ(2, config.TemporalLayerForFrame(7));
}

TEST(TestConfig, ForcedKeyFrameIntervalOff) {
  TestConfig config;
  config.keyframe_interval = 0;
  EXPECT_THAT(config.FrameTypeForFrame(0), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(1), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(2), ElementsAre(kVideoFrameDelta));
}

TEST(TestConfig, ForcedKeyFrameIntervalOn) {
  TestConfig config;
  config.keyframe_interval = 3;
  EXPECT_THAT(config.FrameTypeForFrame(0), ElementsAre(kVideoFrameKey));
  EXPECT_THAT(config.FrameTypeForFrame(1), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(2), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(3), ElementsAre(kVideoFrameKey));
  EXPECT_THAT(config.FrameTypeForFrame(4), ElementsAre(kVideoFrameDelta));
  EXPECT_THAT(config.FrameTypeForFrame(5), ElementsAre(kVideoFrameDelta));
}

TEST(TestConfig, ToString_Vp8) {
  TestConfig config;
  config.filename = "yuvfile";
  config.use_single_core = true;

  config.SetCodecSettings(kVideoCodecVP8, 2, true,  // error_concealment_on,
                          false,                    // denoising_on,
                          false,                    // frame_dropper_on,
                          true,                     // spatial_resize_on,
                          false,                    // resilience_on,
                          320, 180);
  config.codec_settings.startBitrate = 400;
  config.codec_settings.maxBitrate = 500;
  config.codec_settings.minBitrate = 70;
  config.codec_settings.maxFramerate = 35;
  config.codec_settings.qpMax = 66;
  config.codec_settings.VP8()->complexity = kComplexityNormal;
  config.codec_settings.VP8()->keyFrameInterval = 999;

  EXPECT_EQ(
      "\n Filename         : yuvfile"
      "\n # CPU cores used : 1"
      "\n General:"
      "\n  Codec type        : VP8"
      "\n  Start bitrate     : 400 kbps"
      "\n  Max bitrate       : 500 kbps"
      "\n  Min bitrate       : 70 kbps"
      "\n  Width             : 320"
      "\n  Height            : 180"
      "\n  Max frame rate    : 35"
      "\n  QPmax             : 66"
      "\n VP8 specific: "
      "\n  Complexity        : 0"
      "\n  Resilience        : 0"
      "\n  # temporal layers : 2"
      "\n  Denoising         : 0"
      "\n  Error concealment : 1"
      "\n  Automatic resize  : 1"
      "\n  Frame dropping    : 0"
      "\n  Key frame interval: 999\n",
      config.ToString());
}

TEST(TestConfig, FilenameWithParams) {
  TestConfig config;
  config.filename = "filename";
  webrtc::test::CodecSettings(kVideoCodecVP8, &config.codec_settings);
  config.hw_encoder = true;
  config.codec_settings.startBitrate = 400;
  EXPECT_EQ("filename_VP8_hw_400", config.FilenameWithParams());
}

}  // namespace test
}  // namespace webrtc
