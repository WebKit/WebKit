/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_rtcp_demuxer_helper.h"

#include <string.h>

#include <cstdio>

#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr uint32_t kSsrc = 8374;
}  // namespace

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_ByePacket) {
  webrtc::rtcp::Bye rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest,
     ParseRtcpPacketSenderSsrc_ExtendedReportsPacket) {
  webrtc::rtcp::ExtendedReports rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_PsfbPacket) {
  webrtc::rtcp::Pli rtcp_packet;  // Psfb is abstract; use a subclass.
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_ReceiverReportPacket) {
  webrtc::rtcp::ReceiverReport rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_RtpfbPacket) {
  // Rtpfb is abstract; use a subclass.
  webrtc::rtcp::RapidResyncRequest rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_SenderReportPacket) {
  webrtc::rtcp::SenderReport rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_EQ(ssrc, kSsrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_MalformedRtcpPacket) {
  uint8_t garbage[100];
  memset(&garbage[0], 0, arraysize(garbage));

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(garbage);
  EXPECT_FALSE(ssrc);
}

TEST(RtpRtcpDemuxerHelperTest,
     ParseRtcpPacketSenderSsrc_RtcpMessageWithoutSenderSsrc) {
  webrtc::rtcp::ExtendedJitterReport rtcp_packet;  // Has no sender SSRC.
  rtc::Buffer raw_packet = rtcp_packet.Build();

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(raw_packet);
  EXPECT_FALSE(ssrc);
}

TEST(RtpRtcpDemuxerHelperTest, ParseRtcpPacketSenderSsrc_TruncatedRtcpMessage) {
  webrtc::rtcp::Bye rtcp_packet;
  rtcp_packet.SetSenderSsrc(kSsrc);
  rtc::Buffer raw_packet = rtcp_packet.Build();

  constexpr size_t rtcp_length_bytes = 8;
  ASSERT_EQ(rtcp_length_bytes, raw_packet.size());

  absl::optional<uint32_t> ssrc = ParseRtcpPacketSenderSsrc(
      rtc::ArrayView<const uint8_t>(raw_packet.data(), rtcp_length_bytes - 1));
  EXPECT_FALSE(ssrc);
}

}  // namespace webrtc
