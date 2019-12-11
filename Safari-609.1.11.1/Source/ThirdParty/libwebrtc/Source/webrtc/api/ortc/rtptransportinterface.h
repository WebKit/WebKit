/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ORTC_RTPTRANSPORTINTERFACE_H_
#define API_ORTC_RTPTRANSPORTINTERFACE_H_

#include <string>

#include "absl/types/optional.h"
#include "api/ortc/packettransportinterface.h"
#include "api/rtcerror.h"
#include "api/rtp_headers.h"
#include "api/rtpparameters.h"

namespace webrtc {

class RtpTransportAdapter;

struct RtpTransportParameters final {
  RtcpParameters rtcp;

  // Enabled periodic sending of keep-alive packets, that help prevent timeouts
  // on the network level, such as NAT bindings. See RFC6263 section 4.6.
  RtpKeepAliveConfig keepalive;

  bool operator==(const RtpTransportParameters& o) const {
    return rtcp == o.rtcp && keepalive == o.keepalive;
  }
  bool operator!=(const RtpTransportParameters& o) const {
    return !(*this == o);
  }
};

// Base class for different types of RTP transports that can be created by an
// OrtcFactory. Used by RtpSenders/RtpReceivers.
//
// This is not present in the standard ORTC API, but exists here for a few
// reasons. Firstly, it allows different types of RTP transports to be used:
// DTLS-SRTP (which is required for the web), but also SDES-SRTP and
// unencrypted RTP. It also simplifies the handling of RTCP muxing, and
// provides a better API point for it.
//
// Note that Edge's implementation of ORTC provides a similar API point, called
// RTCSrtpSdesTransport:
// https://msdn.microsoft.com/en-us/library/mt502527(v=vs.85).aspx
class RtpTransportInterface {
 public:
  virtual ~RtpTransportInterface() {}

  // Returns packet transport that's used to send RTP packets.
  virtual PacketTransportInterface* GetRtpPacketTransport() const = 0;

  // Returns separate packet transport that's used to send RTCP packets. If
  // RTCP multiplexing is being used, returns null.
  virtual PacketTransportInterface* GetRtcpPacketTransport() const = 0;

  // Set/get RTP/RTCP transport params. Can be used to enable RTCP muxing or
  // reduced-size RTCP if initially not enabled.
  //
  // Changing |mux| from "true" to "false" is not allowed, and changing the
  // CNAME is currently unsupported.
  // RTP keep-alive settings need to be set before before an RtpSender has
  // started sending, altering the payload type or timeout interval after this
  // point is not supported. The parameters must also match across all RTP
  // transports for a given RTP transport controller.
  virtual RTCError SetParameters(const RtpTransportParameters& parameters) = 0;
  // Returns last set or constructed-with parameters. If |cname| was empty in
  // construction, the generated CNAME will be present in the returned
  // parameters (see above).
  virtual RtpTransportParameters GetParameters() const = 0;

 protected:
  // Only for internal use. Returns a pointer to an internal interface, for use
  // by the implementation.
  virtual RtpTransportAdapter* GetInternal() = 0;

  // Classes that can use this internal interface.
  friend class OrtcFactory;
  friend class OrtcRtpSenderAdapter;
  friend class OrtcRtpReceiverAdapter;
  friend class RtpTransportControllerAdapter;
  friend class RtpTransportAdapter;
};

}  // namespace webrtc

#endif  // API_ORTC_RTPTRANSPORTINTERFACE_H_
