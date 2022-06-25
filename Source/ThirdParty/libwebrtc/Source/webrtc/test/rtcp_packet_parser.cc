/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtcp_packet_parser.h"

#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

RtcpPacketParser::RtcpPacketParser() = default;
RtcpPacketParser::~RtcpPacketParser() = default;

bool RtcpPacketParser::Parse(const void* data, size_t length) {
  ++processed_rtcp_packets_;

  const uint8_t* const buffer = static_cast<const uint8_t*>(data);
  const uint8_t* const buffer_end = buffer + length;

  rtcp::CommonHeader header;
  for (const uint8_t* next_packet = buffer; next_packet != buffer_end;
       next_packet = header.NextPacket()) {
    RTC_DCHECK_GT(buffer_end - next_packet, 0);
    if (!header.Parse(next_packet, buffer_end - next_packet)) {
      RTC_LOG(LS_WARNING)
          << "Invalid rtcp header or unaligned rtcp packet at position "
          << (next_packet - buffer);
      return false;
    }
    switch (header.type()) {
      case rtcp::App::kPacketType:
        app_.Parse(header);
        break;
      case rtcp::Bye::kPacketType:
        bye_.Parse(header, &sender_ssrc_);
        break;
      case rtcp::ExtendedReports::kPacketType:
        xr_.Parse(header, &sender_ssrc_);
        break;
      case rtcp::ExtendedJitterReport::kPacketType:
        ij_.Parse(header);
        break;
      case rtcp::Psfb::kPacketType:
        switch (header.fmt()) {
          case rtcp::Fir::kFeedbackMessageType:
            fir_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::Pli::kFeedbackMessageType:
            pli_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::Psfb::kAfbMessageType:
            if (!loss_notification_.Parse(header, &sender_ssrc_) &&
                !remb_.Parse(header, &sender_ssrc_)) {
              RTC_LOG(LS_WARNING) << "Unknown application layer FB message.";
            }
            break;
          default:
            RTC_LOG(LS_WARNING)
                << "Unknown rtcp payload specific feedback type "
                << header.fmt();
            break;
        }
        break;
      case rtcp::ReceiverReport::kPacketType:
        receiver_report_.Parse(header, &sender_ssrc_);
        break;
      case rtcp::Rtpfb::kPacketType:
        switch (header.fmt()) {
          case rtcp::Nack::kFeedbackMessageType:
            nack_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::RapidResyncRequest::kFeedbackMessageType:
            rrr_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::Tmmbn::kFeedbackMessageType:
            tmmbn_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::Tmmbr::kFeedbackMessageType:
            tmmbr_.Parse(header, &sender_ssrc_);
            break;
          case rtcp::TransportFeedback::kFeedbackMessageType:
            transport_feedback_.Parse(header, &sender_ssrc_);
            break;
          default:
            RTC_LOG(LS_WARNING)
                << "Unknown rtcp transport feedback type " << header.fmt();
            break;
        }
        break;
      case rtcp::Sdes::kPacketType:
        sdes_.Parse(header);
        break;
      case rtcp::SenderReport::kPacketType:
        sender_report_.Parse(header, &sender_ssrc_);
        break;
      default:
        RTC_LOG(LS_WARNING) << "Unknown rtcp packet type " << header.type();
        break;
    }
  }
  return true;
}

}  // namespace test
}  // namespace webrtc
