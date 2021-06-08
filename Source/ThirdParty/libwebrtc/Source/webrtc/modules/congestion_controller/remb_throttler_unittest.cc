/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/remb_throttler.h"

#include <vector>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::MockFunction;

TEST(RembThrottlerTest, CallRembSenderOnFirstReceiveBitrateChange) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);

  EXPECT_CALL(remb_sender, Call(12345, std::vector<uint32_t>({1, 2, 3})));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/12345);
}

TEST(RembThrottlerTest, ThrottlesSmallReceiveBitrateDecrease) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);

  EXPECT_CALL(remb_sender, Call);
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/12346);
  clock.AdvanceTime(TimeDelta::Millis(100));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/12345);

  EXPECT_CALL(remb_sender, Call(12345, _));
  clock.AdvanceTime(TimeDelta::Millis(101));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/12345);
}

TEST(RembThrottlerTest, DoNotThrottleLargeReceiveBitrateDecrease) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);

  EXPECT_CALL(remb_sender, Call(2345, _));
  EXPECT_CALL(remb_sender, Call(1234, _));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/2345);
  clock.AdvanceTime(TimeDelta::Millis(1));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/1234);
}

TEST(RembThrottlerTest, ThrottlesReceiveBitrateIncrease) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);

  EXPECT_CALL(remb_sender, Call);
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/1234);
  clock.AdvanceTime(TimeDelta::Millis(100));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/2345);

  // Updates 200ms after previous callback is not throttled.
  EXPECT_CALL(remb_sender, Call(2345, _));
  clock.AdvanceTime(TimeDelta::Millis(101));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/2345);
}

TEST(RembThrottlerTest, CallRembSenderOnSetMaxDesiredReceiveBitrate) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);
  EXPECT_CALL(remb_sender, Call(1234, _));
  remb_throttler.SetMaxDesiredReceiveBitrate(DataRate::BitsPerSec(1234));
}

TEST(RembThrottlerTest, CallRembSenderWithMinOfMaxDesiredAndOnReceivedBitrate) {
  SimulatedClock clock(Timestamp::Zero());
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  RembThrottler remb_throttler(remb_sender.AsStdFunction(), &clock);

  EXPECT_CALL(remb_sender, Call(1234, _));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/1234);
  clock.AdvanceTime(TimeDelta::Millis(1));
  remb_throttler.SetMaxDesiredReceiveBitrate(DataRate::BitsPerSec(4567));

  clock.AdvanceTime(TimeDelta::Millis(200));
  EXPECT_CALL(remb_sender, Call(4567, _));
  remb_throttler.OnReceiveBitrateChanged({1, 2, 3}, /*bitrate_bps=*/5678);
}

}  // namespace webrtc
