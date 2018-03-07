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

#include "api/call/transport.h"
#include "call/rtp_stream_receiver_controller.h"
#include "call/rtx_receive_stream.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/rate_limiter.h"
#include "test/gtest.h"

namespace webrtc {

const int kVideoNackListSize = 30;
const uint32_t kTestSsrc = 3456;
const uint32_t kTestRtxSsrc = kTestSsrc + 1;
const uint16_t kTestSequenceNumber = 2345;
const uint32_t kTestNumberOfPackets = 1350;
const int kTestNumberOfRtxPackets = 149;
const int kNumFrames = 30;
const int kPayloadType = 123;
const int kRtxPayloadType = 98;
const int64_t kMaxRttMs = 1000;

class VerifyingMediaStream : public RtpPacketSinkInterface {
 public:
  VerifyingMediaStream() {}

  void OnRtpPacket(const RtpPacketReceived& packet) override {
    if (!sequence_numbers_.empty())
      EXPECT_EQ(kTestSsrc, packet.Ssrc());

    sequence_numbers_.push_back(packet.SequenceNumber());
  }
  std::list<uint16_t> sequence_numbers_;
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
        module_(NULL) {}

  void SetSendModule(RtpRtcp* rtpRtcpModule) {
    module_ = rtpRtcpModule;
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
    RtpPacketReceived packet;
    if (!packet.Parse(data, len))
      return false;
    if (packet.Ssrc() == rtx_ssrc_) {
      count_rtx_ssrc_++;
    } else {
      // For non-RTX packets only.
      expected_sequence_numbers_.insert(expected_sequence_numbers_.end(),
                                        packet.SequenceNumber());
    }
    if (packet_loss_ > 0) {
      if ((count_ % packet_loss_) == 0) {
        return true;
      }
    } else if (count_ >= consecutive_drop_start_ &&
               count_ < consecutive_drop_end_) {
      return true;
    }
    EXPECT_TRUE(stream_receiver_controller_.OnRtpPacket(packet));
    return true;
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    module_->IncomingRtcpPacket((const uint8_t*)data, len);
    return true;
  }
  int count_;
  int packet_loss_;
  int consecutive_drop_start_;
  int consecutive_drop_end_;
  uint32_t rtx_ssrc_;
  int count_rtx_ssrc_;
  RtpRtcp* module_;
  RtpStreamReceiverController stream_receiver_controller_;
  std::set<uint16_t> expected_sequence_numbers_;
};

class RtpRtcpRtxNackTest : public ::testing::Test {
 protected:
  RtpRtcpRtxNackTest()
      : rtp_rtcp_module_(nullptr),
        transport_(kTestRtxSsrc),
        rtx_stream_(&media_stream_,
                    rtx_associated_payload_types_,
                    kTestSsrc),
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

    rtp_rtcp_module_->SetSSRC(kTestSsrc);
    rtp_rtcp_module_->SetRTCPStatus(RtcpMode::kCompound);
    rtp_rtcp_module_->SetStorePacketsStatus(true, 600);
    EXPECT_EQ(0, rtp_rtcp_module_->SetSendingStatus(true));
    rtp_rtcp_module_->SetSequenceNumber(kTestSequenceNumber);
    rtp_rtcp_module_->SetStartTimestamp(111111);

    // Used for NACK processing.
    // TODO(nisse): Unclear on which side? It's confusing to use a
    // single rtp_rtcp module for both send and receive side.
    rtp_rtcp_module_->SetRemoteSSRC(kTestSsrc);

    rtp_rtcp_module_->RegisterVideoSendPayload(kPayloadType, "video");
    rtp_rtcp_module_->SetRtxSendPayloadType(kRtxPayloadType, kPayloadType);
    transport_.SetSendModule(rtp_rtcp_module_);
    media_receiver_ = transport_.stream_receiver_controller_.CreateReceiver(
        kTestSsrc, &media_stream_);

    for (size_t n = 0; n < payload_data_length; n++) {
      payload_data[n] = n % 10;
    }
  }

  int BuildNackList(uint16_t* nack_list) {
    media_stream_.sequence_numbers_.sort();
    std::list<uint16_t> missing_sequence_numbers;
    std::list<uint16_t>::iterator it = media_stream_.sequence_numbers_.begin();

    while (it != media_stream_.sequence_numbers_.end()) {
      uint16_t sequence_number_1 = *it;
      ++it;
      if (it != media_stream_.sequence_numbers_.end()) {
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
    std::copy(media_stream_.sequence_numbers_.begin(),
              media_stream_.sequence_numbers_.end(),
              std::back_inserter(received_sorted));
    received_sorted.sort();
    return received_sorted.size() ==
               transport_.expected_sequence_numbers_.size() &&
           std::equal(received_sorted.begin(), received_sorted.end(),
                      transport_.expected_sequence_numbers_.begin());
  }

  void RunRtxTest(RtxMode rtx_method, int loss) {
    rtx_receiver_ = transport_.stream_receiver_controller_.CreateReceiver(
        kTestRtxSsrc, &rtx_stream_);
    rtp_rtcp_module_->SetRtxSendStatus(rtx_method);
    rtp_rtcp_module_->SetRtxSsrc(kTestRtxSsrc);
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
    media_stream_.sequence_numbers_.sort();
  }

  void TearDown() override { delete rtp_rtcp_module_; }

  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  RtpRtcp* rtp_rtcp_module_;
  RtxLoopBackTransport transport_;
  const std::map<int, int> rtx_associated_payload_types_ =
      {{kRtxPayloadType, kPayloadType}};
  VerifyingMediaStream media_stream_;
  RtxReceiveStream rtx_stream_;
  uint8_t payload_data[65000];
  size_t payload_data_length;
  SimulatedClock fake_clock;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RtpStreamReceiverInterface> media_receiver_;
  std::unique_ptr<RtpStreamReceiverInterface> rtx_receiver_;
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
  EXPECT_FALSE(media_stream_.sequence_numbers_.empty());
  size_t last_receive_count = media_stream_.sequence_numbers_.size();
  int length = BuildNackList(nack_list);
  for (int i = 0; i < kNumRequiredRtcp - 1; ++i) {
    rtp_rtcp_module_->SendNACK(nack_list, length);
    EXPECT_GT(media_stream_.sequence_numbers_.size(), last_receive_count);
    last_receive_count = media_stream_.sequence_numbers_.size();
    EXPECT_FALSE(ExpectedPacketsReceived());
  }
  rtp_rtcp_module_->SendNACK(nack_list, length);
  EXPECT_GT(media_stream_.sequence_numbers_.size(), last_receive_count);
  EXPECT_TRUE(ExpectedPacketsReceived());
}

TEST_F(RtpRtcpRtxNackTest, RtxNack) {
  RunRtxTest(kRtxRetransmitted, 10);
  EXPECT_EQ(kTestSequenceNumber, *(media_stream_.sequence_numbers_.begin()));
  EXPECT_EQ(kTestSequenceNumber + kTestNumberOfPackets - 1,
            *(media_stream_.sequence_numbers_.rbegin()));
  EXPECT_EQ(kTestNumberOfPackets, media_stream_.sequence_numbers_.size());
  EXPECT_EQ(kTestNumberOfRtxPackets, transport_.count_rtx_ssrc_);
  EXPECT_TRUE(ExpectedPacketsReceived());
}

}  // namespace webrtc
