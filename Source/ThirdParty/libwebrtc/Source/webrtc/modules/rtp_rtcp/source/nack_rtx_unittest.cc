/*
*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <set>

#include "webrtc/api/call/transport.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

const int kVideoNackListSize = 30;
const uint32_t kTestSsrc = 3456;
const uint16_t kTestSequenceNumber = 2345;
const uint32_t kTestNumberOfPackets = 1350;
const int kTestNumberOfRtxPackets = 149;
const int kNumFrames = 30;
const int kPayloadType = 123;
const int kRtxPayloadType = 98;
const int64_t kMaxRttMs = 1000;

class VerifyingRtxReceiver : public RtpData {
 public:
  VerifyingRtxReceiver() {}

  int32_t OnReceivedPayloadData(
      const uint8_t* data,
      size_t size,
      const webrtc::WebRtcRTPHeader* rtp_header) override {
    if (!sequence_numbers_.empty())
      EXPECT_EQ(kTestSsrc, rtp_header->header.ssrc);
    sequence_numbers_.push_back(rtp_header->header.sequenceNumber);
    return 0;
  }
  std::list<uint16_t> sequence_numbers_;
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

class RtxLoopBackTransport : public webrtc::Transport {
 public:
  explicit RtxLoopBackTransport(uint32_t rtx_ssrc)
      : count_(0),
        packet_loss_(0),
        consecutive_drop_start_(0),
        consecutive_drop_end_(0),
        rtx_ssrc_(rtx_ssrc),
        count_rtx_ssrc_(0),
        rtp_payload_registry_(NULL),
        rtp_receiver_(NULL),
        module_(NULL) {}

  void SetSendModule(RtpRtcp* rtpRtcpModule,
                     RTPPayloadRegistry* rtp_payload_registry,
                     RtpReceiver* receiver) {
    module_ = rtpRtcpModule;
    rtp_payload_registry_ = rtp_payload_registry;
    rtp_receiver_ = receiver;
  }

  void DropEveryNthPacket(int n) { packet_loss_ = n; }

  void DropConsecutivePackets(int start, int total) {
    consecutive_drop_start_ = start;
    consecutive_drop_end_ = start + total;
    packet_loss_ = 0;
  }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    count_++;
    const unsigned char* ptr = static_cast<const unsigned char*>(data);
    uint32_t ssrc = (ptr[8] << 24) + (ptr[9] << 16) + (ptr[10] << 8) + ptr[11];
    if (ssrc == rtx_ssrc_)
      count_rtx_ssrc_++;
    uint16_t sequence_number = (ptr[2] << 8) + ptr[3];
    size_t packet_length = len;
    uint8_t restored_packet[1500];
    RTPHeader header;
    std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    if (!parser->Parse(ptr, len, &header)) {
      return false;
    }

    if (!rtp_payload_registry_->IsRtx(header)) {
      // Don't store retransmitted packets since we compare it to the list
      // created by the receiver.
      expected_sequence_numbers_.insert(expected_sequence_numbers_.end(),
                                        sequence_number);
    }
    if (packet_loss_ > 0) {
      if ((count_ % packet_loss_) == 0) {
        return true;
      }
    } else if (count_ >= consecutive_drop_start_ &&
               count_ < consecutive_drop_end_) {
      return true;
    }
    if (rtp_payload_registry_->IsRtx(header)) {
      // Remove the RTX header and parse the original RTP header.
      EXPECT_TRUE(rtp_payload_registry_->RestoreOriginalPacket(
          restored_packet, ptr, &packet_length, rtp_receiver_->SSRC(), header));
      if (!parser->Parse(restored_packet, packet_length, &header)) {
        return false;
      }
      ptr = restored_packet;
    } else {
      rtp_payload_registry_->SetIncomingPayloadType(header);
    }

    PayloadUnion payload_specific;
    if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                    &payload_specific)) {
      return false;
    }
    if (!rtp_receiver_->IncomingRtpPacket(header, ptr + header.headerLength,
                                          packet_length - header.headerLength,
                                          payload_specific, true)) {
      return false;
    }
    return true;
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    return module_->IncomingRtcpPacket((const uint8_t*)data, len) == 0;
  }
  int count_;
  int packet_loss_;
  int consecutive_drop_start_;
  int consecutive_drop_end_;
  uint32_t rtx_ssrc_;
  int count_rtx_ssrc_;
  RTPPayloadRegistry* rtp_payload_registry_;
  RtpReceiver* rtp_receiver_;
  RtpRtcp* module_;
  std::set<uint16_t> expected_sequence_numbers_;
};

class RtpRtcpRtxNackTest : public ::testing::Test {
 protected:
  RtpRtcpRtxNackTest()
      : rtp_rtcp_module_(nullptr),
        transport_(kTestSsrc + 1),
        receiver_(),
        payload_data_length(sizeof(payload_data)),
        fake_clock(123456),
        retransmission_rate_limiter_(&fake_clock, kMaxRttMs) {}
  ~RtpRtcpRtxNackTest() {}

  void SetUp() override {
    RtpRtcp::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &fake_clock;
    receive_statistics_.reset(ReceiveStatistics::Create(&fake_clock));
    configuration.receive_statistics = receive_statistics_.get();
    configuration.outgoing_transport = &transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;
    rtp_rtcp_module_ = RtpRtcp::CreateRtpRtcp(configuration);

    rtp_feedback_.reset(new TestRtpFeedback(rtp_rtcp_module_));

    rtp_receiver_.reset(RtpReceiver::CreateVideoReceiver(
        &fake_clock, &receiver_, rtp_feedback_.get(), &rtp_payload_registry_));

    rtp_rtcp_module_->SetSSRC(kTestSsrc);
    rtp_rtcp_module_->SetRTCPStatus(RtcpMode::kCompound);
    rtp_rtcp_module_->SetStorePacketsStatus(true, 600);
    EXPECT_EQ(0, rtp_rtcp_module_->SetSendingStatus(true));
    rtp_rtcp_module_->SetSequenceNumber(kTestSequenceNumber);
    rtp_rtcp_module_->SetStartTimestamp(111111);

    transport_.SetSendModule(rtp_rtcp_module_, &rtp_payload_registry_,
                             rtp_receiver_.get());

    VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(video_codec));
    video_codec.plType = kPayloadType;
    memcpy(video_codec.plName, "I420", 5);

    EXPECT_EQ(0, rtp_rtcp_module_->RegisterSendPayload(video_codec));
    rtp_rtcp_module_->SetRtxSendPayloadType(kRtxPayloadType, kPayloadType);
    EXPECT_EQ(0, rtp_payload_registry_.RegisterReceivePayload(video_codec));
    rtp_payload_registry_.SetRtxPayloadType(kRtxPayloadType, kPayloadType);

    for (size_t n = 0; n < payload_data_length; n++) {
      payload_data[n] = n % 10;
    }
  }

  int BuildNackList(uint16_t* nack_list) {
    receiver_.sequence_numbers_.sort();
    std::list<uint16_t> missing_sequence_numbers;
    std::list<uint16_t>::iterator it = receiver_.sequence_numbers_.begin();

    while (it != receiver_.sequence_numbers_.end()) {
      uint16_t sequence_number_1 = *it;
      ++it;
      if (it != receiver_.sequence_numbers_.end()) {
        uint16_t sequence_number_2 = *it;
        // Add all missing sequence numbers to list
        for (uint16_t i = sequence_number_1 + 1; i < sequence_number_2; ++i) {
          missing_sequence_numbers.push_back(i);
        }
      }
    }
    int n = 0;
    for (it = missing_sequence_numbers.begin();
         it != missing_sequence_numbers.end(); ++it) {
      nack_list[n++] = (*it);
    }
    return n;
  }

  bool ExpectedPacketsReceived() {
    std::list<uint16_t> received_sorted;
    std::copy(receiver_.sequence_numbers_.begin(),
              receiver_.sequence_numbers_.end(),
              std::back_inserter(received_sorted));
    received_sorted.sort();
    return received_sorted.size() ==
               transport_.expected_sequence_numbers_.size() &&
           std::equal(received_sorted.begin(), received_sorted.end(),
                      transport_.expected_sequence_numbers_.begin());
  }

  void RunRtxTest(RtxMode rtx_method, int loss) {
    rtp_payload_registry_.SetRtxSsrc(kTestSsrc + 1);
    rtp_rtcp_module_->SetRtxSendStatus(rtx_method);
    rtp_rtcp_module_->SetRtxSsrc(kTestSsrc + 1);
    transport_.DropEveryNthPacket(loss);
    uint32_t timestamp = 3000;
    uint16_t nack_list[kVideoNackListSize];
    for (int frame = 0; frame < kNumFrames; ++frame) {
      EXPECT_TRUE(rtp_rtcp_module_->SendOutgoingData(
          webrtc::kVideoFrameDelta, kPayloadType, timestamp, timestamp / 90,
          payload_data, payload_data_length, nullptr, nullptr, nullptr));
      // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
      fake_clock.AdvanceTimeMilliseconds(5);
      int length = BuildNackList(nack_list);
      if (length > 0)
        rtp_rtcp_module_->SendNACK(nack_list, length);
      fake_clock.AdvanceTimeMilliseconds(28);  //  33ms - 5ms delay.
      rtp_rtcp_module_->Process();
      // Prepare next frame.
      timestamp += 3000;
    }
    receiver_.sequence_numbers_.sort();
  }

  void TearDown() override { delete rtp_rtcp_module_; }

  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  RTPPayloadRegistry rtp_payload_registry_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
  RtpRtcp* rtp_rtcp_module_;
  std::unique_ptr<TestRtpFeedback> rtp_feedback_;
  RtxLoopBackTransport transport_;
  VerifyingRtxReceiver receiver_;
  uint8_t payload_data[65000];
  size_t payload_data_length;
  SimulatedClock fake_clock;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpRtxNackTest, LongNackList) {
  const int kNumPacketsToDrop = 900;
  const int kNumRequiredRtcp = 4;
  uint32_t timestamp = 3000;
  uint16_t nack_list[kNumPacketsToDrop];
  // Disable StorePackets to be able to set a larger packet history.
  rtp_rtcp_module_->SetStorePacketsStatus(false, 0);
  // Enable StorePackets with a packet history of 2000 packets.
  rtp_rtcp_module_->SetStorePacketsStatus(true, 2000);
  // Drop 900 packets from the second one so that we get a NACK list which is
  // big enough to require 4 RTCP packets to be fully transmitted to the sender.
  transport_.DropConsecutivePackets(2, kNumPacketsToDrop);
  // Send 30 frames which at the default size is roughly what we need to get
  // enough packets.
  for (int frame = 0; frame < kNumFrames; ++frame) {
    EXPECT_TRUE(rtp_rtcp_module_->SendOutgoingData(
        webrtc::kVideoFrameDelta, kPayloadType, timestamp, timestamp / 90,
        payload_data, payload_data_length, nullptr, nullptr, nullptr));
    // Prepare next frame.
    timestamp += 3000;
    fake_clock.AdvanceTimeMilliseconds(33);
    rtp_rtcp_module_->Process();
  }
  EXPECT_FALSE(transport_.expected_sequence_numbers_.empty());
  EXPECT_FALSE(receiver_.sequence_numbers_.empty());
  size_t last_receive_count = receiver_.sequence_numbers_.size();
  int length = BuildNackList(nack_list);
  for (int i = 0; i < kNumRequiredRtcp - 1; ++i) {
    rtp_rtcp_module_->SendNACK(nack_list, length);
    EXPECT_GT(receiver_.sequence_numbers_.size(), last_receive_count);
    last_receive_count = receiver_.sequence_numbers_.size();
    EXPECT_FALSE(ExpectedPacketsReceived());
  }
  rtp_rtcp_module_->SendNACK(nack_list, length);
  EXPECT_GT(receiver_.sequence_numbers_.size(), last_receive_count);
  EXPECT_TRUE(ExpectedPacketsReceived());
}

TEST_F(RtpRtcpRtxNackTest, RtxNack) {
  RunRtxTest(kRtxRetransmitted, 10);
  EXPECT_EQ(kTestSequenceNumber, *(receiver_.sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
            *(receiver_.sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, receiver_.sequence_numbers_.size());
  EXPECT_EQ(kTestNumberOfRtxPackets, transport_.count_rtx_ssrc_);
  EXPECT_TRUE(ExpectedPacketsReceived());
}

}  // namespace webrtc
