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
#include <vector>

#include "absl/memory/memory.h"
#include "api/video/video_timing.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/buffer.h"
#include "rtc_base/rate_limiter.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {

namespace {
const int kTransmissionTimeOffsetExtensionId = 1;
const int kAbsoluteSendTimeExtensionId = 14;
const int kTransportSequenceNumberExtensionId = 13;
const int kVideoTimingExtensionId = 12;
const int kMidExtensionId = 11;
const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const int kMaxPacketLength = 1500;
const uint8_t kAudioLevel = 0x5a;
const uint16_t kTransportSequenceNumber = 0xaabbu;
const uint8_t kAudioLevelExtensionId = 9;
const int kAudioPayload = 103;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const int kVideoRotationExtensionId = 5;
const size_t kGenericHeaderLength = 1;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
const int64_t kDefaultExpectedRetransmissionTimeMs = 125;
const int kGenericDescriptorId = 10;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::SizeIs;

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return (((time_ms << 18) + 500) / 1000) & 0x00ffffff;
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() : total_bytes_sent_(0) {
    receivers_extensions_.Register(kRtpExtensionTransmissionTimeOffset,
                                   kTransmissionTimeOffsetExtensionId);
    receivers_extensions_.Register(kRtpExtensionAbsoluteSendTime,
                                   kAbsoluteSendTimeExtensionId);
    receivers_extensions_.Register(kRtpExtensionTransportSequenceNumber,
                                   kTransportSequenceNumberExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoRotation,
                                   kVideoRotationExtensionId);
    receivers_extensions_.Register(kRtpExtensionAudioLevel,
                                   kAudioLevelExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoTiming,
                                   kVideoTimingExtensionId);
    receivers_extensions_.Register(kRtpExtensionMid, kMidExtensionId);
    receivers_extensions_.Register(kRtpExtensionGenericFrameDescriptor,
                                   kGenericDescriptorId);
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

}  // namespace

class MockRtpPacketSender : public RtpPacketSender {
 public:
  MockRtpPacketSender() {}
  virtual ~MockRtpPacketSender() {}

  MOCK_METHOD6(InsertPacket,
               void(Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission));
};

class MockTransportSequenceNumberAllocator
    : public TransportSequenceNumberAllocator {
 public:
  MOCK_METHOD0(AllocateSequenceNumber, uint16_t());
};

class MockSendSideDelayObserver : public SendSideDelayObserver {
 public:
  MOCK_METHOD3(SendSideDelayUpdated, void(int, int, uint32_t));
};

class MockSendPacketObserver : public SendPacketObserver {
 public:
  MOCK_METHOD3(OnSendPacket, void(uint16_t, int64_t, uint32_t));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD4(AddPacket,
               void(uint32_t, uint16_t, size_t, const PacedPacketInfo&));
  MOCK_METHOD1(OnTransportFeedback, void(const rtcp::TransportFeedback&));
  MOCK_CONST_METHOD0(GetTransportFeedbackVector, std::vector<PacketFeedback>());
};

class MockOverheadObserver : public OverheadObserver {
 public:
  MOCK_METHOD1(OnOverheadChanged, void(size_t overhead_bytes_per_packet));
};

class RtpSenderTest : public ::testing::TestWithParam<bool> {
 protected:
  RtpSenderTest()
      : fake_clock_(kStartTime),
        mock_rtc_event_log_(),
        mock_paced_sender_(),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        rtp_sender_(),
        payload_(kPayload),
        transport_(),
        kMarkerBit(true),
        field_trials_(GetParam() ? "WebRTC-SendSideBwe-WithOverhead/Enabled/"
                                 : "") {}

  void SetUp() override { SetUpRtpSender(true, false); }

  void SetUpRtpSender(bool pacer, bool populate_network2) {
    rtp_sender_.reset(new RTPSender(
        false, &fake_clock_, &transport_, pacer ? &mock_paced_sender_ : nullptr,
        nullptr, &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
        &mock_rtc_event_log_, &send_packet_observer_,
        &retransmission_rate_limiter_, nullptr, populate_network2));
    rtp_sender_->SetSequenceNumber(kSeqNum);
    rtp_sender_->SetTimestampOffset(0);
    rtp_sender_->SetSSRC(kSsrc);
  }

  SimulatedClock fake_clock_;
  testing::NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  MockRtpPacketSender mock_paced_sender_;
  testing::StrictMock<MockTransportSequenceNumberAllocator> seq_num_allocator_;
  testing::StrictMock<MockSendPacketObserver> send_packet_observer_;
  testing::StrictMock<MockTransportFeedbackObserver> feedback_observer_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RTPSender> rtp_sender_;
  int payload_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  test::ScopedFieldTrials field_trials_;

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header) {
    VerifyRTPHeaderCommon(rtp_header, kMarkerBit, 0);
  }

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header, bool marker_bit) {
    VerifyRTPHeaderCommon(rtp_header, marker_bit, 0);
  }

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header,
                             bool marker_bit,
                             uint8_t number_of_csrcs) {
    EXPECT_EQ(marker_bit, rtp_header.markerBit);
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(number_of_csrcs, rtp_header.numCSRCs);
    EXPECT_EQ(0U, rtp_header.paddingLength);
  }

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(int payload_type,
                                                  bool marker_bit,
                                                  uint32_t timestamp,
                                                  int64_t capture_time_ms) {
    auto packet = rtp_sender_->AllocatePacket();
    packet->SetPayloadType(payload_type);
    packet->SetMarker(marker_bit);
    packet->SetTimestamp(timestamp);
    packet->set_capture_time_ms(capture_time_ms);
    EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
    return packet;
  }

  void SendPacket(int64_t capture_time_ms, int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    auto packet =
        BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
    packet->AllocatePayload(payload_length);

    // Packet should be stored in a send bucket.
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }

  void SendGenericPayload() {
    const uint32_t kTimestamp = 1234;
    const uint8_t kPayloadType = 127;
    const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
    char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
    EXPECT_EQ(0, rtp_sender_->RegisterPayload(payload_name, kPayloadType, 90000,
                                              0, 1500));

    RTPVideoHeader video_header;
    EXPECT_TRUE(rtp_sender_->SendOutgoingData(
        kVideoFrameKey, kPayloadType, kTimestamp, kCaptureTimeMs, kPayloadData,
        sizeof(kPayloadData), nullptr, &video_header, nullptr,
        kDefaultExpectedRetransmissionTimeMs));
  }
};

// TODO(pbos): Move tests over from WithoutPacer to RtpSenderTest as this is our
// default code path.
class RtpSenderTestWithoutPacer : public RtpSenderTest {
 public:
  void SetUp() override { SetUpRtpSender(false, false); }
};

class TestRtpSenderVideo : public RTPSenderVideo {
 public:
  TestRtpSenderVideo(Clock* clock,
                     RTPSender* rtp_sender,
                     FlexfecSender* flexfec_sender)
      : RTPSenderVideo(clock, rtp_sender, flexfec_sender) {}
  ~TestRtpSenderVideo() override {}

  StorageType GetStorageType(const RTPVideoHeader& header,
                             int32_t retransmission_settings,
                             int64_t expected_retransmission_time_ms) {
    return RTPSenderVideo::GetStorageType(GetTemporalId(header),
                                          retransmission_settings,
                                          expected_retransmission_time_ms);
  }
};

class RtpSenderVideoTest : public RtpSenderTest {
 protected:
  void SetUp() override {
    // TODO(pbos): Set up to use pacer.
    SetUpRtpSender(false, false);
    rtp_sender_video_.reset(
        new TestRtpSenderVideo(&fake_clock_, rtp_sender_.get(), nullptr));
  }
  std::unique_ptr<TestRtpSenderVideo> rtp_sender_video_;
};

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketSetCsrc) {
  // Configure rtp_sender with csrc.
  std::vector<uint32_t> csrcs;
  csrcs.push_back(0x23456789);
  rtp_sender_->SetCsrcs(csrcs);

  auto packet = rtp_sender_->AllocatePacket();

  ASSERT_TRUE(packet);
  EXPECT_EQ(rtp_sender_->SSRC(), packet->Ssrc());
  EXPECT_EQ(csrcs, packet->Csrcs());
}

TEST_P(RtpSenderTestWithoutPacer, AllocatePacketReserveExtensions) {
  // Configure rtp_sender with extensions.
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  ASSERT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  auto packet = rtp_sender_->AllocatePacket();

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
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  const uint16_t sequence_number = rtp_sender_->SequenceNumber();

  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));

  EXPECT_EQ(sequence_number, packet->SequenceNumber());
  EXPECT_EQ(sequence_number + 1, rtp_sender_->SequenceNumber());
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberFailsOnNotSending) {
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  rtp_sender_->SetSendingMediaStatus(false);
  EXPECT_FALSE(rtp_sender_->AssignSequenceNumber(packet.get()));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberMayAllowPaddingOnVideo) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);

  ASSERT_FALSE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));
  packet->SetMarker(false);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet without marker bit doesn't allow padding on video stream.
  EXPECT_FALSE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  packet->SetMarker(true);
  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  // Packet with marker bit allows send padding.
  EXPECT_TRUE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));
}

TEST_P(RtpSenderTest, AssignSequenceNumberAllowsPaddingOnAudio) {
  MockTransport transport;
  const bool kEnableAudio = true;
  rtp_sender_.reset(new RTPSender(
      kEnableAudio, &fake_clock_, &transport, &mock_paced_sender_, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      nullptr, &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetTimestampOffset(0);
  rtp_sender_->SetSSRC(kSsrc);

  std::unique_ptr<RtpPacketToSend> audio_packet = rtp_sender_->AllocatePacket();
  // Padding on audio stream allowed regardless of marker in the last packet.
  audio_packet->SetMarker(false);
  audio_packet->SetPayloadType(kPayload);
  rtp_sender_->AssignSequenceNumber(audio_packet.get());

  const size_t kPaddingSize = 59;
  EXPECT_CALL(transport, SendRtp(_, kPaddingSize + kRtpHeaderSize, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kPaddingSize,
            rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  // Requested padding size is too small, will send a larger one.
  const size_t kMinPaddingSize = 50;
  EXPECT_CALL(transport, SendRtp(_, kMinPaddingSize + kRtpHeaderSize, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kMinPaddingSize, rtp_sender_->TimeToSendPadding(kMinPaddingSize - 5,
                                                            PacedPacketInfo()));
}

TEST_P(RtpSenderTestWithoutPacer, AssignSequenceNumberSetPaddingTimestamps) {
  constexpr size_t kPaddingSize = 100;
  auto packet = rtp_sender_->AllocatePacket();
  ASSERT_TRUE(packet);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);

  ASSERT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  ASSERT_TRUE(rtp_sender_->TimeToSendPadding(kPaddingSize, PacedPacketInfo()));

  ASSERT_EQ(1u, transport_.sent_packets_.size());
  // Verify padding packet timestamp.
  EXPECT_EQ(kTimestamp, transport_.last_sent_packet().Timestamp());
}

TEST_P(RtpSenderTestWithoutPacer,
       TransportFeedbackObserverGetsCorrectByteCount) {
  constexpr int kRtpOverheadBytesPerPacket = 12 + 8;
  testing::NiceMock<MockOverheadObserver> mock_overhead_observer;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, &seq_num_allocator_,
      &feedback_observer_, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      nullptr, &retransmission_rate_limiter_, &mock_overhead_observer, false));
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));

  const size_t expected_bytes =
      GetParam() ? sizeof(kPayloadData) + kGenericHeaderLength +
                       kRtpOverheadBytesPerPacket
                 : sizeof(kPayloadData) + kGenericHeaderLength;

  EXPECT_CALL(feedback_observer_,
              AddPacket(rtp_sender_->SSRC(), kTransportSequenceNumber,
                        expected_bytes, PacedPacketInfo()))
      .Times(1);
  EXPECT_CALL(mock_overhead_observer,
              OnOverheadChanged(kRtpOverheadBytesPerPacket))
      .Times(1);
  SendGenericPayload();
}

TEST_P(RtpSenderTestWithoutPacer, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, &seq_num_allocator_,
      &feedback_observer_, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      &send_packet_observer_, &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  EXPECT_CALL(feedback_observer_,
              AddPacket(rtp_sender_->SSRC(), kTransportSequenceNumber, _,
                        PacedPacketInfo()))
      .Times(1);

  SendGenericPayload();

  const auto& packet = transport_.last_sent_packet();
  uint16_t transport_seq_no;
  ASSERT_TRUE(packet.GetExtension<TransportSequenceNumber>(&transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
}

TEST_P(RtpSenderTestWithoutPacer, PacketOptionsNoRetransmission) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, &seq_num_allocator_,
      &feedback_observer_, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
      &send_packet_observer_, &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kSsrc);

  SendGenericPayload();

  EXPECT_FALSE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer, NoAllocationIfNotRegistered) {
  SendGenericPayload();
}

TEST_P(RtpSenderTestWithoutPacer, OnSendSideDelayUpdated) {
  testing::StrictMock<MockSendSideDelayObserver> send_side_delay_observer_;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr, &send_side_delay_observer_,
                    &mock_rtc_event_log_, nullptr, nullptr, nullptr, false));
  rtp_sender_->SetSSRC(kSsrc);

  const uint8_t kPayloadType = 127;
  const uint32_t kCaptureTimeMsToRtpTimestamp = 90;  // 90 kHz clock
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  RTPVideoHeader video_header;
  EXPECT_EQ(0, rtp_sender_->RegisterPayload(payload_name, kPayloadType,
                                            1000 * kCaptureTimeMsToRtpTimestamp,
                                            0, 1500));

  // Send packet with 10 ms send-side delay. The average and max should be 10
  // ms.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(10, 10, kSsrc))
      .Times(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(10);
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet with 20 ms delay. The average
  // and max should be 15 and 20 ms respectively.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(15, 20, kSsrc))
      .Times(1);
  fake_clock_.AdvanceTimeMilliseconds(10);
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));

  // Send another packet at the same time, which replaces the last packet.
  // Since this packet has 0 ms delay, the average is now 5 ms and max is 10 ms.
  // TODO(terelius): Is is not clear that this is the right behavior.
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(5, 10, kSsrc))
      .Times(1);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));

  // Send a packet 1 second later. The earlier packets should have timed
  // out, so both max and average should be the delay of this packet.
  fake_clock_.AdvanceTimeMilliseconds(1000);
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(send_side_delay_observer_, SendSideDelayUpdated(1, 1, kSsrc))
      .Times(1);
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType,
      capture_time_ms * kCaptureTimeMsToRtpTimestamp, capture_time_ms,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderTestWithoutPacer, OnSendPacketUpdated) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);

  SendGenericPayload();
}

TEST_P(RtpSenderTest, SendsPacketsWithTransportSequenceNumber) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, nullptr,
      &seq_num_allocator_, &feedback_observer_, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(feedback_observer_,
              AddPacket(rtp_sender_->SSRC(), kTransportSequenceNumber, _,
                        PacedPacketInfo()))
      .Times(1);

  SendGenericPayload();
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), false,
                                PacedPacketInfo());

  const auto& packet = transport_.last_sent_packet();
  uint16_t transport_seq_no;
  EXPECT_TRUE(packet.GetExtension<TransportSequenceNumber>(&transport_seq_no));
  EXPECT_EQ(kTransportSequenceNumber, transport_seq_no);
  EXPECT_EQ(transport_.last_options_.packet_id, transport_seq_no);
}

TEST_P(RtpSenderTest, WritesPacerExitToTimingExtension) {
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender_->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, 0u, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  {
    EXPECT_CALL(
        mock_paced_sender_,
        InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _));
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, WritesNetwork2ToTimingExtension) {
  SetUpRtpSender(true, true);
  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet = rtp_sender_->AllocatePacket();
  packet->SetPayloadType(kPayload);
  packet->SetMarker(true);
  packet->SetTimestamp(kTimestamp);
  packet->set_capture_time_ms(capture_time_ms);
  const uint16_t kPacerExitMs = 1234u;
  const VideoSendTiming kVideoTiming = {0u, 0u, 0u, kPacerExitMs, 0u, 0u, true};
  packet->SetExtension<VideoTimingExtension>(kVideoTiming);
  EXPECT_TRUE(rtp_sender_->AssignSequenceNumber(packet.get()));
  size_t packet_size = packet->size();

  const int kStoredTimeInMs = 100;
  {
    EXPECT_CALL(
        mock_paced_sender_,
        InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _));
    EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                           kAllowRetransmission,
                                           RtpPacketSender::kNormalPriority));
  }
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_EQ(packet_size, transport_.last_sent_packet().size());

  VideoSendTiming video_timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &video_timing));
  EXPECT_EQ(kStoredTimeInMs, video_timing.network2_timestamp_delta_ms);
  EXPECT_EQ(kPacerExitMs, video_timing.pacer_exit_delta_ms);
}

TEST_P(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent());

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());

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
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent());

  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(static_cast<int>(packet_size), rtp_sender_->ReSendPacket(kSeqNum));
  EXPECT_EQ(0, transport_.packets_sent());

  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum, capture_time_ms, false,
                                PacedPacketInfo());

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

// This test sends 1 regular video packet, then 4 padding packets, and then
// 1 more regular packet.
TEST_P(RtpSenderTest, SendPadding) {
  // Make all (non-padding) packets go to send queue.
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, kSeqNum, _, _, _));
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(1 + 4 + 1);

  uint16_t seq_num = kSeqNum;
  uint32_t timestamp = kTimestamp;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  size_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  webrtc::RTPHeader rtp_header;

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  auto packet =
      BuildRtpPacket(kPayload, kMarkerBit, timestamp, capture_time_ms);
  const uint32_t media_packet_timestamp = timestamp;
  size_t packet_size = packet->size();

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  int total_packets_sent = 0;
  EXPECT_EQ(total_packets_sent, transport_.packets_sent());

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(kSsrc, seq_num++, capture_time_ms, false,
                                PacedPacketInfo());
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
    EXPECT_EQ(kMaxPaddingLength,
              rtp_sender_->TimeToSendPadding(kPaddingBytes, PacedPacketInfo()));

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

  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               kSsrc, seq_num, _, _, _));

  // Packet should be stored in a send bucket.
  EXPECT_TRUE(rtp_sender_->SendToNetwork(std::move(packet),
                                         kAllowRetransmission,
                                         RtpPacketSender::kNormalPriority));

  rtp_sender_->TimeToSendPacket(kSsrc, seq_num, capture_time_ms, false,
                                PacedPacketInfo());
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
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_,
              OnSendPacket(kTransportSequenceNumber, _, _))
      .Times(1);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
}

TEST_P(RtpSenderTest, OnSendPacketNotUpdatedForRetransmits) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(seq_num_allocator_, AllocateSequenceNumber())
      .WillOnce(testing::Return(kTransportSequenceNumber));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = true;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
  EXPECT_TRUE(transport_.last_options_.is_retransmit);
}

TEST_P(RtpSenderTest, OnSendPacketNotUpdatedWithoutSeqNumAllocator) {
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, nullptr,
      nullptr /* TransportSequenceNumberAllocator */, nullptr, nullptr, nullptr,
      nullptr, nullptr, &send_packet_observer_, &retransmission_rate_limiter_,
      nullptr, false));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  EXPECT_CALL(send_packet_observer_, OnSendPacket(_, _, _)).Times(0);
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, kSsrc, kSeqNum, _, _, _))
      .Times(1);

  SendGenericPayload();  // Packet passed to pacer.
  const bool kIsRetransmit = false;
  rtp_sender_->TimeToSendPacket(kSsrc, kSeqNum,
                                fake_clock_.TimeInMilliseconds(), kIsRetransmit,
                                PacedPacketInfo());
  EXPECT_EQ(1, transport_.packets_sent());
}

TEST_P(RtpSenderTest, SendRedundantPayloads) {
  MockTransport transport;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport, &mock_paced_sender_, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, &mock_rtc_event_log_, nullptr,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetSSRC(kSsrc);
  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);

  uint16_t seq_num = kSeqNum;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int32_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender_->SetRtxSsrc(1234);

  const size_t kNumPayloadSizes = 10;
  const size_t kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700,
                                                  750, 800, 850, 900, 950};
  // Expect all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kNormalPriority, kSsrc, _, _, _, _))
      .Times(kNumPayloadSizes);
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(kNumPayloadSizes);

  // Send 10 packets of increasing size.
  for (size_t i = 0; i < kNumPayloadSizes; ++i) {
    int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
    EXPECT_CALL(transport, SendRtp(_, _, _)).WillOnce(testing::Return(true));
    SendPacket(capture_time_ms, kPayloadSizes[i]);
    rtp_sender_->TimeToSendPacket(kSsrc, seq_num++, capture_time_ms, false,
                                  PacedPacketInfo());
    fake_clock_.AdvanceTimeMilliseconds(33);
  }

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(::testing::AtLeast(4));

  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(49, PacedPacketInfo()));

  PacketOptions options;
  EXPECT_CALL(transport,
              SendRtp(_, kPayloadSizes[0] + rtp_header_len + kRtxHeaderSize, _))
      .WillOnce(
          testing::DoAll(testing::SaveArg<2>(&options), testing::Return(true)));
  EXPECT_EQ(kPayloadSizes[0],
            rtp_sender_->TimeToSendPadding(500, PacedPacketInfo()));
  EXPECT_TRUE(options.is_retransmit);

  EXPECT_CALL(transport, SendRtp(_,
                                 kPayloadSizes[kNumPayloadSizes - 1] +
                                     rtp_header_len + kRtxHeaderSize,
                                 _))
      .WillOnce(testing::Return(true));

  options.is_retransmit = false;
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(
          testing::DoAll(testing::SaveArg<2>(&options), testing::Return(true)));
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(999, PacedPacketInfo()));
  EXPECT_FALSE(options.is_retransmit);
}

TEST_P(RtpSenderTestWithoutPacer, SendGenericVideo) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  uint8_t generic_header = sent_payload[0];
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));

  // Send delta frame
  payload[0] = 13;
  payload[1] = 42;
  payload[4] = 13;

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameDelta, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  sent_payload = transport_.last_sent_packet().payload();
  generic_header = sent_payload[0];
  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(payload));
}

TEST_P(RtpSenderTest, SendFlexfecPackets) {
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const char kNoMid[] = "";
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, &flexfec_sender,
      &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_->SetFecParameters(params, params);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc, kSeqNum,
                           _, _, false));
  uint16_t flexfec_seq_num;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .WillOnce(testing::SaveArg<2>(&flexfec_seq_num));
  SendGenericPayload();
  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kFlexfecSsrc, flexfec_seq_num,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(2, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kSeqNum, media_packet.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[1];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(flexfec_seq_num, flexfec_packet.SequenceNumber());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

// TODO(ilnik): because of webrtc:7859. Once FEC moved below pacer, this test
// should be removed.
TEST_P(RtpSenderTest, NoFlexfecForTimingFrames) {
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const char kNoMid[] = "";
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;

  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, &flexfec_sender,
      &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetStorePacketsStatus(true, 10);

  // Need extension to be registered for timing frames to be sent.
  ASSERT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_->SetFecParameters(params, params);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc, kSeqNum,
                           _, _, false));
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .Times(0);  // Not called because packet should not be protected.

  const uint32_t kTimestamp = 1234;
  const uint8_t kPayloadType = 127;
  const int64_t kCaptureTimeMs = fake_clock_.TimeInMilliseconds();
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  EXPECT_EQ(0, rtp_sender_->RegisterPayload(payload_name, kPayloadType, 90000,
                                            0, 1500));
  RTPVideoHeader video_header;
  video_header.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType, kTimestamp, kCaptureTimeMs, kPayloadData,
      sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(1);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(1, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kSeqNum, media_packet.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());

  // Now try to send not a timing frame.
  uint16_t flexfec_seq_num;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kLowPriority,
                                               kFlexfecSsrc, _, _, _, false))
      .WillOnce(testing::SaveArg<2>(&flexfec_seq_num));
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kLowPriority, kMediaSsrc,
                           kSeqNum + 1, _, _, false));
  video_header.video_timing.flags = VideoSendTiming::kInvalid;
  EXPECT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType, kTimestamp + 1, kCaptureTimeMs + 1,
      kPayloadData, sizeof(kPayloadData), nullptr, &video_header, nullptr,
      kDefaultExpectedRetransmissionTimeMs));

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kMediaSsrc, kSeqNum + 1,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  EXPECT_TRUE(rtp_sender_->TimeToSendPacket(kFlexfecSsrc, flexfec_seq_num,
                                            fake_clock_.TimeInMilliseconds(),
                                            false, PacedPacketInfo()));
  ASSERT_EQ(3, transport_.packets_sent());
  const RtpPacketReceived& media_packet2 = transport_.sent_packets_[1];
  EXPECT_EQ(kMediaPayloadType, media_packet2.PayloadType());
  EXPECT_EQ(kSeqNum + 1, media_packet2.SequenceNumber());
  EXPECT_EQ(kMediaSsrc, media_packet2.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[2];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(flexfec_seq_num, flexfec_packet.SequenceNumber());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

TEST_P(RtpSenderTestWithoutPacer, SendFlexfecPackets) {
  constexpr int kMediaPayloadType = 127;
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const char kNoMid[] = "";
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, &flexfec_sender,
                    &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
                    &mock_rtc_event_log_, &send_packet_observer_,
                    &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_->SetFecParameters(params, params);

  EXPECT_CALL(mock_rtc_event_log_,
              LogProxy(SameRtcEventTypeAs(RtcEvent::Type::RtpPacketOutgoing)))
      .Times(2);
  SendGenericPayload();
  ASSERT_EQ(2, transport_.packets_sent());
  const RtpPacketReceived& media_packet = transport_.sent_packets_[0];
  EXPECT_EQ(kMediaPayloadType, media_packet.PayloadType());
  EXPECT_EQ(kMediaSsrc, media_packet.Ssrc());
  const RtpPacketReceived& flexfec_packet = transport_.sent_packets_[1];
  EXPECT_EQ(kFlexfecPayloadType, flexfec_packet.PayloadType());
  EXPECT_EQ(kFlexfecSsrc, flexfec_packet.Ssrc());
}

// Test that the MID header extension is included on sent packets when
// configured.
TEST_P(RtpSenderTestWithoutPacer, MidIncludedOnSentPackets) {
  const char kMid[] = "mid";

  // Register MID header extension and set the MID for the RTPSender.
  rtp_sender_->SetSendingMediaStatus(false);
  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionMid, kMidExtensionId);
  rtp_sender_->SetMid(kMid);
  rtp_sender_->SetSendingMediaStatus(true);

  // Send a couple packets.
  SendGenericPayload();
  SendGenericPayload();

  // Expect both packets to have the MID set.
  ASSERT_EQ(2u, transport_.sent_packets_.size());
  for (const RtpPacketReceived& packet : transport_.sent_packets_) {
    std::string mid;
    ASSERT_TRUE(packet.GetExtension<RtpMid>(&mid));
    EXPECT_EQ(kMid, mid);
  }
}

TEST_P(RtpSenderTest, FecOverheadRate) {
  constexpr int kFlexfecPayloadType = 118;
  constexpr uint32_t kMediaSsrc = 1234;
  constexpr uint32_t kFlexfecSsrc = 5678;
  const char kNoMid[] = "";
  const std::vector<RtpExtension> kNoRtpExtensions;
  const std::vector<RtpExtensionSize> kNoRtpExtensionSizes;
  FlexfecSender flexfec_sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                               kNoMid, kNoRtpExtensions, kNoRtpExtensionSizes,
                               nullptr /* rtp_state */, &fake_clock_);

  // Reset |rtp_sender_| to use FlexFEC.
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, &flexfec_sender,
      &seq_num_allocator_, nullptr, nullptr, nullptr, nullptr,
      &mock_rtc_event_log_, &send_packet_observer_,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kMediaSsrc);
  rtp_sender_->SetSequenceNumber(kSeqNum);

  // Parameters selected to generate a single FEC packet per media packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  rtp_sender_->SetFecParameters(params, params);

  constexpr size_t kNumMediaPackets = 10;
  constexpr size_t kNumFecPackets = kNumMediaPackets;
  constexpr int64_t kTimeBetweenPacketsMs = 10;
  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, false))
      .Times(kNumMediaPackets + kNumFecPackets);
  for (size_t i = 0; i < kNumMediaPackets; ++i) {
    SendGenericPayload();
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
              rtp_sender_->FecOverheadRate(), 500);
}

TEST_P(RtpSenderTest, FrameCountCallbacks) {
  class TestCallback : public FrameCountObserver {
   public:
    TestCallback() : FrameCountObserver(), num_calls_(0), ssrc_(0) {}
    ~TestCallback() override = default;

    void FrameCountUpdated(const FrameCounts& frame_counts,
                           uint32_t ssrc) override {
      ++num_calls_;
      ssrc_ = ssrc;
      frame_counts_ = frame_counts;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    FrameCounts frame_counts_;
  } callback;

  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, &mock_paced_sender_, nullptr, nullptr,
      nullptr, nullptr, &callback, nullptr, nullptr, nullptr,
      &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kSsrc);
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _))
      .Times(::testing::AtLeast(2));

  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_EQ(1U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(0, callback.frame_counts_.delta_frames);

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameDelta, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_EQ(2U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(1, callback.frame_counts_.delta_frames);

  rtp_sender_.reset();
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
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport_, nullptr, nullptr, nullptr,
                    nullptr, &callback, nullptr, nullptr, nullptr, nullptr,
                    &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSSRC(kSsrc);

  // Simulate kNumPackets sent with kPacketInterval ms intervals, with the
  // number of packets selected so that we fill (but don't overflow) the one
  // second averaging window.
  const uint32_t kWindowSizeMs = 1000;
  const uint32_t kPacketInterval = 20;
  const uint32_t kNumPackets =
      (kWindowSizeMs - kPacketInterval) / kPacketInterval;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  // Initial process call so we get a new time window.
  rtp_sender_->ProcessBitrate();

  // Send a few frames.
  RTPVideoHeader video_header;
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    ASSERT_TRUE(rtp_sender_->SendOutgoingData(
        kVideoFrameKey, payload_type, 1234, 4321, payload, sizeof(payload),
        nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));
    fake_clock_.AdvanceTimeMilliseconds(kPacketInterval);
  }

  rtp_sender_->ProcessBitrate();

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

  rtp_sender_.reset();
}

class RtpSenderAudioTest : public RtpSenderTest {
 protected:
  RtpSenderAudioTest() {}

  void SetUp() override {
    payload_ = kAudioPayload;
    rtp_sender_.reset(
        new RTPSender(true, &fake_clock_, &transport_, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, &retransmission_rate_limiter_, nullptr, false));
    rtp_sender_->SetSSRC(kSsrc);
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }
};

TEST_P(RtpSenderTestWithoutPacer, StreamDataCountersCallbacks) {
  class TestCallback : public StreamDataCountersCallback {
   public:
    TestCallback() : StreamDataCountersCallback(), ssrc_(0), counters_() {}
    ~TestCallback() override = default;

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
  } callback;

  const uint8_t kRedPayloadType = 96;
  const uint8_t kUlpfecPayloadType = 97;
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  rtp_sender_->RegisterRtpStatisticsCallback(&callback);

  // Send a frame.
  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));
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
  callback.Matches(ssrc, expected);

  // Retransmit a frame.
  uint16_t seqno = rtp_sender_->SequenceNumber() - 1;
  rtp_sender_->ReSendPacket(seqno);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.retransmitted.payload_bytes = 6;
  expected.retransmitted.header_bytes = 12;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 1;
  callback.Matches(ssrc, expected);

  // Send padding.
  rtp_sender_->TimeToSendPadding(kMaxPaddingSize, PacedPacketInfo());
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 36;
  expected.transmitted.padding_bytes = kMaxPaddingSize;
  expected.transmitted.packets = 3;
  callback.Matches(ssrc, expected);

  // Send ULPFEC.
  rtp_sender_->SetUlpfecConfig(kRedPayloadType, kUlpfecPayloadType);
  FecProtectionParams fec_params;
  fec_params.fec_mask_type = kFecMaskRandom;
  fec_params.fec_rate = 1;
  fec_params.max_fec_frames = 1;
  rtp_sender_->SetFecParameters(fec_params, fec_params);
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameDelta, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));
  expected.transmitted.payload_bytes = 40;
  expected.transmitted.header_bytes = 60;
  expected.transmitted.packets = 5;
  expected.fec.packets = 1;
  callback.Matches(ssrc, expected);

  rtp_sender_->RegisterRtpStatisticsCallback(nullptr);
}

TEST_P(RtpSenderAudioTest, SendAudio) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kAudioFrameCN, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(payload));
}

TEST_P(RtpSenderAudioTest, SendAudioWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kAudioFrameCN, payload_type, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  auto sent_payload = transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(payload));
  // Verify AudioLevel extension.
  bool voice_activity;
  uint8_t audio_level;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<AudioLevel>(
      &voice_activity, &audio_level));
  EXPECT_EQ(kAudioLevel, audio_level);
  EXPECT_FALSE(voice_activity);
}

// As RFC4733, named telephone events are carried as part of the audio stream
// and must use the same sequence number and timestamp base as the regular
// audio channel.
// This test checks the marker bit for the first packet and the consequent
// packets of the same telephone event. Since it is specifically for DTMF
// events, ignoring audio packets and sending kEmptyFrame instead of those.
TEST_P(RtpSenderAudioTest, CheckMarkerBitForTelephoneEvents) {
  const char* kDtmfPayloadName = "telephone-event";
  const uint32_t kPayloadFrequency = 8000;
  const uint8_t kPayloadType = 126;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kDtmfPayloadName, kPayloadType,
                                            kPayloadFrequency, 0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  const char* kPayloadName = "payload_name";
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kPayloadName, kPayloadType,
                                            kPayloadFrequency, 1, 0));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  rtp_sender_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kEmptyFrame, kPayloadType, capture_time_ms, 0, nullptr, 0, nullptr,
      &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));
  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kEmptyFrame, kPayloadType, capture_time_ms + 2000, 0, nullptr, 0, nullptr,
      &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  // Marker Bit should be set to 1 for first packet.
  EXPECT_TRUE(transport_.last_sent_packet().Marker());

  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kEmptyFrame, kPayloadType, capture_time_ms + 4000, 0, nullptr, 0, nullptr,
      &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));
  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_FALSE(transport_.last_sent_packet().Marker());
}

TEST_P(RtpSenderTestWithoutPacer, BytesReportedCorrectly) {
  const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender_->SetSSRC(1234);
  rtp_sender_->SetRtxSsrc(4321);
  rtp_sender_->SetRtxPayloadType(kPayloadType - 1, kPayloadType);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kPayloadName, kPayloadType, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  RTPVideoHeader video_header;
  ASSERT_TRUE(rtp_sender_->SendOutgoingData(
      kVideoFrameKey, kPayloadType, 1234, 4321, payload, sizeof(payload),
      nullptr, &video_header, nullptr, kDefaultExpectedRetransmissionTimeMs));

  // Will send 2 full-size padding packets.
  rtp_sender_->TimeToSendPadding(1, PacedPacketInfo());
  rtp_sender_->TimeToSendPadding(1, PacedPacketInfo());

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload + 1-byte generic header.
  EXPECT_GT(rtp_stats.first_packet_time_ms, -1);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, sizeof(payload) + 1);
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

  rtp_sender_->SetStorePacketsStatus(true, kNumPackets);
  const uint16_t kStartSequenceNumber = rtp_sender_->SequenceNumber();
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
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());

  // Must be at least 5ms in between retransmission attempts.
  fake_clock_.AdvanceTimeMilliseconds(5);

  // Resending should not work, bandwidth exceeded.
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent());
}

TEST_P(RtpSenderVideoTest, KeyFrameHasCVO) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_0;
  rtp_sender_video_->SendVideo(kVideoCodecGeneric, kVideoFrameKey, kPayload,
                               kTimestamp, 0, kFrame, sizeof(kFrame), nullptr,
                               &hdr, kDefaultExpectedRetransmissionTimeMs);

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_P(RtpSenderVideoTest, TimingFrameHasPacketizationTimstampSet) {
  uint8_t kFrame[kMaxPacketLength];
  const int64_t kPacketizationTimeMs = 100;
  const int64_t kEncodeStartDeltaMs = 10;
  const int64_t kEncodeFinishDeltaMs = 50;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoTiming, kVideoTimingExtensionId));

  const int64_t kCaptureTimestamp = fake_clock_.TimeInMilliseconds();

  RTPVideoHeader hdr;
  hdr.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  hdr.video_timing.encode_start_delta_ms = kEncodeStartDeltaMs;
  hdr.video_timing.encode_finish_delta_ms = kEncodeFinishDeltaMs;

  fake_clock_.AdvanceTimeMilliseconds(kPacketizationTimeMs);
  rtp_sender_video_->SendVideo(kVideoCodecGeneric, kVideoFrameKey, kPayload,
                               kTimestamp, kCaptureTimestamp, kFrame,
                               sizeof(kFrame), nullptr, &hdr,
                               kDefaultExpectedRetransmissionTimeMs);
  VideoSendTiming timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &timing));
  EXPECT_EQ(kPacketizationTimeMs, timing.packetization_finish_delta_ms);
  EXPECT_EQ(kEncodeStartDeltaMs, timing.encode_start_delta_ms);
  EXPECT_EQ(kEncodeFinishDeltaMs, timing.encode_finish_delta_ms);
}

TEST_P(RtpSenderVideoTest, DeltaFrameHasCVOWhenChanged) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kVideoCodecGeneric, kVideoFrameKey, kPayload, kTimestamp, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  hdr.rotation = kVideoRotation_0;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kVideoCodecGeneric, kVideoFrameDelta, kPayload, kTimestamp + 1, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_P(RtpSenderVideoTest, DeltaFrameHasCVOWhenNonZero) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kVideoCodecGeneric, kVideoFrameKey, kPayload, kTimestamp, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kVideoCodecGeneric, kVideoFrameDelta, kPayload, kTimestamp + 1, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);
}

// Make sure rotation is parsed correctly when the Camera (C) and Flip (F) bits
// are set in the CVO byte.
TEST_P(RtpSenderVideoTest, SendVideoWithCameraAndFlipCVO) {
  // Test extracting rotation when Camera (C) and Flip (F) bits are zero.
  EXPECT_EQ(kVideoRotation_0, ConvertCVOByteToVideoRotation(0));
  EXPECT_EQ(kVideoRotation_90, ConvertCVOByteToVideoRotation(1));
  EXPECT_EQ(kVideoRotation_180, ConvertCVOByteToVideoRotation(2));
  EXPECT_EQ(kVideoRotation_270, ConvertCVOByteToVideoRotation(3));
  // Test extracting rotation when Camera (C) and Flip (F) bits are set.
  const int flip_bit = 1 << 2;
  const int camera_bit = 1 << 3;
  EXPECT_EQ(kVideoRotation_0,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 0));
  EXPECT_EQ(kVideoRotation_90,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 1));
  EXPECT_EQ(kVideoRotation_180,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 2));
  EXPECT_EQ(kVideoRotation_270,
            ConvertCVOByteToVideoRotation(flip_bit | camera_bit | 3));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesGeneric) {
  RTPVideoHeader header;
  header.codec = kVideoCodecGeneric;

  EXPECT_EQ(kDontRetransmit,
            rtp_sender_video_->GetStorageType(
                header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitBaseLayer,
                                      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitHigherLayers,
                                      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission,
            rtp_sender_video_->GetStorageType(
                header, kConditionallyRetransmitHigherLayers,
                kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitAllPackets,
                                      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesH264) {
  RTPVideoHeader header;
  header.video_type_header.emplace<RTPVideoHeaderH264>().packetization_mode =
      H264PacketizationMode::NonInterleaved;
  header.codec = kVideoCodecH264;

  EXPECT_EQ(kDontRetransmit,
            rtp_sender_video_->GetStorageType(
                header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitBaseLayer,
                                      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitHigherLayers,
                                      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission,
            rtp_sender_video_->GetStorageType(
                header, kConditionallyRetransmitHigherLayers,
                kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitAllPackets,
                                      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP8BaseLayer) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  EXPECT_EQ(kDontRetransmit,
            rtp_sender_video_->GetStorageType(
                header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitBaseLayer,
                                      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                 header, kRetransmitHigherLayers,
                                 kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission,
            rtp_sender_video_->GetStorageType(
                header, kRetransmitHigherLayers | kRetransmitBaseLayer,
                kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                 header, kConditionallyRetransmitHigherLayers,
                                 kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(
      kAllowRetransmission,
      rtp_sender_video_->GetStorageType(
          header, kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers,
          kDefaultExpectedRetransmissionTimeMs));
  EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                      header, kRetransmitAllPackets,
                                      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP8HigherLayers) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp8_header.temporalIdx = tid;

    EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                   header, kRetransmitOff,
                                   kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                   header, kRetransmitBaseLayer,
                                   kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                        header, kRetransmitHigherLayers,
                                        kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission,
              rtp_sender_video_->GetStorageType(
                  header, kRetransmitHigherLayers | kRetransmitBaseLayer,
                  kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                        header, kRetransmitAllPackets,
                                        kDefaultExpectedRetransmissionTimeMs));
  }
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP9) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP9;

  auto& vp9_header = header.video_type_header.emplace<RTPVideoHeaderVP9>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp9_header.temporal_idx = tid;

    EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                   header, kRetransmitOff,
                                   kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kDontRetransmit, rtp_sender_video_->GetStorageType(
                                   header, kRetransmitBaseLayer,
                                   kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                        header, kRetransmitHigherLayers,
                                        kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission,
              rtp_sender_video_->GetStorageType(
                  header, kRetransmitHigherLayers | kRetransmitBaseLayer,
                  kDefaultExpectedRetransmissionTimeMs));
    EXPECT_EQ(kAllowRetransmission, rtp_sender_video_->GetStorageType(
                                        header, kRetransmitAllPackets,
                                        kDefaultExpectedRetransmissionTimeMs));
  }
}

TEST_P(RtpSenderVideoTest, ConditionalRetransmit) {
  const int64_t kFrameIntervalMs = 33;
  const int64_t kRttMs = (kFrameIntervalMs * 3) / 2;
  const uint8_t kSettings =
      kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers;

  // Insert VP8 frames for all temporal layers, but stop before the final index.
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  // Fill averaging window to prevent rounding errors.
  constexpr int kNumRepetitions =
      (RTPSenderVideo::kTLRateWindowSizeMs + (kFrameIntervalMs / 2)) /
      kFrameIntervalMs;
  constexpr int kPattern[] = {0, 2, 1, 2};
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (size_t i = 0; i < arraysize(kPattern) * kNumRepetitions; ++i) {
    vp8_header.temporalIdx = kPattern[i % arraysize(kPattern)];
    rtp_sender_video_->GetStorageType(header, kSettings, kRttMs);
    fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  }

  // Since we're at the start of the pattern, the next expected frame in TL0 is
  // right now. We will wait at most one expected retransmission time before
  // acknowledging that it did not arrive, which means this frame and the next
  // will not be retransmitted.
  vp8_header.temporalIdx = 1;
  EXPECT_EQ(StorageType::kDontRetransmit,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  EXPECT_EQ(StorageType::kDontRetransmit,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // The TL0 frame did not arrive. So allow retransmission.
  EXPECT_EQ(StorageType::kAllowRetransmission,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Insert a frame for TL2. We just had frame in TL1, so the next one there is
  // in three frames away. TL0 is still too far in the past. So, allow
  // retransmission.
  vp8_header.temporalIdx = 2;
  EXPECT_EQ(StorageType::kAllowRetransmission,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Another TL2, next in TL1 is two frames away. Allow again.
  EXPECT_EQ(StorageType::kAllowRetransmission,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Yet another TL2, next in TL1 is now only one frame away, so don't store
  // for retransmission.
  EXPECT_EQ(StorageType::kDontRetransmit,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
}

TEST_P(RtpSenderVideoTest, ConditionalRetransmitLimit) {
  const int64_t kFrameIntervalMs = 200;
  const int64_t kRttMs = (kFrameIntervalMs * 3) / 2;
  const int32_t kSettings =
      kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers;

  // Insert VP8 frames for all temporal layers, but stop before the final index.
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  // Fill averaging window to prevent rounding errors.
  constexpr int kNumRepetitions =
      (RTPSenderVideo::kTLRateWindowSizeMs + (kFrameIntervalMs / 2)) /
      kFrameIntervalMs;
  constexpr int kPattern[] = {0, 2, 2, 2};
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (size_t i = 0; i < arraysize(kPattern) * kNumRepetitions; ++i) {
    vp8_header.temporalIdx = kPattern[i % arraysize(kPattern)];

    rtp_sender_video_->GetStorageType(header, kSettings, kRttMs);
    fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  }

  // Since we're at the start of the pattern, the next expected frame will be
  // right now in TL0. Put it in TL1 instead. Regular rules would dictate that
  // we don't store for retransmission because we expect a frame in a lower
  // layer, but that last frame in TL1 was a long time ago in absolute terms,
  // so allow retransmission anyway.
  vp8_header.temporalIdx = 1;
  EXPECT_EQ(StorageType::kAllowRetransmission,
            rtp_sender_video_->GetStorageType(header, kSettings, kRttMs));
}

TEST_P(RtpSenderVideoTest, PopulateGenericFrameDescriptor) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionGenericFrameDescriptor, kGenericDescriptorId));

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.higher_spatial_layers.push_back(4);
  generic.dependencies.push_back(kFrameId - 1);
  generic.dependencies.push_back(kFrameId - 500);
  rtp_sender_video_->SendVideo(kVideoCodecGeneric, kVideoFrameDelta, kPayload,
                               kTimestamp, 0, kFrame, sizeof(kFrame), nullptr,
                               &hdr, kDefaultExpectedRetransmissionTimeMs);

  RtpGenericFrameDescriptor descriptor_wire;
  EXPECT_EQ(1U, transport_.sent_packets_.size());
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpGenericFrameDescriptorExtension>(&descriptor_wire));
  EXPECT_EQ(static_cast<uint16_t>(generic.frame_id), descriptor_wire.FrameId());
  EXPECT_EQ(generic.temporal_index, descriptor_wire.TemporalLayer());
  EXPECT_THAT(descriptor_wire.FrameDependenciesDiffs(), ElementsAre(1, 500));
  uint8_t spatial_bitmask = 0x14;
  EXPECT_EQ(spatial_bitmask, descriptor_wire.SpatialLayersBitmask());
}

TEST_P(RtpSenderVideoTest,
       UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed) {
  const int64_t kFrameId = 100000;
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      RtpGenericFrameDescriptorExtension::kUri, kGenericDescriptorId));

  RTPVideoHeader hdr;
  hdr.codec = kVideoCodecVP8;
  RTPVideoHeaderVP8& vp8 = hdr.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8.pictureId = kFrameId % 0X7FFF;
  vp8.tl0PicIdx = 13;
  vp8.temporalIdx = 1;
  vp8.keyIdx = 2;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  rtp_sender_video_->SendVideo(kVideoCodecVP8, kVideoFrameDelta, kPayload,
                               kTimestamp, 0, kFrame, sizeof(kFrame), nullptr,
                               &hdr, kDefaultExpectedRetransmissionTimeMs);

  ASSERT_THAT(transport_.sent_packets_, SizeIs(1));
  // Expect only minimal 1-byte vp8 descriptor was generated.
  EXPECT_THAT(transport_.sent_packets_[0].payload_size(), 1 + kFrameSize);
}

TEST_P(RtpSenderTest, OnOverheadChanged) {
  MockOverheadObserver mock_overhead_observer;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      &retransmission_rate_limiter_, &mock_overhead_observer, false));
  rtp_sender_->SetSSRC(kSsrc);

  // RTP overhead is 12B.
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(12)).Times(1);
  SendGenericPayload();

  rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                          kTransmissionTimeOffsetExtensionId);

  // TransmissionTimeOffset extension has a size of 8B.
  // 12B + 8B = 20B
  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(20)).Times(1);
  SendGenericPayload();
}

TEST_P(RtpSenderTest, DoesNotUpdateOverheadOnEqualSize) {
  MockOverheadObserver mock_overhead_observer;
  rtp_sender_.reset(new RTPSender(
      false, &fake_clock_, &transport_, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr,
      &retransmission_rate_limiter_, &mock_overhead_observer, false));
  rtp_sender_->SetSSRC(kSsrc);

  EXPECT_CALL(mock_overhead_observer, OnOverheadChanged(_)).Times(1);
  SendGenericPayload();
  SendGenericPayload();
}

TEST_P(RtpSenderTest, SendsKeepAlive) {
  MockTransport transport;
  rtp_sender_.reset(
      new RTPSender(false, &fake_clock_, &transport, nullptr, nullptr, nullptr,
                    nullptr, nullptr, nullptr, nullptr, &mock_rtc_event_log_,
                    nullptr, &retransmission_rate_limiter_, nullptr, false));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetTimestampOffset(0);
  rtp_sender_->SetSSRC(kSsrc);

  const uint8_t kKeepalivePayloadType = 20;
  RTC_CHECK_NE(kKeepalivePayloadType, kPayload);

  EXPECT_CALL(transport, SendRtp(_, _, _))
      .WillOnce(
          Invoke([&kKeepalivePayloadType](const uint8_t* packet, size_t len,
                                          const PacketOptions& options) {
            webrtc::RTPHeader rtp_header;
            RtpUtility::RtpHeaderParser parser(packet, len);
            EXPECT_TRUE(parser.Parse(&rtp_header, nullptr));
            EXPECT_FALSE(rtp_header.markerBit);
            EXPECT_EQ(0U, rtp_header.paddingLength);
            EXPECT_EQ(kKeepalivePayloadType, rtp_header.payloadType);
            EXPECT_EQ(kSeqNum, rtp_header.sequenceNumber);
            EXPECT_EQ(kSsrc, rtp_header.ssrc);
            EXPECT_EQ(0u, len - rtp_header.headerLength);
            return true;
          }));

  rtp_sender_->SendKeepAlive(kKeepalivePayloadType);
  EXPECT_EQ(kSeqNum + 1, rtp_sender_->SequenceNumber());
}

INSTANTIATE_TEST_CASE_P(WithAndWithoutOverhead,
                        RtpSenderTest,
                        ::testing::Bool());
INSTANTIATE_TEST_CASE_P(WithAndWithoutOverhead,
                        RtpSenderTestWithoutPacer,
                        ::testing::Bool());
INSTANTIATE_TEST_CASE_P(WithAndWithoutOverhead,
                        RtpSenderVideoTest,
                        ::testing::Bool());
INSTANTIATE_TEST_CASE_P(WithAndWithoutOverhead,
                        RtpSenderAudioTest,
                        ::testing::Bool());
}  // namespace webrtc
