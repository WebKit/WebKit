/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/unique_timestamp_counter.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(UniqueTimestampCounterTest, InitiallyZero) {
  UniqueTimestampCounter counter;
  EXPECT_EQ(counter.GetUniqueSeen(), 0);
}

TEST(UniqueTimestampCounterTest, CountsUniqueValues) {
  UniqueTimestampCounter counter;
  counter.Add(100);
  counter.Add(100);
  counter.Add(200);
  counter.Add(150);
  counter.Add(100);
  EXPECT_EQ(counter.GetUniqueSeen(), 3);
}

TEST(UniqueTimestampCounterTest, ForgetsOldValuesAfter1000NewValues) {
  const int kNumValues = 1500;
  const int kMaxHistory = 1000;
  const uint32_t value = 0xFFFFFFF0;
  UniqueTimestampCounter counter;
  for (int i = 0; i < kNumValues; ++i) {
    counter.Add(value + 10 * i);
  }
  ASSERT_EQ(counter.GetUniqueSeen(), kNumValues);
  // Slightly old values not affect number of seen unique values.
  for (int i = kNumValues - kMaxHistory; i < kNumValues; ++i) {
    counter.Add(value + 10 * i);
  }
  EXPECT_EQ(counter.GetUniqueSeen(), kNumValues);
  // Very old value will be treated as unique.
  counter.Add(value);
  EXPECT_EQ(counter.GetUniqueSeen(), kNumValues + 1);
}

}  // namespace
}  // namespace webrtc
