/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_egress.h"

#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_history.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::Field;
using ::testing::NiceMock;
using ::testing::StrictMock;

constexpr Timestamp kStartTime = Timestamp::Millis(123456789);
constexpr int kDefaultPayloadType = 100;
constexpr uint16_t kStartSequenceNumber = 33;
constexpr uint32_t kSsrc = 725242;
constexpr uint32_t kRtxSsrc = 12345;
enum : int {
  kTransportSequenceNumberExtensionId = 1,
  kVideoTimingExtensionExtensionId,
};

struct TestConfig {
  explicit TestConfig(bool with_overhead) : with_overhead(with_overhead) {}
  bool with_overhead = false;
};

class MockSendPacketObserver : public SendPacketObserver {
 public:
  MOCK_METHOD(void, OnSendPacket, (uint16_t, int64_t, uint32_t), (override));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD(void, OnAddPacket, (const RtpPacketSendInfo&), (override));
  MOCK_METHOD(void,
              OnTransportFeedback,
              (const rtcp::TransportFeedback&),
              (override));
};

class MockStreamDataCountersCallback : public StreamDataCountersCallback {
 public:
  MOCK_METHOD(void,
              DataCountersUpdated,
              (const StreamDataCounters& counters, uint32_t ssrc),
              (override));
};

class MockSendSideDelayObserver : public SendSideDelayObserver {
 public:
  MOCK_METHOD(void,
              SendSideDelayUpdated,
              (int, int, uint64_t, uint32_t),
              (override));
};

class FieldTrialConfig : public WebRtcKeyValueConfig {
 public:
  FieldTrialConfig() : overhead_enabled_(false) {}
  ~FieldTrialConfig() override {}

  void SetOverHeadEnabled(bool enabled) { overhead_enabled_ = enabled; }

  std::string Lookup(absl::string_view key) const override {
    if (key == "WebRTC-SendSideBwe-WithOverhead") {
      return overhead_enabled_ ? "Enabled" : "Disabled";
    }
    return "";
  }

 private:
  bool overhead_enabled_;
};

struct TransmittedPacket {
  TransmittedPacket(rtc::ArrayView<const uint8_t> data,
                    const PacketOptions& packet_options,
                    RtpHeaderExtensionMap* extensions)
      : packet(extensions), options(packet_options) {
    EXPECT_TRUE(packet.Parse(data));
  }
  RtpPacketReceived packet;
  PacketOptions options;
};

class TestTransport : public Transport {
 public:
  explicit TestTransport(RtpHeaderExtensionMap* extensions)
      : total_data_sent_(DataSize::Zero()), extensions_(extensions) {}
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override {
    total_data_sent_ += DataSize::Bytes(length);
    last_packet_.emplace(rtc::MakeArrayView(packet, length), options,
                         extensions_);
    return true;
  }

  bool SendRtcp(const uint8_t*, size_t) override { RTC_CHECK_NOTREACHED(); }

  absl::optional<TransmittedPacket> last_packet() { return last_packet_; }

 private:
  DataSize total_data_sent_;
  absl::optional<TransmittedPacket> last_packet_;
  RtpHeaderExtensionMap* const extensions_;
};

}  // namespace

class RtpSenderEgressTest : public ::testing::TestWithParam<TestConfig> {
 protected:
  RtpSenderEgressTest()
      : time_controller_(kStartTime),
        clock_(time_controller_.GetClock()),
        transport_(&header_extensions_),
        packet_history_(clock_, /*enable_rtx_padding_prioritization=*/true),
        sequence_number_(kStartSequenceNumber) {
    trials_.SetOverHeadEnabled(GetParam().with_overhead);
  }

  std::unique_ptr<RtpSenderEgress> CreateRtpSenderEgress() {
    return std::make_unique<RtpSenderEgress>(DefaultConfig(), &packet_history_);
  }

  RtpRtcp::Configuration DefaultConfig() {
    RtpRtcp::Configuration config;
    config.clock = clock_;
    config.outgoing_transport = &transport_;
    config.local_media_ssrc = kSsrc;
    config.rtx_send_ssrc = kRtxSsrc;
    config.fec_generator = nullptr;
    config.event_log = &mock_rtc_event_log_;
    config.send_packet_observer = &send_packet_observer_;
    config.rtp_stats_callback = &mock_rtp_stats_callback_;
    config.transport_feedback_callback = &feedback_observer_;
    config.populate_network2_timestamp = false;
    config.field_trials = &trials_;
    return config;
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(bool marker_bit,
                                                  int64_t capture_time_ms) {
    auto packet = std::make_unique<RtpPacketToSend>(&header_extensions_);
    packet->SetSsrc(kSsrc);
    packet->ReserveExtension<AbsoluteSendTime>();
    packet->ReserveExtension<TransmissionOffset>();
    packet->ReserveExtension<TransportSequenceNumber>();

    packet->SetPayloadType(kDefaultPayloadType);
    packet->set_packet_type(RtpPacketMediaType::kVideo);
    packet->SetMarker(marker_bit);
    packet->SetTimestamp(capture_time_ms * 90);
    packet->set_capture_time_ms(capture_time_ms);
    packet->SetSequenceNumber(sequence_number_++);
    return packet;
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket() {
    return BuildRtpPacket(/*marker_bit=*/true, clock_->CurrentTime().ms());
  }

  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  NiceMock<MockStreamDataCountersCallback> mock_rtp_stats_callback_;
  NiceMock<MockSendPacketObserver> send_packet_observer_;
  NiceMock<MockTransportFeedbackObserver> feedback_observer_;
  RtpHeaderExtensionMap header_extensions_;
  TestTransport transport_;
  RtpPacketHistory packet_history_;
  FieldTrialConfig trials_;
  uint16_t sequence_number_;
};

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverGetsCorrectByteCount) {
  constexpr size_t kRtpOverheadBytesPerPacket = 12 + 8;
  constexpr size_t kPayloadSize = 1400;
  const uint16_t kTransportSequenceNumber = 17;

  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::kUri);

  const size_t expected_bytes = GetParam().with_overhead
                                    ? kPayloadSize + kRtpOverheadBytesPerPacket
                                    : kPayloadSize;

  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(
          Field(&RtpPacketSendInfo::ssrc, kSsrc),
          Field(&RtpPacketSendInfo::transport_sequence_number,
                kTransportSequenceNumber),
          Field(&RtpPacketSendInfo::rtp_sequence_number, kStartSequenceNumber),
          Field(&RtpPacketSendInfo::length, expected_bytes),
          Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))));

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  packet->AllocatePayload(kPayloadSize);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  sender->SendPacket(packet.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, PacketOptionsIsRetransmitSetByPacketType) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
  media_packet->set_packet_type(RtpPacketMediaType::kVideo);
  sender->SendPacket(media_packet.get(), PacedPacketInfo());
  EXPECT_FALSE(transport_.last_packet()->options.is_retransmit);

  std::unique_ptr<RtpPacketToSend> retransmission = BuildRtpPacket();
  retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  sender->SendPacket(retransmission.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_packet()->options.is_retransmit);
}

TEST_P(RtpSenderEgressTest, DoesnSetIncludedInAllocationByDefault) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_FALSE(transport_.last_packet()->options.included_in_feedback);
  EXPECT_FALSE(transport_.last_packet()->options.included_in_allocation);
}

TEST_P(RtpSenderEgressTest,
       SetsIncludedInFeedbackWhenTransportSequenceNumberExtensionIsRegistered) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::kUri);
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_packet()->options.included_in_feedback);
}

TEST_P(
    RtpSenderEgressTest,
    SetsIncludedInAllocationWhenTransportSequenceNumberExtensionIsRegistered) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::kUri);
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_packet()->options.included_in_allocation);
}

TEST_P(RtpSenderEgressTest,
       SetsIncludedInAllocationWhenForcedAsPartOfAllocation) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  sender->ForceIncludeSendPacketsInAllocation(true);

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_FALSE(transport_.last_packet()->options.included_in_feedback);
  EXPECT_TRUE(transport_.last_packet()->options.included_in_allocation);
}

TEST_P(RtpSenderEgressTest, OnSendSideDelayUpdated) {
  StrictMock<MockSendSideDelayObserver> send_side_delay_observer;
  RtpRtcpInterface::Configuration config = DefaultConfig();
  config.send_side_delay_observer = &send_side_delay_observer;
  auto sender = std::make_unique<RtpSenderEgress>(config, &packet_history_);

  // Send packet with 10 ms send-side delay. The average, max and total should
  // be 10 ms.
  EXPECT_CALL(send_side_delay_observer,
              SendSideDelayUpdated(10, 10, 10, kSsrc));
  int64_t capture_time_ms = clock_->TimeInMilliseconds();
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms).get(),
                     PacedPacketInfo());

  // Send another packet with 20 ms delay. The average, max and total should be
  // 15, 20 and 30 ms respectively.
  EXPECT_CALL(send_side_delay_observer,
              SendSideDelayUpdated(15, 20, 30, kSsrc));
  capture_time_ms = clock_->TimeInMilliseconds();
  time_controller_.AdvanceTime(TimeDelta::Millis(20));
  sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms).get(),
                     PacedPacketInfo());

  // Send another packet at the same time, which replaces the last packet.
  // Since this packet has 0 ms delay, the average is now 5 ms and max is 10 ms.
  // The total counter stays the same though.
  // TODO(terelius): Is is not clear that this is the right behavior.
  EXPECT_CALL(send_side_delay_observer, SendSideDelayUpdated(5, 10, 30, kSsrc));
  capture_time_ms = clock_->TimeInMilliseconds();
  sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms).get(),
                     PacedPacketInfo());

  // Send a packet 1 second later. The earlier packets should have timed
  // out, so both max and average should be the delay of this packet. The total
  // keeps increasing.
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));
  EXPECT_CALL(send_side_delay_observer, SendSideDelayUpdated(1, 1, 31, kSsrc));
  capture_time_ms = clock_->TimeInMilliseconds();
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
  sender->SendPacket(BuildRtpPacket(/*marker=*/true, capture_time_ms).get(),
                     PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, WritesPacerExitToTimingExtension) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  header_extensions_.RegisterByUri(kVideoTimingExtensionExtensionId,
                                   VideoTimingExtension::kUri);

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<VideoTimingExtension>(VideoSendTiming{});

  const int kStoredTimeInMs = 100;
  time_controller_.AdvanceTime(TimeDelta::Millis(kStoredTimeInMs));
  sender->SendPacket(packet.get(), PacedPacketInfo());
  ASSERT_TRUE(transport_.last_packet().has_value());

  VideoSendTiming video_timing;
  EXPECT_TRUE(
      transport_.last_packet()->packet.GetExtension<VideoTimingExtension>(
          &video_timing));
  EXPECT_EQ(video_timing.pacer_exit_delta_ms, kStoredTimeInMs);
}

TEST_P(RtpSenderEgressTest, WritesNetwork2ToTimingExtension) {
  RtpRtcpInterface::Configuration rtp_config = DefaultConfig();
  rtp_config.populate_network2_timestamp = true;
  auto sender = std::make_unique<RtpSenderEgress>(rtp_config, &packet_history_);
  header_extensions_.RegisterByUri(kVideoTimingExtensionExtensionId,
                                   VideoTimingExtension::kUri);

  const uint16_t kPacerExitMs = 1234u;
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  VideoSendTiming send_timing = {};
  send_timing.pacer_exit_delta_ms = kPacerExitMs;
  packet->SetExtension<VideoTimingExtension>(send_timing);

  const int kStoredTimeInMs = 100;
  time_controller_.AdvanceTime(TimeDelta::Millis(kStoredTimeInMs));
  sender->SendPacket(packet.get(), PacedPacketInfo());
  ASSERT_TRUE(transport_.last_packet().has_value());

  VideoSendTiming video_timing;
  EXPECT_TRUE(
      transport_.last_packet()->packet.GetExtension<VideoTimingExtension>(
          &video_timing));
  EXPECT_EQ(video_timing.network2_timestamp_delta_ms, kStoredTimeInMs);
  EXPECT_EQ(video_timing.pacer_exit_delta_ms, kPacerExitMs);
}

TEST_P(RtpSenderEgressTest, OnSendPacketUpdated) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::kUri);

  const uint16_t kTransportSequenceNumber = 1;
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber,
                           clock_->TimeInMilliseconds(), kSsrc));
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  sender->SendPacket(packet.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, OnSendPacketNotUpdatedForRetransmits) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::kUri);

  const uint16_t kTransportSequenceNumber = 1;
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(0);
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  sender->SendPacket(packet.get(), PacedPacketInfo());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderEgressTest,
                         ::testing::Values(TestConfig(false),
                                           TestConfig(true)));

}  // namespace webrtc
