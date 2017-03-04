/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/ortcrtpreceiveradapter.h"

#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"  // For "CreateRandomX".
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/ortc/rtptransportadapter.h"

namespace {

void FillAudioReceiverParameters(webrtc::RtpParameters* parameters) {
  for (webrtc::RtpCodecParameters& codec : parameters->codecs) {
    if (!codec.num_channels) {
      codec.num_channels = rtc::Optional<int>(1);
    }
  }
}

void FillVideoReceiverParameters(webrtc::RtpParameters* parameters) {
  for (webrtc::RtpCodecParameters& codec : parameters->codecs) {
    if (!codec.clock_rate) {
      codec.clock_rate = rtc::Optional<int>(cricket::kVideoCodecClockrate);
    }
  }
}

}  // namespace

namespace webrtc {

BEGIN_OWNED_PROXY_MAP(OrtcRtpReceiver)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(rtc::scoped_refptr<MediaStreamTrackInterface>, GetTrack)
PROXY_METHOD1(RTCError, SetTransport, RtpTransportInterface*)
PROXY_CONSTMETHOD0(RtpTransportInterface*, GetTransport)
PROXY_METHOD1(RTCError, Receive, const RtpParameters&)
PROXY_CONSTMETHOD0(RtpParameters, GetParameters)
PROXY_CONSTMETHOD0(cricket::MediaType, GetKind)
END_PROXY_MAP()

// static
std::unique_ptr<OrtcRtpReceiverInterface> OrtcRtpReceiverAdapter::CreateProxy(
    std::unique_ptr<OrtcRtpReceiverAdapter> wrapped_receiver) {
  RTC_DCHECK(wrapped_receiver);
  rtc::Thread* signaling =
      wrapped_receiver->rtp_transport_controller_->signaling_thread();
  rtc::Thread* worker =
      wrapped_receiver->rtp_transport_controller_->worker_thread();
  return OrtcRtpReceiverProxy::Create(signaling, worker,
                                      std::move(wrapped_receiver));
}

OrtcRtpReceiverAdapter::~OrtcRtpReceiverAdapter() {
  internal_receiver_ = nullptr;
  SignalDestroyed();
}

rtc::scoped_refptr<MediaStreamTrackInterface> OrtcRtpReceiverAdapter::GetTrack()
    const {
  return internal_receiver_ ? internal_receiver_->track() : nullptr;
}

RTCError OrtcRtpReceiverAdapter::SetTransport(
    RtpTransportInterface* transport) {
  LOG_AND_RETURN_ERROR(
      RTCErrorType::UNSUPPORTED_OPERATION,
      "Changing the transport of an RtpReceiver is not yet supported.");
}

RtpTransportInterface* OrtcRtpReceiverAdapter::GetTransport() const {
  return transport_;
}

RTCError OrtcRtpReceiverAdapter::Receive(const RtpParameters& parameters) {
  RtpParameters filled_parameters = parameters;
  RTCError err;
  switch (kind_) {
    case cricket::MEDIA_TYPE_AUDIO:
      FillAudioReceiverParameters(&filled_parameters);
      err = rtp_transport_controller_->ValidateAndApplyAudioReceiverParameters(
          filled_parameters);
      if (!err.ok()) {
        return err;
      }
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      FillVideoReceiverParameters(&filled_parameters);
      err = rtp_transport_controller_->ValidateAndApplyVideoReceiverParameters(
          filled_parameters);
      if (!err.ok()) {
        return err;
      }
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
      return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }
  last_applied_parameters_ = filled_parameters;

  // Now that parameters were applied, can create (or recreate) the internal
  // receiver.
  //
  // This is analogous to a PeerConnection creating a receiver after
  // SetRemoteDescription is successful.
  MaybeRecreateInternalReceiver();
  return RTCError::OK();
}

RtpParameters OrtcRtpReceiverAdapter::GetParameters() const {
  return last_applied_parameters_;
}

cricket::MediaType OrtcRtpReceiverAdapter::GetKind() const {
  return kind_;
}

OrtcRtpReceiverAdapter::OrtcRtpReceiverAdapter(
    cricket::MediaType kind,
    RtpTransportInterface* transport,
    RtpTransportControllerAdapter* rtp_transport_controller)
    : kind_(kind),
      transport_(transport),
      rtp_transport_controller_(rtp_transport_controller) {}

void OrtcRtpReceiverAdapter::MaybeRecreateInternalReceiver() {
  if (last_applied_parameters_.encodings.empty()) {
    internal_receiver_ = nullptr;
    return;
  }
  // An SSRC of 0 is valid; this is used to identify "the default SSRC" (which
  // is the first one seen by the underlying media engine).
  uint32_t ssrc = 0;
  if (last_applied_parameters_.encodings[0].ssrc) {
    ssrc = *last_applied_parameters_.encodings[0].ssrc;
  }
  if (internal_receiver_ && ssrc == internal_receiver_->ssrc()) {
    // SSRC not changing; nothing to do.
    return;
  }
  internal_receiver_ = nullptr;
  switch (kind_) {
    case cricket::MEDIA_TYPE_AUDIO:
      internal_receiver_ =
          new AudioRtpReceiver(rtc::CreateRandomUuid(), ssrc,
                               rtp_transport_controller_->voice_channel());
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      internal_receiver_ = new VideoRtpReceiver(
          rtc::CreateRandomUuid(), rtp_transport_controller_->worker_thread(),
          ssrc, rtp_transport_controller_->video_channel());
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
  }
}

}  // namespace webrtc
