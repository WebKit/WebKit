/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/dcsctp_socket.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/chunk/cookie_echo_chunk.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/chunk/error_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_ack_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_request_chunk.h"
#include "net/dcsctp/packet/chunk/idata_chunk.h"
#include "net/dcsctp/packet/chunk/init_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/chunk/shutdown_chunk.h"
#include "net/dcsctp/packet/error_cause/error_cause.h"
#include "net/dcsctp/packet/error_cause/unrecognized_chunk_type_cause.h"
#include "net/dcsctp/packet/parameter/heartbeat_info_parameter.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/rx/reassembly_queue.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr SendOptions kSendOptions;
constexpr size_t kLargeMessageSize = DcSctpOptions::kMaxSafeMTUSize * 20;

MATCHER_P(HasDataChunkWithSsn, ssn, "") {
  absl::optional<SctpPacket> packet = SctpPacket::Parse(arg);
  if (!packet.has_value()) {
    *result_listener << "data didn't parse as an SctpPacket";
    return false;
  }

  if (packet->descriptors()[0].type != DataChunk::kType) {
    *result_listener << "the first chunk in the packet is not a data chunk";
    return false;
  }

  absl::optional<DataChunk> dc =
      DataChunk::Parse(packet->descriptors()[0].data);
  if (!dc.has_value()) {
    *result_listener << "The first chunk didn't parse as a data chunk";
    return false;
  }

  if (dc->ssn() != ssn) {
    *result_listener << "the ssn is " << *dc->ssn();
    return false;
  }

  return true;
}

MATCHER_P(HasDataChunkWithMid, mid, "") {
  absl::optional<SctpPacket> packet = SctpPacket::Parse(arg);
  if (!packet.has_value()) {
    *result_listener << "data didn't parse as an SctpPacket";
    return false;
  }

  if (packet->descriptors()[0].type != IDataChunk::kType) {
    *result_listener << "the first chunk in the packet is not an i-data chunk";
    return false;
  }

  absl::optional<IDataChunk> dc =
      IDataChunk::Parse(packet->descriptors()[0].data);
  if (!dc.has_value()) {
    *result_listener << "The first chunk didn't parse as an i-data chunk";
    return false;
  }

  if (dc->message_id() != mid) {
    *result_listener << "the mid is " << *dc->message_id();
    return false;
  }

  return true;
}

MATCHER_P(HasSackWithCumAckTsn, tsn, "") {
  absl::optional<SctpPacket> packet = SctpPacket::Parse(arg);
  if (!packet.has_value()) {
    *result_listener << "data didn't parse as an SctpPacket";
    return false;
  }

  if (packet->descriptors()[0].type != SackChunk::kType) {
    *result_listener << "the first chunk in the packet is not a data chunk";
    return false;
  }

  absl::optional<SackChunk> sc =
      SackChunk::Parse(packet->descriptors()[0].data);
  if (!sc.has_value()) {
    *result_listener << "The first chunk didn't parse as a data chunk";
    return false;
  }

  if (sc->cumulative_tsn_ack() != tsn) {
    *result_listener << "the cum_ack_tsn is " << *sc->cumulative_tsn_ack();
    return false;
  }

  return true;
}

MATCHER(HasSackWithNoGapAckBlocks, "") {
  absl::optional<SctpPacket> packet = SctpPacket::Parse(arg);
  if (!packet.has_value()) {
    *result_listener << "data didn't parse as an SctpPacket";
    return false;
  }

  if (packet->descriptors()[0].type != SackChunk::kType) {
    *result_listener << "the first chunk in the packet is not a data chunk";
    return false;
  }

  absl::optional<SackChunk> sc =
      SackChunk::Parse(packet->descriptors()[0].data);
  if (!sc.has_value()) {
    *result_listener << "The first chunk didn't parse as a data chunk";
    return false;
  }

  if (!sc->gap_ack_blocks().empty()) {
    *result_listener << "there are gap ack blocks";
    return false;
  }

  return true;
}

TSN AddTo(TSN tsn, int delta) {
  return TSN(*tsn + delta);
}

DcSctpOptions MakeOptionsForTest(bool enable_message_interleaving) {
  DcSctpOptions options;
  // To make the interval more predictable in tests.
  options.heartbeat_interval_include_rtt = false;
  options.enable_message_interleaving = enable_message_interleaving;
  return options;
}

class DcSctpSocketTest : public testing::Test {
 protected:
  explicit DcSctpSocketTest(bool enable_message_interleaving = false)
      : options_(MakeOptionsForTest(enable_message_interleaving)),
        cb_a_("A"),
        cb_z_("Z"),
        sock_a_("A", cb_a_, nullptr, options_),
        sock_z_("Z", cb_z_, nullptr, options_) {}

  void AdvanceTime(DurationMs duration) {
    cb_a_.AdvanceTime(duration);
    cb_z_.AdvanceTime(duration);
  }

  static void ExchangeMessages(DcSctpSocket& sock_a,
                               MockDcSctpSocketCallbacks& cb_a,
                               DcSctpSocket& sock_z,
                               MockDcSctpSocketCallbacks& cb_z) {
    bool delivered_packet = false;
    do {
      delivered_packet = false;
      std::vector<uint8_t> packet_from_a = cb_a.ConsumeSentPacket();
      if (!packet_from_a.empty()) {
        delivered_packet = true;
        sock_z.ReceivePacket(std::move(packet_from_a));
      }
      std::vector<uint8_t> packet_from_z = cb_z.ConsumeSentPacket();
      if (!packet_from_z.empty()) {
        delivered_packet = true;
        sock_a.ReceivePacket(std::move(packet_from_z));
      }
    } while (delivered_packet);
  }

  void RunTimers(MockDcSctpSocketCallbacks& cb, DcSctpSocket& socket) {
    for (;;) {
      absl::optional<TimeoutID> timeout_id = cb.GetNextExpiredTimeout();
      if (!timeout_id.has_value()) {
        break;
      }
      socket.HandleTimeout(*timeout_id);
    }
  }

  void RunTimers() {
    RunTimers(cb_a_, sock_a_);
    RunTimers(cb_z_, sock_z_);
  }

  // Calls Connect() on `sock_a_` and make the connection established.
  void ConnectSockets() {
    EXPECT_CALL(cb_a_, OnConnected).Times(1);
    EXPECT_CALL(cb_z_, OnConnected).Times(1);

    sock_a_.Connect();
    // Z reads INIT, INIT_ACK, COOKIE_ECHO, COOKIE_ACK
    sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
    sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
    sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
    sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

    EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
    EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
  }

  const DcSctpOptions options_;
  testing::NiceMock<MockDcSctpSocketCallbacks> cb_a_;
  testing::NiceMock<MockDcSctpSocketCallbacks> cb_z_;
  DcSctpSocket sock_a_;
  DcSctpSocket sock_z_;
};

TEST_F(DcSctpSocketTest, EstablishConnection) {
  EXPECT_CALL(cb_a_, OnConnected).Times(1);
  EXPECT_CALL(cb_z_, OnConnected).Times(1);
  EXPECT_CALL(cb_a_, OnConnectionRestarted).Times(0);
  EXPECT_CALL(cb_z_, OnConnectionRestarted).Times(0);

  sock_a_.Connect();
  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads COOKIE_ACK.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, EstablishConnectionWithSetupCollision) {
  EXPECT_CALL(cb_a_, OnConnected).Times(1);
  EXPECT_CALL(cb_z_, OnConnected).Times(1);
  EXPECT_CALL(cb_a_, OnConnectionRestarted).Times(0);
  EXPECT_CALL(cb_z_, OnConnectionRestarted).Times(0);
  sock_a_.Connect();
  sock_z_.Connect();

  ExchangeMessages(sock_a_, cb_a_, sock_z_, cb_z_);

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, ShuttingDownWhileEstablishingConnection) {
  EXPECT_CALL(cb_a_, OnConnected).Times(0);
  EXPECT_CALL(cb_z_, OnConnected).Times(1);
  sock_a_.Connect();

  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // Drop COOKIE_ACK, just to more easily verify shutdown protocol.
  cb_z_.ConsumeSentPacket();

  // As Socket A has received INIT_ACK, it has a TCB and is connected, while
  // Socket Z needs to receive COOKIE_ECHO to get there. Socket A still has
  // timers running at this point.
  EXPECT_EQ(sock_a_.state(), SocketState::kConnecting);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);

  // Socket A is now shut down, which should make it stop those timers.
  sock_a_.Shutdown();

  EXPECT_CALL(cb_a_, OnClosed).Times(1);
  EXPECT_CALL(cb_z_, OnClosed).Times(1);

  // Z reads SHUTDOWN, produces SHUTDOWN_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads SHUTDOWN_ACK, produces SHUTDOWN_COMPLETE
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads SHUTDOWN_COMPLETE.
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  EXPECT_TRUE(cb_a_.ConsumeSentPacket().empty());
  EXPECT_TRUE(cb_z_.ConsumeSentPacket().empty());

  EXPECT_EQ(sock_a_.state(), SocketState::kClosed);
  EXPECT_EQ(sock_z_.state(), SocketState::kClosed);
}

TEST_F(DcSctpSocketTest, EstablishSimultaneousConnection) {
  EXPECT_CALL(cb_a_, OnConnected).Times(1);
  EXPECT_CALL(cb_z_, OnConnected).Times(1);
  EXPECT_CALL(cb_a_, OnConnectionRestarted).Times(0);
  EXPECT_CALL(cb_z_, OnConnectionRestarted).Times(0);
  sock_a_.Connect();

  // INIT isn't received by Z, as it wasn't ready yet.
  cb_a_.ConsumeSentPacket();

  sock_z_.Connect();

  // A reads INIT, produces INIT_ACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  // Z reads INIT_ACK, sends COOKIE_ECHO
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  // A reads COOKIE_ECHO - establishes connection.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);

  // Proceed with the remaining packets.
  ExchangeMessages(sock_a_, cb_a_, sock_z_, cb_z_);

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, EstablishConnectionLostCookieAck) {
  EXPECT_CALL(cb_a_, OnConnected).Times(1);
  EXPECT_CALL(cb_z_, OnConnected).Times(1);
  EXPECT_CALL(cb_a_, OnConnectionRestarted).Times(0);
  EXPECT_CALL(cb_z_, OnConnectionRestarted).Times(0);

  sock_a_.Connect();
  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // COOKIE_ACK is lost.
  cb_z_.ConsumeSentPacket();

  EXPECT_EQ(sock_a_.state(), SocketState::kConnecting);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);

  // This will make A re-send the COOKIE_ECHO
  AdvanceTime(DurationMs(options_.t1_cookie_timeout));
  RunTimers();

  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads COOKIE_ACK.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, ResendInitAndEstablishConnection) {
  sock_a_.Connect();
  // INIT is never received by Z.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket init_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(init_packet.descriptors()[0].type, InitChunk::kType);

  AdvanceTime(options_.t1_init_timeout);
  RunTimers();

  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads COOKIE_ACK.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, ResendingInitTooManyTimesAborts) {
  sock_a_.Connect();

  // INIT is never received by Z.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket init_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(init_packet.descriptors()[0].type, InitChunk::kType);

  for (int i = 0; i < options_.max_init_retransmits; ++i) {
    AdvanceTime(options_.t1_init_timeout * (1 << i));
    RunTimers();

    // INIT is resent
    ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket resent_init_packet,
                                SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
    EXPECT_EQ(resent_init_packet.descriptors()[0].type, InitChunk::kType);
  }

  // Another timeout, after the max init retransmits.
  AdvanceTime(options_.t1_init_timeout * (1 << options_.max_init_retransmits));
  EXPECT_CALL(cb_a_, OnAborted).Times(1);
  RunTimers();

  EXPECT_EQ(sock_a_.state(), SocketState::kClosed);
}

TEST_F(DcSctpSocketTest, ResendCookieEchoAndEstablishConnection) {
  sock_a_.Connect();

  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  // COOKIE_ECHO is never received by Z.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket init_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(init_packet.descriptors()[0].type, CookieEchoChunk::kType);

  AdvanceTime(options_.t1_init_timeout);
  RunTimers();

  // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads COOKIE_ACK.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);
}

TEST_F(DcSctpSocketTest, ResendingCookieEchoTooManyTimesAborts) {
  sock_a_.Connect();

  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  // COOKIE_ECHO is never received by Z.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket init_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(init_packet.descriptors()[0].type, CookieEchoChunk::kType);

  for (int i = 0; i < options_.max_init_retransmits; ++i) {
    AdvanceTime(options_.t1_cookie_timeout * (1 << i));
    RunTimers();

    // COOKIE_ECHO is resent
    ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket resent_init_packet,
                                SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
    EXPECT_EQ(resent_init_packet.descriptors()[0].type, CookieEchoChunk::kType);
  }

  // Another timeout, after the max init retransmits.
  AdvanceTime(options_.t1_cookie_timeout *
              (1 << options_.max_init_retransmits));
  EXPECT_CALL(cb_a_, OnAborted).Times(1);
  RunTimers();

  EXPECT_EQ(sock_a_.state(), SocketState::kClosed);
}

TEST_F(DcSctpSocketTest, ShutdownConnection) {
  ConnectSockets();

  RTC_LOG(LS_INFO) << "Shutting down";

  sock_a_.Shutdown();
  // Z reads SHUTDOWN, produces SHUTDOWN_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // A reads SHUTDOWN_ACK, produces SHUTDOWN_COMPLETE
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // Z reads SHUTDOWN_COMPLETE.
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kClosed);
  EXPECT_EQ(sock_z_.state(), SocketState::kClosed);
}

TEST_F(DcSctpSocketTest, ShutdownTimerExpiresTooManyTimeClosesConnection) {
  ConnectSockets();

  sock_a_.Shutdown();
  // Drop first SHUTDOWN packet.
  cb_a_.ConsumeSentPacket();

  EXPECT_EQ(sock_a_.state(), SocketState::kShuttingDown);

  for (int i = 0; i < options_.max_retransmissions; ++i) {
    AdvanceTime(DurationMs(options_.rto_initial * (1 << i)));
    RunTimers();

    // Dropping every shutdown chunk.
    ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket packet,
                                SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
    EXPECT_EQ(packet.descriptors()[0].type, ShutdownChunk::kType);
    EXPECT_TRUE(cb_a_.ConsumeSentPacket().empty());
  }
  // The last expiry, makes it abort the connection.
  AdvanceTime(options_.rto_initial * (1 << options_.max_retransmissions));
  EXPECT_CALL(cb_a_, OnAborted).Times(1);
  RunTimers();

  EXPECT_EQ(sock_a_.state(), SocketState::kClosed);
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(packet.descriptors()[0].type, AbortChunk::kType);
  EXPECT_TRUE(cb_a_.ConsumeSentPacket().empty());
}

TEST_F(DcSctpSocketTest, EstablishConnectionWhileSendingData) {
  sock_a_.Connect();

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2}), kSendOptions);

  // Z reads INIT, produces INIT_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // // A reads INIT_ACK, produces COOKIE_ECHO
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
  // // Z reads COOKIE_ECHO, produces COOKIE_ACK
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // // A reads COOKIE_ACK.
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  EXPECT_EQ(sock_a_.state(), SocketState::kConnected);
  EXPECT_EQ(sock_z_.state(), SocketState::kConnected);

  absl::optional<DcSctpMessage> msg = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));
}

TEST_F(DcSctpSocketTest, SendMessageAfterEstablished) {
  ConnectSockets();

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2}), kSendOptions);
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  absl::optional<DcSctpMessage> msg = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));
}

TEST_F(DcSctpSocketTest, TimeoutResendsPacket) {
  ConnectSockets();

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2}), kSendOptions);
  cb_a_.ConsumeSentPacket();

  RTC_LOG(LS_INFO) << "Advancing time";
  AdvanceTime(options_.rto_initial);
  RunTimers();

  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  absl::optional<DcSctpMessage> msg = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));
}

TEST_F(DcSctpSocketTest, SendALotOfBytesMissedSecondPacket) {
  ConnectSockets();

  std::vector<uint8_t> payload(kLargeMessageSize);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), kSendOptions);

  // First DATA
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // Second DATA (lost)
  cb_a_.ConsumeSentPacket();

  // Retransmit and handle the rest
  ExchangeMessages(sock_a_, cb_a_, sock_z_, cb_z_);

  absl::optional<DcSctpMessage> msg = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));
  EXPECT_THAT(msg->payload(), testing::ElementsAreArray(payload));
}

TEST_F(DcSctpSocketTest, SendingHeartbeatAnswersWithAck) {
  ConnectSockets();

  // Inject a HEARTBEAT chunk
  SctpPacket::Builder b(sock_a_.verification_tag(), DcSctpOptions());
  uint8_t info[] = {1, 2, 3, 4};
  Parameters::Builder params_builder;
  params_builder.Add(HeartbeatInfoParameter(info));
  b.Add(HeartbeatRequestChunk(params_builder.Build()));
  sock_a_.ReceivePacket(b.Build());

  // HEARTBEAT_ACK is sent as a reply. Capture it.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket ack_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  ASSERT_THAT(ack_packet.descriptors(), SizeIs(1));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      HeartbeatAckChunk ack,
      HeartbeatAckChunk::Parse(ack_packet.descriptors()[0].data));
  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatInfoParameter info_param, ack.info());
  EXPECT_THAT(info_param.info(), ElementsAre(1, 2, 3, 4));
}

TEST_F(DcSctpSocketTest, ExpectHeartbeatToBeSent) {
  ConnectSockets();

  EXPECT_THAT(cb_a_.ConsumeSentPacket(), IsEmpty());

  AdvanceTime(options_.heartbeat_interval);
  RunTimers();

  std::vector<uint8_t> hb_packet_raw = cb_a_.ConsumeSentPacket();
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket hb_packet,
                              SctpPacket::Parse(hb_packet_raw));
  ASSERT_THAT(hb_packet.descriptors(), SizeIs(1));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      HeartbeatRequestChunk hb,
      HeartbeatRequestChunk::Parse(hb_packet.descriptors()[0].data));
  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatInfoParameter info_param, hb.info());

  // The info is a single 64-bit number.
  EXPECT_THAT(hb.info()->info(), SizeIs(8));

  // Feed it to Sock-z and expect a HEARTBEAT_ACK that will be propagated back.
  sock_z_.ReceivePacket(hb_packet_raw);
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
}

TEST_F(DcSctpSocketTest, CloseConnectionAfterTooManyLostHeartbeats) {
  ConnectSockets();

  EXPECT_THAT(cb_a_.ConsumeSentPacket(), testing::IsEmpty());
  // Force-close socket Z so that it doesn't interfere from now on.
  sock_z_.Close();

  DurationMs time_to_next_hearbeat = options_.heartbeat_interval;

  for (int i = 0; i < options_.max_retransmissions; ++i) {
    RTC_LOG(LS_INFO) << "Letting HEARTBEAT interval timer expire - sending...";
    AdvanceTime(time_to_next_hearbeat);
    RunTimers();

    // Dropping every heartbeat.
    ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket hb_packet,
                                SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
    EXPECT_EQ(hb_packet.descriptors()[0].type, HeartbeatRequestChunk::kType);

    RTC_LOG(LS_INFO) << "Letting the heartbeat expire.";
    AdvanceTime(DurationMs(1000));
    RunTimers();

    time_to_next_hearbeat = options_.heartbeat_interval - DurationMs(1000);
  }

  RTC_LOG(LS_INFO) << "Letting HEARTBEAT interval timer expire - sending...";
  AdvanceTime(time_to_next_hearbeat);
  RunTimers();

  // Last heartbeat
  EXPECT_THAT(cb_a_.ConsumeSentPacket(), Not(IsEmpty()));

  EXPECT_CALL(cb_a_, OnAborted).Times(1);
  // Should suffice as exceeding RTO
  AdvanceTime(DurationMs(1000));
  RunTimers();
}

TEST_F(DcSctpSocketTest, RecoversAfterASuccessfulAck) {
  ConnectSockets();

  EXPECT_THAT(cb_a_.ConsumeSentPacket(), testing::IsEmpty());
  // Force-close socket Z so that it doesn't interfere from now on.
  sock_z_.Close();

  DurationMs time_to_next_hearbeat = options_.heartbeat_interval;

  for (int i = 0; i < options_.max_retransmissions; ++i) {
    AdvanceTime(time_to_next_hearbeat);
    RunTimers();

    // Dropping every heartbeat.
    cb_a_.ConsumeSentPacket();

    RTC_LOG(LS_INFO) << "Letting the heartbeat expire.";
    AdvanceTime(DurationMs(1000));
    RunTimers();

    time_to_next_hearbeat = options_.heartbeat_interval - DurationMs(1000);
  }

  RTC_LOG(LS_INFO) << "Getting the last heartbeat - and acking it";
  AdvanceTime(time_to_next_hearbeat);
  RunTimers();

  std::vector<uint8_t> hb_packet_raw = cb_a_.ConsumeSentPacket();
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket hb_packet,
                              SctpPacket::Parse(hb_packet_raw));
  ASSERT_THAT(hb_packet.descriptors(), SizeIs(1));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      HeartbeatRequestChunk hb,
      HeartbeatRequestChunk::Parse(hb_packet.descriptors()[0].data));

  SctpPacket::Builder b(sock_a_.verification_tag(), options_);
  b.Add(HeartbeatAckChunk(std::move(hb).extract_parameters()));
  sock_a_.ReceivePacket(b.Build());

  // Should suffice as exceeding RTO - which will not fire.
  EXPECT_CALL(cb_a_, OnAborted).Times(0);
  AdvanceTime(DurationMs(1000));
  RunTimers();
  EXPECT_THAT(cb_a_.ConsumeSentPacket(), IsEmpty());

  // Verify that we get new heartbeats again.
  RTC_LOG(LS_INFO) << "Expecting a new heartbeat";
  AdvanceTime(time_to_next_hearbeat);
  RunTimers();

  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket another_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  EXPECT_EQ(another_packet.descriptors()[0].type, HeartbeatRequestChunk::kType);
}

TEST_F(DcSctpSocketTest, ResetStream) {
  ConnectSockets();

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2}), {});
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  absl::optional<DcSctpMessage> msg = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));

  // Handle SACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  // Reset the outgoing stream. This will directly send a RE-CONFIG.
  sock_a_.ResetStreams(std::vector<StreamID>({StreamID(1)}));

  // Receiving the packet will trigger a callback, indicating that A has
  // reset its stream. It will also send a RE-CONFIG with a response.
  EXPECT_CALL(cb_z_, OnIncomingStreamsReset).Times(1);
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  // Receiving a response will trigger a callback. Streams are now reset.
  EXPECT_CALL(cb_a_, OnStreamsResetPerformed).Times(1);
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
}

TEST_F(DcSctpSocketTest, ResetStreamWillMakeChunksStartAtZeroSsn) {
  ConnectSockets();

  std::vector<uint8_t> payload(options_.mtu - 100);

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), {});
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), {});

  auto packet1 = cb_a_.ConsumeSentPacket();
  EXPECT_THAT(packet1, HasDataChunkWithSsn(SSN(0)));
  sock_z_.ReceivePacket(packet1);

  auto packet2 = cb_a_.ConsumeSentPacket();
  EXPECT_THAT(packet2, HasDataChunkWithSsn(SSN(1)));
  sock_z_.ReceivePacket(packet2);

  // Handle SACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  absl::optional<DcSctpMessage> msg1 = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->stream_id(), StreamID(1));

  absl::optional<DcSctpMessage> msg2 = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg2.has_value());
  EXPECT_EQ(msg2->stream_id(), StreamID(1));

  // Reset the outgoing stream. This will directly send a RE-CONFIG.
  sock_a_.ResetStreams(std::vector<StreamID>({StreamID(1)}));
  // RE-CONFIG, req
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // RE-CONFIG, resp
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), {});

  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), {});

  auto packet3 = cb_a_.ConsumeSentPacket();
  EXPECT_THAT(packet3, HasDataChunkWithSsn(SSN(0)));
  sock_z_.ReceivePacket(packet3);

  auto packet4 = cb_a_.ConsumeSentPacket();
  EXPECT_THAT(packet4, HasDataChunkWithSsn(SSN(1)));
  sock_z_.ReceivePacket(packet4);

  // Handle SACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());
}

TEST_F(DcSctpSocketTest, OnePeerReconnects) {
  ConnectSockets();

  EXPECT_CALL(cb_a_, OnConnectionRestarted).Times(1);
  // Let's be evil here - reconnect while a fragmented packet was about to be
  // sent. The receiving side should get it in full.
  std::vector<uint8_t> payload(kLargeMessageSize);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), kSendOptions);

  // First DATA
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  // Create a new association, z2 - and don't use z anymore.
  testing::NiceMock<MockDcSctpSocketCallbacks> cb_z2("Z2");
  DcSctpSocket sock_z2("Z2", cb_z2, nullptr, options_);

  sock_z2.Connect();

  // Retransmit and handle the rest. As there will be some chunks in-flight that
  // have the wrong verification tag, those will yield errors.
  ExchangeMessages(sock_a_, cb_a_, sock_z2, cb_z2);

  absl::optional<DcSctpMessage> msg = cb_z2.ConsumeReceivedMessage();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->stream_id(), StreamID(1));
  EXPECT_THAT(msg->payload(), testing::ElementsAreArray(payload));
}

TEST_F(DcSctpSocketTest, SendMessageWithLimitedRtx) {
  ConnectSockets();

  SendOptions send_options;
  send_options.max_retransmissions = 0;
  std::vector<uint8_t> payload(options_.mtu - 100);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(51), payload), send_options);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(52), payload), send_options);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), send_options);

  // First DATA
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());
  // Second DATA (lost)
  cb_a_.ConsumeSentPacket();
  // Third DATA
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  // Handle SACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  // Now the missing data chunk will be marked as nacked, but it might still be
  // in-flight and the reported gap could be due to out-of-order delivery. So
  // the RetransmissionQueue will not mark it as "to be retransmitted" until
  // after the t3-rtx timer has expired.
  AdvanceTime(options_.rto_initial);
  RunTimers();

  // The chunk will be marked as retransmitted, and then as abandoned, which
  // will trigger a FORWARD-TSN to be sent.

  // FORWARD-TSN (third)
  sock_z_.ReceivePacket(cb_a_.ConsumeSentPacket());

  // The receiver might have moved into delayed ack mode.
  AdvanceTime(options_.rto_initial);
  RunTimers();

  // Handle SACK
  sock_a_.ReceivePacket(cb_z_.ConsumeSentPacket());

  absl::optional<DcSctpMessage> msg1 = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->ppid(), PPID(51));

  absl::optional<DcSctpMessage> msg2 = cb_z_.ConsumeReceivedMessage();
  ASSERT_TRUE(msg2.has_value());
  EXPECT_EQ(msg2->ppid(), PPID(53));

  absl::optional<DcSctpMessage> msg3 = cb_z_.ConsumeReceivedMessage();
  EXPECT_FALSE(msg3.has_value());
}

struct FakeChunkConfig : ChunkConfig {
  static constexpr int kType = 0x49;
  static constexpr size_t kHeaderSize = 4;
  static constexpr int kVariableLengthAlignment = 0;
};

class FakeChunk : public Chunk, public TLVTrait<FakeChunkConfig> {
 public:
  FakeChunk() {}

  FakeChunk(FakeChunk&& other) = default;
  FakeChunk& operator=(FakeChunk&& other) = default;

  void SerializeTo(std::vector<uint8_t>& out) const override {
    AllocateTLV(out);
  }
  std::string ToString() const override { return "FAKE"; }
};

TEST_F(DcSctpSocketTest, ReceivingUnknownChunkRespondsWithError) {
  ConnectSockets();

  // Inject a FAKE chunk
  SctpPacket::Builder b(sock_a_.verification_tag(), DcSctpOptions());
  b.Add(FakeChunk());
  sock_a_.ReceivePacket(b.Build());

  // ERROR is sent as a reply. Capture it.
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket reply_packet,
                              SctpPacket::Parse(cb_a_.ConsumeSentPacket()));
  ASSERT_THAT(reply_packet.descriptors(), SizeIs(1));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      ErrorChunk error, ErrorChunk::Parse(reply_packet.descriptors()[0].data));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UnrecognizedChunkTypeCause cause,
      error.error_causes().get<UnrecognizedChunkTypeCause>());
  EXPECT_THAT(cause.unrecognized_chunk(), ElementsAre(0x49, 0x00, 0x00, 0x04));
}

TEST_F(DcSctpSocketTest, ReceivingErrorChunkReportsAsCallback) {
  ConnectSockets();

  // Inject a ERROR chunk
  SctpPacket::Builder b(sock_a_.verification_tag(), DcSctpOptions());
  b.Add(
      ErrorChunk(Parameters::Builder()
                     .Add(UnrecognizedChunkTypeCause({0x49, 0x00, 0x00, 0x04}))
                     .Build()));

  EXPECT_CALL(cb_a_, OnError(ErrorKind::kPeerReported,
                             HasSubstr("Unrecognized Chunk Type")));
  sock_a_.ReceivePacket(b.Build());
}

TEST_F(DcSctpSocketTest, PassingHighWatermarkWillOnlyAcceptCumAckTsn) {
  // Create a new association, z2 - and don't use z anymore.
  testing::NiceMock<MockDcSctpSocketCallbacks> cb_z2("Z2");
  DcSctpOptions options = options_;
  options.max_receiver_window_buffer_size = 100;
  DcSctpSocket sock_z2("Z2", cb_z2, nullptr, options);

  EXPECT_CALL(cb_z2, OnClosed).Times(0);
  EXPECT_CALL(cb_z2, OnAborted).Times(0);

  sock_a_.Connect();
  std::vector<uint8_t> init_data = cb_a_.ConsumeSentPacket();
  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket init_packet,
                              SctpPacket::Parse(init_data));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      InitChunk init_chunk,
      InitChunk::Parse(init_packet.descriptors()[0].data));
  sock_z2.ReceivePacket(init_data);
  sock_a_.ReceivePacket(cb_z2.ConsumeSentPacket());
  sock_z2.ReceivePacket(cb_a_.ConsumeSentPacket());
  sock_a_.ReceivePacket(cb_z2.ConsumeSentPacket());

  // Fill up Z2 to the high watermark limit.
  TSN tsn = init_chunk.initial_tsn();
  AnyDataChunk::Options opts;
  opts.is_beginning = Data::IsBeginning(true);
  sock_z2.ReceivePacket(
      SctpPacket::Builder(sock_z2.verification_tag(), options)
          .Add(DataChunk(tsn, StreamID(1), SSN(0), PPID(53),
                         std::vector<uint8_t>(
                             100 * ReassemblyQueue::kHighWatermarkLimit + 1),
                         opts))
          .Build());

  // First DATA will always trigger a SACK. It's not interesting.
  EXPECT_THAT(cb_z2.ConsumeSentPacket(),
              AllOf(HasSackWithCumAckTsn(tsn), HasSackWithNoGapAckBlocks()));

  // This DATA should be accepted - it's advancing cum ack tsn.
  sock_z2.ReceivePacket(SctpPacket::Builder(sock_z2.verification_tag(), options)
                            .Add(DataChunk(AddTo(tsn, 1), StreamID(1), SSN(0),
                                           PPID(53), std::vector<uint8_t>(1),
                                           /*options=*/{}))
                            .Build());

  // The receiver might have moved into delayed ack mode.
  cb_z2.AdvanceTime(options.rto_initial);
  RunTimers(cb_z2, sock_z2);

  EXPECT_THAT(
      cb_z2.ConsumeSentPacket(),
      AllOf(HasSackWithCumAckTsn(AddTo(tsn, 1)), HasSackWithNoGapAckBlocks()));

  // This DATA will not be accepted - it's not advancing cum ack tsn.
  sock_z2.ReceivePacket(SctpPacket::Builder(sock_z2.verification_tag(), options)
                            .Add(DataChunk(AddTo(tsn, 3), StreamID(1), SSN(0),
                                           PPID(53), std::vector<uint8_t>(1),
                                           /*options=*/{}))
                            .Build());

  // Sack will be sent in IMMEDIATE mode when this is happening.
  EXPECT_THAT(
      cb_z2.ConsumeSentPacket(),
      AllOf(HasSackWithCumAckTsn(AddTo(tsn, 1)), HasSackWithNoGapAckBlocks()));

  // This DATA will not be accepted either.
  sock_z2.ReceivePacket(SctpPacket::Builder(sock_z2.verification_tag(), options)
                            .Add(DataChunk(AddTo(tsn, 4), StreamID(1), SSN(0),
                                           PPID(53), std::vector<uint8_t>(1),
                                           /*options=*/{}))
                            .Build());

  // Sack will be sent in IMMEDIATE mode when this is happening.
  EXPECT_THAT(
      cb_z2.ConsumeSentPacket(),
      AllOf(HasSackWithCumAckTsn(AddTo(tsn, 1)), HasSackWithNoGapAckBlocks()));

  // This DATA should be accepted, and it fills the reassembly queue.
  sock_z2.ReceivePacket(SctpPacket::Builder(sock_z2.verification_tag(), options)
                            .Add(DataChunk(AddTo(tsn, 2), StreamID(1), SSN(0),
                                           PPID(53), std::vector<uint8_t>(10),
                                           /*options=*/{}))
                            .Build());

  // The receiver might have moved into delayed ack mode.
  cb_z2.AdvanceTime(options.rto_initial);
  RunTimers(cb_z2, sock_z2);

  EXPECT_THAT(
      cb_z2.ConsumeSentPacket(),
      AllOf(HasSackWithCumAckTsn(AddTo(tsn, 2)), HasSackWithNoGapAckBlocks()));

  EXPECT_CALL(cb_z2, OnAborted(ErrorKind::kResourceExhaustion, _));
  EXPECT_CALL(cb_z2, OnClosed).Times(0);

  // This DATA will make the connection close. It's too full now.
  sock_z2.ReceivePacket(SctpPacket::Builder(sock_z2.verification_tag(), options)
                            .Add(DataChunk(AddTo(tsn, 3), StreamID(1), SSN(0),
                                           PPID(53), std::vector<uint8_t>(10),
                                           /*options=*/{}))
                            .Build());
}

TEST_F(DcSctpSocketTest, SetMaxMessageSize) {
  sock_a_.SetMaxMessageSize(42u);
  EXPECT_EQ(sock_a_.options().max_message_size, 42u);
}

TEST_F(DcSctpSocketTest, SendsMessagesWithLowLifetime) {
  ConnectSockets();

  // Mock that the time always goes forward.
  TimeMs now(0);
  EXPECT_CALL(cb_a_, TimeMillis).WillRepeatedly([&]() {
    now += DurationMs(3);
    return now;
  });
  EXPECT_CALL(cb_z_, TimeMillis).WillRepeatedly([&]() {
    now += DurationMs(3);
    return now;
  });

  // Queue a few small messages with low lifetime, both ordered and unordered,
  // and validate that all are delivered.
  static constexpr int kIterations = 100;
  for (int i = 0; i < kIterations; ++i) {
    SendOptions send_options;
    send_options.unordered = IsUnordered((i % 2) == 0);
    send_options.lifetime = DurationMs(i % 3);  // 0, 1, 2 ms

    sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2}), send_options);
  }

  ExchangeMessages(sock_a_, cb_a_, sock_z_, cb_z_);

  for (int i = 0; i < kIterations; ++i) {
    EXPECT_TRUE(cb_z_.ConsumeReceivedMessage().has_value());
  }

  EXPECT_FALSE(cb_z_.ConsumeReceivedMessage().has_value());

  // Validate that the sockets really make the time move forward.
  EXPECT_GE(*now, kIterations * 2);
}

TEST_F(DcSctpSocketTest, DiscardsMessagesWithLowLifetimeIfMustBuffer) {
  ConnectSockets();

  SendOptions lifetime_0;
  lifetime_0.unordered = IsUnordered(true);
  lifetime_0.lifetime = DurationMs(0);

  SendOptions lifetime_1;
  lifetime_1.unordered = IsUnordered(true);
  lifetime_1.lifetime = DurationMs(1);

  // Mock that the time always goes forward.
  TimeMs now(0);
  EXPECT_CALL(cb_a_, TimeMillis).WillRepeatedly([&]() {
    now += DurationMs(3);
    return now;
  });
  EXPECT_CALL(cb_z_, TimeMillis).WillRepeatedly([&]() {
    now += DurationMs(3);
    return now;
  });

  // Fill up the send buffer with a large message.
  std::vector<uint8_t> payload(kLargeMessageSize);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), payload), kSendOptions);

  // And queue a few small messages with lifetime=0 or 1 ms - can't be sent.
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {1, 2, 3}), lifetime_0);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {4, 5, 6}), lifetime_1);
  sock_a_.Send(DcSctpMessage(StreamID(1), PPID(53), {7, 8, 9}), lifetime_0);

  // Handle all that was sent until congestion window got full.
  for (;;) {
    std::vector<uint8_t> packet_from_a = cb_a_.ConsumeSentPacket();
    if (packet_from_a.empty()) {
      break;
    }
    sock_z_.ReceivePacket(std::move(packet_from_a));
  }

  // Shouldn't be enough to send that large message.
  EXPECT_FALSE(cb_z_.ConsumeReceivedMessage().has_value());

  // Exchange the rest of the messages, with the time ever increasing.
  ExchangeMessages(sock_a_, cb_a_, sock_z_, cb_z_);

  // The large message should be delivered. It was sent reliably.
  ASSERT_HAS_VALUE_AND_ASSIGN(DcSctpMessage m1, cb_z_.ConsumeReceivedMessage());
  EXPECT_EQ(m1.stream_id(), StreamID(1));
  EXPECT_THAT(m1.payload(), SizeIs(kLargeMessageSize));

  // But none of the smaller messages.
  EXPECT_FALSE(cb_z_.ConsumeReceivedMessage().has_value());
}

}  // namespace
}  // namespace dcsctp
