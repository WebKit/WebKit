/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/keyframe_interval_settings.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(KeyframeIntervalSettingsTest, ParsesMinKeyframeSendIntervalMs) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_send_interval_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings::ParseFromFieldTrials()
                .MinKeyframeSendIntervalMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, DoesNotParseIncorrectValues) {
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());

  test::ScopedFieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_send_interval_ms:a/");
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());
  EXPECT_FALSE(KeyframeIntervalSettings::ParseFromFieldTrials()
                   .MinKeyframeSendIntervalMs());
}

}  // namespace
}  // namespace webrtc
