/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/estimators/min_rtt_filter.h"

#include "test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {
TEST(MinRttFilterTest, InitializationCheck) {
  MinRttFilter min_rtt_filter;
  EXPECT_FALSE(min_rtt_filter.min_rtt_ms());
}

TEST(MinRttFilterTest, AddRttSample) {
  MinRttFilter min_rtt_filter;
  min_rtt_filter.AddRttSample(120, 5);
  EXPECT_EQ(*min_rtt_filter.min_rtt_ms(), 120);
  min_rtt_filter.AddRttSample(121, 6);
  min_rtt_filter.AddRttSample(119, 7);
  min_rtt_filter.AddRttSample(140, 10007);
  EXPECT_EQ(*min_rtt_filter.min_rtt_ms(), 119);
}

TEST(MinRttFilterTest, MinRttExpired) {
  MinRttFilter min_rtt_filter;
  for (int i = 1; i <= 25; i++)
    min_rtt_filter.AddRttSample(i, i);
  EXPECT_EQ(min_rtt_filter.MinRttExpired(25), true);
  EXPECT_EQ(min_rtt_filter.MinRttExpired(24), false);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
