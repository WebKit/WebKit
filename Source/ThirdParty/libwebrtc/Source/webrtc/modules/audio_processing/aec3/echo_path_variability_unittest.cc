/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_path_variability.h"

#include "test/gtest.h"

namespace webrtc {

TEST(EchoPathVariability, CorrectBehavior) {
  // Test correct passing and reporting of the gain change information.
  EchoPathVariability v(
      true, EchoPathVariability::DelayAdjustment::kNewDetectedDelay, false);
  EXPECT_TRUE(v.gain_change);
  EXPECT_TRUE(v.delay_change ==
              EchoPathVariability::DelayAdjustment::kNewDetectedDelay);
  EXPECT_TRUE(v.AudioPathChanged());
  EXPECT_FALSE(v.clock_drift);

  v = EchoPathVariability(true, EchoPathVariability::DelayAdjustment::kNone,
                          false);
  EXPECT_TRUE(v.gain_change);
  EXPECT_TRUE(v.delay_change == EchoPathVariability::DelayAdjustment::kNone);
  EXPECT_TRUE(v.AudioPathChanged());
  EXPECT_FALSE(v.clock_drift);

  v = EchoPathVariability(
      false, EchoPathVariability::DelayAdjustment::kNewDetectedDelay, false);
  EXPECT_FALSE(v.gain_change);
  EXPECT_TRUE(v.delay_change ==
              EchoPathVariability::DelayAdjustment::kNewDetectedDelay);
  EXPECT_TRUE(v.AudioPathChanged());
  EXPECT_FALSE(v.clock_drift);

  v = EchoPathVariability(false, EchoPathVariability::DelayAdjustment::kNone,
                          false);
  EXPECT_FALSE(v.gain_change);
  EXPECT_TRUE(v.delay_change == EchoPathVariability::DelayAdjustment::kNone);
  EXPECT_FALSE(v.AudioPathChanged());
  EXPECT_FALSE(v.clock_drift);
}

}  // namespace webrtc
