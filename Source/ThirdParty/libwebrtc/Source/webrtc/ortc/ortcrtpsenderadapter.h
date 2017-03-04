/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_ORTC_ORTCRTPSENDERADAPTER_H_
#define WEBRTC_ORTC_ORTCRTPSENDERADAPTER_H_

#include <memory>

#include "webrtc/api/ortc/ortcrtpsenderinterface.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/api/rtpparameters.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/ortc/rtptransportcontrolleradapter.h"
#include "webrtc/pc/rtpsender.h"

namespace webrtc {

// Implementation of OrtcRtpSenderInterface that works with RtpTransportAdapter,
// and wraps a VideoRtpSender/AudioRtpSender that's normally used with the
// PeerConnection.
//
// TODO(deadbeef): When BaseChannel is split apart into separate
// "RtpSender"/"RtpTransceiver"/"RtpSender"/"RtpReceiver" objects, this adapter
// object can be removed.
class OrtcRtpSenderAdapter : public OrtcRtpSenderInterface {
 public:
  // Wraps |wrapped_sender| in a proxy that will safely call methods on the
  // correct thread.
  static std::unique_ptr<OrtcRtpSenderInterface> CreateProxy(
      std::unique_ptr<OrtcRtpSenderAdapter> wrapped_sender);

  // Should only be called by RtpTransportControllerAdapter.
  OrtcRtpSenderAdapter(cricket::MediaType kind,
                       RtpTransportInterface* transport,
                       RtpTransportControllerAdapter* rtp_transport_controller);
  ~OrtcRtpSenderAdapter() override;

  // OrtcRtpSenderInterface implementation.
  RTCError SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> GetTrack() const override;

  RTCError SetTransport(RtpTransportInterface* transport) override;
  RtpTransportInterface* GetTransport() const override;

  RTCError Send(const RtpParameters& parameters) override;
  RtpParameters GetParameters() const override;

  cricket::MediaType GetKind() const override;

  // Used so that the RtpTransportControllerAdapter knows when it can
  // deallocate resources allocated for this object.
  sigslot::signal0<> SignalDestroyed;

 private:
  void CreateInternalSender();

  cricket::MediaType kind_;
  RtpTransportInterface* transport_;
  RtpTransportControllerAdapter* rtp_transport_controller_;
  // Scoped refptr due to ref-counted interface, but we should be the only
  // reference holder.
  rtc::scoped_refptr<RtpSenderInternal> internal_sender_;
  rtc::scoped_refptr<MediaStreamTrackInterface> track_;
  RtpParameters last_applied_parameters_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(OrtcRtpSenderAdapter);
};

}  // namespace webrtc

#endif  // WEBRTC_ORTC_ORTCRTPSENDERADAPTER_H_
