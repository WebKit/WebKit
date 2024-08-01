/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/config/encoder_stream_factory.h"

#include <tuple>

#include "api/video_codecs/scalability_mode.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/experiments/min_video_bitrate_experiment.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::cricket::EncoderStreamFactory;
using test::ExplicitKeyValueConfig;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::Values;

struct CreateVideoStreamParams {
  int width = 0;
  int height = 0;
  int max_framerate_fps = -1;
  int min_bitrate_bps = -1;
  int target_bitrate_bps = -1;
  int max_bitrate_bps = -1;
  int scale_resolution_down_by = -1;
  std::optional<ScalabilityMode> scalability_mode;
};

// A helper function that creates `VideoStream` with given settings.
VideoStream CreateVideoStream(const CreateVideoStreamParams& params) {
  VideoStream stream;
  stream.width = params.width;
  stream.height = params.height;
  stream.max_framerate = params.max_framerate_fps;
  stream.min_bitrate_bps = params.min_bitrate_bps;
  stream.target_bitrate_bps = params.target_bitrate_bps;
  stream.max_bitrate_bps = params.max_bitrate_bps;
  stream.scale_resolution_down_by = params.scale_resolution_down_by;
  stream.scalability_mode = params.scalability_mode;
  return stream;
}

std::vector<Resolution> GetStreamResolutions(
    const std::vector<VideoStream>& streams) {
  std::vector<Resolution> res;
  for (const auto& s : streams) {
    res.push_back(
        {rtc::checked_cast<int>(s.width), rtc::checked_cast<int>(s.height)});
  }
  return res;
}

std::vector<VideoStream> CreateEncoderStreams(
    const FieldTrialsView& field_trials,
    const Resolution& resolution,
    const VideoEncoderConfig& encoder_config,
    absl::optional<VideoSourceRestrictions> restrictions = absl::nullopt) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory =
      rtc::make_ref_counted<EncoderStreamFactory>(encoder_info, restrictions);
  return factory->CreateEncoderStreams(field_trials, resolution.width,
                                       resolution.height, encoder_config);
}

}  // namespace

TEST(EncoderStreamFactory, SinglecastRequestedResolution) {
  ExplicitKeyValueConfig field_trials("");
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.resize(1);
  encoder_config.simulcast_layers[0].requested_resolution = {.width = 640,
                                                             .height = 360};
  auto streams = CreateEncoderStreams(
      field_trials, {.width = 1280, .height = 720}, encoder_config);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 640, .height = 360},
                                           }));
}

TEST(EncoderStreamFactory, SinglecastRequestedResolutionWithAdaptation) {
  ExplicitKeyValueConfig field_trials("");
  VideoSourceRestrictions restrictions(
      /* max_pixels_per_frame= */ (320 * 320),
      /* target_pixels_per_frame= */ absl::nullopt,
      /* max_frame_rate= */ absl::nullopt);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.resize(1);
  encoder_config.simulcast_layers[0].requested_resolution = {.width = 640,
                                                             .height = 360};
  auto streams =
      CreateEncoderStreams(field_trials, {.width = 1280, .height = 720},
                           encoder_config, restrictions);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 320, .height = 180},
                                           }));
}

TEST(EncoderStreamFactory, BitratePriority) {
  constexpr double kBitratePriority = 0.123;
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  encoder_config.bitrate_priority = kBitratePriority;
  auto streams = CreateEncoderStreams(
      /*field_trials=*/ExplicitKeyValueConfig(""),
      {.width = 640, .height = 360}, encoder_config);
  ASSERT_THAT(streams, SizeIs(2));
  EXPECT_EQ(streams[0].bitrate_priority, kBitratePriority);
  EXPECT_FALSE(streams[1].bitrate_priority);
}

TEST(EncoderStreamFactory, SetsMinBitrateToDefaultValue) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  auto streams = factory->CreateEncoderStreams(ExplicitKeyValueConfig(""), 1920,
                                               1080, encoder_config);
  ASSERT_THAT(streams, Not(IsEmpty()));
  EXPECT_EQ(streams[0].min_bitrate_bps, kDefaultMinVideoBitrateBps);
}

TEST(EncoderStreamFactory, SetsMinBitrateToExperimentalValue) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  auto streams = factory->CreateEncoderStreams(
      ExplicitKeyValueConfig("WebRTC-Video-MinVideoBitrate/Enabled,br:1kbps/"),
      1920, 1080, encoder_config);
  ASSERT_THAT(streams, Not(IsEmpty()));
  EXPECT_NE(streams[0].min_bitrate_bps, kDefaultMinVideoBitrateBps);
  EXPECT_EQ(streams[0].min_bitrate_bps, 1000);
}

struct StreamResolutionTestParams {
  absl::string_view field_trials;
  size_t number_of_streams = 1;
  Resolution resolution = {.width = 640, .height = 480};
  bool is_legacy_screencast = false;
  size_t first_active_layer_idx = 0;
};

std::vector<Resolution> CreateStreamResolutions(
    const StreamResolutionTestParams& test_params) {
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP8;
  encoder_config.number_of_streams = test_params.number_of_streams;
  encoder_config.simulcast_layers.resize(test_params.number_of_streams);
  for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
    encoder_config.simulcast_layers[i].active =
        (i >= test_params.first_active_layer_idx);
  }
  if (test_params.is_legacy_screencast) {
    encoder_config.content_type = VideoEncoderConfig::ContentType::kScreen;
    encoder_config.legacy_conference_mode = true;
  }
  return GetStreamResolutions(
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.resolution, encoder_config));
}

TEST(EncoderStreamFactory, KeepsResolutionUnchangedWhenAligned) {
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 2,
                               .resolution = {.width = 516, .height = 526}}),
      ElementsAre(Resolution{.width = 516 / 2, .height = 526 / 2},
                  Resolution{.width = 516, .height = 526}));
}

TEST(EncoderStreamFactory, AdjustsResolutionWhenUnaligned) {
  // By default width and height of the smallest simulcast stream are required
  // to be whole numbers. To achieve that, the resolution of the highest
  // simulcast stream is adjusted to be multiple of (2 ^ (number_of_streams -
  // 1)) by rounding down.
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 2,
                               .resolution = {.width = 515, .height = 517}}),
      ElementsAre(Resolution{.width = 514 / 2, .height = 516 / 2},
                  Resolution{.width = 514, .height = 516}));
}

TEST(EncoderStreamFactory, MakesResolutionDivisibleBy4) {
  EXPECT_THAT(
      CreateStreamResolutions(
          {.field_trials = "WebRTC-NormalizeSimulcastResolution/Enabled-2/",
           .number_of_streams = 2,
           .resolution = {.width = 515, .height = 517}}),
      ElementsAre(Resolution{.width = 512 / 2, .height = 516 / 2},
                  Resolution{.width = 512, .height = 516}));
}

TEST(EncoderStreamFactory, KeepsStreamCountUnchangedWhenResolutionIsHigh) {
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 3,
                               .resolution = {.width = 1000, .height = 1000}}),
      SizeIs(3));
}

TEST(EncoderStreamFactory, ReducesStreamCountWhenResolutionIsLow) {
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 3,
                               .resolution = {.width = 100, .height = 100}}),
      SizeIs(1));
}

TEST(EncoderStreamFactory, ReducesStreamCountDownToFirstActiveStream) {
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 3,
                               .resolution = {.width = 100, .height = 100},
                               .first_active_layer_idx = 1}),
      SizeIs(2));
}

TEST(EncoderStreamFactory,
     ReducesLegacyScreencastStreamCountWhenResolutionIsLow) {
  // At least 2 streams are expected to be configured in legacy screencast mode.
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 3,
                               .resolution = {.width = 100, .height = 100},
                               .is_legacy_screencast = true}),
      SizeIs(2));
}

TEST(EncoderStreamFactory, KeepsStreamCountUnchangedWhenLegacyLimitIsDisabled) {
  EXPECT_THAT(CreateStreamResolutions(
                  {.field_trials = "WebRTC-LegacySimulcastLayerLimit/Disabled/",
                   .number_of_streams = 3,
                   .resolution = {.width = 100, .height = 100}}),
              SizeIs(3));
}

TEST(EncoderStreamFactory, KeepsHighResolutionWhenStreamCountIsReduced) {
  EXPECT_THAT(
      CreateStreamResolutions({.number_of_streams = 3,
                               .resolution = {.width = 640, .height = 360}}),
      ElementsAre(Resolution{.width = 320, .height = 180},
                  Resolution{.width = 640, .height = 360}));
}

struct OverrideStreamSettingsTestParams {
  std::string field_trials;
  Resolution input_resolution;
  VideoEncoderConfig::ContentType content_type;
  std::vector<VideoStream> requested_streams;
  std::vector<VideoStream> expected_streams;
};

class EncoderStreamFactoryOverrideStreamSettinsTest
    : public ::testing::TestWithParam<
          std::tuple<OverrideStreamSettingsTestParams, VideoCodecType>> {};

TEST_P(EncoderStreamFactoryOverrideStreamSettinsTest, OverrideStreamSettings) {
  OverrideStreamSettingsTestParams test_params = std::get<0>(GetParam());
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = std::get<1>(GetParam());
  encoder_config.number_of_streams = test_params.requested_streams.size();
  encoder_config.simulcast_layers = test_params.requested_streams;
  encoder_config.content_type = test_params.content_type;
  auto streams =
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.input_resolution, encoder_config);
  ASSERT_EQ(streams.size(), test_params.expected_streams.size());
  for (size_t i = 0; i < streams.size(); ++i) {
    SCOPED_TRACE(i);
    const VideoStream& expected = test_params.expected_streams[i];
    EXPECT_EQ(streams[i].width, expected.width);
    EXPECT_EQ(streams[i].height, expected.height);
    EXPECT_EQ(streams[i].max_framerate, expected.max_framerate);
    EXPECT_EQ(streams[i].min_bitrate_bps, expected.min_bitrate_bps);
    EXPECT_EQ(streams[i].target_bitrate_bps, expected.target_bitrate_bps);
    EXPECT_EQ(streams[i].max_bitrate_bps, expected.max_bitrate_bps);
    EXPECT_EQ(streams[i].scalability_mode, expected.scalability_mode);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Screencast,
    EncoderStreamFactoryOverrideStreamSettinsTest,
    Combine(Values(OverrideStreamSettingsTestParams{
                .input_resolution = {.width = 1920, .height = 1080},
                .content_type = VideoEncoderConfig::ContentType::kScreen,
                .requested_streams =
                    {CreateVideoStream(
                         {.max_framerate_fps = 5,
                          .max_bitrate_bps = 420'000,
                          .scale_resolution_down_by = 1,
                          .scalability_mode = ScalabilityMode::kL1T2}),
                     CreateVideoStream(
                         {.max_framerate_fps = 30,
                          .max_bitrate_bps = 2'500'000,
                          .scale_resolution_down_by = 1,
                          .scalability_mode = ScalabilityMode::kL1T2})},
                .expected_streams =
                    {CreateVideoStream(
                         {.width = 1920,
                          .height = 1080,
                          .max_framerate_fps = 5,
                          .min_bitrate_bps = 30'000,
                          .target_bitrate_bps = 420'000,
                          .max_bitrate_bps = 420'000,
                          .scalability_mode = ScalabilityMode::kL1T2}),
                     CreateVideoStream(
                         {.width = 1920,
                          .height = 1080,
                          .max_framerate_fps = 30,
                          .min_bitrate_bps = 800'000,
                          .target_bitrate_bps = 2'500'000,
                          .max_bitrate_bps = 2'500'000,
                          .scalability_mode = ScalabilityMode::kL1T2})}}),
            Values(VideoCodecType::kVideoCodecVP8,
                   VideoCodecType::kVideoCodecAV1)));
}  // namespace webrtc
