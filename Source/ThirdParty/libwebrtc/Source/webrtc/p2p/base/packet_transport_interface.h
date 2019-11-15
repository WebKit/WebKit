/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This exists for backwards compatibility with chromium remoting code that
// uses it.
// TODO(deadbeef): Update chromium and remove this file.

#ifndef P2P_BASE_PACKET_TRANSPORT_INTERFACE_H_
#define P2P_BASE_PACKET_TRANSPORT_INTERFACE_H_

#include "p2p/base/packet_transport_internal.h"

namespace rtc {
typedef PacketTransportInternal PacketTransportInterface;
}

#endif  // P2P_BASE_PACKET_TRANSPORT_INTERFACE_H_
