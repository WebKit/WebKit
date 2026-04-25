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

#include <cstddef>
#include <cstring>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/environment/environment.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::NotNull;

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

static const SocketAddress kClientAddr("11.11.11.11", 0);
static const SocketAddress kServerAddr("22.22.22.22", 0);

class AsyncStunServerTCPSocket : public AsyncTcpListenSocket {
 public:
  AsyncStunServerTCPSocket(const Environment& env,
                           std::unique_ptr<Socket> socket)
      : AsyncTcpListenSocket(env, std::move(socket)) {}
  void HandleIncomingConnection(std::unique_ptr<Socket> socket) override {
    NotifyNewConnection(this, new AsyncStunTCPSocket(env(), std::move(socket)));
  }
};

class AsyncStunTCPSocketTest : public ::testing::Test {
 protected:
  AsyncStunTCPSocketTest()
      : vss_(new VirtualSocketServer()), thread_(vss_.get()) {}

  void SetUp() override { CreateSockets(); }

  void CreateSockets() {
    const Environment env = CreateTestEnvironment();
    std::unique_ptr<Socket> server =
        vss_->Create(kServerAddr.family(), SOCK_STREAM);
    server->Bind(kServerAddr);
    listen_socket_ =
        std::make_unique<AsyncStunServerTCPSocket>(env, std::move(server));
    listen_socket_->SubscribeNewConnection(
        this, [this](AsyncListenSocket* listen_socket,
                     AsyncPacketSocket* packet_socket) {
          OnNewConnection(listen_socket, packet_socket);
        });

    std::unique_ptr<Socket> client =
        vss_->Create(kClientAddr.family(), SOCK_STREAM);
    ASSERT_THAT(client, NotNull());
    ASSERT_EQ(client->Bind(kClientAddr), 0);
    ASSERT_EQ(client->Connect(listen_socket_->GetLocalAddress()), 0);
    send_socket_ = std::make_unique<AsyncStunTCPSocket>(env, std::move(client));
    send_socket_->SubscribeSentPacket(
        this, [this](AsyncPacketSocket* socket, const SentPacketInfo& info) {
          OnSentPacket(socket, info);
        });
    vss_->ProcessMessagesUntilIdle();
  }

  void OnReadPacket(AsyncPacketSocket* /* socket */,
                    const ReceivedIpPacket& packet) {
    recv_packets_.push_back(
        std::string(reinterpret_cast<const char*>(packet.payload().data()),
                    packet.payload().size()));
  }

  void OnSentPacket(AsyncPacketSocket* /* socket */,
                    const SentPacketInfo& /* packet */) {
    ++sent_packets_;
  }

  void OnNewConnection(AsyncListenSocket* /*server*/,
                       AsyncPacketSocket* new_socket) {
    recv_socket_ = absl::WrapUnique(new_socket);
    new_socket->RegisterReceivedPacketCallback(
        [&](AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
          OnReadPacket(socket, packet);
        });
  }

  bool Send(const void* data, size_t len) {
    AsyncSocketPacketOptions options;
    int ret =
        send_socket_->Send(reinterpret_cast<const char*>(data), len, options);
    vss_->ProcessMessagesUntilIdle();
    return (ret == static_cast<int>(len));
  }

  bool CheckData(const void* data, int len) {
    bool ret = false;
    if (!recv_packets_.empty()) {
      std::string packet = recv_packets_.front();
      recv_packets_.pop_front();
      ret = (memcmp(data, packet.c_str(), len) == 0);
    }
    return ret;
  }

  std::unique_ptr<VirtualSocketServer> vss_;
  AutoSocketServerThread thread_;
  std::unique_ptr<AsyncStunTCPSocket> send_socket_;
  std::unique_ptr<AsyncListenSocket> listen_socket_;
  std::unique_ptr<AsyncPacketSocket> recv_socket_;
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
      [&](AsyncPacketSocket* /* socket */, const ReceivedIpPacket& packet) {
        recv_packets_.push_back(
            std::string(reinterpret_cast<const char*>(packet.payload().data()),
                        packet.payload().size()));
      });
  Buffer buffer;
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

}  // namespace webrtc
