/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace {

using ::testing::_;
using ::testing::MockFunction;
using ::webrtc::rtcp::ReceiverReport;
using ::webrtc::rtcp::ReportBlock;

const uint32_t kSenderSsrc = 0x12345678;

TEST(RtcpPacketTest, BuildWithTooSmallBuffer) {
  ReportBlock rb;
  ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(rr.AddReportBlock(rb));

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  // No packet.
  MockFunction<void(rtc::ArrayView<const uint8_t>)> callback;
  EXPECT_CALL(callback, Call(_)).Times(0);
  const size_t kBufferSize = kRrLength + kReportBlockLength - 1;
  EXPECT_FALSE(rr.Build(kBufferSize, callback.AsStdFunction()));
}

}  // namespace
