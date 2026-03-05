/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/datagram_connection.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_datagram_connection_observer.h"
#include "api/transport/enums.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/test/fake_ice_transport.h"
#include "pc/datagram_connection_internal.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

using PacketSendParameters = DatagramConnection::PacketSendParameters;
using SendOutcome = DatagramConnection::Observer::SendOutcome;
using WireProtocol = DatagramConnection::WireProtocol;

class DatagramConnectionTest : public ::testing::Test,
                               public sigslot::has_slots<> {
 public:
  DatagramConnectionTest() : env_(CreateEnvironment()) {}

  ~DatagramConnectionTest() override {
    conn1_->Terminate([] {});
    conn2_->Terminate([] {});
  }

  void CreateConnections(WireProtocol wire_protocol = WireProtocol::kDtlsSrtp) {
    auto observer1 =
        std::make_unique<NiceMock<MockDatagramConnectionObserver>>();
    observer1_ptr_ = observer1.get();
    auto observer2 =
        std::make_unique<NiceMock<MockDatagramConnectionObserver>>();
    observer2_ptr_ = observer2.get();

    cert1_ = FakeRTCCertificateGenerator::GenerateCertificate();
    cert2_ = FakeRTCCertificateGenerator::GenerateCertificate();
    std::string transport_name1 = "FakeTransport1";
    std::string transport_name2 = "FakeTransport2";

    auto ice1 = std::make_unique<FakeIceTransport>(transport_name1,
                                                   ICE_CANDIDATE_COMPONENT_RTP);
    ice1->SetAsync(true);
    auto ice2 = std::make_unique<FakeIceTransport>(transport_name2,
                                                   ICE_CANDIDATE_COMPONENT_RTP);
    ice2->SetAsync(true);
    ice1_ = ice1.get();
    ice2_ = ice2.get();

    conn1_ = make_ref_counted<DatagramConnectionInternal>(
        env_, /*port_allocator=*/nullptr, transport_name1, true, cert1_,
        std::move(observer1), wire_protocol, std::move(ice1));

    conn2_ = make_ref_counted<DatagramConnectionInternal>(
        env_, /*port_allocator=*/nullptr, transport_name2, false, cert2_,
        std::move(observer2), wire_protocol, std::move(ice2));
  }

  void Connect() {
    auto fingerprint1 = SSLFingerprint::CreateFromCertificate(*cert1_);
    auto fingerprint2 = SSLFingerprint::CreateFromCertificate(*cert2_);

    conn1_->SetRemoteDtlsParameters(
        fingerprint2->algorithm, fingerprint2->digest.data(),
        fingerprint2->digest.size(), DatagramConnection::SSLRole::kClient);
    conn2_->SetRemoteDtlsParameters(
        fingerprint1->algorithm, fingerprint1->digest.data(),
        fingerprint1->digest.size(), DatagramConnection::SSLRole::kServer);

    ice1_->SetDestination(ice2_);
  }

 protected:
  AutoThread main_thread_;
  const Environment env_;
  NiceMock<MockDatagramConnectionObserver>* observer1_ptr_ = nullptr;
  NiceMock<MockDatagramConnectionObserver>* observer2_ptr_ = nullptr;
  scoped_refptr<RTCCertificate> cert1_;
  scoped_refptr<RTCCertificate> cert2_;
  scoped_refptr<DatagramConnectionInternal> conn1_;
  scoped_refptr<DatagramConnectionInternal> conn2_;
  FakeIceTransport* ice1_;
  FakeIceTransport* ice2_;
};

TEST_F(DatagramConnectionTest, CreateAndDestroy) {
  CreateConnections();
  EXPECT_TRUE(conn1_);
  EXPECT_TRUE(conn2_);
}

TEST_F(DatagramConnectionTest, TransportsBecomeWritable) {
  main_thread_.BlockingCall([&]() {
    CreateConnections();
    Connect();
    WAIT(conn1_->Writable() && conn2_->Writable(), 1000);
    EXPECT_TRUE(conn1_->Writable());
    EXPECT_TRUE(conn2_->Writable());
  });
}

TEST_F(DatagramConnectionTest, ObserverNotifiedOnWritableChange) {
  CreateConnections();
  EXPECT_FALSE(conn1_->Writable());

  Event event;
  EXPECT_CALL(*observer1_ptr_, OnWritableChange()).WillOnce([&]() {
    event.Set();
  });

  main_thread_.BlockingCall([&]() { Connect(); });

  WAIT(conn1_->Writable() && conn2_->Writable(), 1000);

  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
  EXPECT_TRUE(conn1_->Writable());
}

TEST_F(DatagramConnectionTest, ObserverCalledOnReceivedPacket) {
  CreateConnections();

  Event event;
  std::vector<uint8_t> packet_data = {1, 2, 3, 4};
  RtpPacketReceived packet;
  packet.SetPayload(packet_data);
  packet.set_arrival_time(Timestamp::Seconds(1234));

  EXPECT_CALL(*observer1_ptr_, OnPacketReceived(_, _))
      .WillOnce(
          [&](ArrayView<const uint8_t> data,
              const DatagramConnection::Observer::PacketMetadata& metadata) {
            EXPECT_EQ(data.size(), packet_data.size());
            EXPECT_EQ(
                memcmp(data.data(), packet_data.data(), packet_data.size()), 0);
            EXPECT_EQ(metadata.receive_time, packet.arrival_time());
            event.Set();
          });

  main_thread_.BlockingCall([&]() { conn1_->OnRtpPacket(packet); });

  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, PacketsAreSent) {
  // Calling SendPacket causes the packet to be sent on ice1_
  CreateConnections();
  Connect();
  WAIT(conn1_->Writable() && conn2_->Writable(), 1000);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  Event event;
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kSuccess);
        EXPECT_NE(outcome.send_time, Timestamp::MinusInfinity());
        event.Set();
      });
  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
  // Pull the RTP sequence number from ice1's last_sent_packet
  uint16_t seq_num = ParseRtpSequenceNumber(ice1_->last_sent_packet());
  EXPECT_EQ(seq_num, 0);
  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, PacketsAreReceived) {
  CreateConnections();
  Connect();
  WAIT(conn1_->Writable() && conn2_->Writable(), 1000);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  Event receive_event;
  EXPECT_CALL(*observer2_ptr_, OnPacketReceived(_, _))
      .WillOnce(
          [&](ArrayView<const uint8_t> received_data,
              const DatagramConnection::Observer::PacketMetadata& metadata) {
            EXPECT_EQ(received_data.size(), data.size());
            EXPECT_EQ(memcmp(received_data.data(), data.data(), data.size()),
                      0);
            EXPECT_NE(metadata.receive_time, Timestamp::Zero());
            receive_event.Set();
          });

  Event send_event;
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kSuccess);
        EXPECT_NE(outcome.send_time, Timestamp::MinusInfinity());
        send_event.Set();
      });

  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
  // Process the message queue to ensure the packet is sent.
  Thread::Current()->ProcessMessages(0);
  ASSERT_TRUE(receive_event.Wait(TimeDelta::Millis(1000)));
  ASSERT_TRUE(send_event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, SendPacketFailsWhenNotWritable) {
  CreateConnections();
  // Don't call Connect(), so the transports are not writable.
  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  EXPECT_FALSE(conn1_->Writable());
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kNotSent);
        EXPECT_EQ(outcome.send_time, Timestamp::MinusInfinity());
      });
  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
}

TEST_F(DatagramConnectionTest, SendPacketFailsWhenDtlsNotActive) {
  CreateConnections();
  // Set destination to make the transport channel writable, but don't set DTLS
  // parameters, so DTLS is not active.
  ice1_->SetDestination(ice2_);
  WAIT(ice1_->writable(), 1000);
  EXPECT_TRUE(ice1_->writable());
  EXPECT_FALSE(
      conn1_->Writable());  // Should be false because DTLS is not active.

  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kNotSent);
        EXPECT_EQ(outcome.send_time, Timestamp::MinusInfinity());
      });
  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
}

TEST_F(DatagramConnectionTest, OnCandidateGathered) {
  CreateConnections();

  Candidate candidate(ICE_CANDIDATE_COMPONENT_RTP, "udp",
                      SocketAddress("1.1.1.1", 1234), 100, "", "",
                      IceCandidateType::kHost, 0, "1");
  Event event;
  EXPECT_CALL(*observer1_ptr_, OnCandidateGathered(_))
      .WillOnce([&](const Candidate& c) {
        EXPECT_EQ(c.address(), candidate.address());
        event.Set();
      });

  main_thread_.BlockingCall(
      [&]() { conn1_->OnCandidateGathered(ice1_, candidate); });

  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, ObserverNotifiedOnConnectionError) {
  CreateConnections();

  Event event;
  EXPECT_CALL(*observer1_ptr_, OnConnectionError()).WillOnce([&]() {
    event.Set();
  });

  main_thread_.BlockingCall([&]() {
    ice1_->SetTransportState(webrtc::IceTransportState::kFailed,
                             webrtc::IceTransportStateInternal::STATE_FAILED);
  });

  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, DirectDtlsPacketsAreSent) {
  CreateConnections(WireProtocol::kDtls);
  Connect();
  WAIT(conn1_->Writable() && conn2_->Writable(), 1000);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  Event event;
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kSuccess);
        EXPECT_NE(outcome.send_time, Timestamp::MinusInfinity());
        event.Set();
      });
  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
  // For direct DTLS, the sent packet should be larger than the data due to
  // DTLS overhead.
  EXPECT_GT(ice1_->last_sent_packet().size(), data.size());
  ASSERT_TRUE(event.Wait(TimeDelta::Millis(1000)));
}

TEST_F(DatagramConnectionTest, DirectDtlsPacketsAreReceived) {
  CreateConnections(WireProtocol::kDtls);
  Connect();
  WAIT(conn1_->Writable() && conn2_->Writable(), 1000);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  Event receive_event;
  EXPECT_CALL(*observer2_ptr_, OnPacketReceived(_, _))
      .WillOnce(
          [&](ArrayView<const uint8_t> received_data,
              const DatagramConnection::Observer::PacketMetadata& metadata) {
            EXPECT_EQ(received_data.size(), data.size());
            EXPECT_EQ(memcmp(received_data.data(), data.data(), data.size()),
                      0);
            EXPECT_NE(metadata.receive_time, Timestamp::Zero());
            receive_event.Set();
          });

  Event send_event;
  EXPECT_CALL(*observer1_ptr_, OnSendOutcome(_))
      .WillOnce([&](const SendOutcome& outcome) {
        EXPECT_EQ(outcome.id, 1u);
        EXPECT_EQ(outcome.status, SendOutcome::Status::kSuccess);
        EXPECT_NE(outcome.send_time, Timestamp::MinusInfinity());
        send_event.Set();
      });

  conn1_->SendPacket(data, PacketSendParameters{.id = 1});
  // Process the message queue to ensure the packet is sent.
  Thread::Current()->ProcessMessages(0);
  ASSERT_TRUE(receive_event.Wait(TimeDelta::Millis(1000)));
  ASSERT_TRUE(send_event.Wait(TimeDelta::Millis(1000)));
}

}  // namespace
}  // namespace webrtc
