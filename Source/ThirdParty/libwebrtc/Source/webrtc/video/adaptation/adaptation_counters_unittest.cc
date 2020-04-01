/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/adaptation_counters.h"

#include "test/gtest.h"

namespace webrtc {

TEST(AdaptationCountersTest, Addition) {
  AdaptationCounters a{0, 0};
  AdaptationCounters b{1, 2};
  AdaptationCounters total = a + b;
  EXPECT_EQ(1, total.resolution_adaptations);
  EXPECT_EQ(2, total.fps_adaptations);
}

TEST(AdaptationCountersTest, Subtraction) {
  AdaptationCounters a{0, 1};
  AdaptationCounters b{2, 1};
  AdaptationCounters diff = a - b;
  EXPECT_EQ(-2, diff.resolution_adaptations);
  EXPECT_EQ(0, diff.fps_adaptations);
}

TEST(AdaptationCountersTest, Equality) {
  AdaptationCounters a{1, 2};
  AdaptationCounters b{2, 1};
  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
}

TEST(AdaptationCountersTest, SelfAdditionSubtraction) {
  AdaptationCounters a{1, 0};
  AdaptationCounters b{0, 1};

  EXPECT_EQ(a, a + b - b);
  EXPECT_EQ(a, b + a - b);
  EXPECT_EQ(a, a - b + b);
  EXPECT_EQ(a, b - b + a);
}

}  // namespace webrtc
