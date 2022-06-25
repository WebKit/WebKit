/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_
#define TEST_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_

#include <set>
#include <vector>

#include "api/units/timestamp.h"
#include "rtc_base/async_socket.h"
#include "rtc_base/event.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"
#include "test/network/network_emulation.h"

namespace webrtc {
namespace test {
class FakeNetworkSocket;

// FakeNetworkSocketServer must outlive any sockets it creates.
class FakeNetworkSocketServer : public rtc::SocketServer {
 public:
  explicit FakeNetworkSocketServer(EndpointsContainer* endpoints_controller);
  ~FakeNetworkSocketServer() override;


  // rtc::SocketFactory methods:
  rtc::Socket* CreateSocket(int family, int type) override;
  rtc::AsyncSocket* CreateAsyncSocket(int family, int type) override;

  // rtc::SocketServer methods:
  // Called by the network thread when this server is installed, kicking off the
  // message handler loop.
  void SetMessageQueue(rtc::Thread* thread) override;
  bool Wait(int cms, bool process_io) override;
  void WakeUp() override;

 protected:
  friend class FakeNetworkSocket;
  EmulatedEndpointImpl* GetEndpointNode(const rtc::IPAddress& ip);
  void Unregister(FakeNetworkSocket* socket);

 private:
  const EndpointsContainer* endpoints_container_;
  rtc::Event wakeup_;
  rtc::Thread* thread_ = nullptr;

  Mutex lock_;
  std::vector<FakeNetworkSocket*> sockets_ RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_FAKE_NETWORK_SOCKET_SERVER_H_
