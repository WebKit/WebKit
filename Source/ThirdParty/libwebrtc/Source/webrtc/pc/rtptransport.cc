/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtptransport.h"

#include "media/base/rtputils.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/packettransportinterface.h"
#include "rtc_base/checks.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/trace_event.h"

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
    rtp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtp_packet_transport_->SignalWritableState.disconnect(this);
    rtp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SignalNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChange);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SignalNetworkRouteChanged(new_packet_transport->network_route());
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
    rtcp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtcp_packet_transport_->SignalWritableState.disconnect(this);
    rtcp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SignalNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChange);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SignalNetworkRouteChanged(new_packet_transport->network_route());
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

bool RtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                 const rtc::PacketOptions& options,
                                 int flags) {
  return SendPacket(false, packet, options, flags);
}

bool RtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                  const rtc::PacketOptions& options,
                                  int flags) {
  return SendPacket(true, packet, options, flags);
}

bool RtpTransport::SendPacket(bool rtcp,
                              rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketOptions& options,
                              int flags) {
  rtc::PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                                ? rtcp_packet_transport_
                                                : rtp_packet_transport_;
  int ret = transport->SendPacket(packet->data<char>(), packet->size(), options,
                                  flags);
  if (ret != static_cast<int>(packet->size())) {
    if (transport->GetError() == ENOTCONN) {
      RTC_LOG(LS_WARNING) << "Got ENOTCONN from transport.";
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

RTCError RtpTransport::SetParameters(const RtpTransportParameters& parameters) {
  if (parameters_.rtcp.mux && !parameters.rtcp.mux) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "Disabling RTCP muxing is not allowed.");
  }
  if (parameters.keepalive != parameters_.keepalive) {
    // TODO(sprang): Wire up support for keep-alive (only ORTC support for now).
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "RTP keep-alive parameters not supported by this channel.");
  }

  RtpTransportParameters new_parameters = parameters;

  if (new_parameters.rtcp.cname.empty()) {
    new_parameters.rtcp.cname = parameters_.rtcp.cname;
  }

  parameters_ = new_parameters;
  return RTCError::OK();
}

RtpTransportParameters RtpTransport::GetParameters() const {
  return parameters_;
}

RtpTransportAdapter* RtpTransport::GetInternal() {
  return nullptr;
}

bool RtpTransport::IsRtpTransportWritable() {
  auto rtcp_packet_transport =
      rtcp_mux_enabled_ ? nullptr : rtcp_packet_transport_;
  return rtp_packet_transport_ && rtp_packet_transport_->writable() &&
         (!rtcp_packet_transport || rtcp_packet_transport->writable());
}

void RtpTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  SetReadyToSend(transport == rtcp_packet_transport_, true);
}

void RtpTransport::OnNetworkRouteChange(
    rtc::Optional<rtc::NetworkRoute> network_route) {
  SignalNetworkRouteChanged(network_route);
}

void RtpTransport::OnWritableState(
    rtc::PacketTransportInternal* packet_transport) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SignalWritableState(IsRtpTransportWritable());
}

void RtpTransport::OnSentPacket(rtc::PacketTransportInternal* packet_transport,
                                const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SignalSentPacket(sent_packet);
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
  SignalPacketReceived(rtcp, &packet, packet_time);
}

bool RtpTransport::WantsPacket(bool rtcp,
                               const rtc::CopyOnWriteBuffer* packet) {
  // Protect ourselves against crazy data.
  if (!packet || !cricket::IsValidRtpRtcpPacketSize(rtcp, packet->size())) {
    RTC_LOG(LS_ERROR) << "Dropping incoming "
                      << cricket::RtpRtcpStringLiteral(rtcp)
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
