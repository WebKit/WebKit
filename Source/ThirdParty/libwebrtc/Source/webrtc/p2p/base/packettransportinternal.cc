/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/packettransportinternal.h"

namespace rtc {

PacketTransportInternal::PacketTransportInternal() = default;

PacketTransportInternal::~PacketTransportInternal() = default;

PacketTransportInternal* PacketTransportInternal::GetInternal() {
  return this;
}

bool PacketTransportInternal::GetOption(rtc::Socket::Option opt, int* value) {
  return false;
}

rtc::Optional<NetworkRoute> PacketTransportInternal::network_route() const {
  return rtc::Optional<NetworkRoute>();
}

}  // namespace rtc
