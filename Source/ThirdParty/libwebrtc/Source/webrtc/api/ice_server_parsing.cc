/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/ice_server_parsing.h"

#include <set>
#include <vector>

#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "p2p/base/port_allocator.h"
#include "pc/ice_server_parsing.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

RTCError ParseIceServers(const PeerConnectionInterface::IceServers& servers,
                         std::set<SocketAddress>* stun_servers,
                         std::vector<RelayServerConfig>* turn_servers) {
  return ParseIceServersOrError(servers, stun_servers, turn_servers);
}

}  // namespace webrtc
