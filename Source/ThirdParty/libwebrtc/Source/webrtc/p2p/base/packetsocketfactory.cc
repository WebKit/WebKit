/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "p2p/base/packetsocketfactory.h"

namespace rtc {

PacketSocketTcpOptions::PacketSocketTcpOptions() = default;

PacketSocketTcpOptions::~PacketSocketTcpOptions() = default;

AsyncPacketSocket* PacketSocketFactory::CreateClientTcpSocket(
    const SocketAddress& local_address,
    const SocketAddress& remote_address,
    const ProxyInfo& proxy_info,
    const std::string& user_agent,
    const PacketSocketTcpOptions& tcp_options) {
  return CreateClientTcpSocket(local_address, remote_address, proxy_info,
                               user_agent, tcp_options.opts);
}

}  // namespace rtc
