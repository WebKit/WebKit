/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/byte_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace {
// Extracted from a stock DTLS call using Wireshark.
// Each packet (apart from the last) is truncated to
// the first fragment to keep things short.

// Based on a "server hello done" but with different msg_seq.
const std::vector<uint8_t> dtls_flight1 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x01,                                            // seq=1
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00,  // msg_seq=0x1234
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight2 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x02,                                            // seq=2
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x43, 0x21, 0x00,  // msg_seq=0x4321
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight3 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x03,                                            // seq=3
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00,  // msg_seq=0x4444
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> dtls_flight4 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
    0x00, 0x04,                                            // seq=4
    0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x54, 0x86, 0x00,  // msg_seq=0x5486
    0x00, 0x00, 0x00, 0x00, 0x00};

const std::vector<uint8_t> empty = {};

std::string AsAckAttribute(const std::vector<uint32_t>& list) {
  webrtc::ByteBufferWriter writer;
  for (const auto& val : list) {
    writer.WriteUInt32(val);
  }
  return std::string(writer.DataAsStringView());
}

std::vector<uint8_t> FakeDtlsPacket(uint16_t packet_number) {
  auto packet = dtls_flight1;
  packet[17] = static_cast<uint8_t>(packet_number >> 8);
  packet[18] = static_cast<uint8_t>(packet_number & 255);
  return packet;
}

}  // namespace

namespace webrtc {

using ::testing::MockFunction;
using State = DtlsStunPiggybackController::State;

class DtlsStunPiggybackControllerTest : public ::testing::Test {
 protected:
  DtlsStunPiggybackControllerTest()
      : client_(
            [this](ArrayView<const uint8_t> data) { ClientPacketSink(data); }),
        server_([this](ArrayView<const uint8_t> data) {
          ServerPacketSink(data);
        }) {}

  void SendClientToServer(const std::vector<uint8_t> packet,
                          StunMessageType type) {
    if (!packet.empty()) {
      client_.CapturePacket(packet);
      client_.Flush();
    } else {
      client_.ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (auto data = client_.GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    if (auto ack = client_.GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
    }
    server_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
  }
  void SendServerToClient(const std::vector<uint8_t> packet,
                          StunMessageType type) {
    if (!packet.empty()) {
      server_.CapturePacket(packet);
      server_.Flush();
    } else {
      server_.ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (auto data = server_.GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    if (auto ack = server_.GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
    }
    client_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
    if (packet == dtls_flight4) {
      // After sending flight 4, the server handshake is complete.
      server_.SetDtlsHandshakeComplete(/*is_client=*/false,
                                       /*is_dtls13=*/false);
      // When receiving flight 4, client handshake is complete.
      client_.SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
    }
  }

  std::unique_ptr<StunByteStringAttribute> WrapInStun(IceAttributeType type,
                                                      absl::string_view data) {
    return std::make_unique<StunByteStringAttribute>(type, data);
  }

  std::unique_ptr<StunByteStringAttribute> WrapInStun(
      IceAttributeType type,
      const std::vector<uint8_t>& data) {
    return std::make_unique<StunByteStringAttribute>(type, data.data(),
                                                     data.size());
  }

  void DisableSupport(DtlsStunPiggybackController& client_or_server) {
    ASSERT_EQ(client_or_server.state(), State::TENTATIVE);
    client_or_server.ReportDataPiggybacked(nullptr, nullptr);
    ASSERT_EQ(client_or_server.state(), State::OFF);
  }

  DtlsStunPiggybackController client_;
  DtlsStunPiggybackController server_;

  MOCK_METHOD(void, ClientPacketSink, (ArrayView<const uint8_t>));
  MOCK_METHOD(void, ServerPacketSink, (ArrayView<const uint8_t>));
};

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshake) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, FirstClientPacketLost) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 2+3
  SendServerToClient(dtls_flight2, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight3, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 4
  SendServerToClient(dtls_flight4, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServer) {
  DisableSupport(server_);

  // Flight 1
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServerClientReceives) {
  DisableSupport(server_);

  // Client to server got lost (or arrives late)
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByClient) {
  DisableSupport(client_);

  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, SomeRequestsDoNotGoThrough) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 1+2, server sent request got lost.
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendClientToServer(empty, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, LossOnPostHandshakeAck) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK. Client to server gets lost
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest,
       UnsupportedStateAfterFallbackHandshakeRemainsOff) {
  DisableSupport(client_);
  DisableSupport(server_);

  // Set DTLS complete after normal handshake.
  client_.SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
  EXPECT_EQ(client_.state(), State::OFF);
  server_.SetDtlsHandshakeComplete(/*is_client=*/false, /*is_dtls13=*/false);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshakeAckData) {
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), "");
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), "");

  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({ComputeDtlsPacketHash(dtls_flight1)}));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            AsAckAttribute({ComputeDtlsPacketHash(dtls_flight2)}));

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            AsAckAttribute({
                ComputeDtlsPacketHash(dtls_flight1),
                ComputeDtlsPacketHash(dtls_flight3),
            }));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({
                ComputeDtlsPacketHash(dtls_flight2),
                ComputeDtlsPacketHash(dtls_flight4),
            }));

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_F(DtlsStunPiggybackControllerTest, AckDataNoDuplicates) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({ComputeDtlsPacketHash(dtls_flight1)}));
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({
                ComputeDtlsPacketHash(dtls_flight1),
                ComputeDtlsPacketHash(dtls_flight3),
            }));

  // Receive Flight 1 again, no change expected.
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({
                ComputeDtlsPacketHash(dtls_flight1),
                ComputeDtlsPacketHash(dtls_flight3),
            }));
}

TEST_F(DtlsStunPiggybackControllerTest, IgnoresNonDtlsData) {
  std::vector<uint8_t> ascii = {0x64, 0x72, 0x6f, 0x70, 0x6d, 0x65};

  EXPECT_CALL(*this, ServerPacketSink).Times(0);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, ascii).get(), nullptr);
  EXPECT_EQ(0, server_.GetCountOfReceivedData());
}

TEST_F(DtlsStunPiggybackControllerTest, DontSendAckedPackets) {
  server_.CapturePacket(dtls_flight1);
  server_.Flush();
  EXPECT_TRUE(server_.GetDataToPiggyback(STUN_BINDING_REQUEST).has_value());
  server_.ReportDataPiggybacked(
      nullptr, WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK,
                          AsAckAttribute({ComputeDtlsPacketHash(dtls_flight1)}))
                   .get());
  // No unacked packet exists.
  EXPECT_FALSE(server_.GetDataToPiggyback(STUN_BINDING_REQUEST).has_value());
}

TEST_F(DtlsStunPiggybackControllerTest, LimitAckSize) {
  std::vector<uint8_t> dtls_flight5 = FakeDtlsPacket(0x5487);

  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1).get(), nullptr);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 4u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight2).get(), nullptr);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 8u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight3).get(), nullptr);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 12u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight4).get(), nullptr);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 16u);

  // Limit size of ack so that it does not grow unbounded.
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight5).get(), nullptr);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(),
            DtlsStunPiggybackController::kMaxAckSize);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            AsAckAttribute({
                ComputeDtlsPacketHash(dtls_flight2),
                ComputeDtlsPacketHash(dtls_flight3),
                ComputeDtlsPacketHash(dtls_flight4),
                ComputeDtlsPacketHash(dtls_flight5),
            }));
}

TEST_F(DtlsStunPiggybackControllerTest, MultiPacketRoundRobin) {
  // Let's pretend that a flight is 3 packets...
  server_.CapturePacket(dtls_flight1);
  server_.CapturePacket(dtls_flight2);
  server_.CapturePacket(dtls_flight3);
  server_.Flush();
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight1.begin(), dtls_flight1.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight3.begin(), dtls_flight3.end()));

  server_.ReportDataPiggybacked(
      nullptr, WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK,
                          AsAckAttribute({ComputeDtlsPacketHash(dtls_flight1)}))
                   .get());

  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight3.begin(), dtls_flight3.end()));

  server_.ReportDataPiggybacked(
      nullptr, WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK,
                          AsAckAttribute({ComputeDtlsPacketHash(dtls_flight3)}))
                   .get());

  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
}

}  // namespace webrtc
