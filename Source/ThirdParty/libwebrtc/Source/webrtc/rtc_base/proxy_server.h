/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PROXY_SERVER_H_
#define RTC_BASE_PROXY_SERVER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/memory/fifo_buffer.h"
#include "rtc_base/server_socket_adapters.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"

namespace webrtc {

// ProxyServer is a base class that allows for easy construction of proxy
// servers. With its helper class ProxyBinding, it contains all the necessary
// logic for receiving and bridging connections. The specific client-server
// proxy protocol is implemented by an instance of the AsyncProxyServerSocket
// class; children of ProxyServer implement WrapSocket appropriately to return
// the correct protocol handler.

class ProxyBinding {
 public:
  ProxyBinding(AsyncProxyServerSocket* in_socket, Socket* out_socket);
  virtual ~ProxyBinding();

  ProxyBinding(const ProxyBinding&) = delete;
  ProxyBinding& operator=(const ProxyBinding&) = delete;

  [[deprecated]] void SubscribeDestroyed(
      absl::AnyInvocable<void(ProxyBinding* proxy)> callback) {
    destroyed_callbacks_.AddReceiver(std::move(callback));
  }
  void SubscribeDestroyed(
      void* tag,
      absl::AnyInvocable<void(ProxyBinding* proxy)> callback) {
    destroyed_callbacks_.AddReceiver(tag, std::move(callback));
  }
  void NotifyDestroyed(ProxyBinding* proxy) {
    destroyed_callbacks_.Send(proxy);
  }

 private:
  void OnConnectRequest(AsyncProxyServerSocket* socket,
                        const SocketAddress& addr);
  void OnInternalRead(Socket* socket);
  void OnInternalWrite(Socket* socket);
  void OnInternalClose(Socket* socket, int err);
  void OnExternalConnect(Socket* socket);
  void OnExternalRead(Socket* socket);
  void OnExternalWrite(Socket* socket);
  void OnExternalClose(Socket* socket, int err);

  static void Read(Socket* socket, FifoBuffer* buffer);
  static void Write(Socket* socket, FifoBuffer* buffer);
  void Destroy();

  static const int kBufferSize = 4096;
  std::unique_ptr<AsyncProxyServerSocket> int_socket_;
  std::unique_ptr<Socket> ext_socket_;
  bool connected_;
  FifoBuffer out_buffer_;
  FifoBuffer in_buffer_;

  CallbackList<ProxyBinding*> destroyed_callbacks_;
};

class ProxyServer {
 public:
  ProxyServer(SocketFactory* int_factory,
              const SocketAddress& int_addr,
              SocketFactory* ext_factory,
              const SocketAddress& ext_ip);
  virtual ~ProxyServer();

  ProxyServer(const ProxyServer&) = delete;
  ProxyServer& operator=(const ProxyServer&) = delete;

  // Returns the address to which the proxy server is bound
  SocketAddress GetServerAddress();

 protected:
  void OnAcceptEvent(Socket* socket);
  virtual AsyncProxyServerSocket* WrapSocket(Socket* socket) = 0;

 private:
  SocketFactory* ext_factory_;
  SocketAddress ext_ip_;
  std::unique_ptr<Socket> server_socket_;
  std::vector<std::unique_ptr<ProxyBinding>> bindings_;
};

}  //  namespace webrtc

#endif  // RTC_BASE_PROXY_SERVER_H_
