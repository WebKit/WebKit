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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/ecn_marking.h"
#include "api/units/timestamp.h"
#include "call/rtp_demuxer.h"
#include "media/base/rtp_utils.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

void RtpTransport::SetRtcpMuxEnabled(bool enable) {
  rtcp_mux_enabled_ = enable;
  MaybeSignalReadyToSend();
}

const std::string& RtpTransport::transport_name() const {
  return rtp_packet_transport_->transport_name();
}

int RtpTransport::SetRtpOption(Socket::Option opt, int value) {
  return rtp_packet_transport_->SetOption(opt, value);
}

int RtpTransport::SetRtcpOption(Socket::Option opt, int value) {
  if (rtcp_packet_transport_) {
    return rtcp_packet_transport_->SetOption(opt, value);
  }
  return -1;
}

void RtpTransport::ChangePacketTransport(
    PacketTransportInternal* new_packet_transport,
    PacketTransportInternal*& transport_to_change) {
  if (new_packet_transport == transport_to_change) {
    return;
  }
  if (transport_to_change) {
    transport_to_change->UnsubscribeReadyToSend(this);
    transport_to_change->DeregisterReceivedPacketCallback(this);
    transport_to_change->UnsubscribeNetworkRouteChanged(this);
    transport_to_change->UnsubscribeWritableState(this);
    transport_to_change->UnsubscribeSentPacket(this);
    // Reset the network route of the old transport.
    SendNetworkRouteChanged(std::optional<NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SubscribeReadyToSend(
        this, [this](PacketTransportInternal* transport) {
          OnReadyToSend(transport);
        });
    new_packet_transport->RegisterReceivedPacketCallback(
        this, [&](PacketTransportInternal* transport,
                  const ReceivedIpPacket& packet) {
          OnReadPacket(transport, packet);
        });
    new_packet_transport->SubscribeNetworkRouteChanged(
        this, [this](std::optional<NetworkRoute> network_route) {
          OnNetworkRouteChanged(network_route);
        });
    new_packet_transport->SubscribeWritableState(
        this, [this](PacketTransportInternal* transport) {
          OnWritableState(transport);
        });
    new_packet_transport->SubscribeSentPacket(
        this, [this, flag = safety_.flag()](PacketTransportInternal* transport,
                                            const SentPacketInfo& info) {
          if (flag->alive()) {
            OnSentPacket(transport, info);
          }
        });
    // Set the network route for the new transport.
    SendNetworkRouteChanged(new_packet_transport->network_route());
  }

  transport_to_change = new_packet_transport;
}

void RtpTransport::SetRtcpPacketTransportOwned(
    std::unique_ptr<PacketTransportInternal> new_packet_transport) {
  SetRtcpPacketTransport(new_packet_transport.get());
  owned_rtcp_packet_transport_ = std::move(new_packet_transport);
}

void RtpTransport::SetRtpPacketTransportOwned(
    std::unique_ptr<PacketTransportInternal> new_packet_transport) {
  SetRtpPacketTransport(new_packet_transport.get());
  owned_rtp_packet_transport_ = std::move(new_packet_transport);
}

void RtpTransport::SetRtpPacketTransport(
    PacketTransportInternal* new_packet_transport) {
  std::unique_ptr<PacketTransportInternal> delete_on_exit;
  if (new_packet_transport != owned_rtp_packet_transport_.get()) {
    delete_on_exit = std::move(owned_rtp_packet_transport_);
  }
  ChangePacketTransport(new_packet_transport, rtp_packet_transport_);
  // Assumes the transport is ready to send if it is writable.
  SetReadyToSend(/* rtcp= */ false,
                 rtp_packet_transport_ && rtp_packet_transport_->writable());
}

void RtpTransport::SetRtcpPacketTransport(
    PacketTransportInternal* new_packet_transport) {
  std::unique_ptr<PacketTransportInternal> delete_on_exit;
  if (new_packet_transport != owned_rtcp_packet_transport_.get()) {
    // rtcp_packet_transport_ might still point to owned_rtcp_packet_transport_,
    // so move the owned object to delete_on_exit while we change the transport.
    delete_on_exit = std::move(owned_rtcp_packet_transport_);
  }
  ChangePacketTransport(new_packet_transport, rtcp_packet_transport_);
  // Assumes the transport is ready to send if it is writable.
  SetReadyToSend(/* rtcp= */ true,
                 rtcp_packet_transport_ && rtcp_packet_transport_->writable());
}

bool RtpTransport::IsWritable(bool rtcp) const {
  PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                           ? rtcp_packet_transport_
                                           : rtp_packet_transport_;
  return transport && transport->writable();
}

bool RtpTransport::SendRtpPacket(CopyOnWriteBuffer* packet,
                                 const AsyncSocketPacketOptions& options,
                                 int flags) {
  return SendPacket(false, packet, options, flags);
}

bool RtpTransport::SendRtcpPacket(CopyOnWriteBuffer* packet,
                                  const AsyncSocketPacketOptions& options,
                                  int flags) {
  if (received_rtp_with_ecn_) {
    AsyncSocketPacketOptions options_with_send_as_ect1 = options;
    options_with_send_as_ect1.ect_1 = true;
    return SendPacket(true, packet, options_with_send_as_ect1, flags);
  } else {
    return SendPacket(true, packet, options, flags);
  }
}

bool RtpTransport::SendPacket(bool rtcp,
                              CopyOnWriteBuffer* packet,
                              const AsyncSocketPacketOptions& options,
                              int flags) {
  PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                           ? rtcp_packet_transport_
                                           : rtp_packet_transport_;
  int ret = transport->SendPacket(packet->cdata<char>(), packet->size(),
                                  options, flags);
  if (ret != static_cast<int>(packet->size())) {
    return false;
  }
  return true;
}

void RtpTransport::UpdateRtpHeaderExtensionMap(
    const RtpHeaderExtensions& header_extensions) {
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

void RtpTransport::DemuxPacket(CopyOnWriteBuffer packet,
                               Timestamp arrival_time,
                               EcnMarking ecn) {
  RtpPacketReceived parsed_packet(&header_extension_map_);
  parsed_packet.set_arrival_time(arrival_time);
  parsed_packet.set_ecn(ecn);
  received_rtp_with_ecn_ = (ecn == EcnMarking::kEct1 || ecn == EcnMarking::kCe);

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

void RtpTransport::OnReadyToSend(PacketTransportInternal* transport) {
  SetReadyToSend(transport == rtcp_packet_transport_, true);
}

void RtpTransport::OnNetworkRouteChanged(
    std::optional<NetworkRoute> network_route) {
  SendNetworkRouteChanged(network_route);
}

void RtpTransport::OnWritableState(PacketTransportInternal* packet_transport) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SendWritableState(IsTransportWritable());
}

void RtpTransport::OnSentPacket(PacketTransportInternal* packet_transport,
                                const SentPacketInfo& sent_packet) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SendSentPacket(sent_packet);
}

void RtpTransport::OnRtpPacketReceived(
    const ReceivedIpPacket& received_packet) {
  CopyOnWriteBuffer payload(received_packet.payload());
  DemuxPacket(
      payload,
      received_packet.arrival_time().value_or(Timestamp::MinusInfinity()),
      received_packet.ecn());
}

void RtpTransport::OnRtcpPacketReceived(
    const ReceivedIpPacket& received_packet) {
  SendRtcpPacketReceived(CopyOnWriteBuffer(received_packet.payload()),
                         received_packet.arrival_time(), received_packet.ecn());
}

void RtpTransport::OnReadPacket(PacketTransportInternal* transport,
                                const ReceivedIpPacket& received_packet) {
  TRACE_EVENT0("webrtc", "RtpTransport::OnReadPacket");

  // When using RTCP multiplexing we might get RTCP packets on the RTP
  // transport. We check the RTP payload type to determine if it is RTCP.
  RtpPacketType packet_type = InferRtpPacketType(received_packet.payload());
  // Filter out the packet that is neither RTP nor RTCP.
  if (packet_type == RtpPacketType::kUnknown) {
    return;
  }

  // Protect ourselves against crazy data.
  if (!IsValidRtpPacketSize(packet_type, received_packet.payload().size())) {
    RTC_LOG(LS_ERROR) << "Dropping incoming "
                      << RtpPacketTypeToString(packet_type)
                      << " packet: wrong size="
                      << received_packet.payload().size();
    return;
  }

  if (packet_type == RtpPacketType::kRtcp) {
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
