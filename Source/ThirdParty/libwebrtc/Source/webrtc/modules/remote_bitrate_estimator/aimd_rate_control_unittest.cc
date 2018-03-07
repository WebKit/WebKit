/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int64_t kClockInitialTime = 123456;

constexpr int kMinBwePeriodMsSmoothingExp = 500;
constexpr int kMinBwePeriodMsNoSmoothingExp = 2000;
constexpr int kDefaultPeriodMsNoSmoothingExp = 3000;
constexpr int kMaxBwePeriodMs = 50000;
constexpr char kSmoothingExpFieldTrial[] =
    "WebRTC-Audio-BandwidthSmoothing/Enabled/";

// After an overuse, we back off to 85% to the received bitrate.
constexpr double kFractionAfterOveruse = 0.85;

struct AimdRateControlStates {
  std::unique_ptr<AimdRateControl> aimd_rate_control;
  std::unique_ptr<SimulatedClock> simulated_clock;
};

AimdRateControlStates CreateAimdRateControlStates() {
  AimdRateControlStates states;
  states.aimd_rate_control.reset(new AimdRateControl());
  states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
  return states;
}

void UpdateRateControl(const AimdRateControlStates& states,
                       const BandwidthUsage& bandwidth_usage,
                       int bitrate,
                       int64_t now_ms) {
  RateControlInput input(bandwidth_usage, bitrate, now_ms);
  states.aimd_rate_control->Update(&input, now_ms);
}

}  // namespace

TEST(AimdRateControlTest, MinNearMaxIncreaseRateOnLowBandwith) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 30000;
  states.aimd_rate_control->SetEstimate(
      kBitrate, states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(4000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn90kbpsAnd200msRtt) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 90000;
  states.aimd_rate_control->SetEstimate(
      kBitrate, states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn60kbpsAnd100msRtt) {
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 60000;
  states.aimd_rate_control->SetEstimate(
      kBitrate, states.simulated_clock->TimeInMilliseconds());
  states.aimd_rate_control->SetRtt(100);
  EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
}

TEST(AimdRateControlTest, GetIncreaseRateAndBandwidthPeriod) {
  // Smoothing experiment disabled
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 300000;
  states.aimd_rate_control->SetEstimate(
      kBitrate, states.simulated_clock->TimeInMilliseconds());
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_NEAR(14000, states.aimd_rate_control->GetNearMaxIncreaseRateBps(),
              1000);
  EXPECT_EQ(kDefaultPeriodMsNoSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, GetIncreaseRateAndBandwidthPeriodSmoothingExp) {
  // Smoothing experiment enabled
  test::ScopedFieldTrials override_field_trials(kSmoothingExpFieldTrial);
  auto states = CreateAimdRateControlStates();
  constexpr int kBitrate = 300000;
  states.aimd_rate_control->SetEstimate(
      kBitrate, states.simulated_clock->TimeInMilliseconds());
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_NEAR(14000, states.aimd_rate_control->GetNearMaxIncreaseRateBps(),
              1000);
  EXPECT_EQ(kMinBwePeriodMsSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, BweLimitedByAckedBitrate) {
  auto states = CreateAimdRateControlStates();
  constexpr int kAckedBitrate = 10000;
  states.aimd_rate_control->SetEstimate(
      kAckedBitrate, states.simulated_clock->TimeInMilliseconds());
  while (states.simulated_clock->TimeInMilliseconds() - kClockInitialTime <
         20000) {
    UpdateRateControl(states, BandwidthUsage::kBwNormal, kAckedBitrate,
                      states.simulated_clock->TimeInMilliseconds());
    states.simulated_clock->AdvanceTimeMilliseconds(100);
  }
  ASSERT_TRUE(states.aimd_rate_control->ValidEstimate());
  EXPECT_EQ(static_cast<uint32_t>(1.5 * kAckedBitrate + 10000),
            states.aimd_rate_control->LatestEstimate());
}

TEST(AimdRateControlTest, BweNotLimitedByDecreasingAckedBitrate) {
  auto states = CreateAimdRateControlStates();
  constexpr int kAckedBitrate = 100000;
  states.aimd_rate_control->SetEstimate(
      kAckedBitrate, states.simulated_clock->TimeInMilliseconds());
  while (states.simulated_clock->TimeInMilliseconds() - kClockInitialTime <
         20000) {
    UpdateRateControl(states, BandwidthUsage::kBwNormal, kAckedBitrate,
                      states.simulated_clock->TimeInMilliseconds());
    states.simulated_clock->AdvanceTimeMilliseconds(100);
  }
  ASSERT_TRUE(states.aimd_rate_control->ValidEstimate());
  // If the acked bitrate decreases the BWE shouldn't be reduced to 1.5x
  // what's being acked, but also shouldn't get to increase more.
  uint32_t prev_estimate = states.aimd_rate_control->LatestEstimate();
  UpdateRateControl(states, BandwidthUsage::kBwNormal, kAckedBitrate / 2,
                    states.simulated_clock->TimeInMilliseconds());
  uint32_t new_estimate = states.aimd_rate_control->LatestEstimate();
  EXPECT_NEAR(new_estimate, static_cast<uint32_t>(1.5 * kAckedBitrate + 10000),
              2000);
  EXPECT_EQ(new_estimate, prev_estimate);
}

TEST(AimdRateControlTest, DefaultPeriodUntilFirstOveruse) {
  // Smoothing experiment disabled
  auto states = CreateAimdRateControlStates();
  states.aimd_rate_control->SetStartBitrate(300000);
  EXPECT_EQ(kDefaultPeriodMsNoSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, 280000,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_NE(kDefaultPeriodMsNoSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, MinPeriodUntilFirstOveruseSmoothingExp) {
  // Smoothing experiment enabled
  test::ScopedFieldTrials override_field_trials(kSmoothingExpFieldTrial);
  auto states = CreateAimdRateControlStates();
  states.aimd_rate_control->SetStartBitrate(300000);
  EXPECT_EQ(kMinBwePeriodMsSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, 280000,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_NE(kMinBwePeriodMsSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, ExpectedPeriodAfter20kbpsDropAnd5kbpsIncrease) {
  auto states = CreateAimdRateControlStates();
  constexpr int kInitialBitrate = 110000;
  states.aimd_rate_control->SetEstimate(
      kInitialBitrate, states.simulated_clock->TimeInMilliseconds());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  // Make the bitrate drop by 20 kbps to get to 90 kbps.
  // The rate increase at 90 kbps should be 5 kbps, so the period should be 4 s.
  constexpr int kAckedBitrate =
      (kInitialBitrate - 20000) / kFractionAfterOveruse;
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kAckedBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRateBps());
  EXPECT_EQ(4000, states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, MinPeriodAfterLargeBitrateDecreaseSmoothingExp) {
  // Smoothing experiment enabled
  test::ScopedFieldTrials override_field_trials(kSmoothingExpFieldTrial);
  auto states = CreateAimdRateControlStates();
  constexpr int kInitialBitrate = 110000;
  states.aimd_rate_control->SetEstimate(
      kInitialBitrate, states.simulated_clock->TimeInMilliseconds());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  // Make such a large drop in bitrate that should be treated as network
  // degradation.
  constexpr int kAckedBitrate = kInitialBitrate * 3 / 4 / kFractionAfterOveruse;
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kAckedBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(kMinBwePeriodMsSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, BandwidthPeriodIsNotBelowMin) {
  auto states = CreateAimdRateControlStates();
  constexpr int kInitialBitrate = 10000;
  states.aimd_rate_control->SetEstimate(
      kInitialBitrate, states.simulated_clock->TimeInMilliseconds());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  // Make a small (1.5 kbps) bitrate drop to 8.5 kbps.
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kInitialBitrate - 1,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(kMinBwePeriodMsNoSmoothingExp,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, BandwidthPeriodIsNotAboveMaxSmoothingExp) {
  // Smoothing experiment enabled
  test::ScopedFieldTrials override_field_trials(kSmoothingExpFieldTrial);
  auto states = CreateAimdRateControlStates();
  constexpr int kInitialBitrate = 50000000;
  states.aimd_rate_control->SetEstimate(
      kInitialBitrate, states.simulated_clock->TimeInMilliseconds());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  // Make a large (10 Mbps) bitrate drop to 10 kbps.
  constexpr int kAckedBitrate = 40000000 / kFractionAfterOveruse;
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kAckedBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(kMaxBwePeriodMs,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

TEST(AimdRateControlTest, BandwidthPeriodIsNotAboveMaxNoSmoothingExp) {
  auto states = CreateAimdRateControlStates();
  constexpr int kInitialBitrate = 10010000;
  states.aimd_rate_control->SetEstimate(
      kInitialBitrate, states.simulated_clock->TimeInMilliseconds());
  states.simulated_clock->AdvanceTimeMilliseconds(100);
  // Make a large (10 Mbps) bitrate drop to 10 kbps.
  constexpr int kAckedBitrate = 10000 / kFractionAfterOveruse;
  UpdateRateControl(states, BandwidthUsage::kBwOverusing, kAckedBitrate,
                    states.simulated_clock->TimeInMilliseconds());
  EXPECT_EQ(kMaxBwePeriodMs,
            states.aimd_rate_control->GetExpectedBandwidthPeriodMs());
}

}  // namespace webrtc
