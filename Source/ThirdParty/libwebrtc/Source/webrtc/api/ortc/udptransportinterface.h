/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ORTC_UDPTRANSPORTINTERFACE_H_
#define API_ORTC_UDPTRANSPORTINTERFACE_H_

#include "api/ortc/packettransportinterface.h"
#include "api/proxy.h"
#include "rtc_base/socketaddress.h"

namespace webrtc {

// Interface for a raw UDP transport (not using ICE), meaning a combination of
// a local/remote IP address/port.
//
// An instance can be instantiated using OrtcFactory.
//
// Each instance reserves a UDP port, which will be freed when the
// UdpTransportInterface destructor is called.
//
// Calling SetRemoteAddress sets the destination of outgoing packets; without a
// destination, packets can't be sent, but they can be received.
class UdpTransportInterface : public virtual PacketTransportInterface {
 public:
  // Get the address of the socket allocated for this transport.
  virtual rtc::SocketAddress GetLocalAddress() const = 0;

  // Sets the address to which packets will be delivered.
  //
  // Calling with a "nil" (default-constructed) address is legal, and unsets
  // any previously set destination.
  //
  // However, calling with an incomplete address (port or IP not set) will
  // fail.
  virtual bool SetRemoteAddress(const rtc::SocketAddress& dest) = 0;
  // Simple getter. If never set, returns nil address.
  virtual rtc::SocketAddress GetRemoteAddress() const = 0;
};

}  // namespace webrtc

#endif  // API_ORTC_UDPTRANSPORTINTERFACE_H_
