/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_udp_socket.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

#include "api/environment/environment.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/mock_socket.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::MockFunction;
using ::testing::NotNull;

TEST(AsyncUDPSocketTest, SetSocketOptionIfEctChange) {
  const SocketAddress kAddr("22.22.22.22", 0);
  VirtualSocketServer socket_server;
  std::unique_ptr<AsyncUDPSocket> udp_socket =
      AsyncUDPSocket::Create(CreateTestEnvironment(), kAddr, socket_server);
  ASSERT_THAT(udp_socket, NotNull());

  int ect = 0;
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  ASSERT_EQ(ect, 0);

  uint8_t buffer[] = "hello";
  AsyncSocketPacketOptions packet_options;
  packet_options.ect_1 = false;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);

  packet_options.ect_1 = true;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 1);

  packet_options.ect_1 = false;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);
}

TEST(AsyncUDPSocketTest, ArrivalTimeStampCanBeBeforeCurrentTime) {
  SimulatedClock webrtc_clock(Timestamp::Seconds(456));
  Environment env = CreateTestEnvironment({.time = &webrtc_clock});
  std::unique_ptr<MockSocket> socket = std::make_unique<MockSocket>();
  MockSocket* socket_ptr = socket.get();
  AsyncUDPSocket async_socket(env, std::move(socket));
  testing::MockFunction<void(AsyncPacketSocket*, const ReceivedIpPacket&)>
      received_packet_callback;
  async_socket.RegisterReceivedPacketCallback(
      received_packet_callback.AsStdFunction());

  const Timestamp kSocketEpoch = Timestamp::Seconds(123);
  EXPECT_CALL(*socket_ptr, RecvFrom(_))
      .WillRepeatedly([&](Socket::ReceiveBuffer& buffer) {
        buffer.payload = "hello";
        buffer.arrival_time = kSocketEpoch;
        return buffer.payload.size();
      });
  EXPECT_CALL(received_packet_callback, Call)
      .WillRepeatedly([&](AsyncPacketSocket*, const ReceivedIpPacket& packet) {
        EXPECT_EQ(packet.arrival_time(), webrtc_clock.CurrentTime());
      });
  socket_ptr->NotifyReadEvent(socket_ptr);

  // Let 10ms pass until next read event.
  webrtc_clock.AdvanceTime(TimeDelta::Millis(10));

  EXPECT_CALL(*socket_ptr, RecvFrom(_))
      .WillRepeatedly([&](Socket::ReceiveBuffer& buffer) {
        buffer.payload = "hello";
        // But only 5 ms has passed since the last received packet.
        buffer.arrival_time = kSocketEpoch + TimeDelta::Millis(5);
        return buffer.payload.size();
      });
  EXPECT_CALL(received_packet_callback, Call)
      .WillRepeatedly([&](AsyncPacketSocket*, const ReceivedIpPacket& packet) {
        EXPECT_EQ(packet.arrival_time(),
                  webrtc_clock.CurrentTime() - TimeDelta::Millis(5));
      });
  socket_ptr->NotifyReadEvent(socket_ptr);
}

TEST(AsyncUDPSocketTest, InitiallyBufferedPacketsGetSameArrivalTime) {
  SimulatedClock webrtc_clock(Timestamp::Seconds(456));
  Environment env = CreateTestEnvironment({.time = &webrtc_clock});
  std::unique_ptr<MockSocket> socket = std::make_unique<MockSocket>();
  MockSocket* socket_ptr = socket.get();
  AsyncUDPSocket async_socket(env, std::move(socket));
  testing::MockFunction<void(AsyncPacketSocket*, const ReceivedIpPacket&)>
      received_packet_callback;
  async_socket.RegisterReceivedPacketCallback(
      received_packet_callback.AsStdFunction());

  // Simulate that three packets are received with some time delta.
  const Timestamp kSocketEpoch = Timestamp::Seconds(123);
  std::deque<Timestamp> socket_arrival_times = {
      kSocketEpoch, kSocketEpoch + TimeDelta::Millis(10),
      kSocketEpoch + TimeDelta::Millis(20)};

  EXPECT_CALL(*socket_ptr, RecvFrom(_))
      .Times(3)
      .WillRepeatedly([&](Socket::ReceiveBuffer& buffer) {
        buffer.payload = "hello";
        buffer.arrival_time = socket_arrival_times.front();
        socket_arrival_times.pop_front();
        return buffer.payload.size();
      });

  EXPECT_CALL(received_packet_callback, Call)
      .Times(3)
      .WillRepeatedly([&](AsyncPacketSocket*, const ReceivedIpPacket& packet) {
        // Despite the packets being received at different times, they all have
        // the same timestamp.
        EXPECT_EQ(packet.arrival_time(), webrtc_clock.CurrentTime());
      });
  // But assume, CPU is blocked and can not read the packet at the pace they
  // arrive. Instead they are read one after each other a bit later.
  socket_ptr->NotifyReadEvent(socket_ptr);
  socket_ptr->NotifyReadEvent(socket_ptr);
  socket_ptr->NotifyReadEvent(socket_ptr);
}

TEST(AsyncUDPSocketTest, ArrivalTimeStampCanNotBeAfterCurrentTime) {
  SimulatedClock webrtc_clock(Timestamp::Seconds(456));
  Environment env = CreateTestEnvironment({.time = &webrtc_clock});
  std::unique_ptr<MockSocket> socket = std::make_unique<MockSocket>();
  MockSocket* socket_ptr = socket.get();
  AsyncUDPSocket async_socket(env, std::move(socket));
  testing::MockFunction<void(AsyncPacketSocket*, const ReceivedIpPacket&)>
      received_packet_callback;
  async_socket.RegisterReceivedPacketCallback(
      received_packet_callback.AsStdFunction());

  const Timestamp kSocketEpoch = Timestamp::Seconds(123);
  EXPECT_CALL(*socket_ptr, RecvFrom(_))
      .WillRepeatedly([&](Socket::ReceiveBuffer& buffer) {
        buffer.payload = "hello";
        buffer.arrival_time = kSocketEpoch;
        return buffer.payload.size();
      });
  EXPECT_CALL(received_packet_callback, Call)
      .WillRepeatedly([&](AsyncPacketSocket*, const ReceivedIpPacket& packet) {
        EXPECT_EQ(packet.arrival_time(), webrtc_clock.CurrentTime());
      });
  socket_ptr->NotifyReadEvent(socket_ptr);

  // Let 10ms pass until next read event.
  webrtc_clock.AdvanceTime(TimeDelta::Millis(10));

  EXPECT_CALL(*socket_ptr, RecvFrom(_))
      .WillRepeatedly([&](Socket::ReceiveBuffer& buffer) {
        buffer.payload = "hello";
        // But the next packet indicate it is received 25 ms after the previous.
        buffer.arrival_time = kSocketEpoch + TimeDelta::Millis(25);
        return buffer.payload.size();
      });
  EXPECT_CALL(received_packet_callback, Call)
      .WillRepeatedly([&](AsyncPacketSocket*, const ReceivedIpPacket& packet) {
        // We still expect the arrival time to be set no later than the current
        // time.
        EXPECT_EQ(packet.arrival_time(), webrtc_clock.CurrentTime());
      });
  socket_ptr->NotifyReadEvent(socket_ptr);
}

}  // namespace webrtc
