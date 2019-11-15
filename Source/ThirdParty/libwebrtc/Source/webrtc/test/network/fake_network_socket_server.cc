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

#include <utility>

#include "rtc_base/thread.h"

namespace webrtc {
namespace test {

FakeNetworkSocketServer::FakeNetworkSocketServer(
    Clock* clock,
    EndpointsContainer* endpoints_container)
    : clock_(clock),
      endpoints_container_(endpoints_container),
      wakeup_(/*manual_reset=*/false, /*initially_signaled=*/false) {}
FakeNetworkSocketServer::~FakeNetworkSocketServer() = default;

void FakeNetworkSocketServer::OnMessageQueueDestroyed() {
  msg_queue_ = nullptr;
}

EmulatedEndpoint* FakeNetworkSocketServer::GetEndpointNode(
    const rtc::IPAddress& ip) {
  return endpoints_container_->LookupByLocalAddress(ip);
}

void FakeNetworkSocketServer::Unregister(SocketIoProcessor* io_processor) {
  rtc::CritScope crit(&lock_);
  io_processors_.erase(io_processor);
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
  FakeNetworkSocket* out = new FakeNetworkSocket(this);
  {
    rtc::CritScope crit(&lock_);
    io_processors_.insert(out);
  }
  return out;
}

void FakeNetworkSocketServer::SetMessageQueue(rtc::MessageQueue* msg_queue) {
  msg_queue_ = msg_queue;
  if (msg_queue_) {
    msg_queue_->SignalQueueDestroyed.connect(
        this, &FakeNetworkSocketServer::OnMessageQueueDestroyed);
  }
}

// Always returns true (if return false, it won't be invoked again...)
bool FakeNetworkSocketServer::Wait(int cms, bool process_io) {
  RTC_DCHECK(msg_queue_ == rtc::Thread::Current());
  if (!process_io) {
    wakeup_.Wait(cms);
    return true;
  }
  wakeup_.Wait(cms);

  rtc::CritScope crit(&lock_);
  for (auto* io_processor : io_processors_) {
    while (io_processor->ProcessIo()) {
    }
  }
  return true;
}

void FakeNetworkSocketServer::WakeUp() {
  wakeup_.Set();
}

Timestamp FakeNetworkSocketServer::Now() const {
  return clock_->CurrentTime();
}

}  // namespace test
}  // namespace webrtc
