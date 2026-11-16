/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_BASIC_PACKET_SOCKET_FACTORY_H_
#define P2P_BASE_BASIC_PACKET_SOCKET_FACTORY_H_

#include <cstdint>
#include <memory>

#include "api/async_dns_resolver.h"
#include "api/environment/environment.h"
#include "api/packet_socket_factory.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT BasicPacketSocketFactory : public PacketSocketFactory {
 public:
  explicit BasicPacketSocketFactory(SocketFactory* socket_factory);
  ~BasicPacketSocketFactory() override;

  std::unique_ptr<AsyncPacketSocket> CreateUdpSocket(
      const Environment& env,
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  std::unique_ptr<AsyncListenSocket> CreateServerTcpSocket(
      const Environment& env,
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  std::unique_ptr<AsyncPacketSocket> CreateClientTcpSocket(
      const Environment& env,
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const PacketSocketTcpOptions& tcp_options) override;

  std::unique_ptr<AsyncDnsResolverInterface> CreateAsyncDnsResolver() override;

  std::unique_ptr<AsyncPacketSocket> CreateClientUdpSocket(
      const Environment& env,
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      uint16_t min_port,
      uint16_t max_port,
      const PacketSocketTcpOptions& options) override;

 private:
  int BindSocket(Socket& socket,
                 const SocketAddress& local_address,
                 uint16_t min_port,
                 uint16_t max_port);

  SocketFactory* socket_factory_;
};

}  //  namespace webrtc


#endif  // P2P_BASE_BASIC_PACKET_SOCKET_FACTORY_H_
