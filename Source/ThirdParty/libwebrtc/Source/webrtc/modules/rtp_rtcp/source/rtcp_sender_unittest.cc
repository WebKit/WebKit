/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/rate_limiter.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;

namespace webrtc {

TEST(NACKStringBuilderTest, TestCase1) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase2) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5-7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase3) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  builder.PushNACK(21);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19,21"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase4) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase5) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-16,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase6) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(17);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase7) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(13);
  builder.PushNACK(14);
  builder.PushNACK(15);
  EXPECT_EQ(std::string("5-8,11-15"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase8) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(11);
  builder.PushNACK(15);
  builder.PushNACK(17);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9,11,15,17,19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase9) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  EXPECT_EQ(std::string("5-12"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase10) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  EXPECT_EQ(std::string("5"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase11) {
  NACKStringBuilder builder;
  EXPECT_EQ(std::string(""), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase12) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  EXPECT_EQ(std::string("5-6"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase13) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(9);
  EXPECT_EQ(std::string("5-6,9"), builder.GetResult());
}

class RtcpPacketTypeCounterObserverImpl : public RtcpPacketTypeCounterObserver {
 public:
  RtcpPacketTypeCounterObserverImpl() : ssrc_(0) {}
  virtual ~RtcpPacketTypeCounterObserverImpl() {}
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    ssrc_ = ssrc;
    counter_ = packet_counter;
  }
  uint32_t ssrc_;
  RtcpPacketTypeCounter counter_;
};

class TestTransport : public Transport,
                      public RtpData {
 public:
  TestTransport() {}

  bool SendRtp(const uint8_t* /*data*/,
               size_t /*len*/,
               const PacketOptions& options) override {
    return false;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {
    parser_.Parse(data, len);
    return true;
  }
  int OnReceivedPayloadData(const uint8_t* payload_data,
                            size_t payload_size,
                            const WebRtcRTPHeader* rtp_header) override {
    return 0;
  }
  test::RtcpPacketParser parser_;
};

namespace {
static const uint32_t kSenderSsrc = 0x11111111;
static const uint32_t kRemoteSsrc = 0x22222222;
static const uint32_t kStartRtpTimestamp = 0x34567;
static const uint32_t kRtpTimestamp = 0x45678;
}

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : clock_(1335900000),
        receive_statistics_(ReceiveStatistics::Create(&clock_)),
        retransmission_rate_limiter_(&clock_, 1000) {
    RtpRtcp::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &clock_;
    configuration.outgoing_transport = &test_transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;

    rtp_rtcp_impl_.reset(new ModuleRtpRtcpImpl(configuration));
    rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                      nullptr, nullptr, &test_transport_));
    rtcp_sender_->SetSSRC(kSenderSsrc);
    rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
    rtcp_sender_->SetTimestampOffset(kStartRtpTimestamp);
    rtcp_sender_->SetLastRtpTime(kRtpTimestamp, clock_.TimeInMilliseconds());
  }

  void InsertIncomingPacket(uint32_t remote_ssrc, uint16_t seq_num) {
    RTPHeader header;
    header.ssrc = remote_ssrc;
    header.sequenceNumber = seq_num;
    header.timestamp = 12345;
    header.headerLength = 12;
    size_t kPacketLength = 100;
    receive_statistics_->IncomingPacket(header, kPacketLength, false);
  }

  test::RtcpPacketParser* parser() { return &test_transport_.parser_; }

  RTCPSender::FeedbackState feedback_state() {
    return rtp_rtcp_impl_->GetFeedbackState();
  }

  SimulatedClock clock_;
  TestTransport test_transport_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl> rtp_rtcp_impl_;
  std::unique_ptr<RTCPSender> rtcp_sender_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtcpSenderTest, SetRtcpStatus) {
  EXPECT_EQ(RtcpMode::kOff, rtcp_sender_->Status());
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(RtcpMode::kReducedSize, rtcp_sender_->Status());
}

TEST_F(RtcpSenderTest, SetSendingStatus) {
  EXPECT_FALSE(rtcp_sender_->Sending());
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  EXPECT_TRUE(rtcp_sender_->Sending());
}

TEST_F(RtcpSenderTest, NoPacketSentIfOff) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kOff);
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
}

TEST_F(RtcpSenderTest, SendSr) {
  const uint32_t kPacketCount = 0x12345;
  const uint32_t kOctetCount = 0x23456;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  rtcp_sender_->SetSendingStatus(feedback_state, true);
  feedback_state.packets_sent = kPacketCount;
  feedback_state.media_bytes_sent = kOctetCount;
  NtpTime ntp = clock_.CurrentNtpTime();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->sender_report()->sender_ssrc());
  EXPECT_EQ(ntp, parser()->sender_report()->ntp());
  EXPECT_EQ(kPacketCount, parser()->sender_report()->sender_packet_count());
  EXPECT_EQ(kOctetCount, parser()->sender_report()->sender_octet_count());
  EXPECT_EQ(kStartRtpTimestamp + kRtpTimestamp,
            parser()->sender_report()->rtp_timestamp());
  EXPECT_EQ(0U, parser()->sender_report()->report_blocks().size());
}

TEST_F(RtcpSenderTest, DoNotSendSrBeforeRtp) {
  rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                    nullptr, nullptr, &test_transport_));
  rtcp_sender_->SetSSRC(kSenderSsrc);
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);

  // Sender Report shouldn't be send as an SR nor as a Report.
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr);
  EXPECT_EQ(0, parser()->sender_report()->num_packets());
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport);
  EXPECT_EQ(0, parser()->sender_report()->num_packets());
  // Other packets (e.g. Pli) are allowed, even if useless.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
}

TEST_F(RtcpSenderTest, DoNotSendCompundBeforeRtp) {
  rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                    nullptr, nullptr, &test_transport_));
  rtcp_sender_->SetSSRC(kSenderSsrc);
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);

  // In compound mode no packets are allowed (e.g. Pli) because compound mode
  // should start with Sender Report.
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(0, parser()->pli()->num_packets());
}

TEST_F(RtcpSenderTest, SendRr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  EXPECT_EQ(0U, parser()->receiver_report()->report_blocks().size());
}

TEST_F(RtcpSenderTest, SendRrWithOneReportBlock) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  ASSERT_EQ(1U, parser()->receiver_report()->report_blocks().size());
  const rtcp::ReportBlock& rb = parser()->receiver_report()->report_blocks()[0];
  EXPECT_EQ(kRemoteSsrc, rb.source_ssrc());
  EXPECT_EQ(0U, rb.fraction_lost());
  EXPECT_EQ(0U, rb.cumulative_lost());
  EXPECT_EQ(kSeqNum, rb.extended_high_seq_num());
}

TEST_F(RtcpSenderTest, SendRrWithTwoReportBlocks) {
  const uint16_t kSeqNum = 11111;
  InsertIncomingPacket(kRemoteSsrc, kSeqNum);
  InsertIncomingPacket(kRemoteSsrc + 1, kSeqNum + 1);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->receiver_report()->sender_ssrc());
  EXPECT_EQ(2U, parser()->receiver_report()->report_blocks().size());
  EXPECT_EQ(kRemoteSsrc,
            parser()->receiver_report()->report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1,
            parser()->receiver_report()->report_blocks()[1].source_ssrc());
}

TEST_F(RtcpSenderTest, SendSdes) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSdes));
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1U, parser()->sdes()->chunks().size());
  EXPECT_EQ(kSenderSsrc, parser()->sdes()->chunks()[0].ssrc);
  EXPECT_EQ("alice@host", parser()->sdes()->chunks()[0].cname);
}

TEST_F(RtcpSenderTest, SendSdesWithMaxChunks) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  const char cname[] = "smith@host";
  for (size_t i = 0; i < 30; ++i) {
    const uint32_t csrc = 0x1234 + i;
    EXPECT_EQ(0, rtcp_sender_->AddMixedCNAME(csrc, cname));
  }
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSdes));
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(31U, parser()->sdes()->chunks().size());
}

TEST_F(RtcpSenderTest, SdesIncludedInCompoundPacket) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetCNAME("alice@host"));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->sdes()->num_packets());
  EXPECT_EQ(1U, parser()->sdes()->chunks().size());
}

TEST_F(RtcpSenderTest, SendBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpBye));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

TEST_F(RtcpSenderTest, StopSendingTriggersBye) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  EXPECT_EQ(1, parser()->bye()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->bye()->sender_ssrc());
}

TEST_F(RtcpSenderTest, SendApp) {
  const uint8_t kSubType = 30;
  uint32_t name = 'n' << 24;
  name += 'a' << 16;
  name += 'm' << 8;
  name += 'e';
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  EXPECT_EQ(0, rtcp_sender_->SetApplicationSpecificData(kSubType, name, kData,
                                                        sizeof(kData)));
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpApp));
  EXPECT_EQ(1, parser()->app()->num_packets());
  EXPECT_EQ(kSubType, parser()->app()->sub_type());
  EXPECT_EQ(name, parser()->app()->name());
  EXPECT_EQ(sizeof(kData), parser()->app()->data_size());
  EXPECT_EQ(0, memcmp(kData, parser()->app()->data(), sizeof(kData)));
}

TEST_F(RtcpSenderTest, SendEmptyApp) {
  const uint8_t kSubType = 30;
  const uint32_t kName = 0x6E616D65;

  EXPECT_EQ(
      0, rtcp_sender_->SetApplicationSpecificData(kSubType, kName, nullptr, 0));

  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpApp));
  EXPECT_EQ(1, parser()->app()->num_packets());
  EXPECT_EQ(kSubType, parser()->app()->sub_type());
  EXPECT_EQ(kName, parser()->app()->name());
  EXPECT_EQ(0U, parser()->app()->data_size());
}

TEST_F(RtcpSenderTest, SetInvalidApplicationSpecificData) {
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't'};
  const uint16_t kInvalidDataLength = sizeof(kData) / sizeof(kData[0]);
  EXPECT_EQ(-1, rtcp_sender_->SetApplicationSpecificData(
      0, 0, kData, kInvalidDataLength));  // Should by multiple of 4.
}

TEST_F(RtcpSenderTest, SendFir) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(1, parser()->fir()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->fir()->sender_ssrc());
  EXPECT_EQ(1U, parser()->fir()->requests().size());
  EXPECT_EQ(kRemoteSsrc, parser()->fir()->requests()[0].ssrc);
  uint8_t seq = parser()->fir()->requests()[0].seq_nr;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpFir));
  EXPECT_EQ(2, parser()->fir()->num_packets());
  EXPECT_EQ(seq + 1, parser()->fir()->requests()[0].seq_nr);
}

TEST_F(RtcpSenderTest, SendPli) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->pli()->sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->pli()->media_ssrc());
}

TEST_F(RtcpSenderTest, SendNack) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  const uint16_t kList[] = {0, 1, 16};
  const int32_t kListLength = sizeof(kList) / sizeof(kList[0]);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpNack, kListLength,
                                      kList));
  EXPECT_EQ(1, parser()->nack()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->nack()->sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parser()->nack()->media_ssrc());
  EXPECT_THAT(parser()->nack()->packet_ids(), ElementsAre(0, 1, 16));
}

TEST_F(RtcpSenderTest, RembStatus) {
  const uint64_t kBitrate = 261011;
  const std::vector<uint32_t> kSsrcs = {kRemoteSsrc, kRemoteSsrc + 1};
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);

  EXPECT_FALSE(rtcp_sender_->REMB());
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(1, parser()->receiver_report()->num_packets());
  EXPECT_EQ(0, parser()->remb()->num_packets());

  rtcp_sender_->SetREMBStatus(true);
  EXPECT_TRUE(rtcp_sender_->REMB());
  rtcp_sender_->SetREMBData(kBitrate, kSsrcs);
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(2, parser()->receiver_report()->num_packets());
  EXPECT_EQ(1, parser()->remb()->num_packets());

  // Sending another report sends remb again, even if no new remb data was set.
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(3, parser()->receiver_report()->num_packets());
  EXPECT_EQ(2, parser()->remb()->num_packets());

  // Turn off remb. rtcp_sender no longer should send it.
  rtcp_sender_->SetREMBStatus(false);
  EXPECT_FALSE(rtcp_sender_->REMB());
  rtcp_sender_->SendRTCP(feedback_state(), kRtcpRr);
  ASSERT_EQ(4, parser()->receiver_report()->num_packets());
  EXPECT_EQ(2, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, SendRemb) {
  const uint64_t kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  ssrcs.push_back(kRemoteSsrc + 1);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpRemb));
  EXPECT_EQ(1, parser()->remb()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->remb()->sender_ssrc());
  EXPECT_EQ(kBitrate, parser()->remb()->bitrate_bps());
  EXPECT_THAT(parser()->remb()->ssrcs(),
              ElementsAre(kRemoteSsrc, kRemoteSsrc + 1));
}

TEST_F(RtcpSenderTest, RembIncludedInCompoundPacketIfEnabled) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBStatus(true);
  EXPECT_TRUE(rtcp_sender_->REMB());
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->remb()->num_packets());
  // REMB should be included in each compound packet.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(2, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, RembNotIncludedInCompoundPacketIfNotEnabled) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  EXPECT_FALSE(rtcp_sender_->REMB());
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->remb()->num_packets());
}

TEST_F(RtcpSenderTest, SendXrWithVoipMetric) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  RTCPVoIPMetric metric;
  metric.lossRate = 1;
  metric.discardRate = 2;
  metric.burstDensity = 3;
  metric.gapDensity = 4;
  metric.burstDuration = 0x1111;
  metric.gapDuration = 0x2222;
  metric.roundTripDelay = 0x3333;
  metric.endSystemDelay = 0x4444;
  metric.signalLevel = 5;
  metric.noiseLevel = 6;
  metric.RERL = 7;
  metric.Gmin = 8;
  metric.Rfactor = 9;
  metric.extRfactor = 10;
  metric.MOSLQ = 11;
  metric.MOSCQ = 12;
  metric.RXconfig = 13;
  metric.JBnominal = 0x5555;
  metric.JBmax = 0x6666;
  metric.JBabsMax = 0x7777;
  EXPECT_EQ(0, rtcp_sender_->SetRTCPVoIPMetrics(&metric));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpXrVoipMetric));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  ASSERT_TRUE(parser()->xr()->voip_metric());
  EXPECT_EQ(kRemoteSsrc, parser()->xr()->voip_metric()->ssrc());
  const auto& parsed_metric = parser()->xr()->voip_metric()->voip_metric();
  EXPECT_EQ(metric.lossRate, parsed_metric.lossRate);
  EXPECT_EQ(metric.discardRate, parsed_metric.discardRate);
  EXPECT_EQ(metric.burstDensity, parsed_metric.burstDensity);
  EXPECT_EQ(metric.gapDensity, parsed_metric.gapDensity);
  EXPECT_EQ(metric.burstDuration, parsed_metric.burstDuration);
  EXPECT_EQ(metric.gapDuration, parsed_metric.gapDuration);
  EXPECT_EQ(metric.roundTripDelay, parsed_metric.roundTripDelay);
  EXPECT_EQ(metric.endSystemDelay, parsed_metric.endSystemDelay);
  EXPECT_EQ(metric.signalLevel, parsed_metric.signalLevel);
  EXPECT_EQ(metric.noiseLevel, parsed_metric.noiseLevel);
  EXPECT_EQ(metric.RERL, parsed_metric.RERL);
  EXPECT_EQ(metric.Gmin, parsed_metric.Gmin);
  EXPECT_EQ(metric.Rfactor, parsed_metric.Rfactor);
  EXPECT_EQ(metric.extRfactor, parsed_metric.extRfactor);
  EXPECT_EQ(metric.MOSLQ, parsed_metric.MOSLQ);
  EXPECT_EQ(metric.MOSCQ, parsed_metric.MOSCQ);
  EXPECT_EQ(metric.RXconfig, parsed_metric.RXconfig);
  EXPECT_EQ(metric.JBnominal, parsed_metric.JBnominal);
  EXPECT_EQ(metric.JBmax, parsed_metric.JBmax);
  EXPECT_EQ(metric.JBabsMax, parsed_metric.JBabsMax);
}

TEST_F(RtcpSenderTest, SendXrWithDlrr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPSender::FeedbackState feedback_state = rtp_rtcp_impl_->GetFeedbackState();
  feedback_state.has_last_xr_rr = true;
  rtcp::ReceiveTimeInfo last_xr_rr;
  last_xr_rr.ssrc = 0x11111111;
  last_xr_rr.last_rr = 0x22222222;
  last_xr_rr.delay_since_last_rr = 0x33333333;
  feedback_state.last_xr_rr = last_xr_rr;
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  EXPECT_EQ(1U, parser()->xr()->dlrr().sub_blocks().size());
  EXPECT_EQ(last_xr_rr.ssrc, parser()->xr()->dlrr().sub_blocks()[0].ssrc);
  EXPECT_EQ(last_xr_rr.last_rr, parser()->xr()->dlrr().sub_blocks()[0].last_rr);
  EXPECT_EQ(last_xr_rr.delay_since_last_rr,
            parser()->xr()->dlrr().sub_blocks()[0].delay_since_last_rr);
}

TEST_F(RtcpSenderTest, SendXrWithRrtr) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  NtpTime ntp = clock_.CurrentNtpTime();
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  EXPECT_FALSE(parser()->xr()->dlrr());
  EXPECT_FALSE(parser()->xr()->voip_metric());
  ASSERT_TRUE(parser()->xr()->rrtr());
  EXPECT_EQ(ntp, parser()->xr()->rrtr()->ntp());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfSending) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), true));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(true);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr()->num_packets());
}

TEST_F(RtcpSenderTest, TestNoXrRrtrSentIfNotEnabled) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(0, rtcp_sender_->SetSendingStatus(feedback_state(), false));
  rtcp_sender_->SendRtcpXrReceiverReferenceTime(false);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(0, parser()->xr()->num_packets());
}

TEST_F(RtcpSenderTest, TestRegisterRtcpPacketTypeObserver) {
  RtcpPacketTypeCounterObserverImpl observer;
  rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                    &observer, nullptr, &test_transport_));
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpPli));
  EXPECT_EQ(1, parser()->pli()->num_packets());
  EXPECT_EQ(kRemoteSsrc, observer.ssrc_);
  EXPECT_EQ(1U, observer.counter_.pli_packets);
  EXPECT_EQ(clock_.TimeInMilliseconds(),
            observer.counter_.first_packet_time_ms);
}

TEST_F(RtcpSenderTest, SendTmmbr) {
  const unsigned int kBitrateBps = 312000;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kReducedSize);
  rtcp_sender_->SetTargetBitrate(kBitrateBps);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpTmmbr));
  EXPECT_EQ(1, parser()->tmmbr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbr()->sender_ssrc());
  EXPECT_EQ(1U, parser()->tmmbr()->requests().size());
  EXPECT_EQ(kBitrateBps, parser()->tmmbr()->requests()[0].bitrate_bps());
  // TODO(asapersson): tmmbr_item()->Overhead() looks broken, always zero.
}

TEST_F(RtcpSenderTest, TmmbrIncludedInCompoundPacketIfEnabled) {
  const unsigned int kBitrateBps = 312000;
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_FALSE(rtcp_sender_->TMMBR());
  rtcp_sender_->SetTMMBRStatus(true);
  EXPECT_TRUE(rtcp_sender_->TMMBR());
  rtcp_sender_->SetTargetBitrate(kBitrateBps);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->tmmbr()->num_packets());
  EXPECT_EQ(1U, parser()->tmmbr()->requests().size());
  // TMMBR should be included in each compound packet.
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(2, parser()->tmmbr()->num_packets());

  rtcp_sender_->SetTMMBRStatus(false);
  EXPECT_FALSE(rtcp_sender_->TMMBR());
}

TEST_F(RtcpSenderTest, SendTmmbn) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);
  std::vector<rtcp::TmmbItem> bounding_set;
  const uint32_t kBitrateBps = 32768000;
  const uint32_t kPacketOh = 40;
  const uint32_t kSourceSsrc = 12345;
  const rtcp::TmmbItem tmmbn(kSourceSsrc, kBitrateBps, kPacketOh);
  bounding_set.push_back(tmmbn);
  rtcp_sender_->SetTmmbn(bounding_set);

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->sender_ssrc());
  EXPECT_EQ(1U, parser()->tmmbn()->items().size());
  EXPECT_EQ(kBitrateBps, parser()->tmmbn()->items()[0].bitrate_bps());
  EXPECT_EQ(kPacketOh, parser()->tmmbn()->items()[0].packet_overhead());
  EXPECT_EQ(kSourceSsrc, parser()->tmmbn()->items()[0].ssrc());
}

// This test is written to verify actual behaviour. It does not seem
// to make much sense to send an empty TMMBN, since there is no place
// to put an actual limit here. It's just information that no limit
// is set, which is kind of the starting assumption.
// See http://code.google.com/p/webrtc/issues/detail?id=468 for one
// situation where this caused confusion.
TEST_F(RtcpSenderTest, SendsTmmbnIfSetAndEmpty) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetSendingStatus(feedback_state(), true);
  std::vector<rtcp::TmmbItem> bounding_set;
  rtcp_sender_->SetTmmbn(bounding_set);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpSr));
  EXPECT_EQ(1, parser()->sender_report()->num_packets());
  EXPECT_EQ(1, parser()->tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->tmmbn()->sender_ssrc());
  EXPECT_EQ(0U, parser()->tmmbn()->items().size());
}

TEST_F(RtcpSenderTest, SendCompoundPliRemb) {
  const int kBitrate = 261011;
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(kRemoteSsrc);
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  rtcp_sender_->SetREMBData(kBitrate, ssrcs);
  std::set<RTCPPacketType> packet_types;
  packet_types.insert(kRtcpRemb);
  packet_types.insert(kRtcpPli);
  EXPECT_EQ(0, rtcp_sender_->SendCompoundRTCP(feedback_state(), packet_types));
  EXPECT_EQ(1, parser()->remb()->num_packets());
  EXPECT_EQ(1, parser()->pli()->num_packets());
}


// This test is written to verify that BYE is always the last packet
// type in a RTCP compoud packet.  The rtcp_sender_ is recreated with
// mock_transport, which is used to check for whether BYE at the end
// of a RTCP compound packet.
TEST_F(RtcpSenderTest, ByeMustBeLast) {
  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, SendRtcp(_, _))
    .WillOnce(Invoke([](const uint8_t* data, size_t len) {
    const uint8_t* next_packet = data;
    const uint8_t* const packet_end = data + len;
    rtcp::CommonHeader packet;
    while (next_packet < packet_end) {
      EXPECT_TRUE(packet.Parse(next_packet, packet_end - next_packet));
      next_packet = packet.NextPacket();
      if (packet.type() == rtcp::Bye::kPacketType)  // Main test expectation.
        EXPECT_EQ(0, packet_end - next_packet)
            << "Bye packet should be last in a compound RTCP packet.";
      if (next_packet == packet_end)  // Validate test was set correctly.
        EXPECT_EQ(packet.type(), rtcp::Bye::kPacketType)
            << "Last packet in this test expected to be Bye.";
    }

    return true;
  }));

  // Re-configure rtcp_sender_ with mock_transport_
  rtcp_sender_.reset(new RTCPSender(false, &clock_, receive_statistics_.get(),
                                    nullptr, nullptr, &mock_transport));
  rtcp_sender_->SetSSRC(kSenderSsrc);
  rtcp_sender_->SetRemoteSSRC(kRemoteSsrc);
  rtcp_sender_->SetTimestampOffset(kStartRtpTimestamp);
  rtcp_sender_->SetLastRtpTime(kRtpTimestamp, clock_.TimeInMilliseconds());

  // Set up XR VoIP metric to be included with BYE
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  RTCPVoIPMetric metric;
  EXPECT_EQ(0, rtcp_sender_->SetRTCPVoIPMetrics(&metric));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpBye));
}

TEST_F(RtcpSenderTest, SendXrWithTargetBitrate) {
  rtcp_sender_->SetRTCPStatus(RtcpMode::kCompound);
  const size_t kNumSpatialLayers = 2;
  const size_t kNumTemporalLayers = 2;
  BitrateAllocation allocation;
  for (size_t sl = 0; sl < kNumSpatialLayers; ++sl) {
    uint32_t start_bitrate_bps = (sl + 1) * 100000;
    for (size_t tl = 0; tl < kNumTemporalLayers; ++tl)
      allocation.SetBitrate(sl, tl, start_bitrate_bps + (tl * 20000));
  }
  rtcp_sender_->SetVideoBitrateAllocation(allocation);

  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state(), kRtcpReport));
  EXPECT_EQ(1, parser()->xr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser()->xr()->sender_ssrc());
  const rtc::Optional<rtcp::TargetBitrate>& target_bitrate =
      parser()->xr()->target_bitrate();
  ASSERT_TRUE(target_bitrate);
  const std::vector<rtcp::TargetBitrate::BitrateItem>& bitrates =
      target_bitrate->GetTargetBitrates();
  EXPECT_EQ(kNumSpatialLayers * kNumTemporalLayers, bitrates.size());

  for (size_t sl = 0; sl < kNumSpatialLayers; ++sl) {
    uint32_t start_bitrate_bps = (sl + 1) * 100000;
    for (size_t tl = 0; tl < kNumTemporalLayers; ++tl) {
      size_t index = (sl * kNumSpatialLayers) + tl;
      const rtcp::TargetBitrate::BitrateItem& item = bitrates[index];
      EXPECT_EQ(sl, item.spatial_layer);
      EXPECT_EQ(tl, item.temporal_layer);
      EXPECT_EQ(start_bitrate_bps + (tl * 20000),
                item.target_bitrate_kbps * 1000);
    }
  }
}

}  // namespace webrtc
