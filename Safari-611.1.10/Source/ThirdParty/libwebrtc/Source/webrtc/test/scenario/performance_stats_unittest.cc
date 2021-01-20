/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/performance_stats.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(EventRateCounter, ReturnsCorrectTotalDuration) {
  EventRateCounter event_rate_counter;
  EXPECT_EQ(event_rate_counter.TotalDuration(), TimeDelta::Zero());
  event_rate_counter.AddEvent(Timestamp::Seconds(1));
  EXPECT_EQ(event_rate_counter.TotalDuration(), TimeDelta::Zero());
  event_rate_counter.AddEvent(Timestamp::Seconds(2));
  EXPECT_EQ(event_rate_counter.TotalDuration(), TimeDelta::Seconds(1));
}

}  // namespace test
}  // namespace webrtc
