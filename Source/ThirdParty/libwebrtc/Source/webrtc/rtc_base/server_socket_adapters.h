/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SERVER_SOCKET_ADAPTERS_H_
#define RTC_BASE_SERVER_SOCKET_ADAPTERS_H_

#include <cstddef>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_adapters.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

// Interface for implementing proxy server sockets.
class AsyncProxyServerSocket : public BufferedReadAdapter {
 public:
  AsyncProxyServerSocket(Socket* socket, size_t buffer_size);
  ~AsyncProxyServerSocket() override;

  [[deprecated]] void SubscribeConnectRequest(
      absl::AnyInvocable<void(AsyncProxyServerSocket*, const SocketAddress&)>
          callback) {
    connect_request_callbacks_.AddReceiver(std::move(callback));
  }
  void SubscribeConnectRequest(
      void* tag,
      absl::AnyInvocable<void(AsyncProxyServerSocket*, const SocketAddress&)>
          callback) {
    connect_request_callbacks_.AddReceiver(tag, std::move(callback));
  }
  void NotifyConnectRequest(AsyncProxyServerSocket* socket,
                            const SocketAddress& socket_address) {
    connect_request_callbacks_.Send(socket, socket_address);
  }

  virtual void SendConnectResult(int err, const SocketAddress& addr) = 0;

 private:
  CallbackList<AsyncProxyServerSocket*, const SocketAddress&>
      connect_request_callbacks_;
};

// Implements a socket adapter that performs the server side of a
// fake SSL handshake. Used when implementing a relay server that does "ssltcp".
class AsyncSSLServerSocket : public BufferedReadAdapter {
 public:
  explicit AsyncSSLServerSocket(Socket* socket);

  AsyncSSLServerSocket(const AsyncSSLServerSocket&) = delete;
  AsyncSSLServerSocket& operator=(const AsyncSSLServerSocket&) = delete;

 protected:
  void ProcessInput(char* data, size_t* len) override;
};

}  //  namespace webrtc


#endif  // RTC_BASE_SERVER_SOCKET_ADAPTERS_H_
