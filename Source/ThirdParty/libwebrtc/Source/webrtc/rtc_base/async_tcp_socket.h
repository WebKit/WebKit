/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ASYNC_TCP_SOCKET_H_
#define RTC_BASE_ASYNC_TCP_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "api/array_view.h"
#include "api/environment/environment.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

// Simulates UDP semantics over TCP.  Send and Recv packet sizes
// are preserved, and drops packets silently on Send, rather than
// buffer them in user space.
class AsyncTCPSocketBase : public AsyncPacketSocket {
 public:
  AsyncTCPSocketBase(absl_nonnull std::unique_ptr<Socket> socket,
                     size_t max_packet_size);
  ~AsyncTCPSocketBase() override;

  AsyncTCPSocketBase(const AsyncTCPSocketBase&) = delete;
  AsyncTCPSocketBase& operator=(const AsyncTCPSocketBase&) = delete;

  // Pure virtual methods to send and recv data.
  int Send(const void* pv,
           size_t cb,
           const AsyncSocketPacketOptions& options) override = 0;
  // Must return the number of bytes processed.
  virtual size_t ProcessInput(ArrayView<const uint8_t> data) = 0;

  SocketAddress GetLocalAddress() const override;
  SocketAddress GetRemoteAddress() const override;
  int SendTo(const void* pv,
             size_t cb,
             const SocketAddress& addr,
             const AsyncSocketPacketOptions& options) override;
  int Close() override;

  State GetState() const override;
  int GetOption(Socket::Option opt, int* value) override;
  int SetOption(Socket::Option opt, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 protected:
  int FlushOutBuffer();
  // Add data to `outbuf_`.
  void AppendToOutBuffer(const void* pv, size_t cb);

  // Helper methods for `outpos_`.
  bool IsOutBufferEmpty() const { return outbuf_.empty(); }
  void ClearOutBuffer() { outbuf_.Clear(); }

 private:
  // Called by the underlying socket
  void OnConnectEvent(Socket* socket);
  void OnReadEvent(Socket* socket);
  void OnWriteEvent(Socket* socket);
  void OnCloseEvent(Socket* socket, int error);

  absl_nonnull std::unique_ptr<Socket> socket_;
  Buffer inbuf_;
  Buffer outbuf_;
  size_t max_insize_;
  size_t max_outsize_;
};

class AsyncTCPSocket : public AsyncTCPSocketBase {
 public:
  AsyncTCPSocket(const Environment& env,
                 absl_nonnull std::unique_ptr<Socket> socket);

  AsyncTCPSocket(const AsyncTCPSocket&) = delete;
  AsyncTCPSocket& operator=(const AsyncTCPSocket&) = delete;

  int Send(const void* pv,
           size_t cb,
           const AsyncSocketPacketOptions& options) override;
  size_t ProcessInput(ArrayView<const uint8_t>) override;

 private:
  const Environment env_;
};

class AsyncTcpListenSocket : public AsyncListenSocket {
 public:
  AsyncTcpListenSocket(const Environment& env, std::unique_ptr<Socket> socket);

  State GetState() const override;
  SocketAddress GetLocalAddress() const override;

 protected:
  const Environment& env() const { return env_; }

 private:
  // Called by the underlying socket
  void OnReadEvent(Socket* socket);
  virtual void HandleIncomingConnection(std::unique_ptr<Socket> socket);

  const Environment env_;
  std::unique_ptr<Socket> socket_;
};

}  //  namespace webrtc

#endif  // RTC_BASE_ASYNC_TCP_SOCKET_H_
