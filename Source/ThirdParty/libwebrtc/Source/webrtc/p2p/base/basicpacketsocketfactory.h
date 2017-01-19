/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_BASICPACKETSOCKETFACTORY_H_
#define WEBRTC_P2P_BASE_BASICPACKETSOCKETFACTORY_H_

#include "webrtc/p2p/base/packetsocketfactory.h"

namespace rtc {

class AsyncSocket;
class SocketFactory;
class Thread;

class BasicPacketSocketFactory : public PacketSocketFactory {
 public:
  BasicPacketSocketFactory();
  explicit BasicPacketSocketFactory(Thread* thread);
  explicit BasicPacketSocketFactory(SocketFactory* socket_factory);
  ~BasicPacketSocketFactory() override;

  AsyncPacketSocket* CreateUdpSocket(const SocketAddress& local_address,
                                     uint16_t min_port,
                                     uint16_t max_port) override;
  AsyncPacketSocket* CreateServerTcpSocket(const SocketAddress& local_address,
                                           uint16_t min_port,
                                           uint16_t max_port,
                                           int opts) override;
  AsyncPacketSocket* CreateClientTcpSocket(const SocketAddress& local_address,
                                           const SocketAddress& remote_address,
                                           const ProxyInfo& proxy_info,
                                           const std::string& user_agent,
                                           int opts) override;

  AsyncResolverInterface* CreateAsyncResolver() override;

 private:
  int BindSocket(AsyncSocket* socket,
                 const SocketAddress& local_address,
                 uint16_t min_port,
                 uint16_t max_port);

  SocketFactory* socket_factory();

  Thread* thread_;
  SocketFactory* socket_factory_;
};

}  // namespace rtc

#endif  // WEBRTC_P2P_BASE_BASICPACKETSOCKETFACTORY_H_
