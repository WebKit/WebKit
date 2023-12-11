/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/frame_transformer_factory.h"
#include "api/rtp_headers.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/mock_frame_encryptor.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/thread.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::WithArgs;

enum : int {  // The first valid value is 1.
  kAbsoluteSendTimeExtensionId = 1,
  kGenericDescriptorId,
  kDependencyDescriptorId,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
  kAbsoluteCaptureTimeExtensionId,
  kPlayoutDelayExtensionId,
  kVideoLayersAllocationExtensionId,
};

constexpr int kPayload = 100;
constexpr VideoCodecType kType = VideoCodecType::kVideoCodecGeneric;
constexpr uint32_t kTimestamp = 10;
constexpr uint16_t kSeqNum = 33;
constexpr uint32_t kSsrc = 725242;
constexpr uint32_t kRtxSsrc = 912364;
constexpr int kMaxPacketLength = 1500;
constexpr Timestamp kStartTime = Timestamp::Millis(123456789);
constexpr TimeDelta kDefaultExpectedRetransmissionTime = TimeDelta::Millis(125);

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() {
    receivers_extensions_.Register<TransmissionOffset>(
        kTransmissionTimeOffsetExtensionId);
    receivers_extensions_.Register<AbsoluteSendTime>(
        kAbsoluteSendTimeExtensionId);
    receivers_extensions_.Register<TransportSequenceNumber>(
        kTransportSequenceNumberExtensionId);
    receivers_extensions_.Register<VideoOrientation>(kVideoRotationExtensionId);
    receivers_extensions_.Register<VideoTimingExtension>(
        kVideoTimingExtensionId);
    receivers_extensions_.Register<RtpGenericFrameDescriptorExtension00>(
        kGenericDescriptorId);
    receivers_extensions_.Register<RtpDependencyDescriptorExtension>(
        kDependencyDescriptorId);
    receivers_extensions_.Register<AbsoluteCaptureTimeExtension>(
        kAbsoluteCaptureTimeExtensionId);
    receivers_extensions_.Register<PlayoutDelayLimits>(
        kPlayoutDelayExtensionId);
    receivers_extensions_.Register<RtpVideoLayersAllocationExtension>(
        kVideoLayersAllocationExtensionId);
  }

  bool SendRtp(rtc::ArrayView<const uint8_t> data,
               const PacketOptions& options) override {
    sent_packets_.push_back(RtpPacketReceived(&receivers_extensions_));
    EXPECT_TRUE(sent_packets_.back().Parse(data));
    return true;
  }
  bool SendRtcp(rtc::ArrayView<const uint8_t> data) override { return false; }
  const RtpPacketReceived& last_sent_packet() { return sent_packets_.back(); }
  int packets_sent() { return sent_packets_.size(); }
  const std::vector<RtpPacketReceived>& sent_packets() const {
    return sent_packets_;
  }

 private:
  RtpHeaderExtensionMap receivers_extensions_;
  std::vector<RtpPacketReceived> sent_packets_;
};

class TestRtpSenderVideo : public RTPSenderVideo {
 public:
  TestRtpSenderVideo(Clock* clock,
                     RTPSender* rtp_sender,
                     const FieldTrialsView& field_trials)
      : RTPSenderVideo([&] {
          Config config;
          config.clock = clock;
          config.rtp_sender = rtp_sender;
          config.field_trials = &field_trials;
          return config;
        }()) {}
  ~TestRtpSenderVideo() override {}

  bool AllowRetransmission(const RTPVideoHeader& header,
                           int32_t retransmission_settings,
                           TimeDelta expected_retransmission_time) {
    return RTPSenderVideo::AllowRetransmission(GetTemporalId(header),
                                               retransmission_settings,
                                               expected_retransmission_time);
  }
};

class RtpSenderVideoTest : public ::testing::Test {
 public:
  RtpSenderVideoTest()
      : fake_clock_(kStartTime),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        rtp_module_(ModuleRtpRtcpImpl2::Create([&] {
          RtpRtcpInterface::Configuration config;
          config.clock = &fake_clock_;
          config.outgoing_transport = &transport_;
          config.retransmission_rate_limiter = &retransmission_rate_limiter_;
          config.field_trials = &field_trials_;
          config.local_media_ssrc = kSsrc;
          config.rtx_send_ssrc = kRtxSsrc;
          config.rid = "rid";
          return config;
        }())),
        rtp_sender_video_(
            std::make_unique<TestRtpSenderVideo>(&fake_clock_,
                                                 rtp_module_->RtpSender(),
                                                 field_trials_)) {
    rtp_module_->SetSequenceNumber(kSeqNum);
    rtp_module_->SetStartTimestamp(0);
  }

  void UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(
      int version);

 protected:
  rtc::AutoThread main_thread_;
  const RtpRtcpInterface::Configuration config_;
  test::ExplicitKeyValueConfig field_trials_{""};
  SimulatedClock fake_clock_;
  LoopbackTransportTest transport_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_module_;
  std::unique_ptr<TestRtpSenderVideo> rtp_sender_video_;
};

TEST_F(RtpSenderVideoTest, KeyFrameHasCVO) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::Uri(),
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_0;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_F(RtpSenderVideoTest, TimingFrameHasPacketizationTimstampSet) {
  uint8_t kFrame[kMaxPacketLength];
  const int64_t kPacketizationTimeMs = 100;
  const int64_t kEncodeStartDeltaMs = 10;
  const int64_t kEncodeFinishDeltaMs = 50;
  rtp_module_->RegisterRtpHeaderExtension(VideoTimingExtension::Uri(),
                                          kVideoTimingExtensionId);

  const Timestamp kCaptureTimestamp = fake_clock_.CurrentTime();

  RTPVideoHeader hdr;
  hdr.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  hdr.video_timing.encode_start_delta_ms = kEncodeStartDeltaMs;
  hdr.video_timing.encode_finish_delta_ms = kEncodeFinishDeltaMs;

  fake_clock_.AdvanceTimeMilliseconds(kPacketizationTimeMs);
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(kPayload, kType, kTimestamp, kCaptureTimestamp,
                               kFrame, sizeof(kFrame), hdr,
                               kDefaultExpectedRetransmissionTime, {});
  VideoSendTiming timing;
  EXPECT_TRUE(transport_.last_sent_packet().GetExtension<VideoTimingExtension>(
      &timing));
  EXPECT_EQ(kPacketizationTimeMs, timing.packetization_finish_delta_ms);
  EXPECT_EQ(kEncodeStartDeltaMs, timing.encode_start_delta_ms);
  EXPECT_EQ(kEncodeFinishDeltaMs, timing.encode_finish_delta_ms);
}

TEST_F(RtpSenderVideoTest, DeltaFrameHasCVOWhenChanged) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::Uri(),
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {}));

  hdr.rotation = kVideoRotation_0;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp + 1, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {}));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_F(RtpSenderVideoTest, DeltaFrameHasCVOWhenNonZero) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::Uri(),
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {}));

  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp + 1, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {}));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);
}

// Make sure rotation is parsed correctly when the Camera (C) and Flip (F) bits
// are set in the CVO byte.
TEST_F(RtpSenderVideoTest, SendVideoWithCameraAndFlipCVO) {
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

TEST_F(RtpSenderVideoTest, RetransmissionTypesGeneric) {
  RTPVideoHeader header;
  header.codec = kVideoCodecGeneric;

  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTime));
}

TEST_F(RtpSenderVideoTest, RetransmissionTypesH264) {
  RTPVideoHeader header;
  header.video_type_header.emplace<RTPVideoHeaderH264>().packetization_mode =
      H264PacketizationMode::NonInterleaved;
  header.codec = kVideoCodecH264;

  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTime));
}

TEST_F(RtpSenderVideoTest, RetransmissionTypesVP8BaseLayer) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTime));
  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitHigherLayers | kRetransmitBaseLayer,
      kDefaultExpectedRetransmissionTime));
  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
      header, kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTime));
}

TEST_F(RtpSenderVideoTest, RetransmissionTypesVP8HigherLayers) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp8_header.temporalIdx = tid;

    EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitOff, kDefaultExpectedRetransmissionTime));
    EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTime));
    EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTime));
    EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitHigherLayers | kRetransmitBaseLayer,
        kDefaultExpectedRetransmissionTime));
  }
}

TEST_F(RtpSenderVideoTest, RetransmissionTypesVP9) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP9;

  auto& vp9_header = header.video_type_header.emplace<RTPVideoHeaderVP9>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp9_header.temporal_idx = tid;

    EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitOff, kDefaultExpectedRetransmissionTime));
    EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTime));
    EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTime));
    EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(
        header, kRetransmitHigherLayers | kRetransmitBaseLayer,
        kDefaultExpectedRetransmissionTime));
  }
}

TEST_F(RtpSenderVideoTest, ConditionalRetransmit) {
  constexpr TimeDelta kFrameInterval = TimeDelta::Millis(33);
  constexpr TimeDelta kRtt = (kFrameInterval * 3) / 2;
  const uint8_t kSettings =
      kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers;

  // Insert VP8 frames for all temporal layers, but stop before the final index.
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  // Fill averaging window to prevent rounding errors.
  constexpr int kNumRepetitions =
      RTPSenderVideo::kTLRateWindowSize / kFrameInterval;
  constexpr int kPattern[] = {0, 2, 1, 2};
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (size_t i = 0; i < arraysize(kPattern) * kNumRepetitions; ++i) {
    vp8_header.temporalIdx = kPattern[i % arraysize(kPattern)];
    rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt);
    fake_clock_.AdvanceTime(kFrameInterval);
  }

  // Since we're at the start of the pattern, the next expected frame in TL0 is
  // right now. We will wait at most one expected retransmission time before
  // acknowledging that it did not arrive, which means this frame and the next
  // will not be retransmitted.
  vp8_header.temporalIdx = 1;
  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
  fake_clock_.AdvanceTime(kFrameInterval);
  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
  fake_clock_.AdvanceTime(kFrameInterval);

  // The TL0 frame did not arrive. So allow retransmission.
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
  fake_clock_.AdvanceTime(kFrameInterval);

  // Insert a frame for TL2. We just had frame in TL1, so the next one there is
  // in three frames away. TL0 is still too far in the past. So, allow
  // retransmission.
  vp8_header.temporalIdx = 2;
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
  fake_clock_.AdvanceTime(kFrameInterval);

  // Another TL2, next in TL1 is two frames away. Allow again.
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
  fake_clock_.AdvanceTime(kFrameInterval);

  // Yet another TL2, next in TL1 is now only one frame away, so don't store
  // for retransmission.
  EXPECT_FALSE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
}

TEST_F(RtpSenderVideoTest, ConditionalRetransmitLimit) {
  constexpr TimeDelta kFrameInterval = TimeDelta::Millis(200);
  constexpr TimeDelta kRtt = (kFrameInterval * 3) / 2;
  const int32_t kSettings =
      kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers;

  // Insert VP8 frames for all temporal layers, but stop before the final index.
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  // Fill averaging window to prevent rounding errors.
  constexpr int kNumRepetitions =
      RTPSenderVideo::kTLRateWindowSize / kFrameInterval;
  constexpr int kPattern[] = {0, 2, 2, 2};
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (size_t i = 0; i < arraysize(kPattern) * kNumRepetitions; ++i) {
    vp8_header.temporalIdx = kPattern[i % arraysize(kPattern)];

    rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt);
    fake_clock_.AdvanceTime(kFrameInterval);
  }

  // Since we're at the start of the pattern, the next expected frame will be
  // right now in TL0. Put it in TL1 instead. Regular rules would dictate that
  // we don't store for retransmission because we expect a frame in a lower
  // layer, but that last frame in TL1 was a long time ago in absolute terms,
  // so allow retransmission anyway.
  vp8_header.temporalIdx = 1;
  EXPECT_TRUE(rtp_sender_video_->AllowRetransmission(header, kSettings, kRtt));
}

TEST_F(RtpSenderVideoTest,
       ReservesEnoughSpaceForRtxPacketWhenMidAndRsidAreRegistered) {
  constexpr int kMediaPayloadId = 100;
  constexpr int kRtxPayloadId = 101;
  constexpr size_t kMaxPacketSize = 1'000;

  rtp_module_->SetMaxRtpPacketSize(kMaxPacketSize);
  rtp_module_->RegisterRtpHeaderExtension(RtpMid::Uri(), 1);
  rtp_module_->RegisterRtpHeaderExtension(RtpStreamId::Uri(), 2);
  rtp_module_->RegisterRtpHeaderExtension(RepairedRtpStreamId::Uri(), 3);
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteSendTime::Uri(), 4);
  rtp_module_->SetMid("long_mid");
  rtp_module_->SetRtxSendPayloadType(kRtxPayloadId, kMediaPayloadId);
  rtp_module_->SetStorePacketsStatus(/*enable=*/true, 10);
  rtp_module_->SetRtxSendStatus(kRtxRetransmitted);

  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;
  header.frame_type = VideoFrameType::kVideoFrameDelta;
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  uint8_t kPayload[kMaxPacketSize] = {};
  EXPECT_TRUE(rtp_sender_video_->SendVideo(
      kMediaPayloadId, /*codec_type=*/kVideoCodecVP8, /*rtp_timestamp=*/0,
      /*capture_time=*/Timestamp::Seconds(1), kPayload, sizeof(kPayload),
      header,
      /*expected_retransmission_time=*/TimeDelta::PlusInfinity(),
      /*csrcs=*/{}));
  ASSERT_THAT(transport_.sent_packets(), Not(IsEmpty()));
  // Ack media ssrc, but not rtx ssrc.
  rtcp::ReceiverReport rr;
  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kSsrc);
  rb.SetExtHighestSeqNum(transport_.last_sent_packet().SequenceNumber());
  rr.AddReportBlock(rb);
  rtp_module_->IncomingRtcpPacket(rr.Build());

  // Test for various frame size close to `kMaxPacketSize` to catch edge cases
  // when rtx packet barely fit.
  for (size_t frame_size = 800; frame_size < kMaxPacketSize; ++frame_size) {
    SCOPED_TRACE(frame_size);
    rtc::ArrayView<const uint8_t> payload(kPayload, frame_size);

    EXPECT_TRUE(rtp_sender_video_->SendVideo(
        kMediaPayloadId, /*codec_type=*/kVideoCodecVP8, /*rtp_timestamp=*/0,
        /*capture_time=*/Timestamp::Seconds(1), payload, frame_size, header,
        /*expected_retransmission_time=*/TimeDelta::Seconds(1), /*csrcs=*/{}));
    const RtpPacketReceived& media_packet = transport_.last_sent_packet();
    EXPECT_EQ(media_packet.Ssrc(), kSsrc);

    rtcp::Nack nack;
    nack.SetMediaSsrc(kSsrc);
    nack.SetPacketIds({media_packet.SequenceNumber()});
    rtp_module_->IncomingRtcpPacket(nack.Build());

    const RtpPacketReceived& rtx_packet = transport_.last_sent_packet();
    EXPECT_EQ(rtx_packet.Ssrc(), kRtxSsrc);
    EXPECT_LE(rtx_packet.size(), kMaxPacketSize);
  }
}

TEST_F(RtpSenderVideoTest, SendsDependencyDescriptorWhenVideoStructureIsSet) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 2;
  video_structure.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS"),
      FrameDependencyTemplate().S(1).T(0).Dtis("-S"),
      FrameDependencyTemplate().S(1).T(1).Dtis("-D"),
  };
  rtp_sender_video_->SetVideoStructure(&video_structure);

  // Send key frame.
  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 0;
  generic.spatial_index = 0;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 1);
  DependencyDescriptor descriptor_key;
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpDependencyDescriptorExtension>(
                      nullptr, &descriptor_key));
  ASSERT_TRUE(descriptor_key.attached_structure);
  EXPECT_EQ(descriptor_key.attached_structure->num_decode_targets, 2);
  EXPECT_THAT(descriptor_key.attached_structure->templates, SizeIs(3));
  EXPECT_EQ(descriptor_key.frame_number, kFrameId & 0xFFFF);
  EXPECT_EQ(descriptor_key.frame_dependencies.spatial_id, 0);
  EXPECT_EQ(descriptor_key.frame_dependencies.temporal_id, 0);
  EXPECT_EQ(descriptor_key.frame_dependencies.decode_target_indications,
            generic.decode_target_indications);
  EXPECT_THAT(descriptor_key.frame_dependencies.frame_diffs, IsEmpty());

  // Send delta frame.
  generic.frame_id = kFrameId + 1;
  generic.temporal_index = 1;
  generic.spatial_index = 1;
  generic.dependencies = {kFrameId, kFrameId - 500};
  generic.decode_target_indications = {DecodeTargetIndication::kNotPresent,
                                       DecodeTargetIndication::kRequired};
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  EXPECT_EQ(transport_.packets_sent(), 2);
  DependencyDescriptor descriptor_delta;
  ASSERT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpDependencyDescriptorExtension>(
              descriptor_key.attached_structure.get(), &descriptor_delta));
  EXPECT_EQ(descriptor_delta.attached_structure, nullptr);
  EXPECT_EQ(descriptor_delta.frame_number, (kFrameId + 1) & 0xFFFF);
  EXPECT_EQ(descriptor_delta.frame_dependencies.spatial_id, 1);
  EXPECT_EQ(descriptor_delta.frame_dependencies.temporal_id, 1);
  EXPECT_EQ(descriptor_delta.frame_dependencies.decode_target_indications,
            generic.decode_target_indications);
  EXPECT_THAT(descriptor_delta.frame_dependencies.frame_diffs,
              ElementsAre(1, 501));
}

TEST_F(RtpSenderVideoTest,
       SkipsDependencyDescriptorOnDeltaFrameWhenFailedToAttachToKeyFrame) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  rtp_module_->SetExtmapAllowMixed(false);
  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 2;
  // Use many templates so that key dependency descriptor would be too large
  // to fit into 16 bytes (max size of one byte header rtp header extension)
  video_structure.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS"),
      FrameDependencyTemplate().S(1).T(0).Dtis("-S"),
      FrameDependencyTemplate().S(1).T(1).Dtis("-D").FrameDiffs({1, 2, 3, 4}),
      FrameDependencyTemplate().S(1).T(1).Dtis("-D").FrameDiffs({2, 3, 4, 5}),
      FrameDependencyTemplate().S(1).T(1).Dtis("-D").FrameDiffs({3, 4, 5, 6}),
      FrameDependencyTemplate().S(1).T(1).Dtis("-D").FrameDiffs({4, 5, 6, 7}),
  };
  rtp_sender_video_->SetVideoStructure(&video_structure);

  // Send key frame.
  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 0;
  generic.spatial_index = 0;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 1);
  DependencyDescriptor descriptor_key;
  ASSERT_FALSE(transport_.last_sent_packet()
                   .HasExtension<RtpDependencyDescriptorExtension>());

  // Send delta frame.
  generic.frame_id = kFrameId + 1;
  generic.temporal_index = 1;
  generic.spatial_index = 1;
  generic.dependencies = {kFrameId, kFrameId - 500};
  generic.decode_target_indications = {DecodeTargetIndication::kNotPresent,
                                       DecodeTargetIndication::kRequired};
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  EXPECT_EQ(transport_.packets_sent(), 2);
  EXPECT_FALSE(transport_.last_sent_packet()
                   .HasExtension<RtpDependencyDescriptorExtension>());
}

TEST_F(RtpSenderVideoTest, PropagatesChainDiffsIntoDependencyDescriptor) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 2;
  video_structure.num_chains = 1;
  video_structure.decode_target_protected_by_chain = {0, 0};
  video_structure.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS").ChainDiffs({1}),
  };
  rtp_sender_video_->SetVideoStructure(&video_structure);

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  generic.chain_diffs = {2};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 1);
  DependencyDescriptor descriptor_key;
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpDependencyDescriptorExtension>(
                      nullptr, &descriptor_key));
  EXPECT_THAT(descriptor_key.frame_dependencies.chain_diffs,
              ContainerEq(generic.chain_diffs));
}

TEST_F(RtpSenderVideoTest,
       PropagatesActiveDecodeTargetsIntoDependencyDescriptor) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 2;
  video_structure.num_chains = 1;
  video_structure.decode_target_protected_by_chain = {0, 0};
  video_structure.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS").ChainDiffs({1}),
  };
  rtp_sender_video_->SetVideoStructure(&video_structure);

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  generic.active_decode_targets = 0b01;
  generic.chain_diffs = {1};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 1);
  DependencyDescriptor descriptor_key;
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpDependencyDescriptorExtension>(
                      nullptr, &descriptor_key));
  EXPECT_EQ(descriptor_key.active_decode_targets_bitmask, 0b01u);
}

TEST_F(RtpSenderVideoTest,
       SetDiffentVideoStructureAvoidsCollisionWithThePreviousStructure) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  FrameDependencyStructure video_structure1;
  video_structure1.num_decode_targets = 2;
  video_structure1.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS"),
      FrameDependencyTemplate().S(0).T(1).Dtis("D-"),
  };
  FrameDependencyStructure video_structure2;
  video_structure2.num_decode_targets = 2;
  video_structure2.templates = {
      FrameDependencyTemplate().S(0).T(0).Dtis("SS"),
      FrameDependencyTemplate().S(0).T(1).Dtis("R-"),
  };

  // Send 1st key frame.
  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SetVideoStructure(&video_structure1);
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  // Parse 1st extension.
  ASSERT_EQ(transport_.packets_sent(), 1);
  DependencyDescriptor descriptor_key1;
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpDependencyDescriptorExtension>(
                      nullptr, &descriptor_key1));
  ASSERT_TRUE(descriptor_key1.attached_structure);

  // Send the delta frame.
  generic.frame_id = kFrameId + 1;
  generic.temporal_index = 1;
  generic.decode_target_indications = {DecodeTargetIndication::kDiscardable,
                                       DecodeTargetIndication::kNotPresent};
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 2);
  RtpPacket delta_packet = transport_.last_sent_packet();

  // Send 2nd key frame.
  generic.frame_id = kFrameId + 2;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SetVideoStructure(&video_structure2);
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  // Parse the 2nd key frame.
  ASSERT_EQ(transport_.packets_sent(), 3);
  DependencyDescriptor descriptor_key2;
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpDependencyDescriptorExtension>(
                      nullptr, &descriptor_key2));
  ASSERT_TRUE(descriptor_key2.attached_structure);

  // Try to parse the 1st delta frame. It should parseble using the structure
  // from the 1st key frame, but not using the structure from the 2nd key frame.
  DependencyDescriptor descriptor_delta;
  EXPECT_TRUE(delta_packet.GetExtension<RtpDependencyDescriptorExtension>(
      descriptor_key1.attached_structure.get(), &descriptor_delta));
  EXPECT_FALSE(delta_packet.GetExtension<RtpDependencyDescriptorExtension>(
      descriptor_key2.attached_structure.get(), &descriptor_delta));
}

TEST_F(RtpSenderVideoTest,
       AuthenticateVideoHeaderWhenDependencyDescriptorExtensionIsUsed) {
  static constexpr size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize] = {1, 2, 3, 4};

  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::Uri(), kDependencyDescriptorId);
  auto encryptor = rtc::make_ref_counted<NiceMock<MockFrameEncryptor>>();
  ON_CALL(*encryptor, GetMaxCiphertextByteSize).WillByDefault(ReturnArg<1>());
  ON_CALL(*encryptor, Encrypt)
      .WillByDefault(WithArgs<3, 5>(
          [](rtc::ArrayView<const uint8_t> frame, size_t* bytes_written) {
            *bytes_written = frame.size();
            return 0;
          }));
  RTPSenderVideo::Config config;
  config.clock = &fake_clock_;
  config.rtp_sender = rtp_module_->RtpSender();
  config.field_trials = &field_trials_;
  config.frame_encryptor = encryptor.get();
  RTPSenderVideo rtp_sender_video(config);

  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 1;
  video_structure.templates = {FrameDependencyTemplate().Dtis("S")};
  rtp_sender_video.SetVideoStructure(&video_structure);

  // Send key frame.
  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  hdr.generic.emplace().decode_target_indications =
      video_structure.templates[0].decode_target_indications;

  EXPECT_CALL(*encryptor,
              Encrypt(_, _, Not(IsEmpty()), ElementsAreArray(kFrame), _, _));
  rtp_sender_video.SendVideo(kPayload, kType, kTimestamp,
                             fake_clock_.CurrentTime(), kFrame, sizeof(kFrame),
                             hdr, kDefaultExpectedRetransmissionTime, {});
  // Double check packet with the dependency descriptor is sent.
  ASSERT_EQ(transport_.packets_sent(), 1);
  EXPECT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpDependencyDescriptorExtension>());
}

TEST_F(RtpSenderVideoTest, PopulateGenericFrameDescriptor) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpGenericFrameDescriptorExtension00::Uri(), kGenericDescriptorId);

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.dependencies.push_back(kFrameId - 1);
  generic.dependencies.push_back(kFrameId - 500);
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  RtpGenericFrameDescriptor descriptor_wire;
  EXPECT_EQ(1, transport_.packets_sent());
  ASSERT_TRUE(transport_.last_sent_packet()
                  .GetExtension<RtpGenericFrameDescriptorExtension00>(
                      &descriptor_wire));
  EXPECT_EQ(static_cast<uint16_t>(generic.frame_id), descriptor_wire.FrameId());
  EXPECT_EQ(generic.temporal_index, descriptor_wire.TemporalLayer());
  EXPECT_THAT(descriptor_wire.FrameDependenciesDiffs(), ElementsAre(1, 500));
  EXPECT_EQ(descriptor_wire.SpatialLayersBitmask(), 0b0000'0100);
}

void RtpSenderVideoTest::
    UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(
        int version) {
  const int64_t kFrameId = 100000;
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];

  rtp_module_->RegisterRtpHeaderExtension(
      RtpGenericFrameDescriptorExtension00::Uri(), kGenericDescriptorId);

  RTPVideoHeader hdr;
  hdr.codec = kVideoCodecVP8;
  RTPVideoHeaderVP8& vp8 = hdr.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8.pictureId = kFrameId % 0X7FFF;
  vp8.tl0PicIdx = 13;
  vp8.temporalIdx = 1;
  vp8.keyIdx = 2;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(kPayload, VideoCodecType::kVideoCodecVP8,
                               kTimestamp, fake_clock_.CurrentTime(), kFrame,
                               sizeof(kFrame), hdr,
                               kDefaultExpectedRetransmissionTime, {});

  ASSERT_EQ(transport_.packets_sent(), 1);
  // Expect only minimal 1-byte vp8 descriptor was generated.
  EXPECT_EQ(transport_.last_sent_packet().payload_size(), 1 + kFrameSize);
}

TEST_F(RtpSenderVideoTest,
       UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed00) {
  UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(0);
}

TEST_F(RtpSenderVideoTest,
       UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed01) {
  UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(1);
}

TEST_F(RtpSenderVideoTest, VideoLayersAllocationWithResolutionSentOnKeyFrames) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.resolution_and_frame_rate_is_valid = true;
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  EXPECT_THAT(sent_allocation.active_spatial_layers, ElementsAre(layer));

  // Next key frame also have the allocation.
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
}

TEST_F(RtpSenderVideoTest,
       VideoLayersAllocationWithoutResolutionSentOnDeltaWhenUpdated) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  allocation.resolution_and_frame_rate_is_valid = true;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpVideoLayersAllocationExtension>());

  // No allocation sent on delta frame unless it has been updated.
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_FALSE(transport_.last_sent_packet()
                   .HasExtension<RtpVideoLayersAllocationExtension>());

  // Update the allocation.
  rtp_sender_video_->SetVideoLayersAllocation(allocation);
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  ASSERT_THAT(sent_allocation.active_spatial_layers, SizeIs(1));
  EXPECT_FALSE(sent_allocation.resolution_and_frame_rate_is_valid);
  EXPECT_THAT(sent_allocation.active_spatial_layers[0]
                  .target_bitrate_per_temporal_layer,
              SizeIs(1));
}

TEST_F(RtpSenderVideoTest,
       VideoLayersAllocationWithResolutionSentOnDeltaWhenSpatialLayerAdded) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  allocation.resolution_and_frame_rate_is_valid = true;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.spatial_id = 0;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  ASSERT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpVideoLayersAllocationExtension>());

  // Update the allocation.
  layer.width = 640;
  layer.height = 320;
  layer.spatial_id = 1;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(100));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  EXPECT_THAT(sent_allocation.active_spatial_layers, SizeIs(2));
  EXPECT_TRUE(sent_allocation.resolution_and_frame_rate_is_valid);
}

TEST_F(RtpSenderVideoTest,
       VideoLayersAllocationWithResolutionSentOnLargeFrameRateChange) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  allocation.resolution_and_frame_rate_is_valid = true;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.spatial_id = 0;
  layer.frame_rate_fps = 10;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  ASSERT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpVideoLayersAllocationExtension>());

  // Update frame rate only.
  allocation.active_spatial_layers[0].frame_rate_fps = 20;
  rtp_sender_video_->SetVideoLayersAllocation(allocation);
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  ASSERT_TRUE(sent_allocation.resolution_and_frame_rate_is_valid);
  EXPECT_EQ(sent_allocation.active_spatial_layers[0].frame_rate_fps, 20);
}

TEST_F(RtpSenderVideoTest,
       VideoLayersAllocationWithoutResolutionSentOnSmallFrameRateChange) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  allocation.resolution_and_frame_rate_is_valid = true;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.spatial_id = 0;
  layer.frame_rate_fps = 10;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  ASSERT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpVideoLayersAllocationExtension>());

  // Update frame rate slightly.
  allocation.active_spatial_layers[0].frame_rate_fps = 9;
  rtp_sender_video_->SetVideoLayersAllocation(allocation);
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  EXPECT_FALSE(sent_allocation.resolution_and_frame_rate_is_valid);
}

TEST_F(RtpSenderVideoTest, VideoLayersAllocationSentOnDeltaFramesOnlyOnUpdate) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  VideoLayersAllocation sent_allocation;
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
  EXPECT_THAT(sent_allocation.active_spatial_layers, SizeIs(1));

  // VideoLayersAllocation not sent on the next delta frame.
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_FALSE(transport_.last_sent_packet()
                   .HasExtension<RtpVideoLayersAllocationExtension>());

  // Update allocation. VideoLayesAllocation should be sent on the next frame.
  rtp_sender_video_->SetVideoLayersAllocation(allocation);
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_TRUE(
      transport_.last_sent_packet()
          .GetExtension<RtpVideoLayersAllocationExtension>(&sent_allocation));
}

TEST_F(RtpSenderVideoTest, VideoLayersAllocationNotSentOnHigherTemporalLayers) {
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpVideoLayersAllocationExtension::Uri(),
      kVideoLayersAllocationExtensionId);

  VideoLayersAllocation allocation;
  allocation.resolution_and_frame_rate_is_valid = true;
  VideoLayersAllocation::SpatialLayer layer;
  layer.width = 360;
  layer.height = 180;
  layer.target_bitrate_per_temporal_layer.push_back(
      DataRate::KilobitsPerSec(50));
  allocation.active_spatial_layers.push_back(layer);
  rtp_sender_video_->SetVideoLayersAllocation(allocation);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  hdr.codec = VideoCodecType::kVideoCodecVP8;
  auto& vp8_header = hdr.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 1;

  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_FALSE(transport_.last_sent_packet()
                   .HasExtension<RtpVideoLayersAllocationExtension>());

  // Send a delta frame on tl0.
  vp8_header.temporalIdx = 0;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpVideoLayersAllocationExtension>());
}

TEST_F(RtpSenderVideoTest,
       AbsoluteCaptureTimeNotForwardedWhenImageHasNoCaptureTime) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::Uri(),
                                          kAbsoluteCaptureTimeExtensionId);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(kPayload, kType, kTimestamp,
                               /*capture_time=*/Timestamp::MinusInfinity(),
                               kFrame, sizeof(kFrame), hdr,
                               kDefaultExpectedRetransmissionTime, {});
  // No absolute capture time should be set as the capture_time_ms was the
  // default value.
  for (const RtpPacketReceived& packet : transport_.sent_packets()) {
    EXPECT_FALSE(packet.HasExtension<AbsoluteCaptureTimeExtension>());
  }
}

TEST_F(RtpSenderVideoTest, AbsoluteCaptureTime) {
  rtp_sender_video_ = std::make_unique<TestRtpSenderVideo>(
      &fake_clock_, rtp_module_->RtpSender(), field_trials_);

  constexpr Timestamp kAbsoluteCaptureTimestamp = Timestamp::Millis(12345678);
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::Uri(),
                                          kAbsoluteCaptureTimeExtensionId);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, kAbsoluteCaptureTimestamp, kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});

  absl::optional<AbsoluteCaptureTime> absolute_capture_time;

  // It is expected that one and only one of the packets sent on this video
  // frame has absolute capture time header extension.
  for (const RtpPacketReceived& packet : transport_.sent_packets()) {
    if (absolute_capture_time.has_value()) {
      EXPECT_FALSE(packet.HasExtension<AbsoluteCaptureTimeExtension>());
    } else {
      absolute_capture_time =
          packet.GetExtension<AbsoluteCaptureTimeExtension>();
    }
  }

  // Verify the capture timestamp and that the clock offset is set to zero.
  ASSERT_TRUE(absolute_capture_time.has_value());
  EXPECT_EQ(absolute_capture_time->absolute_capture_timestamp,
            Int64MsToUQ32x32(
                fake_clock_.ConvertTimestampToNtpTime(kAbsoluteCaptureTimestamp)
                    .ToMs()));
  EXPECT_EQ(absolute_capture_time->estimated_capture_clock_offset, 0);
}

TEST_F(RtpSenderVideoTest, AbsoluteCaptureTimeWithExtensionProvided) {
  constexpr AbsoluteCaptureTime kAbsoluteCaptureTime = {
      123,
      absl::optional<int64_t>(456),
  };
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::Uri(),
                                          kAbsoluteCaptureTimeExtensionId);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  hdr.absolute_capture_time = kAbsoluteCaptureTime;
  rtp_sender_video_->SendVideo(kPayload, kType, kTimestamp,
                               /*capture_time=*/Timestamp::Millis(789), kFrame,
                               sizeof(kFrame), hdr,
                               kDefaultExpectedRetransmissionTime, {});

  absl::optional<AbsoluteCaptureTime> absolute_capture_time;

  // It is expected that one and only one of the packets sent on this video
  // frame has absolute capture time header extension.
  for (const RtpPacketReceived& packet : transport_.sent_packets()) {
    if (absolute_capture_time.has_value()) {
      EXPECT_FALSE(packet.HasExtension<AbsoluteCaptureTimeExtension>());
    } else {
      absolute_capture_time =
          packet.GetExtension<AbsoluteCaptureTimeExtension>();
    }
  }

  // Verify the extension.
  EXPECT_EQ(absolute_capture_time, kAbsoluteCaptureTime);
}

TEST_F(RtpSenderVideoTest, PopulatesPlayoutDelay) {
  // Single packet frames.
  constexpr size_t kPacketSize = 123;
  uint8_t kFrame[kPacketSize];
  rtp_module_->RegisterRtpHeaderExtension(PlayoutDelayLimits::Uri(),
                                          kPlayoutDelayExtensionId);
  const VideoPlayoutDelay kExpectedDelay(TimeDelta::Millis(10),
                                         TimeDelta::Millis(20));

  // Send initial key-frame without playout delay.
  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  hdr.codec = VideoCodecType::kVideoCodecVP8;
  auto& vp8_header = hdr.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_FALSE(
      transport_.last_sent_packet().HasExtension<PlayoutDelayLimits>());

  // Set playout delay on a discardable frame.
  hdr.playout_delay = kExpectedDelay;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  vp8_header.temporalIdx = 1;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  VideoPlayoutDelay received_delay = VideoPlayoutDelay();
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);

  // Set playout delay on a non-discardable frame, the extension should still
  // be populated since dilvery wasn't guaranteed on the last one.
  hdr.playout_delay = absl::nullopt;  // Indicates "no change".
  vp8_header.temporalIdx = 0;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);

  // The next frame does not need the extensions since it's delivery has
  // already been guaranteed.
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  EXPECT_FALSE(
      transport_.last_sent_packet().HasExtension<PlayoutDelayLimits>());

  // Insert key-frame, we need to refresh the state here.
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_->SendVideo(
      kPayload, kType, kTimestamp, fake_clock_.CurrentTime(), kFrame,
      sizeof(kFrame), hdr, kDefaultExpectedRetransmissionTime, {});
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);
}

TEST_F(RtpSenderVideoTest, SendGenericVideo) {
  const uint8_t kPayloadType = 127;
  const VideoCodecType kCodecType = VideoCodecType::kVideoCodecGeneric;
  const uint8_t kPayload[] = {47, 11, 32, 93, 89};

  // Send keyframe.
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ASSERT_TRUE(rtp_sender_video_->SendVideo(
      kPayloadType, kCodecType, 1234, fake_clock_.CurrentTime(), kPayload,
      sizeof(kPayload), video_header, TimeDelta::PlusInfinity(), {}));

  rtc::ArrayView<const uint8_t> sent_payload =
      transport_.last_sent_packet().payload();
  uint8_t generic_header = sent_payload[0];
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(kPayload));

  // Send delta frame.
  const uint8_t kDeltaPayload[] = {13, 42, 32, 93, 13};
  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  ASSERT_TRUE(rtp_sender_video_->SendVideo(
      kPayloadType, kCodecType, 1234, fake_clock_.CurrentTime(), kDeltaPayload,
      sizeof(kDeltaPayload), video_header, TimeDelta::PlusInfinity(), {}));

  sent_payload = sent_payload = transport_.last_sent_packet().payload();
  generic_header = sent_payload[0];
  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);
  EXPECT_THAT(sent_payload.subview(1), ElementsAreArray(kDeltaPayload));
}

TEST_F(RtpSenderVideoTest, SendRawVideo) {
  const uint8_t kPayloadType = 111;
  const uint8_t kPayload[] = {11, 22, 33, 44, 55};

  // Send a frame.
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ASSERT_TRUE(rtp_sender_video_->SendVideo(
      kPayloadType, absl::nullopt, 1234, fake_clock_.CurrentTime(), kPayload,
      sizeof(kPayload), video_header, TimeDelta::PlusInfinity(), {}));

  rtc::ArrayView<const uint8_t> sent_payload =
      transport_.last_sent_packet().payload();
  EXPECT_THAT(sent_payload, ElementsAreArray(kPayload));
}

class RtpSenderVideoWithFrameTransformerTest : public ::testing::Test {
 public:
  RtpSenderVideoWithFrameTransformerTest()
      : time_controller_(kStartTime),
        retransmission_rate_limiter_(time_controller_.GetClock(), 1000),
        rtp_module_(ModuleRtpRtcpImpl2::Create([&] {
          RtpRtcpInterface::Configuration config;
          config.clock = time_controller_.GetClock();
          config.outgoing_transport = &transport_;
          config.retransmission_rate_limiter = &retransmission_rate_limiter_;
          config.field_trials = &field_trials_;
          config.local_media_ssrc = kSsrc;
          return config;
        }())) {
    rtp_module_->SetSequenceNumber(kSeqNum);
    rtp_module_->SetStartTimestamp(0);
  }

  std::unique_ptr<RTPSenderVideo> CreateSenderWithFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> transformer) {
    RTPSenderVideo::Config config;
    config.clock = time_controller_.GetClock();
    config.rtp_sender = rtp_module_->RtpSender();
    config.field_trials = &field_trials_;
    config.frame_transformer = transformer;
    config.task_queue_factory = time_controller_.GetTaskQueueFactory();
    return std::make_unique<RTPSenderVideo>(config);
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  test::ExplicitKeyValueConfig field_trials_{""};
  LoopbackTransportTest transport_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_module_;
};

std::unique_ptr<EncodedImage> CreateDefaultEncodedImage() {
  const uint8_t data[] = {1, 2, 3, 4};
  auto encoded_image = std::make_unique<EncodedImage>();
  encoded_image->SetEncodedData(
      webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
  return encoded_image;
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       CreateSenderRegistersFrameTransformer) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  EXPECT_CALL(*mock_frame_transformer,
              RegisterTransformedFrameSinkCallback(_, kSsrc));
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       DestroySenderUnregistersFrameTransformer) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  EXPECT_CALL(*mock_frame_transformer,
              UnregisterTransformedFrameSinkCallback(kSsrc));
  rtp_sender_video = nullptr;
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       SendEncodedImageTransformsFrame) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;

  EXPECT_CALL(*mock_frame_transformer, Transform);
  rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                     *encoded_image, video_header,
                                     kDefaultExpectedRetransmissionTime);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(RtpSenderVideoWithFrameTransformerTest, ValidPayloadTypes) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;

  EXPECT_TRUE(rtp_sender_video->SendEncodedImage(
      0, kType, kTimestamp, *encoded_image, video_header,
      kDefaultExpectedRetransmissionTime));
  EXPECT_TRUE(rtp_sender_video->SendEncodedImage(
      127, kType, kTimestamp, *encoded_image, video_header,
      kDefaultExpectedRetransmissionTime));
  EXPECT_DEATH(rtp_sender_video->SendEncodedImage(
                   -1, kType, kTimestamp, *encoded_image, video_header,
                   kDefaultExpectedRetransmissionTime),
               "");
  EXPECT_DEATH(rtp_sender_video->SendEncodedImage(
                   128, kType, kTimestamp, *encoded_image, video_header,
                   kDefaultExpectedRetransmissionTime),
               "");
}
#endif

TEST_F(RtpSenderVideoWithFrameTransformerTest, OnTransformedFrameSendsVideo) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  ASSERT_TRUE(callback);

  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            callback->OnTransformedFrame(std::move(frame));
          });
  auto encoder_queue = time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
      "encoder_queue", TaskQueueFactory::Priority::NORMAL);
  encoder_queue->PostTask([&] {
    rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                       *encoded_image, video_header,
                                       kDefaultExpectedRetransmissionTime);
  });
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(transport_.packets_sent(), 1);
  encoder_queue->PostTask([&] {
    rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                       *encoded_image, video_header,
                                       kDefaultExpectedRetransmissionTime);
  });
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(transport_.packets_sent(), 2);
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       TransformOverheadCorrectlyAccountedFor) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  ASSERT_TRUE(callback);

  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            const uint8_t data[] = {1, 2,  3,  4,  5,  6,  7,  8,
                                    9, 10, 11, 12, 13, 14, 15, 16};
            frame->SetData(data);
            callback->OnTransformedFrame(std::move(frame));
          });
  auto encoder_queue = time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
      "encoder_queue", TaskQueueFactory::Priority::NORMAL);
  const int kFramesPerSecond = 25;
  for (int i = 0; i < kFramesPerSecond; ++i) {
    encoder_queue->PostTask([&] {
      rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                         *encoded_image, video_header,
                                         kDefaultExpectedRetransmissionTime);
    });
    time_controller_.AdvanceTime(TimeDelta::Millis(1000 / kFramesPerSecond));
  }
  EXPECT_EQ(transport_.packets_sent(), kFramesPerSecond);
  EXPECT_GT(rtp_sender_video->PostEncodeOverhead().bps(), 2200);
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       TransformableFrameMetadataHasCorrectValue) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  video_header.height = 720u;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  generic.dependencies = {5};

  // Check that the transformable frame passed to the frame transformer has the
  // correct metadata.
  EXPECT_CALL(*mock_frame_transformer, Transform)
      .WillOnce(
          [](std::unique_ptr<TransformableFrameInterface> transformable_frame) {
            auto frame =
                absl::WrapUnique(static_cast<TransformableVideoFrameInterface*>(
                    transformable_frame.release()));
            ASSERT_TRUE(frame);
            auto metadata = frame->Metadata();
            EXPECT_EQ(metadata.GetWidth(), 1280u);
            EXPECT_EQ(metadata.GetHeight(), 720u);
            EXPECT_EQ(metadata.GetFrameId(), 10);
            EXPECT_EQ(metadata.GetTemporalIndex(), 3);
            EXPECT_EQ(metadata.GetSpatialIndex(), 2);
            EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5));
            EXPECT_THAT(metadata.GetDecodeTargetIndications(),
                        ElementsAre(DecodeTargetIndication::kSwitch));
          });
  rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                     *encoded_image, video_header,
                                     kDefaultExpectedRetransmissionTime);
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       TransformableFrameHasCorrectCaptureIdentifier) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  auto encoded_image = CreateDefaultEncodedImage();
  encoded_image->SetCaptureTimeIdentifier(Timestamp::Millis(1));
  RTPVideoHeader video_header;

  EXPECT_CALL(*mock_frame_transformer, Transform)
      .WillOnce([&encoded_image](std::unique_ptr<TransformableFrameInterface>
                                     transformable_frame) {
        auto* frame = static_cast<TransformableVideoFrameInterface*>(
            transformable_frame.get());
        ASSERT_TRUE(frame);
        EXPECT_EQ(frame->GetCaptureTimeIdentifier(),
                  encoded_image->CaptureTimeIdentifier());
      });
  rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                     *encoded_image, video_header,
                                     kDefaultExpectedRetransmissionTime);
}

TEST_F(RtpSenderVideoWithFrameTransformerTest,
       OnTransformedFrameSendsVideoWhenCloned) {
  auto mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      CreateSenderWithFrameTransformer(mock_frame_transformer);
  ASSERT_TRUE(callback);

  auto encoded_image = CreateDefaultEncodedImage();
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            auto clone = CloneVideoFrame(
                static_cast<TransformableVideoFrameInterface*>(frame.get()));
            EXPECT_TRUE(clone);
            callback->OnTransformedFrame(std::move(clone));
          });
  auto encoder_queue = time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
      "encoder_queue", TaskQueueFactory::Priority::NORMAL);
  encoder_queue->PostTask([&] {
    rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                       *encoded_image, video_header,
                                       kDefaultExpectedRetransmissionTime);
  });
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(transport_.packets_sent(), 1);
  encoder_queue->PostTask([&] {
    rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp,
                                       *encoded_image, video_header,
                                       kDefaultExpectedRetransmissionTime);
  });
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(transport_.packets_sent(), 2);
}

}  // namespace
}  // namespace webrtc
