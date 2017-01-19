/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/test/channel_transport/udp_transport.h"

// We include the implementation header file to get at the dependency-injecting
// constructor.
#include "webrtc/voice_engine/test/channel_transport/udp_transport_impl.h"

// We must mock the socket manager, for which we need its definition.
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"

using ::testing::_;
using ::testing::Return;

namespace webrtc {
namespace test {

class MockUdpSocketWrapper : public UdpSocketWrapper {
 public:
  // The following methods have to be mocked because they are pure.
  MOCK_METHOD2(SetCallback, bool(CallbackObj, IncomingSocketCallback));
  MOCK_METHOD1(Bind, bool(const SocketAddress&));
  MOCK_METHOD0(ValidHandle, bool());
  MOCK_METHOD4(SetSockopt, bool(int32_t, int32_t,
                                const int8_t*,
                                int32_t));
  MOCK_METHOD1(SetTOS, int32_t(int32_t));
  MOCK_METHOD3(SendTo, int32_t(const int8_t*, size_t, const SocketAddress&));
  MOCK_METHOD8(SetQos, bool(int32_t, int32_t,
                            int32_t, int32_t,
                            int32_t, int32_t,
                            const SocketAddress &,
                            int32_t));
};

class MockUdpSocketManager : public UdpSocketManager {
 public:
  // Access to protected destructor.
  void Destroy() {
    delete this;
  }
  MOCK_METHOD2(Init, bool(int32_t, uint8_t&));
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, bool());
  MOCK_METHOD1(AddSocket, bool(UdpSocketWrapper*));
  MOCK_METHOD1(RemoveSocket, bool(UdpSocketWrapper*));
};

class MockSocketFactory :
    public UdpTransportImpl::SocketFactoryInterface {
 public:
  MockSocketFactory(std::vector<MockUdpSocketWrapper*>* socket_counter)
      : socket_counter_(socket_counter) {
  }
  UdpSocketWrapper* CreateSocket(const int32_t id,
                                 UdpSocketManager* mgr,
                                 CallbackObj obj,
                                 IncomingSocketCallback cb,
                                 bool ipV6Enable,
                                 bool disableGQOS) {
    MockUdpSocketWrapper* socket = new MockUdpSocketWrapper();
    // We instrument the socket with calls that are expected, but do
    // not matter for any specific test, in order to avoid warning messages.
    EXPECT_CALL(*socket, ValidHandle()).WillRepeatedly(Return(true));
    EXPECT_CALL(*socket, Bind(_)).WillOnce(Return(true));
    socket_counter_->push_back(socket);
    return socket;
  }
  std::vector<MockUdpSocketWrapper*>* socket_counter_;
};

class UDPTransportTest : public ::testing::Test {
 public:
  UDPTransportTest()
      : sockets_created_(0) {
  }

  ~UDPTransportTest() {
    // In production, sockets register themselves at creation time with
    // an UdpSocketManager, and the UdpSocketManager is responsible for
    // deleting them. In this test, we just delete them after the test.
    while (!sockets_created_.empty()) {
      delete sockets_created_.back();
      sockets_created_.pop_back();
    }
  }

  size_t NumSocketsCreated() {
    return sockets_created_.size();
  }

  std::vector<MockUdpSocketWrapper*>* sockets_created() {
    return &sockets_created_;
  }
private:
  std::vector<MockUdpSocketWrapper*> sockets_created_;
};

TEST_F(UDPTransportTest, CreateTransport) {
  int32_t id = 0;
  uint8_t threads = 1;
  UdpTransport* transport = UdpTransport::Create(id, threads);
  UdpTransport::Destroy(transport);
}

// This test verifies that the mock_socket is not called from the constructor.
TEST_F(UDPTransportTest, ConstructorDoesNotCreateSocket) {
  int32_t id = 0;
  UdpTransportImpl::SocketFactoryInterface* null_maker = NULL;
  UdpSocketManager* null_manager = NULL;
  UdpTransport* transport = new UdpTransportImpl(id,
                                                 null_maker,
                                                 null_manager);
  delete transport;
}

TEST_F(UDPTransportTest, InitializeSourcePorts) {
  int32_t id = 0;
  UdpTransportImpl::SocketFactoryInterface* mock_maker
      = new MockSocketFactory(sockets_created());
  MockUdpSocketManager* mock_manager = new MockUdpSocketManager();
  UdpTransport* transport = new UdpTransportImpl(id,
                                                 mock_maker,
                                                 mock_manager);
  EXPECT_EQ(0, transport->InitializeSourcePorts(4711, 4712));
  EXPECT_EQ(2u, NumSocketsCreated());

  delete transport;
  mock_manager->Destroy();
}

}  // namespace test
}  // namespace webrtc
