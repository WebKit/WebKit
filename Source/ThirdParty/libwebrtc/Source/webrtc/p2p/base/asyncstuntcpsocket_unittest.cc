/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/p2p/base/asyncstuntcpsocket.h"
#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/virtualsocketserver.h"

namespace cricket {

static unsigned char kStunMessageWithZeroLength[] = {
  0x00, 0x01, 0x00, 0x00,  // length of 0 (last 2 bytes)
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};


static unsigned char kTurnChannelDataMessageWithZeroLength[] = {
  0x40, 0x00, 0x00, 0x00,  // length of 0 (last 2 bytes)
};

static unsigned char kTurnChannelDataMessage[] = {
  0x40, 0x00, 0x00, 0x10,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kStunMessageWithInvalidLength[] = {
  0x00, 0x01, 0x00, 0x10,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithInvalidLength[] = {
  0x80, 0x00, 0x00, 0x20,
  0x21, 0x12, 0xA4, 0x42,
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithOddLength[] = {
  0x40, 0x00, 0x00, 0x05,
  0x21, 0x12, 0xA4, 0x42,
  '0',
};


static const rtc::SocketAddress kClientAddr("11.11.11.11", 0);
static const rtc::SocketAddress kServerAddr("22.22.22.22", 0);

class AsyncStunTCPSocketTest : public testing::Test,
                               public sigslot::has_slots<> {
 protected:
  AsyncStunTCPSocketTest()
      : vss_(new rtc::VirtualSocketServer(NULL)),
        ss_scope_(vss_.get()) {
  }

  virtual void SetUp() {
    CreateSockets();
  }

  void CreateSockets() {
    rtc::AsyncSocket* server = vss_->CreateAsyncSocket(
        kServerAddr.family(), SOCK_STREAM);
    server->Bind(kServerAddr);
    recv_socket_.reset(new AsyncStunTCPSocket(server, true));
    recv_socket_->SignalNewConnection.connect(
        this, &AsyncStunTCPSocketTest::OnNewConnection);

    rtc::AsyncSocket* client = vss_->CreateAsyncSocket(
        kClientAddr.family(), SOCK_STREAM);
    send_socket_.reset(AsyncStunTCPSocket::Create(
        client, kClientAddr, recv_socket_->GetLocalAddress()));
    ASSERT_TRUE(send_socket_.get() != NULL);
    vss_->ProcessMessagesUntilIdle();
  }

  void OnReadPacket(rtc::AsyncPacketSocket* socket, const char* data,
                    size_t len, const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time) {
    recv_packets_.push_back(std::string(data, len));
  }

  void OnNewConnection(rtc::AsyncPacketSocket* server,
                       rtc::AsyncPacketSocket* new_socket) {
    listen_socket_.reset(new_socket);
    new_socket->SignalReadPacket.connect(
        this, &AsyncStunTCPSocketTest::OnReadPacket);
  }

  bool Send(const void* data, size_t len) {
    rtc::PacketOptions options;
    size_t ret = send_socket_->Send(
        reinterpret_cast<const char*>(data), len, options);
    vss_->ProcessMessagesUntilIdle();
    return (ret == len);
  }

  bool CheckData(const void* data, int len) {
    bool ret = false;
    if (recv_packets_.size()) {
      std::string packet =  recv_packets_.front();
      recv_packets_.pop_front();
      ret = (memcmp(data, packet.c_str(), len) == 0);
    }
    return ret;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::SocketServerScope ss_scope_;
  std::unique_ptr<AsyncStunTCPSocket> send_socket_;
  std::unique_ptr<AsyncStunTCPSocket> recv_socket_;
  std::unique_ptr<rtc::AsyncPacketSocket> listen_socket_;
  std::list<std::string> recv_packets_;
};

// Testing a stun packet sent/recv properly.
TEST_F(AsyncStunTCPSocketTest, TestSingleStunPacket) {
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kStunMessageWithZeroLength,
                        sizeof(kStunMessageWithZeroLength)));
}

// Verify sending multiple packets.
TEST_F(AsyncStunTCPSocketTest, TestMultipleStunPackets) {
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(Send(kStunMessageWithZeroLength,
                   sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(4u, recv_packets_.size());
}

// Verifying TURN channel data message with zero length.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataWithZeroLength) {
  EXPECT_TRUE(Send(kTurnChannelDataMessageWithZeroLength,
                   sizeof(kTurnChannelDataMessageWithZeroLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithZeroLength,
                        sizeof(kTurnChannelDataMessageWithZeroLength)));
}

// Verifying TURN channel data message.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelData) {
  EXPECT_TRUE(Send(kTurnChannelDataMessage,
                   sizeof(kTurnChannelDataMessage)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessage,
                        sizeof(kTurnChannelDataMessage)));
}

// Verifying TURN channel messages which needs padding handled properly.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataPadding) {
  EXPECT_TRUE(Send(kTurnChannelDataMessageWithOddLength,
                   sizeof(kTurnChannelDataMessageWithOddLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithOddLength,
                        sizeof(kTurnChannelDataMessageWithOddLength)));
}

// Verifying stun message with invalid length.
TEST_F(AsyncStunTCPSocketTest, TestStunInvalidLength) {
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));
  EXPECT_EQ(0u, recv_packets_.size());

  // Modify the message length to larger value.
  kStunMessageWithInvalidLength[2] = 0xFF;
  kStunMessageWithInvalidLength[3] = 0xFF;
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));

  // Modify the message length to smaller value.
  kStunMessageWithInvalidLength[2] = 0x00;
  kStunMessageWithInvalidLength[3] = 0x01;
  EXPECT_FALSE(Send(kStunMessageWithInvalidLength,
                    sizeof(kStunMessageWithInvalidLength)));
}

// Verifying TURN channel data message with invalid length.
TEST_F(AsyncStunTCPSocketTest, TestTurnChannelDataWithInvalidLength) {
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));
  // Modify the length to larger value.
  kTurnChannelDataMessageWithInvalidLength[2] = 0xFF;
  kTurnChannelDataMessageWithInvalidLength[3] = 0xF0;
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));

  // Modify the length to smaller value.
  kTurnChannelDataMessageWithInvalidLength[2] = 0x00;
  kTurnChannelDataMessageWithInvalidLength[3] = 0x00;
  EXPECT_FALSE(Send(kTurnChannelDataMessageWithInvalidLength,
                   sizeof(kTurnChannelDataMessageWithInvalidLength)));
}

// Verifying a small buffer handled (dropped) properly. This will be
// a common one for both stun and turn.
TEST_F(AsyncStunTCPSocketTest, TestTooSmallMessageBuffer) {
  char data[1];
  EXPECT_FALSE(Send(data, sizeof(data)));
}

// Verifying a legal large turn message.
TEST_F(AsyncStunTCPSocketTest, TestMaximumSizeTurnPacket) {
  // We have problem in getting the SignalWriteEvent from the virtual socket
  // server. So increasing the send buffer to 64k.
  // TODO(mallinath) - Remove this setting after we fix vss issue.
  vss_->set_send_buffer_capacity(64 * 1024);
  unsigned char packet[65539];
  packet[0] = 0x40;
  packet[1] = 0x00;
  packet[2] = 0xFF;
  packet[3] = 0xFF;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Verifying a legal large stun message.
TEST_F(AsyncStunTCPSocketTest, TestMaximumSizeStunPacket) {
  // We have problem in getting the SignalWriteEvent from the virtual socket
  // server. So increasing the send buffer to 64k.
  // TODO(mallinath) - Remove this setting after we fix vss issue.
  vss_->set_send_buffer_capacity(64 * 1024);
  unsigned char packet[65552];
  packet[0] = 0x00;
  packet[1] = 0x01;
  packet[2] = 0xFF;
  packet[3] = 0xFC;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Investigate why WriteEvent is not signaled from VSS.
TEST_F(AsyncStunTCPSocketTest, DISABLED_TestWithSmallSendBuffer) {
  vss_->set_send_buffer_capacity(1);
  Send(kTurnChannelDataMessageWithOddLength,
       sizeof(kTurnChannelDataMessageWithOddLength));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithOddLength,
                        sizeof(kTurnChannelDataMessageWithOddLength)));
}

}  // namespace cricket
