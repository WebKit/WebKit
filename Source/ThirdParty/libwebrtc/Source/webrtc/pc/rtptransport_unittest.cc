/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <utility>

#include "p2p/base/fakepackettransport.h"
#include "pc/rtptransport.h"
#include "pc/rtptransporttestutil.h"
#include "rtc_base/gunit.h"

namespace webrtc {

constexpr bool kMuxDisabled = false;
constexpr bool kMuxEnabled = true;
constexpr uint16_t kLocalNetId = 1;
constexpr uint16_t kRemoteNetId = 2;
constexpr int kLastPacketId = 100;
constexpr int kTransportOverheadPerPacket = 28;  // Ipv4(20) + UDP(8).

TEST(RtpTransportTest, SetRtcpParametersCantDisableRtcpMux) {
  RtpTransport transport(kMuxDisabled);
  RtpTransportParameters params;
  transport.SetParameters(params);
  params.rtcp.mux = false;
  EXPECT_FALSE(transport.SetParameters(params).ok());
}

TEST(RtpTransportTest, SetRtcpParametersEmptyCnameUsesExisting) {
  static const char kName[] = "name";
  RtpTransport transport(kMuxDisabled);
  RtpTransportParameters params_with_name;
  params_with_name.rtcp.cname = kName;
  transport.SetParameters(params_with_name);
  EXPECT_EQ(transport.GetParameters().rtcp.cname, kName);

  RtpTransportParameters params_without_name;
  transport.SetParameters(params_without_name);
  EXPECT_EQ(transport.GetParameters().rtcp.cname, kName);
}

TEST(RtpTransportTest, SetRtpTransportKeepAliveNotSupported) {
  // Tests that we warn users that keep-alive isn't supported yet.
  // TODO(sprang): Wire up keep-alive and remove this test.
  RtpTransport transport(kMuxDisabled);
  RtpTransportParameters params;
  params.keepalive.timeout_interval_ms = 1;
  auto result = transport.SetParameters(params);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());
}

class SignalObserver : public sigslot::has_slots<> {
 public:
  explicit SignalObserver(RtpTransport* transport) {
    transport->SignalReadyToSend.connect(this, &SignalObserver::OnReadyToSend);
    transport->SignalNetworkRouteChanged.connect(
        this, &SignalObserver::OnNetworkRouteChanged);
  }

  bool ready() const { return ready_; }
  void OnReadyToSend(bool ready) { ready_ = ready; }

  rtc::Optional<rtc::NetworkRoute> network_route() { return network_route_; }
  void OnNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute> network_route) {
    network_route_ = std::move(network_route);
  }

 private:
  bool ready_ = false;
  rtc::Optional<rtc::NetworkRoute> network_route_;
};

TEST(RtpTransportTest, SettingRtcpAndRtpSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtcp("fake_rtcp");
  fake_rtcp.SetWritable(true);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtcpPacketTransport(&fake_rtcp);  // rtcp ready
  EXPECT_FALSE(observer.ready());
  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready());
}

TEST(RtpTransportTest, SettingRtpAndRtcpSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtcp("fake_rtcp");
  fake_rtcp.SetWritable(true);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_FALSE(observer.ready());
  transport.SetRtcpPacketTransport(&fake_rtcp);  // rtcp ready
  EXPECT_TRUE(observer.ready());
}

TEST(RtpTransportTest, SettingRtpWithRtcpMuxEnabledSignalsReady) {
  RtpTransport transport(kMuxEnabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready());
}

TEST(RtpTransportTest, DisablingRtcpMuxSignalsNotReady) {
  RtpTransport transport(kMuxEnabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_TRUE(observer.ready());

  transport.SetRtcpMuxEnabled(false);
  EXPECT_FALSE(observer.ready());
}

TEST(RtpTransportTest, EnablingRtcpMuxSignalsReady) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  transport.SetRtpPacketTransport(&fake_rtp);  // rtp ready
  EXPECT_FALSE(observer.ready());

  transport.SetRtcpMuxEnabled(true);
  EXPECT_TRUE(observer.ready());
}

// Tests the SignalNetworkRoute is fired when setting a packet transport.
TEST(RtpTransportTest, SetRtpTransportWithNetworkRouteChanged) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");

  EXPECT_FALSE(observer.network_route());

  rtc::NetworkRoute network_route;
  // Set a non-null RTP transport with a new network route.
  network_route.connected = true;
  network_route.local_network_id = kLocalNetId;
  network_route.remote_network_id = kRemoteNetId;
  network_route.last_sent_packet_id = kLastPacketId;
  network_route.packet_overhead = kTransportOverheadPerPacket;
  fake_rtp.SetNetworkRoute(rtc::Optional<rtc::NetworkRoute>(network_route));
  transport.SetRtpPacketTransport(&fake_rtp);
  ASSERT_TRUE(observer.network_route());
  EXPECT_EQ(network_route, *(observer.network_route()));
  EXPECT_EQ(kTransportOverheadPerPacket,
            observer.network_route()->packet_overhead);
  EXPECT_EQ(kLastPacketId, observer.network_route()->last_sent_packet_id);

  // Set a null RTP transport.
  transport.SetRtpPacketTransport(nullptr);
  EXPECT_FALSE(observer.network_route());
}

TEST(RtpTransportTest, SetRtcpTransportWithNetworkRouteChanged) {
  RtpTransport transport(kMuxDisabled);
  SignalObserver observer(&transport);
  rtc::FakePacketTransport fake_rtcp("fake_rtcp");

  EXPECT_FALSE(observer.network_route());

  rtc::NetworkRoute network_route;
  // Set a non-null RTCP transport with a new network route.
  network_route.connected = true;
  network_route.local_network_id = kLocalNetId;
  network_route.remote_network_id = kRemoteNetId;
  network_route.last_sent_packet_id = kLastPacketId;
  network_route.packet_overhead = kTransportOverheadPerPacket;
  fake_rtcp.SetNetworkRoute(rtc::Optional<rtc::NetworkRoute>(network_route));
  transport.SetRtcpPacketTransport(&fake_rtcp);
  ASSERT_TRUE(observer.network_route());
  EXPECT_EQ(network_route, *(observer.network_route()));
  EXPECT_EQ(kTransportOverheadPerPacket,
            observer.network_route()->packet_overhead);
  EXPECT_EQ(kLastPacketId, observer.network_route()->last_sent_packet_id);

  // Set a null RTCP transport.
  transport.SetRtcpPacketTransport(nullptr);
  EXPECT_FALSE(observer.network_route());
}

class SignalCounter : public sigslot::has_slots<> {
 public:
  explicit SignalCounter(RtpTransport* transport) {
    transport->SignalReadyToSend.connect(this, &SignalCounter::OnReadyToSend);
  }
  int count() const { return count_; }
  void OnReadyToSend(bool ready) { ++count_; }

 private:
  int count_ = 0;
};

TEST(RtpTransportTest, ChangingReadyToSendStateOnlySignalsWhenChanged) {
  RtpTransport transport(kMuxEnabled);
  SignalCounter observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetWritable(true);

  // State changes, so we should signal.
  transport.SetRtpPacketTransport(&fake_rtp);
  EXPECT_EQ(observer.count(), 1);

  // State does not change, so we should not signal.
  transport.SetRtpPacketTransport(&fake_rtp);
  EXPECT_EQ(observer.count(), 1);

  // State does not change, so we should not signal.
  transport.SetRtcpMuxEnabled(true);
  EXPECT_EQ(observer.count(), 1);

  // State changes, so we should signal.
  transport.SetRtcpMuxEnabled(false);
  EXPECT_EQ(observer.count(), 2);
}

// Test that SignalPacketReceived fires with rtcp=true when a RTCP packet is
// received.
TEST(RtpTransportTest, SignalDemuxedRtcp) {
  RtpTransport transport(kMuxDisabled);
  SignalPacketReceivedCounter observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetDestination(&fake_rtp, true);
  transport.SetRtpPacketTransport(&fake_rtp);

  // An rtcp packet.
  const char data[] = {0, 73, 0, 0};
  const int len = 4;
  const rtc::PacketOptions options;
  const int flags = 0;
  fake_rtp.SendPacket(data, len, options, flags);
  EXPECT_EQ(0, observer.rtp_count());
  EXPECT_EQ(1, observer.rtcp_count());
}

static const unsigned char kRtpData[] = {0x80, 0x11, 0, 0, 0, 0,
                                         0,    0,    0, 0, 0, 0};
static const int kRtpLen = 12;

// Test that SignalPacketReceived fires with rtcp=false when a RTP packet with a
// handled payload type is received.
TEST(RtpTransportTest, SignalHandledRtpPayloadType) {
  RtpTransport transport(kMuxDisabled);
  SignalPacketReceivedCounter observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetDestination(&fake_rtp, true);
  transport.SetRtpPacketTransport(&fake_rtp);
  transport.AddHandledPayloadType(0x11);

  // An rtp packet.
  const rtc::PacketOptions options;
  const int flags = 0;
  rtc::Buffer rtp_data(kRtpData, kRtpLen);
  fake_rtp.SendPacket(rtp_data.data<char>(), kRtpLen, options, flags);
  EXPECT_EQ(1, observer.rtp_count());
  EXPECT_EQ(0, observer.rtcp_count());
}

// Test that SignalPacketReceived does not fire when a RTP packet with an
// unhandled payload type is received.
TEST(RtpTransportTest, DontSignalUnhandledRtpPayloadType) {
  RtpTransport transport(kMuxDisabled);
  SignalPacketReceivedCounter observer(&transport);
  rtc::FakePacketTransport fake_rtp("fake_rtp");
  fake_rtp.SetDestination(&fake_rtp, true);
  transport.SetRtpPacketTransport(&fake_rtp);

  const rtc::PacketOptions options;
  const int flags = 0;
  rtc::Buffer rtp_data(kRtpData, kRtpLen);
  fake_rtp.SendPacket(rtp_data.data<char>(), kRtpLen, options, flags);
  EXPECT_EQ(0, observer.rtp_count());
  EXPECT_EQ(0, observer.rtcp_count());
}

}  // namespace webrtc
