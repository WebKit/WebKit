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

#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/mock/mock_histogram.h"
#include "modules/audio_coding/neteq/mock/mock_statistics_calculator.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kMaxNumberOfPackets = 240;
constexpr int kMinDelayMs = 0;
constexpr int kTimeStepMs = 10;
constexpr int kFs = 8000;
constexpr int kFrameSizeMs = 20;
constexpr int kTsIncrement = kFrameSizeMs * kFs / 1000;
constexpr int kMaxBufferSizeMs = kMaxNumberOfPackets * kFrameSizeMs;
constexpr int kDefaultHistogramQuantile = 1020054733;
constexpr int kMaxIat = 64;
constexpr int kForgetFactor = 32745;
}  // namespace

using ::testing::_;
using ::testing::Return;

class DelayManagerTest : public ::testing::Test {
 protected:
  DelayManagerTest();
  virtual void SetUp();
  void RecreateDelayManager();
  void SetPacketAudioLength(int lengt_ms);
  absl::optional<int> InsertNextPacket();
  void IncreaseTime(int inc_ms);

  std::unique_ptr<DelayManager> dm_;
  TickTimer tick_timer_;
  MockStatisticsCalculator stats_;
  MockHistogram* mock_histogram_;
  uint16_t seq_no_;
  uint32_t ts_;
  bool enable_rtx_handling_ = false;
  bool use_mock_histogram_ = false;
};

DelayManagerTest::DelayManagerTest()
    : dm_(nullptr),
      seq_no_(0x1234),
      ts_(0x12345678) {}

void DelayManagerTest::SetUp() {
  RecreateDelayManager();
}

void DelayManagerTest::RecreateDelayManager() {
  if (use_mock_histogram_) {
    mock_histogram_ = new MockHistogram(kMaxIat, kForgetFactor);
    std::unique_ptr<Histogram> histogram(mock_histogram_);
    dm_ = std::make_unique<DelayManager>(
        kMaxNumberOfPackets, kMinDelayMs, kDefaultHistogramQuantile,
        enable_rtx_handling_, &tick_timer_, std::move(histogram));
  } else {
    dm_ = DelayManager::Create(kMaxNumberOfPackets, kMinDelayMs,
                               enable_rtx_handling_, &tick_timer_);
  }
}

void DelayManagerTest::SetPacketAudioLength(int lengt_ms) {
  dm_->SetPacketAudioLength(lengt_ms);
}

absl::optional<int> DelayManagerTest::InsertNextPacket() {
  auto relative_delay = dm_->Update(seq_no_, ts_, kFs);
  seq_no_ += 1;
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

TEST_F(DelayManagerTest, SetPacketAudioLength) {
  const int kLengthMs = 30;
  EXPECT_EQ(0, dm_->SetPacketAudioLength(kLengthMs));
  EXPECT_EQ(-1, dm_->SetPacketAudioLength(-1));  // Illegal parameter value.
}

TEST_F(DelayManagerTest, UpdateNormal) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  InsertNextPacket();
  EXPECT_EQ(1 << 8, dm_->TargetLevel());  // In Q8.
  EXPECT_EQ(1, dm_->base_target_level());
  int lower, higher;
  dm_->BufferLimits(&lower, &higher);
  // Expect |lower| to be 75% of target level, and |higher| to be target level,
  // but also at least 20 ms higher than |lower|, which is the limiting case
  // here.
  EXPECT_EQ((1 << 8) * 3 / 4, lower);
  EXPECT_EQ(lower + (20 << 8) / kFrameSizeMs, higher);
}

TEST_F(DelayManagerTest, UpdateLongInterArrivalTime) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by two frame size.
  IncreaseTime(2 * kFrameSizeMs);
  // Second packet arrival.
  InsertNextPacket();
  EXPECT_EQ(2 << 8, dm_->TargetLevel());  // In Q8.
  EXPECT_EQ(2, dm_->base_target_level());
  int lower, higher;
  dm_->BufferLimits(&lower, &higher);
  // Expect |lower| to be 75% of target level, and |higher| to be target level,
  // but also at least 20 ms higher than |lower|, which is the limiting case
  // here.
  EXPECT_EQ((2 << 8) * 3 / 4, lower);
  EXPECT_EQ(lower + (20 << 8) / kFrameSizeMs, higher);
}

TEST_F(DelayManagerTest, MaxDelay) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is set.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  int kMaxDelayPackets = kExpectedTarget - 2;
  int kMaxDelayMs = kMaxDelayPackets * kFrameSizeMs;
  EXPECT_TRUE(dm_->SetMaximumDelay(kMaxDelayMs));
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();
  EXPECT_EQ(kMaxDelayPackets << 8, dm_->TargetLevel());

  // Target level at least should be one packet.
  EXPECT_FALSE(dm_->SetMaximumDelay(kFrameSizeMs - 1));
}

TEST_F(DelayManagerTest, MinDelay) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  int kMinDelayPackets = kExpectedTarget + 2;
  int kMinDelayMs = kMinDelayPackets * kFrameSizeMs;
  dm_->SetMinimumDelay(kMinDelayMs);
  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(kMinDelayPackets << 8, dm_->TargetLevel());
}

TEST_F(DelayManagerTest, BaseMinimumDelayCheckValidRange) {
  SetPacketAudioLength(kFrameSizeMs);

  // Base minimum delay should be between [0, 10000] milliseconds.
  EXPECT_FALSE(dm_->SetBaseMinimumDelay(-1));
  EXPECT_FALSE(dm_->SetBaseMinimumDelay(10001));
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), 0);

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(7999));
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), 7999);
}

TEST_F(DelayManagerTest, BaseMinimumDelayLowerThanMinimumDelay) {
  SetPacketAudioLength(kFrameSizeMs);
  constexpr int kBaseMinimumDelayMs = 100;
  constexpr int kMinimumDelayMs = 200;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is lower than minimum delay we use minimum delay.
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanMinimumDelay) {
  SetPacketAudioLength(kFrameSizeMs);
  constexpr int kBaseMinimumDelayMs = 70;
  constexpr int kMinimumDelayMs = 30;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is greater than minimum delay we use base minimum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kBaseMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanBufferSize) {
  SetPacketAudioLength(kFrameSizeMs);
  constexpr int kBaseMinimumDelayMs = kMaxBufferSizeMs + 1;
  constexpr int kMinimumDelayMs = 12;
  constexpr int kMaximumDelayMs = 20;
  constexpr int kMaxBufferSizeMsQ75 = 3 * kMaxBufferSizeMs / 4;

  EXPECT_TRUE(dm_->SetMaximumDelay(kMaximumDelayMs));

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is maximum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaxBufferSizeMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMsQ75);

  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));

  // Unset maximum value.
  EXPECT_TRUE(dm_->SetMaximumDelay(0));

  // With maximum value unset, the highest possible value now is 75% of
  // currently possible maximum buffer size.
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kMaxBufferSizeMsQ75);
}

TEST_F(DelayManagerTest, BaseMinimumDelayGreaterThanMaximumDelay) {
  SetPacketAudioLength(kFrameSizeMs);
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs + 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is kMaximumDelayMs.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMs);

  EXPECT_TRUE(dm_->SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kMaximumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelayLowerThanMaxSize) {
  SetPacketAudioLength(kFrameSizeMs);
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs - 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, and lower than maximum
  // delays that is why it is used.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMaximumDelayMs);

  EXPECT_TRUE(dm_->SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kBaseMinimumDelayMs);
}

TEST_F(DelayManagerTest, MinimumDelayMemorization) {
  // Check that when we increase base minimum delay to value higher than
  // minimum delay then minimum delay is still memorized. This allows to
  // restore effective minimum delay to memorized minimum delay value when we
  // decrease base minimum delay.
  SetPacketAudioLength(kFrameSizeMs);

  constexpr int kBaseMinimumDelayMsLow = 10;
  constexpr int kMinimumDelayMs = 20;
  constexpr int kBaseMinimumDelayMsHigh = 30;

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  // Minimum delay is used as it is higher than base minimum delay.
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kMinimumDelayMs);

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMsHigh));
  // Base minimum delay is used as it is now higher than minimum delay.
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(),
            kBaseMinimumDelayMsHigh);

  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  // Check that minimum delay is memorized and is used again.
  EXPECT_EQ(dm_->effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST_F(DelayManagerTest, BaseMinimumDelay) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  constexpr int kBaseMinimumDelayPackets = kExpectedTarget + 2;
  constexpr int kBaseMinimumDelayMs = kBaseMinimumDelayPackets * kFrameSizeMs;
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);

  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(kBaseMinimumDelayPackets << 8, dm_->TargetLevel());
}

TEST_F(DelayManagerTest, BaseMinimumDealyAffectTargetLevel) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  // Minimum delay is lower than base minimum delay, that is why base minimum
  // delay is used to calculate target level.
  constexpr int kMinimumDelayPackets = kExpectedTarget + 1;
  constexpr int kBaseMinimumDelayPackets = kExpectedTarget + 2;

  constexpr int kMinimumDelayMs = kMinimumDelayPackets * kFrameSizeMs;
  constexpr int kBaseMinimumDelayMs = kBaseMinimumDelayPackets * kFrameSizeMs;

  EXPECT_TRUE(kMinimumDelayMs < kBaseMinimumDelayMs);
  EXPECT_TRUE(dm_->SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);

  IncreaseTime(kFrameSizeMs);
  InsertNextPacket();
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(kBaseMinimumDelayPackets << 8, dm_->TargetLevel());
}

TEST_F(DelayManagerTest, EnableRtxHandling) {
  enable_rtx_handling_ = true;
  use_mock_histogram_ = true;
  RecreateDelayManager();
  EXPECT_TRUE(mock_histogram_);

  // Insert first packet.
  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  // Insert reordered packet.
  EXPECT_CALL(*mock_histogram_, Add(2));
  dm_->Update(seq_no_ - 3, ts_ - 3 * kFrameSizeMs, kFs);

  // Insert another reordered packet.
  EXPECT_CALL(*mock_histogram_, Add(1));
  dm_->Update(seq_no_ - 2, ts_ - 2 * kFrameSizeMs, kFs);

  // Insert the next packet in order and verify that the inter-arrival time is
  // estimated correctly.
  IncreaseTime(kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(0));
  InsertNextPacket();
}

// Tests that skipped sequence numbers (simulating empty packets) are handled
// correctly.
// TODO(jakobi): Make delay manager independent of sequence numbers.
TEST_F(DelayManagerTest, EmptyPacketsReported) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();

  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);

  // Advance the sequence number by 5, simulating that 5 empty packets were
  // received, but never inserted.
  seq_no_ += 10;
  for (int j = 0; j < 10; ++j) {
    dm_->RegisterEmptyPacket();
  }

  // Second packet arrival.
  InsertNextPacket();

  EXPECT_EQ(1 << 8, dm_->TargetLevel());  // In Q8.
}

// Same as above, but do not call RegisterEmptyPacket. Target level stays the
// same.
TEST_F(DelayManagerTest, EmptyPacketsNotReported) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();

  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);

  // Advance the sequence number by 10, simulating that 10 empty packets were
  // received, but never inserted.
  seq_no_ += 10;

  // Second packet arrival.
  InsertNextPacket();

  EXPECT_EQ(1 << 8, dm_->TargetLevel());  // In Q8.
}

TEST_F(DelayManagerTest, Failures) {
  // Wrong sample rate.
  EXPECT_EQ(absl::nullopt, dm_->Update(0, 0, -1));
  // Wrong packet size.
  EXPECT_EQ(-1, dm_->SetPacketAudioLength(0));
  EXPECT_EQ(-1, dm_->SetPacketAudioLength(-1));

  // Minimum delay higher than a maximum delay is not accepted.
  EXPECT_TRUE(dm_->SetMaximumDelay(10));
  EXPECT_FALSE(dm_->SetMinimumDelay(20));

  // Maximum delay less than minimum delay is not accepted.
  EXPECT_TRUE(dm_->SetMaximumDelay(100));
  EXPECT_TRUE(dm_->SetMinimumDelay(80));
  EXPECT_FALSE(dm_->SetMaximumDelay(60));
}

TEST_F(DelayManagerTest, DelayHistogramFieldTrial) {
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998/");
    RecreateDelayManager();
    EXPECT_EQ(1030792151, dm_->histogram_quantile());  // 0.96 in Q30.
    EXPECT_EQ(
        32702,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.998 in Q15.
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-97.5-0.998/");
    RecreateDelayManager();
    EXPECT_EQ(1046898278, dm_->histogram_quantile());  // 0.975 in Q30.
    EXPECT_EQ(
        32702,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.998 in Q15.
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
  // Test parameter for new call start adaptation.
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998-1/");
    RecreateDelayManager();
    EXPECT_EQ(dm_->histogram()->start_forget_weight_for_testing().value(), 1.0);
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998-1.5/");
    RecreateDelayManager();
    EXPECT_EQ(dm_->histogram()->start_forget_weight_for_testing().value(), 1.5);
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998-0.5/");
    RecreateDelayManager();
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
}

TEST_F(DelayManagerTest, RelativeArrivalDelay) {
  use_mock_histogram_ = true;
  RecreateDelayManager();

  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  IncreaseTime(kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(0));  // Not delayed.
  InsertNextPacket();

  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(1));  // 20ms delayed.
  dm_->Update(seq_no_, ts_, kFs);

  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(2));  // 40ms delayed.
  dm_->Update(seq_no_ + 1, ts_ + kTsIncrement, kFs);

  EXPECT_CALL(*mock_histogram_, Add(1));  // Reordered, 20ms delayed.
  dm_->Update(seq_no_, ts_, kFs);
}

TEST_F(DelayManagerTest, MaxDelayHistory) {
  use_mock_histogram_ = true;
  RecreateDelayManager();

  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  // Insert 20 ms iat delay in the delay history.
  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(1));  // 20ms delayed.
  InsertNextPacket();

  // Insert next packet with a timestamp difference larger than maximum history
  // size. This removes the previously inserted iat delay from the history.
  constexpr int kMaxHistoryMs = 2000;
  IncreaseTime(kMaxHistoryMs + kFrameSizeMs);
  ts_ += kFs * kMaxHistoryMs / 1000;
  EXPECT_CALL(*mock_histogram_, Add(0));  // Not delayed.
  dm_->Update(seq_no_, ts_, kFs);
}

TEST_F(DelayManagerTest, RelativeArrivalDelayStatistic) {
  SetPacketAudioLength(kFrameSizeMs);
  EXPECT_EQ(absl::nullopt, InsertNextPacket());
  IncreaseTime(kFrameSizeMs);
  EXPECT_EQ(0, InsertNextPacket());
  IncreaseTime(2 * kFrameSizeMs);

  EXPECT_EQ(20, InsertNextPacket());
}

TEST_F(DelayManagerTest, DecelerationTargetLevelOffset) {
  SetPacketAudioLength(kFrameSizeMs);

  // Deceleration target level offset follows the value hardcoded in
  // delay_manager.cc.
  constexpr int kDecelerationTargetLevelOffsetMs = 85 << 8;  // In Q8.
  // Border value where |x * 3/4 = target_level - x|.
  constexpr int kBoarderTargetLevel = kDecelerationTargetLevelOffsetMs * 4;
  {
    // Test that for a low target level, default behaviour is intact.
    const int target_level_ms = kBoarderTargetLevel / kFrameSizeMs - 1;

    int lower, higher;  // In Q8.
    dm_->BufferLimits(target_level_ms, &lower, &higher);

    // Default behaviour of taking 75% of target level.
    EXPECT_EQ(target_level_ms * 3 / 4, lower);
    EXPECT_EQ(target_level_ms, higher);
  }

  {
    // Test that for the high target level, |lower| is below target level by
    // fixed |kOffset|.
    const int target_level_ms = kBoarderTargetLevel / kFrameSizeMs + 1;

    int lower, higher;  // In Q8.
    dm_->BufferLimits(target_level_ms, &lower, &higher);

    EXPECT_EQ(target_level_ms - kDecelerationTargetLevelOffsetMs / kFrameSizeMs,
              lower);
    EXPECT_EQ(target_level_ms, higher);
  }
}

}  // namespace webrtc
