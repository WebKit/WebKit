/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <iterator>
#include <utility>

#include "pc/channel.h"

#include "api/call/audio_sink.h"
#include "media/base/mediaconstants.h"
#include "media/base/rtputils.h"
#include "rtc_base/bind.h"
#include "rtc_base/byteorder.h"
#include "rtc_base/checks.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/trace_event.h"
// Adding 'nogncheck' to disable the gn include headers check to support modular
// WebRTC build targets.
#include "media/engine/webrtcvoiceengine.h"  // nogncheck
#include "p2p/base/packettransportinternal.h"
#include "pc/channelmanager.h"
#include "pc/rtpmediautils.h"

namespace cricket {
using rtc::Bind;
using webrtc::SdpType;

namespace {

struct SendPacketMessageData : public rtc::MessageData {
  rtc::CopyOnWriteBuffer packet;
  rtc::PacketOptions options;
};

}  // namespace

enum {
  MSG_EARLYMEDIATIMEOUT = 1,
  MSG_SEND_RTP_PACKET,
  MSG_SEND_RTCP_PACKET,
  MSG_READYTOSENDDATA,
  MSG_DATARECEIVED,
  MSG_FIRSTPACKETRECEIVED,
};

static void SafeSetError(const std::string& message, std::string* error_desc) {
  if (error_desc) {
    *error_desc = message;
  }
}

static bool ValidPacket(bool rtcp, const rtc::CopyOnWriteBuffer* packet) {
  // Check the packet size. We could check the header too if needed.
  return packet && IsValidRtpRtcpPacketSize(rtcp, packet->size());
}

template <class Codec>
void RtpParametersFromMediaDescription(
    const MediaContentDescriptionImpl<Codec>* desc,
    const RtpHeaderExtensions& extensions,
    RtpParameters<Codec>* params) {
  // TODO(pthatcher): Remove this once we're sure no one will give us
  // a description without codecs. Currently the ORTC implementation is relying
  // on this.
  if (desc->has_codecs()) {
    params->codecs = desc->codecs();
  }
  // TODO(pthatcher): See if we really need
  // rtp_header_extensions_set() and remove it if we don't.
  if (desc->rtp_header_extensions_set()) {
    params->extensions = extensions;
  }
  params->rtcp.reduced_size = desc->rtcp_reduced_size();
}

template <class Codec>
void RtpSendParametersFromMediaDescription(
    const MediaContentDescriptionImpl<Codec>* desc,
    const RtpHeaderExtensions& extensions,
    RtpSendParameters<Codec>* send_params) {
  RtpParametersFromMediaDescription(desc, extensions, send_params);
  send_params->max_bandwidth_bps = desc->bandwidth();
}

BaseChannel::BaseChannel(rtc::Thread* worker_thread,
                         rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         std::unique_ptr<MediaChannel> media_channel,
                         const std::string& content_name,
                         bool rtcp_mux_required,
                         bool srtp_required)
    : worker_thread_(worker_thread),
      network_thread_(network_thread),
      signaling_thread_(signaling_thread),
      content_name_(content_name),
      rtcp_mux_required_(rtcp_mux_required),
      unencrypted_rtp_transport_(
          rtc::MakeUnique<webrtc::RtpTransport>(rtcp_mux_required)),
      srtp_required_(srtp_required),
      media_channel_(std::move(media_channel)) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  rtp_transport_ = unencrypted_rtp_transport_.get();
  ConnectToRtpTransport();
  RTC_LOG(LS_INFO) << "Created channel for " << content_name;
}

BaseChannel::~BaseChannel() {
  TRACE_EVENT0("webrtc", "BaseChannel::~BaseChannel");
  RTC_DCHECK_RUN_ON(worker_thread_);
  Deinit();
  StopConnectionMonitor();
  // Eats any outstanding messages or packets.
  worker_thread_->Clear(&invoker_);
  worker_thread_->Clear(this);
  // We must destroy the media channel before the transport channel, otherwise
  // the media channel may try to send on the dead transport channel. NULLing
  // is not an effective strategy since the sends will come on another thread.
  media_channel_.reset();
  RTC_LOG(LS_INFO) << "Destroyed channel: " << content_name_;
}

void BaseChannel::ConnectToRtpTransport() {
  RTC_DCHECK(rtp_transport_);
  rtp_transport_->SignalReadyToSend.connect(
      this, &BaseChannel::OnTransportReadyToSend);
  // TODO(zstein):  RtpTransport::SignalPacketReceived will probably be replaced
  // with a callback interface later so that the demuxer can select which
  // channel to signal.
  rtp_transport_->SignalPacketReceived.connect(this,
                                               &BaseChannel::OnPacketReceived);
  rtp_transport_->SignalNetworkRouteChanged.connect(
      this, &BaseChannel::OnNetworkRouteChanged);
  rtp_transport_->SignalWritableState.connect(this,
                                              &BaseChannel::OnWritableState);
  rtp_transport_->SignalSentPacket.connect(this,
                                           &BaseChannel::SignalSentPacket_n);
}

void BaseChannel::DisconnectFromRtpTransport() {
  RTC_DCHECK(rtp_transport_);
  rtp_transport_->SignalReadyToSend.disconnect(this);
  rtp_transport_->SignalPacketReceived.disconnect(this);
  rtp_transport_->SignalNetworkRouteChanged.disconnect(this);
  rtp_transport_->SignalWritableState.disconnect(this);
  rtp_transport_->SignalSentPacket.disconnect(this);
}

void BaseChannel::Init_w(DtlsTransportInternal* rtp_dtls_transport,
                         DtlsTransportInternal* rtcp_dtls_transport,
                         rtc::PacketTransportInternal* rtp_packet_transport,
                         rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  network_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    SetTransports_n(rtp_dtls_transport, rtcp_dtls_transport,
                    rtp_packet_transport, rtcp_packet_transport);

    if (rtcp_mux_required_) {
      rtcp_mux_filter_.SetActive();
    }
  });

  // Both RTP and RTCP channels should be set, we can call SetInterface on
  // the media channel and it can set network options.
  media_channel_->SetInterface(this);
}

void BaseChannel::Init_w(webrtc::RtpTransportInternal* rtp_transport) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  network_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    SetRtpTransport(rtp_transport);

    if (rtcp_mux_required_) {
      rtcp_mux_filter_.SetActive();
    }
  });

  // Both RTP and RTCP channels should be set, we can call SetInterface on
  // the media channel and it can set network options.
  media_channel_->SetInterface(this);
}

void BaseChannel::Deinit() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  media_channel_->SetInterface(NULL);
  // Packets arrive on the network thread, processing packets calls virtual
  // functions, so need to stop this process in Deinit that is called in
  // derived classes destructor.
  network_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    FlushRtcpMessages_n();

    if (dtls_srtp_transport_) {
      dtls_srtp_transport_->SetDtlsTransports(nullptr, nullptr);
    } else {
      rtp_transport_->SetRtpPacketTransport(nullptr);
      rtp_transport_->SetRtcpPacketTransport(nullptr);
    }
    // Clear pending read packets/messages.
    network_thread_->Clear(&invoker_);
    network_thread_->Clear(this);
  });
}

void BaseChannel::SetRtpTransport(webrtc::RtpTransportInternal* rtp_transport) {
  if (!network_thread_->IsCurrent()) {
    network_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      SetRtpTransport(rtp_transport);
      return;
    });
  }

  RTC_DCHECK(rtp_transport);

  if (rtp_transport_) {
    DisconnectFromRtpTransport();
  }
  rtp_transport_ = rtp_transport;
  RTC_LOG(LS_INFO) << "Setting the RtpTransport for " << content_name();
  ConnectToRtpTransport();

  UpdateWritableState_n();
}

void BaseChannel::SetTransports(DtlsTransportInternal* rtp_dtls_transport,
                                DtlsTransportInternal* rtcp_dtls_transport) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(&BaseChannel::SetTransports_n, this, rtp_dtls_transport,
           rtcp_dtls_transport, rtp_dtls_transport, rtcp_dtls_transport));
}

void BaseChannel::SetTransports(
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE, Bind(&BaseChannel::SetTransports_n, this, nullptr, nullptr,
                          rtp_packet_transport, rtcp_packet_transport));
}

void BaseChannel::SetTransports_n(
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Validate some assertions about the input.
  RTC_DCHECK(rtp_packet_transport);
  RTC_DCHECK_EQ(NeedsRtcpTransport(), rtcp_packet_transport != nullptr);
  if (rtp_dtls_transport || rtcp_dtls_transport) {
    // DTLS/non-DTLS pointers should be to the same object.
    RTC_DCHECK(rtp_dtls_transport == rtp_packet_transport);
    RTC_DCHECK(rtcp_dtls_transport == rtcp_packet_transport);
    // Can't go from non-DTLS to DTLS.
    RTC_DCHECK(!rtp_transport_->rtp_packet_transport() || rtp_dtls_transport_);
  } else {
    // Can't go from DTLS to non-DTLS.
    RTC_DCHECK(!rtp_dtls_transport_);
  }
  // Transport names should be the same.
  if (rtp_dtls_transport && rtcp_dtls_transport) {
    RTC_DCHECK(rtp_dtls_transport->transport_name() ==
               rtcp_dtls_transport->transport_name());
  }

  if (rtp_packet_transport == rtp_transport_->rtp_packet_transport()) {
    // Nothing to do if transport isn't changing.
    return;
  }

  std::string debug_name;
  if (rtp_dtls_transport) {
    transport_name_ = rtp_dtls_transport->transport_name();
    debug_name = transport_name_;
  } else {
    debug_name = rtp_packet_transport->transport_name();
  }
  // If this BaseChannel doesn't require RTCP mux and we haven't fully
  // negotiated RTCP mux, we need an RTCP transport.
  if (rtcp_packet_transport) {
    RTC_LOG(LS_INFO) << "Setting RTCP Transport for " << content_name()
                     << " on " << debug_name << " transport "
                     << rtcp_packet_transport;
    SetTransport_n(/*rtcp=*/true, rtcp_dtls_transport, rtcp_packet_transport);
  }

  RTC_LOG(LS_INFO) << "Setting RTP Transport for " << content_name() << " on "
                   << debug_name << " transport " << rtp_packet_transport;
  SetTransport_n(/*rtcp=*/false, rtp_dtls_transport, rtp_packet_transport);

  // Set DtlsTransport/PacketTransport for RTP-level transport.
  if ((rtp_dtls_transport_ || rtcp_dtls_transport_) && dtls_srtp_transport_) {
    // When setting the transport with non-null |dtls_srtp_transport_|, we are
    // using DTLS-SRTP. This could happen for bundling. If the
    // |dtls_srtp_transport| is null, we cannot tell if it doing DTLS-SRTP or
    // SDES until the description is set. So don't call |EnableDtlsSrtp_n| here.
    dtls_srtp_transport_->SetDtlsTransports(rtp_dtls_transport,
                                            rtcp_dtls_transport);
  } else {
    rtp_transport_->SetRtpPacketTransport(rtp_packet_transport);
    rtp_transport_->SetRtcpPacketTransport(rtcp_packet_transport);
  }

  // Update aggregate writable/ready-to-send state between RTP and RTCP upon
  // setting new transport channels.
  UpdateWritableState_n();
}

void BaseChannel::SetTransport_n(
    bool rtcp,
    DtlsTransportInternal* new_dtls_transport,
    rtc::PacketTransportInternal* new_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (new_dtls_transport) {
    RTC_DCHECK(new_dtls_transport == new_packet_transport);
  }
  DtlsTransportInternal*& old_dtls_transport =
      rtcp ? rtcp_dtls_transport_ : rtp_dtls_transport_;
  rtc::PacketTransportInternal* old_packet_transport =
      rtcp ? rtp_transport_->rtcp_packet_transport()
           : rtp_transport_->rtp_packet_transport();

  if (!old_packet_transport && !new_packet_transport) {
    // Nothing to do.
    return;
  }

  RTC_DCHECK(old_packet_transport != new_packet_transport);

  old_dtls_transport = new_dtls_transport;

  // If there's no new transport, we're done.
  if (!new_packet_transport) {
    return;
  }

  if (rtcp && new_dtls_transport) {
    RTC_CHECK(!(ShouldSetupDtlsSrtp_n() && srtp_active()))
        << "Setting RTCP for DTLS/SRTP after the DTLS is active "
        << "should never happen.";
  }

  auto& socket_options = rtcp ? rtcp_socket_options_ : socket_options_;
  for (const auto& pair : socket_options) {
    new_packet_transport->SetOption(pair.first, pair.second);
  }
}

bool BaseChannel::Enable(bool enable) {
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(enable ? &BaseChannel::EnableMedia_w : &BaseChannel::DisableMedia_w,
           this));
  return true;
}

bool BaseChannel::AddRecvStream(const StreamParams& sp) {
  return InvokeOnWorker<bool>(RTC_FROM_HERE,
                              Bind(&BaseChannel::AddRecvStream_w, this, sp));
}

bool BaseChannel::RemoveRecvStream(uint32_t ssrc) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&BaseChannel::RemoveRecvStream_w, this, ssrc));
}

bool BaseChannel::AddSendStream(const StreamParams& sp) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&MediaChannel::AddSendStream, media_channel(), sp));
}

bool BaseChannel::RemoveSendStream(uint32_t ssrc) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&MediaChannel::RemoveSendStream, media_channel(), ssrc));
}

bool BaseChannel::SetLocalContent(const MediaContentDescription* content,
                                  SdpType type,
                                  std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetLocalContent");
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&BaseChannel::SetLocalContent_w, this, content, type, error_desc));
}

bool BaseChannel::SetRemoteContent(const MediaContentDescription* content,
                                   SdpType type,
                                   std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetRemoteContent");
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&BaseChannel::SetRemoteContent_w, this, content, type, error_desc));
}

void BaseChannel::StartConnectionMonitor(int cms) {
  // We pass in the BaseChannel instead of the rtp_dtls_transport_
  // because if the rtp_dtls_transport_ changes, the ConnectionMonitor
  // would be pointing to the wrong TransportChannel.
  // We pass in the network thread because on that thread connection monitor
  // will call BaseChannel::GetConnectionStats which must be called on the
  // network thread.
  connection_monitor_.reset(
      new ConnectionMonitor(this, network_thread(), rtc::Thread::Current()));
  connection_monitor_->SignalUpdate.connect(
      this, &BaseChannel::OnConnectionMonitorUpdate);
  connection_monitor_->Start(cms);
}

void BaseChannel::StopConnectionMonitor() {
  if (connection_monitor_) {
    connection_monitor_->Stop();
    connection_monitor_.reset();
  }
}

bool BaseChannel::GetConnectionStats(ConnectionInfos* infos) {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (!rtp_dtls_transport_) {
    return false;
  }
  return rtp_dtls_transport_->ice_transport()->GetStats(infos);
}

bool BaseChannel::NeedsRtcpTransport() {
  // If this BaseChannel doesn't require RTCP mux and we haven't fully
  // negotiated RTCP mux, we need an RTCP transport.
  return !rtcp_mux_required_ && !rtcp_mux_filter_.IsFullyActive();
}

bool BaseChannel::IsReadyToReceiveMedia_w() const {
  // Receive data if we are enabled and have local content,
  return enabled() &&
         webrtc::RtpTransceiverDirectionHasRecv(local_content_direction_);
}

bool BaseChannel::IsReadyToSendMedia_w() const {
  // Need to access some state updated on the network thread.
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&BaseChannel::IsReadyToSendMedia_n, this));
}

bool BaseChannel::IsReadyToSendMedia_n() const {
  // Send outgoing data if we are enabled, have local and remote content,
  // and we have had some form of connectivity.
  return enabled() &&
         webrtc::RtpTransceiverDirectionHasRecv(remote_content_direction_) &&
         webrtc::RtpTransceiverDirectionHasSend(local_content_direction_) &&
         was_ever_writable() && (srtp_active() || !ShouldSetupDtlsSrtp_n());
}

bool BaseChannel::SendPacket(rtc::CopyOnWriteBuffer* packet,
                             const rtc::PacketOptions& options) {
  return SendPacket(false, packet, options);
}

bool BaseChannel::SendRtcp(rtc::CopyOnWriteBuffer* packet,
                           const rtc::PacketOptions& options) {
  return SendPacket(true, packet, options);
}

int BaseChannel::SetOption(SocketType type, rtc::Socket::Option opt,
                           int value) {
  return network_thread_->Invoke<int>(
      RTC_FROM_HERE, Bind(&BaseChannel::SetOption_n, this, type, opt, value));
}

int BaseChannel::SetOption_n(SocketType type,
                             rtc::Socket::Option opt,
                             int value) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::PacketTransportInternal* transport = nullptr;
  switch (type) {
    case ST_RTP:
      transport = rtp_transport_->rtp_packet_transport();
      socket_options_.push_back(
          std::pair<rtc::Socket::Option, int>(opt, value));
      break;
    case ST_RTCP:
      transport = rtp_transport_->rtcp_packet_transport();
      rtcp_socket_options_.push_back(
          std::pair<rtc::Socket::Option, int>(opt, value));
      break;
  }
  return transport ? transport->SetOption(opt, value) : -1;
}

void BaseChannel::OnWritableState(bool writable) {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (writable) {
    // This is used to cover the scenario when the DTLS handshake is completed
    // and DtlsTransport becomes writable before the remote description is set.
    if (ShouldSetupDtlsSrtp_n()) {
      EnableDtlsSrtp_n();
    }
    ChannelWritable_n();
  } else {
    ChannelNotWritable_n();
  }
}

void BaseChannel::OnNetworkRouteChanged(
    rtc::Optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::NetworkRoute new_route;
  if (network_route) {
    new_route = *(network_route);
  }
  // Note: When the RTCP-muxing is not enabled, RTCP transport and RTP transport
  // use the same transport name and MediaChannel::OnNetworkRouteChanged cannot
  // work correctly. Intentionally leave it broken to simplify the code and
  // encourage the users to stop using non-muxing RTCP.
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, worker_thread_, [=] {
    media_channel_->OnNetworkRouteChanged(transport_name_, new_route);
  });
}

void BaseChannel::OnTransportReadyToSend(bool ready) {
  invoker_.AsyncInvoke<void>(RTC_FROM_HERE, worker_thread_,
                             [=] { media_channel_->OnReadyToSend(ready); });
}

bool BaseChannel::SendPacket(bool rtcp,
                             rtc::CopyOnWriteBuffer* packet,
                             const rtc::PacketOptions& options) {
  // SendPacket gets called from MediaEngine, on a pacer or an encoder thread.
  // If the thread is not our network thread, we will post to our network
  // so that the real work happens on our network. This avoids us having to
  // synchronize access to all the pieces of the send path, including
  // SRTP and the inner workings of the transport channels.
  // The only downside is that we can't return a proper failure code if
  // needed. Since UDP is unreliable anyway, this should be a non-issue.
  if (!network_thread_->IsCurrent()) {
    // Avoid a copy by transferring the ownership of the packet data.
    int message_id = rtcp ? MSG_SEND_RTCP_PACKET : MSG_SEND_RTP_PACKET;
    SendPacketMessageData* data = new SendPacketMessageData;
    data->packet = std::move(*packet);
    data->options = options;
    network_thread_->Post(RTC_FROM_HERE, this, message_id, data);
    return true;
  }
  TRACE_EVENT0("webrtc", "BaseChannel::SendPacket");

  // Now that we are on the correct thread, ensure we have a place to send this
  // packet before doing anything. (We might get RTCP packets that we don't
  // intend to send.) If we've negotiated RTCP mux, send RTCP over the RTP
  // transport.
  if (!rtp_transport_->IsWritable(rtcp)) {
    return false;
  }

  // Protect ourselves against crazy data.
  if (!ValidPacket(rtcp, packet)) {
    RTC_LOG(LS_ERROR) << "Dropping outgoing " << content_name_ << " "
                      << RtpRtcpStringLiteral(rtcp)
                      << " packet: wrong size=" << packet->size();
    return false;
  }

  if (!srtp_active()) {
    if (srtp_required_) {
      // The audio/video engines may attempt to send RTCP packets as soon as the
      // streams are created, so don't treat this as an error for RTCP.
      // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=6809
      if (rtcp) {
        return false;
      }
      // However, there shouldn't be any RTP packets sent before SRTP is set up
      // (and SetSend(true) is called).
      RTC_LOG(LS_ERROR)
          << "Can't send outgoing RTP packet when SRTP is inactive"
          << " and crypto is required";
      RTC_NOTREACHED();
      return false;
    }

    std::string packet_type = rtcp ? "RTCP" : "RTP";
    RTC_LOG(LS_WARNING) << "Sending an " << packet_type
                        << " packet without encryption.";
  } else {
    // Make sure we didn't accidentally send any packets without encryption.
    RTC_DCHECK(rtp_transport_ == sdes_transport_.get() ||
               rtp_transport_ == dtls_srtp_transport_.get());
  }
  // Bon voyage.
  return rtcp ? rtp_transport_->SendRtcpPacket(packet, options, PF_SRTP_BYPASS)
              : rtp_transport_->SendRtpPacket(packet, options, PF_SRTP_BYPASS);
}

bool BaseChannel::HandlesPayloadType(int packet_type) const {
  return rtp_transport_->HandlesPayloadType(packet_type);
}

void BaseChannel::OnPacketReceived(bool rtcp,
                                   rtc::CopyOnWriteBuffer* packet,
                                   const rtc::PacketTime& packet_time) {
  if (!has_received_packet_ && !rtcp) {
    has_received_packet_ = true;
    signaling_thread()->Post(RTC_FROM_HERE, this, MSG_FIRSTPACKETRECEIVED);
  }

  if (!srtp_active() && srtp_required_) {
    // Our session description indicates that SRTP is required, but we got a
    // packet before our SRTP filter is active. This means either that
    // a) we got SRTP packets before we received the SDES keys, in which case
    //    we can't decrypt it anyway, or
    // b) we got SRTP packets before DTLS completed on both the RTP and RTCP
    //    transports, so we haven't yet extracted keys, even if DTLS did
    //    complete on the transport that the packets are being sent on. It's
    //    really good practice to wait for both RTP and RTCP to be good to go
    //    before sending  media, to prevent weird failure modes, so it's fine
    //    for us to just eat packets here. This is all sidestepped if RTCP mux
    //    is used anyway.
    RTC_LOG(LS_WARNING)
        << "Can't process incoming " << RtpRtcpStringLiteral(rtcp)
        << " packet when SRTP is inactive and crypto is required";
    return;
  }

  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      Bind(&BaseChannel::ProcessPacket, this, rtcp, *packet, packet_time));
}

void BaseChannel::ProcessPacket(bool rtcp,
                                const rtc::CopyOnWriteBuffer& packet,
                                const rtc::PacketTime& packet_time) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  // Need to copy variable because OnRtcpReceived/OnPacketReceived
  // requires non-const pointer to buffer. This doesn't memcpy the actual data.
  rtc::CopyOnWriteBuffer data(packet);
  if (rtcp) {
    media_channel_->OnRtcpReceived(&data, packet_time);
  } else {
    media_channel_->OnPacketReceived(&data, packet_time);
  }
}

void BaseChannel::EnableMedia_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  if (enabled_)
    return;

  RTC_LOG(LS_INFO) << "Channel enabled";
  enabled_ = true;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::DisableMedia_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  if (!enabled_)
    return;

  RTC_LOG(LS_INFO) << "Channel disabled";
  enabled_ = false;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::UpdateWritableState_n() {
  rtc::PacketTransportInternal* rtp_packet_transport =
      rtp_transport_->rtp_packet_transport();
  rtc::PacketTransportInternal* rtcp_packet_transport =
      rtp_transport_->rtcp_packet_transport();
  if (rtp_packet_transport && rtp_packet_transport->writable() &&
      (!rtcp_packet_transport || rtcp_packet_transport->writable())) {
    ChannelWritable_n();
  } else {
    ChannelNotWritable_n();
  }
}

void BaseChannel::ChannelWritable_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (writable_) {
    return;
  }

  RTC_LOG(LS_INFO) << "Channel writable (" << content_name_ << ")"
                   << (was_ever_writable_ ? "" : " for the first time");

  was_ever_writable_ = true;
  writable_ = true;
  UpdateMediaSendRecvState();
}

bool BaseChannel::ShouldSetupDtlsSrtp_n() const {
  // Since DTLS is applied to all transports, checking RTP should be enough.
  return rtp_dtls_transport_ && rtp_dtls_transport_->IsDtlsActive();
}

void BaseChannel::ChannelNotWritable_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (!writable_)
    return;

  RTC_LOG(LS_INFO) << "Channel not writable (" << content_name_ << ")";
  writable_ = false;
  UpdateMediaSendRecvState();
}

bool BaseChannel::SetRtpTransportParameters(
    const MediaContentDescription* content,
    SdpType type,
    ContentSource src,
    const RtpHeaderExtensions& extensions,
    std::string* error_desc) {
  std::vector<int> encrypted_extension_ids;
  for (const webrtc::RtpExtension& extension : extensions) {
    if (extension.encrypt) {
      RTC_LOG(LS_INFO) << "Using " << (src == CS_LOCAL ? "local" : "remote")
                       << " encrypted extension: " << extension.ToString();
      encrypted_extension_ids.push_back(extension.id);
    }
  }

  // Cache srtp_required_ for belt and suspenders check on SendPacket
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      Bind(&BaseChannel::SetRtpTransportParameters_n, this, content, type, src,
           encrypted_extension_ids, error_desc));
}

bool BaseChannel::SetRtpTransportParameters_n(
    const MediaContentDescription* content,
    SdpType type,
    ContentSource src,
    const std::vector<int>& encrypted_extension_ids,
    std::string* error_desc) {
  RTC_DCHECK(network_thread_->IsCurrent());

  if (!SetSrtp_n(content->cryptos(), type, src, encrypted_extension_ids,
                 error_desc)) {
    return false;
  }

  if (!SetRtcpMux_n(content->rtcp_mux(), type, src, error_desc)) {
    return false;
  }

  return true;
}

// |dtls| will be set to true if DTLS is active for transport and crypto is
// empty.
bool BaseChannel::CheckSrtpConfig_n(const std::vector<CryptoParams>& cryptos,
                                    bool* dtls,
                                    std::string* error_desc) {
  *dtls = rtp_dtls_transport_ && rtp_dtls_transport_->IsDtlsActive();
  if (*dtls && !cryptos.empty()) {
    SafeSetError("Cryptos must be empty when DTLS is active.", error_desc);
    return false;
  }
  return true;
}

void BaseChannel::EnableSdes_n() {
  if (sdes_transport_) {
    return;
  }
  // DtlsSrtpTransport and SrtpTransport shouldn't be enabled at the same
  // time.
  RTC_DCHECK(!dtls_srtp_transport_);
  RTC_DCHECK(unencrypted_rtp_transport_);
  sdes_transport_ = rtc::MakeUnique<webrtc::SrtpTransport>(
      std::move(unencrypted_rtp_transport_));
#if defined(ENABLE_EXTERNAL_AUTH)
  sdes_transport_->EnableExternalAuth();
#endif
  SetRtpTransport(sdes_transport_.get());
  RTC_LOG(LS_INFO) << "Wrapping RtpTransport in SrtpTransport.";
}

void BaseChannel::EnableDtlsSrtp_n() {
  if (dtls_srtp_transport_) {
    return;
  }
  // DtlsSrtpTransport and SrtpTransport shouldn't be enabled at the same
  // time.
  RTC_DCHECK(!sdes_transport_);
  RTC_DCHECK(unencrypted_rtp_transport_);

  auto srtp_transport = rtc::MakeUnique<webrtc::SrtpTransport>(
      std::move(unencrypted_rtp_transport_));
#if defined(ENABLE_EXTERNAL_AUTH)
  srtp_transport->EnableExternalAuth();
#endif
  dtls_srtp_transport_ =
      rtc::MakeUnique<webrtc::DtlsSrtpTransport>(std::move(srtp_transport));

  SetRtpTransport(dtls_srtp_transport_.get());
  if (cached_send_extension_ids_) {
    dtls_srtp_transport_->UpdateSendEncryptedHeaderExtensionIds(
        *cached_send_extension_ids_);
  }
  if (cached_recv_extension_ids_) {
    dtls_srtp_transport_->UpdateRecvEncryptedHeaderExtensionIds(
        *cached_recv_extension_ids_);
  }
  // Set the DtlsTransport and the |dtls_srtp_transport_| will handle the DTLS
  // relate signal internally.
  RTC_DCHECK(rtp_dtls_transport_);
  dtls_srtp_transport_->SetDtlsTransports(rtp_dtls_transport_,
                                          rtcp_dtls_transport_);

  RTC_LOG(LS_INFO) << "Wrapping SrtpTransport in DtlsSrtpTransport.";
}

bool BaseChannel::SetSrtp_n(const std::vector<CryptoParams>& cryptos,
                            SdpType type,
                            ContentSource src,
                            const std::vector<int>& encrypted_extension_ids,
                            std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetSrtp_w");
  bool ret = false;
  bool dtls = false;
  ret = CheckSrtpConfig_n(cryptos, &dtls, error_desc);
  if (!ret) {
    return false;
  }

  // If SRTP was not required, but we're setting a description that uses SDES,
  // we need to upgrade to an SrtpTransport.
  if (!sdes_transport_ && !dtls && !cryptos.empty()) {
    EnableSdes_n();
  }

  if ((type == SdpType::kAnswer || type == SdpType::kPrAnswer) && dtls) {
    EnableDtlsSrtp_n();
  }

  UpdateEncryptedHeaderExtensionIds(src, encrypted_extension_ids);

  if (!dtls) {
    switch (type) {
      case SdpType::kOffer:
        ret = sdes_negotiator_.SetOffer(cryptos, src);
        break;
      case SdpType::kPrAnswer:
        ret = sdes_negotiator_.SetProvisionalAnswer(cryptos, src);
        break;
      case SdpType::kAnswer:
        ret = sdes_negotiator_.SetAnswer(cryptos, src);
        break;
      default:
        break;
    }

    // If setting an SDES answer succeeded, apply the negotiated parameters
    // to the SRTP transport.
    if ((type == SdpType::kPrAnswer || type == SdpType::kAnswer) && ret) {
      if (sdes_negotiator_.send_cipher_suite() &&
          sdes_negotiator_.recv_cipher_suite()) {
        RTC_DCHECK(cached_send_extension_ids_);
        RTC_DCHECK(cached_recv_extension_ids_);
        ret = sdes_transport_->SetRtpParams(
            *(sdes_negotiator_.send_cipher_suite()),
            sdes_negotiator_.send_key().data(),
            static_cast<int>(sdes_negotiator_.send_key().size()),
            *(cached_send_extension_ids_),
            *(sdes_negotiator_.recv_cipher_suite()),
            sdes_negotiator_.recv_key().data(),
            static_cast<int>(sdes_negotiator_.recv_key().size()),
            *(cached_recv_extension_ids_));
      } else {
        RTC_LOG(LS_INFO) << "No crypto keys are provided for SDES.";
        if (type == SdpType::kAnswer && sdes_transport_) {
          // Explicitly reset the |sdes_transport_| if no crypto param is
          // provided in the answer. No need to call |ResetParams()| for
          // |sdes_negotiator_| because it resets the params inside |SetAnswer|.
          sdes_transport_->ResetParams();
        }
      }
    }
  }

  if (!ret) {
    SafeSetError("Failed to setup SRTP.", error_desc);
    return false;
  }
  return true;
}

bool BaseChannel::SetRtcpMux_n(bool enable,
                               SdpType type,
                               ContentSource src,
                               std::string* error_desc) {
  // Provide a more specific error message for the RTCP mux "require" policy
  // case.
  if (rtcp_mux_required_ && !enable) {
    SafeSetError(
        "rtcpMuxPolicy is 'require', but media description does not "
        "contain 'a=rtcp-mux'.",
        error_desc);
    return false;
  }
  bool ret = false;
  switch (type) {
    case SdpType::kOffer:
      ret = rtcp_mux_filter_.SetOffer(enable, src);
      break;
    case SdpType::kPrAnswer:
      // This may activate RTCP muxing, but we don't yet destroy the transport
      // because the final answer may deactivate it.
      ret = rtcp_mux_filter_.SetProvisionalAnswer(enable, src);
      break;
    case SdpType::kAnswer:
      ret = rtcp_mux_filter_.SetAnswer(enable, src);
      if (ret && rtcp_mux_filter_.IsActive()) {
        ActivateRtcpMux();
      }
      break;
    default:
      break;
  }
  if (!ret) {
    SafeSetError("Failed to setup RTCP mux filter.", error_desc);
    return false;
  }
  rtp_transport_->SetRtcpMuxEnabled(rtcp_mux_filter_.IsActive());
  // |rtcp_mux_filter_| can be active if |action| is SdpType::kPrAnswer or
  // SdpType::kAnswer, but we only want to tear down the RTCP transport if we
  // received a final answer.
  if (rtcp_mux_filter_.IsActive()) {
    // If the RTP transport is already writable, then so are we.
    if (rtp_transport_->rtp_packet_transport()->writable()) {
      ChannelWritable_n();
    }
  }

  return true;
}

bool BaseChannel::AddRecvStream_w(const StreamParams& sp) {
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  return media_channel()->AddRecvStream(sp);
}

bool BaseChannel::RemoveRecvStream_w(uint32_t ssrc) {
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  return media_channel()->RemoveRecvStream(ssrc);
}

bool BaseChannel::UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                                       SdpType type,
                                       std::string* error_desc) {
  // Check for streams that have been removed.
  bool ret = true;
  for (StreamParamsVec::const_iterator it = local_streams_.begin();
       it != local_streams_.end(); ++it) {
    if (!GetStreamBySsrc(streams, it->first_ssrc())) {
      if (!media_channel()->RemoveSendStream(it->first_ssrc())) {
        std::ostringstream desc;
        desc << "Failed to remove send stream with ssrc "
             << it->first_ssrc() << ".";
        SafeSetError(desc.str(), error_desc);
        ret = false;
      }
    }
  }
  // Check for new streams.
  for (StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    if (!GetStreamBySsrc(local_streams_, it->first_ssrc())) {
      if (media_channel()->AddSendStream(*it)) {
        RTC_LOG(LS_INFO) << "Add send stream ssrc: " << it->ssrcs[0];
      } else {
        std::ostringstream desc;
        desc << "Failed to add send stream ssrc: " << it->first_ssrc();
        SafeSetError(desc.str(), error_desc);
        ret = false;
      }
    }
  }
  local_streams_ = streams;
  return ret;
}

bool BaseChannel::UpdateRemoteStreams_w(
    const std::vector<StreamParams>& streams,
    SdpType type,
    std::string* error_desc) {
  // Check for streams that have been removed.
  bool ret = true;
  for (StreamParamsVec::const_iterator it = remote_streams_.begin();
       it != remote_streams_.end(); ++it) {
    if (!GetStreamBySsrc(streams, it->first_ssrc())) {
      if (!RemoveRecvStream_w(it->first_ssrc())) {
        std::ostringstream desc;
        desc << "Failed to remove remote stream with ssrc "
             << it->first_ssrc() << ".";
        SafeSetError(desc.str(), error_desc);
        ret = false;
      }
    }
  }
  // Check for new streams.
  for (StreamParamsVec::const_iterator it = streams.begin();
      it != streams.end(); ++it) {
    if (!GetStreamBySsrc(remote_streams_, it->first_ssrc())) {
      if (AddRecvStream_w(*it)) {
        RTC_LOG(LS_INFO) << "Add remote ssrc: " << it->ssrcs[0];
      } else {
        std::ostringstream desc;
        desc << "Failed to add remote stream ssrc: " << it->first_ssrc();
        SafeSetError(desc.str(), error_desc);
        ret = false;
      }
    }
  }
  remote_streams_ = streams;
  return ret;
}

RtpHeaderExtensions BaseChannel::GetFilteredRtpHeaderExtensions(
    const RtpHeaderExtensions& extensions) {
  if (!rtp_dtls_transport_ ||
      !rtp_dtls_transport_->crypto_options()
          .enable_encrypted_rtp_header_extensions) {
    RtpHeaderExtensions filtered;
    auto pred = [](const webrtc::RtpExtension& extension) {
        return !extension.encrypt;
    };
    std::copy_if(extensions.begin(), extensions.end(),
        std::back_inserter(filtered), pred);
    return filtered;
  }

  return webrtc::RtpExtension::FilterDuplicateNonEncrypted(extensions);
}

void BaseChannel::MaybeCacheRtpAbsSendTimeHeaderExtension_w(
    const std::vector<webrtc::RtpExtension>& extensions) {
// Absolute Send Time extension id is used only with external auth,
// so do not bother searching for it and making asyncronious call to set
// something that is not used.
#if defined(ENABLE_EXTERNAL_AUTH)
  const webrtc::RtpExtension* send_time_extension =
      webrtc::RtpExtension::FindHeaderExtensionByUri(
          extensions, webrtc::RtpExtension::kAbsSendTimeUri);
  int rtp_abs_sendtime_extn_id =
      send_time_extension ? send_time_extension->id : -1;
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, network_thread_,
      Bind(&BaseChannel::CacheRtpAbsSendTimeHeaderExtension_n, this,
           rtp_abs_sendtime_extn_id));
#endif
}

void BaseChannel::CacheRtpAbsSendTimeHeaderExtension_n(
    int rtp_abs_sendtime_extn_id) {
  if (sdes_transport_) {
    sdes_transport_->CacheRtpAbsSendTimeHeaderExtension(
        rtp_abs_sendtime_extn_id);
  } else if (dtls_srtp_transport_) {
    dtls_srtp_transport_->CacheRtpAbsSendTimeHeaderExtension(
        rtp_abs_sendtime_extn_id);
  } else {
    RTC_LOG(LS_WARNING)
        << "Trying to cache the Absolute Send Time extension id "
           "but the SRTP is not active.";
  }
}

void BaseChannel::OnMessage(rtc::Message *pmsg) {
  TRACE_EVENT0("webrtc", "BaseChannel::OnMessage");
  switch (pmsg->message_id) {
    case MSG_SEND_RTP_PACKET:
    case MSG_SEND_RTCP_PACKET: {
      RTC_DCHECK(network_thread_->IsCurrent());
      SendPacketMessageData* data =
          static_cast<SendPacketMessageData*>(pmsg->pdata);
      bool rtcp = pmsg->message_id == MSG_SEND_RTCP_PACKET;
      SendPacket(rtcp, &data->packet, data->options);
      delete data;
      break;
    }
    case MSG_FIRSTPACKETRECEIVED: {
      SignalFirstPacketReceived(this);
      break;
    }
  }
}

void BaseChannel::AddHandledPayloadType(int payload_type) {
  rtp_transport_->AddHandledPayloadType(payload_type);
}

void BaseChannel::FlushRtcpMessages_n() {
  // Flush all remaining RTCP messages. This should only be called in
  // destructor.
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::MessageList rtcp_messages;
  network_thread_->Clear(this, MSG_SEND_RTCP_PACKET, &rtcp_messages);
  for (const auto& message : rtcp_messages) {
    network_thread_->Send(RTC_FROM_HERE, this, MSG_SEND_RTCP_PACKET,
                          message.pdata);
  }
}

void BaseChannel::SignalSentPacket_n(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(network_thread_->IsCurrent());
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      rtc::Bind(&BaseChannel::SignalSentPacket_w, this, sent_packet));
}

void BaseChannel::SignalSentPacket_w(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  SignalSentPacket(sent_packet);
}

void BaseChannel::UpdateEncryptedHeaderExtensionIds(
    cricket::ContentSource source,
    const std::vector<int>& extension_ids) {
  if (source == ContentSource::CS_LOCAL) {
    cached_recv_extension_ids_ = std::move(extension_ids);
    if (dtls_srtp_transport_) {
      dtls_srtp_transport_->UpdateRecvEncryptedHeaderExtensionIds(
          extension_ids);
    }
  } else {
    cached_send_extension_ids_ = std::move(extension_ids);
    if (dtls_srtp_transport_) {
      dtls_srtp_transport_->UpdateSendEncryptedHeaderExtensionIds(
          extension_ids);
    }
  }
}

void BaseChannel::ActivateRtcpMux() {
  // We permanently activated RTCP muxing; signal that we no longer need
  // the RTCP transport.
  std::string debug_name =
      transport_name_.empty()
          ? rtp_transport_->rtp_packet_transport()->transport_name()
          : transport_name_;
  RTC_LOG(LS_INFO) << "Enabling rtcp-mux for " << content_name()
                   << "; no longer need RTCP transport for " << debug_name;
  if (rtp_transport_->rtcp_packet_transport()) {
    SetTransport_n(/*rtcp=*/true, nullptr, nullptr);
    if (dtls_srtp_transport_) {
      RTC_DCHECK(rtp_dtls_transport_);
      dtls_srtp_transport_->SetDtlsTransports(rtp_dtls_transport_,
                                              /*rtcp_dtls_transport_=*/nullptr);
    } else {
      rtp_transport_->SetRtcpPacketTransport(nullptr);
    }
    SignalRtcpMuxFullyActive(transport_name_);
  }
  UpdateWritableState_n();
}

VoiceChannel::VoiceChannel(rtc::Thread* worker_thread,
                           rtc::Thread* network_thread,
                           rtc::Thread* signaling_thread,
                           MediaEngineInterface* media_engine,
                           std::unique_ptr<VoiceMediaChannel> media_channel,
                           const std::string& content_name,
                           bool rtcp_mux_required,
                           bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  std::move(media_channel),
                  content_name,
                  rtcp_mux_required,
                  srtp_required),
      media_engine_(media_engine) {}

VoiceChannel::~VoiceChannel() {
  TRACE_EVENT0("webrtc", "VoiceChannel::~VoiceChannel");
  StopAudioMonitor();
  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
  Deinit();
}

bool VoiceChannel::SetAudioSend(uint32_t ssrc,
                                bool enable,
                                const AudioOptions* options,
                                AudioSource* source) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&VoiceMediaChannel::SetAudioSend, media_channel(),
                          ssrc, enable, options, source));
}

// TODO(juberti): Handle early media the right way. We should get an explicit
// ringing message telling us to start playing local ringback, which we cancel
// if any early media actually arrives. For now, we do the opposite, which is
// to wait 1 second for early media, and start playing local ringback if none
// arrives.
void VoiceChannel::SetEarlyMedia(bool enable) {
  if (enable) {
    // Start the early media timeout
    worker_thread()->PostDelayed(RTC_FROM_HERE, kEarlyMediaTimeout, this,
                                 MSG_EARLYMEDIATIMEOUT);
  } else {
    // Stop the timeout if currently going.
    worker_thread()->Clear(this, MSG_EARLYMEDIATIMEOUT);
  }
}

bool VoiceChannel::GetStats(VoiceMediaInfo* stats) {
  return InvokeOnWorker<bool>(RTC_FROM_HERE, Bind(&VoiceMediaChannel::GetStats,
                                                  media_channel(), stats));
}

void VoiceChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new VoiceMediaMonitor(media_channel(), worker_thread(),
      rtc::Thread::Current()));
  media_monitor_->SignalUpdate.connect(
      this, &VoiceChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void VoiceChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_->SignalUpdate.disconnect(this);
    media_monitor_.reset();
  }
}

void VoiceChannel::StartAudioMonitor(int cms) {
  audio_monitor_.reset(new AudioMonitor(this, rtc::Thread::Current()));
  audio_monitor_
    ->SignalUpdate.connect(this, &VoiceChannel::OnAudioMonitorUpdate);
  audio_monitor_->Start(cms);
}

void VoiceChannel::StopAudioMonitor() {
  if (audio_monitor_) {
    audio_monitor_->Stop();
    audio_monitor_.reset();
  }
}

bool VoiceChannel::IsAudioMonitorRunning() const {
  return (audio_monitor_.get() != NULL);
}

int VoiceChannel::GetInputLevel_w() {
  return media_engine_->GetInputLevel();
}

int VoiceChannel::GetOutputLevel_w() {
  return media_channel()->GetOutputLevel();
}

void VoiceChannel::GetActiveStreams_w(AudioInfo::StreamList* actives) {
  media_channel()->GetActiveStreams(actives);
}

void VoiceChannel::OnPacketReceived(bool rtcp,
                                    rtc::CopyOnWriteBuffer* packet,
                                    const rtc::PacketTime& packet_time) {
  BaseChannel::OnPacketReceived(rtcp, packet, packet_time);
  // Set a flag when we've received an RTP packet. If we're waiting for early
  // media, this will disable the timeout.
  if (!received_media_ && !rtcp) {
    received_media_ = true;
  }
}

void BaseChannel::UpdateMediaSendRecvState() {
  RTC_DCHECK(network_thread_->IsCurrent());
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      Bind(&BaseChannel::UpdateMediaSendRecvState_w, this));
}

void VoiceChannel::UpdateMediaSendRecvState_w() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool recv = IsReadyToReceiveMedia_w();
  media_channel()->SetPlayout(recv);

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  media_channel()->SetSend(send);

  RTC_LOG(LS_INFO) << "Changing voice state, recv=" << recv << " send=" << send;
}

bool VoiceChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     SdpType type,
                                     std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetLocalContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting local voice description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find audio content in local description.", error_desc);
    return false;
  }

  const AudioContentDescription* audio = content->as_audio();

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(audio->rtp_header_extensions());

  if (!SetRtpTransportParameters(content, type, CS_LOCAL, rtp_header_extensions,
                                 error_desc)) {
    return false;
  }

  AudioRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(audio, rtp_header_extensions, &recv_params);
  if (!media_channel()->SetRecvParameters(recv_params)) {
    SafeSetError("Failed to set local audio description recv parameters.",
                 error_desc);
    return false;
  }
  for (const AudioCodec& codec : audio->codecs()) {
    AddHandledPayloadType(codec.id);
  }
  last_recv_params_ = recv_params;

  // TODO(pthatcher): Move local streams into AudioSendParameters, and
  // only give it to the media channel once we have a remote
  // description too (without a remote description, we won't be able
  // to send them anyway).
  if (!UpdateLocalStreams_w(audio->streams(), type, error_desc)) {
    SafeSetError("Failed to set local audio description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool VoiceChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      SdpType type,
                                      std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetRemoteContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting remote voice description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find audio content in remote description.", error_desc);
    return false;
  }

  const AudioContentDescription* audio = content->as_audio();

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(audio->rtp_header_extensions());

  if (!SetRtpTransportParameters(content, type, CS_REMOTE,
                                 rtp_header_extensions, error_desc)) {
    return false;
  }

  AudioSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(audio, rtp_header_extensions,
      &send_params);

  bool parameters_applied = media_channel()->SetSendParameters(send_params);
  if (!parameters_applied) {
    SafeSetError("Failed to set remote audio description send parameters.",
                 error_desc);
    return false;
  }
  last_send_params_ = send_params;

  // TODO(pthatcher): Move remote streams into AudioRecvParameters,
  // and only give it to the media channel once we have a local
  // description too (without a local description, we won't be able to
  // recv them anyway).
  if (!UpdateRemoteStreams_w(audio->streams(), type, error_desc)) {
    SafeSetError("Failed to set remote audio description streams.", error_desc);
    return false;
  }

  if (audio->rtp_header_extensions_set()) {
    MaybeCacheRtpAbsSendTimeHeaderExtension_w(rtp_header_extensions);
  }

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

void VoiceChannel::HandleEarlyMediaTimeout() {
  // This occurs on the main thread, not the worker thread.
  if (!received_media_) {
    RTC_LOG(LS_INFO) << "No early media received before timeout";
    SignalEarlyMediaTimeout(this);
  }
}

void VoiceChannel::OnMessage(rtc::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_EARLYMEDIATIMEOUT:
      HandleEarlyMediaTimeout();
      break;
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
}

void VoiceChannel::OnConnectionMonitorUpdate(
    ConnectionMonitor* monitor, const std::vector<ConnectionInfo>& infos) {
  SignalConnectionMonitor(this, infos);
}

void VoiceChannel::OnMediaMonitorUpdate(
    VoiceMediaChannel* media_channel, const VoiceMediaInfo& info) {
  RTC_DCHECK(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

void VoiceChannel::OnAudioMonitorUpdate(AudioMonitor* monitor,
                                        const AudioInfo& info) {
  SignalAudioMonitor(this, info);
}

VideoChannel::VideoChannel(rtc::Thread* worker_thread,
                           rtc::Thread* network_thread,
                           rtc::Thread* signaling_thread,
                           std::unique_ptr<VideoMediaChannel> media_channel,
                           const std::string& content_name,
                           bool rtcp_mux_required,
                           bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  std::move(media_channel),
                  content_name,
                  rtcp_mux_required,
                  srtp_required) {}

VideoChannel::~VideoChannel() {
  TRACE_EVENT0("webrtc", "VideoChannel::~VideoChannel");
  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();

  Deinit();
}

void VideoChannel::UpdateMediaSendRecvState_w() {
  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  if (!media_channel()->SetSend(send)) {
    RTC_LOG(LS_ERROR) << "Failed to SetSend on video channel";
    // TODO(gangji): Report error back to server.
  }

  RTC_LOG(LS_INFO) << "Changing video state, send=" << send;
}

void VideoChannel::FillBitrateInfo(BandwidthEstimationInfo* bwe_info) {
  InvokeOnWorker<void>(RTC_FROM_HERE, Bind(&VideoMediaChannel::FillBitrateInfo,
                                           media_channel(), bwe_info));
}

bool VideoChannel::GetStats(VideoMediaInfo* stats) {
  return InvokeOnWorker<bool>(RTC_FROM_HERE, Bind(&VideoMediaChannel::GetStats,
                                                  media_channel(), stats));
}

void VideoChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new VideoMediaMonitor(media_channel(), worker_thread(),
      rtc::Thread::Current()));
  media_monitor_->SignalUpdate.connect(
      this, &VideoChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void VideoChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_.reset();
  }
}

bool VideoChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     SdpType type,
                                     std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetLocalContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting local video description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find video content in local description.", error_desc);
    return false;
  }

  const VideoContentDescription* video = content->as_video();

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(video->rtp_header_extensions());

  if (!SetRtpTransportParameters(content, type, CS_LOCAL, rtp_header_extensions,
                                 error_desc)) {
    return false;
  }

  VideoRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(video, rtp_header_extensions, &recv_params);
  if (!media_channel()->SetRecvParameters(recv_params)) {
    SafeSetError("Failed to set local video description recv parameters.",
                 error_desc);
    return false;
  }
  for (const VideoCodec& codec : video->codecs()) {
    AddHandledPayloadType(codec.id);
  }
  last_recv_params_ = recv_params;

  // TODO(pthatcher): Move local streams into VideoSendParameters, and
  // only give it to the media channel once we have a remote
  // description too (without a remote description, we won't be able
  // to send them anyway).
  if (!UpdateLocalStreams_w(video->streams(), type, error_desc)) {
    SafeSetError("Failed to set local video description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool VideoChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      SdpType type,
                                      std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetRemoteContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting remote video description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find video content in remote description.", error_desc);
    return false;
  }

  const VideoContentDescription* video = content->as_video();

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(video->rtp_header_extensions());

  if (!SetRtpTransportParameters(content, type, CS_REMOTE,
                                 rtp_header_extensions, error_desc)) {
    return false;
  }

  VideoSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(video, rtp_header_extensions,
      &send_params);
  if (video->conference_mode()) {
    send_params.conference_mode = true;
  }

  bool parameters_applied = media_channel()->SetSendParameters(send_params);

  if (!parameters_applied) {
    SafeSetError("Failed to set remote video description send parameters.",
                 error_desc);
    return false;
  }
  last_send_params_ = send_params;

  // TODO(pthatcher): Move remote streams into VideoRecvParameters,
  // and only give it to the media channel once we have a local
  // description too (without a local description, we won't be able to
  // recv them anyway).
  if (!UpdateRemoteStreams_w(video->streams(), type, error_desc)) {
    SafeSetError("Failed to set remote video description streams.", error_desc);
    return false;
  }

  if (video->rtp_header_extensions_set()) {
    MaybeCacheRtpAbsSendTimeHeaderExtension_w(rtp_header_extensions);
  }

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

void VideoChannel::OnConnectionMonitorUpdate(
    ConnectionMonitor* monitor, const std::vector<ConnectionInfo> &infos) {
  SignalConnectionMonitor(this, infos);
}

// TODO(pthatcher): Look into removing duplicate code between
// audio, video, and data, perhaps by using templates.
void VideoChannel::OnMediaMonitorUpdate(
    VideoMediaChannel* media_channel, const VideoMediaInfo &info) {
  RTC_DCHECK(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

RtpDataChannel::RtpDataChannel(rtc::Thread* worker_thread,
                               rtc::Thread* network_thread,
                               rtc::Thread* signaling_thread,
                               std::unique_ptr<DataMediaChannel> media_channel,
                               const std::string& content_name,
                               bool rtcp_mux_required,
                               bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  std::move(media_channel),
                  content_name,
                  rtcp_mux_required,
                  srtp_required) {}

RtpDataChannel::~RtpDataChannel() {
  TRACE_EVENT0("webrtc", "RtpDataChannel::~RtpDataChannel");
  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();

  Deinit();
}

void RtpDataChannel::Init_w(
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  BaseChannel::Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                      rtp_packet_transport, rtcp_packet_transport);

  media_channel()->SignalDataReceived.connect(this,
                                              &RtpDataChannel::OnDataReceived);
  media_channel()->SignalReadyToSend.connect(
      this, &RtpDataChannel::OnDataChannelReadyToSend);
}

void RtpDataChannel::Init_w(webrtc::RtpTransportInternal* rtp_transport) {
  BaseChannel::Init_w(rtp_transport);
  media_channel()->SignalDataReceived.connect(this,
                                              &RtpDataChannel::OnDataReceived);
  media_channel()->SignalReadyToSend.connect(
      this, &RtpDataChannel::OnDataChannelReadyToSend);
}

bool RtpDataChannel::SendData(const SendDataParams& params,
                              const rtc::CopyOnWriteBuffer& payload,
                              SendDataResult* result) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&DataMediaChannel::SendData, media_channel(), params,
                          payload, result));
}

bool RtpDataChannel::CheckDataChannelTypeFromContent(
    const DataContentDescription* content,
    std::string* error_desc) {
  bool is_sctp = ((content->protocol() == kMediaProtocolSctp) ||
                  (content->protocol() == kMediaProtocolDtlsSctp));
  // It's been set before, but doesn't match.  That's bad.
  if (is_sctp) {
    SafeSetError("Data channel type mismatch. Expected RTP, got SCTP.",
                 error_desc);
    return false;
  }
  return true;
}

bool RtpDataChannel::SetLocalContent_w(const MediaContentDescription* content,
                                       SdpType type,
                                       std::string* error_desc) {
  TRACE_EVENT0("webrtc", "RtpDataChannel::SetLocalContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting local data description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find data content in local description.", error_desc);
    return false;
  }

  const DataContentDescription* data = content->as_data();

  if (!CheckDataChannelTypeFromContent(data, error_desc)) {
    return false;
  }

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(data->rtp_header_extensions());

  if (!SetRtpTransportParameters(content, type, CS_LOCAL, rtp_header_extensions,
                                 error_desc)) {
    return false;
  }

  DataRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(data, rtp_header_extensions, &recv_params);
  if (!media_channel()->SetRecvParameters(recv_params)) {
    SafeSetError("Failed to set remote data description recv parameters.",
                 error_desc);
    return false;
  }
  for (const DataCodec& codec : data->codecs()) {
    AddHandledPayloadType(codec.id);
  }
  last_recv_params_ = recv_params;

  // TODO(pthatcher): Move local streams into DataSendParameters, and
  // only give it to the media channel once we have a remote
  // description too (without a remote description, we won't be able
  // to send them anyway).
  if (!UpdateLocalStreams_w(data->streams(), type, error_desc)) {
    SafeSetError("Failed to set local data description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool RtpDataChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                        SdpType type,
                                        std::string* error_desc) {
  TRACE_EVENT0("webrtc", "RtpDataChannel::SetRemoteContent_w");
  RTC_DCHECK_RUN_ON(worker_thread());
  RTC_LOG(LS_INFO) << "Setting remote data description";

  RTC_DCHECK(content);
  if (!content) {
    SafeSetError("Can't find data content in remote description.", error_desc);
    return false;
  }

  const DataContentDescription* data = content->as_data();

  // If the remote data doesn't have codecs, it must be empty, so ignore it.
  if (!data->has_codecs()) {
    return true;
  }

  if (!CheckDataChannelTypeFromContent(data, error_desc)) {
    return false;
  }

  RtpHeaderExtensions rtp_header_extensions =
      GetFilteredRtpHeaderExtensions(data->rtp_header_extensions());

  RTC_LOG(LS_INFO) << "Setting remote data description";
  if (!SetRtpTransportParameters(content, type, CS_REMOTE,
                                 rtp_header_extensions, error_desc)) {
    return false;
  }

  DataSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription<DataCodec>(data, rtp_header_extensions,
      &send_params);
  if (!media_channel()->SetSendParameters(send_params)) {
    SafeSetError("Failed to set remote data description send parameters.",
                 error_desc);
    return false;
  }
  last_send_params_ = send_params;

  // TODO(pthatcher): Move remote streams into DataRecvParameters,
  // and only give it to the media channel once we have a local
  // description too (without a local description, we won't be able to
  // recv them anyway).
  if (!UpdateRemoteStreams_w(data->streams(), type, error_desc)) {
    SafeSetError("Failed to set remote data description streams.",
                 error_desc);
    return false;
  }

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

void RtpDataChannel::UpdateMediaSendRecvState_w() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool recv = IsReadyToReceiveMedia_w();
  if (!media_channel()->SetReceive(recv)) {
    RTC_LOG(LS_ERROR) << "Failed to SetReceive on data channel";
  }

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  if (!media_channel()->SetSend(send)) {
    RTC_LOG(LS_ERROR) << "Failed to SetSend on data channel";
  }

  // Trigger SignalReadyToSendData asynchronously.
  OnDataChannelReadyToSend(send);

  RTC_LOG(LS_INFO) << "Changing data state, recv=" << recv << " send=" << send;
}

void RtpDataChannel::OnMessage(rtc::Message* pmsg) {
  switch (pmsg->message_id) {
    case MSG_READYTOSENDDATA: {
      DataChannelReadyToSendMessageData* data =
          static_cast<DataChannelReadyToSendMessageData*>(pmsg->pdata);
      ready_to_send_data_ = data->data();
      SignalReadyToSendData(ready_to_send_data_);
      delete data;
      break;
    }
    case MSG_DATARECEIVED: {
      DataReceivedMessageData* data =
          static_cast<DataReceivedMessageData*>(pmsg->pdata);
      SignalDataReceived(data->params, data->payload);
      delete data;
      break;
    }
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
}

void RtpDataChannel::OnConnectionMonitorUpdate(
    ConnectionMonitor* monitor,
    const std::vector<ConnectionInfo>& infos) {
  SignalConnectionMonitor(this, infos);
}

void RtpDataChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new DataMediaMonitor(media_channel(), worker_thread(),
      rtc::Thread::Current()));
  media_monitor_->SignalUpdate.connect(this,
                                       &RtpDataChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void RtpDataChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_->SignalUpdate.disconnect(this);
    media_monitor_.reset();
  }
}

void RtpDataChannel::OnMediaMonitorUpdate(DataMediaChannel* media_channel,
                                          const DataMediaInfo& info) {
  RTC_DCHECK(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

void RtpDataChannel::OnDataReceived(const ReceiveDataParams& params,
                                    const char* data,
                                    size_t len) {
  DataReceivedMessageData* msg = new DataReceivedMessageData(
      params, data, len);
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_DATARECEIVED, msg);
}

void RtpDataChannel::OnDataChannelReadyToSend(bool writable) {
  // This is usded for congestion control to indicate that the stream is ready
  // to send by the MediaChannel, as opposed to OnReadyToSend, which indicates
  // that the transport channel is ready.
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_READYTOSENDDATA,
                           new DataChannelReadyToSendMessageData(writable));
}

}  // namespace cricket
