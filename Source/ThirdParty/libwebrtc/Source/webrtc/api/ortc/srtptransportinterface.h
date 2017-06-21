/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ORTC_SRTPTRANSPORTINTERFACE_H_
#define WEBRTC_API_ORTC_SRTPTRANSPORTINTERFACE_H_

#include "webrtc/api/ortc/rtptransportinterface.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/media/base/cryptoparams.h"

namespace webrtc {

// The subclass of the RtpTransport which uses SRTP. The keying information
// is explicitly passed in from the application.
//
// If using SDP and SDES (RFC4568) for signaling, then after applying the
// answer, the negotiated keying information from the offer and answer would be
// set and the SRTP would be active.
//
// Note that Edge's implementation of ORTC provides a similar API point, called
// RTCSrtpSdesTransport:
// https://msdn.microsoft.com/en-us/library/mt502527(v=vs.85).aspx
class SrtpTransportInterface : public RtpTransportInterface {
 public:
  virtual ~SrtpTransportInterface() {}

  // There are some limitations of the current implementation:
  //  1. Send and receive keys must use the same crypto suite.
  //  2. The keys can't be changed after initially set.
  //  3. The keys must be set before creating a sender/receiver using the SRTP
  //     transport.
  // Set the SRTP keying material for sending RTP and RTCP.
  virtual RTCError SetSrtpSendKey(const cricket::CryptoParams& params) = 0;

  // Set the SRTP keying material for receiving RTP and RTCP.
  virtual RTCError SetSrtpReceiveKey(const cricket::CryptoParams& params) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTC_SRTPTRANSPORTINTERFACE_H_
