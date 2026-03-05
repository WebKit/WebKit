/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_udp_socket.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"

namespace webrtc {

absl_nullable std::unique_ptr<AsyncUDPSocket> AsyncUDPSocket::Create(
    const Environment& env,
    const SocketAddress& bind_address,
    SocketFactory& factory) {
  std::unique_ptr<Socket> socket =
      factory.Create(bind_address.family(), SOCK_DGRAM);
  if (socket == nullptr) {
    return nullptr;
  }
  if (socket->Bind(bind_address) < 0) {
    RTC_LOG(LS_ERROR) << "Bind() failed with error " << socket->GetError();
    return nullptr;
  }
  return std::make_unique<AsyncUDPSocket>(env, std::move(socket));
}

AsyncUDPSocket::AsyncUDPSocket(const Environment& env,
                               absl_nonnull std::unique_ptr<Socket> socket)
    : env_(env),
      sequence_checker_(SequenceChecker::kDetached),
      socket_(std::move(socket)) {
  // The socket should start out readable but not writable.
  socket_->SignalReadEvent.connect(this, &AsyncUDPSocket::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &AsyncUDPSocket::OnWriteEvent);
}

SocketAddress AsyncUDPSocket::GetLocalAddress() const {
  return socket_->GetLocalAddress();
}

SocketAddress AsyncUDPSocket::GetRemoteAddress() const {
  return socket_->GetRemoteAddress();
}

int AsyncUDPSocket::Send(const void* pv,
                         size_t cb,
                         const AsyncSocketPacketOptions& options) {
  SentPacketInfo sent_packet(options.packet_id,
                             env_.clock().TimeInMilliseconds(),
                             options.info_signaled_after_sent);
  CopySocketInformationToPacketInfo(cb, *this, &sent_packet.info);
  int ret = socket_->Send(pv, cb);
  SignalSentPacket(this, sent_packet);
  return ret;
}

int AsyncUDPSocket::SendTo(const void* pv,
                           size_t cb,
                           const SocketAddress& addr,
                           const AsyncSocketPacketOptions& options) {
  SentPacketInfo sent_packet(options.packet_id,
                             env_.clock().TimeInMilliseconds(),
                             options.info_signaled_after_sent);
  CopySocketInformationToPacketInfo(cb, *this, &sent_packet.info);
  if (has_set_ect1_options_ != options.ect_1) {
    // It is unclear what is most efficient, setting options on every sent
    // packet or when changed. Potentially, can separate send sockets be used?
    // This is the easier implementation.
    if (socket_->SetOption(Socket::Option::OPT_SEND_ECN,
                           options.ect_1 ? 1 : 0) == 0) {
      has_set_ect1_options_ = options.ect_1;
    }
  }
  int ret = socket_->SendTo(pv, cb, addr);
  SignalSentPacket(this, sent_packet);
  return ret;
}

int AsyncUDPSocket::Close() {
  return socket_->Close();
}

AsyncUDPSocket::State AsyncUDPSocket::GetState() const {
  return STATE_BOUND;
}

int AsyncUDPSocket::GetOption(Socket::Option opt, int* value) {
  return socket_->GetOption(opt, value);
}

int AsyncUDPSocket::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int AsyncUDPSocket::GetError() const {
  return socket_->GetError();
}

void AsyncUDPSocket::SetError(int error) {
  return socket_->SetError(error);
}

void AsyncUDPSocket::OnReadEvent(Socket* socket) {
  RTC_DCHECK(socket_.get() == socket);
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  Socket::ReceiveBuffer receive_buffer(buffer_);
  int len = socket_->RecvFrom(receive_buffer);
  if (len < 0) {
    // An error here typically means we got an ICMP error in response to our
    // send datagram, indicating the remote address was unreachable.
    // When doing ICE, this kind of thing will often happen.
    // TODO: Do something better like forwarding the error to the user.
    SocketAddress local_addr = socket_->GetLocalAddress();
    RTC_LOG(LS_INFO) << "AsyncUDPSocket[" << local_addr.ToSensitiveString()
                     << "] receive failed with error " << socket_->GetError();
    return;
  }
  if (len == 0) {
    // Spurios wakeup.
    return;
  }

  if (!receive_buffer.arrival_time) {
    // Timestamp from socket is not available.
    receive_buffer.arrival_time = env_.clock().CurrentTime();
  } else {
    Timestamp current_time = env_.clock().CurrentTime();
    if (!socket_time_offset_ ||
        *receive_buffer.arrival_time + *socket_time_offset_ > current_time) {
      // Estimate timestamp offset from first packet arrival time.
      // This may be wrong if packets have been buffered in the socket before we
      // read the first packet and `socket_time_offset_` may then have to be set
      // again to ensure no arrival times are set in the future.
      socket_time_offset_ = current_time - *receive_buffer.arrival_time;
    }
    *receive_buffer.arrival_time += *socket_time_offset_;
    RTC_DCHECK_LE(*receive_buffer.arrival_time, current_time);
  }
  NotifyPacketReceived(
      ReceivedIpPacket(receive_buffer.payload, receive_buffer.source_address,
                       receive_buffer.arrival_time, receive_buffer.ecn));
}

void AsyncUDPSocket::OnWriteEvent(Socket* socket) {
  SignalReadyToSend(this);
}

}  // namespace webrtc
