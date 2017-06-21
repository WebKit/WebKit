/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/pc/channel.h"

#include "webrtc/api/call/audio_sink.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/networkroute.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/rtputils.h"
// Adding 'nogncheck' to disable the gn include headers check to support modular
// WebRTC build targets.
#include "webrtc/media/engine/webrtcvoiceengine.h"  // nogncheck
#include "webrtc/p2p/base/packettransportinternal.h"
#include "webrtc/pc/channelmanager.h"

namespace cricket {
using rtc::Bind;

namespace {
// See comment below for why we need to use a pointer to a unique_ptr.
bool SetRawAudioSink_w(VoiceMediaChannel* channel,
                       uint32_t ssrc,
                       std::unique_ptr<webrtc::AudioSinkInterface>* sink) {
  channel->SetRawAudioSink(ssrc, std::move(*sink));
  return true;
}

struct SendPacketMessageData : public rtc::MessageData {
  rtc::CopyOnWriteBuffer packet;
  rtc::PacketOptions options;
};

#if defined(ENABLE_EXTERNAL_AUTH)
// Returns the named header extension if found among all extensions,
// nullptr otherwise.
const webrtc::RtpExtension* FindHeaderExtension(
    const std::vector<webrtc::RtpExtension>& extensions,
    const std::string& uri) {
  for (const auto& extension : extensions) {
    if (extension.uri == uri)
      return &extension;
  }
  return nullptr;
}
#endif

}  // namespace

enum {
  MSG_EARLYMEDIATIMEOUT = 1,
  MSG_SEND_RTP_PACKET,
  MSG_SEND_RTCP_PACKET,
  MSG_CHANNEL_ERROR,
  MSG_READYTOSENDDATA,
  MSG_DATARECEIVED,
  MSG_FIRSTPACKETRECEIVED,
};

// Value specified in RFC 5764.
static const char kDtlsSrtpExporterLabel[] = "EXTRACTOR-dtls_srtp";

static const int kAgcMinus10db = -10;

static void SafeSetError(const std::string& message, std::string* error_desc) {
  if (error_desc) {
    *error_desc = message;
  }
}

struct VoiceChannelErrorMessageData : public rtc::MessageData {
  VoiceChannelErrorMessageData(uint32_t in_ssrc,
                               VoiceMediaChannel::Error in_error)
      : ssrc(in_ssrc), error(in_error) {}
  uint32_t ssrc;
  VoiceMediaChannel::Error error;
};

struct VideoChannelErrorMessageData : public rtc::MessageData {
  VideoChannelErrorMessageData(uint32_t in_ssrc,
                               VideoMediaChannel::Error in_error)
      : ssrc(in_ssrc), error(in_error) {}
  uint32_t ssrc;
  VideoMediaChannel::Error error;
};

struct DataChannelErrorMessageData : public rtc::MessageData {
  DataChannelErrorMessageData(uint32_t in_ssrc,
                              DataMediaChannel::Error in_error)
      : ssrc(in_ssrc), error(in_error) {}
  uint32_t ssrc;
  DataMediaChannel::Error error;
};

static bool ValidPacket(bool rtcp, const rtc::CopyOnWriteBuffer* packet) {
  // Check the packet size. We could check the header too if needed.
  return packet && IsValidRtpRtcpPacketSize(rtcp, packet->size());
}

static bool IsReceiveContentDirection(MediaContentDirection direction) {
  return direction == MD_SENDRECV || direction == MD_RECVONLY;
}

static bool IsSendContentDirection(MediaContentDirection direction) {
  return direction == MD_SENDRECV || direction == MD_SENDONLY;
}

static const MediaContentDescription* GetContentDescription(
    const ContentInfo* cinfo) {
  if (cinfo == NULL)
    return NULL;
  return static_cast<const MediaContentDescription*>(cinfo->description);
}

template <class Codec>
void RtpParametersFromMediaDescription(
    const MediaContentDescriptionImpl<Codec>* desc,
    RtpParameters<Codec>* params) {
  // TODO(pthatcher): Remove this once we're sure no one will give us
  // a description without codecs (currently a CA_UPDATE with just
  // streams can).
  if (desc->has_codecs()) {
    params->codecs = desc->codecs();
  }
  // TODO(pthatcher): See if we really need
  // rtp_header_extensions_set() and remove it if we don't.
  if (desc->rtp_header_extensions_set()) {
    params->extensions = desc->rtp_header_extensions();
  }
  params->rtcp.reduced_size = desc->rtcp_reduced_size();
}

template <class Codec>
void RtpSendParametersFromMediaDescription(
    const MediaContentDescriptionImpl<Codec>* desc,
    RtpSendParameters<Codec>* send_params) {
  RtpParametersFromMediaDescription(desc, send_params);
  send_params->max_bandwidth_bps = desc->bandwidth();
}

BaseChannel::BaseChannel(rtc::Thread* worker_thread,
                         rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         MediaChannel* media_channel,
                         const std::string& content_name,
                         bool rtcp_mux_required,
                         bool srtp_required)
    : worker_thread_(worker_thread),
      network_thread_(network_thread),
      signaling_thread_(signaling_thread),
      content_name_(content_name),
      rtcp_mux_required_(rtcp_mux_required),
      rtp_transport_(rtcp_mux_required),
      srtp_required_(srtp_required),
      media_channel_(media_channel),
      selected_candidate_pair_(nullptr) {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
#if defined(ENABLE_EXTERNAL_AUTH)
  srtp_filter_.EnableExternalAuth();
#endif
  rtp_transport_.SignalReadyToSend.connect(
      this, &BaseChannel::OnTransportReadyToSend);
  // TODO(zstein):  RtpTransport::SignalPacketReceived will probably be replaced
  // with a callback interface later so that the demuxer can select which
  // channel to signal.
  rtp_transport_.SignalPacketReceived.connect(this,
                                              &BaseChannel::OnPacketReceived);
  LOG(LS_INFO) << "Created channel for " << content_name;
}

BaseChannel::~BaseChannel() {
  TRACE_EVENT0("webrtc", "BaseChannel::~BaseChannel");
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  Deinit();
  StopConnectionMonitor();
  // Eats any outstanding messages or packets.
  worker_thread_->Clear(&invoker_);
  worker_thread_->Clear(this);
  // We must destroy the media channel before the transport channel, otherwise
  // the media channel may try to send on the dead transport channel. NULLing
  // is not an effective strategy since the sends will come on another thread.
  delete media_channel_;
  LOG(LS_INFO) << "Destroyed channel: " << content_name_;
}

void BaseChannel::DisconnectTransportChannels_n() {
  // Send any outstanding RTCP packets.
  FlushRtcpMessages_n();

  // Stop signals from transport channels, but keep them alive because
  // media_channel may use them from a different thread.
  if (rtp_dtls_transport_) {
    DisconnectFromDtlsTransport(rtp_dtls_transport_);
  } else if (rtp_transport_.rtp_packet_transport()) {
    DisconnectFromPacketTransport(rtp_transport_.rtp_packet_transport());
  }
  if (rtcp_dtls_transport_) {
    DisconnectFromDtlsTransport(rtcp_dtls_transport_);
  } else if (rtp_transport_.rtcp_packet_transport()) {
    DisconnectFromPacketTransport(rtp_transport_.rtcp_packet_transport());
  }

  rtp_transport_.SetRtpPacketTransport(nullptr);
  rtp_transport_.SetRtcpPacketTransport(nullptr);

  // Clear pending read packets/messages.
  network_thread_->Clear(&invoker_);
  network_thread_->Clear(this);
}

bool BaseChannel::Init_w(DtlsTransportInternal* rtp_dtls_transport,
                         DtlsTransportInternal* rtcp_dtls_transport,
                         rtc::PacketTransportInternal* rtp_packet_transport,
                         rtc::PacketTransportInternal* rtcp_packet_transport) {
  if (!network_thread_->Invoke<bool>(
          RTC_FROM_HERE, Bind(&BaseChannel::InitNetwork_n, this,
                              rtp_dtls_transport, rtcp_dtls_transport,
                              rtp_packet_transport, rtcp_packet_transport))) {
    return false;
  }
  // Both RTP and RTCP channels should be set, we can call SetInterface on
  // the media channel and it can set network options.
  RTC_DCHECK_RUN_ON(worker_thread_);
  media_channel_->SetInterface(this);
  return true;
}

bool BaseChannel::InitNetwork_n(
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  SetTransports_n(rtp_dtls_transport, rtcp_dtls_transport, rtp_packet_transport,
                  rtcp_packet_transport);

  if (rtcp_mux_required_) {
    rtcp_mux_filter_.SetActive();
  }
  return true;
}

void BaseChannel::Deinit() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  media_channel_->SetInterface(NULL);
  // Packets arrive on the network thread, processing packets calls virtual
  // functions, so need to stop this process in Deinit that is called in
  // derived classes destructor.
  network_thread_->Invoke<void>(
      RTC_FROM_HERE, Bind(&BaseChannel::DisconnectTransportChannels_n, this));
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
    RTC_DCHECK(!rtp_transport_.rtp_packet_transport() || rtp_dtls_transport_);
  } else {
    // Can't go from DTLS to non-DTLS.
    RTC_DCHECK(!rtp_dtls_transport_);
  }
  // Transport names should be the same.
  if (rtp_dtls_transport && rtcp_dtls_transport) {
    RTC_DCHECK(rtp_dtls_transport->transport_name() ==
               rtcp_dtls_transport->transport_name());
  }
  std::string debug_name;
  if (rtp_dtls_transport) {
    transport_name_ = rtp_dtls_transport->transport_name();
    debug_name = transport_name_;
  } else {
    debug_name = rtp_packet_transport->debug_name();
  }
  if (rtp_packet_transport == rtp_transport_.rtp_packet_transport()) {
    // Nothing to do if transport isn't changing.
    return;
  }

  // When using DTLS-SRTP, we must reset the SrtpFilter every time the transport
  // changes and wait until the DTLS handshake is complete to set the newly
  // negotiated parameters.
  if (ShouldSetupDtlsSrtp_n()) {
    // Set |writable_| to false such that UpdateWritableState_w can set up
    // DTLS-SRTP when |writable_| becomes true again.
    writable_ = false;
    srtp_filter_.ResetParams();
  }

  // If this BaseChannel doesn't require RTCP mux and we haven't fully
  // negotiated RTCP mux, we need an RTCP transport.
  if (rtcp_packet_transport) {
    LOG(LS_INFO) << "Setting RTCP Transport for " << content_name() << " on "
                 << debug_name << " transport " << rtcp_packet_transport;
    SetTransport_n(true, rtcp_dtls_transport, rtcp_packet_transport);
  }

  LOG(LS_INFO) << "Setting RTP Transport for " << content_name() << " on "
               << debug_name << " transport " << rtp_packet_transport;
  SetTransport_n(false, rtp_dtls_transport, rtp_packet_transport);

  // Update aggregate writable/ready-to-send state between RTP and RTCP upon
  // setting new transport channels.
  UpdateWritableState_n();
}

void BaseChannel::SetTransport_n(
    bool rtcp,
    DtlsTransportInternal* new_dtls_transport,
    rtc::PacketTransportInternal* new_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  DtlsTransportInternal*& old_dtls_transport =
      rtcp ? rtcp_dtls_transport_ : rtp_dtls_transport_;
  rtc::PacketTransportInternal* old_packet_transport =
      rtcp ? rtp_transport_.rtcp_packet_transport()
           : rtp_transport_.rtp_packet_transport();

  if (!old_packet_transport && !new_packet_transport) {
    // Nothing to do.
    return;
  }

  RTC_DCHECK(old_packet_transport != new_packet_transport);
  if (old_dtls_transport) {
    DisconnectFromDtlsTransport(old_dtls_transport);
  } else if (old_packet_transport) {
    DisconnectFromPacketTransport(old_packet_transport);
  }

  if (rtcp) {
    rtp_transport_.SetRtcpPacketTransport(new_packet_transport);
  } else {
    rtp_transport_.SetRtpPacketTransport(new_packet_transport);
  }
  old_dtls_transport = new_dtls_transport;

  // If there's no new transport, we're done after disconnecting from old one.
  if (!new_packet_transport) {
    return;
  }

  if (rtcp && new_dtls_transport) {
    RTC_CHECK(!(ShouldSetupDtlsSrtp_n() && srtp_filter_.IsActive()))
        << "Setting RTCP for DTLS/SRTP after SrtpFilter is active "
        << "should never happen.";
  }

  if (new_dtls_transport) {
    ConnectToDtlsTransport(new_dtls_transport);
  } else {
    ConnectToPacketTransport(new_packet_transport);
  }
  auto& socket_options = rtcp ? rtcp_socket_options_ : socket_options_;
  for (const auto& pair : socket_options) {
    new_packet_transport->SetOption(pair.first, pair.second);
  }
}

void BaseChannel::ConnectToDtlsTransport(DtlsTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // TODO(zstein): de-dup with ConnectToPacketTransport
  transport->SignalWritableState.connect(this, &BaseChannel::OnWritableState);
  transport->SignalDtlsState.connect(this, &BaseChannel::OnDtlsState);
  transport->SignalSentPacket.connect(this, &BaseChannel::SignalSentPacket_n);
  transport->ice_transport()->SignalSelectedCandidatePairChanged.connect(
      this, &BaseChannel::OnSelectedCandidatePairChanged);
}

void BaseChannel::DisconnectFromDtlsTransport(
    DtlsTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  OnSelectedCandidatePairChanged(transport->ice_transport(), nullptr, -1,
                                 false);

  transport->SignalWritableState.disconnect(this);
  transport->SignalDtlsState.disconnect(this);
  transport->SignalSentPacket.disconnect(this);
  transport->ice_transport()->SignalSelectedCandidatePairChanged.disconnect(
      this);
}

void BaseChannel::ConnectToPacketTransport(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  transport->SignalWritableState.connect(this, &BaseChannel::OnWritableState);
  transport->SignalSentPacket.connect(this, &BaseChannel::SignalSentPacket_n);
}

void BaseChannel::DisconnectFromPacketTransport(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  transport->SignalWritableState.disconnect(this);
  transport->SignalSentPacket.disconnect(this);
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
                                  ContentAction action,
                                  std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetLocalContent");
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&BaseChannel::SetLocalContent_w, this, content, action, error_desc));
}

bool BaseChannel::SetRemoteContent(const MediaContentDescription* content,
                                   ContentAction action,
                                   std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetRemoteContent");
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&BaseChannel::SetRemoteContent_w, this, content,
                          action, error_desc));
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
  return enabled() && IsReceiveContentDirection(local_content_direction_);
}

bool BaseChannel::IsReadyToSendMedia_w() const {
  // Need to access some state updated on the network thread.
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&BaseChannel::IsReadyToSendMedia_n, this));
}

bool BaseChannel::IsReadyToSendMedia_n() const {
  // Send outgoing data if we are enabled, have local and remote content,
  // and we have had some form of connectivity.
  return enabled() && IsReceiveContentDirection(remote_content_direction_) &&
         IsSendContentDirection(local_content_direction_) &&
         was_ever_writable() &&
         (srtp_filter_.IsActive() || !ShouldSetupDtlsSrtp_n());
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
      transport = rtp_transport_.rtp_packet_transport();
      socket_options_.push_back(
          std::pair<rtc::Socket::Option, int>(opt, value));
      break;
    case ST_RTCP:
      transport = rtp_transport_.rtcp_packet_transport();
      rtcp_socket_options_.push_back(
          std::pair<rtc::Socket::Option, int>(opt, value));
      break;
  }
  return transport ? transport->SetOption(opt, value) : -1;
}

void BaseChannel::OnWritableState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(transport == rtp_transport_.rtp_packet_transport() ||
             transport == rtp_transport_.rtcp_packet_transport());
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateWritableState_n();
}

void BaseChannel::OnDtlsState(DtlsTransportInternal* transport,
                              DtlsTransportState state) {
  if (!ShouldSetupDtlsSrtp_n()) {
    return;
  }

  // Reset the srtp filter if it's not the CONNECTED state. For the CONNECTED
  // state, setting up DTLS-SRTP context is deferred to ChannelWritable_w to
  // cover other scenarios like the whole transport is writable (not just this
  // TransportChannel) or when TransportChannel is attached after DTLS is
  // negotiated.
  if (state != DTLS_TRANSPORT_CONNECTED) {
    srtp_filter_.ResetParams();
  }
}

void BaseChannel::OnSelectedCandidatePairChanged(
    IceTransportInternal* ice_transport,
    CandidatePairInterface* selected_candidate_pair,
    int last_sent_packet_id,
    bool ready_to_send) {
  RTC_DCHECK((rtp_dtls_transport_ &&
              ice_transport == rtp_dtls_transport_->ice_transport()) ||
             (rtcp_dtls_transport_ &&
              ice_transport == rtcp_dtls_transport_->ice_transport()));
  RTC_DCHECK(network_thread_->IsCurrent());
  selected_candidate_pair_ = selected_candidate_pair;
  std::string transport_name = ice_transport->transport_name();
  rtc::NetworkRoute network_route;
  if (selected_candidate_pair) {
    network_route = rtc::NetworkRoute(
        ready_to_send, selected_candidate_pair->local_candidate().network_id(),
        selected_candidate_pair->remote_candidate().network_id(),
        last_sent_packet_id);

    UpdateTransportOverhead();
  }
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      Bind(&MediaChannel::OnNetworkRouteChanged, media_channel_, transport_name,
           network_route));
}

void BaseChannel::OnTransportReadyToSend(bool ready) {
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      Bind(&MediaChannel::OnReadyToSend, media_channel_, ready));
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
  if (!rtp_transport_.IsWritable(rtcp)) {
    return false;
  }

  // Protect ourselves against crazy data.
  if (!ValidPacket(rtcp, packet)) {
    LOG(LS_ERROR) << "Dropping outgoing " << content_name_ << " "
                  << RtpRtcpStringLiteral(rtcp)
                  << " packet: wrong size=" << packet->size();
    return false;
  }

  rtc::PacketOptions updated_options;
  updated_options = options;
  // Protect if needed.
  if (srtp_filter_.IsActive()) {
    TRACE_EVENT0("webrtc", "SRTP Encode");
    bool res;
    uint8_t* data = packet->data();
    int len = static_cast<int>(packet->size());
    if (!rtcp) {
    // If ENABLE_EXTERNAL_AUTH flag is on then packet authentication is not done
    // inside libsrtp for a RTP packet. A external HMAC module will be writing
    // a fake HMAC value. This is ONLY done for a RTP packet.
    // Socket layer will update rtp sendtime extension header if present in
    // packet with current time before updating the HMAC.
#if !defined(ENABLE_EXTERNAL_AUTH)
      res = srtp_filter_.ProtectRtp(
          data, len, static_cast<int>(packet->capacity()), &len);
#else
      if (!srtp_filter_.IsExternalAuthActive()) {
        res = srtp_filter_.ProtectRtp(
            data, len, static_cast<int>(packet->capacity()), &len);
      } else {
        updated_options.packet_time_params.rtp_sendtime_extension_id =
            rtp_abs_sendtime_extn_id_;
        res = srtp_filter_.ProtectRtp(
            data, len, static_cast<int>(packet->capacity()), &len,
            &updated_options.packet_time_params.srtp_packet_index);
        // If protection succeeds, let's get auth params from srtp.
        if (res) {
          uint8_t* auth_key = NULL;
          int key_len;
          res = srtp_filter_.GetRtpAuthParams(
              &auth_key, &key_len,
              &updated_options.packet_time_params.srtp_auth_tag_len);
          if (res) {
            updated_options.packet_time_params.srtp_auth_key.resize(key_len);
            updated_options.packet_time_params.srtp_auth_key.assign(
                auth_key, auth_key + key_len);
          }
        }
      }
#endif
      if (!res) {
        int seq_num = -1;
        uint32_t ssrc = 0;
        GetRtpSeqNum(data, len, &seq_num);
        GetRtpSsrc(data, len, &ssrc);
        LOG(LS_ERROR) << "Failed to protect " << content_name_
                      << " RTP packet: size=" << len
                      << ", seqnum=" << seq_num << ", SSRC=" << ssrc;
        return false;
      }
    } else {
      res = srtp_filter_.ProtectRtcp(data, len,
                                     static_cast<int>(packet->capacity()),
                                     &len);
      if (!res) {
        int type = -1;
        GetRtcpType(data, len, &type);
        LOG(LS_ERROR) << "Failed to protect " << content_name_
                      << " RTCP packet: size=" << len << ", type=" << type;
        return false;
      }
    }

    // Update the length of the packet now that we've added the auth tag.
    packet->SetSize(len);
  } else if (srtp_required_) {
    // The audio/video engines may attempt to send RTCP packets as soon as the
    // streams are created, so don't treat this as an error for RTCP.
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=6809
    if (rtcp) {
      return false;
    }
    // However, there shouldn't be any RTP packets sent before SRTP is set up
    // (and SetSend(true) is called).
    LOG(LS_ERROR) << "Can't send outgoing RTP packet when SRTP is inactive"
                  << " and crypto is required";
    RTC_NOTREACHED();
    return false;
  }

  // Bon voyage.
  int flags = (secure() && secure_dtls()) ? PF_SRTP_BYPASS : PF_NORMAL;
  return rtp_transport_.SendPacket(rtcp, packet, updated_options, flags);
}

bool BaseChannel::HandlesPayloadType(int packet_type) const {
  return rtp_transport_.HandlesPayloadType(packet_type);
}

void BaseChannel::OnPacketReceived(bool rtcp,
                                   rtc::CopyOnWriteBuffer& packet,
                                   const rtc::PacketTime& packet_time) {
  if (!has_received_packet_ && !rtcp) {
    has_received_packet_ = true;
    signaling_thread()->Post(RTC_FROM_HERE, this, MSG_FIRSTPACKETRECEIVED);
  }

  // Unprotect the packet, if needed.
  if (srtp_filter_.IsActive()) {
    TRACE_EVENT0("webrtc", "SRTP Decode");
    char* data = packet.data<char>();
    int len = static_cast<int>(packet.size());
    bool res;
    if (!rtcp) {
      res = srtp_filter_.UnprotectRtp(data, len, &len);
      if (!res) {
        int seq_num = -1;
        uint32_t ssrc = 0;
        GetRtpSeqNum(data, len, &seq_num);
        GetRtpSsrc(data, len, &ssrc);
        LOG(LS_ERROR) << "Failed to unprotect " << content_name_
                      << " RTP packet: size=" << len << ", seqnum=" << seq_num
                      << ", SSRC=" << ssrc;
        return;
      }
    } else {
      res = srtp_filter_.UnprotectRtcp(data, len, &len);
      if (!res) {
        int type = -1;
        GetRtcpType(data, len, &type);
        LOG(LS_ERROR) << "Failed to unprotect " << content_name_
                      << " RTCP packet: size=" << len << ", type=" << type;
        return;
      }
    }

    packet.SetSize(len);
  } else if (srtp_required_) {
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
    LOG(LS_WARNING) << "Can't process incoming " << RtpRtcpStringLiteral(rtcp)
                    << " packet when SRTP is inactive and crypto is required";
    return;
  }

  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      Bind(&BaseChannel::ProcessPacket, this, rtcp, packet, packet_time));
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

bool BaseChannel::PushdownLocalDescription(
    const SessionDescription* local_desc, ContentAction action,
    std::string* error_desc) {
  const ContentInfo* content_info = GetFirstContent(local_desc);
  const MediaContentDescription* content_desc =
      GetContentDescription(content_info);
  if (content_desc && content_info && !content_info->rejected &&
      !SetLocalContent(content_desc, action, error_desc)) {
    LOG(LS_ERROR) << "Failure in SetLocalContent with action " << action;
    return false;
  }
  return true;
}

bool BaseChannel::PushdownRemoteDescription(
    const SessionDescription* remote_desc, ContentAction action,
    std::string* error_desc) {
  const ContentInfo* content_info = GetFirstContent(remote_desc);
  const MediaContentDescription* content_desc =
      GetContentDescription(content_info);
  if (content_desc && content_info && !content_info->rejected &&
      !SetRemoteContent(content_desc, action, error_desc)) {
    LOG(LS_ERROR) << "Failure in SetRemoteContent with action " << action;
    return false;
  }
  return true;
}

void BaseChannel::EnableMedia_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  if (enabled_)
    return;

  LOG(LS_INFO) << "Channel enabled";
  enabled_ = true;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::DisableMedia_w() {
  RTC_DCHECK(worker_thread_ == rtc::Thread::Current());
  if (!enabled_)
    return;

  LOG(LS_INFO) << "Channel disabled";
  enabled_ = false;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::UpdateWritableState_n() {
  rtc::PacketTransportInternal* rtp_packet_transport =
      rtp_transport_.rtp_packet_transport();
  rtc::PacketTransportInternal* rtcp_packet_transport =
      rtp_transport_.rtcp_packet_transport();
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

  LOG(LS_INFO) << "Channel writable (" << content_name_ << ")"
               << (was_ever_writable_ ? "" : " for the first time");

  if (selected_candidate_pair_)
    LOG(LS_INFO)
        << "Using "
        << selected_candidate_pair_->local_candidate().ToSensitiveString()
        << "->"
        << selected_candidate_pair_->remote_candidate().ToSensitiveString();

  was_ever_writable_ = true;
  MaybeSetupDtlsSrtp_n();
  writable_ = true;
  UpdateMediaSendRecvState();
}

void BaseChannel::SignalDtlsSrtpSetupFailure_n(bool rtcp) {
  RTC_DCHECK(network_thread_->IsCurrent());
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(),
      Bind(&BaseChannel::SignalDtlsSrtpSetupFailure_s, this, rtcp));
}

void BaseChannel::SignalDtlsSrtpSetupFailure_s(bool rtcp) {
  RTC_DCHECK(signaling_thread() == rtc::Thread::Current());
  SignalDtlsSrtpSetupFailure(this, rtcp);
}

bool BaseChannel::ShouldSetupDtlsSrtp_n() const {
  // Since DTLS is applied to all transports, checking RTP should be enough.
  return rtp_dtls_transport_ && rtp_dtls_transport_->IsDtlsActive();
}

// This function returns true if either DTLS-SRTP is not in use
// *or* DTLS-SRTP is successfully set up.
bool BaseChannel::SetupDtlsSrtp_n(bool rtcp) {
  RTC_DCHECK(network_thread_->IsCurrent());
  bool ret = false;

  DtlsTransportInternal* transport =
      rtcp ? rtcp_dtls_transport_ : rtp_dtls_transport_;
  RTC_DCHECK(transport);
  RTC_DCHECK(transport->IsDtlsActive());

  int selected_crypto_suite;

  if (!transport->GetSrtpCryptoSuite(&selected_crypto_suite)) {
    LOG(LS_ERROR) << "No DTLS-SRTP selected crypto suite";
    return false;
  }

  LOG(LS_INFO) << "Installing keys from DTLS-SRTP on " << content_name() << " "
               << RtpRtcpStringLiteral(rtcp);

  int key_len;
  int salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(selected_crypto_suite, &key_len,
      &salt_len)) {
    LOG(LS_ERROR) << "Unknown DTLS-SRTP crypto suite" << selected_crypto_suite;
    return false;
  }

  // OK, we're now doing DTLS (RFC 5764)
  std::vector<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);

  // RFC 5705 exporter using the RFC 5764 parameters
  if (!transport->ExportKeyingMaterial(kDtlsSrtpExporterLabel, NULL, 0, false,
                                       &dtls_buffer[0], dtls_buffer.size())) {
    LOG(LS_WARNING) << "DTLS-SRTP key export failed";
    RTC_NOTREACHED();  // This should never happen
    return false;
  }

  // Sync up the keys with the DTLS-SRTP interface
  std::vector<unsigned char> client_write_key(key_len + salt_len);
  std::vector<unsigned char> server_write_key(key_len + salt_len);
  size_t offset = 0;
  memcpy(&client_write_key[0], &dtls_buffer[offset], key_len);
  offset += key_len;
  memcpy(&server_write_key[0], &dtls_buffer[offset], key_len);
  offset += key_len;
  memcpy(&client_write_key[key_len], &dtls_buffer[offset], salt_len);
  offset += salt_len;
  memcpy(&server_write_key[key_len], &dtls_buffer[offset], salt_len);

  std::vector<unsigned char> *send_key, *recv_key;
  rtc::SSLRole role;
  if (!transport->GetSslRole(&role)) {
    LOG(LS_WARNING) << "GetSslRole failed";
    return false;
  }

  if (role == rtc::SSL_SERVER) {
    send_key = &server_write_key;
    recv_key = &client_write_key;
  } else {
    send_key = &client_write_key;
    recv_key = &server_write_key;
  }

  if (rtcp) {
    ret = srtp_filter_.SetRtcpParams(selected_crypto_suite, &(*send_key)[0],
                                     static_cast<int>(send_key->size()),
                                     selected_crypto_suite, &(*recv_key)[0],
                                     static_cast<int>(recv_key->size()));
  } else {
    ret = srtp_filter_.SetRtpParams(selected_crypto_suite, &(*send_key)[0],
                                    static_cast<int>(send_key->size()),
                                    selected_crypto_suite, &(*recv_key)[0],
                                    static_cast<int>(recv_key->size()));
  }

  if (!ret) {
    LOG(LS_WARNING) << "DTLS-SRTP key installation failed";
  } else {
    dtls_keyed_ = true;
    UpdateTransportOverhead();
  }
  return ret;
}

void BaseChannel::MaybeSetupDtlsSrtp_n() {
  if (srtp_filter_.IsActive()) {
    return;
  }

  if (!ShouldSetupDtlsSrtp_n()) {
    return;
  }

  if (!SetupDtlsSrtp_n(false)) {
    SignalDtlsSrtpSetupFailure_n(false);
    return;
  }

  if (rtcp_dtls_transport_) {
    if (!SetupDtlsSrtp_n(true)) {
      SignalDtlsSrtpSetupFailure_n(true);
      return;
    }
  }
}

void BaseChannel::ChannelNotWritable_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  if (!writable_)
    return;

  LOG(LS_INFO) << "Channel not writable (" << content_name_ << ")";
  writable_ = false;
  UpdateMediaSendRecvState();
}

bool BaseChannel::SetRtpTransportParameters(
    const MediaContentDescription* content,
    ContentAction action,
    ContentSource src,
    std::string* error_desc) {
  if (action == CA_UPDATE) {
    // These parameters never get changed by a CA_UDPATE.
    return true;
  }

  // Cache srtp_required_ for belt and suspenders check on SendPacket
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, Bind(&BaseChannel::SetRtpTransportParameters_n, this,
                          content, action, src, error_desc));
}

bool BaseChannel::SetRtpTransportParameters_n(
    const MediaContentDescription* content,
    ContentAction action,
    ContentSource src,
    std::string* error_desc) {
  RTC_DCHECK(network_thread_->IsCurrent());

  if (!SetSrtp_n(content->cryptos(), action, src, error_desc)) {
    return false;
  }

  if (!SetRtcpMux_n(content->rtcp_mux(), action, src, error_desc)) {
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

bool BaseChannel::SetSrtp_n(const std::vector<CryptoParams>& cryptos,
                            ContentAction action,
                            ContentSource src,
                            std::string* error_desc) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetSrtp_w");
  if (action == CA_UPDATE) {
    // no crypto params.
    return true;
  }
  bool ret = false;
  bool dtls = false;
  ret = CheckSrtpConfig_n(cryptos, &dtls, error_desc);
  if (!ret) {
    return false;
  }
  switch (action) {
    case CA_OFFER:
      // If DTLS is already active on the channel, we could be renegotiating
      // here. We don't update the srtp filter.
      if (!dtls) {
        ret = srtp_filter_.SetOffer(cryptos, src);
      }
      break;
    case CA_PRANSWER:
      // If we're doing DTLS-SRTP, we don't want to update the filter
      // with an answer, because we already have SRTP parameters.
      if (!dtls) {
        ret = srtp_filter_.SetProvisionalAnswer(cryptos, src);
      }
      break;
    case CA_ANSWER:
      // If we're doing DTLS-SRTP, we don't want to update the filter
      // with an answer, because we already have SRTP parameters.
      if (!dtls) {
        ret = srtp_filter_.SetAnswer(cryptos, src);
      }
      break;
    default:
      break;
  }
  if (!ret) {
    SafeSetError("Failed to setup SRTP filter.", error_desc);
    return false;
  }
  return true;
}

bool BaseChannel::SetRtcpMux_n(bool enable,
                               ContentAction action,
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
  switch (action) {
    case CA_OFFER:
      ret = rtcp_mux_filter_.SetOffer(enable, src);
      break;
    case CA_PRANSWER:
      // This may activate RTCP muxing, but we don't yet destroy the transport
      // because the final answer may deactivate it.
      ret = rtcp_mux_filter_.SetProvisionalAnswer(enable, src);
      break;
    case CA_ANSWER:
      ret = rtcp_mux_filter_.SetAnswer(enable, src);
      if (ret && rtcp_mux_filter_.IsActive()) {
        // We permanently activated RTCP muxing; signal that we no longer need
        // the RTCP transport.
        std::string debug_name =
            transport_name_.empty()
                ? rtp_transport_.rtp_packet_transport()->debug_name()
                : transport_name_;
        ;
        LOG(LS_INFO) << "Enabling rtcp-mux for " << content_name()
                     << "; no longer need RTCP transport for " << debug_name;
        if (rtp_transport_.rtcp_packet_transport()) {
          SetTransport_n(true, nullptr, nullptr);
          SignalRtcpMuxFullyActive(transport_name_);
        }
        UpdateWritableState_n();
      }
      break;
    case CA_UPDATE:
      // No RTCP mux info.
      ret = true;
      break;
    default:
      break;
  }
  if (!ret) {
    SafeSetError("Failed to setup RTCP mux filter.", error_desc);
    return false;
  }
  rtp_transport_.SetRtcpMuxEnabled(rtcp_mux_filter_.IsActive());
  // |rtcp_mux_filter_| can be active if |action| is CA_PRANSWER or
  // CA_ANSWER, but we only want to tear down the RTCP transport if we received
  // a final answer.
  if (rtcp_mux_filter_.IsActive()) {
    // If the RTP transport is already writable, then so are we.
    if (rtp_transport_.rtp_packet_transport()->writable()) {
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
                                       ContentAction action,
                                       std::string* error_desc) {
  if (!(action == CA_OFFER || action == CA_ANSWER ||
        action == CA_PRANSWER || action == CA_UPDATE))
    return false;

  // If this is an update, streams only contain streams that have changed.
  if (action == CA_UPDATE) {
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      const StreamParams* existing_stream =
          GetStreamByIds(local_streams_, it->groupid, it->id);
      if (!existing_stream && it->has_ssrcs()) {
        if (media_channel()->AddSendStream(*it)) {
          local_streams_.push_back(*it);
          LOG(LS_INFO) << "Add send stream ssrc: " << it->first_ssrc();
        } else {
          std::ostringstream desc;
          desc << "Failed to add send stream ssrc: " << it->first_ssrc();
          SafeSetError(desc.str(), error_desc);
          return false;
        }
      } else if (existing_stream && !it->has_ssrcs()) {
        if (!media_channel()->RemoveSendStream(existing_stream->first_ssrc())) {
          std::ostringstream desc;
          desc << "Failed to remove send stream with ssrc "
               << it->first_ssrc() << ".";
          SafeSetError(desc.str(), error_desc);
          return false;
        }
        RemoveStreamBySsrc(&local_streams_, existing_stream->first_ssrc());
      } else {
        LOG(LS_WARNING) << "Ignore unsupported stream update";
      }
    }
    return true;
  }
  // Else streams are all the streams we want to send.

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
        LOG(LS_INFO) << "Add send stream ssrc: " << it->ssrcs[0];
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
    ContentAction action,
    std::string* error_desc) {
  if (!(action == CA_OFFER || action == CA_ANSWER ||
        action == CA_PRANSWER || action == CA_UPDATE))
    return false;

  // If this is an update, streams only contain streams that have changed.
  if (action == CA_UPDATE) {
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      const StreamParams* existing_stream =
          GetStreamByIds(remote_streams_, it->groupid, it->id);
      if (!existing_stream && it->has_ssrcs()) {
        if (AddRecvStream_w(*it)) {
          remote_streams_.push_back(*it);
          LOG(LS_INFO) << "Add remote stream ssrc: " << it->first_ssrc();
        } else {
          std::ostringstream desc;
          desc << "Failed to add remote stream ssrc: " << it->first_ssrc();
          SafeSetError(desc.str(), error_desc);
          return false;
        }
      } else if (existing_stream && !it->has_ssrcs()) {
        if (!RemoveRecvStream_w(existing_stream->first_ssrc())) {
          std::ostringstream desc;
          desc << "Failed to remove remote stream with ssrc "
               << it->first_ssrc() << ".";
          SafeSetError(desc.str(), error_desc);
          return false;
        }
        RemoveStreamBySsrc(&remote_streams_, existing_stream->first_ssrc());
      } else {
        LOG(LS_WARNING) << "Ignore unsupported stream update."
                        << " Stream exists? " << (existing_stream != nullptr)
                        << " new stream = " << it->ToString();
      }
    }
    return true;
  }
  // Else streams are all the streams we want to receive.

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
        LOG(LS_INFO) << "Add remote ssrc: " << it->ssrcs[0];
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

void BaseChannel::MaybeCacheRtpAbsSendTimeHeaderExtension_w(
    const std::vector<webrtc::RtpExtension>& extensions) {
// Absolute Send Time extension id is used only with external auth,
// so do not bother searching for it and making asyncronious call to set
// something that is not used.
#if defined(ENABLE_EXTERNAL_AUTH)
  const webrtc::RtpExtension* send_time_extension =
      FindHeaderExtension(extensions, webrtc::RtpExtension::kAbsSendTimeUri);
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
  rtp_abs_sendtime_extn_id_ = rtp_abs_sendtime_extn_id;
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
  rtp_transport_.AddHandledPayloadType(payload_type);
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

void BaseChannel::SignalSentPacket_n(
    rtc::PacketTransportInternal* /* transport */,
    const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(network_thread_->IsCurrent());
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, worker_thread_,
      rtc::Bind(&BaseChannel::SignalSentPacket_w, this, sent_packet));
}

void BaseChannel::SignalSentPacket_w(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  SignalSentPacket(sent_packet);
}

VoiceChannel::VoiceChannel(rtc::Thread* worker_thread,
                           rtc::Thread* network_thread,
                           rtc::Thread* signaling_thread,
                           MediaEngineInterface* media_engine,
                           VoiceMediaChannel* media_channel,
                           const std::string& content_name,
                           bool rtcp_mux_required,
                           bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  media_channel,
                  content_name,
                  rtcp_mux_required,
                  srtp_required),
      media_engine_(media_engine),
      received_media_(false) {}

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

bool VoiceChannel::CanInsertDtmf() {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&VoiceMediaChannel::CanInsertDtmf, media_channel()));
}

bool VoiceChannel::InsertDtmf(uint32_t ssrc,
                              int event_code,
                              int duration) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VoiceChannel::InsertDtmf_w, this, ssrc, event_code, duration));
}

bool VoiceChannel::SetOutputVolume(uint32_t ssrc, double volume) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VoiceMediaChannel::SetOutputVolume, media_channel(), ssrc, volume));
}

void VoiceChannel::SetRawAudioSink(
    uint32_t ssrc,
    std::unique_ptr<webrtc::AudioSinkInterface> sink) {
  // We need to work around Bind's lack of support for unique_ptr and ownership
  // passing.  So we invoke to our own little routine that gets a pointer to
  // our local variable.  This is OK since we're synchronously invoking.
  InvokeOnWorker<bool>(RTC_FROM_HERE,
                       Bind(&SetRawAudioSink_w, media_channel(), ssrc, &sink));
}

webrtc::RtpParameters VoiceChannel::GetRtpSendParameters(uint32_t ssrc) const {
  return worker_thread()->Invoke<webrtc::RtpParameters>(
      RTC_FROM_HERE, Bind(&VoiceChannel::GetRtpSendParameters_w, this, ssrc));
}

webrtc::RtpParameters VoiceChannel::GetRtpSendParameters_w(
    uint32_t ssrc) const {
  return media_channel()->GetRtpSendParameters(ssrc);
}

bool VoiceChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VoiceChannel::SetRtpSendParameters_w, this, ssrc, parameters));
}

bool VoiceChannel::SetRtpSendParameters_w(uint32_t ssrc,
                                          webrtc::RtpParameters parameters) {
  return media_channel()->SetRtpSendParameters(ssrc, parameters);
}

webrtc::RtpParameters VoiceChannel::GetRtpReceiveParameters(
    uint32_t ssrc) const {
  return worker_thread()->Invoke<webrtc::RtpParameters>(
      RTC_FROM_HERE,
      Bind(&VoiceChannel::GetRtpReceiveParameters_w, this, ssrc));
}

webrtc::RtpParameters VoiceChannel::GetRtpReceiveParameters_w(
    uint32_t ssrc) const {
  return media_channel()->GetRtpReceiveParameters(ssrc);
}

bool VoiceChannel::SetRtpReceiveParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VoiceChannel::SetRtpReceiveParameters_w, this, ssrc, parameters));
}

bool VoiceChannel::SetRtpReceiveParameters_w(uint32_t ssrc,
                                             webrtc::RtpParameters parameters) {
  return media_channel()->SetRtpReceiveParameters(ssrc, parameters);
}

bool VoiceChannel::GetStats(VoiceMediaInfo* stats) {
  return InvokeOnWorker<bool>(RTC_FROM_HERE, Bind(&VoiceMediaChannel::GetStats,
                                                  media_channel(), stats));
}

std::vector<webrtc::RtpSource> VoiceChannel::GetSources(uint32_t ssrc) const {
  return worker_thread()->Invoke<std::vector<webrtc::RtpSource>>(
      RTC_FROM_HERE, Bind(&VoiceChannel::GetSources_w, this, ssrc));
}

std::vector<webrtc::RtpSource> VoiceChannel::GetSources_w(uint32_t ssrc) const {
  RTC_DCHECK(worker_thread()->IsCurrent());
  return media_channel()->GetSources(ssrc);
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
                                    rtc::CopyOnWriteBuffer& packet,
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

int BaseChannel::GetTransportOverheadPerPacket() const {
  RTC_DCHECK(network_thread_->IsCurrent());

  if (!selected_candidate_pair_)
    return 0;

  int transport_overhead_per_packet = 0;

  constexpr int kIpv4Overhaed = 20;
  constexpr int kIpv6Overhaed = 40;
  transport_overhead_per_packet +=
      selected_candidate_pair_->local_candidate().address().family() == AF_INET
          ? kIpv4Overhaed
          : kIpv6Overhaed;

  constexpr int kUdpOverhaed = 8;
  constexpr int kTcpOverhaed = 20;
  transport_overhead_per_packet +=
      selected_candidate_pair_->local_candidate().protocol() ==
              TCP_PROTOCOL_NAME
          ? kTcpOverhaed
          : kUdpOverhaed;

  if (secure()) {
    int srtp_overhead = 0;
    if (srtp_filter_.GetSrtpOverhead(&srtp_overhead))
      transport_overhead_per_packet += srtp_overhead;
  }

  return transport_overhead_per_packet;
}

void BaseChannel::UpdateTransportOverhead() {
  int transport_overhead_per_packet = GetTransportOverheadPerPacket();
  if (transport_overhead_per_packet)
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, worker_thread_,
        Bind(&MediaChannel::OnTransportOverheadChanged, media_channel_,
             transport_overhead_per_packet));
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

  LOG(LS_INFO) << "Changing voice state, recv=" << recv << " send=" << send;
}

const ContentInfo* VoiceChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstAudioContent(sdesc);
}

bool VoiceChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     ContentAction action,
                                     std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetLocalContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  LOG(LS_INFO) << "Setting local voice description";

  const AudioContentDescription* audio =
      static_cast<const AudioContentDescription*>(content);
  RTC_DCHECK(audio != NULL);
  if (!audio) {
    SafeSetError("Can't find audio content in local description.", error_desc);
    return false;
  }

  if (!SetRtpTransportParameters(content, action, CS_LOCAL, error_desc)) {
    return false;
  }

  AudioRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(audio, &recv_params);
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
  if (!UpdateLocalStreams_w(audio->streams(), action, error_desc)) {
    SafeSetError("Failed to set local audio description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool VoiceChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      ContentAction action,
                                      std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetRemoteContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  LOG(LS_INFO) << "Setting remote voice description";

  const AudioContentDescription* audio =
      static_cast<const AudioContentDescription*>(content);
  RTC_DCHECK(audio != NULL);
  if (!audio) {
    SafeSetError("Can't find audio content in remote description.", error_desc);
    return false;
  }

  if (!SetRtpTransportParameters(content, action, CS_REMOTE, error_desc)) {
    return false;
  }

  AudioSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(audio, &send_params);
  if (audio->agc_minus_10db()) {
    send_params.options.adjust_agc_delta = rtc::Optional<int>(kAgcMinus10db);
  }

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
  if (!UpdateRemoteStreams_w(audio->streams(), action, error_desc)) {
    SafeSetError("Failed to set remote audio description streams.", error_desc);
    return false;
  }

  if (audio->rtp_header_extensions_set()) {
    MaybeCacheRtpAbsSendTimeHeaderExtension_w(audio->rtp_header_extensions());
  }

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

void VoiceChannel::HandleEarlyMediaTimeout() {
  // This occurs on the main thread, not the worker thread.
  if (!received_media_) {
    LOG(LS_INFO) << "No early media received before timeout";
    SignalEarlyMediaTimeout(this);
  }
}

bool VoiceChannel::InsertDtmf_w(uint32_t ssrc,
                                int event,
                                int duration) {
  if (!enabled()) {
    return false;
  }
  return media_channel()->InsertDtmf(ssrc, event, duration);
}

void VoiceChannel::OnMessage(rtc::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_EARLYMEDIATIMEOUT:
      HandleEarlyMediaTimeout();
      break;
    case MSG_CHANNEL_ERROR: {
      VoiceChannelErrorMessageData* data =
          static_cast<VoiceChannelErrorMessageData*>(pmsg->pdata);
      delete data;
      break;
    }
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
                           VideoMediaChannel* media_channel,
                           const std::string& content_name,
                           bool rtcp_mux_required,
                           bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  media_channel,
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

bool VideoChannel::SetSink(uint32_t ssrc,
                           rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  worker_thread()->Invoke<void>(
      RTC_FROM_HERE,
      Bind(&VideoMediaChannel::SetSink, media_channel(), ssrc, sink));
  return true;
}

bool VideoChannel::SetVideoSend(
    uint32_t ssrc,
    bool mute,
    const VideoOptions* options,
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&VideoMediaChannel::SetVideoSend, media_channel(),
                          ssrc, mute, options, source));
}

webrtc::RtpParameters VideoChannel::GetRtpSendParameters(uint32_t ssrc) const {
  return worker_thread()->Invoke<webrtc::RtpParameters>(
      RTC_FROM_HERE, Bind(&VideoChannel::GetRtpSendParameters_w, this, ssrc));
}

webrtc::RtpParameters VideoChannel::GetRtpSendParameters_w(
    uint32_t ssrc) const {
  return media_channel()->GetRtpSendParameters(ssrc);
}

bool VideoChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VideoChannel::SetRtpSendParameters_w, this, ssrc, parameters));
}

bool VideoChannel::SetRtpSendParameters_w(uint32_t ssrc,
                                          webrtc::RtpParameters parameters) {
  return media_channel()->SetRtpSendParameters(ssrc, parameters);
}

webrtc::RtpParameters VideoChannel::GetRtpReceiveParameters(
    uint32_t ssrc) const {
  return worker_thread()->Invoke<webrtc::RtpParameters>(
      RTC_FROM_HERE,
      Bind(&VideoChannel::GetRtpReceiveParameters_w, this, ssrc));
}

webrtc::RtpParameters VideoChannel::GetRtpReceiveParameters_w(
    uint32_t ssrc) const {
  return media_channel()->GetRtpReceiveParameters(ssrc);
}

bool VideoChannel::SetRtpReceiveParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE,
      Bind(&VideoChannel::SetRtpReceiveParameters_w, this, ssrc, parameters));
}

bool VideoChannel::SetRtpReceiveParameters_w(uint32_t ssrc,
                                             webrtc::RtpParameters parameters) {
  return media_channel()->SetRtpReceiveParameters(ssrc, parameters);
}

void VideoChannel::UpdateMediaSendRecvState_w() {
  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  if (!media_channel()->SetSend(send)) {
    LOG(LS_ERROR) << "Failed to SetSend on video channel";
    // TODO(gangji): Report error back to server.
  }

  LOG(LS_INFO) << "Changing video state, send=" << send;
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

const ContentInfo* VideoChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstVideoContent(sdesc);
}

bool VideoChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     ContentAction action,
                                     std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetLocalContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  LOG(LS_INFO) << "Setting local video description";

  const VideoContentDescription* video =
      static_cast<const VideoContentDescription*>(content);
  RTC_DCHECK(video != NULL);
  if (!video) {
    SafeSetError("Can't find video content in local description.", error_desc);
    return false;
  }

  if (!SetRtpTransportParameters(content, action, CS_LOCAL, error_desc)) {
    return false;
  }

  VideoRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(video, &recv_params);
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
  if (!UpdateLocalStreams_w(video->streams(), action, error_desc)) {
    SafeSetError("Failed to set local video description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool VideoChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      ContentAction action,
                                      std::string* error_desc) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetRemoteContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  LOG(LS_INFO) << "Setting remote video description";

  const VideoContentDescription* video =
      static_cast<const VideoContentDescription*>(content);
  RTC_DCHECK(video != NULL);
  if (!video) {
    SafeSetError("Can't find video content in remote description.", error_desc);
    return false;
  }

  if (!SetRtpTransportParameters(content, action, CS_REMOTE, error_desc)) {
    return false;
  }

  VideoSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(video, &send_params);
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
  if (!UpdateRemoteStreams_w(video->streams(), action, error_desc)) {
    SafeSetError("Failed to set remote video description streams.", error_desc);
    return false;
  }

  if (video->rtp_header_extensions_set()) {
    MaybeCacheRtpAbsSendTimeHeaderExtension_w(video->rtp_header_extensions());
  }

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

void VideoChannel::OnMessage(rtc::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_CHANNEL_ERROR: {
      const VideoChannelErrorMessageData* data =
          static_cast<VideoChannelErrorMessageData*>(pmsg->pdata);
      delete data;
      break;
    }
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
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
                               DataMediaChannel* media_channel,
                               const std::string& content_name,
                               bool rtcp_mux_required,
                               bool srtp_required)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  media_channel,
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

bool RtpDataChannel::Init_w(
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  if (!BaseChannel::Init_w(rtp_dtls_transport, rtcp_dtls_transport,
                           rtp_packet_transport, rtcp_packet_transport)) {
    return false;
  }
  media_channel()->SignalDataReceived.connect(this,
                                              &RtpDataChannel::OnDataReceived);
  media_channel()->SignalReadyToSend.connect(
      this, &RtpDataChannel::OnDataChannelReadyToSend);
  return true;
}

bool RtpDataChannel::SendData(const SendDataParams& params,
                              const rtc::CopyOnWriteBuffer& payload,
                              SendDataResult* result) {
  return InvokeOnWorker<bool>(
      RTC_FROM_HERE, Bind(&DataMediaChannel::SendData, media_channel(), params,
                          payload, result));
}

const ContentInfo* RtpDataChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstDataContent(sdesc);
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
                                       ContentAction action,
                                       std::string* error_desc) {
  TRACE_EVENT0("webrtc", "RtpDataChannel::SetLocalContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());
  LOG(LS_INFO) << "Setting local data description";

  const DataContentDescription* data =
      static_cast<const DataContentDescription*>(content);
  RTC_DCHECK(data != NULL);
  if (!data) {
    SafeSetError("Can't find data content in local description.", error_desc);
    return false;
  }

  if (!CheckDataChannelTypeFromContent(data, error_desc)) {
    return false;
  }

  if (!SetRtpTransportParameters(content, action, CS_LOCAL, error_desc)) {
    return false;
  }

  DataRecvParameters recv_params = last_recv_params_;
  RtpParametersFromMediaDescription(data, &recv_params);
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
  if (!UpdateLocalStreams_w(data->streams(), action, error_desc)) {
    SafeSetError("Failed to set local data description streams.", error_desc);
    return false;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();
  return true;
}

bool RtpDataChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                        ContentAction action,
                                        std::string* error_desc) {
  TRACE_EVENT0("webrtc", "RtpDataChannel::SetRemoteContent_w");
  RTC_DCHECK(worker_thread() == rtc::Thread::Current());

  const DataContentDescription* data =
      static_cast<const DataContentDescription*>(content);
  RTC_DCHECK(data != NULL);
  if (!data) {
    SafeSetError("Can't find data content in remote description.", error_desc);
    return false;
  }

  // If the remote data doesn't have codecs and isn't an update, it
  // must be empty, so ignore it.
  if (!data->has_codecs() && action != CA_UPDATE) {
    return true;
  }

  if (!CheckDataChannelTypeFromContent(data, error_desc)) {
    return false;
  }

  LOG(LS_INFO) << "Setting remote data description";
  if (!SetRtpTransportParameters(content, action, CS_REMOTE, error_desc)) {
    return false;
  }

  DataSendParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription<DataCodec>(data, &send_params);
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
  if (!UpdateRemoteStreams_w(data->streams(), action, error_desc)) {
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
    LOG(LS_ERROR) << "Failed to SetReceive on data channel";
  }

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  if (!media_channel()->SetSend(send)) {
    LOG(LS_ERROR) << "Failed to SetSend on data channel";
  }

  // Trigger SignalReadyToSendData asynchronously.
  OnDataChannelReadyToSend(send);

  LOG(LS_INFO) << "Changing data state, recv=" << recv << " send=" << send;
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
    case MSG_CHANNEL_ERROR: {
      const DataChannelErrorMessageData* data =
          static_cast<DataChannelErrorMessageData*>(pmsg->pdata);
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

void RtpDataChannel::OnDataChannelError(uint32_t ssrc,
                                        DataMediaChannel::Error err) {
  DataChannelErrorMessageData* data = new DataChannelErrorMessageData(
      ssrc, err);
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_CHANNEL_ERROR, data);
}

void RtpDataChannel::OnDataChannelReadyToSend(bool writable) {
  // This is usded for congestion control to indicate that the stream is ready
  // to send by the MediaChannel, as opposed to OnReadyToSend, which indicates
  // that the transport channel is ready.
  signaling_thread()->Post(RTC_FROM_HERE, this, MSG_READYTOSENDDATA,
                           new DataChannelReadyToSendMessageData(writable));
}

}  // namespace cricket
