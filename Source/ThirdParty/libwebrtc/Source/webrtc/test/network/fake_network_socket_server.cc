/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/fake_network_socket_server.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {
namespace {
std::string ToString(const rtc::SocketAddress& addr) {
  return addr.HostAsURIString() + ":" + std::to_string(addr.port());
}

}  // namespace

// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket,
                          public EmulatedNetworkReceiverInterface {
 public:
  explicit FakeNetworkSocket(FakeNetworkSocketServer* scoket_manager,
                             rtc::Thread* thread);
  ~FakeNetworkSocket() override;

  // Will be invoked by EmulatedEndpoint to deliver packets into this socket.
  void OnPacketReceived(EmulatedIpPacket packet) override;

  // rtc::Socket methods:
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Bind(const rtc::SocketAddress& addr) override;
  int Connect(const rtc::SocketAddress& addr) override;
  int Close() override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               rtc::SocketAddress* paddr,
               int64_t* timestamp) override;
  int Listen(int backlog) override;
  rtc::AsyncSocket* Accept(rtc::SocketAddress* paddr) override;
  int GetError() const override;
  void SetError(int error) override;
  ConnState GetState() const override;
  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

 private:
  FakeNetworkSocketServer* const socket_server_;
  rtc::Thread* const thread_;
  EmulatedEndpointImpl* endpoint_ RTC_GUARDED_BY(&thread_);
  rtc::SocketAddress local_addr_ RTC_GUARDED_BY(&thread_);
  rtc::SocketAddress remote_addr_ RTC_GUARDED_BY(&thread_);
  ConnState state_ RTC_GUARDED_BY(&thread_);
  int error_ RTC_GUARDED_BY(&thread_);
  std::map<Option, int> options_map_ RTC_GUARDED_BY(&thread_);

  absl::optional<EmulatedIpPacket> pending_ RTC_GUARDED_BY(thread_);
  rtc::scoped_refptr<PendingTaskSafetyFlag> alive_;
};

FakeNetworkSocket::FakeNetworkSocket(FakeNetworkSocketServer* socket_server,
                                     rtc::Thread* thread)
    : socket_server_(socket_server),
      thread_(thread),
      state_(CS_CLOSED),
      error_(0),
      alive_(PendingTaskSafetyFlag::Create()) {}

FakeNetworkSocket::~FakeNetworkSocket() {
  // Abandon all pending packets.
  alive_->SetNotAlive();

  Close();
  socket_server_->Unregister(this);
}

void FakeNetworkSocket::OnPacketReceived(EmulatedIpPacket packet) {
  auto task = [this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(thread_);
    if (!endpoint_->Enabled())
      return;
    RTC_DCHECK(!pending_);
    pending_ = std::move(packet);
    // Note that we expect that this will trigger exactly one call to RecvFrom()
    // where pending_packet will be read and reset. This call is done without
    // any thread switch (see AsyncUDPSocket::OnReadEvent) so it's safe to
    // assume that SignalReadEvent() will block until the packet has been read.
    SignalReadEvent(this);
    RTC_DCHECK(!pending_);
  };
  thread_->PostTask(ToQueuedTask(alive_, std::move(task)));
  socket_server_->WakeUp();
}


rtc::SocketAddress FakeNetworkSocket::GetLocalAddress() const {
  RTC_DCHECK_RUN_ON(thread_);
  return local_addr_;
}

rtc::SocketAddress FakeNetworkSocket::GetRemoteAddress() const {
  RTC_DCHECK_RUN_ON(thread_);
  return remote_addr_;
}

int FakeNetworkSocket::Bind(const rtc::SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_CHECK(local_addr_.IsNil())
      << "Socket already bound to address: " << ToString(local_addr_);
  local_addr_ = addr;
  endpoint_ = socket_server_->GetEndpointNode(local_addr_.ipaddr());
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
  RTC_DCHECK_RUN_ON(thread_);
  RTC_CHECK(remote_addr_.IsNil())
      << "Socket already connected to address: " << ToString(remote_addr_);
  RTC_CHECK(!local_addr_.IsNil())
      << "Socket have to be bind to some local address";
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  return 0;
}

int FakeNetworkSocket::Send(const void* pv, size_t cb) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_CHECK(state_ == CS_CONNECTED) << "Socket cannot send: not connected";
  return SendTo(pv, cb, remote_addr_);
}

int FakeNetworkSocket::SendTo(const void* pv,
                              size_t cb,
                              const rtc::SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(thread_);
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
  RTC_DCHECK_RUN_ON(thread_);

  if (timestamp) {
    *timestamp = -1;
  }
  RTC_CHECK(pending_);

  *paddr = pending_->from;
  size_t data_read = std::min(cb, pending_->size());
  memcpy(pv, pending_->cdata(), data_read);
  *timestamp = pending_->arrival_time.us();

  // According to RECV(2) Linux Man page
  // real socket will discard data, that won't fit into provided buffer,
  // but we won't to skip such error, so we will assert here.
  RTC_CHECK(data_read == pending_->size())
      << "Too small buffer is provided for socket read. "
         "Received data size: "
      << pending_->size() << "; Provided buffer size: " << cb;

  pending_.reset();

  // According to RECV(2) Linux Man page
  // real socket will return message length, not data read. In our case it is
  // actually the same value.
  return static_cast<int>(data_read);
}

int FakeNetworkSocket::Listen(int backlog) {
  RTC_CHECK(false) << "Listen() isn't valid for SOCK_DGRAM";
}

rtc::AsyncSocket* FakeNetworkSocket::Accept(rtc::SocketAddress* /*paddr*/) {
  RTC_CHECK(false) << "Accept() isn't valid for SOCK_DGRAM";
}

int FakeNetworkSocket::Close() {
  RTC_DCHECK_RUN_ON(thread_);
  state_ = CS_CLOSED;
  if (!local_addr_.IsNil()) {
    endpoint_->UnbindReceiver(local_addr_.port());
  }
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int FakeNetworkSocket::GetError() const {
  RTC_DCHECK_RUN_ON(thread_);
  return error_;
}

void FakeNetworkSocket::SetError(int error) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_CHECK(error == 0);
  error_ = error;
}

rtc::AsyncSocket::ConnState FakeNetworkSocket::GetState() const {
  RTC_DCHECK_RUN_ON(thread_);
  return state_;
}

int FakeNetworkSocket::GetOption(Option opt, int* value) {
  RTC_DCHECK_RUN_ON(thread_);
  auto it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int FakeNetworkSocket::SetOption(Option opt, int value) {
  RTC_DCHECK_RUN_ON(thread_);
  options_map_[opt] = value;
  return 0;
}

FakeNetworkSocketServer::FakeNetworkSocketServer(
    EndpointsContainer* endpoints_container)
    : endpoints_container_(endpoints_container),
      wakeup_(/*manual_reset=*/false, /*initially_signaled=*/false) {}
FakeNetworkSocketServer::~FakeNetworkSocketServer() = default;

EmulatedEndpointImpl* FakeNetworkSocketServer::GetEndpointNode(
    const rtc::IPAddress& ip) {
  return endpoints_container_->LookupByLocalAddress(ip);
}

void FakeNetworkSocketServer::Unregister(FakeNetworkSocket* socket) {
  MutexLock lock(&lock_);
  sockets_.erase(absl::c_find(sockets_, socket));
}

rtc::Socket* FakeNetworkSocketServer::CreateSocket(int /*family*/,
                                                   int /*type*/) {
  RTC_CHECK(false) << "Only async sockets are supported";
}

rtc::AsyncSocket* FakeNetworkSocketServer::CreateAsyncSocket(int family,
                                                             int type) {
  RTC_DCHECK(family == AF_INET || family == AF_INET6);
  // We support only UDP sockets for now.
  RTC_DCHECK(type == SOCK_DGRAM) << "Only UDP sockets are supported";
  RTC_DCHECK(thread_) << "must be attached to thread before creating sockets";
  FakeNetworkSocket* out = new FakeNetworkSocket(this, thread_);
  {
    MutexLock lock(&lock_);
    sockets_.push_back(out);
  }
  return out;
}

void FakeNetworkSocketServer::SetMessageQueue(rtc::Thread* thread) {
  thread_ = thread;
}

// Always returns true (if return false, it won't be invoked again...)
bool FakeNetworkSocketServer::Wait(int cms, bool process_io) {
  RTC_DCHECK(thread_ == rtc::Thread::Current());
  if (cms != 0)
    wakeup_.Wait(cms);
  return true;
}

void FakeNetworkSocketServer::WakeUp() {
  wakeup_.Set();
}


}  // namespace test
}  // namespace webrtc
