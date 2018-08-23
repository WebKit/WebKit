/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/sample_counter.h"

#include <utility>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

using testing::Eq;

namespace rtc {

TEST(SampleCounterTest, ProcessesNoSamples) {
  constexpr int kMinSamples = 1;
  SampleCounter counter;
  EXPECT_THAT(counter.Avg(kMinSamples), Eq(absl::nullopt));
  EXPECT_THAT(counter.Avg(kMinSamples), Eq(absl::nullopt));
  EXPECT_THAT(counter.Max(), Eq(absl::nullopt));
}

TEST(SampleCounterTest, NotEnoughSamples) {
  constexpr int kMinSamples = 6;
  SampleCounter counter;
  for (int value : {1, 2, 3, 4, 5}) {
    counter.Add(value);
  }
  EXPECT_THAT(counter.Avg(kMinSamples), Eq(absl::nullopt));
  EXPECT_THAT(counter.Avg(kMinSamples), Eq(absl::nullopt));
  EXPECT_THAT(counter.Max(), Eq(5));
}

TEST(SampleCounterTest, EnoughSamples) {
  constexpr int kMinSamples = 5;
  SampleCounter counter;
  for (int value : {1, 2, 3, 4, 5}) {
    counter.Add(value);
  }
  EXPECT_THAT(counter.Avg(kMinSamples), Eq(3));
  EXPECT_THAT(counter.Variance(kMinSamples), Eq(2));
  EXPECT_THAT(counter.Max(), Eq(5));
}

}  // namespace rtc
