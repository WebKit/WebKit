/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/async_stun_tcp_socket.h"

#include <stdint.h>
#include <string.h>

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

namespace cricket {

static unsigned char kStunMessageWithZeroLength[] = {
    0x00, 0x01, 0x00, 0x00,  // length of 0 (last 2 bytes)
    0x21, 0x12, 0xA4, 0x42, '0', '1', '2', '3',
    '4',  '5',  '6',  '7',  '8', '9', 'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithZeroLength[] = {
    0x40, 0x00, 0x00, 0x00,  // length of 0 (last 2 bytes)
};

static unsigned char kTurnChannelDataMessage[] = {
    0x40, 0x00, 0x00, 0x10, 0x21, 0x12, 0xA4, 0x42, '0', '1',
    '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  'a', 'b',
};

static unsigned char kStunMessageWithInvalidLength[] = {
    0x00, 0x01, 0x00, 0x10, 0x21, 0x12, 0xA4, 0x42, '0', '1',
    '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithInvalidLength[] = {
    0x80, 0x00, 0x00, 0x20, 0x21, 0x12, 0xA4, 0x42, '0', '1',
    '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  'a', 'b',
};

static unsigned char kTurnChannelDataMessageWithOddLength[] = {
    0x40, 0x00, 0x00, 0x05, 0x21, 0x12, 0xA4, 0x42, '0',
};

static const rtc::SocketAddress kClientAddr("11.11.11.11", 0);
static const rtc::SocketAddress kServerAddr("22.22.22.22", 0);

class AsyncStunServerTCPSocket : public rtc::AsyncTcpListenSocket {
 public:
  explicit AsyncStunServerTCPSocket(std::unique_ptr<rtc::Socket> socket)
      : AsyncTcpListenSocket(std::move(socket)) {}
  void HandleIncomingConnection(rtc::Socket* socket) override {
    SignalNewConnection(this, new AsyncStunTCPSocket(socket));
  }
};

class AsyncStunTCPSocketTest : public ::testing::Test,
                               public sigslot::has_slots<> {
 protected:
  AsyncStunTCPSocketTest()
      : vss_(new rtc::VirtualSocketServer()), thread_(vss_.get()) {}

  virtual void SetUp() { CreateSockets(); }

  void CreateSockets() {
    std::unique_ptr<rtc::Socket> server =
        absl::WrapUnique(vss_->CreateSocket(kServerAddr.family(), SOCK_STREAM));
    server->Bind(kServerAddr);
    listen_socket_ =
        std::make_unique<AsyncStunServerTCPSocket>(std::move(server));
    listen_socket_->SignalNewConnection.connect(
        this, &AsyncStunTCPSocketTest::OnNewConnection);

    rtc::Socket* client = vss_->CreateSocket(kClientAddr.family(), SOCK_STREAM);
    send_socket_.reset(AsyncStunTCPSocket::Create(
        client, kClientAddr, listen_socket_->GetLocalAddress()));
    send_socket_->SignalSentPacket.connect(
        this, &AsyncStunTCPSocketTest::OnSentPacket);
    ASSERT_TRUE(send_socket_.get() != NULL);
    vss_->ProcessMessagesUntilIdle();
  }

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet) {
    recv_packets_.push_back(
        std::string(reinterpret_cast<const char*>(packet.payload().data()),
                    packet.payload().size()));
  }

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& packet) {
    ++sent_packets_;
  }

  void OnNewConnection(rtc::AsyncListenSocket* /*server*/,
                       rtc::AsyncPacketSocket* new_socket) {
    recv_socket_ = absl::WrapUnique(new_socket);
    new_socket->RegisterReceivedPacketCallback(
        [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
          OnReadPacket(socket, packet);
        });
  }

  bool Send(const void* data, size_t len) {
    rtc::PacketOptions options;
    int ret =
        send_socket_->Send(reinterpret_cast<const char*>(data), len, options);
    vss_->ProcessMessagesUntilIdle();
    return (ret == static_cast<int>(len));
  }

  bool CheckData(const void* data, int len) {
    bool ret = false;
    if (recv_packets_.size()) {
      std::string packet = recv_packets_.front();
      recv_packets_.pop_front();
      ret = (memcmp(data, packet.c_str(), len) == 0);
    }
    return ret;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<AsyncStunTCPSocket> send_socket_;
  std::unique_ptr<rtc::AsyncListenSocket> listen_socket_;
  std::unique_ptr<rtc::AsyncPacketSocket> recv_socket_;
  std::list<std::string> recv_packets_;
  int sent_packets_ = 0;
};

// Testing a stun packet sent/recv properly.
TEST_F(AsyncStunTCPSocketTest, TestSingleStunPacket) {
  EXPECT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kStunMessageWithZeroLength,
                        sizeof(kStunMessageWithZeroLength)));
}

// Verify sending multiple packets.
TEST_F(AsyncStunTCPSocketTest, TestMultipleStunPackets) {
  EXPECT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(4u, recv_packets_.size());
}

TEST_F(AsyncStunTCPSocketTest, ProcessInputHandlesMultiplePackets) {
  send_socket_->RegisterReceivedPacketCallback(
      [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
        recv_packets_.push_back(
            std::string(reinterpret_cast<const char*>(packet.payload().data()),
                        packet.payload().size()));
      });
  rtc::Buffer buffer;
  buffer.AppendData(kStunMessageWithZeroLength,
                    sizeof(kStunMessageWithZeroLength));
  // ChannelData message MUST be padded to
  // a multiple of four bytes.
  const unsigned char kTurnChannelData[] = {
      0x40, 0x00, 0x00, 0x04, 0x21, 0x12, 0xA4, 0x42,
  };
  buffer.AppendData(kTurnChannelData, sizeof(kTurnChannelData));

  send_socket_->ProcessInput(buffer);
  EXPECT_EQ(2u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kStunMessageWithZeroLength,
                        sizeof(kStunMessageWithZeroLength)));
  EXPECT_TRUE(CheckData(kTurnChannelData, sizeof(kTurnChannelData)));
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
  EXPECT_TRUE(Send(kTurnChannelDataMessage, sizeof(kTurnChannelDataMessage)));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(
      CheckData(kTurnChannelDataMessage, sizeof(kTurnChannelDataMessage)));
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
  unsigned char packet[65539];
  packet[0] = 0x40;
  packet[1] = 0x00;
  packet[2] = 0xFF;
  packet[3] = 0xFF;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Verifying a legal large stun message.
TEST_F(AsyncStunTCPSocketTest, TestMaximumSizeStunPacket) {
  unsigned char packet[65552];
  packet[0] = 0x00;
  packet[1] = 0x01;
  packet[2] = 0xFF;
  packet[3] = 0xFC;
  EXPECT_TRUE(Send(packet, sizeof(packet)));
}

// Test that a turn message is sent completely even if it exceeds the socket
// send buffer capacity.
TEST_F(AsyncStunTCPSocketTest, TestWithSmallSendBuffer) {
  vss_->set_send_buffer_capacity(1);
  Send(kTurnChannelDataMessageWithOddLength,
       sizeof(kTurnChannelDataMessageWithOddLength));
  EXPECT_EQ(1u, recv_packets_.size());
  EXPECT_TRUE(CheckData(kTurnChannelDataMessageWithOddLength,
                        sizeof(kTurnChannelDataMessageWithOddLength)));
}

// Test that SignalSentPacket is fired when a packet is sent.
TEST_F(AsyncStunTCPSocketTest, SignalSentPacketFiredWhenPacketSent) {
  ASSERT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(1, sent_packets_);
  // Send another packet for good measure.
  ASSERT_TRUE(
      Send(kStunMessageWithZeroLength, sizeof(kStunMessageWithZeroLength)));
  EXPECT_EQ(2, sent_packets_);
}

// Test that SignalSentPacket isn't fired when a packet isn't sent (for
// example, because it's invalid).
TEST_F(AsyncStunTCPSocketTest, SignalSentPacketNotFiredWhenPacketNotSent) {
  // Attempt to send a packet that's too small; since it isn't sent,
  // SignalSentPacket shouldn't fire.
  char data[1];
  ASSERT_FALSE(Send(data, sizeof(data)));
  EXPECT_EQ(0, sent_packets_);
}

}  // namespace cricket
