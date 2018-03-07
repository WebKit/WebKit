/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ORTC_RTPTRANSPORTCONTROLLERINTERFACE_H_
#define API_ORTC_RTPTRANSPORTCONTROLLERINTERFACE_H_

#include <vector>

#include "api/ortc/rtptransportinterface.h"

namespace webrtc {

class RtpTransportControllerAdapter;

// Used to group RTP transports between a local endpoint and the same remote
// endpoint, for the purpose of sharing bandwidth estimation and other things.
//
// Comparing this to the PeerConnection model, non-budled audio/video would use
// two RtpTransports with a single RtpTransportController, whereas bundled
// media would use a single RtpTransport, and two PeerConnections would use
// independent RtpTransportControllers.
//
// RtpTransports are associated with this controller when they're created, by
// passing the controller into OrtcFactory's relevant "CreateRtpTransport"
// method. When a transport is destroyed, it's automatically disassociated.
// GetTransports returns all currently associated transports.
//
// This is the RTP equivalent of "IceTransportController" in ORTC; RtpTransport
// is to RtpTransportController as IceTransport is to IceTransportController.
class RtpTransportControllerInterface {
 public:
  virtual ~RtpTransportControllerInterface() {}

  // Returns all transports associated with this controller (see explanation
  // above). No ordering is guaranteed.
  virtual std::vector<RtpTransportInterface*> GetTransports() const = 0;

 protected:
  // Only for internal use. Returns a pointer to an internal interface, for use
  // by the implementation.
  virtual RtpTransportControllerAdapter* GetInternal() = 0;

  // Classes that can use this internal interface.
  friend class OrtcFactory;
  friend class RtpTransportAdapter;
};

}  // namespace webrtc

#endif  // API_ORTC_RTPTRANSPORTCONTROLLERINTERFACE_H_
