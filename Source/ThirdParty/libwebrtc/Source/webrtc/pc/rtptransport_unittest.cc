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

#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/fakepackettransport.h"
#include "webrtc/pc/rtptransport.h"

namespace webrtc {

constexpr bool kMuxDisabled = false;
constexpr bool kMuxEnabled = true;

TEST(RtpTransportTest, SetRtcpParametersCantDisableRtcpMux) {
  RtpTransport transport(kMuxDisabled);
  RtcpParameters params;
  transport.SetRtcpParameters(params);
  params.mux = false;
  EXPECT_FALSE(transport.SetRtcpParameters(params).ok());
}

TEST(RtpTransportTest, SetRtcpParametersEmptyCnameUsesExisting) {
  static const char kName[] = "name";
  RtpTransport transport(kMuxDisabled);
  RtcpParameters params_with_name;
  params_with_name.cname = kName;
  transport.SetRtcpParameters(params_with_name);
  EXPECT_EQ(transport.GetRtcpParameters().cname, kName);

  RtcpParameters params_without_name;
  transport.SetRtcpParameters(params_without_name);
  EXPECT_EQ(transport.GetRtcpParameters().cname, kName);
}

class SignalObserver : public sigslot::has_slots<> {
 public:
  explicit SignalObserver(RtpTransport* transport) {
    transport->SignalReadyToSend.connect(this, &SignalObserver::OnReadyToSend);
  }
  bool ready() const { return ready_; }
  void OnReadyToSend(bool ready) { ready_ = ready; }

 private:
  bool ready_ = false;
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

class SignalPacketReceivedCounter : public sigslot::has_slots<> {
 public:
  explicit SignalPacketReceivedCounter(RtpTransport* transport) {
    transport->SignalPacketReceived.connect(
        this, &SignalPacketReceivedCounter::OnPacketReceived);
  }
  int rtcp_count() const { return rtcp_count_; }
  int rtp_count() const { return rtp_count_; }

 private:
  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer&,
                        const rtc::PacketTime&) {
    if (rtcp) {
      ++rtcp_count_;
    } else {
      ++rtp_count_;
    }
  }
  int rtcp_count_ = 0;
  int rtp_count_ = 0;
};

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
