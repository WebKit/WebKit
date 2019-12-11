/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/fake_network_socket.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {
namespace {

std::string ToString(const rtc::SocketAddress& addr) {
  return addr.HostAsURIString() + ":" + std::to_string(addr.port());
}

}  // namespace

FakeNetworkSocket::FakeNetworkSocket(SocketManager* socket_manager)
    : socket_manager_(socket_manager),
      state_(CS_CLOSED),
      error_(0),
      pending_read_events_count_(0) {}
FakeNetworkSocket::~FakeNetworkSocket() {
  Close();
  socket_manager_->Unregister(this);
}

void FakeNetworkSocket::OnPacketReceived(EmulatedIpPacket packet) {
  {
    rtc::CritScope crit(&lock_);
    packet_queue_.push_back(std::move(packet));
    pending_read_events_count_++;
  }
  socket_manager_->WakeUp();
}

bool FakeNetworkSocket::ProcessIo() {
  {
    rtc::CritScope crit(&lock_);
    if (pending_read_events_count_ == 0) {
      return false;
    }
    pending_read_events_count_--;
    RTC_DCHECK_GE(pending_read_events_count_, 0);
  }
  if (!endpoint_->Enabled()) {
    // If endpoint disabled then just pop and discard packet.
    PopFrontPacket();
    return true;
  }
  SignalReadEvent(this);
  return true;
}

rtc::SocketAddress FakeNetworkSocket::GetLocalAddress() const {
  return local_addr_;
}

rtc::SocketAddress FakeNetworkSocket::GetRemoteAddress() const {
  return remote_addr_;
}

int FakeNetworkSocket::Bind(const rtc::SocketAddress& addr) {
  RTC_CHECK(local_addr_.IsNil())
      << "Socket already bound to address: " << ToString(local_addr_);
  local_addr_ = addr;
  endpoint_ = socket_manager_->GetEndpointNode(local_addr_.ipaddr());
  if (!endpoint_) {
    local_addr_.Clear();
    RTC_LOG(INFO) << "No endpoint for address: " << ToString(addr);
    error_ = EADDRNOTAVAIL;
    return 2;
  }
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(local_addr_.port(), this);
  if (!port) {
    local_addr_.Clear();
    RTC_LOG(INFO) << "Cannot bind to in-use address: " << ToString(addr);
    error_ = EADDRINUSE;
    return 1;
  }
  local_addr_.SetPort(port.value());
  return 0;
}

int FakeNetworkSocket::Connect(const rtc::SocketAddress& addr) {
  RTC_CHECK(remote_addr_.IsNil())
      << "Socket already connected to address: " << ToString(remote_addr_);
  RTC_CHECK(!local_addr_.IsNil())
      << "Socket have to be bind to some local address";
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  return 0;
}

int FakeNetworkSocket::Send(const void* pv, size_t cb) {
  RTC_CHECK(state_ == CS_CONNECTED) << "Socket cannot send: not connected";
  return SendTo(pv, cb, remote_addr_);
}

int FakeNetworkSocket::SendTo(const void* pv,
                              size_t cb,
                              const rtc::SocketAddress& addr) {
  RTC_CHECK(!local_addr_.IsNil())
      << "Socket have to be bind to some local address";
  if (!endpoint_->Enabled()) {
    error_ = ENETDOWN;
    return -1;
  }
  rtc::CopyOnWriteBuffer packet(static_cast<const uint8_t*>(pv), cb);
  endpoint_->SendPacket(local_addr_, addr, packet);
  return cb;
}

int FakeNetworkSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  rtc::SocketAddress paddr;
  return RecvFrom(pv, cb, &paddr, timestamp);
}

// Reads 1 packet from internal queue. Reads up to |cb| bytes into |pv|
// and returns the length of received packet.
int FakeNetworkSocket::RecvFrom(void* pv,
                                size_t cb,
                                rtc::SocketAddress* paddr,
                                int64_t* timestamp) {
  if (timestamp) {
    *timestamp = -1;
  }
  absl::optional<EmulatedIpPacket> packetOpt = PopFrontPacket();

  if (!packetOpt) {
    error_ = EAGAIN;
    return -1;
  }

  EmulatedIpPacket packet = std::move(packetOpt.value());
  *paddr = packet.from;
  size_t data_read = std::min(cb, packet.size());
  memcpy(pv, packet.cdata(), data_read);
  *timestamp = packet.arrival_time.us();

  // According to RECV(2) Linux Man page
  // real socket will discard data, that won't fit into provided buffer,
  // but we won't to skip such error, so we will assert here.
  RTC_CHECK(data_read == packet.size())
      << "Too small buffer is provided for socket read. "
      << "Received data size: " << packet.size()
      << "; Provided buffer size: " << cb;

  // According to RECV(2) Linux Man page
  // real socket will return message length, not data read. In our case it is
  // actually the same value.
  return static_cast<int>(packet.size());
}

int FakeNetworkSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Listen() isn't valid for SOCK_DGRAM";
}

rtc::AsyncSocket* FakeNetworkSocket::Accept(rtc::SocketAddress* /*paddr*/) {
  RTC_CHECK(false) << "Accept() isn't valid for SOCK_DGRAM";
}

int FakeNetworkSocket::Close() {
  state_ = CS_CLOSED;
  if (!local_addr_.IsNil()) {
    endpoint_->UnbindReceiver(local_addr_.port());
  }
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int FakeNetworkSocket::GetError() const {
  return error_;
}

void FakeNetworkSocket::SetError(int error) {
  RTC_CHECK(error == 0);
  error_ = error;
}

rtc::AsyncSocket::ConnState FakeNetworkSocket::GetState() const {
  return state_;
}

int FakeNetworkSocket::GetOption(Option opt, int* value) {
  auto it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int FakeNetworkSocket::SetOption(Option opt, int value) {
  options_map_[opt] = value;
  return 0;
}

absl::optional<EmulatedIpPacket> FakeNetworkSocket::PopFrontPacket() {
  rtc::CritScope crit(&lock_);
  if (packet_queue_.empty()) {
    return absl::nullopt;
  }

  absl::optional<EmulatedIpPacket> packet =
      absl::make_optional(std::move(packet_queue_.front()));
  packet_queue_.pop_front();
  return packet;
}

}  // namespace test
}  // namespace webrtc
