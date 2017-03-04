/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/udptransport.h"

#include <string>
#include <utility>  // For std::move.

#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_checker.h"

namespace cricket {

UdpTransport::UdpTransport(const std::string& transport_name,
                           std::unique_ptr<rtc::AsyncPacketSocket> socket)
    : transport_name_(transport_name), socket_(std::move(socket)) {
  RTC_DCHECK(socket_);
  socket_->SignalReadPacket.connect(this, &UdpTransport::OnSocketReadPacket);
  socket_->SignalSentPacket.connect(this, &UdpTransport::OnSocketSentPacket);
}

UdpTransport::~UdpTransport() {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
}

rtc::SocketAddress UdpTransport::GetLocalAddress() const {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  return socket_->GetLocalAddress();
}

bool UdpTransport::SetRemoteAddress(const rtc::SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  if (!addr.IsComplete()) {
    LOG(LS_WARNING) << "Remote address not complete.";
    return false;
  }
  // TODO(johan): check for ipv4, other settings.
  bool prev_destination_nil = remote_address_.IsNil();
  remote_address_ = addr;
  // Going from "didn't have destination" to "have destination" or vice versa.
  if (prev_destination_nil != remote_address_.IsNil()) {
    SignalWritableState(this);
    if (prev_destination_nil) {
      SignalReadyToSend(this);
    }
  }
  return true;
}

rtc::SocketAddress UdpTransport::GetRemoteAddress() const {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  return remote_address_;
}

bool UdpTransport::writable() const {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  return !remote_address_.IsNil();
}

int UdpTransport::SendPacket(const char* data,
                             size_t len,
                             const rtc::PacketOptions& options,
                             int flags) {
  // No thread_checker in high frequency network function.
  if (remote_address_.IsNil()) {
    LOG(LS_WARNING) << "Remote address not set.";
    send_error_ = ENOTCONN;
    return -1;
  }
  int result =
      socket_->SendTo((const void*)data, len, remote_address_, options);
  if (result <= 0) {
    LOG(LS_VERBOSE) << "SendPacket() " << result;
  }
  return result;
}

void UdpTransport::OnSocketReadPacket(rtc::AsyncPacketSocket* socket,
                                      const char* data,
                                      size_t len,
                                      const rtc::SocketAddress& remote_addr,
                                      const rtc::PacketTime& packet_time) {
  // No thread_checker in high frequency network function.
  SignalReadPacket(this, data, len, packet_time, 0);
}

void UdpTransport::OnSocketSentPacket(rtc::AsyncPacketSocket* socket,
                                      const rtc::SentPacket& packet) {
  RTC_DCHECK_EQ(socket_.get(), socket);
  SignalSentPacket(this, packet);
}

}  // namespace cricket
