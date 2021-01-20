/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/moving_median_filter.h"

#include <stdint.h>
#include <algorithm>

#include "test/gtest.h"

namespace webrtc {

TEST(MovingMedianFilterTest, ProcessesNoSamples) {
  MovingMedianFilter<int> filter(2);
  EXPECT_EQ(0, filter.GetFilteredValue());
  EXPECT_EQ(0u, filter.GetNumberOfSamplesStored());
}

TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow5) {
  MovingMedianFilter<int> filter(5);
  const int64_t kSamples[5] = {1, 5, 2, 3, 4};
  const int64_t kExpectedFilteredValues[5] = {1, 1, 2, 2, 3};
  for (size_t i = 0; i < 5; ++i) {
    filter.Insert(kSamples[i]);
    EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
    EXPECT_EQ(i + 1, filter.GetNumberOfSamplesStored());
  }
}

TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow3) {
  MovingMedianFilter<int> filter(3);
  const int64_t kSamples[5] = {1, 5, 2, 3, 4};
  const int64_t kExpectedFilteredValues[5] = {1, 1, 2, 3, 3};
  for (int i = 0; i < 5; ++i) {
    filter.Insert(kSamples[i]);
    EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
    EXPECT_EQ(std::min<size_t>(i + 1, 3), filter.GetNumberOfSamplesStored());
  }
}

TEST(MovingMedianFilterTest, ReturnsMovingMedianWindow1) {
  MovingMedianFilter<int> filter(1);
  const int64_t kSamples[5] = {1, 5, 2, 3, 4};
  const int64_t kExpectedFilteredValues[5] = {1, 5, 2, 3, 4};
  for (int i = 0; i < 5; ++i) {
    filter.Insert(kSamples[i]);
    EXPECT_EQ(kExpectedFilteredValues[i], filter.GetFilteredValue());
    EXPECT_EQ(1u, filter.GetNumberOfSamplesStored());
  }
}

}  // namespace webrtc
