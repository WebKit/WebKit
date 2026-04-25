/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/corruption_detection_frame_selector_settings.h"

#include "api/field_trials.h"
#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(CorruptionDetectionFrameSelectorSettingsTest, DisabledByDefault) {
  FieldTrials trials("");
  CorruptionDetectionFrameSelectorSettings settings(trials);
  EXPECT_FALSE(settings.is_enabled());
}

TEST(CorruptionDetectionFrameSelectorSettingsTest, EnabledWithDefaults) {
  FieldTrials trials("WebRTC-CorruptionDetectionFrameSelector/enabled:true/");
  CorruptionDetectionFrameSelectorSettings settings(trials);
  EXPECT_TRUE(settings.is_enabled());
  EXPECT_EQ(settings.low_overhead_lower_bound(), TimeDelta::Millis(1));
  EXPECT_EQ(settings.low_overhead_upper_bound(), TimeDelta::Millis(500));
  EXPECT_EQ(settings.high_overhead_lower_bound(), TimeDelta::Millis(33));
  EXPECT_EQ(settings.high_overhead_upper_bound(), TimeDelta::Millis(5000));
}

TEST(CorruptionDetectionFrameSelectorSettingsTest, ParsesValues) {
  FieldTrials trials(
      "WebRTC-CorruptionDetectionFrameSelector/enabled:true,"
      "low_overhead_lower_bound:10ms,low_overhead_upper_bound:100ms,"
      "high_overhead_lower_bound:20ms,high_overhead_upper_bound:200ms/");
  CorruptionDetectionFrameSelectorSettings settings(trials);
  EXPECT_TRUE(settings.is_enabled());
  EXPECT_EQ(settings.low_overhead_lower_bound(), TimeDelta::Millis(10));
  EXPECT_EQ(settings.low_overhead_upper_bound(), TimeDelta::Millis(100));
  EXPECT_EQ(settings.high_overhead_lower_bound(), TimeDelta::Millis(20));
  EXPECT_EQ(settings.high_overhead_upper_bound(), TimeDelta::Millis(200));
}

TEST(CorruptionDetectionFrameSelectorSettingsTest, ValidationLowOverhead) {
  FieldTrials trials(
      "WebRTC-CorruptionDetectionFrameSelector/enabled:true,"
      "low_overhead_lower_bound:100ms,low_overhead_upper_bound:10ms/");
  CorruptionDetectionFrameSelectorSettings settings(trials);
  EXPECT_FALSE(settings.is_enabled());
}

TEST(CorruptionDetectionFrameSelectorSettingsTest, ValidationHighOverhead) {
  FieldTrials trials(
      "WebRTC-CorruptionDetectionFrameSelector/enabled:true,"
      "high_overhead_lower_bound:100ms,high_overhead_upper_bound:10ms/");
  CorruptionDetectionFrameSelectorSettings settings(trials);
  EXPECT_FALSE(settings.is_enabled());
}

}  // namespace
}  // namespace webrtc
