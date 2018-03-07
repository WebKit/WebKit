/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/moving_average.h"

#include "test/gtest.h"

TEST(MovingAverageTest, EmptyAverage) {
  webrtc::MovingAverage moving_average(1);
  EXPECT_EQ(0u, moving_average.size());
  EXPECT_FALSE(moving_average.GetAverage(0));
}

// Test single value.
TEST(MovingAverageTest, OneElement) {
  webrtc::MovingAverage moving_average(1);
  moving_average.AddSample(3);
  EXPECT_EQ(1u, moving_average.size());
  EXPECT_EQ(3, *moving_average.GetAverage());
  EXPECT_EQ(3, *moving_average.GetAverage(1));
  EXPECT_FALSE(moving_average.GetAverage(2));
}

TEST(MovingAverageTest, GetAverage) {
  webrtc::MovingAverage moving_average(1024);
  moving_average.AddSample(1);
  moving_average.AddSample(1);
  moving_average.AddSample(3);
  moving_average.AddSample(3);
  EXPECT_EQ(*moving_average.GetAverage(4), 2);
  EXPECT_EQ(*moving_average.GetAverage(2), 3);
  EXPECT_FALSE(moving_average.GetAverage(0));
}

TEST(MovingAverageTest, Reset) {
  webrtc::MovingAverage moving_average(5);
  moving_average.AddSample(1);
  EXPECT_EQ(1, *moving_average.GetAverage(1));
  moving_average.Reset();
  EXPECT_FALSE(moving_average.GetAverage(1));
  EXPECT_FALSE(moving_average.GetAverage(6));
}

TEST(MovingAverageTest, ManySamples) {
  webrtc::MovingAverage moving_average(10);
  for (int i = 1; i < 11; i++) {
    moving_average.AddSample(i);
  }
  EXPECT_EQ(*moving_average.GetAverage(), 5);
  moving_average.Reset();
  for (int i = 1; i < 2001; i++) {
    moving_average.AddSample(i);
  }
  EXPECT_EQ(*moving_average.GetAverage(), 1995);
}
