/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ORTC_ORTCRTPRECEIVERADAPTER_H_
#define ORTC_ORTCRTPRECEIVERADAPTER_H_

#include <memory>

#include "api/ortc/ortcrtpreceiverinterface.h"
#include "api/rtcerror.h"
#include "api/rtpparameters.h"
#include "ortc/rtptransportcontrolleradapter.h"
#include "pc/rtpreceiver.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Implementation of OrtcRtpReceiverInterface that works with
// RtpTransportAdapter, and wraps a VideoRtpReceiver/AudioRtpReceiver that's
// normally used with the PeerConnection.
//
// TODO(deadbeef): When BaseChannel is split apart into separate
// "RtpReceiver"/"RtpTransceiver"/"RtpReceiver"/"RtpReceiver" objects, this
// adapter object can be removed.
class OrtcRtpReceiverAdapter : public OrtcRtpReceiverInterface {
 public:
  // Wraps |wrapped_receiver| in a proxy that will safely call methods on the
  // correct thread.
  static std::unique_ptr<OrtcRtpReceiverInterface> CreateProxy(
      std::unique_ptr<OrtcRtpReceiverAdapter> wrapped_receiver);

  // Should only be called by RtpTransportControllerAdapter.
  OrtcRtpReceiverAdapter(
      cricket::MediaType kind,
      RtpTransportInterface* transport,
      RtpTransportControllerAdapter* rtp_transport_controller);
  ~OrtcRtpReceiverAdapter() override;

  // OrtcRtpReceiverInterface implementation.
  rtc::scoped_refptr<MediaStreamTrackInterface> GetTrack() const override;

  RTCError SetTransport(RtpTransportInterface* transport) override;
  RtpTransportInterface* GetTransport() const override;

  RTCError Receive(const RtpParameters& parameters) override;
  RtpParameters GetParameters() const override;

  cricket::MediaType GetKind() const override;

  // Used so that the RtpTransportControllerAdapter knows when it can
  // deallocate resources allocated for this object.
  sigslot::signal0<> SignalDestroyed;

 private:
  void MaybeRecreateInternalReceiver();

  cricket::MediaType kind_;
  RtpTransportInterface* transport_;
  RtpTransportControllerAdapter* rtp_transport_controller_;
  // Scoped refptr due to ref-counted interface, but we should be the only
  // reference holder.
  rtc::scoped_refptr<RtpReceiverInternal> internal_receiver_;
  RtpParameters last_applied_parameters_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(OrtcRtpReceiverAdapter);
};

}  // namespace webrtc

#endif  // ORTC_ORTCRTPRECEIVERADAPTER_H_
