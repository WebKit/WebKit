/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/composite_rtp_transport.h"

#include <memory>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/fake_packet_transport.h"
#include "pc/rtp_transport.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr char kTransportName[] = "test-transport";
constexpr char kRtcpTransportName[] = "test-transport-rtcp";
constexpr uint8_t kRtpPayloadType = 100;

constexpr uint8_t kRtcpPacket[] = {0x80, 73, 0, 0};
constexpr uint8_t kRtpPacket[] = {0x80, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

class CompositeRtpTransportTest : public ::testing::Test,
                                  public sigslot::has_slots<>,
                                  public RtpPacketSinkInterface {
 public:
  CompositeRtpTransportTest()
      : packet_transport_1_(
            std::make_unique<rtc::FakePacketTransport>(kTransportName)),
        packet_transport_2_(
            std::make_unique<rtc::FakePacketTransport>(kTransportName)),
        rtcp_transport_1_(
            std::make_unique<rtc::FakePacketTransport>(kRtcpTransportName)),
        rtcp_transport_2_(
            std::make_unique<rtc::FakePacketTransport>(kRtcpTransportName)) {}

  void SetupRtpTransports(bool rtcp_mux) {
    transport_1_ = std::make_unique<RtpTransport>(rtcp_mux);
    transport_2_ = std::make_unique<RtpTransport>(rtcp_mux);

    transport_1_->SetRtpPacketTransport(packet_transport_1_.get());
    transport_2_->SetRtpPacketTransport(packet_transport_2_.get());
    if (!rtcp_mux) {
      transport_1_->SetRtcpPacketTransport(rtcp_transport_1_.get());
      transport_2_->SetRtcpPacketTransport(rtcp_transport_2_.get());
    }

    composite_ = std::make_unique<CompositeRtpTransport>(
        std::vector<RtpTransportInternal*>{transport_1_.get(),
                                           transport_2_.get()});

    composite_->SignalReadyToSend.connect(
        this, &CompositeRtpTransportTest::OnReadyToSend);
    composite_->SignalWritableState.connect(
        this, &CompositeRtpTransportTest::OnWritableState);
    composite_->SignalSentPacket.connect(
        this, &CompositeRtpTransportTest::OnSentPacket);
    composite_->SignalNetworkRouteChanged.connect(
        this, &CompositeRtpTransportTest::OnNetworkRouteChanged);
    composite_->SignalRtcpPacketReceived.connect(
        this, &CompositeRtpTransportTest::OnRtcpPacketReceived);

    RtpDemuxerCriteria criteria;
    criteria.payload_types.insert(kRtpPayloadType);
    composite_->RegisterRtpDemuxerSink(criteria, this);
  }

  void TearDown() override { composite_->UnregisterRtpDemuxerSink(this); }

  void OnReadyToSend(bool ready) { ++ready_to_send_count_; }
  void OnWritableState(bool writable) { ++writable_state_count_; }
  void OnSentPacket(const rtc::SentPacket& packet) { ++sent_packet_count_; }
  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> route) {
    ++network_route_count_;
    last_network_route_ = route;
  }
  void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer* buffer,
                            int64_t packet_time_us) {
    ++rtcp_packet_count_;
    last_packet_ = *buffer;
  }
  void OnRtpPacket(const RtpPacketReceived& packet) {
    ++rtp_packet_count_;
    last_packet_ = packet.Buffer();
  }

 protected:
  std::unique_ptr<rtc::FakePacketTransport> packet_transport_1_;
  std::unique_ptr<rtc::FakePacketTransport> packet_transport_2_;
  std::unique_ptr<rtc::FakePacketTransport> rtcp_transport_1_;
  std::unique_ptr<rtc::FakePacketTransport> rtcp_transport_2_;
  std::unique_ptr<RtpTransport> transport_1_;
  std::unique_ptr<RtpTransport> transport_2_;
  std::unique_ptr<CompositeRtpTransport> composite_;

  int ready_to_send_count_ = 0;
  int writable_state_count_ = 0;
  int sent_packet_count_ = 0;
  int network_route_count_ = 0;
  int rtcp_packet_count_ = 0;
  int rtp_packet_count_ = 0;

  absl::optional<rtc::NetworkRoute> last_network_route_;
  rtc::CopyOnWriteBuffer last_packet_;
};

TEST_F(CompositeRtpTransportTest, EnableRtcpMux) {
  SetupRtpTransports(/*rtcp_mux=*/false);
  EXPECT_FALSE(composite_->rtcp_mux_enabled());
  EXPECT_FALSE(transport_1_->rtcp_mux_enabled());
  EXPECT_FALSE(transport_2_->rtcp_mux_enabled());

  composite_->SetRtcpMuxEnabled(true);
  EXPECT_TRUE(composite_->rtcp_mux_enabled());
  EXPECT_TRUE(transport_1_->rtcp_mux_enabled());
  EXPECT_TRUE(transport_2_->rtcp_mux_enabled());
}

TEST_F(CompositeRtpTransportTest, DisableRtcpMux) {
  SetupRtpTransports(/*rtcp_mux=*/true);
  EXPECT_TRUE(composite_->rtcp_mux_enabled());
  EXPECT_TRUE(transport_1_->rtcp_mux_enabled());
  EXPECT_TRUE(transport_2_->rtcp_mux_enabled());

  // If the component transports didn't have an RTCP transport before, they need
  // to be set independently before disabling RTCP mux.  There's no other sane
  // way to do this, as the interface only allows sending a single RTCP
  // transport, and we need one for each component.
  transport_1_->SetRtcpPacketTransport(rtcp_transport_1_.get());
  transport_2_->SetRtcpPacketTransport(rtcp_transport_2_.get());

  composite_->SetRtcpMuxEnabled(false);
  EXPECT_FALSE(composite_->rtcp_mux_enabled());
  EXPECT_FALSE(transport_1_->rtcp_mux_enabled());
  EXPECT_FALSE(transport_2_->rtcp_mux_enabled());
}

TEST_F(CompositeRtpTransportTest, SetRtpOption) {
  SetupRtpTransports(/*rtcp_mux=*/true);
  EXPECT_EQ(0, composite_->SetRtpOption(rtc::Socket::OPT_DSCP, 2));

  int value = 0;
  EXPECT_TRUE(packet_transport_1_->GetOption(rtc::Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);

  EXPECT_TRUE(packet_transport_2_->GetOption(rtc::Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);
}

TEST_F(CompositeRtpTransportTest, SetRtcpOption) {
  SetupRtpTransports(/*rtcp_mux=*/false);
  EXPECT_EQ(0, composite_->SetRtcpOption(rtc::Socket::OPT_DSCP, 2));

  int value = 0;
  EXPECT_TRUE(rtcp_transport_1_->GetOption(rtc::Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);

  EXPECT_TRUE(rtcp_transport_2_->GetOption(rtc::Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);
}

TEST_F(CompositeRtpTransportTest, NeverWritableWithoutSendTransport) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  packet_transport_1_->SetWritable(true);
  packet_transport_2_->SetWritable(true);

  EXPECT_FALSE(composite_->IsWritable(false));
  EXPECT_FALSE(composite_->IsWritable(true));
  EXPECT_FALSE(composite_->IsReadyToSend());
  EXPECT_EQ(0, ready_to_send_count_);
  EXPECT_EQ(0, writable_state_count_);
}

TEST_F(CompositeRtpTransportTest, WritableWhenSendTransportBecomesWritable) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  composite_->SetSendTransport(transport_1_.get());

  EXPECT_FALSE(composite_->IsWritable(false));
  EXPECT_FALSE(composite_->IsWritable(true));
  EXPECT_FALSE(composite_->IsReadyToSend());
  EXPECT_EQ(0, ready_to_send_count_);
  EXPECT_EQ(1, writable_state_count_);

  packet_transport_2_->SetWritable(true);

  EXPECT_FALSE(composite_->IsWritable(false));
  EXPECT_FALSE(composite_->IsWritable(true));
  EXPECT_FALSE(composite_->IsReadyToSend());
  EXPECT_EQ(0, ready_to_send_count_);
  EXPECT_EQ(1, writable_state_count_);

  packet_transport_1_->SetWritable(true);

  EXPECT_TRUE(composite_->IsWritable(false));
  EXPECT_TRUE(composite_->IsWritable(true));
  EXPECT_TRUE(composite_->IsReadyToSend());
  EXPECT_EQ(1, ready_to_send_count_);
  EXPECT_EQ(2, writable_state_count_);
}

TEST_F(CompositeRtpTransportTest, SendTransportAlreadyWritable) {
  SetupRtpTransports(/*rtcp_mux=*/true);
  packet_transport_1_->SetWritable(true);

  composite_->SetSendTransport(transport_1_.get());

  EXPECT_TRUE(composite_->IsWritable(false));
  EXPECT_TRUE(composite_->IsWritable(true));
  EXPECT_TRUE(composite_->IsReadyToSend());
  EXPECT_EQ(1, ready_to_send_count_);
  EXPECT_EQ(1, writable_state_count_);
}

TEST_F(CompositeRtpTransportTest, IsSrtpActive) {
  SetupRtpTransports(/*rtcp_mux=*/true);
  EXPECT_FALSE(composite_->IsSrtpActive());
}

TEST_F(CompositeRtpTransportTest, NetworkRouteChange) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::NetworkRoute route;
  route.local = rtc::RouteEndpoint::CreateWithNetworkId(7);
  packet_transport_1_->SetNetworkRoute(route);

  EXPECT_EQ(1, network_route_count_);
  EXPECT_EQ(7, last_network_route_->local.network_id());

  route.local = rtc::RouteEndpoint::CreateWithNetworkId(8);
  packet_transport_2_->SetNetworkRoute(route);

  EXPECT_EQ(2, network_route_count_);
  EXPECT_EQ(8, last_network_route_->local.network_id());
}

TEST_F(CompositeRtpTransportTest, RemoveTransport) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  composite_->RemoveTransport(transport_1_.get());

  // Check that signals are disconnected.
  rtc::NetworkRoute route;
  route.local = rtc::RouteEndpoint::CreateWithNetworkId(7);
  packet_transport_1_->SetNetworkRoute(route);

  EXPECT_EQ(0, network_route_count_);
}

TEST_F(CompositeRtpTransportTest, SendRtcpBeforeSendTransportSet) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtcpPacket);
  EXPECT_FALSE(composite_->SendRtcpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(0, sent_packet_count_);
}

TEST_F(CompositeRtpTransportTest, SendRtcpOn1) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);
  composite_->SetSendTransport(transport_1_.get());

  rtc::CopyOnWriteBuffer packet(kRtcpPacket);
  EXPECT_TRUE(composite_->SendRtcpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(1, sent_packet_count_);
  EXPECT_EQ(packet, *packet_transport_1_->last_sent_packet());
}

TEST_F(CompositeRtpTransportTest, SendRtcpOn2) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_2_.get(), false);
  composite_->SetSendTransport(transport_2_.get());

  rtc::CopyOnWriteBuffer packet(kRtcpPacket);
  EXPECT_TRUE(composite_->SendRtcpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(1, sent_packet_count_);
  EXPECT_EQ(packet, *packet_transport_2_->last_sent_packet());
}

TEST_F(CompositeRtpTransportTest, SendRtpBeforeSendTransportSet) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtpPacket);
  EXPECT_FALSE(composite_->SendRtpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(0, sent_packet_count_);
}

TEST_F(CompositeRtpTransportTest, SendRtpOn1) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);
  composite_->SetSendTransport(transport_1_.get());

  rtc::CopyOnWriteBuffer packet(kRtpPacket);
  EXPECT_TRUE(composite_->SendRtpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(1, sent_packet_count_);
  EXPECT_EQ(packet, *packet_transport_1_->last_sent_packet());
}

TEST_F(CompositeRtpTransportTest, SendRtpOn2) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_2_.get(), false);
  composite_->SetSendTransport(transport_2_.get());

  rtc::CopyOnWriteBuffer packet(kRtpPacket);
  EXPECT_TRUE(composite_->SendRtpPacket(&packet, rtc::PacketOptions(), 0));
  EXPECT_EQ(1, sent_packet_count_);
  EXPECT_EQ(packet, *packet_transport_2_->last_sent_packet());
}

TEST_F(CompositeRtpTransportTest, ReceiveRtcpFrom1) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtcpPacket);
  remote.SendPacket(packet.cdata<char>(), packet.size(), rtc::PacketOptions(),
                    0);

  EXPECT_EQ(1, rtcp_packet_count_);
  EXPECT_EQ(packet, last_packet_);
}

TEST_F(CompositeRtpTransportTest, ReceiveRtcpFrom2) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_2_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtcpPacket);
  remote.SendPacket(packet.cdata<char>(), packet.size(), rtc::PacketOptions(),
                    0);

  EXPECT_EQ(1, rtcp_packet_count_);
  EXPECT_EQ(packet, last_packet_);
}

TEST_F(CompositeRtpTransportTest, ReceiveRtpFrom1) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_1_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtpPacket);
  remote.SendPacket(packet.cdata<char>(), packet.size(), rtc::PacketOptions(),
                    0);

  EXPECT_EQ(1, rtp_packet_count_);
  EXPECT_EQ(packet, last_packet_);
}

TEST_F(CompositeRtpTransportTest, ReceiveRtpFrom2) {
  SetupRtpTransports(/*rtcp_mux=*/true);

  rtc::FakePacketTransport remote("remote");
  remote.SetDestination(packet_transport_2_.get(), false);

  rtc::CopyOnWriteBuffer packet(kRtpPacket);
  remote.SendPacket(packet.cdata<char>(), packet.size(), rtc::PacketOptions(),
                    0);

  EXPECT_EQ(1, rtp_packet_count_);
  EXPECT_EQ(packet, last_packet_);
}

}  // namespace
}  // namespace webrtc
