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

#include "api/test/mock_frame_encryptor.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SizeIs;
using ::testing::WithArgs;

enum : int {  // The first valid value is 1.
  kAbsoluteSendTimeExtensionId = 1,
  kFrameMarkingExtensionId,
  kGenericDescriptorId00,
  kGenericDescriptorId01,
  kDependencyDescriptorId,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
  kAbsoluteCaptureTimeExtensionId,
  kPlayoutDelayExtensionId
};

constexpr int kPayload = 100;
constexpr VideoCodecType kType = VideoCodecType::kVideoCodecGeneric;
constexpr uint32_t kTimestamp = 10;
constexpr uint16_t kSeqNum = 33;
constexpr uint32_t kSsrc = 725242;
constexpr int kMaxPacketLength = 1500;
constexpr uint64_t kStartTime = 123456789;
constexpr int64_t kDefaultExpectedRetransmissionTimeMs = 125;

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
        kGenericDescriptorId00);
    receivers_extensions_.Register<RtpGenericFrameDescriptorExtension01>(
        kGenericDescriptorId01);
    receivers_extensions_.Register<RtpDependencyDescriptorExtension>(
        kDependencyDescriptorId);
    receivers_extensions_.Register<FrameMarkingExtension>(
        kFrameMarkingExtensionId);
    receivers_extensions_.Register<AbsoluteCaptureTimeExtension>(
        kAbsoluteCaptureTimeExtensionId);
    receivers_extensions_.Register<PlayoutDelayLimits>(
        kPlayoutDelayExtensionId);
  }

  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    sent_packets_.push_back(RtpPacketReceived(&receivers_extensions_));
    EXPECT_TRUE(sent_packets_.back().Parse(data, len));
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  const RtpPacketReceived& last_sent_packet() { return sent_packets_.back(); }
  int packets_sent() { return sent_packets_.size(); }
  const std::vector<RtpPacketReceived>& sent_packets() const {
    return sent_packets_;
  }

 private:
  RtpHeaderExtensionMap receivers_extensions_;
  std::vector<RtpPacketReceived> sent_packets_;
};

}  // namespace

class TestRtpSenderVideo : public RTPSenderVideo {
 public:
  TestRtpSenderVideo(Clock* clock,
                     RTPSender* rtp_sender,
                     FlexfecSender* flexfec_sender,
                     const WebRtcKeyValueConfig& field_trials)
      : RTPSenderVideo([&] {
          Config config;
          config.clock = clock;
          config.rtp_sender = rtp_sender;
          config.fec_generator = flexfec_sender;
          config.field_trials = &field_trials;
          return config;
        }()) {}
  ~TestRtpSenderVideo() override {}

  bool AllowRetransmission(const RTPVideoHeader& header,
                           int32_t retransmission_settings,
                           int64_t expected_retransmission_time_ms) {
    return RTPSenderVideo::AllowRetransmission(GetTemporalId(header),
                                               retransmission_settings,
                                               expected_retransmission_time_ms);
  }
};

class FieldTrials : public WebRtcKeyValueConfig {
 public:
  explicit FieldTrials(bool use_send_side_bwe_with_overhead)
      : use_send_side_bwe_with_overhead_(use_send_side_bwe_with_overhead) {}

  std::string Lookup(absl::string_view key) const override {
    return key == "WebRTC-SendSideBwe-WithOverhead" &&
                   use_send_side_bwe_with_overhead_
               ? "Enabled"
               : "";
  }

 private:
  bool use_send_side_bwe_with_overhead_;
};

class RtpSenderVideoTest : public ::testing::TestWithParam<bool> {
 public:
  RtpSenderVideoTest()
      : field_trials_(GetParam()),
        fake_clock_(kStartTime),
        retransmission_rate_limiter_(&fake_clock_, 1000),
        rtp_module_(RtpRtcp::Create([&] {
          RtpRtcp::Configuration config;
          config.clock = &fake_clock_;
          config.outgoing_transport = &transport_;
          config.retransmission_rate_limiter = &retransmission_rate_limiter_;
          config.field_trials = &field_trials_;
          config.local_media_ssrc = kSsrc;
          return config;
        }())),
        rtp_sender_video_(&fake_clock_,
                          rtp_module_->RtpSender(),
                          nullptr,
                          field_trials_) {
    rtp_module_->SetSequenceNumber(kSeqNum);
    rtp_module_->SetStartTimestamp(0);
  }

  void PopulateGenericFrameDescriptor(int version);

  void UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(
      int version);

 protected:
  const RtpRtcp::Configuration config_;
  FieldTrials field_trials_;
  SimulatedClock fake_clock_;
  LoopbackTransportTest transport_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RtpRtcp> rtp_module_;
  TestRtpSenderVideo rtp_sender_video_;
};

TEST_P(RtpSenderVideoTest, KeyFrameHasCVO) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::kUri,
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_0;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

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
  rtp_module_->RegisterRtpHeaderExtension(VideoTimingExtension::kUri,
                                          kVideoTimingExtensionId);

  const int64_t kCaptureTimestamp = fake_clock_.TimeInMilliseconds();

  RTPVideoHeader hdr;
  hdr.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  hdr.video_timing.encode_start_delta_ms = kEncodeStartDeltaMs;
  hdr.video_timing.encode_finish_delta_ms = kEncodeFinishDeltaMs;

  fake_clock_.AdvanceTimeMilliseconds(kPacketizationTimeMs);
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, kCaptureTimestamp,
                              kFrame, nullptr, hdr,
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
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::kUri,
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      kPayload, kType, kTimestamp, 0, kFrame, nullptr, hdr,
      kDefaultExpectedRetransmissionTimeMs));

  hdr.rotation = kVideoRotation_0;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      kPayload, kType, kTimestamp + 1, 0, kFrame, nullptr, hdr,
      kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_P(RtpSenderVideoTest, DeltaFrameHasCVOWhenNonZero) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(VideoOrientation::kUri,
                                          kVideoRotationExtensionId);

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      kPayload, kType, kTimestamp, 0, kFrame, nullptr, hdr,
      kDefaultExpectedRetransmissionTimeMs));

  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      kPayload, kType, kTimestamp + 1, 0, kFrame, nullptr, hdr,
      kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);
}

TEST_P(RtpSenderVideoTest, CheckH264FrameMarking) {
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(FrameMarkingExtension::kUri,
                                          kFrameMarkingExtensionId);

  RTPFragmentationHeader frag;
  frag.VerifyAndAllocateFragmentationHeader(1);
  frag.fragmentationOffset[0] = 0;
  frag.fragmentationLength[0] = sizeof(kFrame);

  RTPVideoHeader hdr;
  hdr.video_type_header.emplace<RTPVideoHeaderH264>().packetization_mode =
      H264PacketizationMode::NonInterleaved;
  hdr.codec = kVideoCodecH264;
  hdr.frame_marking.temporal_id = kNoTemporalIdx;
  hdr.frame_marking.tl0_pic_idx = 99;
  hdr.frame_marking.base_layer_sync = true;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, &frag,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

  FrameMarking fm;
  EXPECT_FALSE(
      transport_.last_sent_packet().GetExtension<FrameMarkingExtension>(&fm));

  hdr.frame_marking.temporal_id = 0;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp + 1, 0, kFrame, &frag,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<FrameMarkingExtension>(&fm));
  EXPECT_EQ(hdr.frame_marking.temporal_id, fm.temporal_id);
  EXPECT_EQ(hdr.frame_marking.tl0_pic_idx, fm.tl0_pic_idx);
  EXPECT_EQ(hdr.frame_marking.base_layer_sync, fm.base_layer_sync);
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

  EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesH264) {
  RTPVideoHeader header;
  header.video_type_header.emplace<RTPVideoHeaderH264>().packetization_mode =
      H264PacketizationMode::NonInterleaved;
  header.codec = kVideoCodecH264;
  header.frame_marking.temporal_id = kNoTemporalIdx;

  EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTimeMs));

  // Test higher level retransmit.
  for (int tid = 0; tid <= kMaxTemporalStreams; ++tid) {
    header.frame_marking.temporal_id = tid;
    EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitHigherLayers | kRetransmitBaseLayer,
        kDefaultExpectedRetransmissionTimeMs));
  }
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP8BaseLayer) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;
  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitHigherLayers | kRetransmitBaseLayer,
      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
      header, kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTimeMs));
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
      header, kRetransmitBaseLayer | kConditionallyRetransmitHigherLayers,
      kDefaultExpectedRetransmissionTimeMs));
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP8HigherLayers) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP8;

  auto& vp8_header = header.video_type_header.emplace<RTPVideoHeaderVP8>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp8_header.temporalIdx = tid;

    EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitHigherLayers | kRetransmitBaseLayer,
        kDefaultExpectedRetransmissionTimeMs));
  }
}

TEST_P(RtpSenderVideoTest, RetransmissionTypesVP9) {
  RTPVideoHeader header;
  header.codec = kVideoCodecVP9;

  auto& vp9_header = header.video_type_header.emplace<RTPVideoHeaderVP9>();
  for (int tid = 1; tid <= kMaxTemporalStreams; ++tid) {
    vp9_header.temporal_idx = tid;

    EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitOff, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_FALSE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitBaseLayer, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitHigherLayers, kDefaultExpectedRetransmissionTimeMs));
    EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(
        header, kRetransmitHigherLayers | kRetransmitBaseLayer,
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
    rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs);
    fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  }

  // Since we're at the start of the pattern, the next expected frame in TL0 is
  // right now. We will wait at most one expected retransmission time before
  // acknowledging that it did not arrive, which means this frame and the next
  // will not be retransmitted.
  vp8_header.temporalIdx = 1;
  EXPECT_FALSE(
      rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  EXPECT_FALSE(
      rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // The TL0 frame did not arrive. So allow retransmission.
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Insert a frame for TL2. We just had frame in TL1, so the next one there is
  // in three frames away. TL0 is still too far in the past. So, allow
  // retransmission.
  vp8_header.temporalIdx = 2;
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Another TL2, next in TL1 is two frames away. Allow again.
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
  fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);

  // Yet another TL2, next in TL1 is now only one frame away, so don't store
  // for retransmission.
  EXPECT_FALSE(
      rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
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

    rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs);
    fake_clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
  }

  // Since we're at the start of the pattern, the next expected frame will be
  // right now in TL0. Put it in TL1 instead. Regular rules would dictate that
  // we don't store for retransmission because we expect a frame in a lower
  // layer, but that last frame in TL1 was a long time ago in absolute terms,
  // so allow retransmission anyway.
  vp8_header.temporalIdx = 1;
  EXPECT_TRUE(rtp_sender_video_.AllowRetransmission(header, kSettings, kRttMs));
}

TEST_P(RtpSenderVideoTest, SendsDependencyDescriptorWhenVideoStructureIsSet) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::kUri, kDependencyDescriptorId);
  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 2;
  video_structure.templates = {
      GenericFrameInfo::Builder().S(0).T(0).Dtis("SS").Build(),
      GenericFrameInfo::Builder().S(1).T(0).Dtis("-S").Build(),
      GenericFrameInfo::Builder().S(1).T(1).Dtis("-D").Build(),
  };
  rtp_sender_video_.SetVideoStructure(&video_structure);

  // Send key frame.
  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 0;
  generic.spatial_index = 0;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

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
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

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

TEST_P(RtpSenderVideoTest,
       SetDiffentVideoStructureAvoidsCollisionWithThePreviousStructure) {
  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::kUri, kDependencyDescriptorId);
  FrameDependencyStructure video_structure1;
  video_structure1.num_decode_targets = 2;
  video_structure1.templates = {
      GenericFrameInfo::Builder().S(0).T(0).Dtis("SS").Build(),
      GenericFrameInfo::Builder().S(0).T(1).Dtis("D-").Build(),
  };
  FrameDependencyStructure video_structure2;
  video_structure2.num_decode_targets = 2;
  video_structure2.templates = {
      GenericFrameInfo::Builder().S(0).T(0).Dtis("SS").Build(),
      GenericFrameInfo::Builder().S(0).T(1).Dtis("R-").Build(),
  };

  // Send 1st key frame.
  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SetVideoStructure(&video_structure1);
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
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
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

  ASSERT_EQ(transport_.packets_sent(), 2);
  RtpPacket delta_packet = transport_.last_sent_packet();

  // Send 2nd key frame.
  generic.frame_id = kFrameId + 2;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch,
                                       DecodeTargetIndication::kSwitch};
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SetVideoStructure(&video_structure2);
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
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

TEST_P(RtpSenderVideoTest,
       AuthenticateVideoHeaderWhenDependencyDescriptorExtensionIsUsed) {
  static constexpr size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize] = {1, 2, 3, 4};

  rtp_module_->RegisterRtpHeaderExtension(
      RtpDependencyDescriptorExtension::kUri, kDependencyDescriptorId);
  rtc::scoped_refptr<MockFrameEncryptor> encryptor(
      new rtc::RefCountedObject<NiceMock<MockFrameEncryptor>>);
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
  config.frame_encryptor = encryptor;
  RTPSenderVideo rtp_sender_video(config);

  FrameDependencyStructure video_structure;
  video_structure.num_decode_targets = 1;
  video_structure.templates = {GenericFrameInfo::Builder().Dtis("S").Build()};
  rtp_sender_video.SetVideoStructure(&video_structure);

  // Send key frame.
  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  hdr.generic.emplace().decode_target_indications =
      video_structure.templates[0].decode_target_indications;

  EXPECT_CALL(*encryptor,
              Encrypt(_, _, Not(IsEmpty()), ElementsAreArray(kFrame), _, _));
  rtp_sender_video.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                             hdr, kDefaultExpectedRetransmissionTimeMs);
  // Double check packet with the dependency descriptor is sent.
  ASSERT_EQ(transport_.packets_sent(), 1);
  EXPECT_TRUE(transport_.last_sent_packet()
                  .HasExtension<RtpDependencyDescriptorExtension>());
}

void RtpSenderVideoTest::PopulateGenericFrameDescriptor(int version) {
  const absl::string_view ext_uri =
      (version == 0) ? RtpGenericFrameDescriptorExtension00::kUri
                     : RtpGenericFrameDescriptorExtension01::kUri;
  const int ext_id =
      (version == 0) ? kGenericDescriptorId00 : kGenericDescriptorId01;

  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  rtp_module_->RegisterRtpHeaderExtension(ext_uri, ext_id);

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.dependencies.push_back(kFrameId - 1);
  generic.dependencies.push_back(kFrameId - 500);
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);

  RtpGenericFrameDescriptor descriptor_wire;
  EXPECT_EQ(1, transport_.packets_sent());
  if (version == 0) {
    ASSERT_TRUE(transport_.last_sent_packet()
                    .GetExtension<RtpGenericFrameDescriptorExtension00>(
                        &descriptor_wire));
  } else {
    ASSERT_TRUE(transport_.last_sent_packet()
                    .GetExtension<RtpGenericFrameDescriptorExtension01>(
                        &descriptor_wire));
  }
  EXPECT_EQ(static_cast<uint16_t>(generic.frame_id), descriptor_wire.FrameId());
  EXPECT_EQ(generic.temporal_index, descriptor_wire.TemporalLayer());
  EXPECT_THAT(descriptor_wire.FrameDependenciesDiffs(), ElementsAre(1, 500));
  EXPECT_EQ(descriptor_wire.SpatialLayersBitmask(), 0b0000'0100);
}

TEST_P(RtpSenderVideoTest, PopulateGenericFrameDescriptor00) {
  PopulateGenericFrameDescriptor(0);
}

TEST_P(RtpSenderVideoTest, PopulateGenericFrameDescriptor01) {
  PopulateGenericFrameDescriptor(1);
}

void RtpSenderVideoTest::
    UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(
        int version) {
  const int64_t kFrameId = 100000;
  const size_t kFrameSize = 100;
  uint8_t kFrame[kFrameSize];

  if (version == 0) {
    rtp_module_->RegisterRtpHeaderExtension(
        RtpGenericFrameDescriptorExtension00::kUri, kGenericDescriptorId00);
  } else {
    rtp_module_->RegisterRtpHeaderExtension(
        RtpGenericFrameDescriptorExtension01::kUri, kGenericDescriptorId01);
  }

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
  rtp_sender_video_.SendVideo(kPayload, VideoCodecType::kVideoCodecVP8,
                              kTimestamp, 0, kFrame, nullptr, hdr,
                              kDefaultExpectedRetransmissionTimeMs);

  ASSERT_EQ(transport_.packets_sent(), 1);
  // Expect only minimal 1-byte vp8 descriptor was generated.
  EXPECT_EQ(transport_.last_sent_packet().payload_size(), 1 + kFrameSize);
}

TEST_P(RtpSenderVideoTest,
       UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed00) {
  UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(0);
}

TEST_P(RtpSenderVideoTest,
       UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed01) {
  UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(1);
}

TEST_P(RtpSenderVideoTest, AbsoluteCaptureTime) {
  constexpr int64_t kAbsoluteCaptureTimestampMs = 12345678;
  uint8_t kFrame[kMaxPacketLength];
  rtp_module_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::kUri,
                                          kAbsoluteCaptureTimeExtensionId);

  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp,
                              kAbsoluteCaptureTimestampMs, kFrame, nullptr, hdr,
                              kDefaultExpectedRetransmissionTimeMs);

  // It is expected that one and only one of the packets sent on this video
  // frame has absolute capture time header extension.
  int packets_with_abs_capture_time = 0;
  for (const RtpPacketReceived& packet : transport_.sent_packets()) {
    auto absolute_capture_time =
        packet.GetExtension<AbsoluteCaptureTimeExtension>();
    if (absolute_capture_time) {
      ++packets_with_abs_capture_time;
      EXPECT_EQ(absolute_capture_time->absolute_capture_timestamp,
                Int64MsToUQ32x32(kAbsoluteCaptureTimestampMs + NtpOffsetMs()));
    }
  }
  EXPECT_EQ(packets_with_abs_capture_time, 1);
}

TEST_P(RtpSenderVideoTest, PopulatesPlayoutDelay) {
  // Single packet frames.
  constexpr size_t kPacketSize = 123;
  uint8_t kFrame[kPacketSize];
  rtp_module_->RegisterRtpHeaderExtension(PlayoutDelayLimits::kUri,
                                          kPlayoutDelayExtensionId);
  const PlayoutDelay kExpectedDelay = {10, 20};

  // Send initial key-frame without playout delay.
  RTPVideoHeader hdr;
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  hdr.codec = VideoCodecType::kVideoCodecVP8;
  auto& vp8_header = hdr.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.temporalIdx = 0;

  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
  EXPECT_FALSE(
      transport_.last_sent_packet().HasExtension<PlayoutDelayLimits>());

  // Set playout delay on a discardable frame.
  hdr.playout_delay = kExpectedDelay;
  hdr.frame_type = VideoFrameType::kVideoFrameDelta;
  vp8_header.temporalIdx = 1;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
  PlayoutDelay received_delay = PlayoutDelay::Noop();
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);

  // Set playout delay on a non-discardable frame, the extension should still
  // be populated since dilvery wasn't guaranteed on the last one.
  hdr.playout_delay = PlayoutDelay::Noop();  // Inidcates "no change".
  vp8_header.temporalIdx = 0;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);

  // The next frame does not need the extensions since it's delivery has
  // already been guaranteed.
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
  EXPECT_FALSE(
      transport_.last_sent_packet().HasExtension<PlayoutDelayLimits>());

  // Insert key-frame, we need to refresh the state here.
  hdr.frame_type = VideoFrameType::kVideoFrameKey;
  rtp_sender_video_.SendVideo(kPayload, kType, kTimestamp, 0, kFrame, nullptr,
                              hdr, kDefaultExpectedRetransmissionTimeMs);
  ASSERT_TRUE(transport_.last_sent_packet().GetExtension<PlayoutDelayLimits>(
      &received_delay));
  EXPECT_EQ(received_delay, kExpectedDelay);
}

class MockFrameTransformer : public FrameTransformerInterface {
 public:
  MOCK_METHOD3(TransformFrame,
               void(std::unique_ptr<video_coding::EncodedFrame> frame,
                    std::vector<uint8_t> additional_data,
                    uint32_t ssrc));
  MOCK_METHOD1(RegisterTransformedFrameCallback,
               void(rtc::scoped_refptr<TransformedFrameCallback>));
  MOCK_METHOD0(UnregisterTransformedFrameCallback, void());
};

TEST_P(RtpSenderVideoTest, SendEncodedImageWithFrameTransformer) {
  rtc::scoped_refptr<MockFrameTransformer> transformer =
      new rtc::RefCountedObject<MockFrameTransformer>();
  RTPSenderVideo::Config config;
  config.clock = &fake_clock_;
  config.rtp_sender = rtp_module_->RtpSender();
  config.field_trials = &field_trials_;
  config.frame_transformer = transformer;

  EXPECT_CALL(*transformer, RegisterTransformedFrameCallback(_));
  std::unique_ptr<RTPSenderVideo> rtp_sender_video =
      std::make_unique<RTPSenderVideo>(config);

  const uint8_t data[] = {1, 2, 3, 4};
  EncodedImage encoded_image;
  encoded_image.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
  RTPVideoHeader hdr;
  EXPECT_CALL(*transformer, TransformFrame(_, RtpDescriptorAuthentication(hdr),
                                           rtp_module_->RtpSender()->SSRC()));
  rtp_sender_video->SendEncodedImage(kPayload, kType, kTimestamp, encoded_image,
                                     nullptr, hdr,
                                     kDefaultExpectedRetransmissionTimeMs);

  EXPECT_CALL(*transformer, UnregisterTransformedFrameCallback());
  rtp_sender_video.reset();
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderVideoTest,
                         ::testing::Bool());

}  // namespace webrtc
