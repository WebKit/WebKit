/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/ortcrtpsenderadapter.h"

#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/ortc/rtptransportadapter.h"

namespace {

void FillAudioSenderParameters(webrtc::RtpParameters* parameters) {
  for (webrtc::RtpCodecParameters& codec : parameters->codecs) {
    if (!codec.num_channels) {
      codec.num_channels = rtc::Optional<int>(1);
    }
  }
}

void FillVideoSenderParameters(webrtc::RtpParameters* parameters) {
  for (webrtc::RtpCodecParameters& codec : parameters->codecs) {
    if (!codec.clock_rate) {
      codec.clock_rate = rtc::Optional<int>(cricket::kVideoCodecClockrate);
    }
  }
}

}  // namespace

namespace webrtc {

BEGIN_OWNED_PROXY_MAP(OrtcRtpSender)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_METHOD1(RTCError, SetTrack, MediaStreamTrackInterface*)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<MediaStreamTrackInterface>, GetTrack)
PROXY_METHOD1(RTCError, SetTransport, RtpTransportInterface*)
PROXY_CONSTMETHOD0(RtpTransportInterface*, GetTransport)
PROXY_METHOD1(RTCError, Send, const RtpParameters&)
PROXY_CONSTMETHOD0(RtpParameters, GetParameters)
PROXY_CONSTMETHOD0(cricket::MediaType, GetKind)
END_PROXY_MAP()

// static
std::unique_ptr<OrtcRtpSenderInterface> OrtcRtpSenderAdapter::CreateProxy(
    std::unique_ptr<OrtcRtpSenderAdapter> wrapped_sender) {
  RTC_DCHECK(wrapped_sender);
  rtc::Thread* signaling =
      wrapped_sender->rtp_transport_controller_->signaling_thread();
  rtc::Thread* worker =
      wrapped_sender->rtp_transport_controller_->worker_thread();
  return OrtcRtpSenderProxy::Create(signaling, worker,
                                    std::move(wrapped_sender));
}

OrtcRtpSenderAdapter::~OrtcRtpSenderAdapter() {
  internal_sender_ = nullptr;
  SignalDestroyed();
}

RTCError OrtcRtpSenderAdapter::SetTrack(MediaStreamTrackInterface* track) {
  if (track && cricket::MediaTypeFromString(track->kind()) != kind_) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Track kind (audio/video) doesn't match the kind of this sender.");
  }
  if (internal_sender_ && !internal_sender_->SetTrack(track)) {
    // Since we checked the track type above, this should never happen...
    RTC_NOTREACHED();
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to set track on RtpSender.");
  }
  track_ = track;
  return RTCError::OK();
}

rtc::scoped_refptr<MediaStreamTrackInterface> OrtcRtpSenderAdapter::GetTrack()
    const {
  return track_;
}

RTCError OrtcRtpSenderAdapter::SetTransport(RtpTransportInterface* transport) {
  LOG_AND_RETURN_ERROR(
      RTCErrorType::UNSUPPORTED_OPERATION,
      "Changing the transport of an RtpSender is not yet supported.");
}

RtpTransportInterface* OrtcRtpSenderAdapter::GetTransport() const {
  return transport_;
}

RTCError OrtcRtpSenderAdapter::Send(const RtpParameters& parameters) {
  RtpParameters filled_parameters = parameters;
  RTCError err;
  uint32_t ssrc = 0;
  switch (kind_) {
    case cricket::MEDIA_TYPE_AUDIO:
      FillAudioSenderParameters(&filled_parameters);
      err = rtp_transport_controller_->ValidateAndApplyAudioSenderParameters(
          filled_parameters, &ssrc);
      if (!err.ok()) {
        return err;
      }
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      FillVideoSenderParameters(&filled_parameters);
      err = rtp_transport_controller_->ValidateAndApplyVideoSenderParameters(
          filled_parameters, &ssrc);
      if (!err.ok()) {
        return err;
      }
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
      return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }
  last_applied_parameters_ = filled_parameters;

  // Now that parameters were applied, can call SetSsrc on the internal sender.
  // This is analogous to a PeerConnection calling SetSsrc after
  // SetLocalDescription is successful.
  //
  // If there were no encodings, this SSRC may be 0, which is valid.
  if (!internal_sender_) {
    CreateInternalSender();
  }
  internal_sender_->SetSsrc(ssrc);

  return RTCError::OK();
}

RtpParameters OrtcRtpSenderAdapter::GetParameters() const {
  return last_applied_parameters_;
}

cricket::MediaType OrtcRtpSenderAdapter::GetKind() const {
  return kind_;
}

OrtcRtpSenderAdapter::OrtcRtpSenderAdapter(
    cricket::MediaType kind,
    RtpTransportInterface* transport,
    RtpTransportControllerAdapter* rtp_transport_controller)
    : kind_(kind),
      transport_(transport),
      rtp_transport_controller_(rtp_transport_controller) {}

void OrtcRtpSenderAdapter::CreateInternalSender() {
  switch (kind_) {
    case cricket::MEDIA_TYPE_AUDIO:
      internal_sender_ = new AudioRtpSender(
          rtp_transport_controller_->voice_channel(), nullptr);
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      internal_sender_ =
          new VideoRtpSender(rtp_transport_controller_->video_channel());
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
  }
  if (track_) {
    if (!internal_sender_->SetTrack(track_)) {
      // Since we checked the track type when it was set, this should never
      // happen...
      RTC_NOTREACHED();
    }
  }
}

}  // namespace webrtc
