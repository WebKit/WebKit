/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/test/stun_server.h"

#include <cstring>
#include <memory>
#include <string>

#include "api/environment/environment.h"
#include "api/transport/stun.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/test_client.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const SocketAddress server_addr("99.99.99.1", 3478);
const SocketAddress client_addr("1.2.3.4", 1234);
}  // namespace

class StunServerTest : public ::testing::Test {
 public:
  void Send(const StunMessage& msg) {
    ByteBufferWriter buf;
    msg.Write(&buf);
    Send(reinterpret_cast<const char*>(buf.Data()),
         static_cast<int>(buf.Length()));
  }
  void Send(const char* buf, int len) { client_.SendTo(buf, len, server_addr); }
  bool ReceiveFails() { return client_.CheckNoPacket(); }
  StunMessage* Receive() {
    StunMessage* msg = nullptr;
    std::unique_ptr<TestClient::Packet> packet =
        client_.NextPacket(TestClient::kTimeoutMs);
    if (packet) {
      ByteBufferReader buf(packet->buf);
      msg = new StunMessage();
      msg->Read(&buf);
    }
    return msg;
  }

 private:
  const Environment env_ = CreateTestEnvironment();
  VirtualSocketServer ss_;
  AutoSocketServerThread main_thread_{&ss_};
  StunServer server_{AsyncUDPSocket::Create(env_, server_addr, ss_)};
  TestClient client_{AsyncUDPSocket::Create(env_, client_addr, ss_)};
};

TEST_F(StunServerTest, TestGood) {
  // kStunLegacyTransactionIdLength = 16 for legacy RFC 3489 request
  std::string transaction_id = "0123456789abcdef";
  StunMessage req(STUN_BINDING_REQUEST, transaction_id);
  Send(req);

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg != nullptr);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_EQ(req.transaction_id(), msg->transaction_id());

  const StunAddressAttribute* mapped_addr =
      msg->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  EXPECT_TRUE(mapped_addr != nullptr);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(client_addr.port(), mapped_addr->port());

  delete msg;
}

TEST_F(StunServerTest, TestGoodXorMappedAddr) {
  // kStunTransactionIdLength = 12 for RFC 5389 request
  // StunMessage::Write will automatically insert magic cookie (0x2112A442)
  std::string transaction_id = "0123456789ab";
  StunMessage req(STUN_BINDING_REQUEST, transaction_id);
  Send(req);

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg != nullptr);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_EQ(req.transaction_id(), msg->transaction_id());

  const StunAddressAttribute* mapped_addr =
      msg->GetAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  EXPECT_TRUE(mapped_addr != nullptr);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(client_addr.port(), mapped_addr->port());

  delete msg;
}

// Send legacy RFC 3489 request, should not get xor mapped addr
TEST_F(StunServerTest, TestNoXorMappedAddr) {
  // kStunLegacyTransactionIdLength = 16 for legacy RFC 3489 request
  std::string transaction_id = "0123456789abcdef";
  StunMessage req(STUN_BINDING_REQUEST, transaction_id);
  Send(req);

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg != nullptr);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_EQ(req.transaction_id(), msg->transaction_id());

  const StunAddressAttribute* mapped_addr =
      msg->GetAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  EXPECT_TRUE(mapped_addr == nullptr);

  delete msg;
}

TEST_F(StunServerTest, TestBad) {
  const char* bad =
      "this is a completely nonsensical message whose only "
      "purpose is to make the parser go 'ack'.  it doesn't "
      "look anything like a normal stun message";
  Send(bad, static_cast<int>(strlen(bad)));

  ASSERT_TRUE(ReceiveFails());
}

}  // namespace webrtc
