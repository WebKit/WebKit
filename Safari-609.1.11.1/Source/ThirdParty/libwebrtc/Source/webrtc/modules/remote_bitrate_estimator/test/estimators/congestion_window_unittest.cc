/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/estimators/congestion_window.h"
#include "modules/remote_bitrate_estimator/test/estimators/bbr.h"

#include "test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {
namespace {
// Same value used in CongestionWindow class.
const int64_t kStartingCongestionWindow = 6000;
}  // namespace

TEST(CongestionWindowTest, InitializationCheck) {
  CongestionWindow congestion_window;
  congestion_window.PacketSent(0);
  EXPECT_EQ(congestion_window.data_inflight(), 0u);
  congestion_window.AckReceived(0);
  EXPECT_EQ(congestion_window.data_inflight(), 0u);
}

TEST(CongestionWindowTest, DataInflight) {
  CongestionWindow congestion_window;
  congestion_window.PacketSent(13);
  EXPECT_EQ(congestion_window.data_inflight(), 13u);
  congestion_window.AckReceived(12);
  EXPECT_EQ(congestion_window.data_inflight(), 1u);
  congestion_window.PacketSent(10);
  congestion_window.PacketSent(9);
  EXPECT_EQ(congestion_window.data_inflight(), 20u);
  congestion_window.AckReceived(20);
  EXPECT_EQ(congestion_window.data_inflight(), 0u);
}

TEST(CongestionWindowTest, ZeroBandwidthDelayProduct) {
  CongestionWindow congestion_window;
  int64_t target_congestion_window =
      congestion_window.GetTargetCongestionWindow(100, 0, 2.885f);
  EXPECT_EQ(target_congestion_window, 2.885f * kStartingCongestionWindow);
}

TEST(CongestionWindowTest, CalculateCongestionWindow) {
  CongestionWindow congestion_window;
  int64_t cwnd = congestion_window.GetCongestionWindow(BbrBweSender::STARTUP,
                                                       800000, 100l, 2.885f);
  EXPECT_EQ(cwnd, 28850);
  cwnd = congestion_window.GetCongestionWindow(BbrBweSender::STARTUP, 400000,
                                               200l, 2.885f);
  EXPECT_EQ(cwnd, 28850);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
