/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/packet_transport_internal.h"

#include <optional>

#include "api/transport/ecn_marking.h"
#include "p2p/test/fake_packet_transport.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket_address.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::MockFunction;

TEST(PacketTransportInternal,
     NotifyPacketReceivedPassthrougPacketToRegisteredListener) {
  FakePacketTransport packet_transport(CreateTestEnvironment(), "test");
  MockFunction<void(PacketTransportInternal*, const ReceivedIpPacket&)>
      receiver;

  packet_transport.RegisterReceivedPacketCallback(&receiver,
                                                  receiver.AsStdFunction());
  EXPECT_CALL(receiver, Call)
      .WillOnce([](PacketTransportInternal*, const ReceivedIpPacket& packet) {
        EXPECT_EQ(packet.decryption_info(), ReceivedIpPacket::kDtlsDecrypted);
      });
  packet_transport.NotifyPacketReceived(
      ReceivedIpPacket({}, SocketAddress(), std::nullopt, EcnMarking::kNotEct,
                       ReceivedIpPacket::kDtlsDecrypted));

  packet_transport.DeregisterReceivedPacketCallback(&receiver);
}

TEST(PacketTransportInternal, NotifiesOnceOnClose) {
  FakePacketTransport packet_transport(CreateTestEnvironment(), "test");
  int call_count = 0;
  packet_transport.SetOnCloseCallback([&]() { ++call_count; });
  ASSERT_EQ(call_count, 0);
  packet_transport.NotifyOnClose();
  EXPECT_EQ(call_count, 1);
  packet_transport.NotifyOnClose();
  EXPECT_EQ(call_count, 1);  // Call count should not have increased.
}

TEST(PacketTransportInternal, UnsubscribeReceivingState) {
  FakePacketTransport packet_transport(CreateTestEnvironment(), "test");
  int call_count = 0;
  void* tag = &call_count;
  packet_transport.SubscribeReceivingState(
      tag, [&](PacketTransportInternal*) { ++call_count; });
  packet_transport.NotifyReceivingState(&packet_transport);
  EXPECT_EQ(call_count, 1);
  packet_transport.UnsubscribeReceivingState(tag);
  packet_transport.NotifyReceivingState(&packet_transport);
  EXPECT_EQ(call_count, 1);
}

}  // namespace

}  // namespace webrtc
