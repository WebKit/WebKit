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
#include "modules/rtp_rtcp/include/flexfec_sender.h"
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

using ::testing::_;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::StrictMock;

constexpr Timestamp kStartTime = Timestamp::Millis(123456789);
constexpr int kDefaultPayloadType = 100;
constexpr int kFlexfectPayloadType = 110;
constexpr uint16_t kStartSequenceNumber = 33;
constexpr uint32_t kSsrc = 725242;
constexpr uint32_t kRtxSsrc = 12345;
constexpr uint32_t kFlexFecSsrc = 23456;
enum : int {
  kTransportSequenceNumberExtensionId = 1,
  kAbsoluteSendTimeExtensionId,
  kTransmissionOffsetExtensionId,
  kVideoTimingExtensionId,
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

class FieldTrialConfig : public FieldTrialsView {
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
    packet->set_capture_time(Timestamp::Millis(capture_time_ms));
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
                                   TransportSequenceNumber::Uri());

  const size_t expected_bytes = GetParam().with_overhead
                                    ? kPayloadSize + kRtpOverheadBytesPerPacket
                                    : kPayloadSize;

  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(
          Field(&RtpPacketSendInfo::media_ssrc, kSsrc),
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
  retransmission->set_retransmitted_sequence_number(
      media_packet->SequenceNumber());
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
                                   TransportSequenceNumber::Uri());
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_packet()->options.included_in_feedback);
}

TEST_P(
    RtpSenderEgressTest,
    SetsIncludedInAllocationWhenTransportSequenceNumberExtensionIsRegistered) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());
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
  header_extensions_.RegisterByUri(kVideoTimingExtensionId,
                                   VideoTimingExtension::Uri());

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
  header_extensions_.RegisterByUri(kVideoTimingExtensionId,
                                   VideoTimingExtension::Uri());

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
                                   TransportSequenceNumber::Uri());

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
                                   TransportSequenceNumber::Uri());

  const uint16_t kTransportSequenceNumber = 1;
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(0);
  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->set_retransmitted_sequence_number(packet->SequenceNumber());
  sender->SendPacket(packet.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, ReportsFecRate) {
  constexpr int kNumPackets = 10;
  constexpr TimeDelta kTimeBetweenPackets = TimeDelta::Millis(33);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  DataSize total_fec_data_sent = DataSize::Zero();
  // Send some packets, alternating between media and FEC.
  for (size_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
    media_packet->set_packet_type(RtpPacketMediaType::kVideo);
    media_packet->SetPayloadSize(500);
    sender->SendPacket(media_packet.get(), PacedPacketInfo());

    std::unique_ptr<RtpPacketToSend> fec_packet = BuildRtpPacket();
    fec_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
    fec_packet->SetPayloadSize(123);
    sender->SendPacket(fec_packet.get(), PacedPacketInfo());
    total_fec_data_sent += DataSize::Bytes(fec_packet->size());

    time_controller_.AdvanceTime(kTimeBetweenPackets);
  }

  EXPECT_NEAR(
      (sender->GetSendRates()[RtpPacketMediaType::kForwardErrorCorrection])
          .bps(),
      (total_fec_data_sent / (kTimeBetweenPackets * kNumPackets)).bps(), 500);
}

TEST_P(RtpSenderEgressTest, BitrateCallbacks) {
  class MockBitrateStaticsObserver : public BitrateStatisticsObserver {
   public:
    MOCK_METHOD(void, Notify, (uint32_t, uint32_t, uint32_t), (override));
  } observer;

  RtpRtcpInterface::Configuration config = DefaultConfig();
  config.send_bitrate_observer = &observer;
  auto sender = std::make_unique<RtpSenderEgress>(config, &packet_history_);

  // Simulate kNumPackets sent with kPacketInterval intervals, with the
  // number of packets selected so that we fill (but don't overflow) the one
  // second averaging window.
  const TimeDelta kWindowSize = TimeDelta::Seconds(1);
  const TimeDelta kPacketInterval = TimeDelta::Millis(20);
  const int kNumPackets = (kWindowSize - kPacketInterval) / kPacketInterval;

  DataSize total_data_sent = DataSize::Zero();

  // Send all but on of the packets, expect a call for each packet but don't
  // verify bitrate yet (noisy measurements in the beginning).
  for (int i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
    packet->SetPayloadSize(500);
    // Mark all packets as retransmissions - will cause total and retransmission
    // rates to be equal.
    packet->set_packet_type(RtpPacketMediaType::kRetransmission);
    packet->set_retransmitted_sequence_number(packet->SequenceNumber());
    total_data_sent += DataSize::Bytes(packet->size());

    EXPECT_CALL(observer, Notify(_, _, kSsrc))
        .WillOnce([&](uint32_t total_bitrate_bps,
                      uint32_t retransmission_bitrate_bps, uint32_t /*ssrc*/) {
          TimeDelta window_size = i * kPacketInterval + TimeDelta::Millis(1);
          // If there is just a single data point, there is no well defined
          // averaging window so a bitrate of zero will be reported.
          const double expected_bitrate_bps =
              i == 0 ? 0.0 : (total_data_sent / window_size).bps();
          EXPECT_NEAR(total_bitrate_bps, expected_bitrate_bps, 500);
          EXPECT_NEAR(retransmission_bitrate_bps, expected_bitrate_bps, 500);
        });

    sender->SendPacket(packet.get(), PacedPacketInfo());
    time_controller_.AdvanceTime(kPacketInterval);
  }
}

TEST_P(RtpSenderEgressTest, DoesNotPutNotRetransmittablePacketsInHistory) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->set_allow_retransmission(false);
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_FALSE(packet_history_.GetPacketState(packet->SequenceNumber()));
}

TEST_P(RtpSenderEgressTest, PutsRetransmittablePacketsInHistory) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->set_allow_retransmission(true);
  sender->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_TRUE(packet_history_.GetPacketState(packet->SequenceNumber()));
}

TEST_P(RtpSenderEgressTest, DoesNotPutNonMediaInHistory) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  // Non-media packets, even when marked as retransmittable, are not put into
  // the packet history.
  std::unique_ptr<RtpPacketToSend> retransmission = BuildRtpPacket();
  retransmission->set_allow_retransmission(true);
  retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  retransmission->set_retransmitted_sequence_number(
      retransmission->SequenceNumber());
  sender->SendPacket(retransmission.get(), PacedPacketInfo());
  EXPECT_FALSE(
      packet_history_.GetPacketState(retransmission->SequenceNumber()));

  std::unique_ptr<RtpPacketToSend> fec = BuildRtpPacket();
  fec->set_allow_retransmission(true);
  fec->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  sender->SendPacket(fec.get(), PacedPacketInfo());
  EXPECT_FALSE(packet_history_.GetPacketState(fec->SequenceNumber()));

  std::unique_ptr<RtpPacketToSend> padding = BuildRtpPacket();
  padding->set_allow_retransmission(true);
  padding->set_packet_type(RtpPacketMediaType::kPadding);
  sender->SendPacket(padding.get(), PacedPacketInfo());
  EXPECT_FALSE(packet_history_.GetPacketState(padding->SequenceNumber()));
}

TEST_P(RtpSenderEgressTest, UpdatesSendStatusOfRetransmittedPackets) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  // Send a packet, putting it in the history.
  std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
  media_packet->set_allow_retransmission(true);
  sender->SendPacket(media_packet.get(), PacedPacketInfo());
  EXPECT_TRUE(packet_history_.GetPacketState(media_packet->SequenceNumber()));

  // Simulate a retransmission, marking the packet as pending.
  std::unique_ptr<RtpPacketToSend> retransmission =
      packet_history_.GetPacketAndMarkAsPending(media_packet->SequenceNumber());
  retransmission->set_retransmitted_sequence_number(
      media_packet->SequenceNumber());
  retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  EXPECT_TRUE(packet_history_.GetPacketState(media_packet->SequenceNumber()));

  // Simulate packet leaving pacer, the packet should be marked as non-pending.
  sender->SendPacket(retransmission.get(), PacedPacketInfo());
  EXPECT_TRUE(packet_history_.GetPacketState(media_packet->SequenceNumber()));
}

TEST_P(RtpSenderEgressTest, StreamDataCountersCallbacks) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  const RtpPacketCounter kEmptyCounter;
  RtpPacketCounter expected_transmitted_counter;
  RtpPacketCounter expected_retransmission_counter;

  // Send a media packet.
  std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
  media_packet->SetPayloadSize(6);
  media_packet->SetSequenceNumber(kStartSequenceNumber);
  expected_transmitted_counter.packets += 1;
  expected_transmitted_counter.payload_bytes += media_packet->payload_size();
  expected_transmitted_counter.header_bytes += media_packet->headers_size();

  EXPECT_CALL(
      mock_rtp_stats_callback_,
      DataCountersUpdated(AllOf(Field(&StreamDataCounters::transmitted,
                                      expected_transmitted_counter),
                                Field(&StreamDataCounters::retransmitted,
                                      expected_retransmission_counter),
                                Field(&StreamDataCounters::fec, kEmptyCounter)),
                          kSsrc));
  sender->SendPacket(media_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Send a retransmission. Retransmissions are counted into both transmitted
  // and retransmitted packet statistics.
  std::unique_ptr<RtpPacketToSend> retransmission_packet = BuildRtpPacket();
  retransmission_packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  retransmission_packet->SetSequenceNumber(kStartSequenceNumber);
  retransmission_packet->set_retransmitted_sequence_number(
      kStartSequenceNumber);
  media_packet->SetPayloadSize(7);
  expected_transmitted_counter.packets += 1;
  expected_transmitted_counter.payload_bytes +=
      retransmission_packet->payload_size();
  expected_transmitted_counter.header_bytes +=
      retransmission_packet->headers_size();

  expected_retransmission_counter.packets += 1;
  expected_retransmission_counter.payload_bytes +=
      retransmission_packet->payload_size();
  expected_retransmission_counter.header_bytes +=
      retransmission_packet->headers_size();

  EXPECT_CALL(
      mock_rtp_stats_callback_,
      DataCountersUpdated(AllOf(Field(&StreamDataCounters::transmitted,
                                      expected_transmitted_counter),
                                Field(&StreamDataCounters::retransmitted,
                                      expected_retransmission_counter),
                                Field(&StreamDataCounters::fec, kEmptyCounter)),
                          kSsrc));
  sender->SendPacket(retransmission_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Send a padding packet.
  std::unique_ptr<RtpPacketToSend> padding_packet = BuildRtpPacket();
  padding_packet->set_packet_type(RtpPacketMediaType::kPadding);
  padding_packet->SetPadding(224);
  padding_packet->SetSequenceNumber(kStartSequenceNumber + 1);
  expected_transmitted_counter.packets += 1;
  expected_transmitted_counter.padding_bytes += padding_packet->padding_size();
  expected_transmitted_counter.header_bytes += padding_packet->headers_size();

  EXPECT_CALL(
      mock_rtp_stats_callback_,
      DataCountersUpdated(AllOf(Field(&StreamDataCounters::transmitted,
                                      expected_transmitted_counter),
                                Field(&StreamDataCounters::retransmitted,
                                      expected_retransmission_counter),
                                Field(&StreamDataCounters::fec, kEmptyCounter)),
                          kSsrc));
  sender->SendPacket(padding_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(RtpSenderEgressTest, StreamDataCountersCallbacksFec) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  const RtpPacketCounter kEmptyCounter;
  RtpPacketCounter expected_transmitted_counter;
  RtpPacketCounter expected_fec_counter;

  // Send a media packet.
  std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
  media_packet->SetPayloadSize(6);
  expected_transmitted_counter.packets += 1;
  expected_transmitted_counter.payload_bytes += media_packet->payload_size();
  expected_transmitted_counter.header_bytes += media_packet->headers_size();

  EXPECT_CALL(
      mock_rtp_stats_callback_,
      DataCountersUpdated(
          AllOf(Field(&StreamDataCounters::transmitted,
                      expected_transmitted_counter),
                Field(&StreamDataCounters::retransmitted, kEmptyCounter),
                Field(&StreamDataCounters::fec, expected_fec_counter)),
          kSsrc));
  sender->SendPacket(media_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Send and FEC packet. FEC is counted into both transmitted and FEC packet
  // statistics.
  std::unique_ptr<RtpPacketToSend> fec_packet = BuildRtpPacket();
  fec_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  fec_packet->SetPayloadSize(6);
  expected_transmitted_counter.packets += 1;
  expected_transmitted_counter.payload_bytes += fec_packet->payload_size();
  expected_transmitted_counter.header_bytes += fec_packet->headers_size();

  expected_fec_counter.packets += 1;
  expected_fec_counter.payload_bytes += fec_packet->payload_size();
  expected_fec_counter.header_bytes += fec_packet->headers_size();

  EXPECT_CALL(
      mock_rtp_stats_callback_,
      DataCountersUpdated(
          AllOf(Field(&StreamDataCounters::transmitted,
                      expected_transmitted_counter),
                Field(&StreamDataCounters::retransmitted, kEmptyCounter),
                Field(&StreamDataCounters::fec, expected_fec_counter)),
          kSsrc));
  sender->SendPacket(fec_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(RtpSenderEgressTest, UpdatesDataCounters) {
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  const RtpPacketCounter kEmptyCounter;

  // Send a media packet.
  std::unique_ptr<RtpPacketToSend> media_packet = BuildRtpPacket();
  media_packet->SetPayloadSize(6);
  sender->SendPacket(media_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Send an RTX retransmission packet.
  std::unique_ptr<RtpPacketToSend> rtx_packet = BuildRtpPacket();
  rtx_packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtx_packet->SetSsrc(kRtxSsrc);
  rtx_packet->SetPayloadSize(7);
  rtx_packet->set_retransmitted_sequence_number(media_packet->SequenceNumber());
  sender->SendPacket(rtx_packet.get(), PacedPacketInfo());
  time_controller_.AdvanceTime(TimeDelta::Zero());

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  sender->GetDataCounters(&rtp_stats, &rtx_stats);

  EXPECT_EQ(rtp_stats.transmitted.packets, 1u);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, media_packet->payload_size());
  EXPECT_EQ(rtp_stats.transmitted.padding_bytes, media_packet->padding_size());
  EXPECT_EQ(rtp_stats.transmitted.header_bytes, media_packet->headers_size());
  EXPECT_EQ(rtp_stats.retransmitted, kEmptyCounter);
  EXPECT_EQ(rtp_stats.fec, kEmptyCounter);

  // Retransmissions are counted both into transmitted and retransmitted
  // packet counts.
  EXPECT_EQ(rtx_stats.transmitted.packets, 1u);
  EXPECT_EQ(rtx_stats.transmitted.payload_bytes, rtx_packet->payload_size());
  EXPECT_EQ(rtx_stats.transmitted.padding_bytes, rtx_packet->padding_size());
  EXPECT_EQ(rtx_stats.transmitted.header_bytes, rtx_packet->headers_size());
  EXPECT_EQ(rtx_stats.retransmitted, rtx_stats.transmitted);
  EXPECT_EQ(rtx_stats.fec, kEmptyCounter);
}

TEST_P(RtpSenderEgressTest, SendPacketUpdatesExtensions) {
  header_extensions_.RegisterByUri(kVideoTimingExtensionId,
                                   VideoTimingExtension::Uri());
  header_extensions_.RegisterByUri(kAbsoluteSendTimeExtensionId,
                                   AbsoluteSendTime::Uri());
  header_extensions_.RegisterByUri(kTransmissionOffsetExtensionId,
                                   TransmissionOffset::Uri());
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->set_packetization_finish_time(clock_->CurrentTime());

  const int32_t kDiffMs = 10;
  time_controller_.AdvanceTime(TimeDelta::Millis(kDiffMs));

  sender->SendPacket(packet.get(), PacedPacketInfo());

  RtpPacketReceived received_packet = transport_.last_packet()->packet;

  EXPECT_EQ(received_packet.GetExtension<TransmissionOffset>(), kDiffMs * 90);

  EXPECT_EQ(received_packet.GetExtension<AbsoluteSendTime>(),
            AbsoluteSendTime::To24Bits(clock_->CurrentTime()));

  VideoSendTiming timing;
  EXPECT_TRUE(received_packet.GetExtension<VideoTimingExtension>(&timing));
  EXPECT_EQ(timing.pacer_exit_delta_ms, kDiffMs);
}

TEST_P(RtpSenderEgressTest, SendPacketSetsPacketOptions) {
  const uint16_t kPacketId = 42;
  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());

  std::unique_ptr<RtpPacketToSend> packet = BuildRtpPacket();
  packet->SetExtension<TransportSequenceNumber>(kPacketId);
  EXPECT_CALL(send_packet_observer_, OnSendPacket);
  sender->SendPacket(packet.get(), PacedPacketInfo());

  PacketOptions packet_options = transport_.last_packet()->options;

  EXPECT_EQ(packet_options.packet_id, kPacketId);
  EXPECT_TRUE(packet_options.included_in_allocation);
  EXPECT_TRUE(packet_options.included_in_feedback);
  EXPECT_FALSE(packet_options.is_retransmit);

  // Send another packet as retransmission, verify options are populated.
  std::unique_ptr<RtpPacketToSend> retransmission = BuildRtpPacket();
  retransmission->SetExtension<TransportSequenceNumber>(kPacketId + 1);
  retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  retransmission->set_retransmitted_sequence_number(packet->SequenceNumber());
  sender->SendPacket(retransmission.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_packet()->options.is_retransmit);
}

TEST_P(RtpSenderEgressTest, SendPacketUpdatesStats) {
  const size_t kPayloadSize = 1000;
  StrictMock<MockSendSideDelayObserver> send_side_delay_observer;

  const rtc::ArrayView<const RtpExtensionSize> kNoRtpHeaderExtensionSizes;
  FlexfecSender flexfec(kFlexfectPayloadType, kFlexFecSsrc, kSsrc, /*mid=*/"",
                        /*header_extensions=*/{}, kNoRtpHeaderExtensionSizes,
                        /*rtp_state=*/nullptr, time_controller_.GetClock());
  RtpRtcpInterface::Configuration config = DefaultConfig();
  config.fec_generator = &flexfec;
  config.send_side_delay_observer = &send_side_delay_observer;
  auto sender = std::make_unique<RtpSenderEgress>(config, &packet_history_);

  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());

  const int64_t capture_time_ms = clock_->TimeInMilliseconds();

  std::unique_ptr<RtpPacketToSend> video_packet = BuildRtpPacket();
  video_packet->set_packet_type(RtpPacketMediaType::kVideo);
  video_packet->SetPayloadSize(kPayloadSize);
  video_packet->SetExtension<TransportSequenceNumber>(1);

  std::unique_ptr<RtpPacketToSend> rtx_packet = BuildRtpPacket();
  rtx_packet->SetSsrc(kRtxSsrc);
  rtx_packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtx_packet->set_retransmitted_sequence_number(video_packet->SequenceNumber());
  rtx_packet->SetPayloadSize(kPayloadSize);
  rtx_packet->SetExtension<TransportSequenceNumber>(2);

  std::unique_ptr<RtpPacketToSend> fec_packet = BuildRtpPacket();
  fec_packet->SetSsrc(kFlexFecSsrc);
  fec_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  fec_packet->SetPayloadSize(kPayloadSize);
  fec_packet->SetExtension<TransportSequenceNumber>(3);

  const int64_t kDiffMs = 25;
  time_controller_.AdvanceTime(TimeDelta::Millis(kDiffMs));

  EXPECT_CALL(send_side_delay_observer,
              SendSideDelayUpdated(kDiffMs, kDiffMs, kDiffMs, kSsrc));
  EXPECT_CALL(
      send_side_delay_observer,
      SendSideDelayUpdated(kDiffMs, kDiffMs, 2 * kDiffMs, kFlexFecSsrc));

  EXPECT_CALL(send_packet_observer_, OnSendPacket(1, capture_time_ms, kSsrc));

  sender->SendPacket(video_packet.get(), PacedPacketInfo());

  // Send packet observer not called for padding/retransmissions.
  EXPECT_CALL(send_packet_observer_, OnSendPacket(2, _, _)).Times(0);
  sender->SendPacket(rtx_packet.get(), PacedPacketInfo());

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(3, capture_time_ms, kFlexFecSsrc));
  sender->SendPacket(fec_packet.get(), PacedPacketInfo());

  time_controller_.AdvanceTime(TimeDelta::Zero());
  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  sender->GetDataCounters(&rtp_stats, &rtx_stats);
  EXPECT_EQ(rtp_stats.transmitted.packets, 2u);
  EXPECT_EQ(rtp_stats.fec.packets, 1u);
  EXPECT_EQ(rtx_stats.retransmitted.packets, 1u);
}

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverWithRetransmission) {
  const uint16_t kTransportSequenceNumber = 17;
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());
  std::unique_ptr<RtpPacketToSend> retransmission = BuildRtpPacket();
  retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  retransmission->SetExtension<TransportSequenceNumber>(
      kTransportSequenceNumber);
  uint16_t retransmitted_seq = retransmission->SequenceNumber() - 2;
  retransmission->set_retransmitted_sequence_number(retransmitted_seq);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(
          Field(&RtpPacketSendInfo::media_ssrc, kSsrc),
          Field(&RtpPacketSendInfo::rtp_sequence_number, retransmitted_seq),
          Field(&RtpPacketSendInfo::transport_sequence_number,
                kTransportSequenceNumber))));
  sender->SendPacket(retransmission.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverWithRtxRetransmission) {
  const uint16_t kTransportSequenceNumber = 17;
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());

  std::unique_ptr<RtpPacketToSend> rtx_retransmission = BuildRtpPacket();
  rtx_retransmission->SetSsrc(kRtxSsrc);
  rtx_retransmission->SetExtension<TransportSequenceNumber>(
      kTransportSequenceNumber);
  rtx_retransmission->set_packet_type(RtpPacketMediaType::kRetransmission);
  uint16_t rtx_retransmitted_seq = rtx_retransmission->SequenceNumber() - 2;
  rtx_retransmission->set_retransmitted_sequence_number(rtx_retransmitted_seq);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(
          Field(&RtpPacketSendInfo::media_ssrc, kSsrc),
          Field(&RtpPacketSendInfo::rtp_sequence_number, rtx_retransmitted_seq),
          Field(&RtpPacketSendInfo::transport_sequence_number,
                kTransportSequenceNumber))));
  sender->SendPacket(rtx_retransmission.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverPadding) {
  const uint16_t kTransportSequenceNumber = 17;
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());
  std::unique_ptr<RtpPacketToSend> padding = BuildRtpPacket();
  padding->SetPadding(224);
  padding->set_packet_type(RtpPacketMediaType::kPadding);
  padding->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(Field(&RtpPacketSendInfo::media_ssrc, absl::nullopt),
                        Field(&RtpPacketSendInfo::transport_sequence_number,
                              kTransportSequenceNumber))));
  sender->SendPacket(padding.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverRtxPadding) {
  const uint16_t kTransportSequenceNumber = 17;
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());

  std::unique_ptr<RtpPacketToSend> rtx_padding = BuildRtpPacket();
  rtx_padding->SetPadding(224);
  rtx_padding->SetSsrc(kRtxSsrc);
  rtx_padding->set_packet_type(RtpPacketMediaType::kPadding);
  rtx_padding->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);

  std::unique_ptr<RtpSenderEgress> sender = CreateRtpSenderEgress();
  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(Field(&RtpPacketSendInfo::media_ssrc, absl::nullopt),
                        Field(&RtpPacketSendInfo::transport_sequence_number,
                              kTransportSequenceNumber))));
  sender->SendPacket(rtx_padding.get(), PacedPacketInfo());
}

TEST_P(RtpSenderEgressTest, TransportFeedbackObserverFec) {
  const uint16_t kTransportSequenceNumber = 17;
  header_extensions_.RegisterByUri(kTransportSequenceNumberExtensionId,
                                   TransportSequenceNumber::Uri());

  std::unique_ptr<RtpPacketToSend> fec_packet = BuildRtpPacket();
  fec_packet->SetSsrc(kFlexFecSsrc);
  fec_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  fec_packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);

  const rtc::ArrayView<const RtpExtensionSize> kNoRtpHeaderExtensionSizes;
  FlexfecSender flexfec(kFlexfectPayloadType, kFlexFecSsrc, kSsrc, /*mid=*/"",
                        /*header_extensions=*/{}, kNoRtpHeaderExtensionSizes,
                        /*rtp_state=*/nullptr, time_controller_.GetClock());
  RtpRtcpInterface::Configuration config = DefaultConfig();
  config.fec_generator = &flexfec;
  auto sender = std::make_unique<RtpSenderEgress>(config, &packet_history_);
  EXPECT_CALL(
      feedback_observer_,
      OnAddPacket(AllOf(Field(&RtpPacketSendInfo::media_ssrc, absl::nullopt),
                        Field(&RtpPacketSendInfo::transport_sequence_number,
                              kTransportSequenceNumber))));
  sender->SendPacket(fec_packet.get(), PacedPacketInfo());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderEgressTest,
                         ::testing::Values(TestConfig(false),
                                           TestConfig(true)));

}  // namespace webrtc
