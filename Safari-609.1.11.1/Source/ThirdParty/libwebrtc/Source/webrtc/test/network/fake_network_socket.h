/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_FAKE_NETWORK_SOCKET_H_
#define TEST_NETWORK_FAKE_NETWORK_SOCKET_H_

#include <deque>
#include <map>
#include <vector>

#include "rtc_base/async_socket.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/socket_address.h"
#include "test/network/network_emulation.h"

namespace webrtc {
namespace test {

class SocketIoProcessor {
 public:
  virtual ~SocketIoProcessor() = default;

  // Process single IO operation.
  virtual bool ProcessIo() = 0;
};

class SocketManager {
 public:
  virtual ~SocketManager() = default;

  virtual void WakeUp() = 0;
  virtual void Unregister(SocketIoProcessor* io_processor) = 0;
  // Provides endpoints by IP address.
  virtual EmulatedEndpoint* GetEndpointNode(const rtc::IPAddress& ip) = 0;
};

// Represents a socket, which will operate with emulated network.
class FakeNetworkSocket : public rtc::AsyncSocket,
                          public EmulatedNetworkReceiverInterface,
                          public SocketIoProcessor {
 public:
  explicit FakeNetworkSocket(SocketManager* scoket_manager);
  ~FakeNetworkSocket() override;

  // Will be invoked by EmulatedEndpoint to deliver packets into this socket.
  void OnPacketReceived(EmulatedIpPacket packet) override;
  // Will fire read event for incoming packets.
  bool ProcessIo() override;

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
  absl::optional<EmulatedIpPacket> PopFrontPacket();

  SocketManager* const socket_manager_;
  EmulatedEndpoint* endpoint_;

  rtc::SocketAddress local_addr_;
  rtc::SocketAddress remote_addr_;
  ConnState state_;
  int error_;
  std::map<Option, int> options_map_;

  rtc::CriticalSection lock_;
  // Count of packets in the queue for which we didn't fire read event.
  // |pending_read_events_count_| can be different from |packet_queue_.size()|
  // because read events will be fired by one thread and packets in the queue
  // can be processed by another thread.
  int pending_read_events_count_;
  std::deque<EmulatedIpPacket> packet_queue_ RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_FAKE_NETWORK_SOCKET_H_
