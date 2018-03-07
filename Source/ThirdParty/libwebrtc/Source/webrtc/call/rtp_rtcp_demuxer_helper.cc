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

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"

namespace webrtc {

rtc::Optional<uint32_t> ParseRtcpPacketSenderSsrc(
    rtc::ArrayView<const uint8_t> packet) {
  rtcp::CommonHeader header;
  for (const uint8_t* next_packet = packet.begin(); next_packet < packet.end();
       next_packet = header.NextPacket()) {
    if (!header.Parse(next_packet, packet.end() - next_packet)) {
      return rtc::Optional<uint32_t>();
    }

    switch (header.type()) {
      case rtcp::Bye::kPacketType:
      case rtcp::ExtendedReports::kPacketType:
      case rtcp::Psfb::kPacketType:
      case rtcp::ReceiverReport::kPacketType:
      case rtcp::Rtpfb::kPacketType:
      case rtcp::SenderReport::kPacketType: {
        // Sender SSRC at the beginning of the RTCP payload.
        if (header.payload_size_bytes() >= sizeof(uint32_t)) {
          const uint32_t ssrc_sender =
              ByteReader<uint32_t>::ReadBigEndian(header.payload());
          return rtc::Optional<uint32_t>(ssrc_sender);
        } else {
          return rtc::Optional<uint32_t>();
        }
      }
    }
  }

  return rtc::Optional<uint32_t>();
}

}  // namespace webrtc
