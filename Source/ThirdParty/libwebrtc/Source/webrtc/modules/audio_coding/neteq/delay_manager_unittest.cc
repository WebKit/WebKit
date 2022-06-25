/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for DelayManager class.

#include "modules/audio_coding/neteq/delay_manager.h"

#include <math.h>

#include <memory>

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/mock/mock_histogram.h"
#include "modules/audio_coding/neteq/mock/mock_statistics_calculator.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kMaxNumberOfPackets = 200;
constexpr int kTimeStepMs = 10;
constexpr int kFs = 8000;
constexpr int kFrameSizeMs = 20;
constexpr int kTsIncrement = kFrameSizeMs * kFs / 1000;
constexpr int kMaxBufferSizeMs = kMaxNumberOfPackets * kFrameSizeMs;

}  // namespace

class DelayManagerTest : public ::testing::Test {
 protected:
  DelayManagerTest();
  virtual void SetUp();
  absl::optional<int> InsertNextPacket();
  void IncreaseTime(int inc_ms);

  DelayManager dm_;
  TickTimer tick_timer_;
  uint32_t ts_;
};

DelayManagerTest::DelayManagerTest()
    : dm_(DelayManager::Config(), &tick_timer_), ts_(0x12345678) {}

void DelayManagerTest::SetUp() {
  dm_.SetPacketAudioLength(kFrameSizeMs);
}

absl::optional<int> DelayManagerTest::InsertNextPacket() {
  auto relative_delay = dm_.Update(ts_, kFs);
  ts_ += kTsIncrement;
  return relative_delay;
}

void DelayManagerTest::IncreaseTime(int inc_ms) {
  for (int t = 0; t < inc_ms; t += kTimeStepMs) {
    tick_timer_.Increment();
  }
}

TEST_F(DelayManagerTest, CreateAndDestroy) {
  // Nothing to do here. The test fixture creates and destroys the DelayManager
  // object.
}

TEST_F(DelayManagerTest, UpdateNormal) {
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  InsertNextPacket();
  EXPECT_EQ(20, dm_.TargetDelayMs());
}

TEST_F(DelayManagerTest, UpdateLongInterArrivalTime) {
  // First packet arrival.
  InsertNextPacket();
  // Advance time by two frame size.
  IncreaseTime(2 * kFrameSizeMs);
  // Second packet arrival.
  InsertNextPacket();
  EXPECT_EQ(40, dm_.TargetDelayMs());
}

TEST_F(DelayManagerTest, MaxDelay) {
  const int kExpectedTarget = 5 * kFrameSizeMs;
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kExpectedTarget);
  InsertNextPacket();

  // No limit is set.
  EXPECT_EQ(kExpectedTarget, dm_.TargetDelayMs());

  const int kMaxDelayMs = 3 * kFrameSizeMs;
  EXPECT_TRUE(dm_.SetMaximumDelay(kMaxDelayMs));
  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(kMaxDelayMs, dm_.TargetDelayMs());

  // Target level at least should be one packet.
  EXPECT_FALSE(dm_.SetMaximumDelay(kFrameSizeMs - 1));
}

TEST_F(DelayManagerTest, MinDelay) {
  const int kExpectedTarget = 5 * kFrameSizeMs;
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kExpectedTarget);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget, dm_.TargetDelayMs());

  int kMinDelayMs = 7 * kFrameSizeMs;
  dm_.SetMinimumDelay(kMinDelayMs);
  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(kMinDelayMs, dm_.TargetDelayMs());
}

TEST_F(DelayManagerTest, BaseMinimumDelayCheckValidRange) {
  // Base minimum delay should be between [0, 10000] milliseconds.
  EXPECT_FALSE(dm_.SetBaseMinimumDelay(-1));
  EXPECT_FALSE(dm_.SetBaseMinimumDelay(10001));
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), 0);

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(7999));
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), 7999);
}

TEST_F(DelayManagerTest, BaseMinimumDelayLowerThanMinimumDelay) {
  constexpr int kBaseMinimumDelayMs = 100;
  constexpr int kMinimumDelayMs = 200;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is lower than minimum delay we use minimum delay.
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanMinimumDelay) {
  constexpr int kBaseMinimumDelayMs = 70;
  constexpr int kMinimumDelayMs = 30;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is greater than minimum delay we use base minimum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kBaseMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanBufferSize) {
  constexpr int kBaseMinimumDelayMs = kMaxBufferSizeMs + 1;
  constexpr int kMinimumDelayMs = 12;
  constexpr int kMaximumDelayMs = 20;
  constexpr int kMaxBufferSizeMsQ75 = 3 * kMaxBufferSizeMs / 4;

  EXPECT_TRUE(dm_.SetMaximumDelay(kMaximumDelayMs));

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is maximum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaxBufferSizeMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMsQ75);

  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));

  // Unset maximum value.
  EXPECT_TRUE(dm_.SetMaximumDelay(0));

  // With maximum value unset, the highest possible value now is 75% of
  // currently possible maximum buffer size.
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kMaxBufferSizeMsQ75);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanMaximumDelay) {
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs + 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is kMaximumDelayMs.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMs);

  EXPECT_TRUE(dm_.SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kMaximumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayLowerThanMaxSize) {
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs - 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, and lower than maximum
  // delays that is why it is used.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMaximumDelayMs);

  EXPECT_TRUE(dm_.SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kBaseMinimumDelayMs);
}

TEST_F(DelayManagerTest, MinimumDelayMemorization) {
  // Check that when we increase base minimum delay to value higher than
  // minimum delay then minimum delay is still memorized. This allows to
  // restore effective minimum delay to memorized minimum delay value when we
  // decrease base minimum delay.
  constexpr int kBaseMinimumDelayMsLow = 10;
  constexpr int kMinimumDelayMs = 20;
  constexpr int kBaseMinimumDelayMsHigh = 30;

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  // Minimum delay is used as it is higher than base minimum delay.
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMsHigh));
  // Base minimum delay is used as it is now higher than minimum delay.
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kBaseMinimumDelayMsHigh);

  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  // Check that minimum delay is memorized and is used again.
  EXPECT_EQ(dm_.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelay) {
  const int kExpectedTarget = 5 * kFrameSizeMs;
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kExpectedTarget);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget, dm_.TargetDelayMs());

  constexpr int kBaseMinimumDelayMs = 7 * kFrameSizeMs;
  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), kBaseMinimumDelayMs);

  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(kBaseMinimumDelayMs, dm_.TargetDelayMs());
}

TEST_F(DelayManagerTest, BaseMinimumDelayAffectsTargetDelay) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kTimeIncrement, dm_.TargetDelayMs());

  // Minimum delay is lower than base minimum delay, that is why base minimum
  // delay is used to calculate target level.
  constexpr int kMinimumDelayPackets = kExpectedTarget + 1;
  constexpr int kBaseMinimumDelayPackets = kExpectedTarget + 2;

  constexpr int kMinimumDelayMs = kMinimumDelayPackets * kFrameSizeMs;
  constexpr int kBaseMinimumDelayMs = kBaseMinimumDelayPackets * kFrameSizeMs;

  EXPECT_TRUE(kMinimumDelayMs < kBaseMinimumDelayMs);
  EXPECT_TRUE(dm_.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), kBaseMinimumDelayMs);

  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(dm_.GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(kBaseMinimumDelayMs, dm_.TargetDelayMs());
}

TEST_F(DelayManagerTest, Failures) {
  // Wrong sample rate.
  EXPECT_EQ(absl::nullopt, dm_.Update(0, -1));
  // Wrong packet size.
  EXPECT_EQ(-1, dm_.SetPacketAudioLength(0));
  EXPECT_EQ(-1, dm_.SetPacketAudioLength(-1));

  // Minimum delay higher than a maximum delay is not accepted.
  EXPECT_TRUE(dm_.SetMaximumDelay(20));
  EXPECT_FALSE(dm_.SetMinimumDelay(40));

  // Maximum delay less than minimum delay is not accepted.
  EXPECT_TRUE(dm_.SetMaximumDelay(100));
  EXPECT_TRUE(dm_.SetMinimumDelay(80));
  EXPECT_FALSE(dm_.SetMaximumDelay(60));
}

TEST_F(DelayManagerTest, RelativeArrivalDelayStatistic) {
  EXPECT_EQ(absl::nullopt, InsertNextPacket());
  IncreaseTime(kFrameSizeMs);
  EXPECT_EQ(0, InsertNextPacket());
  IncreaseTime(2 * kFrameSizeMs);

  EXPECT_EQ(20, InsertNextPacket());
}

}  // namespace webrtc
