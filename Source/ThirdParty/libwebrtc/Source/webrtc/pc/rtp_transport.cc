/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_transport.h"

#include <errno.h>

#include <cstdint>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "media/base/rtp_utils.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

void RtpTransport::SetRtcpMuxEnabled(bool enable) {
  rtcp_mux_enabled_ = enable;
  MaybeSignalReadyToSend();
}

const std::string& RtpTransport::transport_name() const {
  return rtp_packet_transport_->transport_name();
}

int RtpTransport::SetRtpOption(rtc::Socket::Option opt, int value) {
  return rtp_packet_transport_->SetOption(opt, value);
}

int RtpTransport::SetRtcpOption(rtc::Socket::Option opt, int value) {
  if (rtcp_packet_transport_) {
    return rtcp_packet_transport_->SetOption(opt, value);
  }
  return -1;
}

void RtpTransport::SetRtpPacketTransport(
    rtc::PacketTransportInternal* new_packet_transport) {
  if (new_packet_transport == rtp_packet_transport_) {
    return;
  }
  if (rtp_packet_transport_) {
    rtp_packet_transport_->SignalReadyToSend.disconnect(this);
    rtp_packet_transport_->DeregisterReceivedPacketCallback(this);
    rtp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtp_packet_transport_->SignalWritableState.disconnect(this);
    rtp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SendNetworkRouteChanged(absl::optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->RegisterReceivedPacketCallback(
        this, [&](rtc::PacketTransportInternal* transport,
                  const rtc::ReceivedPacket& packet) {
          OnReadPacket(transport, packet);
        });
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChanged);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SendNetworkRouteChanged(new_packet_transport->network_route());
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
    rtcp_packet_transport_->DeregisterReceivedPacketCallback(this);
    rtcp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtcp_packet_transport_->SignalWritableState.disconnect(this);
    rtcp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SendNetworkRouteChanged(absl::optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->RegisterReceivedPacketCallback(
        this, [&](rtc::PacketTransportInternal* transport,
                  const rtc::ReceivedPacket& packet) {
          OnReadPacket(transport, packet);
        });
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChanged);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SendNetworkRouteChanged(new_packet_transport->network_route());
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
  int ret = transport->SendPacket(packet->cdata<char>(), packet->size(),
                                  options, flags);
  if (ret != static_cast<int>(packet->size())) {
    if (transport->GetError() == ENOTCONN) {
      RTC_LOG(LS_WARNING) << "Got ENOTCONN from transport.";
      SetReadyToSend(rtcp, false);
    }
    return false;
  }
  return true;
}

void RtpTransport::UpdateRtpHeaderExtensionMap(
    const cricket::RtpHeaderExtensions& header_extensions) {
  header_extension_map_ = RtpHeaderExtensionMap(header_extensions);
}

bool RtpTransport::RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                                          RtpPacketSinkInterface* sink) {
  rtp_demuxer_.RemoveSink(sink);
  if (!rtp_demuxer_.AddSink(criteria, sink)) {
    RTC_LOG(LS_ERROR) << "Failed to register the sink for RTP demuxer.";
    return false;
  }
  return true;
}

bool RtpTransport::UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) {
  if (!rtp_demuxer_.RemoveSink(sink)) {
    RTC_LOG(LS_ERROR) << "Failed to unregister the sink for RTP demuxer.";
    return false;
  }
  return true;
}

flat_set<uint32_t> RtpTransport::GetSsrcsForSink(RtpPacketSinkInterface* sink) {
  return rtp_demuxer_.GetSsrcsForSink(sink);
}

void RtpTransport::DemuxPacket(rtc::CopyOnWriteBuffer packet,
                               webrtc::Timestamp arrival_time,
                               rtc::EcnMarking ecn) {
  RtpPacketReceived parsed_packet(&header_extension_map_);
  parsed_packet.set_arrival_time(arrival_time);
  parsed_packet.set_ecn(ecn);

  if (!parsed_packet.Parse(std::move(packet))) {
    RTC_LOG(LS_ERROR)
        << "Failed to parse the incoming RTP packet before demuxing. Drop it.";
    return;
  }

  if (!rtp_demuxer_.OnRtpPacket(parsed_packet)) {
    RTC_LOG(LS_VERBOSE) << "Failed to demux RTP packet: "
                        << RtpDemuxer::DescribePacket(parsed_packet);
    NotifyUnDemuxableRtpPacketReceived(parsed_packet);
  }
}

bool RtpTransport::IsTransportWritable() {
  auto rtcp_packet_transport =
      rtcp_mux_enabled_ ? nullptr : rtcp_packet_transport_;
  return rtp_packet_transport_ && rtp_packet_transport_->writable() &&
         (!rtcp_packet_transport || rtcp_packet_transport->writable());
}

void RtpTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  SetReadyToSend(transport == rtcp_packet_transport_, true);
}

void RtpTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  SendNetworkRouteChanged(network_route);
}

void RtpTransport::OnWritableState(
    rtc::PacketTransportInternal* packet_transport) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SendWritableState(IsTransportWritable());
}

void RtpTransport::OnSentPacket(rtc::PacketTransportInternal* packet_transport,
                                const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  if (processing_sent_packet_) {
    TaskQueueBase::Current()->PostTask(SafeTask(
        safety_.flag(), [this, sent_packet] { SendSentPacket(sent_packet); }));
    return;
  }
  processing_sent_packet_ = true;
  SendSentPacket(sent_packet);
  processing_sent_packet_ = false;
}

void RtpTransport::OnRtpPacketReceived(
    const rtc::ReceivedPacket& received_packet) {
  rtc::CopyOnWriteBuffer payload(received_packet.payload());
  DemuxPacket(
      payload,
      received_packet.arrival_time().value_or(Timestamp::MinusInfinity()),
      received_packet.ecn());
}

void RtpTransport::OnRtcpPacketReceived(
    const rtc::ReceivedPacket& received_packet) {
  rtc::CopyOnWriteBuffer payload(received_packet.payload());
  // TODO(bugs.webrtc.org/15368): Propagate timestamp and maybe received packet
  // further.
  SendRtcpPacketReceived(&payload, received_packet.arrival_time()
                                       ? received_packet.arrival_time()->us()
                                       : -1);
}

void RtpTransport::OnReadPacket(rtc::PacketTransportInternal* transport,
                                const rtc::ReceivedPacket& received_packet) {
  TRACE_EVENT0("webrtc", "RtpTransport::OnReadPacket");

  // When using RTCP multiplexing we might get RTCP packets on the RTP
  // transport. We check the RTP payload type to determine if it is RTCP.
  cricket::RtpPacketType packet_type =
      cricket::InferRtpPacketType(received_packet.payload());
  // Filter out the packet that is neither RTP nor RTCP.
  if (packet_type == cricket::RtpPacketType::kUnknown) {
    return;
  }

  // Protect ourselves against crazy data.
  if (!cricket::IsValidRtpPacketSize(packet_type,
                                     received_packet.payload().size())) {
    RTC_LOG(LS_ERROR) << "Dropping incoming "
                      << cricket::RtpPacketTypeToString(packet_type)
                      << " packet: wrong size="
                      << received_packet.payload().size();
    return;
  }

  if (packet_type == cricket::RtpPacketType::kRtcp) {
    OnRtcpPacketReceived(received_packet);
  } else {
    OnRtpPacketReceived(received_packet);
  }
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
    if (processing_ready_to_send_) {
      // Delay ReadyToSend processing until current operation is finished.
      // Note that this may not cause a signal, since ready_to_send may
      // have a new value by the time this executes.
      TaskQueueBase::Current()->PostTask(
          SafeTask(safety_.flag(), [this] { MaybeSignalReadyToSend(); }));
      return;
    }
    ready_to_send_ = ready_to_send;
    processing_ready_to_send_ = true;
    SendReadyToSend(ready_to_send);
    processing_ready_to_send_ = false;
  }
}

}  // namespace webrtc
