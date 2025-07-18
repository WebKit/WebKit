/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/config/simulcast.h"

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

#include "api/units/data_rate.h"
#include "api/video/resolution.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "media/base/media_constants.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {
using test::ExplicitKeyValueConfig;
using ::testing::Combine;
using ::testing::SizeIs;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;

constexpr bool kScreenshare = true;
constexpr int kDefaultTemporalLayers = 3;      // Value from simulcast.cc.
constexpr int kDefaultH265TemporalLayers = 1;  // Value from simulcast.cc.

// Values from kSimulcastConfigs in simulcast.cc.
const std::vector<VideoStream> GetSimulcastBitrates720p() {
  std::vector<VideoStream> streams(3);
  streams[0].min_bitrate_bps = 30000;
  streams[0].target_bitrate_bps = 150000;
  streams[0].max_bitrate_bps = 200000;
  streams[1].min_bitrate_bps = 150000;
  streams[1].target_bitrate_bps = 500000;
  streams[1].max_bitrate_bps = 700000;
  streams[2].min_bitrate_bps = 600000;
  streams[2].target_bitrate_bps = 2500000;
  streams[2].max_bitrate_bps = 2500000;
  return streams;
}

// Values from kSimulcastFormatsH265 in simulcast.cc
const std::vector<VideoStream> GetH265SimulcastBitrates720p() {
  std::vector<VideoStream> streams(3);
  streams[0].min_bitrate_bps = 30'000;
  streams[0].target_bitrate_bps = 142'000;
  streams[0].max_bitrate_bps = 142'000;
  streams[1].min_bitrate_bps = 193'000;
  streams[1].target_bitrate_bps = 420'000;
  streams[1].max_bitrate_bps = 420'000;
  streams[2].min_bitrate_bps = 481'000;
  streams[2].target_bitrate_bps = 1'524'000;
  streams[2].max_bitrate_bps = 1'524'000;
  return streams;
}

// Creates a vector of resolutions scaled down with 1/2 factor ordered from low
// to high.
std::vector<Resolution> CreateResolutions(int max_width,
                                          int max_height,
                                          int num_streams) {
  std::vector<Resolution> resolutions(num_streams);
  for (int i = 0; i < num_streams; ++i) {
    resolutions[i].width = max_width >> (num_streams - i - 1);
    resolutions[i].height = max_height >> (num_streams - i - 1);
  }
  return resolutions;
}
}  // namespace

TEST(SimulcastTest, TotalMaxBitrateIsZeroForNoStreams) {
  std::vector<VideoStream> streams;
  EXPECT_EQ(0, GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, GetTotalMaxBitrateForSingleStream) {
  std::vector<VideoStream> streams(1);
  streams[0].max_bitrate_bps = 100000;
  EXPECT_EQ(100000, GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, GetTotalMaxBitrateForMultipleStreams) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;
  EXPECT_EQ(700000, GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, BandwidthAboveTotalMaxBitrateGivenToHighestStream) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;

  const DataRate one_bps = DataRate::BitsPerSec(1);

  // No bitrate above the total max to give to the highest stream.
  const DataRate max_total_bitrate = GetTotalMaxBitrate(streams);
  BoostMaxSimulcastLayer(max_total_bitrate, &streams);
  EXPECT_EQ(400000, streams[2].max_bitrate_bps);
  EXPECT_EQ(max_total_bitrate, GetTotalMaxBitrate(streams));

  // The bitrate above the total max should be given to the highest stream.
  BoostMaxSimulcastLayer(max_total_bitrate + one_bps, &streams);
  EXPECT_EQ(400000 + 1, streams[2].max_bitrate_bps);
  EXPECT_EQ(max_total_bitrate + one_bps, GetTotalMaxBitrate(streams));
}

TEST(SimulcastTest, GetConfig) {
  const ExplicitKeyValueConfig trials("");

  const std::vector<VideoStream> kExpected = GetSimulcastBitrates720p();

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(1280, 720, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
  EXPECT_EQ(1280u, streams[2].width);
  EXPECT_EQ(720u, streams[2].height);

  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(size_t{kDefaultTemporalLayers}, streams[i].num_temporal_layers);
    EXPECT_EQ(kDefaultVideoMaxFramerate, streams[i].max_framerate);
    EXPECT_EQ(-1, streams[i].max_qp);
    EXPECT_EQ(kExpected[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kExpected[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kExpected[i].max_bitrate_bps, streams[i].max_bitrate_bps);
    EXPECT_TRUE(streams[i].active);
  }
}

TEST(SimulcastTest, GetConfigH265) {
  const ExplicitKeyValueConfig trials("");
  const std::vector<VideoStream> kExpected = GetH265SimulcastBitrates720p();

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(1280, 720, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecH265);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
  EXPECT_EQ(1280u, streams[2].width);
  EXPECT_EQ(720u, streams[2].height);

  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(size_t{kDefaultH265TemporalLayers},
              streams[i].num_temporal_layers);
    EXPECT_EQ(kDefaultVideoMaxFramerate, streams[i].max_framerate);
    EXPECT_EQ(-1, streams[i].max_qp);
    EXPECT_EQ(kExpected[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kExpected[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kExpected[i].max_bitrate_bps, streams[i].max_bitrate_bps);
    EXPECT_TRUE(streams[i].active);
  }
}

TEST(SimulcastTest, GetConfigWithBaseHeavyVP8TL3RateAllocation) {
  ExplicitKeyValueConfig trials(
      "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/");

  const std::vector<VideoStream> kExpected = GetSimulcastBitrates720p();

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(1280, 720, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(kExpected[0].min_bitrate_bps, streams[0].min_bitrate_bps);
  EXPECT_EQ(static_cast<int>(0.4 * kExpected[0].target_bitrate_bps / 0.6),
            streams[0].target_bitrate_bps);
  EXPECT_EQ(static_cast<int>(0.4 * kExpected[0].max_bitrate_bps / 0.6),
            streams[0].max_bitrate_bps);
  for (size_t i = 1; i < streams.size(); ++i) {
    EXPECT_EQ(kExpected[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kExpected[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kExpected[i].max_bitrate_bps, streams[i].max_bitrate_bps);
  }
}

TEST(SimulcastTest, GetConfigWithLimitedMaxLayers) {
  ExplicitKeyValueConfig trials("");

  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(1280, 720, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(640u, streams[0].width);
  EXPECT_EQ(360u, streams[0].height);
  EXPECT_EQ(1280u, streams[1].width);
  EXPECT_EQ(720u, streams[1].height);
}

TEST(SimulcastTest, GetConfigForScreenshareSimulcast) {
  ExplicitKeyValueConfig trials("");
  std::vector<VideoStream> streams = GetSimulcastConfig(
      std::vector<Resolution>{{.width = 1400, .height = 800},
                              {.width = 1400, .height = 800},
                              {.width = 1400, .height = 800}},
      kScreenshare, true, trials, kVideoCodecVP8);

  EXPECT_THAT(streams, SizeIs(2));
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(1400u, streams[i].width) << "Screen content never scaled.";
    EXPECT_EQ(800u, streams[i].height) << "Screen content never scaled.";
    EXPECT_EQ(-1, streams[i].max_qp);
    EXPECT_TRUE(streams[i].active);
    EXPECT_GT(streams[i].num_temporal_layers, size_t{1});
    EXPECT_GT(streams[i].max_framerate, 0);
    EXPECT_GT(streams[i].min_bitrate_bps, 0);
    EXPECT_GT(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
  }
}

TEST(SimulcastTest, GetConfigForScreenshareSimulcastWithLimitedMaxLayers) {
  ExplicitKeyValueConfig trials("");
  std::vector<VideoStream> streams = GetSimulcastConfig(
      std::vector<Resolution>{{.width = 1400, .height = 800}}, kScreenshare,
      true, trials, kVideoCodecVP8);
  EXPECT_THAT(streams, SizeIs(1));
}

TEST(SimulcastTest, AveragesBitratesForNonStandardResolution) {
  ExplicitKeyValueConfig trials("");
  std::vector<VideoStream> streams =
      GetSimulcastConfig(std::vector<Resolution>{{.width = 900, .height = 800}},
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(1));
  EXPECT_EQ(900u, streams[0].width);
  EXPECT_EQ(800u, streams[0].height);
  EXPECT_EQ(1850000, streams[0].max_bitrate_bps);
  EXPECT_EQ(1850000, streams[0].target_bitrate_bps);
  EXPECT_EQ(475000, streams[0].min_bitrate_bps);
}

TEST(SimulcastTest, BitratesForCloseToStandardResolution) {
  ExplicitKeyValueConfig trials("");

  const size_t kMaxLayers = 3;
  // Resolution very close to 720p in number of pixels
  const size_t kWidth = 1280;
  const size_t kHeight = 716;
  const std::vector<VideoStream> kExpectedNear = GetSimulcastBitrates720p();

  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(kWidth, kHeight, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(kWidth, streams[2].width);
  EXPECT_EQ(kHeight, streams[2].height);
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_NEAR(kExpectedNear[i].max_bitrate_bps, streams[i].max_bitrate_bps,
                20000);
    EXPECT_NEAR(kExpectedNear[i].target_bitrate_bps,
                streams[i].target_bitrate_bps, 20000);
    EXPECT_NEAR(kExpectedNear[i].min_bitrate_bps, streams[i].min_bitrate_bps,
                20000);
  }
}

TEST(SimulcastTest, MaxLayersWithRoundUpDisabled) {
  ExplicitKeyValueConfig trials(
      "WebRTC-SimulcastLayerLimitRoundUp/max_ratio:0.0/");

  const size_t kMinLayers = 1;
  const int kMaxLayers = 3;

  size_t num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 960, 540,
                                               trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 3u);
  // <960x540: 2 layers
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 960, 539,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 270,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  // <480x270: 1 layer
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 269,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 1u);
}

TEST(SimulcastTest, MaxLayersWithDefaultRoundUpRatio) {
  // Default: "WebRTC-SimulcastLayerLimitRoundUp/max_ratio:0.1/"
  ExplicitKeyValueConfig trials("");
  const size_t kMinLayers = 1;
  const int kMaxLayers = 3;

  size_t num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 960, 540,
                                               trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 3u);
  // Lowest cropped height where max layers from higher resolution is used.
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 960, 512,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 3u);
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 960, 508,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 270,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  // Lowest cropped height where max layers from higher resolution is used.
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 256,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 254,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 1u);
}

TEST(SimulcastTest, MaxLayersWithRoundUpRatio) {
  ExplicitKeyValueConfig trials(
      "WebRTC-SimulcastLayerLimitRoundUp/max_ratio:0.13/");

  const size_t kMinLayers = 1;
  const int kMaxLayers = 3;

  size_t num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 270,
                                               trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  // Lowest cropped height where max layers from higher resolution is used.
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 252,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 2u);
  num_layers = LimitSimulcastLayerCount(kMinLayers, kMaxLayers, 480, 250,
                                        trials, kVideoCodecVP8);
  EXPECT_EQ(num_layers, 1u);
}

TEST(SimulcastTest, BitratesInterpolatedForResBelow180p) {
  // TODO(webrtc:12415): Remove when feature launches.
  ExplicitKeyValueConfig trials(
      "WebRTC-LowresSimulcastBitrateInterpolation/Enabled/");

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = GetSimulcastConfig(
      CreateResolutions(/*max_width=*/960, /*max_height=*/540, kMaxLayers),
      !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(240u, streams[0].width);
  EXPECT_EQ(135u, streams[0].height);
  EXPECT_EQ(streams[0].max_bitrate_bps, 112500);
  EXPECT_EQ(streams[0].target_bitrate_bps, 84375);
  EXPECT_EQ(streams[0].min_bitrate_bps, 30000);
}

TEST(SimulcastTest, BitratesConsistentForVerySmallRes) {
  // TODO(webrtc:12415): Remove when feature launches.
  ExplicitKeyValueConfig trials(
      "WebRTC-LowresSimulcastBitrateInterpolation/Enabled/");

  std::vector<VideoStream> streams =
      GetSimulcastConfig(std::vector<Resolution>{{.width = 1, .height = 1}},
                         !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(1));
  EXPECT_EQ(1u, streams[0].width);
  EXPECT_EQ(1u, streams[0].height);
  EXPECT_EQ(streams[0].max_bitrate_bps, 30000);
  EXPECT_EQ(streams[0].target_bitrate_bps, 30000);
  EXPECT_EQ(streams[0].min_bitrate_bps, 30000);
}

TEST(SimulcastTest,
     BitratesNotInterpolatedForResBelow180pWhenDisabledTrialSet) {
  ExplicitKeyValueConfig trials(
      "WebRTC-LowresSimulcastBitrateInterpolation/Disabled/");

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = GetSimulcastConfig(
      CreateResolutions(/*max_width=*/960, /*max_height=*/540, kMaxLayers),
      !kScreenshare, true, trials, kVideoCodecVP8);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  EXPECT_EQ(240u, streams[0].width);
  EXPECT_EQ(135u, streams[0].height);
  EXPECT_EQ(streams[0].max_bitrate_bps, 200000);
  EXPECT_EQ(streams[0].target_bitrate_bps, 150000);
  EXPECT_EQ(streams[0].min_bitrate_bps, 30000);
}

struct BitrateLimitsTestParams {
  int width;
  int height;
  std::vector<int> expected_min_bitrate_kbps;
  std::vector<int> expected_max_bitrate_kbps;
};

using BitrateLimitsTest =
    TestWithParam<std::tuple<VideoCodecType, BitrateLimitsTestParams>>;

TEST_P(BitrateLimitsTest, VerifyBitrateLimits) {
  const auto codec_type = std::get<VideoCodecType>(GetParam());
  const auto test_params = std::get<BitrateLimitsTestParams>(GetParam());
  ExplicitKeyValueConfig trials("");
  std::vector<VideoStream> streams = GetSimulcastConfig(
      CreateResolutions(test_params.width, test_params.height,
                        /*num_streams=*/3),
      !kScreenshare,
      /*temporal_layers_supported=*/true, trials, codec_type);
  ASSERT_THAT(streams, SizeIs(3));
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(streams[i].min_bitrate_bps / 1000,
              test_params.expected_min_bitrate_kbps[i]);
    EXPECT_EQ(streams[i].max_bitrate_bps / 1000,
              test_params.expected_max_bitrate_kbps[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Vp8H264,
    BitrateLimitsTest,
    Combine(Values(kVideoCodecVP8, kVideoCodecH264),
            Values(BitrateLimitsTestParams{.width = 1920,
                                           .height = 1080,
                                           .expected_min_bitrate_kbps{150, 350,
                                                                      800},
                                           .expected_max_bitrate_kbps{450, 1200,
                                                                      5000}},
                   BitrateLimitsTestParams{
                       .width = 1280,
                       .height = 720,
                       .expected_min_bitrate_kbps{30, 150, 600},
                       .expected_max_bitrate_kbps{200, 700, 2500}},
                   BitrateLimitsTestParams{
                       .width = 960,
                       .height = 540,
                       .expected_min_bitrate_kbps{30, 150, 350},
                       .expected_max_bitrate_kbps{200, 450, 1200}})),
    [](const TestParamInfo<BitrateLimitsTest::ParamType>& info) {
      return CodecTypeToPayloadString(std::get<VideoCodecType>(info.param)) +
             std::to_string(
                 std::get<BitrateLimitsTestParams>(info.param).height);
    });

INSTANTIATE_TEST_SUITE_P(
    Av1Vp9H265,
    BitrateLimitsTest,
    Combine(
        Values(
#ifdef RTC_ENABLE_H265
            kVideoCodecH265,
#endif
            kVideoCodecAV1,
            kVideoCodecVP9),
        Values(
            BitrateLimitsTestParams{.width = 1920,
                                    .height = 1080,
                                    .expected_min_bitrate_kbps{121, 337, 769},
                                    .expected_max_bitrate_kbps{257, 879, 3367}},
            BitrateLimitsTestParams{.width = 1280,
                                    .height = 720,
                                    .expected_min_bitrate_kbps{30, 193, 481},
                                    .expected_max_bitrate_kbps{142, 420, 1524}},
            BitrateLimitsTestParams{
                .width = 960,
                .height = 540,
                .expected_min_bitrate_kbps{30, 121, 337},
                .expected_max_bitrate_kbps{101, 257, 879}})),
    [](const TestParamInfo<BitrateLimitsTest::ParamType>& info) {
      return CodecTypeToPayloadString(std::get<VideoCodecType>(info.param)) +
             std::to_string(
                 std::get<BitrateLimitsTestParams>(info.param).height);
    });

#ifdef RTC_ENABLE_H265
// Test that for H.265, the simulcast layers are created with the correct
// default temporal layers, before that is overrided by application settings.
TEST(SimulcastTest, GetConfigForH265) {
  const ExplicitKeyValueConfig trials("");

  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      GetSimulcastConfig(CreateResolutions(1280, 720, kMaxLayers),
                         !kScreenshare, true, trials, kVideoCodecH265);

  ASSERT_THAT(streams, SizeIs(kMaxLayers));
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(1ul, streams[i].num_temporal_layers);
  }
}
#endif

}  // namespace webrtc
