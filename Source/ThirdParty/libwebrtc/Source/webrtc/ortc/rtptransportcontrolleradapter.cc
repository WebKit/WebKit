/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/rtptransportcontrolleradapter.h"

#include <algorithm>  // For "remove", "find".
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>  // For std::move.

#include "webrtc/api/proxy.h"
#include "webrtc/base/checks.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/ortc/ortcrtpreceiveradapter.h"
#include "webrtc/ortc/ortcrtpsenderadapter.h"
#include "webrtc/ortc/rtpparametersconversion.h"
#include "webrtc/ortc/rtptransportadapter.h"

namespace webrtc {

// Note: It's assumed that each individual list doesn't have conflicts, since
// they should have been detected already by rtpparametersconversion.cc. This
// only needs to detect conflicts *between* A and B.
template <typename C1, typename C2>
static RTCError CheckForIdConflicts(
    const std::vector<C1>& codecs_a,
    const cricket::RtpHeaderExtensions& extensions_a,
    const cricket::StreamParamsVec& streams_a,
    const std::vector<C2>& codecs_b,
    const cricket::RtpHeaderExtensions& extensions_b,
    const cricket::StreamParamsVec& streams_b) {
  std::ostringstream oss;
  // Since it's assumed that C1 and C2 are different types, codecs_a and
  // codecs_b should never contain the same payload type, and thus we can just
  // use a set.
  std::set<int> seen_payload_types;
  for (const C1& codec : codecs_a) {
    seen_payload_types.insert(codec.id);
  }
  for (const C2& codec : codecs_b) {
    if (!seen_payload_types.insert(codec.id).second) {
      oss << "Same payload type used for audio and video codecs: " << codec.id;
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, oss.str());
    }
  }
  // Audio and video *may* use the same header extensions, so use a map.
  std::unordered_map<int, std::string> seen_extensions;
  for (const webrtc::RtpExtension& extension : extensions_a) {
    seen_extensions[extension.id] = extension.uri;
  }
  for (const webrtc::RtpExtension& extension : extensions_b) {
    if (seen_extensions.find(extension.id) != seen_extensions.end() &&
        seen_extensions.at(extension.id) != extension.uri) {
      oss << "Same ID used for different RTP header extensions: "
          << extension.id;
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, oss.str());
    }
  }
  std::set<uint32_t> seen_ssrcs;
  for (const cricket::StreamParams& stream : streams_a) {
    seen_ssrcs.insert(stream.ssrcs.begin(), stream.ssrcs.end());
  }
  for (const cricket::StreamParams& stream : streams_b) {
    for (uint32_t ssrc : stream.ssrcs) {
      if (!seen_ssrcs.insert(ssrc).second) {
        oss << "Same SSRC used for audio and video senders: " << ssrc;
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, oss.str());
      }
    }
  }
  return RTCError::OK();
}

BEGIN_OWNED_PROXY_MAP(RtpTransportController)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(std::vector<RtpTransportInterface*>, GetTransports)
protected:
RtpTransportControllerAdapter* GetInternal() override {
  return internal();
}
END_PROXY_MAP()

// static
std::unique_ptr<RtpTransportControllerInterface>
RtpTransportControllerAdapter::CreateProxied(
    const cricket::MediaConfig& config,
    cricket::ChannelManager* channel_manager,
    webrtc::RtcEventLog* event_log,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread) {
  std::unique_ptr<RtpTransportControllerAdapter> wrapped(
      new RtpTransportControllerAdapter(config, channel_manager, event_log,
                                        signaling_thread, worker_thread));
  return RtpTransportControllerProxyWithInternal<
      RtpTransportControllerAdapter>::Create(signaling_thread, worker_thread,
                                             std::move(wrapped));
}

RtpTransportControllerAdapter::~RtpTransportControllerAdapter() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (!transport_proxies_.empty()) {
    LOG(LS_ERROR)
        << "Destroying RtpTransportControllerAdapter while RtpTransports "
           "are still using it; this is unsafe.";
  }
  if (voice_channel_) {
    // This would mean audio RTP senders/receivers that are using us haven't
    // been destroyed. This isn't safe (see error log above).
    DestroyVoiceChannel();
  }
  if (voice_channel_) {
    // This would mean video RTP senders/receivers that are using us haven't
    // been destroyed. This isn't safe (see error log above).
    DestroyVideoChannel();
  }
  // Call must be destroyed on the worker thread.
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&RtpTransportControllerAdapter::Close_w, this));
}

RTCErrorOr<std::unique_ptr<RtpTransportInterface>>
RtpTransportControllerAdapter::CreateProxiedRtpTransport(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp) {
  auto result =
      RtpTransportAdapter::CreateProxied(rtcp_parameters, rtp, rtcp, this);
  if (result.ok()) {
    transport_proxies_.push_back(result.value().get());
    transport_proxies_.back()->GetInternal()->SignalDestroyed.connect(
        this, &RtpTransportControllerAdapter::OnRtpTransportDestroyed);
  }
  return result;
}

RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>
RtpTransportControllerAdapter::CreateProxiedSrtpTransport(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp) {
  auto result =
      RtpTransportAdapter::CreateSrtpProxied(rtcp_parameters, rtp, rtcp, this);
  if (result.ok()) {
    transport_proxies_.push_back(result.value().get());
    transport_proxies_.back()->GetInternal()->SignalDestroyed.connect(
        this, &RtpTransportControllerAdapter::OnRtpTransportDestroyed);
  }
  return result;
}

RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>>
RtpTransportControllerAdapter::CreateProxiedRtpSender(
    cricket::MediaType kind,
    RtpTransportInterface* transport_proxy) {
  RTC_DCHECK(transport_proxy);
  RTC_DCHECK(std::find(transport_proxies_.begin(), transport_proxies_.end(),
                       transport_proxy) != transport_proxies_.end());
  std::unique_ptr<OrtcRtpSenderAdapter> new_sender(
      new OrtcRtpSenderAdapter(kind, transport_proxy, this));
  RTCError err;
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO:
      err = AttachAudioSender(new_sender.get(), transport_proxy->GetInternal());
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      err = AttachVideoSender(new_sender.get(), transport_proxy->GetInternal());
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
  }
  if (!err.ok()) {
    return std::move(err);
  }

  return OrtcRtpSenderAdapter::CreateProxy(std::move(new_sender));
}

RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>>
RtpTransportControllerAdapter::CreateProxiedRtpReceiver(
    cricket::MediaType kind,
    RtpTransportInterface* transport_proxy) {
  RTC_DCHECK(transport_proxy);
  RTC_DCHECK(std::find(transport_proxies_.begin(), transport_proxies_.end(),
                       transport_proxy) != transport_proxies_.end());
  std::unique_ptr<OrtcRtpReceiverAdapter> new_receiver(
      new OrtcRtpReceiverAdapter(kind, transport_proxy, this));
  RTCError err;
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO:
      err = AttachAudioReceiver(new_receiver.get(),
                                transport_proxy->GetInternal());
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      err = AttachVideoReceiver(new_receiver.get(),
                                transport_proxy->GetInternal());
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
  }
  if (!err.ok()) {
    return std::move(err);
  }

  return OrtcRtpReceiverAdapter::CreateProxy(std::move(new_receiver));
}

std::vector<RtpTransportInterface*>
RtpTransportControllerAdapter::GetTransports() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return transport_proxies_;
}

RTCError RtpTransportControllerAdapter::SetRtcpParameters(
    const RtcpParameters& parameters,
    RtpTransportInterface* inner_transport) {
  do {
    if (inner_transport == inner_audio_transport_) {
      CopyRtcpParametersToDescriptions(parameters, &local_audio_description_,
                                       &remote_audio_description_);
      if (!voice_channel_->SetLocalContent(&local_audio_description_,
                                           cricket::CA_OFFER, nullptr)) {
        break;
      }
      if (!voice_channel_->SetRemoteContent(&remote_audio_description_,
                                            cricket::CA_ANSWER, nullptr)) {
        break;
      }
    } else if (inner_transport == inner_video_transport_) {
      CopyRtcpParametersToDescriptions(parameters, &local_video_description_,
                                       &remote_video_description_);
      if (!video_channel_->SetLocalContent(&local_video_description_,
                                           cricket::CA_OFFER, nullptr)) {
        break;
      }
      if (!video_channel_->SetRemoteContent(&remote_video_description_,
                                            cricket::CA_ANSWER, nullptr)) {
        break;
      }
    }
    return RTCError::OK();
  } while (false);
  LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                       "Failed to apply new RTCP parameters.");
}

RTCError RtpTransportControllerAdapter::ValidateAndApplyAudioSenderParameters(
    const RtpParameters& parameters,
    uint32_t* primary_ssrc) {
  RTC_DCHECK(voice_channel_);
  RTC_DCHECK(have_audio_sender_);

  auto codecs_result = ToCricketCodecs<cricket::AudioCodec>(parameters.codecs);
  if (!codecs_result.ok()) {
    return codecs_result.MoveError();
  }

  auto extensions_result =
      ToCricketRtpHeaderExtensions(parameters.header_extensions);
  if (!extensions_result.ok()) {
    return extensions_result.MoveError();
  }

  auto stream_params_result = MakeSendStreamParamsVec(
      parameters.encodings, inner_audio_transport_->GetRtcpParameters().cname,
      local_audio_description_);
  if (!stream_params_result.ok()) {
    return stream_params_result.MoveError();
  }

  // Check that audio/video sender aren't using the same IDs to refer to
  // different things, if they share the same transport.
  if (inner_audio_transport_ == inner_video_transport_) {
    RTCError err = CheckForIdConflicts(
        codecs_result.value(), extensions_result.value(),
        stream_params_result.value(), remote_video_description_.codecs(),
        remote_video_description_.rtp_header_extensions(),
        local_video_description_.streams());
    if (!err.ok()) {
      return err;
    }
  }

  cricket::RtpTransceiverDirection local_direction =
      cricket::RtpTransceiverDirection::FromMediaContentDirection(
          local_audio_description_.direction());
  int bandwidth = cricket::kAutoBandwidth;
  if (parameters.encodings.size() == 1u) {
    if (parameters.encodings[0].max_bitrate_bps) {
      bandwidth = *parameters.encodings[0].max_bitrate_bps;
    }
    local_direction.send = parameters.encodings[0].active;
  } else {
    local_direction.send = false;
  }
  if (primary_ssrc && !stream_params_result.value().empty()) {
    *primary_ssrc = stream_params_result.value()[0].first_ssrc();
  }

  // Validation is done, so we can attempt applying the descriptions. Sent
  // codecs and header extensions go in remote description, streams go in
  // local.
  //
  // If there are no codecs or encodings, just leave the previous set of
  // codecs. The media engine doesn't like an empty set of codecs.
  if (local_audio_description_.streams().empty() &&
      remote_audio_description_.codecs().empty()) {
  } else {
    remote_audio_description_.set_codecs(codecs_result.MoveValue());
  }
  remote_audio_description_.set_rtp_header_extensions(
      extensions_result.MoveValue());
  remote_audio_description_.set_bandwidth(bandwidth);
  local_audio_description_.mutable_streams() = stream_params_result.MoveValue();
  // Direction set based on encoding "active" flag.
  local_audio_description_.set_direction(
      local_direction.ToMediaContentDirection());
  remote_audio_description_.set_direction(
      local_direction.Reversed().ToMediaContentDirection());

  // Set remote content first, to ensure the stream is created with the correct
  // codec.
  if (!voice_channel_->SetRemoteContent(&remote_audio_description_,
                                        cricket::CA_OFFER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply remote parameters to media channel.");
  }
  if (!voice_channel_->SetLocalContent(&local_audio_description_,
                                       cricket::CA_ANSWER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply local parameters to media channel.");
  }
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::ValidateAndApplyVideoSenderParameters(
    const RtpParameters& parameters,
    uint32_t* primary_ssrc) {
  RTC_DCHECK(video_channel_);
  RTC_DCHECK(have_video_sender_);

  auto codecs_result = ToCricketCodecs<cricket::VideoCodec>(parameters.codecs);
  if (!codecs_result.ok()) {
    return codecs_result.MoveError();
  }

  auto extensions_result =
      ToCricketRtpHeaderExtensions(parameters.header_extensions);
  if (!extensions_result.ok()) {
    return extensions_result.MoveError();
  }

  auto stream_params_result = MakeSendStreamParamsVec(
      parameters.encodings, inner_video_transport_->GetRtcpParameters().cname,
      local_video_description_);
  if (!stream_params_result.ok()) {
    return stream_params_result.MoveError();
  }

  // Check that audio/video sender aren't using the same IDs to refer to
  // different things, if they share the same transport.
  if (inner_audio_transport_ == inner_video_transport_) {
    RTCError err = CheckForIdConflicts(
        codecs_result.value(), extensions_result.value(),
        stream_params_result.value(), remote_audio_description_.codecs(),
        remote_audio_description_.rtp_header_extensions(),
        local_audio_description_.streams());
    if (!err.ok()) {
      return err;
    }
  }

  cricket::RtpTransceiverDirection local_direction =
      cricket::RtpTransceiverDirection::FromMediaContentDirection(
          local_video_description_.direction());
  int bandwidth = cricket::kAutoBandwidth;
  if (parameters.encodings.size() == 1u) {
    if (parameters.encodings[0].max_bitrate_bps) {
      bandwidth = *parameters.encodings[0].max_bitrate_bps;
    }
    local_direction.send = parameters.encodings[0].active;
  } else {
    local_direction.send = false;
  }
  if (primary_ssrc && !stream_params_result.value().empty()) {
    *primary_ssrc = stream_params_result.value()[0].first_ssrc();
  }

  // Validation is done, so we can attempt applying the descriptions. Sent
  // codecs and header extensions go in remote description, streams go in
  // local.
  //
  // If there are no codecs or encodings, just leave the previous set of
  // codecs. The media engine doesn't like an empty set of codecs.
  if (local_video_description_.streams().empty() &&
      remote_video_description_.codecs().empty()) {
  } else {
    remote_video_description_.set_codecs(codecs_result.MoveValue());
  }
  remote_video_description_.set_rtp_header_extensions(
      extensions_result.MoveValue());
  remote_video_description_.set_bandwidth(bandwidth);
  local_video_description_.mutable_streams() = stream_params_result.MoveValue();
  // Direction set based on encoding "active" flag.
  local_video_description_.set_direction(
      local_direction.ToMediaContentDirection());
  remote_video_description_.set_direction(
      local_direction.Reversed().ToMediaContentDirection());

  // Set remote content first, to ensure the stream is created with the correct
  // codec.
  if (!video_channel_->SetRemoteContent(&remote_video_description_,
                                        cricket::CA_OFFER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply remote parameters to media channel.");
  }
  if (!video_channel_->SetLocalContent(&local_video_description_,
                                       cricket::CA_ANSWER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply local parameters to media channel.");
  }
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::ValidateAndApplyAudioReceiverParameters(
    const RtpParameters& parameters) {
  RTC_DCHECK(voice_channel_);
  RTC_DCHECK(have_audio_receiver_);

  auto codecs_result = ToCricketCodecs<cricket::AudioCodec>(parameters.codecs);
  if (!codecs_result.ok()) {
    return codecs_result.MoveError();
  }

  auto extensions_result =
      ToCricketRtpHeaderExtensions(parameters.header_extensions);
  if (!extensions_result.ok()) {
    return extensions_result.MoveError();
  }

  cricket::RtpTransceiverDirection local_direction =
      cricket::RtpTransceiverDirection::FromMediaContentDirection(
          local_audio_description_.direction());
  auto stream_params_result = ToCricketStreamParamsVec(parameters.encodings);
  if (!stream_params_result.ok()) {
    return stream_params_result.MoveError();
  }

  // Check that audio/video receive aren't using the same IDs to refer to
  // different things, if they share the same transport.
  if (inner_audio_transport_ == inner_video_transport_) {
    RTCError err = CheckForIdConflicts(
        codecs_result.value(), extensions_result.value(),
        stream_params_result.value(), local_video_description_.codecs(),
        local_video_description_.rtp_header_extensions(),
        remote_video_description_.streams());
    if (!err.ok()) {
      return err;
    }
  }

  local_direction.recv =
      !parameters.encodings.empty() && parameters.encodings[0].active;

  // Validation is done, so we can attempt applying the descriptions. Received
  // codecs and header extensions go in local description, streams go in
  // remote.
  //
  // If there are no codecs or encodings, just leave the previous set of
  // codecs. The media engine doesn't like an empty set of codecs.
  if (remote_audio_description_.streams().empty() &&
      local_audio_description_.codecs().empty()) {
  } else {
    local_audio_description_.set_codecs(codecs_result.MoveValue());
  }
  local_audio_description_.set_rtp_header_extensions(
      extensions_result.MoveValue());
  remote_audio_description_.mutable_streams() =
      stream_params_result.MoveValue();
  // Direction set based on encoding "active" flag.
  local_audio_description_.set_direction(
      local_direction.ToMediaContentDirection());
  remote_audio_description_.set_direction(
      local_direction.Reversed().ToMediaContentDirection());

  if (!voice_channel_->SetLocalContent(&local_audio_description_,
                                       cricket::CA_OFFER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply local parameters to media channel.");
  }
  if (!voice_channel_->SetRemoteContent(&remote_audio_description_,
                                        cricket::CA_ANSWER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply remote parameters to media channel.");
  }
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::ValidateAndApplyVideoReceiverParameters(
    const RtpParameters& parameters) {
  RTC_DCHECK(video_channel_);
  RTC_DCHECK(have_video_receiver_);

  auto codecs_result = ToCricketCodecs<cricket::VideoCodec>(parameters.codecs);
  if (!codecs_result.ok()) {
    return codecs_result.MoveError();
  }

  auto extensions_result =
      ToCricketRtpHeaderExtensions(parameters.header_extensions);
  if (!extensions_result.ok()) {
    return extensions_result.MoveError();
  }

  cricket::RtpTransceiverDirection local_direction =
      cricket::RtpTransceiverDirection::FromMediaContentDirection(
          local_video_description_.direction());
  int bandwidth = cricket::kAutoBandwidth;
  auto stream_params_result = ToCricketStreamParamsVec(parameters.encodings);
  if (!stream_params_result.ok()) {
    return stream_params_result.MoveError();
  }

  // Check that audio/video receiver aren't using the same IDs to refer to
  // different things, if they share the same transport.
  if (inner_audio_transport_ == inner_video_transport_) {
    RTCError err = CheckForIdConflicts(
        codecs_result.value(), extensions_result.value(),
        stream_params_result.value(), local_audio_description_.codecs(),
        local_audio_description_.rtp_header_extensions(),
        remote_audio_description_.streams());
    if (!err.ok()) {
      return err;
    }
  }

  local_direction.recv =
      !parameters.encodings.empty() && parameters.encodings[0].active;

  // Validation is done, so we can attempt applying the descriptions. Received
  // codecs and header extensions go in local description, streams go in
  // remote.
  //
  // If there are no codecs or encodings, just leave the previous set of
  // codecs. The media engine doesn't like an empty set of codecs.
  if (remote_video_description_.streams().empty() &&
      local_video_description_.codecs().empty()) {
  } else {
    local_video_description_.set_codecs(codecs_result.MoveValue());
  }
  local_video_description_.set_rtp_header_extensions(
      extensions_result.MoveValue());
  local_video_description_.set_bandwidth(bandwidth);
  remote_video_description_.mutable_streams() =
      stream_params_result.MoveValue();
  // Direction set based on encoding "active" flag.
  local_video_description_.set_direction(
      local_direction.ToMediaContentDirection());
  remote_video_description_.set_direction(
      local_direction.Reversed().ToMediaContentDirection());

  if (!video_channel_->SetLocalContent(&local_video_description_,
                                       cricket::CA_OFFER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply local parameters to media channel.");
  }
  if (!video_channel_->SetRemoteContent(&remote_video_description_,
                                        cricket::CA_ANSWER, nullptr)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to apply remote parameters to media channel.");
  }
  return RTCError::OK();
}

RtpTransportControllerAdapter::RtpTransportControllerAdapter(
    const cricket::MediaConfig& config,
    cricket::ChannelManager* channel_manager,
    webrtc::RtcEventLog* event_log,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      media_config_(config),
      channel_manager_(channel_manager),
      event_log_(event_log) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(channel_manager_);
  // Add "dummy" codecs to the descriptions, because the media engines
  // currently reject empty lists of codecs. Note that these codecs will never
  // actually be used, because when parameters are set, the dummy codecs will
  // be replaced by actual codecs before any send/receive streams are created.
  static const cricket::AudioCodec dummy_audio(0, cricket::kPcmuCodecName, 8000,
                                               0, 1);
  static const cricket::VideoCodec dummy_video(96, cricket::kVp8CodecName);
  local_audio_description_.AddCodec(dummy_audio);
  remote_audio_description_.AddCodec(dummy_audio);
  local_video_description_.AddCodec(dummy_video);
  remote_video_description_.AddCodec(dummy_video);

  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&RtpTransportControllerAdapter::Init_w, this));
}

// TODO(nisse): Duplicates corresponding method in PeerConnection (used
// to be in MediaController).
void RtpTransportControllerAdapter::Init_w() {
  RTC_DCHECK(worker_thread_->IsCurrent());
  RTC_DCHECK(!call_);

  const int kMinBandwidthBps = 30000;
  const int kStartBandwidthBps = 300000;
  const int kMaxBandwidthBps = 2000000;

  webrtc::Call::Config call_config(event_log_);
  call_config.audio_state = channel_manager_->media_engine()->GetAudioState();
  call_config.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
  call_config.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
  call_config.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;

  call_.reset(webrtc::Call::Create(call_config));
}

void RtpTransportControllerAdapter::Close_w() {
  call_.reset();
}

RTCError RtpTransportControllerAdapter::AttachAudioSender(
    OrtcRtpSenderAdapter* sender,
    RtpTransportInterface* inner_transport) {
  if (have_audio_sender_) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using two audio RtpSenders with the same "
                         "RtpTransportControllerAdapter is not currently "
                         "supported.");
  }
  if (inner_audio_transport_ && inner_audio_transport_ != inner_transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using different transports for the audio "
                         "RtpSender and RtpReceiver is not currently "
                         "supported.");
  }
  RTCError err = MaybeSetCryptos(inner_transport, &local_audio_description_,
                                 &remote_audio_description_);
  if (!err.ok()) {
    return err;
  }
  // If setting new transport, extract its RTCP parameters and create voice
  // channel.
  if (!inner_audio_transport_) {
    CopyRtcpParametersToDescriptions(inner_transport->GetRtcpParameters(),
                                     &local_audio_description_,
                                     &remote_audio_description_);
    inner_audio_transport_ = inner_transport;
    CreateVoiceChannel();
  }
  have_audio_sender_ = true;
  sender->SignalDestroyed.connect(
      this, &RtpTransportControllerAdapter::OnAudioSenderDestroyed);
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::AttachVideoSender(
    OrtcRtpSenderAdapter* sender,
    RtpTransportInterface* inner_transport) {
  if (have_video_sender_) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using two video RtpSenders with the same "
                         "RtpTransportControllerAdapter is not currently "
                         "supported.");
  }
  if (inner_video_transport_ && inner_video_transport_ != inner_transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using different transports for the video "
                         "RtpSender and RtpReceiver is not currently "
                         "supported.");
  }
  RTCError err = MaybeSetCryptos(inner_transport, &local_video_description_,
                                 &remote_video_description_);
  if (!err.ok()) {
    return err;
  }
  // If setting new transport, extract its RTCP parameters and create video
  // channel.
  if (!inner_video_transport_) {
    CopyRtcpParametersToDescriptions(inner_transport->GetRtcpParameters(),
                                     &local_video_description_,
                                     &remote_video_description_);
    inner_video_transport_ = inner_transport;
    CreateVideoChannel();
  }
  have_video_sender_ = true;
  sender->SignalDestroyed.connect(
      this, &RtpTransportControllerAdapter::OnVideoSenderDestroyed);
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::AttachAudioReceiver(
    OrtcRtpReceiverAdapter* receiver,
    RtpTransportInterface* inner_transport) {
  if (have_audio_receiver_) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using two audio RtpReceivers with the same "
                         "RtpTransportControllerAdapter is not currently "
                         "supported.");
  }
  if (inner_audio_transport_ && inner_audio_transport_ != inner_transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using different transports for the audio "
                         "RtpReceiver and RtpReceiver is not currently "
                         "supported.");
  }
  RTCError err = MaybeSetCryptos(inner_transport, &local_audio_description_,
                                 &remote_audio_description_);
  if (!err.ok()) {
    return err;
  }
  // If setting new transport, extract its RTCP parameters and create voice
  // channel.
  if (!inner_audio_transport_) {
    CopyRtcpParametersToDescriptions(inner_transport->GetRtcpParameters(),
                                     &local_audio_description_,
                                     &remote_audio_description_);
    inner_audio_transport_ = inner_transport;
    CreateVoiceChannel();
  }
  have_audio_receiver_ = true;
  receiver->SignalDestroyed.connect(
      this, &RtpTransportControllerAdapter::OnAudioReceiverDestroyed);
  return RTCError::OK();
}

RTCError RtpTransportControllerAdapter::AttachVideoReceiver(
    OrtcRtpReceiverAdapter* receiver,
    RtpTransportInterface* inner_transport) {
  if (have_video_receiver_) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using two video RtpReceivers with the same "
                         "RtpTransportControllerAdapter is not currently "
                         "supported.");
  }
  if (inner_video_transport_ && inner_video_transport_ != inner_transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                         "Using different transports for the video "
                         "RtpReceiver and RtpReceiver is not currently "
                         "supported.");
  }
  RTCError err = MaybeSetCryptos(inner_transport, &local_video_description_,
                                 &remote_video_description_);
  if (!err.ok()) {
    return err;
  }
  // If setting new transport, extract its RTCP parameters and create video
  // channel.
  if (!inner_video_transport_) {
    CopyRtcpParametersToDescriptions(inner_transport->GetRtcpParameters(),
                                     &local_video_description_,
                                     &remote_video_description_);
    inner_video_transport_ = inner_transport;
    CreateVideoChannel();
  }
  have_video_receiver_ = true;
  receiver->SignalDestroyed.connect(
      this, &RtpTransportControllerAdapter::OnVideoReceiverDestroyed);
  return RTCError::OK();
}

void RtpTransportControllerAdapter::OnRtpTransportDestroyed(
    RtpTransportAdapter* transport) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  auto it = std::find_if(transport_proxies_.begin(), transport_proxies_.end(),
                         [transport](RtpTransportInterface* proxy) {
                           return proxy->GetInternal() == transport;
                         });
  if (it == transport_proxies_.end()) {
    RTC_NOTREACHED();
    return;
  }
  transport_proxies_.erase(it);
}

void RtpTransportControllerAdapter::OnAudioSenderDestroyed() {
  if (!have_audio_sender_) {
    RTC_NOTREACHED();
    return;
  }
  // Empty parameters should result in sending being stopped.
  RTCError err =
      ValidateAndApplyAudioSenderParameters(RtpParameters(), nullptr);
  RTC_DCHECK(err.ok());
  have_audio_sender_ = false;
  if (!have_audio_receiver_) {
    DestroyVoiceChannel();
  }
}

void RtpTransportControllerAdapter::OnVideoSenderDestroyed() {
  if (!have_video_sender_) {
    RTC_NOTREACHED();
    return;
  }
  // Empty parameters should result in sending being stopped.
  RTCError err =
      ValidateAndApplyVideoSenderParameters(RtpParameters(), nullptr);
  RTC_DCHECK(err.ok());
  have_video_sender_ = false;
  if (!have_video_receiver_) {
    DestroyVideoChannel();
  }
}

void RtpTransportControllerAdapter::OnAudioReceiverDestroyed() {
  if (!have_audio_receiver_) {
    RTC_NOTREACHED();
    return;
  }
  // Empty parameters should result in receiving being stopped.
  RTCError err = ValidateAndApplyAudioReceiverParameters(RtpParameters());
  RTC_DCHECK(err.ok());
  have_audio_receiver_ = false;
  if (!have_audio_sender_) {
    DestroyVoiceChannel();
  }
}

void RtpTransportControllerAdapter::OnVideoReceiverDestroyed() {
  if (!have_video_receiver_) {
    RTC_NOTREACHED();
    return;
  }
  // Empty parameters should result in receiving being stopped.
  RTCError err = ValidateAndApplyVideoReceiverParameters(RtpParameters());
  RTC_DCHECK(err.ok());
  have_video_receiver_ = false;
  if (!have_video_sender_) {
    DestroyVideoChannel();
  }
}

void RtpTransportControllerAdapter::CreateVoiceChannel() {
  voice_channel_ = channel_manager_->CreateVoiceChannel(
      call_.get(), media_config_,
      inner_audio_transport_->GetRtpPacketTransport()->GetInternal(),
      inner_audio_transport_->GetRtcpPacketTransport()
          ? inner_audio_transport_->GetRtcpPacketTransport()->GetInternal()
          : nullptr,
      signaling_thread_, "audio", false, cricket::AudioOptions());
  RTC_DCHECK(voice_channel_);
  voice_channel_->Enable(true);
}

void RtpTransportControllerAdapter::CreateVideoChannel() {
  video_channel_ = channel_manager_->CreateVideoChannel(
      call_.get(), media_config_,
      inner_video_transport_->GetRtpPacketTransport()->GetInternal(),
      inner_video_transport_->GetRtcpPacketTransport()
          ? inner_video_transport_->GetRtcpPacketTransport()->GetInternal()
          : nullptr,
      signaling_thread_, "video", false, cricket::VideoOptions());
  RTC_DCHECK(video_channel_);
  video_channel_->Enable(true);
}

void RtpTransportControllerAdapter::DestroyVoiceChannel() {
  RTC_DCHECK(voice_channel_);
  channel_manager_->DestroyVoiceChannel(voice_channel_);
  voice_channel_ = nullptr;
  inner_audio_transport_ = nullptr;
}

void RtpTransportControllerAdapter::DestroyVideoChannel() {
  RTC_DCHECK(video_channel_);
  channel_manager_->DestroyVideoChannel(video_channel_);
  video_channel_ = nullptr;
  inner_video_transport_ = nullptr;
}

void RtpTransportControllerAdapter::CopyRtcpParametersToDescriptions(
    const RtcpParameters& params,
    cricket::MediaContentDescription* local,
    cricket::MediaContentDescription* remote) {
  local->set_rtcp_mux(params.mux);
  remote->set_rtcp_mux(params.mux);
  local->set_rtcp_reduced_size(params.reduced_size);
  remote->set_rtcp_reduced_size(params.reduced_size);
  for (cricket::StreamParams& stream_params : local->mutable_streams()) {
    stream_params.cname = params.cname;
  }
}

uint32_t RtpTransportControllerAdapter::GenerateUnusedSsrc(
    std::set<uint32_t>* new_ssrcs) const {
  uint32_t ssrc;
  do {
    ssrc = rtc::CreateRandomNonZeroId();
  } while (
      cricket::GetStreamBySsrc(local_audio_description_.streams(), ssrc) ||
      cricket::GetStreamBySsrc(remote_audio_description_.streams(), ssrc) ||
      cricket::GetStreamBySsrc(local_video_description_.streams(), ssrc) ||
      cricket::GetStreamBySsrc(remote_video_description_.streams(), ssrc) ||
      !new_ssrcs->insert(ssrc).second);
  return ssrc;
}

RTCErrorOr<cricket::StreamParamsVec>
RtpTransportControllerAdapter::MakeSendStreamParamsVec(
    std::vector<RtpEncodingParameters> encodings,
    const std::string& cname,
    const cricket::MediaContentDescription& description) const {
  if (encodings.size() > 1u) {
    LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::UNSUPPORTED_PARAMETER,
                         "ORTC API implementation doesn't currently "
                         "support simulcast or layered encodings.");
  } else if (encodings.empty()) {
    return cricket::StreamParamsVec();
  }
  RtpEncodingParameters& encoding = encodings[0];
  std::set<uint32_t> new_ssrcs;
  if (encoding.ssrc) {
    new_ssrcs.insert(*encoding.ssrc);
  }
  if (encoding.rtx && encoding.rtx->ssrc) {
    new_ssrcs.insert(*encoding.rtx->ssrc);
  }
  // May need to fill missing SSRCs with generated ones.
  if (!encoding.ssrc) {
    if (!description.streams().empty()) {
      encoding.ssrc.emplace(description.streams()[0].first_ssrc());
    } else {
      encoding.ssrc.emplace(GenerateUnusedSsrc(&new_ssrcs));
    }
  }
  if (encoding.rtx && !encoding.rtx->ssrc) {
    uint32_t existing_rtx_ssrc;
    if (!description.streams().empty() &&
        description.streams()[0].GetFidSsrc(
            description.streams()[0].first_ssrc(), &existing_rtx_ssrc)) {
      encoding.rtx->ssrc.emplace(existing_rtx_ssrc);
    } else {
      encoding.rtx->ssrc.emplace(GenerateUnusedSsrc(&new_ssrcs));
    }
  }

  auto result = ToCricketStreamParamsVec(encodings);
  if (!result.ok()) {
    return result.MoveError();
  }
  // If conversion was successful, there should be one StreamParams.
  RTC_DCHECK_EQ(1u, result.value().size());
  result.value()[0].cname = cname;
  return result;
}

RTCError RtpTransportControllerAdapter::MaybeSetCryptos(
    RtpTransportInterface* rtp_transport,
    cricket::MediaContentDescription* local_description,
    cricket::MediaContentDescription* remote_description) {
  if (rtp_transport->GetInternal()->is_srtp_transport()) {
    if (!rtp_transport->GetInternal()->send_key() ||
        !rtp_transport->GetInternal()->receive_key()) {
      LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::UNSUPPORTED_PARAMETER,
                           "The SRTP send key or receive key is not set.")
    }
    std::vector<cricket::CryptoParams> cryptos;
    cryptos.push_back(*(rtp_transport->GetInternal()->receive_key()));
    local_description->set_cryptos(cryptos);

    cryptos.clear();
    cryptos.push_back(*(rtp_transport->GetInternal()->send_key()));
    remote_description->set_cryptos(cryptos);
  }
  return RTCError::OK();
}

}  // namespace webrtc
