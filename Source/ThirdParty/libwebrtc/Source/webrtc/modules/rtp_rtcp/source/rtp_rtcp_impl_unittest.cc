/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <set>

#include "webrtc/base/rate_limiter.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345;
const uint32_t kReceiverSsrc = 0x23456;
const int64_t kOneWayNetworkDelayMs = 100;
const uint8_t kBaseLayerTid = 0;
const uint8_t kHigherLayerTid = 1;
const uint16_t kSequenceNumber = 100;
const int64_t kMaxRttMs = 1000;

class RtcpRttStatsTestImpl : public RtcpRttStats {
 public:
  RtcpRttStatsTestImpl() : rtt_ms_(0) {}
  virtual ~RtcpRttStatsTestImpl() {}

  void OnRttUpdate(int64_t rtt_ms) override { rtt_ms_ = rtt_ms; }
  int64_t LastProcessedRtt() const override { return rtt_ms_; }
  int64_t rtt_ms_;
};

class SendTransport : public Transport,
                      public NullRtpData {
 public:
  SendTransport()
      : receiver_(NULL),
        clock_(NULL),
        delay_ms_(0),
        rtp_packets_sent_(0) {
  }

  void SetRtpRtcpModule(ModuleRtpRtcpImpl* receiver) {
    receiver_ = receiver;
  }
  void SimulateNetworkDelay(int64_t delay_ms, SimulatedClock* clock) {
    clock_ = clock;
    delay_ms_ = delay_ms;
  }
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    EXPECT_TRUE(parser->Parse(static_cast<const uint8_t*>(data), len, &header));
    ++rtp_packets_sent_;
    last_rtp_header_ = header;
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override {
    test::RtcpPacketParser parser;
    parser.Parse(data, len);
    last_nack_list_ = parser.nack()->packet_ids();

    if (clock_) {
      clock_->AdvanceTimeMilliseconds(delay_ms_);
    }
    EXPECT_TRUE(receiver_);
    EXPECT_EQ(0, receiver_->IncomingRtcpPacket(data, len));
    return true;
  }
  ModuleRtpRtcpImpl* receiver_;
  SimulatedClock* clock_;
  int64_t delay_ms_;
  int rtp_packets_sent_;
  RTPHeader last_rtp_header_;
  std::vector<uint16_t> last_nack_list_;
};

class RtpRtcpModule : public RtcpPacketTypeCounterObserver {
 public:
  explicit RtpRtcpModule(SimulatedClock* clock)
      : receive_statistics_(ReceiveStatistics::Create(clock)),
        remote_ssrc_(0),
        retransmission_rate_limiter_(clock, kMaxRttMs) {
    RtpRtcp::Configuration config;
    config.audio = false;
    config.clock = clock;
    config.outgoing_transport = &transport_;
    config.receive_statistics = receive_statistics_.get();
    config.rtcp_packet_type_counter_observer = this;
    config.rtt_stats = &rtt_stats_;
    config.retransmission_rate_limiter = &retransmission_rate_limiter_;

    impl_.reset(new ModuleRtpRtcpImpl(config));
    impl_->SetRTCPStatus(RtcpMode::kCompound);

    transport_.SimulateNetworkDelay(kOneWayNetworkDelayMs, clock);
  }

  RtcpPacketTypeCounter packets_sent_;
  RtcpPacketTypeCounter packets_received_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  SendTransport transport_;
  RtcpRttStatsTestImpl rtt_stats_;
  std::unique_ptr<ModuleRtpRtcpImpl> impl_;
  uint32_t remote_ssrc_;
  RateLimiter retransmission_rate_limiter_;

  void SetRemoteSsrc(uint32_t ssrc) {
    remote_ssrc_ = ssrc;
    impl_->SetRemoteSSRC(ssrc);
  }

  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    counter_map_[ssrc] = packet_counter;
  }

  RtcpPacketTypeCounter RtcpSent() {
    // RTCP counters for remote SSRC.
    return counter_map_[remote_ssrc_];
  }

  RtcpPacketTypeCounter RtcpReceived() {
    // Received RTCP stats for (own) local SSRC.
    return counter_map_[impl_->SSRC()];
  }
  int RtpSent() {
    return transport_.rtp_packets_sent_;
  }
  uint16_t LastRtpSequenceNumber() {
    return transport_.last_rtp_header_.sequenceNumber;
  }
  std::vector<uint16_t> LastNackListSent() {
    return transport_.last_nack_list_;
  }

 private:
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map_;
};
}  // namespace

class RtpRtcpImplTest : public ::testing::Test {
 protected:
  RtpRtcpImplTest()
      : clock_(133590000000000),
        sender_(&clock_),
        receiver_(&clock_) {
    // Send module.
    sender_.impl_->SetSSRC(kSenderSsrc);
    EXPECT_EQ(0, sender_.impl_->SetSendingStatus(true));
    sender_.impl_->SetSendingMediaStatus(true);
    sender_.SetRemoteSsrc(kReceiverSsrc);
    sender_.impl_->SetSequenceNumber(kSequenceNumber);
    sender_.impl_->SetStorePacketsStatus(true, 100);

    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.plType = 100;
    strncpy(codec_.plName, "VP8", 3);
    codec_.width = 320;
    codec_.height = 180;
    EXPECT_EQ(0, sender_.impl_->RegisterSendPayload(codec_));

    // Receive module.
    EXPECT_EQ(0, receiver_.impl_->SetSendingStatus(false));
    receiver_.impl_->SetSendingMediaStatus(false);
    receiver_.impl_->SetSSRC(kReceiverSsrc);
    receiver_.SetRemoteSsrc(kSenderSsrc);
    // Transport settings.
    sender_.transport_.SetRtpRtcpModule(receiver_.impl_.get());
    receiver_.transport_.SetRtpRtcpModule(sender_.impl_.get());
  }
  SimulatedClock clock_;
  RtpRtcpModule sender_;
  RtpRtcpModule receiver_;
  VideoCodec codec_;

  void SendFrame(const RtpRtcpModule* module, uint8_t tid) {
    RTPVideoHeaderVP8 vp8_header = {};
    vp8_header.temporalIdx = tid;
    RTPVideoHeader rtp_video_header;
    rtp_video_header.width = codec_.width;
    rtp_video_header.height = codec_.height;
    rtp_video_header.rotation = kVideoRotation_0;
    rtp_video_header.playout_delay = {-1, -1};
    rtp_video_header.is_first_packet_in_frame = true;
    rtp_video_header.simulcastIdx = 0;
    rtp_video_header.codec = kRtpVideoVp8;
    rtp_video_header.codecHeader = {vp8_header};

    const uint8_t payload[100] = {0};
    EXPECT_EQ(true, module->impl_->SendOutgoingData(
                     kVideoFrameKey, codec_.plType, 0, 0, payload,
                     sizeof(payload), nullptr, &rtp_video_header, nullptr));
  }

  void IncomingRtcpNack(const RtpRtcpModule* module, uint16_t sequence_number) {
    bool sender = module->impl_->SSRC() == kSenderSsrc;
    rtcp::Nack nack;
    uint16_t list[1];
    list[0] = sequence_number;
    const uint16_t kListLength = sizeof(list) / sizeof(list[0]);
    nack.SetSenderSsrc(sender ? kReceiverSsrc : kSenderSsrc);
    nack.SetMediaSsrc(sender ? kSenderSsrc : kReceiverSsrc);
    nack.SetPacketIds(list, kListLength);
    rtc::Buffer packet = nack.Build();
    EXPECT_EQ(0, module->impl_->IncomingRtcpPacket(packet.data(),
                                                   packet.size()));
  }
};

TEST_F(RtpRtcpImplTest, SetSelectiveRetransmissions_BaseLayer) {
  sender_.impl_->SetSelectiveRetransmissions(kRetransmitBaseLayer);
  EXPECT_EQ(kRetransmitBaseLayer, sender_.impl_->SelectiveRetransmissions());

  // Send frames.
  EXPECT_EQ(0, sender_.RtpSent());
  SendFrame(&sender_, kBaseLayerTid);    // kSequenceNumber
  SendFrame(&sender_, kHigherLayerTid);  // kSequenceNumber + 1
  SendFrame(&sender_, kNoTemporalIdx);   // kSequenceNumber + 2
  EXPECT_EQ(3, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());

  // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
  clock_.AdvanceTimeMilliseconds(5);

  // Frame with kBaseLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber);
  EXPECT_EQ(4, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber, sender_.LastRtpSequenceNumber());
  // Frame with kHigherLayerTid not re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 1);
  EXPECT_EQ(4, sender_.RtpSent());
  // Frame with kNoTemporalIdx re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 2);
  EXPECT_EQ(5, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());
}

TEST_F(RtpRtcpImplTest, SetSelectiveRetransmissions_HigherLayers) {
  const uint8_t kSetting = kRetransmitBaseLayer + kRetransmitHigherLayers;
  sender_.impl_->SetSelectiveRetransmissions(kSetting);
  EXPECT_EQ(kSetting, sender_.impl_->SelectiveRetransmissions());

  // Send frames.
  EXPECT_EQ(0, sender_.RtpSent());
  SendFrame(&sender_, kBaseLayerTid);    // kSequenceNumber
  SendFrame(&sender_, kHigherLayerTid);  // kSequenceNumber + 1
  SendFrame(&sender_, kNoTemporalIdx);   // kSequenceNumber + 2
  EXPECT_EQ(3, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());

  // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
  clock_.AdvanceTimeMilliseconds(5);

  // Frame with kBaseLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber);
  EXPECT_EQ(4, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber, sender_.LastRtpSequenceNumber());
  // Frame with kHigherLayerTid re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 1);
  EXPECT_EQ(5, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 1, sender_.LastRtpSequenceNumber());
  // Frame with kNoTemporalIdx re-sent.
  IncomingRtcpNack(&sender_, kSequenceNumber + 2);
  EXPECT_EQ(6, sender_.RtpSent());
  EXPECT_EQ(kSequenceNumber + 2, sender_.LastRtpSequenceNumber());
}

TEST_F(RtpRtcpImplTest, Rtt) {
  RTPHeader header;
  header.timestamp = 1;
  header.sequenceNumber = 123;
  header.ssrc = kSenderSsrc;
  header.headerLength = 12;
  receiver_.receive_statistics_->IncomingPacket(header, 100, false);

  // Send Frame before sending an SR.
  SendFrame(&sender_, kBaseLayerTid);
  // Sender module should send an SR.
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Receiver module should send a RR with a response to the last received SR.
  clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  int64_t rtt;
  int64_t avg_rtt;
  int64_t min_rtt;
  int64_t max_rtt;
  EXPECT_EQ(0,
      sender_.impl_->RTT(kReceiverSsrc, &rtt, &avg_rtt, &min_rtt, &max_rtt));
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, avg_rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, min_rtt, 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, max_rtt, 1);

  // No RTT from other ssrc.
  EXPECT_EQ(-1,
      sender_.impl_->RTT(kReceiverSsrc+1, &rtt, &avg_rtt, &min_rtt, &max_rtt));

  // Verify RTT from rtt_stats config.
  EXPECT_EQ(0, sender_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0, sender_.impl_->rtt_ms());
  sender_.impl_->Process();
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, sender_.rtt_stats_.LastProcessedRtt(),
              1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, sender_.impl_->rtt_ms(), 1);
}

TEST_F(RtpRtcpImplTest, SetRtcpXrRrtrStatus) {
  EXPECT_FALSE(receiver_.impl_->RtcpXrRrtrStatus());
  receiver_.impl_->SetRtcpXrRrtrStatus(true);
  EXPECT_TRUE(receiver_.impl_->RtcpXrRrtrStatus());
}

TEST_F(RtpRtcpImplTest, RttForReceiverOnly) {
  receiver_.impl_->SetRtcpXrRrtrStatus(true);

  // Receiver module should send a Receiver time reference report (RTRR).
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpReport));

  // Sender module should send a response to the last received RTRR (DLRR).
  clock_.AdvanceTimeMilliseconds(1000);
  // Send Frame before sending a SR.
  SendFrame(&sender_, kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendRTCP(kRtcpReport));

  // Verify RTT.
  EXPECT_EQ(0, receiver_.rtt_stats_.LastProcessedRtt());
  EXPECT_EQ(0, receiver_.impl_->rtt_ms());
  receiver_.impl_->Process();
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs,
              receiver_.rtt_stats_.LastProcessedRtt(), 1);
  EXPECT_NEAR(2 * kOneWayNetworkDelayMs, receiver_.impl_->rtt_ms(), 1);
}

TEST_F(RtpRtcpImplTest, NoSrBeforeMedia) {
  // Ignore fake transport delays in this test.
  sender_.transport_.SimulateNetworkDelay(0, &clock_);
  receiver_.transport_.SimulateNetworkDelay(0, &clock_);

  sender_.impl_->Process();
  EXPECT_EQ(-1, sender_.RtcpSent().first_packet_time_ms);

  // Verify no SR is sent before media has been sent, RR should still be sent
  // from the receiving module though.
  clock_.AdvanceTimeMilliseconds(2000);
  int64_t current_time = clock_.TimeInMilliseconds();
  sender_.impl_->Process();
  receiver_.impl_->Process();
  EXPECT_EQ(-1, sender_.RtcpSent().first_packet_time_ms);
  EXPECT_EQ(receiver_.RtcpSent().first_packet_time_ms, current_time);

  SendFrame(&sender_, kBaseLayerTid);
  EXPECT_EQ(sender_.RtcpSent().first_packet_time_ms, current_time);
}

TEST_F(RtpRtcpImplTest, RtcpPacketTypeCounter_Nack) {
  EXPECT_EQ(-1, receiver_.RtcpSent().first_packet_time_ms);
  EXPECT_EQ(-1, sender_.RtcpReceived().first_packet_time_ms);
  EXPECT_EQ(0U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_packets);

  // Receive module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, receiver_.RtcpSent().nack_packets);
  EXPECT_GT(receiver_.RtcpSent().first_packet_time_ms, -1);

  // Send module receives the NACK.
  EXPECT_EQ(1U, sender_.RtcpReceived().nack_packets);
  EXPECT_GT(sender_.RtcpReceived().first_packet_time_ms, -1);
}

TEST_F(RtpRtcpImplTest, RtcpPacketTypeCounter_FirAndPli) {
  EXPECT_EQ(0U, sender_.RtcpReceived().fir_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().fir_packets);
  // Receive module sends a FIR.
  EXPECT_EQ(0, receiver_.impl_->SendRTCP(kRtcpFir));
  EXPECT_EQ(1U, receiver_.RtcpSent().fir_packets);
  // Send module receives the FIR.
  EXPECT_EQ(1U, sender_.RtcpReceived().fir_packets);

  // Receive module sends a FIR and PLI.
  std::set<RTCPPacketType> packet_types;
  packet_types.insert(kRtcpFir);
  packet_types.insert(kRtcpPli);
  EXPECT_EQ(0, receiver_.impl_->SendCompoundRTCP(packet_types));
  EXPECT_EQ(2U, receiver_.RtcpSent().fir_packets);
  EXPECT_EQ(1U, receiver_.RtcpSent().pli_packets);
  // Send module receives the FIR and PLI.
  EXPECT_EQ(2U, sender_.RtcpReceived().fir_packets);
  EXPECT_EQ(1U, sender_.RtcpReceived().pli_packets);
}

TEST_F(RtpRtcpImplTest, AddStreamDataCounters) {
  StreamDataCounters rtp;
  const int64_t kStartTimeMs = 1;
  rtp.first_packet_time_ms = kStartTimeMs;
  rtp.transmitted.packets = 1;
  rtp.transmitted.payload_bytes = 1;
  rtp.transmitted.header_bytes = 2;
  rtp.transmitted.padding_bytes = 3;
  EXPECT_EQ(rtp.transmitted.TotalBytes(), rtp.transmitted.payload_bytes +
                                          rtp.transmitted.header_bytes +
                                          rtp.transmitted.padding_bytes);

  StreamDataCounters rtp2;
  rtp2.first_packet_time_ms = -1;
  rtp2.transmitted.packets = 10;
  rtp2.transmitted.payload_bytes = 10;
  rtp2.retransmitted.header_bytes = 4;
  rtp2.retransmitted.payload_bytes = 5;
  rtp2.retransmitted.padding_bytes = 6;
  rtp2.retransmitted.packets = 7;
  rtp2.fec.packets = 8;

  StreamDataCounters sum = rtp;
  sum.Add(rtp2);
  EXPECT_EQ(kStartTimeMs, sum.first_packet_time_ms);
  EXPECT_EQ(11U, sum.transmitted.packets);
  EXPECT_EQ(11U, sum.transmitted.payload_bytes);
  EXPECT_EQ(2U, sum.transmitted.header_bytes);
  EXPECT_EQ(3U, sum.transmitted.padding_bytes);
  EXPECT_EQ(4U, sum.retransmitted.header_bytes);
  EXPECT_EQ(5U, sum.retransmitted.payload_bytes);
  EXPECT_EQ(6U, sum.retransmitted.padding_bytes);
  EXPECT_EQ(7U, sum.retransmitted.packets);
  EXPECT_EQ(8U, sum.fec.packets);
  EXPECT_EQ(sum.transmitted.TotalBytes(),
            rtp.transmitted.TotalBytes() + rtp2.transmitted.TotalBytes());

  StreamDataCounters rtp3;
  rtp3.first_packet_time_ms = kStartTimeMs + 10;
  sum.Add(rtp3);
  EXPECT_EQ(kStartTimeMs, sum.first_packet_time_ms);  // Holds oldest time.
}

TEST_F(RtpRtcpImplTest, SendsInitialNackList) {
  // Send module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));
}

TEST_F(RtpRtcpImplTest, SendsExtendedNackList) {
  // Send module sends a NACK.
  const uint16_t kNackLength = 1;
  uint16_t nack_list[kNackLength] = {123};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));

  // Same list not re-send.
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123));

  // Only extended list sent.
  const uint16_t kNackExtLength = 2;
  uint16_t nack_list_ext[kNackExtLength] = {123, 124};
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list_ext, kNackExtLength));
  EXPECT_EQ(2U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(124));
}

TEST_F(RtpRtcpImplTest, ReSendsNackListAfterRttMs) {
  sender_.transport_.SimulateNetworkDelay(0, &clock_);
  // Send module sends a NACK.
  const uint16_t kNackLength = 2;
  uint16_t nack_list[kNackLength] = {123, 125};
  EXPECT_EQ(0U, sender_.RtcpSent().nack_packets);
  // Send Frame before sending a compound RTCP that starts with SR.
  SendFrame(&sender_, kBaseLayerTid);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123, 125));

  // Same list not re-send, rtt interval has not passed.
  const int kStartupRttMs = 100;
  clock_.AdvanceTimeMilliseconds(kStartupRttMs);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, sender_.RtcpSent().nack_packets);

  // Rtt interval passed, full list sent.
  clock_.AdvanceTimeMilliseconds(1);
  EXPECT_EQ(0, sender_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(2U, sender_.RtcpSent().nack_packets);
  EXPECT_THAT(sender_.LastNackListSent(), ElementsAre(123, 125));
}

TEST_F(RtpRtcpImplTest, UniqueNackRequests) {
  receiver_.transport_.SimulateNetworkDelay(0, &clock_);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(0U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(0U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_EQ(0, receiver_.RtcpSent().UniqueNackRequestsInPercent());

  // Receive module sends NACK request.
  const uint16_t kNackLength = 4;
  uint16_t nack_list[kNackLength] = {10, 11, 13, 18};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list, kNackLength));
  EXPECT_EQ(1U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(4U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(4U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_THAT(receiver_.LastNackListSent(), ElementsAre(10, 11, 13, 18));

  // Send module receives the request.
  EXPECT_EQ(1U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(4U, sender_.RtcpReceived().nack_requests);
  EXPECT_EQ(4U, sender_.RtcpReceived().unique_nack_requests);
  EXPECT_EQ(100, sender_.RtcpReceived().UniqueNackRequestsInPercent());

  // Receive module sends new request with duplicated packets.
  const int kStartupRttMs = 100;
  clock_.AdvanceTimeMilliseconds(kStartupRttMs + 1);
  const uint16_t kNackLength2 = 4;
  uint16_t nack_list2[kNackLength2] = {11, 18, 20, 21};
  EXPECT_EQ(0, receiver_.impl_->SendNACK(nack_list2, kNackLength2));
  EXPECT_EQ(2U, receiver_.RtcpSent().nack_packets);
  EXPECT_EQ(8U, receiver_.RtcpSent().nack_requests);
  EXPECT_EQ(6U, receiver_.RtcpSent().unique_nack_requests);
  EXPECT_THAT(receiver_.LastNackListSent(), ElementsAre(11, 18, 20, 21));

  // Send module receives the request.
  EXPECT_EQ(2U, sender_.RtcpReceived().nack_packets);
  EXPECT_EQ(8U, sender_.RtcpReceived().nack_requests);
  EXPECT_EQ(6U, sender_.RtcpReceived().unique_nack_requests);
  EXPECT_EQ(75, sender_.RtcpReceived().UniqueNackRequestsInPercent());
}
}  // namespace webrtc
