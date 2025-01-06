/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::webrtc::test::ExplicitKeyValueConfig;

constexpr Timestamp kInitialTime = Timestamp::Millis(123'456);

constexpr TimeDelta kMinBwePeriod = TimeDelta::Seconds(2);
constexpr TimeDelta kDefaultPeriod = TimeDelta::Seconds(3);
constexpr TimeDelta kMaxBwePeriod = TimeDelta::Seconds(50);

// After an overuse, we back off to 85% to the received bitrate.
constexpr double kFractionAfterOveruse = 0.85;

}  // namespace

TEST(AimdRateControlTest, MinNearMaxIncreaseRateOnLowBandwith) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(30'000), kInitialTime);
  EXPECT_EQ(aimd_rate_control.GetNearMaxIncreaseRateBpsPerSecond(), 4'000);
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn90kbpsAnd200msRtt) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(90'000), kInitialTime);
  EXPECT_EQ(aimd_rate_control.GetNearMaxIncreaseRateBpsPerSecond(), 5'000);
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn60kbpsAnd100msRtt) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(60'000), kInitialTime);
  aimd_rate_control.SetRtt(TimeDelta::Millis(100));
  EXPECT_EQ(aimd_rate_control.GetNearMaxIncreaseRateBpsPerSecond(), 5'000);
}

TEST(AimdRateControlTest, GetIncreaseRateAndBandwidthPeriod) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kBitrate = DataRate::BitsPerSec(300'000);
  aimd_rate_control.SetEstimate(kBitrate, kInitialTime);
  aimd_rate_control.Update({BandwidthUsage::kBwOverusing, kBitrate},
                           kInitialTime);
  EXPECT_NEAR(aimd_rate_control.GetNearMaxIncreaseRateBpsPerSecond(), 14'000,
              1'000);
  EXPECT_EQ(aimd_rate_control.GetExpectedBandwidthPeriod(), kDefaultPeriod);
}

TEST(AimdRateControlTest, BweLimitedByAckedBitrate) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kAckedBitrate = DataRate::BitsPerSec(10'000);
  Timestamp now = kInitialTime;
  aimd_rate_control.SetEstimate(kAckedBitrate, now);
  while (now - kInitialTime < TimeDelta::Seconds(20)) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, kAckedBitrate}, now);
    now += TimeDelta::Millis(100);
  }
  ASSERT_TRUE(aimd_rate_control.ValidEstimate());
  EXPECT_EQ(aimd_rate_control.LatestEstimate(),
            1.5 * kAckedBitrate + DataRate::BitsPerSec(10'000));
}

TEST(AimdRateControlTest, BweNotLimitedByDecreasingAckedBitrate) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kAckedBitrate = DataRate::BitsPerSec(10'000);
  Timestamp now = kInitialTime;
  aimd_rate_control.SetEstimate(kAckedBitrate, now);
  while (now - kInitialTime < TimeDelta::Seconds(20)) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, kAckedBitrate}, now);
    now += TimeDelta::Millis(100);
  }
  ASSERT_TRUE(aimd_rate_control.ValidEstimate());
  // If the acked bitrate decreases the BWE shouldn't be reduced to 1.5x
  // what's being acked, but also shouldn't get to increase more.
  DataRate prev_estimate = aimd_rate_control.LatestEstimate();
  aimd_rate_control.Update({BandwidthUsage::kBwNormal, kAckedBitrate / 2}, now);
  DataRate new_estimate = aimd_rate_control.LatestEstimate();
  EXPECT_EQ(new_estimate, prev_estimate);
  EXPECT_NEAR(new_estimate.bps(),
              (1.5 * kAckedBitrate + DataRate::BitsPerSec(10'000)).bps(),
              2'000);
}

TEST(AimdRateControlTest, DefaultPeriodUntilFirstOveruse) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  aimd_rate_control.SetStartBitrate(DataRate::KilobitsPerSec(300));
  EXPECT_EQ(aimd_rate_control.GetExpectedBandwidthPeriod(), kDefaultPeriod);
  aimd_rate_control.Update(
      {BandwidthUsage::kBwOverusing, DataRate::KilobitsPerSec(280)},
      kInitialTime);
  EXPECT_NE(aimd_rate_control.GetExpectedBandwidthPeriod(), kDefaultPeriod);
}

TEST(AimdRateControlTest, ExpectedPeriodAfterTypicalDrop) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  // The rate increase at 216 kbps should be 12 kbps. If we drop from
  // 216 + 4*12 = 264 kbps, it should take 4 seconds to recover. Since we
  // back off to 0.85*acked_rate-5kbps, the acked bitrate needs to be 260
  // kbps to end up at 216 kbps.
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(264'000);
  constexpr DataRate kUpdatedBitrate = DataRate::BitsPerSec(216'000);
  const DataRate kAckedBitrate =
      (kUpdatedBitrate + DataRate::BitsPerSec(5'000)) / kFractionAfterOveruse;
  Timestamp now = kInitialTime;
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  now += TimeDelta::Millis(100);
  aimd_rate_control.Update({BandwidthUsage::kBwOverusing, kAckedBitrate}, now);
  EXPECT_EQ(aimd_rate_control.LatestEstimate(), kUpdatedBitrate);
  EXPECT_EQ(aimd_rate_control.GetNearMaxIncreaseRateBpsPerSecond(), 12'000);
  EXPECT_EQ(aimd_rate_control.GetExpectedBandwidthPeriod(),
            TimeDelta::Seconds(4));
}

TEST(AimdRateControlTest, BandwidthPeriodIsNotBelowMin) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(10'000);
  Timestamp now = kInitialTime;
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  now += TimeDelta::Millis(100);
  // Make a small (1.5 kbps) bitrate drop to 8.5 kbps.
  aimd_rate_control.Update(
      {BandwidthUsage::kBwOverusing, kInitialBitrate - DataRate::BitsPerSec(1)},
      now);
  EXPECT_EQ(aimd_rate_control.GetExpectedBandwidthPeriod(), kMinBwePeriod);
}

TEST(AimdRateControlTest, BandwidthPeriodIsNotAboveMaxNoSmoothingExp) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(10'010'000);
  Timestamp now = kInitialTime;
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  now += TimeDelta::Millis(100);
  // Make a large (10 Mbps) bitrate drop to 10 kbps.
  const DataRate kAckedBitrate =
      DataRate::BitsPerSec(10'000) / kFractionAfterOveruse;
  aimd_rate_control.Update({BandwidthUsage::kBwOverusing, kAckedBitrate}, now);
  EXPECT_EQ(aimd_rate_control.GetExpectedBandwidthPeriod(), kMaxBwePeriod);
}

TEST(AimdRateControlTest, SendingRateBoundedWhenThroughputNotEstimated) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""));
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(123'000);
  Timestamp now = kInitialTime;
  aimd_rate_control.Update({BandwidthUsage::kBwNormal, kInitialBitrate}, now);
  // AimdRateControl sets the initial bit rate to what it receives after
  // five seconds has passed.
  // TODO(bugs.webrtc.org/9379): The comment in the AimdRateControl does not
  // match the constant.
  constexpr TimeDelta kInitializationTime = TimeDelta::Seconds(5);
  now += (kInitializationTime + TimeDelta::Millis(1));
  aimd_rate_control.Update({BandwidthUsage::kBwNormal, kInitialBitrate}, now);
  for (int i = 0; i < 100; ++i) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, std::nullopt}, now);
    now += TimeDelta::Millis(100);
  }
  EXPECT_LE(aimd_rate_control.LatestEstimate(),
            kInitialBitrate * 1.5 + DataRate::BitsPerSec(10'000));
}

TEST(AimdRateControlTest, EstimateDoesNotIncreaseInAlr) {
  // When alr is detected, the delay based estimator is not allowed to increase
  // bwe since there will be no feedback from the network if the new estimate
  // is correct.
  ExplicitKeyValueConfig field_trials(
      "WebRTC-DontIncreaseDelayBasedBweInAlr/Enabled/");
  AimdRateControl aimd_rate_control(field_trials, /*send_side=*/true);
  Timestamp now = kInitialTime;
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(123'000);
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  aimd_rate_control.SetInApplicationLimitedRegion(true);
  aimd_rate_control.Update({BandwidthUsage::kBwNormal, kInitialBitrate}, now);
  ASSERT_EQ(aimd_rate_control.LatestEstimate(), kInitialBitrate);

  for (int i = 0; i < 100; ++i) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, std::nullopt}, now);
    now += TimeDelta::Millis(100);
  }
  EXPECT_EQ(aimd_rate_control.LatestEstimate(), kInitialBitrate);
}

TEST(AimdRateControlTest, SetEstimateIncreaseBweInAlr) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-DontIncreaseDelayBasedBweInAlr/Enabled/");
  AimdRateControl aimd_rate_control(field_trials, /*send_side=*/true);
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(123'000);
  aimd_rate_control.SetEstimate(kInitialBitrate, kInitialTime);
  aimd_rate_control.SetInApplicationLimitedRegion(true);
  ASSERT_EQ(aimd_rate_control.LatestEstimate(), kInitialBitrate);
  aimd_rate_control.SetEstimate(2 * kInitialBitrate, kInitialTime);
  EXPECT_EQ(aimd_rate_control.LatestEstimate(), 2 * kInitialBitrate);
}

TEST(AimdRateControlTest, SetEstimateUpperLimitedByNetworkEstimate) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""),
                                    /*send_side=*/true);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(300'000), kInitialTime);
  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_upper = DataRate::BitsPerSec(400'000);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(500'000), kInitialTime);
  EXPECT_EQ(aimd_rate_control.LatestEstimate(),
            network_estimate.link_capacity_upper);
}

TEST(AimdRateControlTest,
     SetEstimateDefaultUpperLimitedByCurrentBitrateIfNetworkEstimateIsLow) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""),
                                    /*send_side=*/true);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(500'000), kInitialTime);
  ASSERT_EQ(aimd_rate_control.LatestEstimate(), DataRate::BitsPerSec(500'000));

  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_upper = DataRate::BitsPerSec(300'000);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(700'000), kInitialTime);
  EXPECT_EQ(aimd_rate_control.LatestEstimate(), DataRate::BitsPerSec(500'000));
}

TEST(AimdRateControlTest,
     SetEstimateNotUpperLimitedByCurrentBitrateIfNetworkEstimateIsLowIf) {
  AimdRateControl aimd_rate_control(
      ExplicitKeyValueConfig(
          "WebRTC-Bwe-EstimateBoundedIncrease/c_upper:false/"),
      /*send_side=*/true);

  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(500'000), kInitialTime);
  ASSERT_EQ(aimd_rate_control.LatestEstimate(), DataRate::BitsPerSec(500'000));

  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_upper = DataRate::BitsPerSec(300'000);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(700'000), kInitialTime);
  EXPECT_EQ(aimd_rate_control.LatestEstimate(), DataRate::BitsPerSec(300'000));
}

TEST(AimdRateControlTest, SetEstimateLowerLimitedByNetworkEstimate) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""),
                                    /*send_side=*/true);
  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_lower = DataRate::BitsPerSec(400'000);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);
  aimd_rate_control.SetEstimate(DataRate::BitsPerSec(100'000), kInitialTime);
  // 0.85 is default backoff factor. (`beta_`)
  EXPECT_EQ(aimd_rate_control.LatestEstimate(),
            network_estimate.link_capacity_lower * 0.85);
}

TEST(AimdRateControlTest,
     SetEstimateIgnoredIfLowerThanNetworkEstimateAndCurrent) {
  AimdRateControl aimd_rate_control(ExplicitKeyValueConfig(""),
                                    /*send_side=*/true);
  aimd_rate_control.SetEstimate(DataRate::KilobitsPerSec(200), kInitialTime);
  ASSERT_EQ(aimd_rate_control.LatestEstimate().kbps(), 200);
  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_lower = DataRate::KilobitsPerSec(400);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);
  // Ignore the next SetEstimate, since the estimate is lower than 85% of
  // the network estimate.
  aimd_rate_control.SetEstimate(DataRate::KilobitsPerSec(100), kInitialTime);
  EXPECT_EQ(aimd_rate_control.LatestEstimate().kbps(), 200);
}

TEST(AimdRateControlTest, EstimateIncreaseWhileNotInAlr) {
  // Allow the estimate to increase as long as alr is not detected to ensure
  // tha BWE can not get stuck at a certain bitrate.
  ExplicitKeyValueConfig field_trials(
      "WebRTC-DontIncreaseDelayBasedBweInAlr/Enabled/");
  AimdRateControl aimd_rate_control(field_trials, /*send_side=*/true);
  Timestamp now = kInitialTime;
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(123'000);
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  aimd_rate_control.SetInApplicationLimitedRegion(false);
  aimd_rate_control.Update({BandwidthUsage::kBwNormal, kInitialBitrate}, now);
  for (int i = 0; i < 100; ++i) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, std::nullopt}, now);
    now += TimeDelta::Millis(100);
  }
  EXPECT_GT(aimd_rate_control.LatestEstimate(), kInitialBitrate);
}

TEST(AimdRateControlTest, EstimateNotLimitedByNetworkEstimateIfDisabled) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-Bwe-EstimateBoundedIncrease/Disabled/");
  AimdRateControl aimd_rate_control(field_trials, /*send_side=*/true);
  Timestamp now = kInitialTime;
  constexpr DataRate kInitialBitrate = DataRate::BitsPerSec(123'000);
  aimd_rate_control.SetEstimate(kInitialBitrate, now);
  aimd_rate_control.SetInApplicationLimitedRegion(false);
  NetworkStateEstimate network_estimate;
  network_estimate.link_capacity_upper = DataRate::KilobitsPerSec(150);
  aimd_rate_control.SetNetworkStateEstimate(network_estimate);

  for (int i = 0; i < 100; ++i) {
    aimd_rate_control.Update({BandwidthUsage::kBwNormal, std::nullopt}, now);
    now += TimeDelta::Millis(100);
  }
  EXPECT_GT(aimd_rate_control.LatestEstimate(),
            network_estimate.link_capacity_upper);
}

}  // namespace webrtc
