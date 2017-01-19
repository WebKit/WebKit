/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_P2PTRANSPORT_H_
#define WEBRTC_P2P_BASE_P2PTRANSPORT_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/p2p/base/transport.h"

namespace cricket {

// Everything in this class should be called on the network thread.
class P2PTransport : public Transport {
 public:
  P2PTransport(const std::string& name, PortAllocator* allocator);
  virtual ~P2PTransport();

 protected:
  // Creates and destroys P2PTransportChannel.
  virtual TransportChannelImpl* CreateTransportChannel(int component);
  virtual void DestroyTransportChannel(TransportChannelImpl* channel);

  friend class P2PTransportChannel;

  RTC_DISALLOW_COPY_AND_ASSIGN(P2PTransport);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_P2PTRANSPORT_H_
