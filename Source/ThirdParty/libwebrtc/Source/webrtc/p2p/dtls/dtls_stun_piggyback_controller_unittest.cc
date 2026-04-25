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
#include "rtc_base/checks.h"
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

std::vector<uint32_t> FromAckAttribute(webrtc::ArrayView<uint8_t> attr) {
  webrtc::ByteBufferReader ack_reader(attr);
  std::vector<uint32_t> values;
  uint32_t value;
  while (ack_reader.ReadUInt32(&value)) {
    values.push_back(value);
  }
  RTC_DCHECK_EQ(ack_reader.Length(), 0);
  return values;
}

std::vector<uint8_t> FakeDtlsPacket(uint16_t packet_number) {
  auto packet = dtls_flight1;
  packet[17] = static_cast<uint8_t>(packet_number >> 8);
  packet[18] = static_cast<uint8_t>(packet_number & 255);
  return packet;
}

std::unique_ptr<webrtc::StunByteStringAttribute> WrapInStun(
    webrtc::IceAttributeType type,
    absl::string_view data) {
  return std::make_unique<webrtc::StunByteStringAttribute>(type, data);
}

std::unique_ptr<webrtc::StunByteStringAttribute> WrapInStun(
    webrtc::IceAttributeType type,
    const std::vector<uint8_t>& data) {
  return std::make_unique<webrtc::StunByteStringAttribute>(type, data.data(),
                                                           data.size());
}

std::unique_ptr<webrtc::StunByteStringAttribute> WrapInStun(
    webrtc::IceAttributeType type,
    const std::vector<uint32_t>& data) {
  return std::make_unique<webrtc::StunByteStringAttribute>(type, data);
}

}  // namespace

namespace webrtc {

using ::testing::ElementsAreArray;
using ::testing::MockFunction;
using ::testing::NotNull;
using State = DtlsStunPiggybackController::State;

class DtlsStunPiggybackControllerTest : public ::testing::Test {
 protected:
  DtlsStunPiggybackControllerTest()
      : client_(
            [this](ArrayView<const uint8_t> data) { ClientPacketSink(data); }),
        server_([this](ArrayView<const uint8_t> data) {
          ServerPacketSink(data);
        }) {}

  // Send from client to server embedded in STUN.
  void SendClientToServerEmbedded(const std::vector<uint8_t>& packet,
                                  StunMessageType type) {
    if (!packet.empty()) {
      client_.CapturePacket(packet);
      client_.Flush();
    } else {
      client_.ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    std::optional<ArrayView<uint8_t>> view_data;
    if (auto data = client_.GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
      view_data = attr_data->array_view();
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    std::optional<std::vector<uint32_t>> view_acks;
    if (auto ack = client_.GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
      view_acks = FromAckAttribute(attr_ack->array_view());
    }
    server_.ReportDataPiggybacked(view_data, view_acks);
  }
  // Send from client to server as plain DTLS.
  void SendClientToServerDtls(const std::vector<uint8_t> packet) {
    if (!packet.empty()) {
      client_.CapturePacket(packet);
      client_.Flush();
    } else {
      client_.ClearCachedPacketForTesting();
    }
    server_.ReportDtlsPacket(packet);
  }
  // Send from server to client embedded in STUN
  void SendServerToClientEmbedded(const std::vector<uint8_t>& packet,
                                  StunMessageType type) {
    if (!packet.empty()) {
      server_.CapturePacket(packet);
      server_.Flush();
    } else {
      server_.ClearCachedPacketForTesting();
    }
    std::unique_ptr<StunByteStringAttribute> attr_data;
    std::optional<ArrayView<uint8_t>> view_data;
    if (auto data = server_.GetDataToPiggyback(type)) {
      attr_data = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, *data);
      view_data = attr_data->array_view();
    }
    std::unique_ptr<StunByteStringAttribute> attr_ack;
    std::optional<std::vector<uint32_t>> view_acks;
    if (auto ack = server_.GetAckToPiggyback(type)) {
      attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
      view_acks = FromAckAttribute(attr_ack->array_view());
    }
    client_.ReportDataPiggybacked(view_data, view_acks);
    MaybeSetHandshakeComplete(packet);
  }
  // Send from server to client as plain DTLS.
  void SendServerToClientDtls(const std::vector<uint8_t> packet) {
    if (!packet.empty()) {
      server_.CapturePacket(packet);
      server_.Flush();
    } else {
      server_.ClearCachedPacketForTesting();
    }
    client_.ReportDtlsPacket(packet);
    MaybeSetHandshakeComplete(packet);
  }

  void DisableSupport(DtlsStunPiggybackController& client_or_server) {
    ASSERT_EQ(client_or_server.state(), State::TENTATIVE);
    client_or_server.ReportDataPiggybacked(std::nullopt, std::nullopt);
    ASSERT_EQ(client_or_server.state(), State::OFF);
  }

  DtlsStunPiggybackController client_;
  DtlsStunPiggybackController server_;

  MOCK_METHOD(void, ClientPacketSink, (ArrayView<const uint8_t>));
  MOCK_METHOD(void, ServerPacketSink, (ArrayView<const uint8_t>));

 private:
  void MaybeSetHandshakeComplete(std::vector<uint8_t> packet) {
    // Note: this assumes DTLS 1.2
    if (packet == dtls_flight4) {
      // After sending flight 4, the server handshake is complete.
      server_.SetDtlsHandshakeComplete(/*is_client=*/false,
                                       /*is_dtls13=*/false);
      // When receiving flight 4, client handshake is complete.
      client_.SetDtlsHandshakeComplete(/*is_client=*/true, /*is_dtls13=*/false);
    }
  }
};

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshake) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, FirstClientPacketLost) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 2+3
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 4
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServer) {
  DisableSupport(server_);

  // Flight 1
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServerClientReceives) {
  DisableSupport(server_);

  // Client to server got lost (or arrives late)
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByClient) {
  DisableSupport(client_);

  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, SomeRequestsDoNotGoThrough) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 1+2, server sent request got lost.
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendClientToServerEmbedded(empty, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, LossOnPostHandshakeAck) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK. Client to server gets lost
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
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
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));

  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  EXPECT_THAT(*client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight2)}));

  // Flight 3+4
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
  EXPECT_THAT(*client_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight2),
                  ComputeDtlsPacketHash(dtls_flight4),
              }));

  // Post-handshake ACK
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_F(DtlsStunPiggybackControllerTest, UnwrappedHandshakeAckData) {
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::vector<uint32_t>({}));

  // Flight 1+2 (embedded)
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClientEmbedded(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  EXPECT_THAT(*client_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight2)}));

  // Flight 3+4 (not embedded)
  SendClientToServerDtls(dtls_flight3);
  SendServerToClientDtls(dtls_flight4);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
  EXPECT_THAT(*client_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight2),
                  ComputeDtlsPacketHash(dtls_flight4),
              }));

  // Post-handshake ACK
  SendServerToClientEmbedded(empty, STUN_BINDING_REQUEST);
  SendClientToServerEmbedded(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_F(DtlsStunPiggybackControllerTest, AckDataNoDuplicates) {
  // Flight 1+2
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
  SendClientToServerEmbedded(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));

  // Receive Flight 1 again, no change expected.
  SendClientToServerEmbedded(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
                  ComputeDtlsPacketHash(dtls_flight1),
                  ComputeDtlsPacketHash(dtls_flight3),
              }));
}

TEST_F(DtlsStunPiggybackControllerTest, AckDataNoDuplicatesFromDualReporting) {
  std::unique_ptr<StunByteStringAttribute> attr_data =
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1);
  std::unique_ptr<StunByteStringAttribute> attr_ack;
  if (auto ack = client_.GetAckToPiggyback(STUN_BINDING_REQUEST)) {
    attr_ack = WrapInStun(STUN_ATTR_META_DTLS_IN_STUN_ACK, *ack);
  }
  ASSERT_THAT(attr_ack, NotNull());
  server_.ReportDataPiggybacked(attr_data->array_view(),
                                FromAckAttribute(attr_ack->array_view()));
  server_.ReportDtlsPacket(dtls_flight1);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({ComputeDtlsPacketHash(dtls_flight1)}));
}

TEST_F(DtlsStunPiggybackControllerTest, IgnoresNonDtlsData) {
  std::vector<uint8_t> ascii = {0x64, 0x72, 0x6f, 0x70, 0x6d, 0x65};

  EXPECT_CALL(*this, ServerPacketSink).Times(0);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, ascii)->array_view(),
      std::nullopt);
  EXPECT_EQ(0, server_.GetCountOfReceivedData());
}

TEST_F(DtlsStunPiggybackControllerTest, DontSendAckedPackets) {
  server_.CapturePacket(dtls_flight1);
  server_.Flush();
  EXPECT_TRUE(server_.GetDataToPiggyback(STUN_BINDING_REQUEST).has_value());
  server_.ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1)}));
  // No unacked packet exists.
  EXPECT_FALSE(server_.GetDataToPiggyback(STUN_BINDING_REQUEST).has_value());
}

TEST_F(DtlsStunPiggybackControllerTest, LimitAckSize) {
  std::vector<uint8_t> dtls_flight5 = FakeDtlsPacket(0x5487);

  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight1)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 1u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight2)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 2u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight3)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 3u);
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight4)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(), 4u);

  // Limit size of ack so that it does not grow unbounded.
  server_.ReportDataPiggybacked(
      WrapInStun(STUN_ATTR_META_DTLS_IN_STUN, dtls_flight5)->array_view(),
      std::nullopt);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST)->size(),
            DtlsStunPiggybackController::kMaxAckSize);
  EXPECT_THAT(*server_.GetAckToPiggyback(STUN_BINDING_REQUEST),
              ElementsAreArray({
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
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1)}));

  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight3.begin(), dtls_flight3.end()));

  server_.ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight3)}));

  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
  EXPECT_EQ(server_.GetDataToPiggyback(STUN_BINDING_REQUEST),
            std::string(dtls_flight2.begin(), dtls_flight2.end()));
}

TEST_F(DtlsStunPiggybackControllerTest, DuplicateAck) {
  server_.CapturePacket(dtls_flight1);
  server_.Flush();
  server_.ReportDataPiggybacked(
      std::nullopt,
      std::vector<uint32_t>({ComputeDtlsPacketHash(dtls_flight1),
                             ComputeDtlsPacketHash(dtls_flight1)}));
}

}  // namespace webrtc
