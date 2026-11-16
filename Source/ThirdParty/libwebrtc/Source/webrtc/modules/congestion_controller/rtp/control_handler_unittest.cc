/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/control_handler.h"

#include <optional>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TargetTransferRate CreateTargetRate(DataRate target_rate,
                                    double cwnd_reduce_ratio = 0.0) {
  TargetTransferRate msg;
  msg.at_time = Timestamp::Seconds(100);
  msg.target_rate = target_rate;
  msg.cwnd_reduce_ratio = cwnd_reduce_ratio;
  msg.network_estimate.at_time = Timestamp::Seconds(100);
  msg.network_estimate.round_trip_time = TimeDelta::Millis(50);
  msg.network_estimate.loss_rate_ratio = 0.0;
  return msg;
}

TEST(CongestionControlHandlerTest, ReturnsUpdateOnFirstTargetRate) {
  CongestionControlHandler handler;
  handler.SetTargetRate(CreateTargetRate(DataRate::KilobitsPerSec(300)));
  std::optional<TargetTransferRate> update = handler.GetUpdate();
  ASSERT_TRUE(update.has_value());
  EXPECT_EQ(update->target_rate, DataRate::KilobitsPerSec(300));
}

TEST(CongestionControlHandlerTest, ReturnsNulloptOnIdenticalTargetRate) {
  CongestionControlHandler handler;
  handler.SetTargetRate(CreateTargetRate(DataRate::KilobitsPerSec(300)));
  EXPECT_TRUE(handler.GetUpdate().has_value());

  handler.SetTargetRate(CreateTargetRate(DataRate::KilobitsPerSec(300)));
  EXPECT_FALSE(handler.GetUpdate().has_value());
}

TEST(CongestionControlHandlerTest, ReturnsUpdateWhenCwndReduceRatioChanges) {
  CongestionControlHandler handler;
  handler.SetTargetRate(CreateTargetRate(DataRate::KilobitsPerSec(300),
                                         /*cwnd_reduce_ratio=*/0.0));
  std::optional<TargetTransferRate> update1 = handler.GetUpdate();
  ASSERT_TRUE(update1.has_value());
  EXPECT_EQ(update1->cwnd_reduce_ratio, 0.0);

  // Update with same target bitrate, RTT, and loss, but changed
  // cwnd_reduce_ratio.
  handler.SetTargetRate(CreateTargetRate(DataRate::KilobitsPerSec(300),
                                         /*cwnd_reduce_ratio=*/1.0));
  std::optional<TargetTransferRate> update2 = handler.GetUpdate();
  ASSERT_TRUE(update2.has_value());
  EXPECT_EQ(update2->cwnd_reduce_ratio, 1.0);
}

}  // namespace
}  // namespace webrtc
