/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/simulcast.h"

#include "media/base/media_constants.h"
#include "media/engine/constants.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kQpMax = 55;
constexpr double kBitratePriority = 2.0;
constexpr bool kScreenshare = true;
constexpr int kDefaultTemporalLayers = 3;  // Value from simulcast.cc.

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
}  // namespace

TEST(SimulcastTest, TotalMaxBitrateIsZeroForNoStreams) {
  std::vector<VideoStream> streams;
  EXPECT_EQ(0, cricket::GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, GetTotalMaxBitrateForSingleStream) {
  std::vector<VideoStream> streams(1);
  streams[0].max_bitrate_bps = 100000;
  EXPECT_EQ(100000, cricket::GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, GetTotalMaxBitrateForMultipleStreams) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;
  EXPECT_EQ(700000, cricket::GetTotalMaxBitrate(streams).bps());
}

TEST(SimulcastTest, BandwidthAboveTotalMaxBitrateGivenToHighestStream) {
  std::vector<VideoStream> streams(3);
  streams[0].target_bitrate_bps = 100000;
  streams[1].target_bitrate_bps = 200000;
  streams[2].max_bitrate_bps = 400000;

  const webrtc::DataRate one_bps = webrtc::DataRate::BitsPerSec(1);

  // No bitrate above the total max to give to the highest stream.
  const webrtc::DataRate max_total_bitrate =
      cricket::GetTotalMaxBitrate(streams);
  cricket::BoostMaxSimulcastLayer(max_total_bitrate, &streams);
  EXPECT_EQ(400000, streams[2].max_bitrate_bps);
  EXPECT_EQ(max_total_bitrate, cricket::GetTotalMaxBitrate(streams));

  // The bitrate above the total max should be given to the highest stream.
  cricket::BoostMaxSimulcastLayer(max_total_bitrate + one_bps, &streams);
  EXPECT_EQ(400000 + 1, streams[2].max_bitrate_bps);
  EXPECT_EQ(max_total_bitrate + one_bps, cricket::GetTotalMaxBitrate(streams));
}

TEST(SimulcastTest, GetConfig) {
  const std::vector<VideoStream> kExpected = GetSimulcastBitrates720p();

  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 1280, 720, kBitratePriority, kQpMax,
      !kScreenshare, true);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
  EXPECT_EQ(1280u, streams[2].width);
  EXPECT_EQ(720u, streams[2].height);

  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(size_t{kDefaultTemporalLayers}, streams[i].num_temporal_layers);
    EXPECT_EQ(cricket::kDefaultVideoMaxFramerate, streams[i].max_framerate);
    EXPECT_EQ(kQpMax, streams[i].max_qp);
    EXPECT_EQ(kExpected[i].min_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_EQ(kExpected[i].target_bitrate_bps, streams[i].target_bitrate_bps);
    EXPECT_EQ(kExpected[i].max_bitrate_bps, streams[i].max_bitrate_bps);
    EXPECT_TRUE(streams[i].active);
  }
  // Currently set on lowest stream.
  EXPECT_EQ(kBitratePriority, streams[0].bitrate_priority);
  EXPECT_FALSE(streams[1].bitrate_priority);
  EXPECT_FALSE(streams[2].bitrate_priority);
}

TEST(SimulcastTest, GetConfigWithBaseHeavyVP8TL3RateAllocation) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/");

  const std::vector<VideoStream> kExpected = GetSimulcastBitrates720p();

  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 1280, 720, kBitratePriority, kQpMax,
      !kScreenshare, true);

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
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 1280, 720, kBitratePriority, kQpMax,
      !kScreenshare, true);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(640u, streams[0].width);
  EXPECT_EQ(360u, streams[0].height);
  EXPECT_EQ(1280u, streams[1].width);
  EXPECT_EQ(720u, streams[1].height);
}

TEST(SimulcastTest, GetConfigWithLimitedMaxLayersForResolution) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-LegacySimulcastLayerLimit/Enabled/");
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 800, 600, kBitratePriority, kQpMax, !kScreenshare,
      true);

  EXPECT_EQ(2u, streams.size());
  EXPECT_EQ(400u, streams[0].width);
  EXPECT_EQ(300u, streams[0].height);
  EXPECT_EQ(800u, streams[1].width);
  EXPECT_EQ(600u, streams[1].height);
}

TEST(SimulcastTest, GetConfigWithLowResolutionScreenshare) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-LegacySimulcastLayerLimit/Enabled/");
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 100, 100,
                                  kBitratePriority, kQpMax, kScreenshare, true);

  // Simulcast streams number is never decreased for screenshare,
  // even for very low resolution.
  EXPECT_GT(streams.size(), 1u);
}

TEST(SimulcastTest, GetConfigWithNotLimitedMaxLayersForResolution) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-LegacySimulcastLayerLimit/Disabled/");
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 800, 600, kBitratePriority, kQpMax, !kScreenshare,
      true);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(200u, streams[0].width);
  EXPECT_EQ(150u, streams[0].height);
  EXPECT_EQ(400u, streams[1].width);
  EXPECT_EQ(300u, streams[1].height);
  EXPECT_EQ(800u, streams[2].width);
  EXPECT_EQ(600u, streams[2].height);
}

TEST(SimulcastTest, GetConfigWithNormalizedResolution) {
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 640 + 1, 360 + 1, kBitratePriority, kQpMax,
      !kScreenshare, true);

  // Must be divisible by |2 ^ (num_layers - 1)|.
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(320u, streams[0].width);
  EXPECT_EQ(180u, streams[0].height);
  EXPECT_EQ(640u, streams[1].width);
  EXPECT_EQ(360u, streams[1].height);
}

TEST(SimulcastTest, GetConfigWithNormalizedResolutionDivisibleBy4) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-2/");

  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 709, 501, kBitratePriority, kQpMax, !kScreenshare,
      true);

  // Must be divisible by |2 ^ 2|.
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(354u, streams[0].width);
  EXPECT_EQ(250u, streams[0].height);
  EXPECT_EQ(708u, streams[1].width);
  EXPECT_EQ(500u, streams[1].height);
}

TEST(SimulcastTest, GetConfigWithNormalizedResolutionDivisibleBy8) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-3/");

  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 709, 501, kBitratePriority, kQpMax, !kScreenshare,
      true);

  // Must be divisible by |2 ^ 3|.
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(352u, streams[0].width);
  EXPECT_EQ(248u, streams[0].height);
  EXPECT_EQ(704u, streams[1].width);
  EXPECT_EQ(496u, streams[1].height);
}

TEST(SimulcastTest, GetConfigForLegacyLayerLimit) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-LegacySimulcastLayerLimit/Enabled/");

  const size_t kMinLayers = 1;
  const int kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 320, 180, kBitratePriority, kQpMax, !kScreenshare,
      true);
  EXPECT_EQ(1u, streams.size());

  streams = cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 640, 360,
                                        kBitratePriority, kQpMax, !kScreenshare,
                                        true);
  EXPECT_EQ(2u, streams.size());

  streams = cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 1920, 1080,
                                        kBitratePriority, kQpMax, !kScreenshare,
                                        true);
  EXPECT_EQ(3u, streams.size());
}

TEST(SimulcastTest, GetConfigForLegacyLayerLimitWithRequiredHD) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-LegacySimulcastLayerLimit/Enabled/");

  const size_t kMinLayers = 3;  // "HD" layer must be present!
  const int kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 320, 180, kBitratePriority, kQpMax, !kScreenshare,
      true);
  EXPECT_EQ(3u, streams.size());

  streams = cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 640, 360,
                                        kBitratePriority, kQpMax, !kScreenshare,
                                        true);
  EXPECT_EQ(3u, streams.size());

  streams = cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 1920, 1080,
                                        kBitratePriority, kQpMax, !kScreenshare,
                                        true);
  EXPECT_EQ(3u, streams.size());
}

TEST(SimulcastTest, GetConfigForScreenshareSimulcast) {
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 1400, 800,
                                  kBitratePriority, kQpMax, kScreenshare, true);

  EXPECT_GT(streams.size(), 1u);
  for (size_t i = 0; i < streams.size(); ++i) {
    EXPECT_EQ(1400u, streams[i].width) << "Screen content never scaled.";
    EXPECT_EQ(800u, streams[i].height) << "Screen content never scaled.";
    EXPECT_EQ(kQpMax, streams[i].max_qp);
    EXPECT_TRUE(streams[i].active);
    EXPECT_GT(streams[i].num_temporal_layers, size_t{1});
    EXPECT_GT(streams[i].max_framerate, 0);
    EXPECT_GT(streams[i].min_bitrate_bps, 0);
    EXPECT_GT(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    EXPECT_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
  }
}

TEST(SimulcastTest, GetConfigForScreenshareSimulcastWithLimitedMaxLayers) {
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 1;
  std::vector<VideoStream> streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 1400, 800,
                                  kBitratePriority, kQpMax, kScreenshare, true);

  EXPECT_EQ(kMaxLayers, streams.size());
}

TEST(SimulcastTest, SimulcastScreenshareMaxBitrateAdjustedForResolution) {
  constexpr int kScreenshareHighStreamMinBitrateBps = 600000;
  constexpr int kScreenshareHighStreamMaxBitrateBps = 1250000;
  constexpr int kMaxBitrate960_540 = 1200000;

  // Normal case, max bitrate not limited by resolution.
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 2;
  std::vector<VideoStream> streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 1920, 1080,
                                  kBitratePriority, kQpMax, kScreenshare, true);
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(streams[1].max_bitrate_bps, kScreenshareHighStreamMaxBitrateBps);
  EXPECT_EQ(streams[1].min_bitrate_bps, kScreenshareHighStreamMinBitrateBps);
  EXPECT_GE(streams[1].max_bitrate_bps, streams[1].min_bitrate_bps);

  // At 960x540, the max bitrate is limited to 900kbps.
  streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 960, 540,
                                  kBitratePriority, kQpMax, kScreenshare, true);
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(streams[1].max_bitrate_bps, kMaxBitrate960_540);
  EXPECT_EQ(streams[1].min_bitrate_bps, kScreenshareHighStreamMinBitrateBps);
  EXPECT_GE(streams[1].max_bitrate_bps, streams[1].min_bitrate_bps);

  // At 480x270, the max bitrate is limited to 450kbps. This is lower than
  // the min bitrate, so use that as a lower bound.
  streams =
      cricket::GetSimulcastConfig(kMinLayers, kMaxLayers, 480, 270,
                                  kBitratePriority, kQpMax, kScreenshare, true);
  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(streams[1].max_bitrate_bps, kScreenshareHighStreamMinBitrateBps);
  EXPECT_EQ(streams[1].min_bitrate_bps, kScreenshareHighStreamMinBitrateBps);
  EXPECT_GE(streams[1].max_bitrate_bps, streams[1].min_bitrate_bps);
}

TEST(SimulcastTest, AveragesBitratesForNonStandardResolution) {
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, 900, 800, kBitratePriority, kQpMax, !kScreenshare,
      true);

  EXPECT_EQ(kMaxLayers, streams.size());
  EXPECT_EQ(900u, streams[2].width);
  EXPECT_EQ(800u, streams[2].height);
  EXPECT_EQ(1850000, streams[2].max_bitrate_bps);
  EXPECT_EQ(1850000, streams[2].target_bitrate_bps);
  EXPECT_EQ(475000, streams[2].min_bitrate_bps);
}

TEST(SimulcastTest, BitratesForCloseToStandardResolution) {
  const size_t kMinLayers = 1;
  const size_t kMaxLayers = 3;
  // Resolution very close to 720p in number of pixels
  const size_t kWidth = 1280;
  const size_t kHeight = 716;
  const std::vector<VideoStream> kExpectedNear = GetSimulcastBitrates720p();

  std::vector<VideoStream> streams = cricket::GetSimulcastConfig(
      kMinLayers, kMaxLayers, kWidth, kHeight, kBitratePriority, kQpMax,
      !kScreenshare, true);

  EXPECT_EQ(kMaxLayers, streams.size());
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

}  // namespace webrtc
