/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpReceivers:
// http://publications.ortc.org/2016/20161202/#rtcrtpreceiver*
//
// However, underneath the RtpReceiver is an RtpTransport, rather than a
// DtlsTransport. This is to allow different types of RTP transports (besides
// DTLS-SRTP) to be used.

#ifndef WEBRTC_API_ORTC_ORTCRTPRECEIVERINTERFACE_H_
#define WEBRTC_API_ORTC_ORTCRTPRECEIVERINTERFACE_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediatypes.h"
#include "webrtc/api/ortc/rtptransportinterface.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/api/rtpparameters.h"

namespace webrtc {

// Note: Since receiver capabilities may depend on how the OrtcFactory was
// created, instead of a static "GetCapabilities" method on this interface,
// there is a "GetRtpReceiverCapabilities" method on the OrtcFactory.
class OrtcRtpReceiverInterface {
 public:
  virtual ~OrtcRtpReceiverInterface() {}

  // Returns a track representing the media received by this receiver.
  //
  // Currently, this will return null until Receive has been successfully
  // called. Also, a new track will be created every time the primary SSRC
  // changes.
  //
  // If encodings are removed, GetTrack will return null. Though deactivating
  // an encoding (setting |active| to false) will not do this.
  //
  // In the future, these limitations will be fixed, and GetTrack will return
  // the same track for the lifetime of the RtpReceiver. So it's not
  // recommended to write code that depends on this non-standard behavior.
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> GetTrack() const = 0;

  // Once supported, will switch to receiving media on a new transport.
  // However, this is not currently supported and will always return an error.
  virtual RTCError SetTransport(RtpTransportInterface* transport) = 0;
  // Returns previously set (or constructed-with) transport.
  virtual RtpTransportInterface* GetTransport() const = 0;

  // Start receiving media with |parameters| (if |parameters| contains an
  // active encoding).
  //
  // There are no limitations to how the parameters can be changed after the
  // initial call to Receive, as long as they're valid (for example, they can't
  // use the same payload type for two codecs).
  virtual RTCError Receive(const RtpParameters& parameters) = 0;
  // Returns parameters that were last successfully passed into Receive, or
  // empty parameters if that hasn't yet occurred.
  //
  // Note that for parameters that are described as having an "implementation
  // default" value chosen, GetParameters() will return those chosen defaults,
  // with the exception of SSRCs which have special behavior. See
  // rtpparameters.h for more details.
  virtual RtpParameters GetParameters() const = 0;

  // Audio or video receiver?
  //
  // Once GetTrack() starts always returning a track, this method will be
  // redundant, as one can call "GetTrack()->kind()". However, it's still a
  // nice convenience, and is symmetric with OrtcRtpSenderInterface::GetKind.
  virtual cricket::MediaType GetKind() const = 0;

  // TODO(deadbeef): GetContributingSources
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTC_ORTCRTPRECEIVERINTERFACE_H_
