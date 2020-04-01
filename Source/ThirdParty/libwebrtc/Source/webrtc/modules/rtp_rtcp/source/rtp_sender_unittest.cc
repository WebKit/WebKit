/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender.h"

#include <memory>
#include <vector>

#include "api/rtc_event_log/rtc_event.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender_egress.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/rate_limiter.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtp_header_parser.h"

namespace webrtc {

namespace {
enum : int {  // The first valid value is 1.
  kAbsoluteSendTimeExtensionId = 1,
  kAudioLevelExtensionId,
  kGenericDescriptorId00,
  kGenericDescriptorId01,
  kMidExtensionId,
  kRepairedRidExtensionId,
  kRidExtensionId,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
};

const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint32_t kRtxSsrc = 12345;
const uint32_t kFlexFecSsrc = 45678;
const uint16_t kTransportSequenceNumber = 1;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
const int64_t kDefaultExpectedRetransmissionTimeMs = 125;
const char kNoRid[] = "";
const char kNoMid[] = "";

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return (((time_ms << 18) + 500) / 1000) & 0x00ffffff;
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() : total_bytes_sent_(0) {
    receivers_extensions_.Register<TransmissionOffset>(
        kTransmissionTimeOffsetExtensionId);
    receivers_extensions_.Register<AbsoluteSendTime>(
        kAbsoluteSendTimeExtensionId);
    receivers_extensions_.Register<TransportSequenceNumber>(
        kTransportSequenceNumberExtensionId);
    receivers_extensions_.Register<VideoOrientation>(kVideoRotationExtensionId);
    receivers_extensions_.Register<AudioLevel>(kAudioLevelExtensionId);
    receivers_extensions_.Register<VideoTimingExtension>(
        kVideoTimingExtensionId);
    receivers_extensions_.Register<RtpMid>(kMidExtensionId);
    receivers_extensions_.Register<RtpGenericFrameDescriptorExtension00>(
        kGenericDescriptorId00);
    receivers_extensions_.Register<RtpGenericFrameDescriptorExtension01>(
        kGenericDescriptorId01);
    receivers_extensions_.Register<RtpStreamId>(kRidExtensionId);
    receivers_extensions_.Register<RepairedRtpStreamId>(
        kRepairedRidExtensionId);
  }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    last_options_ = options;
    total_bytes_sent_ += len;
    sent_packets_.push_back(RtpPacketReceived(&receivers_extensions_));
    EXPECT_TRUE(sent_packets_.back().Parse(data, len));
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  const RtpPacketReceived& last_sent_packet() { return sent_packets_.back(); }
  int packets_sent() { return sent_packets_.size(); }

  size_t total_bytes_sent_;
  PacketOptions last_options_;
  std::vector<RtpPacketReceived> sent_packets_;

 private:
  RtpHeaderExtensionMap receivers_extensions_;
};

MATCHER_P(SameRtcEventTypeAs, value, "") {
  return value == arg->GetType();
}

struct TestConfig {
  explicit TestConfig(bool with_overhead) : with_overhead(with_overhead) {}
  bool with_overhead = false;
};

std::string ToFieldTrialString(TestConfig config) {
  std::string field_trials;
  if (config.with_overhead) {
    field_trials += "WebRTC-SendSideBwe-WithOverhead/Enabled/";
  }
  return field_trials;
}

class MockRtpPacketPacer : public RtpPacketSender {
 public:
  MockRtpPacketPacer() {}
  virtual ~MockRtpPacketPacer() {}

  MOCK_METHOD1(EnqueuePackets,
               void(std::vector<std::unique_ptr<RtpPacketToSend>>));

  MOCK_METHOD2(CreateProbeCluster, void(int bitrate_bps, int cluster_id));

  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Resume, void());
  MOCK_METHOD1(SetCongestionWindow,
               void(absl::optional<int64_t> congestion_window_bytes));
  MOCK_METHOD1(UpdateOutstandingData, void(int64_t outstanding_bytes));
  MOCK_METHOD1(SetAccountForAudioPackets, void(bool account_for_audio));
};

class MockSendSideDelayObserver : public SendSideDelayObserver {
 public:
  MOCK_METHOD4(SendSideDelayUpdated, void(int, int, uint64_t, uint32_t));
};

class MockSendPacketObserver : public SendPacketObserver {
 public:
  MOCK_METHOD3(OnSendPacket, void(uint16_t, int64_t, uint32_t));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD1(OnAddPacket, void(const RtpPacketSendInfo&));
  MOCK_METHOD1(OnTransportFeedback, void(const rtcp::TransportFeedback&));
};

class MockOverheadObserver : public OverheadObserver {
 public:
  MOCK_METHOD1(OnOverheadChanged, void(size_t overhead_bytes_per_packet));
};

class StreamDataTestCallback : public StreamDataCountersCallback {
 public:
  StreamDataTestCallback()
      : StreamDataCountersCallback(), ssrc_(0), counters_() {}
  ~StreamDataTestCallback() override = default;

  void DataCountersUpdated(const StreamDataCounters& counters,
                           uint32_t ssrc) override {
    ssrc_ = ssrc;
    counters_ = counters;
  }

  uint32_t ssrc_;
  StreamDataCounters counters_;

  void MatchPacketCounter(const RtpPacketCounter& expected,
                          const RtpPacketCounter& actual) {
    EXPECT_EQ(expected.payload_bytes, actual.payload_bytes);
    EXPECT_EQ(expected.header_bytes, actual.header_bytes);
    EXPECT_EQ(expected.padding_bytes, actual.padding_bytes);
    EXPECT_EQ(expected.packets, actual.packets);
  }

  void Matches(uint32_t ssrc, const StreamDataCounters& counters) {
    EXPECT_EQ(ssrc, ssrc_);
    MatchPacketCounter(counters.transmitted, counters_.transmitted);
    MatchPacketCounter(counters.retransmitted, counters_.retransmitted);
    EXPECT_EQ(counters.fec.packets, counters_.fec.packets);
  }
};

// Mimics ModuleRtpRtcp::RtpSenderContext.
// TODO(sprang): Split up unit tests and test these components individually
// wherever possible.
struct RtpSenderContext {
  explicit RtpSenderContext(const RtpRtcp::Configuration& config)
      : packet_history_(config.clock),
        packet_sender_(config, &packet_history_),
        non_paced_sender_(&packet_sender_),
        packet_generator_(
            config,
            &packet_history_,
            config.paced_sender ? config.paced_sender : &non_paced_sender_) {}
  RtpPacketHistory packet_history_;
  RtpSenderEgress packet_sender_;
  RtpSenderEgress::NonPacedPacketSender non_paced_sender_;
  RTPSender packet_generator_;
};

}  // namespace

class RtpSenderTest : public ::testing::TestWithParam<TestConfig> {
 protected:
  RtpSenderTest()
      : fake_clock_(kStartTime),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        flexfec_sender_(0,
                        kFlexFecSsrc,
                        kSsrc,
                        "",
                        std::vector<RtpExtension>(),
                        std::vector<RtpExtensionSize>(),
                        nullptr,
                        &fake_clock_),
        kMarkerBit(true),
        field_trials_(ToFieldTrialString(GetParam())) {}

  void SetUp() override { SetUpRtpSender(true, false, false); }

  RTPSender* rtp_sender() {
    RTC_DCHECK(rtp_sender_context_);
    return &rtp_sender_context_->packet_generator_;
  }

  RtpSenderEgress* rtp_egress() {
    RTC_DCHECK(rtp_sender_context_);
    return &rtp_sender_context_->packet_sender_;
  }

  void SetUpRtpSender(bool pacer,
                      bool populate_network2,
                      bool always_send_mid_and_rid) {
    RtpRtcp::Configuration config;
    config.clock = &fake_clock_;
    config.outgoing_transport = &transport_;
    config.local_media_ssrc = kSsrc;
    config.rtx_send_ssrc = kRtxSsrc;
    config.fec_generator = &flexfec_sender_;
    config.event_log = &mock_rtc_event_log_;
    config.send_packet_observer = &send_packet_observer_;
    config.retransmission_rate_limiter = &retransmission_rate_limiter_;
    config.paced_sender = pacer ? &mock_paced_sender_ : nullptr;
    config.populate_network2_timestamp = populate_network2;
    config.rtp_stats_callback = &rtp_stats_callback_;
    config.always_send_mid_and_rid = always_send_mid_and_rid;
    rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);
    rtp_sender()->SetSequenceNumber(kSeqNum);
    rtp_sender()->SetTimestampOffset(0);
  }

  SimulatedClock fake_clock_;
  NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  MockRtpPacketPacer mock_paced_sender_;
  StrictMock<MockSendPacketObserver> send_packet_observer_;
  StrictMock<MockTransportFeedbackObserver> feedback_observer_;
  RateLimiter retransmission_rate_limiter_;
  FlexfecSender flexfec_sender_;

  std::unique_ptr<RtpSenderContext> rtp_sender_context_;

  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  test::ScopedFieldTrials field_trials_;
  StreamDataTestCallback rtp_stats_callback_;

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(int payload_type,
                                                  bool marker_bit,
                                                  uint32_t timestamp,
                                                  int64_t capture_time_ms) {
    auto packet = rtp_sender()->AllocatePacket();
    packet->SetPayloadType(payload_type);
    packet->set_packet_type(RtpPacketMediaType::kVideo);
    packet->SetMarker(marker_bit);
    packet->SetTimestamp(timestamp);
    packet->set_capture_time_ms(capture_time_ms);
    EXPECT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
    return packet;
  }

  std::unique_ptr<RtpPacketToSend> SendPacket(int64_t capture_time_ms,
                                              int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    auto packet =
        BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
    packet->AllocatePayload(payload_length);
    packet->set_allow_retransmission(true);

    // Packet should be stored in a send bucket.
    EXPECT_TRUE(rtp_sender()->SendToNetwork(
        std::make_unique<RtpPacketToSend>(*packet)));
    return packet;
  }

  std::unique_ptr<RtpPacketToSend> SendGenericPacket() {
    const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
    return SendPacket(kCaptureTimeMs, sizeof(kPayloadData));
  }

  size_t GenerateAndSendPadding(size_t target_size_bytes) {
    size_t generated_bytes = 0;
    for (auto& packet :
         rtp_sender()->GeneratePadding(target_size_bytes, true)) {
      generated_bytes += packet->payload_size() + packet->padding_size();
      rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
    }
    return generated_bytes;
  }

  // The following are helpers for configuring the RTPSender. They must be
  // called before sending any packets.

  // Enable the retransmission stream with sizable packet storage.
  void EnableRtx() {
    // RTX needs to be able to read the source packets from the packet store.
    // Pick a number of packets to store big enough for any unit test.
    constexpr uint16_t kNumberOfPacketsToStore = 100;
    rtp_sender_context_->packet_history_.SetStorePacketsStatus(
        RtpPacketHistory::StorageMode::kStoreAndCull, kNumberOfPacketsToStore);
    rtp_sender()->SetRtxPayloadType(kRtxPayload, kPayload);
    rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  }

  // Enable sending of the MID header extension for both the primary SSRC and
  // the RTX SSRC.
  void EnableMidSending(const std::string& mid) {
    rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionMid, kMidExtensionId);
    rtp_sender()->SetMid(mid);
  }

  // Enable sending of the RSID header extension for the primary SSRC and the
  // RRSID header extension for the RTX SSRC.
  void EnableRidSending(const std::string& rid) {
    rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionRtpStreamId,
                                             kRidExtensionId);
    rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionRepairedRtpStreamId,
                                             kRepairedRidExtensionId);
    rtp_sender()->SetRid(rid);
  }
};

// TODO(pbos): Move tests over from WithoutPacer to RtpSenderTest as this is our
// default code path.
class RtpSenderTestWithoutPacer : public RtpSenderTest {
 public:
  void SetUp() override { SetUpRtpSender(false, false, false); }
};

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketSetCsrc) {
  // Configure rtp_sender with csrc.
  std::vector<uint32_t> csrcs;
  csrcs.push_back(0x23456789);
  rtp_sender()->SetCsrcs(csrcs);

  auto packet = rtp_sender()->AllocatePacket();

  ASSERT_TRUE(packet);
  EXPECT_EQ(rtp_sender()->SSRC(), packet->Ssrc());
  EXPECT_EQ(csrcs, packet->Csrcs());
}

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketReserveExtensions) {
  // Configure rtp_sender with extensions.
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  ASSERT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionAudioLevel, kAudioLevelExtensionId));
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  auto packet = rtp_sender()->AllocatePacket();

  ASSERT_TRUE(packet);
  // Preallocate BWE extensions RtpSender set itself.
  EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());
  // Do not allocate media specific extensions.
  EXPECT_FALSE(packet->HasExtension<AudioLevel>());
  EXPECT_FALSE(packet->HasExtension<VideoOrientation>());
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberAdvanceSequenceNumber) {
  auto packet = rtp_sender()->AllocatePacket();
  ASSERT_TRUE(packet);
  const uint16_t sequence_number = rtp_sender()->SequenceNumber();

  EXPECT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));

  EXPECT_EQ(sequence_number, packet->SequenceNumber());
  EXPECT_EQ(sequence_number + 1, rtp_sender()->SequenceNumber());
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberFailsOnNotSending) {
  auto packet = rtp_sender()->AllocatePacket();
  ASSERT_TRUE(packet);

  rtp_sender()->SetSendingMediaStatus(false);
  EXPECT_FALSE(rtp_sender()->AssignSequenceNumber(packet.get()));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberMayAllowPaddingOnVideo) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender()->AllocatePacket();
  ASSERT_TRUE(packet);

  ASSERT_TRUE(rtp_sender()->GeneratePadding(kPaddingSize, true).empty());
  packet->SetMarker(false);
  ASSERT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  // Packet without marker bit doesn't allow padding on video stream.
  ASSERT_TRUE(rtp_sender()->GeneratePadding(kPaddingSize, true).empty());

  packet->SetMarker(true);
  ASSERT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  // Packet with marker bit allows send padding.
  ASSERT_FALSE(rtp_sender()->GeneratePadding(kPaddingSize, true).empty());
}

TEST_P(RtpSenderTest, AssignSequenceNumberAllowsPaddingOnAudio) {
  MockTransport transport;
  RtpRtcp::Configuration config;
  config.audio = true;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport;
  config.paced_sender = &mock_paced_sender_;
  config.local_media_ssrc = kSsrc;
  config.event_log = &mock_rtc_event_log_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  rtp_sender()->SetTimestampOffset(0);

  std::unique_ptr<RtpPacketToSend> audio_packet =
      rtp_sender()->AllocatePacket();
  // Padding on audio stream allowed regardless of marker in the last packet.
  audio_packet->SetMarker(false);
  audio_packet->SetPayloadType(kPayload);
  rtp_sender()->AssignSequenceNumber(audio_packet.get());

  const size_t kPaddingSize = 59;
  EXPECT_CALL(transport, SendRtp(_, kPaddingSize + kRtpHeaderSize, _))
      .WillOnce(Return(true));
  EXPECT_EQ(kPaddingSize, GenerateAndSendPadding(kPaddingSize));

  // Requested padding size is too small, will send a larger one.
  const size_t kMinPaddingSize = 50;
  EXPECT_CALL(transport, SendRtp(_, kMinPaddingSize + kRtpHeaderSize, _))
      .WillOnce(Return(true));
  EXPECT_EQ(kMinPaddingSize, GenerateAndSendPadding(kMinPaddingSize - 5));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberSetPaddingTimestamps) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender()->AllocatePacket();
  ASSERT_TRUE(packet);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);

  ASSERT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  auto padding_packets = rtp_sender()->GeneratePadding(kPaddingSize, true);

  ASSERT_EQ(1u, padding_packets.size());
  // Verify padding packet timestamp.
  EXPECT_EQ(kTimestamp, padding_packets[0]->Timestamp());
}

TEST_P(RtpSenderTestWithoutPacer,
       TransportFeedbackObserverGetsCorrectByteCount) {
  constexpr int kRtpOverheadBytesPerPacket = 12 + 8;
  NiceMock<MockOverheadObserver> mock_overhead_observer;

  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.transport_feedback_callback = &feedback_observer_;
  config.event_log = &mock_rtc_event_log_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  config.overhead_observer = &mock_overhead_observer;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  const size_t expected_bytes =
      GetParam().with_overhead
          ? sizeof(kPayloadData) + kRtpOverheadBytesPerPacket
          : sizeof(kPayloadData);

  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender()->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender()->SequenceNumber()),
                  Field(&RtpPacketSendInfo::length, expected_bytes),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);
  EXPECT_CALL(mock_overhead_observer,
              OnOverheadChanged(kRtpOverheadBytesPerPacket))
      .Times(1);
  SendGenericPacket();
}

TEST_P(RtpSenderTestWithoutPacer, SendsPacketsWithTransportSequenceNumber) {
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.transport_feedback_callback = &feedback_observer_;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender()->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender()->SequenceNumber()),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);

  SendGenericPacket();

  const auto& packet = transport_.last_sent_packet();
  uint16_t transport_seq_no;
  ASSERT_TRUE(packet.GetExtension<TransportSequenceNumber>(&transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, PacketOptionsNoRetransmission) {
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.transport_feedback_callback = &feedback_observer_;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  SendGenericPacket();

  EXPECT_FALSE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer,
       SetsIncludedInFeedbackWhenTransportSequenceNumberExtensionIsRegistered) {
  SetUpRtpSender(false, false, false);
  rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                           kTransportSequenceNumberExtensionId);
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  SendGenericPacket();
  EXPECT_TRUE(transport_.last_options_.included_in_feedback);
}

TEST_P(
    RtpSenderTestWithoutPacer,
    SetsIncludedInAllocationWhenTransportSequenceNumberExtensionIsRegistered) {
  SetUpRtpSender(false, false, false);
  rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                           kTransportSequenceNumberExtensionId);
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  SendGenericPacket();
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer,
       SetsIncludedInAllocationWhenForcedAsPartOfAllocation) {
  SetUpRtpSender(false, false, false);
  rtp_egress()->ForceIncludeSendPacketsInAllocation(true);
  SendGenericPacket();
  EXPECT_FALSE(transport_.last_options_.included_in_feedback);
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, DoesnSetIncludedInAllocationByDefault) {
  SetUpRtpSender(false, false, false);
  SendGenericPacket();
  EXPECT_FALSE(transport_.last_options_.included_in_feedback);
  EXPECT_FALSE(transport_.last_options_.included_in_allocation);
}

TEST_P(RtpSenderTestWithoutPacer, OnSendSideDelayUpdated) {
  StrictMock<MockSendSideDelayObserver> send_side_delay_observer_;

  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.send_side_delay_observer = &send_side_delay_observer_;
  config.event_log = &mock_rtc_event_log_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);

  const uint8_t kPayloadType = 127;
  const absl::optional<VideoCodecType> kCodecType =
      VideoCodecType::kVideoCodecGeneric;

  const uint32_t kCaptureTimeMsToRtpTimestamp = 90;  // 90 kHz clock
  RTPVideoHeader video_header;

  // Send packet with 10 ms send-side delay. The average, max and total should
  // be 10 ms.
  EXPECT_CALL(send_side_delay_observer_,
              SendSideDelayUpdated(10, 10, 10, kSsrc))
      .Times(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(10);
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      kPayloadType, kCodecType, capture_time_ms * kCaptureTimeMsToRtpTimestamp,
      capture_time_ms, kPayloadData, nullptr, video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet with 20 ms delay. The average, max and total should be
  // 15, 20 and 30 ms respectively.
  EXPECT_CALL(send_side_delay_observer_,
              SendSideDelayUpdated(15, 20, 30, kSsrc))
      .Times(1);
  fake_clock_.AdvanceTimeMilliseconds(10);
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      kPayloadType, kCodecType, capture_time_ms * kCaptureTimeMsToRtpTimestamp,
      capture_time_ms, kPayloadData, nullptr, video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet at the same time, which replaces the last packet.
  // Since this packet has 0 ms delay, the average is now 5 ms and max is 10 ms.
  // The total counter stays the same though.
  // TODO(terelius): Is is not clear that this is the right behavior.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(5, 10, 30, kSsrc))
      .Times(1);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      kPayloadType, kCodecType, capture_time_ms * kCaptureTimeMsToRtpTimestamp,
      capture_time_ms, kPayloadData, nullptr, video_header,
      kDefaultExpectedRetransmissionTimeMs));

  // Send a packet 1 second later. The earlier packets should have timed
  // out, so both max and average should be the delay of this packet. The total
  // keeps increasing.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(1, 1, 31, kSsrc))
      .Times(1);
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      kPayloadType, kCodecType, capture_time_ms * kCaptureTimeMsToRtpTimestamp,
      capture_time_ms, kPayloadData, nullptr, video_header,
      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderTestWithoutPacer, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  SendGenericPacket();
}

TEST_P(RtpSenderTest, SendsPacketsWithTransportSequenceNumber) {
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.paced_sender = &mock_paced_sender_;
  config.local_media_ssrc = kSsrc;
  config.transport_feedback_callback = &feedback_observer_;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  rtp_sender()->SetSequenceNumber(kSeqNum);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(feedback_observer_,
              OnAddPacket(AllOf(
                  Field(&RtpPacketSendInfo::ssrc, rtp_sender()->SSRC()),
                  Field(&RtpPacketSendInfo::transport_sequence_number,
                        kTransportSequenceNumber),
                  Field(&RtpPacketSendInfo::rtp_sequence_number,
                        rtp_sender()->SequenceNumber()),
                  Field(&RtpPacketSendInfo::pacing_info, PacedPacketInfo()))))
      .Times(1);

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  auto packet = SendGenericPacket();
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  // Transport sequence number is set by PacketRouter, before SendPacket().
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  uint16_t transport_seq_no;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<TransportSequenceNumber>(
          &transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
}

TEST_P(RtpSenderTest, WritesPacerExitToTimingExtension) {
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender()->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, 0u, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(Contains(Pointee(Property(
                                      &RtpPacketToSend::Ssrc, kSsrc)))));
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, WritesNetwork2ToTimingExtensionWithPacer) {
  SetUpRtpSender(/*pacer=*/true, /*populate_network2=*/true, false);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender()->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const uint16_t kPacerExitMs = 1234u;
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, kPacerExitMs, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;

  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(Contains(Pointee(Property(
                                      &RtpPacketToSend::Ssrc, kSsrc)))));
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.network2_timestamp_delta_ms);
  EXPECT_EQ(kPacerExitMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, WritesNetwork2ToTimingExtensionWithoutPacer) {
  SetUpRtpSender(/*pacer=*/false, /*populate_network2=*/true, false);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  auto packet = rtp_sender()->AllocatePacket();
  packet->SetMarker(true);
  packet->set_capture_time_ms(fake_clock_.TimeInMilliseconds());
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, 0u, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  packet->set_allow_retransmission(true);
  EXPECT_TRUE(rtp_sender()->AssignSequenceNumber(packet.get()));
  packet->set_packet_type(RtpPacketMediaType::kVideo);

  const int kPropagateTimeMs = 10;
  fake_clock_.AdvanceTimeMilliseconds(kPropagateTimeMs);

  EXPECT_TRUE(rtp_sender()->SendToNetwork(std::move(packet)));

  EXPECT_EQ(1, transport_.packets_sent());
  absl::optional<VideoSendTiming> video_timing =
      transport_.last_sent_packet().GetExtension<VideoTimingExtension>();
  ASSERT_TRUE(video_timing);
  EXPECT_EQ(kPropagateTimeMs, video_timing->network2_timestamp_delta_ms);
}

TEST_P(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  EXPECT_EQ(0, transport_.packets_sent());
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  webrtc::RTPHeader rtp_header;
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_P(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  // Immediately process send bucket and send packet.
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  EXPECT_EQ(1, transport_.packets_sent());

  // Retransmit packet.
  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->set_retransmitted_sequence_number(kSeqNum);
  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  EXPECT_EQ(static_cast<int>(packet_size), rtp_sender()->ReSendPacket(kSeqNum));
  EXPECT_EQ(1, transport_.packets_sent());
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(2, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  webrtc::RTPHeader rtp_header;
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

// This test sends 1 regular video packet, then 4 padding packets, and then
// 1 more regular packet.
TEST_P(RtpSenderTest, SendPadding) {
  // Make all (non-padding) packets go to send queue.
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(1 + 4 + 1);

  uint16_t seq_num = kSeqNum;
  uint32_t timestamp = kTimestamp;
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  size_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  EXPECT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  webrtc::RTPHeader rtp_header;

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
  const uint32_t media_packet_timestamp = timestamp;
  size_t packet_size = packet->size();
  int total_packets_sent = 0;
  const int kStoredTimeInMs = 100;

  // Packet should be stored in a send bucket.
  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  EXPECT_EQ(total_packets_sent, transport_.packets_sent());
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  ++seq_num;

  // Packet should now be sent. This test doesn't verify the regular video
  // packet, since it is tested in another test.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
  timestamp += 90 * kStoredTimeInMs;

  // Send padding 4 times, waiting 50 ms between each.
  for (int i = 0; i < 4; ++i) {
    const int kPaddingPeriodMs = 50;
    const size_t kPaddingBytes = 100;
    const size_t kMaxPaddingLength = 224;  // Value taken from rtp_sender.cc.
    // Padding will be forced to full packets.
    EXPECT_EQ(kMaxPaddingLength, GenerateAndSendPadding(kPaddingBytes));

    // Process send bucket. Padding should now be sent.
    EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
    EXPECT_EQ(kMaxPaddingLength + rtp_header_len,
              transport_.last_sent_packet().size());

    transport_.last_sent_packet().GetHeader(&rtp_header);
    EXPECT_EQ(kMaxPaddingLength, rtp_header.paddingLength);

    // Verify sequence number and timestamp. The timestamp should be the same
    // as the last media packet.
    EXPECT_EQ(seq_num++, rtp_header.sequenceNumber);
    EXPECT_EQ(media_packet_timestamp, rtp_header.timestamp);
    // Verify transmission time offset.
    int offset = timestamp - media_packet_timestamp;
    EXPECT_EQ(offset, rtp_header.extension.transmissionTimeOffset);
    uint64_t expected_send_time =
        ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
    EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
    fake_clock_.AdvanceTimeMilliseconds(kPaddingPeriodMs);
    timestamp += 90 * kPaddingPeriodMs;
  }

  // Send a regular video packet again.
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  packet = BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
  packet_size = packet->size();

  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, seq_num))))));
  EXPECT_TRUE(
      rtp_sender()->SendToNetwork(std::make_unique<RtpPacketToSend>(*packet)));
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Process send bucket.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());
  transport_.last_sent_packet().GetHeader(&rtp_header);

  // Verify sequence number and timestamp.
  EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
  EXPECT_EQ(timestamp, rtp_header.timestamp);
  // Verify transmission time offset. This packet is sent without delay.
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_P(RtpSenderTest, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  auto packet = SendGenericPacket();
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  EXPECT_EQ(1, transport_.packets_sent());
}

TEST_P(RtpSenderTest, OnSendPacketNotUpdatedForRetransmits) {
  EXPECT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(Contains(AllOf(
          Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
          Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum))))));
  auto packet = SendGenericPacket();
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->SetExtension<TransportSequenceNumber>(kTransportSequenceNumber);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_TRUE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer, SendGenericVideo) {
  const uint8_t kPayloadType = 127;
  const VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ASSERT_TRUE(rtp_sender_video.SendVideo(kPayloadType, kCodecType, 1234, 4321,
                                         payload, nullptr, video_header,
                                         kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  uint8_t generic_header = sent_payload[0];
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));

  // Send delta frame
  payload[0] = 13;
  payload[1] = 42;
  payload[4] = 13;

  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  ASSERT_TRUE(rtp_sender_video.SendVideo(kPayloadType, kCodecType, 1234, 4321,
                                         payload, nullptr, video_header,
                                         kDefaultExpectedRetransmissionTimeMs));

  sent_payload = transport_.last_sent_packet().payload();
  generic_header = sent_payload[0];
  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));
}

TEST_P(RtpSenderTestWithoutPacer, SendRawVideo) {
  const uint8_t kPayloadType = 111;
  const uint8_t payload[] = {11, 22, 33, 44, 55};

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);

  // Send a frame.
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ASSERT_TRUE(rtp_sender_video.SendVideo(kPayloadType, absl::nullopt, 1234,
                                         4321, payload, nullptr, video_header,
                                         kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(payload));
}

TEST_P(RtpSenderTest, SendFlexfecPackets) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  constexpr int kFlexfecPayloadType = 118;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexFecSsrc, kSsrc, kNoMid,
                               kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.paced_sender = &mock_paced_sender_;
  config.local_media_ssrc = kSsrc;
  config.fec_generator = &flexfec_sender_;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  rtp_sender()->SetSequenceNumber(kSeqNum);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.fec_generator = &flexfec_sender;
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  uint16_t flexfec_seq_num;
  RTPVideoHeader video_header;

    std::unique_ptr<RtpPacketToSend> media_packet;
    std::unique_ptr<RtpPacketToSend> fec_packet;

    EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
        .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
          for (auto& packet : packets) {
            if (packet->packet_type() == RtpPacketMediaType::kVideo) {
              EXPECT_EQ(packet->Ssrc(), kSsrc);
              EXPECT_EQ(packet->SequenceNumber(), kSeqNum);
              media_packet = std::move(packet);
            } else {
              EXPECT_EQ(packet->packet_type(),
                        RtpPacketMediaType::kForwardErrorCorrection);
              EXPECT_EQ(packet->Ssrc(), kFlexFecSsrc);
              fec_packet = std::move(packet);
            }
          }
        });

    video_header.frame_type = VideoFrameType::kVideoFrameKey;
    EXPECT_TRUE(rtp_sender_video.SendVideo(
        kMediaPayloadType, kCodecType, kTimestamp,
        fake_clock_.TimeInMilliseconds(), kPayloadData, nullptr, video_header,
        kDefaultExpectedRetransmissionTimeMs));
    ASSERT_TRUE(media_packet != nullptr);
    ASSERT_TRUE(fec_packet != nullptr);

    flexfec_seq_num = fec_packet->SequenceNumber();
    rtp_egress()->SendPacket(media_packet.get(), PacedPacketInfo());
    rtp_egress()->SendPacket(fec_packet.get(), PacedPacketInfo());

    ASSERT_EQ(2, transport_.packets_sent());
    const RtpPacketReceived& sent_media_packet = transport_.sent_packets_[0];
    EXPECT_EQ(kMediaPayloadType, sent_media_packet.PayloadType());
    EXPECT_EQ(kSeqNum, sent_media_packet.SequenceNumber());
    EXPECT_EQ(kSsrc, sent_media_packet.Ssrc());
    const RtpPacketReceived& sent_flexfec_packet = transport_.sent_packets_[1];
    EXPECT_EQ(kFlexfecPayloadType, sent_flexfec_packet.PayloadType());
    EXPECT_EQ(flexfec_seq_num, sent_flexfec_packet.SequenceNumber());
    EXPECT_EQ(kFlexFecSsrc, sent_flexfec_packet.Ssrc());
}

TEST_P(RtpSenderTestWithoutPacer, SendFlexfecPackets) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  constexpr int kFlexfecPayloadType = 118;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexFecSsrc, kSsrc, kNoMid,
                               kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.fec_generator = &flexfec_sender;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  rtp_sender()->SetSequenceNumber(kSeqNum);

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.fec_generator = &flexfec_sender;
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video.SendVideo(
      kMediaPayloadType, kCodecType, kTimestamp,
      fake_clock_.TimeInMilliseconds(), kPayloadData, nullptr, video_header,
      kDefaultExpectedRetransmissionTimeMs));

  ASSERT_EQ(2, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kSsrc, media_packet.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[1];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(kFlexFecSsrc, flexfec_packet.Ssrc());
}

// Test that the MID header extension is included on sent packets when
// configured.
TEST_P(RtpSenderTestWithoutPacer, MidIncludedOnSentPackets) {
  const char kMid[] = "mid";

  EnableMidSending(kMid);

  // Send a couple packets.
  SendGenericPacket();
  SendGenericPacket();

  // Expect both packets to have the MID set.
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  for (const RtpPacketReceived& packet : transport_.sent_packets_) {
    std::string mid;
    ASSERT_TRUE(packet.GetExtension<RtpMid>(&mid));
    EXPECT_EQ(kMid, mid);
  }
}

TEST_P(RtpSenderTestWithoutPacer, RidIncludedOnSentPackets) {
  const char kRid[] = "f";

  EnableRidSending(kRid);

  SendGenericPacket();

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const RtpPacketReceived& packet = transport_.sent_packets_[0];
  std::string rid;
  ASSERT_TRUE(packet.GetExtension<RtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
}

TEST_P(RtpSenderTestWithoutPacer, RidIncludedOnRtxSentPackets) {
  const char kRid[] = "f";

  EnableRtx();
  EnableRidSending(kRid);

  SendGenericPacket();
  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const RtpPacketReceived& packet = transport_.sent_packets_[0];
  std::string rid;
  ASSERT_TRUE(packet.GetExtension<RtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
  rid = kNoRid;
  EXPECT_FALSE(packet.HasExtension<RepairedRtpStreamId>());

  uint16_t packet_id = packet.SequenceNumber();
  rtp_sender()->ReSendPacket(packet_id);
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  const RtpPacketReceived& rtx_packet = transport_.sent_packets_[1];
  ASSERT_TRUE(rtx_packet.GetExtension<RepairedRtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);
  EXPECT_FALSE(rtx_packet.HasExtension<RtpStreamId>());
}

TEST_P(RtpSenderTestWithoutPacer, MidAndRidNotIncludedOnSentPacketsAfterAck) {
  const char kMid[] = "mid";
  const char kRid[] = "f";

  EnableMidSending(kMid);
  EnableRidSending(kRid);

  // This first packet should include both MID and RID.
  auto first_built_packet = SendGenericPacket();

  rtp_sender()->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  // The second packet should include neither since an ack was received.
  SendGenericPacket();

  ASSERT_EQ(2u, transport_.sent_packets_.size());

  const RtpPacketReceived& first_packet = transport_.sent_packets_[0];
  std::string mid, rid;
  ASSERT_TRUE(first_packet.GetExtension<RtpMid>(&mid));
  EXPECT_EQ(kMid, mid);
  ASSERT_TRUE(first_packet.GetExtension<RtpStreamId>(&rid));
  EXPECT_EQ(kRid, rid);

  const RtpPacketReceived& second_packet = transport_.sent_packets_[1];
  EXPECT_FALSE(second_packet.HasExtension<RtpMid>());
  EXPECT_FALSE(second_packet.HasExtension<RtpStreamId>());
}

TEST_P(RtpSenderTestWithoutPacer,
       MidAndRidAlwaysIncludedOnSentPacketsWhenConfigured) {
  SetUpRtpSender(false, false, /*always_send_mid_and_rid=*/true);
  const char kMid[] = "mid";
  const char kRid[] = "f";
  EnableMidSending(kMid);
  EnableRidSending(kRid);

  // Send two media packets: one before and one after the ack.
  auto first_packet = SendGenericPacket();
  rtp_sender()->OnReceivedAckOnSsrc(first_packet->SequenceNumber());
  SendGenericPacket();

  // Due to the configuration, both sent packets should contain MID and RID.
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  for (const RtpPacketReceived& packet : transport_.sent_packets_) {
    EXPECT_EQ(packet.GetExtension<RtpMid>(), kMid);
    EXPECT_EQ(packet.GetExtension<RtpStreamId>(), kRid);
  }
}

// Test that the first RTX packet includes both MID and RRID even if the packet
// being retransmitted did not have MID or RID. The MID and RID are needed on
// the first packets for a given SSRC, and RTX packets are sent on a separate
// SSRC.
TEST_P(RtpSenderTestWithoutPacer, MidAndRidIncludedOnFirstRtxPacket) {
  const char kMid[] = "mid";
  const char kRid[] = "f";

  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending(kRid);

  // This first packet will include both MID and RID.
  auto first_built_packet = SendGenericPacket();
  rtp_sender()->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  // The second packet will include neither since an ack was received.
  auto second_built_packet = SendGenericPacket();

  // The first RTX packet should include MID and RRID.
  ASSERT_LT(0,
            rtp_sender()->ReSendPacket(second_built_packet->SequenceNumber()));

  ASSERT_EQ(3u, transport_.sent_packets_.size());

  const RtpPacketReceived& rtx_packet = transport_.sent_packets_[2];
  std::string mid, rrid;
  ASSERT_TRUE(rtx_packet.GetExtension<RtpMid>(&mid));
  EXPECT_EQ(kMid, mid);
  ASSERT_TRUE(rtx_packet.GetExtension<RepairedRtpStreamId>(&rrid));
  EXPECT_EQ(kRid, rrid);
}

// Test that the RTX packets sent after receving an ACK on the RTX SSRC does
// not include either MID or RRID even if the packet being retransmitted did
// had a MID or RID.
TEST_P(RtpSenderTestWithoutPacer, MidAndRidNotIncludedOnRtxPacketsAfterAck) {
  const char kMid[] = "mid";
  const char kRid[] = "f";

  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending(kRid);

  // This first packet will include both MID and RID.
  auto first_built_packet = SendGenericPacket();
  rtp_sender()->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  // The second packet will include neither since an ack was received.
  auto second_built_packet = SendGenericPacket();

  // The first RTX packet will include MID and RRID.
  ASSERT_LT(0,
            rtp_sender()->ReSendPacket(second_built_packet->SequenceNumber()));

  ASSERT_EQ(3u, transport_.sent_packets_.size());
  const RtpPacketReceived& first_rtx_packet = transport_.sent_packets_[2];

  rtp_sender()->OnReceivedAckOnRtxSsrc(first_rtx_packet.SequenceNumber());

  // The second and third RTX packets should not include MID nor RRID.
  ASSERT_LT(0,
            rtp_sender()->ReSendPacket(first_built_packet->SequenceNumber()));
  ASSERT_LT(0,
            rtp_sender()->ReSendPacket(second_built_packet->SequenceNumber()));

  ASSERT_EQ(5u, transport_.sent_packets_.size());

  const RtpPacketReceived& second_rtx_packet = transport_.sent_packets_[3];
  EXPECT_FALSE(second_rtx_packet.HasExtension<RtpMid>());
  EXPECT_FALSE(second_rtx_packet.HasExtension<RepairedRtpStreamId>());

  const RtpPacketReceived& third_rtx_packet = transport_.sent_packets_[4];
  EXPECT_FALSE(third_rtx_packet.HasExtension<RtpMid>());
  EXPECT_FALSE(third_rtx_packet.HasExtension<RepairedRtpStreamId>());
}

TEST_P(RtpSenderTestWithoutPacer,
       MidAndRidAlwaysIncludedOnRtxPacketsWhenConfigured) {
  SetUpRtpSender(false, false, /*always_send_mid_and_rid=*/true);
  const char kMid[] = "mid";
  const char kRid[] = "f";
  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending(kRid);

  // Send two media packets: one before and one after the ack.
  auto media_packet1 = SendGenericPacket();
  rtp_sender()->OnReceivedAckOnSsrc(media_packet1->SequenceNumber());
  auto media_packet2 = SendGenericPacket();

  // Send three RTX packets with different combinations of orders w.r.t. the
  // media and RTX acks.
  ASSERT_LT(0, rtp_sender()->ReSendPacket(media_packet2->SequenceNumber()));
  ASSERT_EQ(3u, transport_.sent_packets_.size());
  rtp_sender()->OnReceivedAckOnRtxSsrc(
      transport_.sent_packets_[2].SequenceNumber());
  ASSERT_LT(0, rtp_sender()->ReSendPacket(media_packet1->SequenceNumber()));
  ASSERT_LT(0, rtp_sender()->ReSendPacket(media_packet2->SequenceNumber()));

  // Due to the configuration, all sent packets should contain MID
  // and either RID (media) or RRID (RTX).
  ASSERT_EQ(5u, transport_.sent_packets_.size());
  for (const auto& packet : transport_.sent_packets_) {
    EXPECT_EQ(packet.GetExtension<RtpMid>(), kMid);
  }
  for (size_t i = 0; i < 2; ++i) {
    const RtpPacketReceived& packet = transport_.sent_packets_[i];
    EXPECT_EQ(packet.GetExtension<RtpStreamId>(), kRid);
  }
  for (size_t i = 2; i < transport_.sent_packets_.size(); ++i) {
    const RtpPacketReceived& packet = transport_.sent_packets_[i];
    EXPECT_EQ(packet.GetExtension<RepairedRtpStreamId>(), kRid);
  }
}

// Test that if the RtpState indicates an ACK has been received on that SSRC
// then neither the MID nor RID header extensions will be sent.
TEST_P(RtpSenderTestWithoutPacer,
       MidAndRidNotIncludedOnSentPacketsAfterRtpStateRestored) {
  const char kMid[] = "mid";
  const char kRid[] = "f";

  EnableMidSending(kMid);
  EnableRidSending(kRid);

  RtpState state = rtp_sender()->GetRtpState();
  EXPECT_FALSE(state.ssrc_has_acked);
  state.ssrc_has_acked = true;
  rtp_sender()->SetRtpState(state);

  SendGenericPacket();

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const RtpPacketReceived& packet = transport_.sent_packets_[0];
  EXPECT_FALSE(packet.HasExtension<RtpMid>());
  EXPECT_FALSE(packet.HasExtension<RtpStreamId>());
}

// Test that if the RTX RtpState indicates an ACK has been received on that
// RTX SSRC then neither the MID nor RRID header extensions will be sent on
// RTX packets.
TEST_P(RtpSenderTestWithoutPacer,
       MidAndRridNotIncludedOnRtxPacketsAfterRtpStateRestored) {
  const char kMid[] = "mid";
  const char kRid[] = "f";

  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending(kRid);

  RtpState rtx_state = rtp_sender()->GetRtxRtpState();
  EXPECT_FALSE(rtx_state.ssrc_has_acked);
  rtx_state.ssrc_has_acked = true;
  rtp_sender()->SetRtxRtpState(rtx_state);

  auto built_packet = SendGenericPacket();
  ASSERT_LT(0, rtp_sender()->ReSendPacket(built_packet->SequenceNumber()));

  ASSERT_EQ(2u, transport_.sent_packets_.size());
  const RtpPacketReceived& rtx_packet = transport_.sent_packets_[1];
  EXPECT_FALSE(rtx_packet.HasExtension<RtpMid>());
  EXPECT_FALSE(rtx_packet.HasExtension<RepairedRtpStreamId>());
}

TEST_P(RtpSenderTest, FecOverheadRate) {
  constexpr uint32_t kTimestamp = 1234;
  constexpr int kMediaPayloadType = 127;
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  constexpr int kFlexfecPayloadType = 118;
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexFecSsrc, kSsrc, kNoMid,
                               kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.paced_sender = &mock_paced_sender_;
  config.local_media_ssrc = kSsrc;
  config.fec_generator = &flexfec_sender;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  rtp_sender()->SetSequenceNumber(kSeqNum);

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.fec_generator = &flexfec_sender;
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);
  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_video.SetFecParameters(params, params);

  constexpr size_t kNumMediaPackets = 10;
  constexpr size_t kNumFecPackets = kNumMediaPackets;
  constexpr int64_t kTimeBetweenPacketsMs = 10;
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets).Times(kNumMediaPackets);
  for (size_t i = 0; i < kNumMediaPackets; ++i) {
    RTPVideoHeader video_header;

    video_header.frame_type = VideoFrameType::kVideoFrameKey;
    EXPECT_TRUE(rtp_sender_video.SendVideo(
        kMediaPayloadType, kCodecType, kTimestamp,
        fake_clock_.TimeInMilliseconds(), kPayloadData, nullptr, video_header,
        kDefaultExpectedRetransmissionTimeMs));

    fake_clock_.AdvanceTimeMilliseconds(kTimeBetweenPacketsMs);
  }
  constexpr size_t kRtpHeaderLength = 12;
  constexpr size_t kFlexfecHeaderLength = 20;
  constexpr size_t kGenericCodecHeaderLength = 1;
  constexpr size_t kPayloadLength = sizeof(kPayloadData);
  constexpr size_t kPacketLength = kRtpHeaderLength + kFlexfecHeaderLength +
                                   kGenericCodecHeaderLength + kPayloadLength;
  EXPECT_NEAR(kNumFecPackets * kPacketLength * 8 /
                  (kNumFecPackets * kTimeBetweenPacketsMs / 1000.0f),
              rtp_sender_video.FecOverheadRate(), 500);
}

TEST_P(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback()
        : BitrateStatisticsObserver(),
          num_calls_(0),
          ssrc_(0),
          total_bitrate_(0),
          retransmit_bitrate_(0) {}
    ~TestCallback() override = default;

    void Notify(uint32_t total_bitrate,
                uint32_t retransmit_bitrate,
                uint32_t ssrc) override {
      ++num_calls_;
      ssrc_ = ssrc;
      total_bitrate_ = total_bitrate;
      retransmit_bitrate_ = retransmit_bitrate;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    uint32_t total_bitrate_;
    uint32_t retransmit_bitrate_;
  } callback;

  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.send_bitrate_observer = &callback;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);
  const VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  const uint8_t kPayloadType = 127;

  // Simulate kNumPackets sent with kPacketInterval ms intervals, with the
  // number of packets selected so that we fill (but don't overflow) the one
  // second averaging window.
  const uint32_t kWindowSizeMs = 1000;
  const uint32_t kPacketInterval = 20;
  const uint32_t kNumPackets =
      (kWindowSizeMs - kPacketInterval) / kPacketInterval;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);
  uint32_t ssrc = rtp_sender()->SSRC();

  // Initial process call so we get a new time window.
  rtp_egress()->ProcessBitrateAndNotifyObservers();

  // Send a few frames.
  RTPVideoHeader video_header;
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    video_header.frame_type = VideoFrameType::kVideoFrameKey;
    ASSERT_TRUE(rtp_sender_video.SendVideo(
        kPayloadType, kCodecType, 1234, 4321, payload, nullptr, video_header,
        kDefaultExpectedRetransmissionTimeMs));
    fake_clock_.AdvanceTimeMilliseconds(kPacketInterval);
  }

  rtp_egress()->ProcessBitrateAndNotifyObservers();

  // We get one call for every stats updated, thus two calls since both the
  // stream stats and the retransmit stats are updated once.
  EXPECT_EQ(2u, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  const uint32_t kTotalPacketSize = kPacketOverhead + sizeof(payload);
  // Bitrate measured over delta between last and first timestamp, plus one.
  const uint32_t kExpectedWindowMs = kNumPackets * kPacketInterval + 1;
  const uint32_t kExpectedBitsAccumulated = kTotalPacketSize * kNumPackets * 8;
  const uint32_t kExpectedRateBps =
      (kExpectedBitsAccumulated * 1000 + (kExpectedWindowMs / 2)) /
      kExpectedWindowMs;
  EXPECT_EQ(kExpectedRateBps, callback.total_bitrate_);
}

TEST_P(RtpSenderTestWithoutPacer, StreamDataCountersCallbacks) {
  const uint8_t kPayloadType = 127;
  const VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  FieldTrialBasedConfig field_trials;
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  RTPSenderVideo rtp_sender_video(video_config);
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);
  uint32_t ssrc = rtp_sender()->SSRC();

  // Send a frame.
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ASSERT_TRUE(rtp_sender_video.SendVideo(kPayloadType, kCodecType, 1234, 4321,
                                         payload, nullptr, video_header,
                                         kDefaultExpectedRetransmissionTimeMs));
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = 6;
  expected.transmitted.header_bytes = 12;
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.retransmitted.payload_bytes = 0;
  expected.retransmitted.header_bytes = 0;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 0;
  expected.fec.packets = 0;
  rtp_stats_callback_.Matches(ssrc, expected);

  // Retransmit a frame.
  uint16_t seqno = rtp_sender()->SequenceNumber() - 1;
  rtp_sender()->ReSendPacket(seqno);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.retransmitted.payload_bytes = 6;
  expected.retransmitted.header_bytes = 12;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 1;
  rtp_stats_callback_.Matches(ssrc, expected);

  // Send padding.
  GenerateAndSendPadding(kMaxPaddingSize);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 36;
  expected.transmitted.padding_bytes = kMaxPaddingSize;
  expected.transmitted.packets = 3;
  rtp_stats_callback_.Matches(ssrc, expected);
}

TEST_P(RtpSenderTestWithoutPacer, StreamDataCountersCallbacksUlpfec) {
  const uint8_t kRedPayloadType = 96;
  const uint8_t kUlpfecPayloadType = 97;
  const uint8_t kPayloadType = 127;
  const VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  FieldTrialBasedConfig field_trials;
  UlpfecGenerator ulpfec_generator(kRedPayloadType, kUlpfecPayloadType,
                                   &fake_clock_);
  RTPSenderVideo::Config video_config;
  video_config.clock = &fake_clock_;
  video_config.rtp_sender = rtp_sender();
  video_config.field_trials = &field_trials;
  video_config.red_payload_type = kRedPayloadType;
  video_config.fec_generator = &ulpfec_generator;
  RTPSenderVideo rtp_sender_video(video_config);
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);
  uint32_t ssrc = rtp_sender()->SSRC();

  RTPVideoHeader video_header;
  StreamDataCounters expected;

  // Send ULPFEC.
  FecProtectionParams fec_params;
  fec_params.fec_mask_type = kFecMaskRandom;
  fec_params.fec_rate = 1;
  fec_params.max_fec_frames = 1;
  rtp_sender_video.SetFecParameters(fec_params, fec_params);
  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  ASSERT_TRUE(rtp_sender_video.SendVideo(kPayloadType, kCodecType, 1234, 4321,
                                         payload, nullptr, video_header,
                                         kDefaultExpectedRetransmissionTimeMs));
  expected.transmitted.payload_bytes = 28;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.fec.packets = 1;
  rtp_stats_callback_.Matches(ssrc, expected);
}

TEST_P(RtpSenderTestWithoutPacer, BytesReportedCorrectly) {
  // XXX const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender()->SetRtxPayloadType(kPayloadType - 1, kPayloadType);
  rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  SendGenericPacket();
  // Will send 2 full-size padding packets.
  GenerateAndSendPadding(1);
  GenerateAndSendPadding(1);

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_egress()->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload
  EXPECT_GT(rtp_stats.first_packet_time_ms, -1);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, sizeof(kPayloadData));
  EXPECT_EQ(rtp_stats.transmitted.header_bytes, 12u);
  EXPECT_EQ(rtp_stats.transmitted.padding_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.payload_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.header_bytes, 24u);
  EXPECT_EQ(rtx_stats.transmitted.padding_bytes, 2 * kMaxPaddingSize);

  EXPECT_EQ(rtp_stats.transmitted.TotalBytes(),
            rtp_stats.transmitted.payload_bytes +
                rtp_stats.transmitted.header_bytes +
                rtp_stats.transmitted.padding_bytes);
  EXPECT_EQ(rtx_stats.transmitted.TotalBytes(),
            rtx_stats.transmitted.payload_bytes +
                rtx_stats.transmitted.header_bytes +
                rtx_stats.transmitted.padding_bytes);

  EXPECT_EQ(
      transport_.total_bytes_sent_,
      rtp_stats.transmitted.TotalBytes() + rtx_stats.transmitted.TotalBytes());
}

TEST_P(RtpSenderTestWithoutPacer, RespectsNackBitrateLimit) {
  const int32_t kPacketSize = 1400;
  const int32_t kNumPackets = 30;

  retransmission_rate_limiter_.SetMaxRate(kPacketSize * kNumPackets * 8);

  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, kNumPackets);
  const uint16_t kStartSequenceNumber = rtp_sender()->SequenceNumber();
  std::vector<uint16_t> sequence_numbers;
  for (int32_t i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(kStartSequenceNumber + i);
    fake_clock_.AdvanceTimeMilliseconds(1);
    SendPacket(fake_clock_.TimeInMilliseconds(), kPacketSize);
  }
  EXPECT_EQ(kNumPackets, transport_.packets_sent());

  fake_clock_.AdvanceTimeMilliseconds(1000 - kNumPackets);

  // Resending should work - brings the bandwidth up to the limit.
  // NACK bitrate is capped to the same bitrate as the encoder, since the max
  // protection overhead is 50% (see MediaOptimization::SetTargetRates).
  rtp_sender()->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());

  // Must be at least 5ms in between retransmission attempts.
  fake_clock_.AdvanceTimeMilliseconds(5);

  // Resending should not work, bandwidth exceeded.
  rtp_sender()->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());
}

TEST_P(RtpSenderTest, OnOverheadChanged) {
  MockOverheadObserver mock_overhead_observer;
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  config.overhead_observer = &mock_overhead_observer;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  // RTP overhead is 12B.
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(12)).Times(1);
  SendGenericPacket();

  rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                           kTransmissionTimeOffsetExtensionId);

  // TransmissionTimeOffset extension has a size of 8B.
  // 12B + 8B = 20B
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(20)).Times(1);
  SendGenericPacket();
}

TEST_P(RtpSenderTest, DoesNotUpdateOverheadOnEqualSize) {
  MockOverheadObserver mock_overhead_observer;
  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.retransmission_rate_limiter = &retransmission_rate_limiter_;
  config.overhead_observer = &mock_overhead_observer;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);

  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(_)).Times(1);
  SendGenericPacket();
  SendGenericPacket();
}

TEST_P(RtpSenderTest, SendPacketMatchesVideo) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kVideo);

  // Verify sent with correct SSRC.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kSsrc);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);
}

TEST_P(RtpSenderTest, SendPacketMatchesAudio) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kAudio);

  // Verify sent with correct SSRC.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kSsrc);
  packet->set_packet_type(RtpPacketMediaType::kAudio);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);
}

TEST_P(RtpSenderTest, SendPacketMatchesRetransmissions) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);

  // Verify sent with correct SSRC (non-RTX).
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kSsrc);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);

  // RTX retransmission.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kRtxSsrc);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 2);
}

TEST_P(RtpSenderTest, SendPacketMatchesPadding) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kPadding);

  // Verify sent with correct SSRC (non-RTX).
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kSsrc);
  packet->set_packet_type(RtpPacketMediaType::kPadding);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);

  // RTX padding.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kRtxSsrc);
  packet->set_packet_type(RtpPacketMediaType::kPadding);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 2);
}

TEST_P(RtpSenderTest, SendPacketMatchesFlexfec) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);

  // Verify sent with correct SSRC.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kFlexFecSsrc);
  packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);
}

TEST_P(RtpSenderTest, SendPacketMatchesUlpfec) {
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);

  // Verify sent with correct SSRC.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetSsrc(kSsrc);
  packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_EQ(transport_.packets_sent(), 1);
}

TEST_P(RtpSenderTest, SendPacketHandlesRetransmissionHistory) {
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  // Build a media packet and send it.
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  const uint16_t media_sequence_number = packet->SequenceNumber();
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet->set_allow_retransmission(true);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Simulate retransmission request.
  fake_clock_.AdvanceTimeMilliseconds(30);
  EXPECT_GT(rtp_sender()->ReSendPacket(media_sequence_number), 0);

  // Packet already pending, retransmission not allowed.
  fake_clock_.AdvanceTimeMilliseconds(30);
  EXPECT_EQ(rtp_sender()->ReSendPacket(media_sequence_number), 0);

  // Packet exiting pacer, mark as not longer pending.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  EXPECT_NE(packet->SequenceNumber(), media_sequence_number);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->SetSsrc(kRtxSsrc);
  packet->set_retransmitted_sequence_number(media_sequence_number);
  packet->set_allow_retransmission(false);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Retransmissions allowed again.
  fake_clock_.AdvanceTimeMilliseconds(30);
  EXPECT_GT(rtp_sender()->ReSendPacket(media_sequence_number), 0);

  // Retransmission of RTX packet should not be allowed.
  EXPECT_EQ(rtp_sender()->ReSendPacket(packet->SequenceNumber()), 0);
}

TEST_P(RtpSenderTest, SendPacketUpdatesExtensions) {
  ASSERT_EQ(rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionTransmissionTimeOffset,
                kTransmissionTimeOffsetExtensionId),
            0);
  ASSERT_EQ(rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId),
            0);
  ASSERT_EQ(rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionVideoTiming,
                                                     kVideoTimingExtensionId),
            0);

  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_packetization_finish_time_ms(fake_clock_.TimeInMilliseconds());

  const int32_t kDiffMs = 10;
  fake_clock_.AdvanceTimeMilliseconds(kDiffMs);

  packet->set_packet_type(RtpPacketMediaType::kVideo);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  const RtpPacketReceived& received_packet = transport_.last_sent_packet();

  EXPECT_EQ(received_packet.GetExtension<TransmissionOffset>(), kDiffMs * 90);

  EXPECT_EQ(received_packet.GetExtension<AbsoluteSendTime>(),
            AbsoluteSendTime::MsTo24Bits(fake_clock_.TimeInMilliseconds()));

  VideoSendTiming timing;
  EXPECT_TRUE(received_packet.GetExtension<VideoTimingExtension>(&timing));
  EXPECT_EQ(timing.pacer_exit_delta_ms, kDiffMs);
}

TEST_P(RtpSenderTest, SendPacketSetsPacketOptions) {
  const uint16_t kPacketId = 42;
  ASSERT_EQ(rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionTransportSequenceNumber,
                kTransportSequenceNumberExtensionId),
            0);
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetExtension<TransportSequenceNumber>(kPacketId);

  packet->set_packet_type(RtpPacketMediaType::kVideo);
  EXPECT_CALL(send_packet_observer_, OnSendPacket);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  EXPECT_EQ(transport_.last_options_.packet_id, kPacketId);
  EXPECT_TRUE(transport_.last_options_.included_in_allocation);
  EXPECT_TRUE(transport_.last_options_.included_in_feedback);
  EXPECT_FALSE(transport_.last_options_.is_retransmit);

  // Send another packet as retransmission, verify options are populated.
  packet = BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->SetExtension<TransportSequenceNumber>(kPacketId + 1);
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
  EXPECT_TRUE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTest, SendPacketUpdatesStats) {
  const size_t kPayloadSize = 1000;

  StrictMock<MockSendSideDelayObserver> send_side_delay_observer;

  RtpRtcp::Configuration config;
  config.clock = &fake_clock_;
  config.outgoing_transport = &transport_;
  config.local_media_ssrc = kSsrc;
  config.rtx_send_ssrc = kRtxSsrc;
  config.fec_generator = &flexfec_sender_;
  config.send_side_delay_observer = &send_side_delay_observer;
  config.event_log = &mock_rtc_event_log_;
  config.send_packet_observer = &send_packet_observer_;
  rtp_sender_context_ = std::make_unique<RtpSenderContext>(config);
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  const int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();

  std::unique_ptr<RtpPacketToSend> video_packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  video_packet->set_packet_type(RtpPacketMediaType::kVideo);
  video_packet->SetPayloadSize(kPayloadSize);
  video_packet->SetExtension<TransportSequenceNumber>(1);

  std::unique_ptr<RtpPacketToSend> rtx_packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  rtx_packet->SetSsrc(kRtxSsrc);
  rtx_packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  rtx_packet->SetPayloadSize(kPayloadSize);
  rtx_packet->SetExtension<TransportSequenceNumber>(2);

  std::unique_ptr<RtpPacketToSend> fec_packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  fec_packet->SetSsrc(kFlexFecSsrc);
  fec_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
  fec_packet->SetPayloadSize(kPayloadSize);
  fec_packet->SetExtension<TransportSequenceNumber>(3);

  const int64_t kDiffMs = 25;
  fake_clock_.AdvanceTimeMilliseconds(kDiffMs);

  EXPECT_CALL(send_side_delay_observer,
              SendSideDelayUpdated(kDiffMs, kDiffMs, kDiffMs, kSsrc));
  EXPECT_CALL(
      send_side_delay_observer,
      SendSideDelayUpdated(kDiffMs, kDiffMs, 2 * kDiffMs, kFlexFecSsrc));

  EXPECT_CALL(send_packet_observer_, OnSendPacket(1, capture_time_ms, kSsrc));

  rtp_egress()->SendPacket(video_packet.get(), PacedPacketInfo());

  // Send packet observer not called for padding/retransmissions.
  EXPECT_CALL(send_packet_observer_, OnSendPacket(2, _, _)).Times(0);
  rtp_egress()->SendPacket(rtx_packet.get(), PacedPacketInfo());

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(3, capture_time_ms, kFlexFecSsrc));
  rtp_egress()->SendPacket(fec_packet.get(), PacedPacketInfo());

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_egress()->GetDataCounters(&rtp_stats, &rtx_stats);
  EXPECT_EQ(rtp_stats.transmitted.packets, 2u);
  EXPECT_EQ(rtp_stats.fec.packets, 1u);
  EXPECT_EQ(rtx_stats.retransmitted.packets, 1u);
}

TEST_P(RtpSenderTest, GeneratePaddingResendsOldPacketsWithRtx) {
  // Min requested size in order to use RTX payload.
  const size_t kMinPaddingSize = 50;

  rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender()->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);

  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  ASSERT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  const size_t kPayloadPacketSize = 1234;
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kPayloadPacketSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);

  // Send a dummy video packet so it ends up in the packet history.
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Generated padding has large enough budget that the video packet should be
  // retransmitted as padding.
  std::vector<std::unique_ptr<RtpPacketToSend>> generated_packets =
      rtp_sender()->GeneratePadding(kMinPaddingSize, true);
  ASSERT_EQ(generated_packets.size(), 1u);
  auto& padding_packet = generated_packets.front();
  EXPECT_EQ(padding_packet->packet_type(), RtpPacketMediaType::kPadding);
  EXPECT_EQ(padding_packet->Ssrc(), kRtxSsrc);
  EXPECT_EQ(padding_packet->payload_size(),
            kPayloadPacketSize + kRtxHeaderSize);
  EXPECT_TRUE(padding_packet->HasExtension<TransportSequenceNumber>());
  EXPECT_TRUE(padding_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(padding_packet->HasExtension<TransmissionOffset>());

  // Verify all header extensions are received.
  rtp_egress()->SendPacket(padding_packet.get(), PacedPacketInfo());
  webrtc::RTPHeader rtp_header;
  transport_.last_sent_packet().GetHeader(&rtp_header);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);

  // Not enough budged for payload padding, use plain padding instead.
  const size_t kPaddingBytesRequested = kMinPaddingSize - 1;

  size_t padding_bytes_generated = 0;
  generated_packets =
      rtp_sender()->GeneratePadding(kPaddingBytesRequested, true);
  EXPECT_EQ(generated_packets.size(), 1u);
  for (auto& packet : generated_packets) {
    EXPECT_EQ(packet->packet_type(), RtpPacketMediaType::kPadding);
    EXPECT_EQ(packet->Ssrc(), kRtxSsrc);
    EXPECT_EQ(packet->payload_size(), 0u);
    EXPECT_GT(packet->padding_size(), 0u);
    padding_bytes_generated += packet->padding_size();

    EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());
    EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
    EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());

    // Verify all header extensions are received.
    rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
    webrtc::RTPHeader rtp_header;
    transport_.last_sent_packet().GetHeader(&rtp_header);
    EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
    EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
    EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  }

  EXPECT_EQ(padding_bytes_generated, kMaxPaddingSize);
}

TEST_P(RtpSenderTest, GeneratePaddingCreatesPurePaddingWithoutRtx) {
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  ASSERT_EQ(0,
            rtp_sender()->RegisterRtpHeaderExtension(
                kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  ASSERT_EQ(0, rtp_sender()->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  const size_t kPayloadPacketSize = 1234;
  // Send a dummy video packet so it ends up in the packet history. Since we
  // are not using RTX, it should never be used as padding.
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, fake_clock_.TimeInMilliseconds());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kPayloadPacketSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  EXPECT_CALL(send_packet_observer_, OnSendPacket).Times(1);
  rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());

  // Payload padding not available without RTX, only generate plain padding on
  // the media SSRC.
  // Number of padding packets is the requested padding size divided by max
  // padding packet size, rounded up. Pure padding packets are always of the
  // maximum size.
  const size_t kPaddingBytesRequested = kPayloadPacketSize + kRtxHeaderSize;
  const size_t kExpectedNumPaddingPackets =
      (kPaddingBytesRequested + kMaxPaddingSize - 1) / kMaxPaddingSize;
  size_t padding_bytes_generated = 0;
  std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
      rtp_sender()->GeneratePadding(kPaddingBytesRequested, true);
  EXPECT_EQ(padding_packets.size(), kExpectedNumPaddingPackets);
  for (auto& packet : padding_packets) {
    EXPECT_EQ(packet->packet_type(), RtpPacketMediaType::kPadding);
    EXPECT_EQ(packet->Ssrc(), kSsrc);
    EXPECT_EQ(packet->payload_size(), 0u);
    EXPECT_GT(packet->padding_size(), 0u);
    padding_bytes_generated += packet->padding_size();
    EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());
    EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
    EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());

    // Verify all header extensions are received.
    rtp_egress()->SendPacket(packet.get(), PacedPacketInfo());
    webrtc::RTPHeader rtp_header;
    transport_.last_sent_packet().GetHeader(&rtp_header);
    EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
    EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
    EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  }

  EXPECT_EQ(padding_bytes_generated,
            kExpectedNumPaddingPackets * kMaxPaddingSize);
}

TEST_P(RtpSenderTest, SupportsPadding) {
  bool kSendingMediaStats[] = {true, false};
  bool kEnableRedundantPayloads[] = {true, false};
  RTPExtensionType kBweExtensionTypes[] = {
      kRtpExtensionTransportSequenceNumber,
      kRtpExtensionTransportSequenceNumber02, kRtpExtensionAbsoluteSendTime,
      kRtpExtensionTransmissionTimeOffset};
  const int kExtensionsId = 7;

  for (bool sending_media : kSendingMediaStats) {
    rtp_sender()->SetSendingMediaStatus(sending_media);
    for (bool redundant_payloads : kEnableRedundantPayloads) {
      int rtx_mode = kRtxRetransmitted;
      if (redundant_payloads) {
        rtx_mode |= kRtxRedundantPayloads;
      }
      rtp_sender()->SetRtxStatus(rtx_mode);

      for (auto extension_type : kBweExtensionTypes) {
        EXPECT_FALSE(rtp_sender()->SupportsPadding());
        rtp_sender()->RegisterRtpHeaderExtension(extension_type, kExtensionsId);
        if (!sending_media) {
          EXPECT_FALSE(rtp_sender()->SupportsPadding());
        } else {
          EXPECT_TRUE(rtp_sender()->SupportsPadding());
          if (redundant_payloads) {
            EXPECT_TRUE(rtp_sender()->SupportsRtxPayloadPadding());
          } else {
            EXPECT_FALSE(rtp_sender()->SupportsRtxPayloadPadding());
          }
        }
        rtp_sender()->DeregisterRtpHeaderExtension(extension_type);
        EXPECT_FALSE(rtp_sender()->SupportsPadding());
      }
    }
  }
}

TEST_P(RtpSenderTest, SetsCaptureTimeAndPopulatesTransmissionOffset) {
  rtp_sender()->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                           kTransmissionTimeOffsetExtensionId);

  rtp_sender()->SetSendingMediaStatus(true);
  rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender()->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  const int64_t kMissingCaptureTimeMs = 0;
  const uint32_t kTimestampTicksPerMs = 90;
  const int64_t kOffsetMs = 10;

    auto packet =
        BuildRtpPacket(kPayload, kMarkerBit, fake_clock_.TimeInMilliseconds(),
                       kMissingCaptureTimeMs);
    packet->set_packet_type(RtpPacketMediaType::kVideo);
    packet->ReserveExtension<TransmissionOffset>();
    packet->AllocatePayload(sizeof(kPayloadData));

    std::unique_ptr<RtpPacketToSend> packet_to_pace;
    EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
        .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
          EXPECT_EQ(packets.size(), 1u);
          EXPECT_GT(packets[0]->capture_time_ms(), 0);
          packet_to_pace = std::move(packets[0]);
        });

    packet->set_allow_retransmission(true);
    EXPECT_TRUE(rtp_sender()->SendToNetwork(std::move(packet)));

    fake_clock_.AdvanceTimeMilliseconds(kOffsetMs);

    rtp_egress()->SendPacket(packet_to_pace.get(), PacedPacketInfo());

    EXPECT_EQ(1, transport_.packets_sent());
    absl::optional<int32_t> transmission_time_extension =
        transport_.sent_packets_.back().GetExtension<TransmissionOffset>();
    ASSERT_TRUE(transmission_time_extension.has_value());
    EXPECT_EQ(*transmission_time_extension, kOffsetMs * kTimestampTicksPerMs);

    // Retransmit packet. The RTX packet should get the same capture time as the
    // original packet, so offset is delta from original packet to now.
    fake_clock_.AdvanceTimeMilliseconds(kOffsetMs);

    std::unique_ptr<RtpPacketToSend> rtx_packet_to_pace;
    EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
        .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
          EXPECT_GT(packets[0]->capture_time_ms(), 0);
          rtx_packet_to_pace = std::move(packets[0]);
        });

    EXPECT_GT(rtp_sender()->ReSendPacket(kSeqNum), 0);
    rtp_egress()->SendPacket(rtx_packet_to_pace.get(), PacedPacketInfo());

    EXPECT_EQ(2, transport_.packets_sent());
    transmission_time_extension =
        transport_.sent_packets_.back().GetExtension<TransmissionOffset>();
    ASSERT_TRUE(transmission_time_extension.has_value());
    EXPECT_EQ(*transmission_time_extension,
              2 * kOffsetMs * kTimestampTicksPerMs);
}

TEST_P(RtpSenderTestWithoutPacer, ClearHistoryOnSequenceNumberCange) {
  const int64_t kRtt = 10;

  rtp_sender()->SetSendingMediaStatus(true);
  rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender()->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  rtp_sender_context_->packet_history_.SetRtt(kRtt);

  // Send a packet and record its sequence numbers.
  SendGenericPacket();
  ASSERT_EQ(1u, transport_.sent_packets_.size());
  const uint16_t packet_seqence_number =
      transport_.sent_packets_.back().SequenceNumber();

  // Advance time and make sure it can be retransmitted, even if we try to set
  // the ssrc the what it already is.
  rtp_sender()->SetSequenceNumber(rtp_sender()->SequenceNumber());
  fake_clock_.AdvanceTimeMilliseconds(kRtt);
  EXPECT_GT(rtp_sender()->ReSendPacket(packet_seqence_number), 0);

  // Change the sequence number, then move the time and try to retransmit again.
  // The old packet should now be gone.
  rtp_sender()->SetSequenceNumber(rtp_sender()->SequenceNumber() - 1);
  fake_clock_.AdvanceTimeMilliseconds(kRtt);
  EXPECT_EQ(rtp_sender()->ReSendPacket(packet_seqence_number), 0);
}

TEST_P(RtpSenderTest, IgnoresNackAfterDisablingMedia) {
  const int64_t kRtt = 10;

  rtp_sender()->SetSendingMediaStatus(true);
  rtp_sender()->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender()->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_context_->packet_history_.SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  rtp_sender_context_->packet_history_.SetRtt(kRtt);

  // Send a packet so it is in the packet history.
    std::unique_ptr<RtpPacketToSend> packet_to_pace;
    EXPECT_CALL(mock_paced_sender_, EnqueuePackets)
        .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
          packet_to_pace = std::move(packets[0]);
        });

    SendGenericPacket();
    rtp_egress()->SendPacket(packet_to_pace.get(), PacedPacketInfo());

    ASSERT_EQ(1u, transport_.sent_packets_.size());

    // Disable media sending and try to retransmit the packet, it should fail.
    rtp_sender()->SetSendingMediaStatus(false);
    fake_clock_.AdvanceTimeMilliseconds(kRtt);
    EXPECT_LT(rtp_sender()->ReSendPacket(kSeqNum), 0);
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderTest,
                         ::testing::Values(TestConfig{false},
                                           TestConfig{true}));

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderTestWithoutPacer,
                         ::testing::Values(TestConfig{false},
                                           TestConfig{true}));

}  // namespace webrtc
