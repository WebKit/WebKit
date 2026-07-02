/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ASYNC_UDP_SOCKET_H_
#define RTC_BASE_ASYNC_UDP_SOCKET_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Provides the ability to receive packets asynchronously.  Sends are not
// buffered since it is acceptable to drop packets under high load.
class AsyncUDPSocket : public AsyncPacketSocket {
 public:
  // Creates a new socket for sending asynchronous UDP packets using an
  // asynchronous socket from the given factory.
  static absl_nullable std::unique_ptr<AsyncUDPSocket> Create(
      const Environment& env,
      const SocketAddress& bind_address,
      SocketFactory& factory);

  AsyncUDPSocket(const Environment& env,
                 absl_nonnull std::unique_ptr<Socket> socket);
  ~AsyncUDPSocket() override = default;

  SocketAddress GetLocalAddress() const override;
  SocketAddress GetRemoteAddress() const override;
  int Send(const void* pv,
           size_t cb,
           const AsyncSocketPacketOptions& options) override;
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

 private:
  // called when the underlying socket is connected - DTLS handshake case
  void OnConnectEvent(Socket* socket);
  // Called when the underlying socket is ready to be read from.
  void OnReadEvent(Socket* socket);
  // Called when the underlying socket is ready to send.
  void OnWriteEvent(Socket* socket);

  const Environment env_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  std::unique_ptr<Socket> socket_;
  bool has_set_ect1_options_ = false;
  Buffer buffer_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<TimeDelta> socket_time_offset_
      RTC_GUARDED_BY(sequence_checker_);
};

}  //  namespace webrtc


#endif  // RTC_BASE_ASYNC_UDP_SOCKET_H_
