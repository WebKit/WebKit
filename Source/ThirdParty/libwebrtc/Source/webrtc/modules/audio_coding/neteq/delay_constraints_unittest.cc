/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/delay_constraints.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kMaxNumberOfPackets = 200;
constexpr int kFrameSizeMs = 20;
constexpr int kMaxBufferSizeMs = kMaxNumberOfPackets * kFrameSizeMs;

TEST(DelayConstraintsTest, NoConstraints) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  EXPECT_EQ(constraints.Clamp(100), 100);
  EXPECT_EQ(constraints.Clamp(0), 0);
}

TEST(DelayConstraintsTest, MaxDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kMaxDelayMs = 60;
  EXPECT_TRUE(constraints.SetMaximumDelay(kMaxDelayMs));
  EXPECT_EQ(constraints.Clamp(100), kMaxDelayMs);
}

TEST(DelayConstraintsTest, MinDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kMinDelayMs = 7 * kFrameSizeMs;
  constraints.SetMinimumDelay(kMinDelayMs);
  EXPECT_EQ(constraints.Clamp(20), kMinDelayMs);
}

TEST(DelayConstraintsTest, BaseMinimumDelayCheckValidRange) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  // Base minimum delay should be between [0, 10000] milliseconds.
  EXPECT_FALSE(constraints.SetBaseMinimumDelay(-1));
  EXPECT_FALSE(constraints.SetBaseMinimumDelay(10001));
  EXPECT_EQ(constraints.GetBaseMinimumDelay(), 0);

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(7999));
  EXPECT_EQ(constraints.GetBaseMinimumDelay(), 7999);
}

TEST(DelayConstraintsTest, BaseMinimumDelayLowerThanMinimumDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kBaseMinimumDelayMs = 100;
  constexpr int kMinimumDelayMs = 200;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is lower than minimum delay we use minimum delay.
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST(DelayConstraintsTest, BaseMinimumDelayGreaterThanMinimumDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kBaseMinimumDelayMs = 70;
  constexpr int kMinimumDelayMs = 30;

  // Base minimum delay sets lower bound on minimum. That is why when base
  // minimum delay is greater than minimum delay we use base minimum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(),
            kBaseMinimumDelayMs);
}

TEST(DelayConstraintsTest, BaseMinimumDelayGreaterThanBufferSize) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kBaseMinimumDelayMs = kMaxBufferSizeMs + 1;
  constexpr int kMinimumDelayMs = 12;
  constexpr int kMaximumDelayMs = 20;
  constexpr int kMaxBufferSizeMsQ75 = 3 * kMaxBufferSizeMs / 4;
  EXPECT_TRUE(constraints.SetPacketAudioLength(kFrameSizeMs));

  EXPECT_TRUE(constraints.SetMaximumDelay(kMaximumDelayMs));

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is maximum delay.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaxBufferSizeMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMsQ75);

  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));

  // Unset maximum value.
  EXPECT_TRUE(constraints.SetMaximumDelay(0));

  // With maximum value unset, the highest possible value now is 75% of
  // currently possible maximum buffer size.
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(),
            kMaxBufferSizeMsQ75);
}

TEST(DelayConstraintsTest, BaseMinimumDelayGreaterThanMaximumDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs + 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, that is why we clamp
  // it to current the highest possible value which is kMaximumDelayMs.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMaximumDelayMs);
  RTC_DCHECK_LT(kMaximumDelayMs, kMaxBufferSizeMs);

  EXPECT_TRUE(constraints.SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(), kMaximumDelayMs);
}

TEST(DelayConstraintsTest, BaseMinimumDelayLowerThanMaxSize) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kMaximumDelayMs = 400;
  constexpr int kBaseMinimumDelayMs = kMaximumDelayMs - 1;
  constexpr int kMinimumDelayMs = 20;

  // Base minimum delay is greater than minimum delay, and lower than maximum
  // delays that is why it is used.
  RTC_DCHECK_GT(kBaseMinimumDelayMs, kMinimumDelayMs);
  RTC_DCHECK_LT(kBaseMinimumDelayMs, kMaximumDelayMs);

  EXPECT_TRUE(constraints.SetMaximumDelay(kMaximumDelayMs));
  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(),
            kBaseMinimumDelayMs);
}

TEST(DelayConstraintsTest, MinimumDelayMemorization) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  // Check that when we increase base minimum delay to value higher than
  // minimum delay then minimum delay is still memorized. This allows to
  // restore effective minimum delay to memorized minimum delay value when we
  // decrease base minimum delay.
  constexpr int kBaseMinimumDelayMsLow = 10;
  constexpr int kMinimumDelayMs = 20;
  constexpr int kBaseMinimumDelayMsHigh = 30;

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  EXPECT_TRUE(constraints.SetMinimumDelay(kMinimumDelayMs));
  // Minimum delay is used as it is higher than base minimum delay.
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMsHigh));
  // Base minimum delay is used as it is now higher than minimum delay.
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(),
            kBaseMinimumDelayMsHigh);

  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMsLow));
  // Check that minimum delay is memorized and is used again.
  EXPECT_EQ(constraints.effective_minimum_delay_ms_for_test(), kMinimumDelayMs);
}

TEST(DelayConstraintsTest, BaseMinimumDelay) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  constexpr int kBaseMinimumDelayMs = 7 * kFrameSizeMs;
  EXPECT_TRUE(constraints.SetBaseMinimumDelay(kBaseMinimumDelayMs));
  EXPECT_EQ(constraints.GetBaseMinimumDelay(), kBaseMinimumDelayMs);
  EXPECT_EQ(constraints.Clamp(20), kBaseMinimumDelayMs);
}

TEST(DelayConstraintsTest, Failures) {
  DelayConstraints constraints(kMaxNumberOfPackets, 0);
  // Wrong packet size.
  EXPECT_FALSE(constraints.SetPacketAudioLength(0));
  EXPECT_FALSE(constraints.SetPacketAudioLength(-1));

  // Minimum delay higher than a maximum delay is not accepted.
  EXPECT_TRUE(constraints.SetMaximumDelay(20));
  EXPECT_FALSE(constraints.SetMinimumDelay(40));

  // Maximum delay less than minimum delay is not accepted.
  EXPECT_TRUE(constraints.SetMaximumDelay(100));
  EXPECT_TRUE(constraints.SetMinimumDelay(80));
  EXPECT_FALSE(constraints.SetMaximumDelay(60));
}

}  // namespace
}  // namespace webrtc
