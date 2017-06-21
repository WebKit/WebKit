/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/rtptransport.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/rtputils.h"
#include "webrtc/p2p/base/packettransportinterface.h"

namespace webrtc {

void RtpTransport::SetRtcpMuxEnabled(bool enable) {
  rtcp_mux_enabled_ = enable;
  MaybeSignalReadyToSend();
}

void RtpTransport::SetRtpPacketTransport(
    rtc::PacketTransportInternal* new_packet_transport) {
  if (new_packet_transport == rtp_packet_transport_) {
    return;
  }
  if (rtp_packet_transport_) {
    rtp_packet_transport_->SignalReadyToSend.disconnect(this);
    rtp_packet_transport_->SignalReadPacket.disconnect(this);
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
  }
  rtp_packet_transport_ = new_packet_transport;

  // Assumes the transport is ready to send if it is writable. If we are wrong,
  // ready to send will be updated the next time we try to send.
  SetReadyToSend(false,
                 rtp_packet_transport_ && rtp_packet_transport_->writable());
}

void RtpTransport::SetRtcpPacketTransport(
    rtc::PacketTransportInternal* new_packet_transport) {
  if (new_packet_transport == rtcp_packet_transport_) {
    return;
  }
  if (rtcp_packet_transport_) {
    rtcp_packet_transport_->SignalReadyToSend.disconnect(this);
    rtcp_packet_transport_->SignalReadPacket.disconnect(this);
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
  }
  rtcp_packet_transport_ = new_packet_transport;

  // Assumes the transport is ready to send if it is writable. If we are wrong,
  // ready to send will be updated the next time we try to send.
  SetReadyToSend(true,
                 rtcp_packet_transport_ && rtcp_packet_transport_->writable());
}

bool RtpTransport::IsWritable(bool rtcp) const {
  rtc::PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                                ? rtcp_packet_transport_
                                                : rtp_packet_transport_;
  return transport && transport->writable();
}

bool RtpTransport::SendPacket(bool rtcp,
                              const rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketOptions& options,
                              int flags) {
  rtc::PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                                ? rtcp_packet_transport_
                                                : rtp_packet_transport_;
  int ret = transport->SendPacket(packet->data<char>(), packet->size(), options,
                                  flags);
  if (ret != static_cast<int>(packet->size())) {
    if (transport->GetError() == ENOTCONN) {
      LOG(LS_WARNING) << "Got ENOTCONN from transport.";
      SetReadyToSend(rtcp, false);
    }
    return false;
  }
  return true;
}

bool RtpTransport::HandlesPacket(const uint8_t* data, size_t len) {
  return bundle_filter_.DemuxPacket(data, len);
}

bool RtpTransport::HandlesPayloadType(int payload_type) const {
  return bundle_filter_.FindPayloadType(payload_type);
}

void RtpTransport::AddHandledPayloadType(int payload_type) {
  bundle_filter_.AddPayloadType(payload_type);
}

PacketTransportInterface* RtpTransport::GetRtpPacketTransport() const {
  return rtp_packet_transport_;
}

PacketTransportInterface* RtpTransport::GetRtcpPacketTransport() const {
  return rtcp_packet_transport_;
}

RTCError RtpTransport::SetRtcpParameters(const RtcpParameters& parameters) {
  if (rtcp_parameters_.mux && !parameters.mux) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "Disabling RTCP muxing is not allowed.");
  }

  RtcpParameters new_parameters = parameters;

  if (new_parameters.cname.empty()) {
    new_parameters.cname = rtcp_parameters_.cname;
  }

  rtcp_parameters_ = new_parameters;
  return RTCError::OK();
}

RtcpParameters RtpTransport::GetRtcpParameters() const {
  return rtcp_parameters_;
}

RtpTransportAdapter* RtpTransport::GetInternal() {
  return nullptr;
}

void RtpTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  SetReadyToSend(transport == rtcp_packet_transport_, true);
}

void RtpTransport::SetReadyToSend(bool rtcp, bool ready) {
  if (rtcp) {
    rtcp_ready_to_send_ = ready;
  } else {
    rtp_ready_to_send_ = ready;
  }

  MaybeSignalReadyToSend();
}

void RtpTransport::MaybeSignalReadyToSend() {
  bool ready_to_send =
      rtp_ready_to_send_ && (rtcp_ready_to_send_ || rtcp_mux_enabled_);
  if (ready_to_send != ready_to_send_) {
    ready_to_send_ = ready_to_send;
    SignalReadyToSend(ready_to_send);
  }
}

// Check the RTP payload type.  If 63 < payload type < 96, it's RTCP.
// For additional details, see http://tools.ietf.org/html/rfc5761.
bool IsRtcp(const char* data, int len) {
  if (len < 2) {
    return false;
  }
  char pt = data[1] & 0x7F;
  return (63 < pt) && (pt < 96);
}

void RtpTransport::OnReadPacket(rtc::PacketTransportInternal* transport,
                                const char* data,
                                size_t len,
                                const rtc::PacketTime& packet_time,
                                int flags) {
  TRACE_EVENT0("webrtc", "RtpTransport::OnReadPacket");

  // When using RTCP multiplexing we might get RTCP packets on the RTP
  // transport. We check the RTP payload type to determine if it is RTCP.
  bool rtcp = transport == rtcp_packet_transport() ||
              IsRtcp(data, static_cast<int>(len));
  rtc::CopyOnWriteBuffer packet(data, len);

  if (!WantsPacket(rtcp, &packet)) {
    return;
  }

  // This mutates |packet| if it is protected.
  SignalPacketReceived(rtcp, packet, packet_time);
}

bool RtpTransport::WantsPacket(bool rtcp,
                               const rtc::CopyOnWriteBuffer* packet) {
  // Protect ourselves against crazy data.
  if (!packet || !cricket::IsValidRtpRtcpPacketSize(rtcp, packet->size())) {
    LOG(LS_ERROR) << "Dropping incoming " << cricket::RtpRtcpStringLiteral(rtcp)
                  << " packet: wrong size=" << packet->size();
    return false;
  }
  if (rtcp) {
    // Permit all (seemingly valid) RTCP packets.
    return true;
  }
  // Check whether we handle this payload.
  return HandlesPacket(packet->data(), packet->size());
}

}  // namespace webrtc
