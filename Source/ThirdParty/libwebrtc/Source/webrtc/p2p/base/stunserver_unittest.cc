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

#include "absl/memory/memory.h"
#include "p2p/base/stunserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/testclient.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

namespace cricket {

namespace {
const rtc::SocketAddress server_addr("99.99.99.1", 3478);
const rtc::SocketAddress client_addr("1.2.3.4", 1234);
}  // namespace

class StunServerTest : public testing::Test {
 public:
  StunServerTest() : ss_(new rtc::VirtualSocketServer()), network_(ss_.get()) {}
  virtual void SetUp() {
    server_.reset(
        new StunServer(rtc::AsyncUDPSocket::Create(ss_.get(), server_addr)));
    client_.reset(new rtc::TestClient(
        absl::WrapUnique(rtc::AsyncUDPSocket::Create(ss_.get(), client_addr))));

    network_.Start();
  }
  void Send(const StunMessage& msg) {
    rtc::ByteBufferWriter buf;
    msg.Write(&buf);
    Send(buf.Data(), static_cast<int>(buf.Length()));
  }
  void Send(const char* buf, int len) {
    client_->SendTo(buf, len, server_addr);
  }
  bool ReceiveFails() { return (client_->CheckNoPacket()); }
  StunMessage* Receive() {
    StunMessage* msg = NULL;
    std::unique_ptr<rtc::TestClient::Packet> packet =
        client_->NextPacket(rtc::TestClient::kTimeoutMs);
    if (packet) {
      rtc::ByteBufferReader buf(packet->buf, packet->size);
      msg = new StunMessage();
      msg->Read(&buf);
    }
    return msg;
  }

 private:
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::Thread network_;
  std::unique_ptr<StunServer> server_;
  std::unique_ptr<rtc::TestClient> client_;
};

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=2517 for details.
#if !defined(THREAD_SANITIZER)

TEST_F(StunServerTest, TestGood) {
  StunMessage req;
  std::string transaction_id = "0123456789ab";
  req.SetType(STUN_BINDING_REQUEST);
  req.SetTransactionID(transaction_id);
  Send(req);

  StunMessage* msg = Receive();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());
  EXPECT_EQ(req.transaction_id(), msg->transaction_id());

  const StunAddressAttribute* mapped_addr =
      msg->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  EXPECT_TRUE(mapped_addr != NULL);
  EXPECT_EQ(1, mapped_addr->family());
  EXPECT_EQ(client_addr.port(), mapped_addr->port());
  if (mapped_addr->ipaddr() != client_addr.ipaddr()) {
    RTC_LOG(LS_WARNING) << "Warning: mapped IP ("
                        << mapped_addr->ipaddr().ToString() << ") != local IP ("
                        << client_addr.ipaddr().ToString() << ")";
  }

  delete msg;
}

#endif  // if !defined(THREAD_SANITIZER)

TEST_F(StunServerTest, TestBad) {
  const char* bad =
      "this is a completely nonsensical message whose only "
      "purpose is to make the parser go 'ack'.  it doesn't "
      "look anything like a normal stun message";
  Send(bad, static_cast<int>(strlen(bad)));

  ASSERT_TRUE(ReceiveFails());
}

}  // namespace cricket
