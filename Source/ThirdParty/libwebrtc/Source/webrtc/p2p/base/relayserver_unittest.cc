/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "p2p/base/relayserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/testclient.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

using rtc::SocketAddress;

namespace cricket {
namespace {

constexpr uint32_t LIFETIME = 4;  // seconds
const SocketAddress server_int_addr("127.0.0.1", 5000);
const SocketAddress server_ext_addr("127.0.0.1", 5001);
const SocketAddress client1_addr("127.0.0.1", 6111);
const SocketAddress client2_addr("127.0.0.1", 7222);
const char* bad =
    "this is a completely nonsensical message whose only "
    "purpose is to make the parser go 'ack'.  it doesn't "
    "look anything like a normal stun message";
const char* msg1 = "spamspamspamspamspamspamspambakedbeansspam";
const char* msg2 = "Lobster Thermidor a Crevette with a mornay sauce...";

}  // namespace

class RelayServerTest : public testing::Test {
 public:
  RelayServerTest()
      : ss_(new rtc::VirtualSocketServer()),
        thread_(ss_.get()),
        username_(rtc::CreateRandomString(12)),
        password_(rtc::CreateRandomString(12)) {}

 protected:
  virtual void SetUp() {
    server_.reset(new RelayServer(rtc::Thread::Current()));

    server_->AddInternalSocket(
        rtc::AsyncUDPSocket::Create(ss_.get(), server_int_addr));
    server_->AddExternalSocket(
        rtc::AsyncUDPSocket::Create(ss_.get(), server_ext_addr));

    client1_.reset(new rtc::TestClient(absl::WrapUnique(
        rtc::AsyncUDPSocket::Create(ss_.get(), client1_addr))));
    client2_.reset(new rtc::TestClient(absl::WrapUnique(
        rtc::AsyncUDPSocket::Create(ss_.get(), client2_addr))));
  }

  void Allocate() {
    std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_ALLOCATE_REQUEST));
    AddUsernameAttr(req.get(), username_);
    AddLifetimeAttr(req.get(), LIFETIME);
    Send1(req.get());
    delete Receive1();
  }
  void Bind() {
    std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_BINDING_REQUEST));
    AddUsernameAttr(req.get(), username_);
    Send2(req.get());
    delete Receive1();
  }

  void Send1(const StunMessage* msg) {
    rtc::ByteBufferWriter buf;
    msg->Write(&buf);
    SendRaw1(buf.Data(), static_cast<int>(buf.Length()));
  }
  void Send2(const StunMessage* msg) {
    rtc::ByteBufferWriter buf;
    msg->Write(&buf);
    SendRaw2(buf.Data(), static_cast<int>(buf.Length()));
  }
  void SendRaw1(const char* data, int len) {
    return Send(client1_.get(), data, len, server_int_addr);
  }
  void SendRaw2(const char* data, int len) {
    return Send(client2_.get(), data, len, server_ext_addr);
  }
  void Send(rtc::TestClient* client,
            const char* data,
            int len,
            const SocketAddress& addr) {
    client->SendTo(data, len, addr);
  }

  bool Receive1Fails() { return client1_.get()->CheckNoPacket(); }
  bool Receive2Fails() { return client2_.get()->CheckNoPacket(); }

  StunMessage* Receive1() { return Receive(client1_.get()); }
  StunMessage* Receive2() { return Receive(client2_.get()); }
  std::string ReceiveRaw1() { return ReceiveRaw(client1_.get()); }
  std::string ReceiveRaw2() { return ReceiveRaw(client2_.get()); }
  StunMessage* Receive(rtc::TestClient* client) {
    StunMessage* msg = NULL;
    std::unique_ptr<rtc::TestClient::Packet> packet =
        client->NextPacket(rtc::TestClient::kTimeoutMs);
    if (packet) {
      rtc::ByteBufferWriter buf(packet->buf, packet->size);
      rtc::ByteBufferReader read_buf(buf);
      msg = new RelayMessage();
      msg->Read(&read_buf);
    }
    return msg;
  }
  std::string ReceiveRaw(rtc::TestClient* client) {
    std::string raw;
    std::unique_ptr<rtc::TestClient::Packet> packet =
        client->NextPacket(rtc::TestClient::kTimeoutMs);
    if (packet) {
      raw = std::string(packet->buf, packet->size);
    }
    return raw;
  }

  static StunMessage* CreateStunMessage(int type) {
    StunMessage* msg = new RelayMessage();
    msg->SetType(type);
    msg->SetTransactionID(rtc::CreateRandomString(kStunTransactionIdLength));
    return msg;
  }
  static void AddMagicCookieAttr(StunMessage* msg) {
    auto attr = StunAttribute::CreateByteString(STUN_ATTR_MAGIC_COOKIE);
    attr->CopyBytes(TURN_MAGIC_COOKIE_VALUE, sizeof(TURN_MAGIC_COOKIE_VALUE));
    msg->AddAttribute(std::move(attr));
  }
  static void AddUsernameAttr(StunMessage* msg, const std::string& val) {
    auto attr = StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
    attr->CopyBytes(val.c_str(), val.size());
    msg->AddAttribute(std::move(attr));
  }
  static void AddLifetimeAttr(StunMessage* msg, int val) {
    auto attr = StunAttribute::CreateUInt32(STUN_ATTR_LIFETIME);
    attr->SetValue(val);
    msg->AddAttribute(std::move(attr));
  }
  static void AddDestinationAttr(StunMessage* msg, const SocketAddress& addr) {
    auto attr = StunAttribute::CreateAddress(STUN_ATTR_DESTINATION_ADDRESS);
    attr->SetIP(addr.ipaddr());
    attr->SetPort(addr.port());
    msg->AddAttribute(std::move(attr));
  }

  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<RelayServer> server_;
  std::unique_ptr<rtc::TestClient> client1_;
  std::unique_ptr<rtc::TestClient> client2_;
  std::string username_;
  std::string password_;
};

// Send a complete nonsense message and verify that it is eaten.
TEST_F(RelayServerTest, TestBadRequest) {
  SendRaw1(bad, static_cast<int>(strlen(bad)));
  ASSERT_TRUE(Receive1Fails());
}

// Send an allocate request without a username and verify it is rejected.
TEST_F(RelayServerTest, TestAllocateNoUsername) {
  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_ALLOCATE_REQUEST)),
      res;

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(32, err->number());
  EXPECT_EQ("Missing Username", err->reason());
}

// Send a binding request and verify that it is rejected.
TEST_F(RelayServerTest, TestBindingRequest) {
  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_BINDING_REQUEST)),
      res;
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());
}

// Send an allocate request and verify that it is accepted.
TEST_F(RelayServerTest, TestAllocate) {
  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_ALLOCATE_REQUEST)),
      res;
  AddUsernameAttr(req.get(), username_);
  AddLifetimeAttr(req.get(), LIFETIME);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunAddressAttribute* mapped_addr =
      res->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  ASSERT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(server_ext_addr.port(), mapped_addr->port());
  EXPECT_EQ(server_ext_addr.ipaddr(), mapped_addr->ipaddr());

  const StunUInt32Attribute* res_lifetime_attr =
      res->GetUInt32(STUN_ATTR_LIFETIME);
  ASSERT_TRUE(res_lifetime_attr != NULL);
  EXPECT_EQ(LIFETIME, res_lifetime_attr->value());
}

// Send a second allocate request and verify that it is also accepted, though
// the lifetime should be ignored.
TEST_F(RelayServerTest, TestReallocate) {
  Allocate();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_ALLOCATE_REQUEST)),
      res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_ALLOCATE_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunAddressAttribute* mapped_addr =
      res->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  ASSERT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(server_ext_addr.port(), mapped_addr->port());
  EXPECT_EQ(server_ext_addr.ipaddr(), mapped_addr->ipaddr());

  const StunUInt32Attribute* lifetime_attr = res->GetUInt32(STUN_ATTR_LIFETIME);
  ASSERT_TRUE(lifetime_attr != NULL);
  EXPECT_EQ(LIFETIME, lifetime_attr->value());
}

// Send a request from another client and see that it arrives at the first
// client in the binding.
TEST_F(RelayServerTest, TestRemoteBind) {
  Allocate();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_BINDING_REQUEST)),
      res;
  AddUsernameAttr(req.get(), username_);

  Send2(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_DATA_INDICATION, res->type());

  const StunByteStringAttribute* recv_data = res->GetByteString(STUN_ATTR_DATA);
  ASSERT_TRUE(recv_data != NULL);

  rtc::ByteBufferReader buf(recv_data->bytes(), recv_data->length());
  std::unique_ptr<StunMessage> res2(new StunMessage());
  EXPECT_TRUE(res2->Read(&buf));
  EXPECT_EQ(STUN_BINDING_REQUEST, res2->type());
  EXPECT_EQ(req->transaction_id(), res2->transaction_id());

  const StunAddressAttribute* src_addr =
      res->GetAddress(STUN_ATTR_SOURCE_ADDRESS2);
  ASSERT_TRUE(src_addr != NULL);
  EXPECT_EQ(1, src_addr->family());
  EXPECT_EQ(client2_addr.ipaddr(), src_addr->ipaddr());
  EXPECT_EQ(client2_addr.port(), src_addr->port());

  EXPECT_TRUE(Receive2Fails());
}

// Send a complete nonsense message to the established connection and verify
// that it is dropped by the server.
TEST_F(RelayServerTest, TestRemoteBadRequest) {
  Allocate();
  Bind();

  SendRaw1(bad, static_cast<int>(strlen(bad)));
  EXPECT_TRUE(Receive1Fails());
  EXPECT_TRUE(Receive2Fails());
}

// Send a send request without a username and verify it is rejected.
TEST_F(RelayServerTest, TestSendRequestMissingUsername) {
  Allocate();
  Bind();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(32, err->number());
  EXPECT_EQ("Missing Username", err->reason());
}

// Send a send request with the wrong username and verify it is rejected.
TEST_F(RelayServerTest, TestSendRequestBadUsername) {
  Allocate();
  Bind();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), "foobarbizbaz");

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(30, err->number());
  EXPECT_EQ("Stale Credentials", err->reason());
}

// Send a send request without a destination address and verify that it is
// rejected.
TEST_F(RelayServerTest, TestSendRequestNoDestinationAddress) {
  Allocate();
  Bind();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Bad Request", err->reason());
}

// Send a send request without data and verify that it is rejected.
TEST_F(RelayServerTest, TestSendRequestNoData) {
  Allocate();
  Bind();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);
  AddDestinationAttr(req.get(), client2_addr);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(4, err->eclass());
  EXPECT_EQ(00, err->number());
  EXPECT_EQ("Bad Request", err->reason());
}

// Send a binding request after an allocate and verify that it is rejected.
TEST_F(RelayServerTest, TestSendRequestWrongType) {
  Allocate();
  Bind();

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_BINDING_REQUEST)),
      res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, res->type());
  EXPECT_EQ(req->transaction_id(), res->transaction_id());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());
}

// Verify that we can send traffic back and forth between the clients after a
// successful allocate and bind.
TEST_F(RelayServerTest, TestSendRaw) {
  Allocate();
  Bind();

  for (int i = 0; i < 10; i++) {
    std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
    AddMagicCookieAttr(req.get());
    AddUsernameAttr(req.get(), username_);
    AddDestinationAttr(req.get(), client2_addr);

    auto send_data = StunAttribute::CreateByteString(STUN_ATTR_DATA);
    send_data->CopyBytes(msg1);
    req->AddAttribute(std::move(send_data));

    Send1(req.get());
    EXPECT_EQ(msg1, ReceiveRaw2());
    SendRaw2(msg2, static_cast<int>(strlen(msg2)));
    res.reset(Receive1());

    ASSERT_TRUE(res);
    EXPECT_EQ(STUN_DATA_INDICATION, res->type());

    const StunAddressAttribute* src_addr =
        res->GetAddress(STUN_ATTR_SOURCE_ADDRESS2);
    ASSERT_TRUE(src_addr != NULL);
    EXPECT_EQ(1, src_addr->family());
    EXPECT_EQ(client2_addr.ipaddr(), src_addr->ipaddr());
    EXPECT_EQ(client2_addr.port(), src_addr->port());

    const StunByteStringAttribute* recv_data =
        res->GetByteString(STUN_ATTR_DATA);
    ASSERT_TRUE(recv_data != NULL);
    EXPECT_EQ(strlen(msg2), recv_data->length());
    EXPECT_EQ(0, memcmp(msg2, recv_data->bytes(), recv_data->length()));
  }
}

// Verify that a binding expires properly, and rejects send requests.
// Flaky, see https://code.google.com/p/webrtc/issues/detail?id=4134
TEST_F(RelayServerTest, DISABLED_TestExpiration) {
  Allocate();
  Bind();

  // Wait twice the lifetime to make sure the server has expired the binding.
  rtc::Thread::Current()->ProcessMessages((LIFETIME * 2) * 1000);

  std::unique_ptr<StunMessage> req(CreateStunMessage(STUN_SEND_REQUEST)), res;
  AddMagicCookieAttr(req.get());
  AddUsernameAttr(req.get(), username_);
  AddDestinationAttr(req.get(), client2_addr);

  auto data_attr = StunAttribute::CreateByteString(STUN_ATTR_DATA);
  data_attr->CopyBytes(msg1);
  req->AddAttribute(std::move(data_attr));

  Send1(req.get());
  res.reset(Receive1());

  ASSERT_TRUE(res.get() != NULL);
  EXPECT_EQ(STUN_SEND_ERROR_RESPONSE, res->type());

  const StunErrorCodeAttribute* err = res->GetErrorCode();
  ASSERT_TRUE(err != NULL);
  EXPECT_EQ(6, err->eclass());
  EXPECT_EQ(0, err->number());
  EXPECT_EQ("Operation Not Supported", err->reason());

  // Also verify that traffic from the external client is ignored.
  SendRaw2(msg2, static_cast<int>(strlen(msg2)));
  EXPECT_TRUE(ReceiveRaw1().empty());
}

}  // namespace cricket
