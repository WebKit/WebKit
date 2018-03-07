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

using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;

TEST(RtcpPacketTest, BuildWithTooSmallBuffer) {
  ReportBlock rb;
  ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(rr.AddReportBlock(rb));

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  // No packet.
  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
    void OnPacketReady(uint8_t* data, size_t length) override {
      ADD_FAILURE() << "Packet should not fit within max size.";
    }
  } verifier;
  const size_t kBufferSize = kRrLength + kReportBlockLength - 1;
  uint8_t buffer[kBufferSize];
  EXPECT_FALSE(rr.BuildExternalBuffer(buffer, kBufferSize, &verifier));
}
}  // namespace webrtc
