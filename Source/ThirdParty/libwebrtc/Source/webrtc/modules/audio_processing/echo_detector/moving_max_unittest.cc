/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/echo_detector/moving_max.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

// Test if the maximum is correctly found.
TEST(MovingMaxTests, SimpleTest) {
  MovingMax test_moving_max(5);
  test_moving_max.Update(1.0f);
  test_moving_max.Update(1.1f);
  test_moving_max.Update(1.9f);
  test_moving_max.Update(1.87f);
  test_moving_max.Update(1.89f);
  EXPECT_EQ(1.9f, test_moving_max.max());
}

// Test if values fall out of the window when expected.
TEST(MovingMaxTests, SlidingWindowTest) {
  MovingMax test_moving_max(5);
  test_moving_max.Update(1.0f);
  test_moving_max.Update(1.9f);
  test_moving_max.Update(1.7f);
  test_moving_max.Update(1.87f);
  test_moving_max.Update(1.89f);
  test_moving_max.Update(1.3f);
  test_moving_max.Update(1.2f);
  EXPECT_LT(test_moving_max.max(), 1.9f);
}

// Test if Clear() works as expected.
TEST(MovingMaxTests, ClearTest) {
  MovingMax test_moving_max(5);
  test_moving_max.Update(1.0f);
  test_moving_max.Update(1.1f);
  test_moving_max.Update(1.9f);
  test_moving_max.Update(1.87f);
  test_moving_max.Update(1.89f);
  EXPECT_EQ(1.9f, test_moving_max.max());
  test_moving_max.Clear();
  EXPECT_EQ(0.f, test_moving_max.max());
}

// Test the decay of the estimated maximum.
TEST(MovingMaxTests, DecayTest) {
  MovingMax test_moving_max(1);
  test_moving_max.Update(1.0f);
  float previous_value = 1.0f;
  for (int i = 0; i < 500; i++) {
    test_moving_max.Update(0.0f);
    EXPECT_LT(test_moving_max.max(), previous_value);
    EXPECT_GT(test_moving_max.max(), 0.0f);
    previous_value = test_moving_max.max();
  }
  EXPECT_LT(test_moving_max.max(), 0.01f);
}

}  // namespace webrtc
