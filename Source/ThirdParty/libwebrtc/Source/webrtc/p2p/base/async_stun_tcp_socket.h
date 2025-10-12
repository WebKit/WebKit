/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ASYNC_STUN_TCP_SOCKET_H_
#define P2P_BASE_ASYNC_STUN_TCP_SOCKET_H_

#include <cstddef>
#include <cstdint>

#include "api/array_view.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

class AsyncStunTCPSocket : public AsyncTCPSocketBase {
 public:
  // Binds and connects `socket` and creates AsyncTCPSocket for
  // it. Takes ownership of `socket`. Returns NULL if bind() or
  // connect() fail (`socket` is destroyed in that case).
  static AsyncStunTCPSocket* Create(Socket* socket,
                                    const SocketAddress& bind_address,
                                    const SocketAddress& remote_address);

  explicit AsyncStunTCPSocket(Socket* socket);

  AsyncStunTCPSocket(const AsyncStunTCPSocket&) = delete;
  AsyncStunTCPSocket& operator=(const AsyncStunTCPSocket&) = delete;

  int Send(const void* pv,
           size_t cb,
           const AsyncSocketPacketOptions& options) override;
  size_t ProcessInput(ArrayView<const uint8_t> data) override;

 private:
  // This method returns the message hdr + length written in the header.
  // This method also returns the number of padding bytes needed/added to the
  // turn message. `pad_bytes` should be used only when `is_turn` is true.
  size_t GetExpectedLength(const void* data, size_t len, int* pad_bytes);
};

}  //  namespace webrtc


#endif  // P2P_BASE_ASYNC_STUN_TCP_SOCKET_H_
