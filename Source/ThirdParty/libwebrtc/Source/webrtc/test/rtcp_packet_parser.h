/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_TEST_RTCP_PACKET_PARSER_H_
#define WEBRTC_TEST_RTCP_PACKET_PARSER_H_

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace webrtc {
namespace test {
// Parse RTCP packet of given type. Assumes RTCP header is valid and that there
// is excatly one packet of correct type in the buffer.
template <typename Packet>
bool ParseSinglePacket(const uint8_t* buffer, size_t size, Packet* packet) {
  rtcp::CommonHeader header;
  RTC_CHECK(header.Parse(buffer, size));
  RTC_CHECK_EQ(size, header.NextPacket() - buffer);
  return packet->Parse(header);
}
// Same function, but takes raw buffer as single argument instead of pair.
template <typename Packet>
bool ParseSinglePacket(rtc::ArrayView<const uint8_t> buffer, Packet* packet) {
  return ParseSinglePacket(buffer.data(), buffer.size(), packet);
}

class RtcpPacketParser {
 public:
  // Keeps last parsed packet, count number of parsed packets of given type.
  template <typename TypedRtcpPacket>
  class PacketCounter : public TypedRtcpPacket {
   public:
    int num_packets() const { return num_packets_; }
    void Parse(const rtcp::CommonHeader& header) {
      if (TypedRtcpPacket::Parse(header))
        ++num_packets_;
    }
    void Parse(const rtcp::CommonHeader& header, uint32_t* sender_ssrc) {
      if (TypedRtcpPacket::Parse(header)) {
        ++num_packets_;
        if (*sender_ssrc == 0)  // Use first sender ssrc in compound packet.
          *sender_ssrc = TypedRtcpPacket::sender_ssrc();
      }
    }

   private:
    int num_packets_ = 0;
  };

  RtcpPacketParser();
  ~RtcpPacketParser();

  bool Parse(const void* packet, size_t packet_len);

  PacketCounter<rtcp::App>* app() { return &app_; }
  PacketCounter<rtcp::Bye>* bye() { return &bye_; }
  PacketCounter<rtcp::ExtendedJitterReport>* ij() { return &ij_; }
  PacketCounter<rtcp::ExtendedReports>* xr() { return &xr_; }
  PacketCounter<rtcp::Fir>* fir() { return &fir_; }
  PacketCounter<rtcp::Nack>* nack() { return &nack_; }
  PacketCounter<rtcp::Pli>* pli() { return &pli_; }
  PacketCounter<rtcp::RapidResyncRequest>* rrr() { return &rrr_; }
  PacketCounter<rtcp::ReceiverReport>* receiver_report() {
    return &receiver_report_;
  }
  PacketCounter<rtcp::Remb>* remb() { return &remb_; }
  PacketCounter<rtcp::Rpsi>* rpsi() { return &rpsi_; }
  PacketCounter<rtcp::Sdes>* sdes() { return &sdes_; }
  PacketCounter<rtcp::SenderReport>* sender_report() { return &sender_report_; }
  PacketCounter<rtcp::Sli>* sli() { return &sli_; }
  PacketCounter<rtcp::Tmmbn>* tmmbn() { return &tmmbn_; }
  PacketCounter<rtcp::Tmmbr>* tmmbr() { return &tmmbr_; }
  PacketCounter<rtcp::TransportFeedback>* transport_feedback() {
    return &transport_feedback_;
  }
  uint32_t sender_ssrc() const { return sender_ssrc_; }

 private:
  PacketCounter<rtcp::App> app_;
  PacketCounter<rtcp::Bye> bye_;
  PacketCounter<rtcp::ExtendedJitterReport> ij_;
  PacketCounter<rtcp::ExtendedReports> xr_;
  PacketCounter<rtcp::Fir> fir_;
  PacketCounter<rtcp::Nack> nack_;
  PacketCounter<rtcp::Pli> pli_;
  PacketCounter<rtcp::RapidResyncRequest> rrr_;
  PacketCounter<rtcp::ReceiverReport> receiver_report_;
  PacketCounter<rtcp::Remb> remb_;
  PacketCounter<rtcp::Rpsi> rpsi_;
  PacketCounter<rtcp::Sdes> sdes_;
  PacketCounter<rtcp::SenderReport> sender_report_;
  PacketCounter<rtcp::Sli> sli_;
  PacketCounter<rtcp::Tmmbn> tmmbn_;
  PacketCounter<rtcp::Tmmbr> tmmbr_;
  PacketCounter<rtcp::TransportFeedback> transport_feedback_;
  uint32_t sender_ssrc_ = 0;
};

}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_TEST_RTCP_PACKET_PARSER_H_
