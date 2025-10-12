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
#include "test/gmock.h"
#include "test/gtest.h"

namespace {

using ::testing::MockFunction;

TEST(PacketTransportInternal,
     NotifyPacketReceivedPassthrougPacketToRegisteredListener) {
  webrtc::FakePacketTransport packet_transport("test");
  MockFunction<void(webrtc::PacketTransportInternal*,
                    const webrtc::ReceivedIpPacket&)>
      receiver;

  packet_transport.RegisterReceivedPacketCallback(&receiver,
                                                  receiver.AsStdFunction());
  EXPECT_CALL(receiver, Call)
      .WillOnce([](webrtc::PacketTransportInternal*,
                   const webrtc::ReceivedIpPacket& packet) {
        EXPECT_EQ(packet.decryption_info(),
                  webrtc::ReceivedIpPacket::kDtlsDecrypted);
      });
  packet_transport.NotifyPacketReceived(webrtc::ReceivedIpPacket(
      {}, webrtc::SocketAddress(), std::nullopt, webrtc::EcnMarking::kNotEct,
      webrtc::ReceivedIpPacket::kDtlsDecrypted));

  packet_transport.DeregisterReceivedPacketCallback(&receiver);
}

TEST(PacketTransportInternal, NotifiesOnceOnClose) {
  webrtc::FakePacketTransport packet_transport("test");
  int call_count = 0;
  packet_transport.SetOnCloseCallback([&]() { ++call_count; });
  ASSERT_EQ(call_count, 0);
  packet_transport.NotifyOnClose();
  EXPECT_EQ(call_count, 1);
  packet_transport.NotifyOnClose();
  EXPECT_EQ(call_count, 1);  // Call count should not have increased.
}

}  // namespace
