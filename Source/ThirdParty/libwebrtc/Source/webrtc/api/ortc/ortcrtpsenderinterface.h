/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpSenders:
// http://publications.ortc.org/2016/20161202/#rtcrtpsender*
//
// However, underneath the RtpSender is an RtpTransport, rather than a
// DtlsTransport. This is to allow different types of RTP transports (besides
// DTLS-SRTP) to be used.

#ifndef WEBRTC_API_ORTC_ORTCRTPSENDERINTERFACE_H_
#define WEBRTC_API_ORTC_ORTCRTPSENDERINTERFACE_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediatypes.h"
#include "webrtc/api/ortc/rtptransportinterface.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/api/rtpparameters.h"

namespace webrtc {

// Note: Since sender capabilities may depend on how the OrtcFactory was
// created, instead of a static "GetCapabilities" method on this interface,
// there is a "GetRtpSenderCapabilities" method on the OrtcFactory.
class OrtcRtpSenderInterface {
 public:
  virtual ~OrtcRtpSenderInterface() {}

  // Sets the source of media that will be sent by this sender.
  //
  // If Send has already been called, will immediately switch to sending this
  // track. If |track| is null, will stop sending media.
  //
  // Returns INVALID_PARAMETER error if an audio track is set on a video
  // RtpSender, or vice-versa.
  virtual RTCError SetTrack(MediaStreamTrackInterface* track) = 0;
  // Returns previously set (or constructed-with) track.
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> GetTrack() const = 0;

  // Once supported, will switch to sending media on a new transport. However,
  // this is not currently supported and will always return an error.
  virtual RTCError SetTransport(RtpTransportInterface* transport) = 0;
  // Returns previously set (or constructed-with) transport.
  virtual RtpTransportInterface* GetTransport() const = 0;

  // Start sending media with |parameters| (if |parameters| contains an active
  // encoding).
  //
  // There are no limitations to how the parameters can be changed after the
  // initial call to Send, as long as they're valid (for example, they can't
  // use the same payload type for two codecs).
  virtual RTCError Send(const RtpParameters& parameters) = 0;
  // Returns parameters that were last successfully passed into Send, or empty
  // parameters if that hasn't yet occurred.
  //
  // Note that for parameters that are described as having an "implementation
  // default" value chosen, GetParameters() will return those chosen defaults,
  // with the exception of SSRCs which have special behavior. See
  // rtpparameters.h for more details.
  virtual RtpParameters GetParameters() const = 0;

  // Audio or video sender?
  virtual cricket::MediaType GetKind() const = 0;

  // TODO(deadbeef): SSRC conflict signal.
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTC_ORTCRTPSENDERINTERFACE_H_
