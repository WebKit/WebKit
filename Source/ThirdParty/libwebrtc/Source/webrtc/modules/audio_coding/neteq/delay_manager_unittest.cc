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

#include "absl/memory/memory.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/mock/mock_delay_peak_detector.h"
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
  virtual void TearDown();
  void RecreateDelayManager();
  void SetPacketAudioLength(int lengt_ms);
  void InsertNextPacket();
  void IncreaseTime(int inc_ms);

  std::unique_ptr<DelayManager> dm_;
  TickTimer tick_timer_;
  MockStatisticsCalculator stats_;
  MockDelayPeakDetector detector_;
  MockHistogram* mock_histogram_;
  uint16_t seq_no_;
  uint32_t ts_;
  bool enable_rtx_handling_ = false;
  bool use_mock_histogram_ = false;
  DelayManager::HistogramMode histogram_mode_ =
      DelayManager::HistogramMode::INTER_ARRIVAL_TIME;
};

DelayManagerTest::DelayManagerTest()
    : dm_(nullptr),
      detector_(&tick_timer_, false),
      seq_no_(0x1234),
      ts_(0x12345678) {}

void DelayManagerTest::SetUp() {
  RecreateDelayManager();
}

void DelayManagerTest::RecreateDelayManager() {
  EXPECT_CALL(detector_, Reset()).Times(1);
  if (use_mock_histogram_) {
    mock_histogram_ = new MockHistogram(kMaxIat, kForgetFactor);
    std::unique_ptr<Histogram> histogram(mock_histogram_);
    dm_ = absl::make_unique<DelayManager>(
        kMaxNumberOfPackets, kMinDelayMs, kDefaultHistogramQuantile,
        histogram_mode_, enable_rtx_handling_, &detector_, &tick_timer_,
        &stats_, std::move(histogram));
  } else {
    dm_ = DelayManager::Create(kMaxNumberOfPackets, kMinDelayMs,
                               enable_rtx_handling_, &detector_, &tick_timer_,
                               &stats_);
  }
}

void DelayManagerTest::SetPacketAudioLength(int lengt_ms) {
  EXPECT_CALL(detector_, SetPacketAudioLength(lengt_ms));
  dm_->SetPacketAudioLength(lengt_ms);
}

void DelayManagerTest::InsertNextPacket() {
  EXPECT_EQ(0, dm_->Update(seq_no_, ts_, kFs));
  seq_no_ += 1;
  ts_ += kTsIncrement;
}

void DelayManagerTest::IncreaseTime(int inc_ms) {
  for (int t = 0; t < inc_ms; t += kTimeStepMs) {
    tick_timer_.Increment();
  }
}

void DelayManagerTest::TearDown() {
  EXPECT_CALL(detector_, Die());
}

TEST_F(DelayManagerTest, CreateAndDestroy) {
  // Nothing to do here. The test fixture creates and destroys the DelayManager
  // object.
}

TEST_F(DelayManagerTest, SetPacketAudioLength) {
  const int kLengthMs = 30;
  // Expect DelayManager to pass on the new length to the detector object.
  EXPECT_CALL(detector_, SetPacketAudioLength(kLengthMs)).Times(1);
  EXPECT_EQ(0, dm_->SetPacketAudioLength(kLengthMs));
  EXPECT_EQ(-1, dm_->SetPacketAudioLength(-1));  // Illegal parameter value.
}

TEST_F(DelayManagerTest, PeakFound) {
  // Expect DelayManager to pass on the question to the detector.
  // Call twice, and let the detector return true the first time and false the
  // second time.
  EXPECT_CALL(detector_, peak_found())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(dm_->PeakFound());
  EXPECT_FALSE(dm_->PeakFound());
}

TEST_F(DelayManagerTest, UpdateNormal) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return false to indicate no peaks found.
  EXPECT_CALL(detector_, Update(1, false, 1)).WillOnce(Return(false));
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
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return false to indicate no peaks found.
  EXPECT_CALL(detector_, Update(2, false, 2)).WillOnce(Return(false));
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

TEST_F(DelayManagerTest, UpdatePeakFound) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return true to indicate that peaks are found. Let the peak height be 5.
  EXPECT_CALL(detector_, Update(1, false, 1)).WillOnce(Return(true));
  EXPECT_CALL(detector_, MaxPeakHeight()).WillOnce(Return(5));
  InsertNextPacket();
  EXPECT_EQ(5 << 8, dm_->TargetLevel());
  EXPECT_EQ(1, dm_->base_target_level());  // Base target level is w/o peaks.
  int lower, higher;
  dm_->BufferLimits(&lower, &higher);
  // Expect |lower| to be 75% of target level, and |higher| to be target level.
  EXPECT_EQ((5 << 8) * 3 / 4, lower);
  EXPECT_EQ(5 << 8, higher);
}

TEST_F(DelayManagerTest, TargetDelay) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return false to indicate no peaks found.
  EXPECT_CALL(detector_, Update(1, false, 1)).WillOnce(Return(false));
  InsertNextPacket();
  const int kExpectedTarget = 1;
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());  // In Q8.
  EXPECT_EQ(1, dm_->base_target_level());
  int lower, higher;
  dm_->BufferLimits(&lower, &higher);
  // Expect |lower| to be 75% of base target level, and |higher| to be
  // lower + 20 ms headroom.
  EXPECT_EQ((1 << 8) * 3 / 4, lower);
  EXPECT_EQ(lower + (20 << 8) / kFrameSizeMs, higher);
}

TEST_F(DelayManagerTest, MaxDelay) {
  const int kExpectedTarget = 5;
  const int kTimeIncrement = kExpectedTarget * kFrameSizeMs;
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to |kExpectedTarget| packet. Return true to indicate peaks found.
  EXPECT_CALL(detector_, Update(kExpectedTarget, false, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(detector_, MaxPeakHeight())
      .WillRepeatedly(Return(kExpectedTarget));
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
  // Expect detector update method to be called once with inter-arrival time
  // equal to |kExpectedTarget| packet. Return true to indicate peaks found.
  EXPECT_CALL(detector_, Update(kExpectedTarget, false, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(detector_, MaxPeakHeight())
      .WillRepeatedly(Return(kExpectedTarget));
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  int kMinDelayPackets = kExpectedTarget + 2;
  int kMinDelayMs = kMinDelayPackets * kFrameSizeMs;
  dm_->SetMinimumDelay(kMinDelayMs);
  IncreaseTime(kTimeIncrement);
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
  // Expect detector update method to be called once with inter-arrival time
  // equal to |kExpectedTarget| packet. Return true to indicate peaks found.
  EXPECT_CALL(detector_, Update(kExpectedTarget, false, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(detector_, MaxPeakHeight())
      .WillRepeatedly(Return(kExpectedTarget));
  IncreaseTime(kTimeIncrement);
  InsertNextPacket();

  // No limit is applied.
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());

  constexpr int kBaseMinimumDelayPackets = kExpectedTarget + 2;
  constexpr int kBaseMinimumDelayMs = kBaseMinimumDelayPackets * kFrameSizeMs;
  EXPECT_TRUE(dm_->SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);

  IncreaseTime(kTimeIncrement);
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
  // Expect detector update method to be called once with inter-arrival time
  // equal to |kExpectedTarget|. Return true to indicate peaks found.
  EXPECT_CALL(detector_, Update(kExpectedTarget, false, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(detector_, MaxPeakHeight())
      .WillRepeatedly(Return(kExpectedTarget));
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

  IncreaseTime(kTimeIncrement);
  InsertNextPacket();
  EXPECT_EQ(dm_->GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(kBaseMinimumDelayPackets << 8, dm_->TargetLevel());
}

TEST_F(DelayManagerTest, UpdateReorderedPacket) {
  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  // Insert packet that was sent before the previous packet.
  EXPECT_CALL(detector_, Update(_, true, _));
  EXPECT_EQ(0, dm_->Update(seq_no_ - 1, ts_ - kFrameSizeMs, kFs));
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
  EXPECT_CALL(*mock_histogram_, Add(3));
  EXPECT_EQ(0, dm_->Update(seq_no_ - 3, ts_ - 3 * kFrameSizeMs, kFs));

  // Insert another reordered packet.
  EXPECT_CALL(*mock_histogram_, Add(2));
  EXPECT_EQ(0, dm_->Update(seq_no_ - 2, ts_ - 2 * kFrameSizeMs, kFs));

  // Insert the next packet in order and verify that the inter-arrival time is
  // estimated correctly.
  IncreaseTime(kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(1));
  InsertNextPacket();
}

// Tests that skipped sequence numbers (simulating empty packets) are handled
// correctly.
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
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return false to indicate no peaks found.
  EXPECT_CALL(detector_, Update(1, false, 1)).WillOnce(Return(false));
  InsertNextPacket();

  EXPECT_EQ(1 << 8, dm_->TargetLevel());  // In Q8.
}

// Same as above, but do not call RegisterEmptyPacket. Observe the target level
// increase dramatically.
TEST_F(DelayManagerTest, EmptyPacketsNotReported) {
  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();

  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);

  // Advance the sequence number by 5, simulating that 5 empty packets were
  // received, but never inserted.
  seq_no_ += 10;

  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet, and (base) target level equal to 1 as well.
  // Return false to indicate no peaks found.
  EXPECT_CALL(detector_, Update(10, false, 10)).WillOnce(Return(false));
  InsertNextPacket();

  // Note 10 times higher target value.
  EXPECT_EQ(10 * 1 << 8, dm_->TargetLevel());  // In Q8.
}

TEST_F(DelayManagerTest, Failures) {
  // Wrong sample rate.
  EXPECT_EQ(-1, dm_->Update(0, 0, -1));
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

TEST_F(DelayManagerTest, TargetDelayGreaterThanOne) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled-0/");
  RecreateDelayManager();
  EXPECT_EQ(0, dm_->histogram_quantile());

  SetPacketAudioLength(kFrameSizeMs);
  // First packet arrival.
  InsertNextPacket();
  // Advance time by one frame size.
  IncreaseTime(kFrameSizeMs);
  // Second packet arrival.
  // Expect detector update method to be called once with inter-arrival time
  // equal to 1 packet.
  EXPECT_CALL(detector_, Update(1, false, 1)).WillOnce(Return(false));
  InsertNextPacket();
  constexpr int kExpectedTarget = 1;
  EXPECT_EQ(kExpectedTarget << 8, dm_->TargetLevel());  // In Q8.
}

TEST_F(DelayManagerTest, ForcedTargetDelayPercentile) {
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled-95/");
    RecreateDelayManager();
    EXPECT_EQ(kDefaultHistogramQuantile, dm_->histogram_quantile());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled-99.95/");
    RecreateDelayManager();
    EXPECT_EQ(1073204953, dm_->histogram_quantile());  // 0.9995 in Q30.
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Disabled/");
    RecreateDelayManager();
    EXPECT_EQ(kDefaultHistogramQuantile, dm_->histogram_quantile());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled--1/");
    EXPECT_EQ(kDefaultHistogramQuantile, dm_->histogram_quantile());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled-100.1/");
    RecreateDelayManager();
    EXPECT_EQ(kDefaultHistogramQuantile, dm_->histogram_quantile());
  }
}

TEST_F(DelayManagerTest, DelayHistogramFieldTrial) {
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998/");
    RecreateDelayManager();
    EXPECT_EQ(DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY,
              dm_->histogram_mode());
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
    EXPECT_EQ(DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY,
              dm_->histogram_mode());
    EXPECT_EQ(1046898278, dm_->histogram_quantile());  // 0.975 in Q30.
    EXPECT_EQ(
        32702,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.998 in Q15.
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
  {
    // NetEqDelayHistogram should take precedence over
    // NetEqForceTargetDelayPercentile.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqForceTargetDelayPercentile/Enabled-99.95/"
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96-0.998/");
    RecreateDelayManager();
    EXPECT_EQ(DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY,
              dm_->histogram_mode());
    EXPECT_EQ(1030792151, dm_->histogram_quantile());  // 0.96 in Q30.
    EXPECT_EQ(
        32702,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.998 in Q15.
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
  {
    // Invalid parameters.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Enabled-96/");
    RecreateDelayManager();
    EXPECT_EQ(DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY,
              dm_->histogram_mode());
    EXPECT_EQ(kDefaultHistogramQuantile,
              dm_->histogram_quantile());  // 0.95 in Q30.
    EXPECT_EQ(
        kForgetFactor,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.9993 in Q15.
    EXPECT_FALSE(dm_->histogram()->start_forget_weight_for_testing());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDelayHistogram/Disabled/");
    RecreateDelayManager();
    EXPECT_EQ(DelayManager::HistogramMode::INTER_ARRIVAL_TIME,
              dm_->histogram_mode());
    EXPECT_EQ(kDefaultHistogramQuantile,
              dm_->histogram_quantile());  // 0.95 in Q30.
    EXPECT_EQ(
        kForgetFactor,
        dm_->histogram()->base_forget_factor_for_testing());  // 0.9993 in Q15.
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

TEST_F(DelayManagerTest, RelativeArrivalDelayMode) {
  histogram_mode_ = DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY;
  use_mock_histogram_ = true;
  RecreateDelayManager();

  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  IncreaseTime(kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(0));  // Not delayed.
  InsertNextPacket();

  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(1));  // 20ms delayed.
  EXPECT_EQ(0, dm_->Update(seq_no_, ts_, kFs));

  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(*mock_histogram_, Add(2));  // 40ms delayed.
  EXPECT_EQ(0, dm_->Update(seq_no_ + 1, ts_ + kTsIncrement, kFs));

  EXPECT_CALL(*mock_histogram_, Add(1));  // Reordered, 20ms delayed.
  EXPECT_EQ(0, dm_->Update(seq_no_, ts_, kFs));
}

TEST_F(DelayManagerTest, MaxDelayHistory) {
  histogram_mode_ = DelayManager::HistogramMode::RELATIVE_ARRIVAL_DELAY;
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
  EXPECT_EQ(0, dm_->Update(seq_no_, ts_, kFs));
}

TEST_F(DelayManagerTest, RelativeArrivalDelayStatistic) {
  SetPacketAudioLength(kFrameSizeMs);
  InsertNextPacket();

  IncreaseTime(kFrameSizeMs);
  EXPECT_CALL(stats_, RelativePacketArrivalDelay(0));
  InsertNextPacket();

  IncreaseTime(2 * kFrameSizeMs);
  EXPECT_CALL(stats_, RelativePacketArrivalDelay(20));
  InsertNextPacket();
}

TEST_F(DelayManagerTest, DecelerationTargetLevelOffsetFieldTrial) {
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled-105/");
    RecreateDelayManager();
    EXPECT_EQ(dm_->deceleration_target_level_offset_ms().value(), 105 << 8);
  }
  {
    // Negative number.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled--105/");
    RecreateDelayManager();
    EXPECT_FALSE(dm_->deceleration_target_level_offset_ms().has_value());
  }
  {
    // Disabled.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Disabled/");
    RecreateDelayManager();
    EXPECT_FALSE(dm_->deceleration_target_level_offset_ms().has_value());
  }
  {
    // Float number.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled-105.5/");
    RecreateDelayManager();
    EXPECT_EQ(dm_->deceleration_target_level_offset_ms().value(), 105 << 8);
  }
  {
    // Several numbers.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled-20-40/");
    RecreateDelayManager();
    EXPECT_EQ(dm_->deceleration_target_level_offset_ms().value(), 20 << 8);
  }
}

TEST_F(DelayManagerTest, DecelerationTargetLevelOffset) {
  // Border value where 1/4 target buffer level meets
  // WebRTC-Audio-NetEqDecelerationTargetLevelOffset.
  constexpr int kBoarderTargetLevel = 100 * 4;
  {
    // Test that for a low target level, default behaviour is intact.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled-100/");
    const int target_level_ms = ((kBoarderTargetLevel - 1) << 8) / kFrameSizeMs;
    RecreateDelayManager();
    SetPacketAudioLength(kFrameSizeMs);

    int lower, higher;  // In Q8.
    dm_->BufferLimits(target_level_ms, &lower, &higher);

    // Default behaviour of taking 75% of target level.
    EXPECT_EQ(target_level_ms * 3 / 4, lower);
    EXPECT_EQ(target_level_ms, higher);
  }

  {
    // Test that for the high target level, |lower| is below target level by
    // fixed constant (100 ms in this Field Trial setup).
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqDecelerationTargetLevelOffset/Enabled-100/");
    const int target_level_ms = ((kBoarderTargetLevel + 1) << 8) / kFrameSizeMs;
    RecreateDelayManager();
    SetPacketAudioLength(kFrameSizeMs);

    int lower, higher;  // In Q8.
    dm_->BufferLimits(target_level_ms, &lower, &higher);

    EXPECT_EQ(target_level_ms - ((100 << 8) / kFrameSizeMs), lower);
    EXPECT_EQ(target_level_ms, higher);
  }

  {
    // Test that for the high target level, without Field Trial the behaviour
    // will remain the same.
    const int target_level_ms = ((kBoarderTargetLevel + 1) << 8) / kFrameSizeMs;
    RecreateDelayManager();
    SetPacketAudioLength(kFrameSizeMs);

    int lower, higher;  // In Q8.
    dm_->BufferLimits(target_level_ms, &lower, &higher);

    // Default behaviour of taking 75% of target level.
    EXPECT_EQ(target_level_ms * 3 / 4, lower);
    EXPECT_EQ(target_level_ms, higher);
  }
}

TEST_F(DelayManagerTest, ExtraDelay) {
  {
    // Default behavior. Insert two packets so that a new target level is
    // calculated.
    SetPacketAudioLength(kFrameSizeMs);
    InsertNextPacket();
    IncreaseTime(kFrameSizeMs);
    InsertNextPacket();
    EXPECT_EQ(dm_->TargetLevel(), 1 << 8);
  }
  {
    // Add 80 ms extra delay and calculate a new target level.
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqExtraDelay/Enabled-80/");
    RecreateDelayManager();
    SetPacketAudioLength(kFrameSizeMs);
    InsertNextPacket();
    IncreaseTime(kFrameSizeMs);
    InsertNextPacket();
    EXPECT_EQ(dm_->TargetLevel(), 5 << 8);
  }
}

}  // namespace webrtc
