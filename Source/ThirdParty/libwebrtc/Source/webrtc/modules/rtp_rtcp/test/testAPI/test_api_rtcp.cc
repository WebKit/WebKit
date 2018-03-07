/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "modules/rtp_rtcp/test/testAPI/test_api.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class RtcpCallback : public RtcpIntraFrameObserver {
 public:
  void SetModule(RtpRtcp* module) {
    _rtpRtcpModule = module;
  }
  virtual void OnRTCPPacketTimeout(const int32_t id) {
  }
  virtual void OnLipSyncUpdate(const int32_t id,
                               const int32_t audioVideoOffset) {}
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) {}

 private:
  RtpRtcp* _rtpRtcpModule;
};

class TestRtpFeedback : public NullRtpFeedback {
 public:
  explicit TestRtpFeedback(RtpRtcp* rtp_rtcp) : rtp_rtcp_(rtp_rtcp) {}
  virtual ~TestRtpFeedback() {}

  void OnIncomingSSRCChanged(uint32_t ssrc) override {
    rtp_rtcp_->SetRemoteSSRC(ssrc);
  }

 private:
  RtpRtcp* rtp_rtcp_;
};

class RtpRtcpRtcpTest : public ::testing::Test {
 protected:
  RtpRtcpRtcpTest()
      : fake_clock(123456), retransmission_rate_limiter_(&fake_clock, 1000) {
    test_csrcs.push_back(1234);
    test_csrcs.push_back(2345);
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpRtcpTest() {}

  virtual void SetUp() {
    receiver = new TestRtpReceiver();
    transport1 = new LoopBackTransport();
    transport2 = new LoopBackTransport();
    myRTCPFeedback1 = new RtcpCallback();
    myRTCPFeedback2 = new RtcpCallback();

    receive_statistics1_.reset(ReceiveStatistics::Create(&fake_clock));
    receive_statistics2_.reset(ReceiveStatistics::Create(&fake_clock));

    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    configuration.receive_statistics = receive_statistics1_.get();
    configuration.outgoing_transport = transport1;
    configuration.intra_frame_callback = myRTCPFeedback1;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;

    rtp_payload_registry1_.reset(new RTPPayloadRegistry());
    rtp_payload_registry2_.reset(new RTPPayloadRegistry());

    module1 = RtpRtcp::CreateRtpRtcp(configuration);

    rtp_feedback1_.reset(new TestRtpFeedback(module1));

    rtp_receiver1_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock, receiver, rtp_feedback1_.get(),
        rtp_payload_registry1_.get()));

    configuration.receive_statistics = receive_statistics2_.get();
    configuration.outgoing_transport = transport2;
    configuration.intra_frame_callback = myRTCPFeedback2;

    module2 = RtpRtcp::CreateRtpRtcp(configuration);

    rtp_feedback2_.reset(new TestRtpFeedback(module2));

    rtp_receiver2_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock, receiver, rtp_feedback2_.get(),
        rtp_payload_registry2_.get()));

    transport1->SetSendModule(module2, rtp_payload_registry2_.get(),
                              rtp_receiver2_.get(), receive_statistics2_.get());
    transport2->SetSendModule(module1, rtp_payload_registry1_.get(),
                              rtp_receiver1_.get(), receive_statistics1_.get());
    myRTCPFeedback1->SetModule(module1);
    myRTCPFeedback2->SetModule(module2);

    module1->SetRTCPStatus(RtcpMode::kCompound);
    module2->SetRTCPStatus(RtcpMode::kCompound);

    module2->SetSSRC(test_ssrc + 1);
    module1->SetSSRC(test_ssrc);
    module1->SetSequenceNumber(test_sequence_number);
    module1->SetStartTimestamp(test_timestamp);

    module1->SetCsrcs(test_csrcs);
    EXPECT_EQ(0, module1->SetCNAME("john.doe@test.test"));

    EXPECT_EQ(0, module1->SetSendingStatus(true));

    CodecInst voice_codec;
    voice_codec.pltype = 96;
    voice_codec.plfreq = 8000;
    voice_codec.rate = 64000;
    memcpy(voice_codec.plname, "PCMU", 5);

    EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
    EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
                     voice_codec.pltype, CodecInstToSdp(voice_codec)));
    EXPECT_EQ(0, module2->RegisterSendPayload(voice_codec));
    EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
                     voice_codec.pltype, CodecInstToSdp(voice_codec)));

    // We need to send one RTP packet to get the RTCP packet to be accepted by
    // the receiving module.
    // send RTP packet with the data "testtest"
    const uint8_t test[9] = "testtest";
    EXPECT_EQ(true,
              module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96, 0, -1,
                                        test, 8, nullptr, nullptr, nullptr));
  }

  virtual void TearDown() {
    delete module1;
    delete module2;
    delete myRTCPFeedback1;
    delete myRTCPFeedback2;
    delete transport1;
    delete transport2;
    delete receiver;
  }

  std::unique_ptr<TestRtpFeedback> rtp_feedback1_;
  std::unique_ptr<TestRtpFeedback> rtp_feedback2_;
  std::unique_ptr<ReceiveStatistics> receive_statistics1_;
  std::unique_ptr<ReceiveStatistics> receive_statistics2_;
  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry1_;
  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry2_;
  std::unique_ptr<RtpReceiver> rtp_receiver1_;
  std::unique_ptr<RtpReceiver> rtp_receiver2_;
  RtpRtcp* module1;
  RtpRtcp* module2;
  TestRtpReceiver* receiver;
  LoopBackTransport* transport1;
  LoopBackTransport* transport2;
  RtcpCallback* myRTCPFeedback1;
  RtcpCallback* myRTCPFeedback2;

  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  std::vector<uint32_t> test_csrcs;
  SimulatedClock fake_clock;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpRtcpTest, RTCP_CNAME) {
  uint32_t testOfCSRC[webrtc::kRtpCsrcSize];
  EXPECT_EQ(2, rtp_receiver2_->CSRCs(testOfCSRC));
  EXPECT_EQ(test_csrcs[0], testOfCSRC[0]);
  EXPECT_EQ(test_csrcs[1], testOfCSRC[1]);

  // Set cname of mixed.
  EXPECT_EQ(0, module1->AddMixedCNAME(test_csrcs[0], "john@192.168.0.1"));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_csrcs[1], "jane@192.168.0.2"));

  EXPECT_EQ(-1, module1->RemoveMixedCNAME(test_csrcs[0] + 1));
  EXPECT_EQ(0, module1->RemoveMixedCNAME(test_csrcs[1]));
  EXPECT_EQ(0, module1->AddMixedCNAME(test_csrcs[1], "jane@192.168.0.2"));

  // send RTCP packet, triggered by timer
  fake_clock.AdvanceTimeMilliseconds(7500);
  module1->Process();
  fake_clock.AdvanceTimeMilliseconds(100);
  module2->Process();

  char cName[RTCP_CNAME_SIZE];
  EXPECT_EQ(-1, module2->RemoteCNAME(rtp_receiver2_->SSRC() + 1, cName));

  // Check multiple CNAME.
  EXPECT_EQ(0, module2->RemoteCNAME(rtp_receiver2_->SSRC(), cName));
  EXPECT_EQ(0, strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_csrcs[0], cName));
  EXPECT_EQ(0, strncmp(cName, "john@192.168.0.1", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module2->RemoteCNAME(test_csrcs[1], cName));
  EXPECT_EQ(0, strncmp(cName, "jane@192.168.0.2", RTCP_CNAME_SIZE));

  EXPECT_EQ(0, module1->SetSendingStatus(false));

  // Test that BYE clears the CNAME
  EXPECT_EQ(-1, module2->RemoteCNAME(rtp_receiver2_->SSRC(), cName));
}

TEST_F(RtpRtcpRtcpTest, RemoteRTCPStatRemote) {
  std::vector<RTCPReportBlock> report_blocks;

  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  EXPECT_EQ(0u, report_blocks.size());

  // send RTCP packet, triggered by timer
  fake_clock.AdvanceTimeMilliseconds(7500);
  module1->Process();
  fake_clock.AdvanceTimeMilliseconds(100);
  module2->Process();

  EXPECT_EQ(0, module1->RemoteRTCPStat(&report_blocks));
  ASSERT_EQ(1u, report_blocks.size());

  // |test_ssrc+1| is the SSRC of module2 that send the report.
  EXPECT_EQ(test_ssrc + 1, report_blocks[0].sender_ssrc);
  EXPECT_EQ(test_ssrc, report_blocks[0].source_ssrc);

  EXPECT_EQ(0u, report_blocks[0].packets_lost);
  EXPECT_LT(0u, report_blocks[0].delay_since_last_sender_report);
  EXPECT_EQ(test_sequence_number,
            report_blocks[0].extended_highest_sequence_number);
  EXPECT_EQ(0u, report_blocks[0].fraction_lost);
}

}  // namespace
}  // namespace webrtc
