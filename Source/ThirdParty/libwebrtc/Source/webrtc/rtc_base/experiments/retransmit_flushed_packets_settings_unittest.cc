/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/retransmit_flushed_packets_settings.h"

#include "api/field_trials.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(RetransmitFlushedPacketsSettingsTest, DisabledByDefault) {
  FieldTrials trials = CreateTestFieldTrials("");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_FALSE(settings.is_enabled());
  EXPECT_DOUBLE_EQ(settings.rtt_multiplier(), 2.0);
}

TEST(RetransmitFlushedPacketsSettingsTest, DisabledExplicitly) {
  FieldTrials trials =
      CreateTestFieldTrials("WebRTC-RetransmitFlushedPackets/Disabled/");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_FALSE(settings.is_enabled());
}

TEST(RetransmitFlushedPacketsSettingsTest, EnabledDefaultMultiplier) {
  FieldTrials trials =
      CreateTestFieldTrials("WebRTC-RetransmitFlushedPackets/Enabled/");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_TRUE(settings.is_enabled());
  EXPECT_DOUBLE_EQ(settings.rtt_multiplier(), 2.0);
}

TEST(RetransmitFlushedPacketsSettingsTest, EnabledWithCustomMultiplier) {
  FieldTrials trials = CreateTestFieldTrials(
      "WebRTC-RetransmitFlushedPackets/Enabled,rtt_multiplier:3.5/");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_TRUE(settings.is_enabled());
  EXPECT_DOUBLE_EQ(settings.rtt_multiplier(), 3.5);
}

TEST(RetransmitFlushedPacketsSettingsTest, EnabledViaParameter) {
  FieldTrials trials = CreateTestFieldTrials(
      "WebRTC-RetransmitFlushedPackets/enabled:true,rtt_multiplier:1.5/");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_TRUE(settings.is_enabled());
  EXPECT_DOUBLE_EQ(settings.rtt_multiplier(), 1.5);
}

TEST(RetransmitFlushedPacketsSettingsTest, NegativeMultiplierClampedToZero) {
  FieldTrials trials = CreateTestFieldTrials(
      "WebRTC-RetransmitFlushedPackets/Enabled,rtt_multiplier:-1.0/");
  RetransmitFlushedPacketsSettings settings(trials);
  EXPECT_DOUBLE_EQ(settings.rtt_multiplier(), 0.0);
}

}  // namespace
}  // namespace webrtc
