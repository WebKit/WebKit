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
#include <utility>

#include "webrtc/modules/congestion_controller/probing_interval_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_aimd_rate_control.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::Return;

namespace webrtc {

namespace {

constexpr int kMinIntervalMs = 2000;
constexpr int kMaxIntervalMs = 50000;
constexpr int kDefaultIntervalMs = 3000;

struct ProbingIntervalEstimatorStates {
  std::unique_ptr<ProbingIntervalEstimator> probing_interval_estimator;
  std::unique_ptr<MockAimdRateControl> aimd_rate_control;
};

ProbingIntervalEstimatorStates CreateProbingIntervalEstimatorStates() {
  ProbingIntervalEstimatorStates states;
  states.aimd_rate_control.reset(new MockAimdRateControl());
  states.probing_interval_estimator.reset(new ProbingIntervalEstimator(
      kMinIntervalMs, kMaxIntervalMs, kDefaultIntervalMs,
      states.aimd_rate_control.get()));
  return states;
}
}  // namespace

TEST(ProbingIntervalEstimatorTest, DefaultIntervalUntillWeHaveDrop) {
  auto states = CreateProbingIntervalEstimatorStates();

  EXPECT_CALL(*states.aimd_rate_control, GetLastBitrateDecreaseBps())
      .WillOnce(Return(rtc::Optional<int>()))
      .WillOnce(Return(rtc::Optional<int>(5000)));
  EXPECT_CALL(*states.aimd_rate_control, GetNearMaxIncreaseRateBps())
      .WillOnce(Return(4000))
      .WillOnce(Return(4000));

  EXPECT_EQ(kDefaultIntervalMs,
            states.probing_interval_estimator->GetIntervalMs());
  EXPECT_NE(kDefaultIntervalMs,
            states.probing_interval_estimator->GetIntervalMs());
}

TEST(ProbingIntervalEstimatorTest, CalcInterval) {
  auto states = CreateProbingIntervalEstimatorStates();
  EXPECT_CALL(*states.aimd_rate_control, GetLastBitrateDecreaseBps())
      .WillOnce(Return(rtc::Optional<int>(20000)));
  EXPECT_CALL(*states.aimd_rate_control, GetNearMaxIncreaseRateBps())
      .WillOnce(Return(5000));
  EXPECT_EQ(4000, states.probing_interval_estimator->GetIntervalMs());
}

TEST(ProbingIntervalEstimatorTest, IntervalDoesNotExceedMin) {
  auto states = CreateProbingIntervalEstimatorStates();
  EXPECT_CALL(*states.aimd_rate_control, GetLastBitrateDecreaseBps())
      .WillOnce(Return(rtc::Optional<int>(1000)));
  EXPECT_CALL(*states.aimd_rate_control, GetNearMaxIncreaseRateBps())
      .WillOnce(Return(5000));
  EXPECT_EQ(kMinIntervalMs, states.probing_interval_estimator->GetIntervalMs());
}

TEST(ProbingIntervalEstimatorTest, IntervalDoesNotExceedMax) {
  auto states = CreateProbingIntervalEstimatorStates();
  EXPECT_CALL(*states.aimd_rate_control, GetLastBitrateDecreaseBps())
      .WillOnce(Return(rtc::Optional<int>(50000)));
  EXPECT_CALL(*states.aimd_rate_control, GetNearMaxIncreaseRateBps())
      .WillOnce(Return(100));
  EXPECT_EQ(kMaxIntervalMs, states.probing_interval_estimator->GetIntervalMs());
}
}  // namespace webrtc
