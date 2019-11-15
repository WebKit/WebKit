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

#include <string>
#include <vector>

#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::ElementsAre;

enum : int {  // The first valid value is 1.
  kAbsoluteSendTimeExtensionId = 1,
  kFrameMarkingExtensionId,
  kGenericDescriptorId00,
  kGenericDescriptorId01,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
};

constexpr int kPayload = 100;
constexpr uint32_t kTimestamp = 10;
constexpr uint16_t kSeqNum = 33;
constexpr uint32_t kSsrc = 725242;
constexpr int kMaxPacketLength = 1500;
constexpr uint64_t kStartTime = 123456789;
constexpr int64_t kDefaultExpectedRetransmissionTimeMs = 125;

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest() {
    receivers_extensions_.Register(kRtpExtensionTransmissionTimeOffset,
                                   kTransmissionTimeOffsetExtensionId);
    receivers_extensions_.Register(kRtpExtensionAbsoluteSendTime,
                                   kAbsoluteSendTimeExtensionId);
    receivers_extensions_.Register(kRtpExtensionTransportSequenceNumber,
                                   kTransportSequenceNumberExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoRotation,
                                   kVideoRotationExtensionId);
    receivers_extensions_.Register(kRtpExtensionVideoTiming,
                                   kVideoTimingExtensionId);
    receivers_extensions_.Register(kRtpExtensionGenericFrameDescriptor00,
                                   kGenericDescriptorId00);
    receivers_extensions_.Register(kRtpExtensionGenericFrameDescriptor01,
                                   kGenericDescriptorId01);
    receivers_extensions_.Register(kRtpExtensionFrameMarking,
                                   kFrameMarkingExtensionId);
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
      : RTPSenderVideo(clock,
                       rtp_sender,
                       flexfec_sender,
                       &playout_delay_oracle_,
                       nullptr,
                       false,
                       false,
                       field_trials) {}
  ~TestRtpSenderVideo() override {}

  bool AllowRetransmission(const RTPVideoHeader& header,
                           int32_t retransmission_settings,
                           int64_t expected_retransmission_time_ms) {
    return RTPSenderVideo::AllowRetransmission(GetTemporalId(header),
                                               retransmission_settings,
                                               expected_retransmission_time_ms);
  }
  PlayoutDelayOracle playout_delay_oracle_;
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
        rtp_sender_([&] {
          RtpRtcp::Configuration config;
          config.clock = &fake_clock_;
          config.outgoing_transport = &transport_;
          config.retransmission_rate_limiter = &retransmission_rate_limiter_;
          config.field_trials = &field_trials_;
          config.local_media_ssrc = kSsrc;
          return config;
        }()),
        rtp_sender_video_(&fake_clock_, &rtp_sender_, nullptr, field_trials_) {
    rtp_sender_.SetSequenceNumber(kSeqNum);
    rtp_sender_.SetTimestampOffset(0);

    rtp_sender_video_.RegisterPayloadType(kPayload, "generic",
                                          /*raw_payload=*/false);
  }

  void PopulateGenericFrameDescriptor(int version);

  void UsesMinimalVp8DescriptorWhenGenericFrameDescriptorExtensionIsUsed(
      int version);

 protected:
  FieldTrials field_trials_;
  SimulatedClock fake_clock_;
  LoopbackTransportTest transport_;
  RateLimiter retransmission_rate_limiter_;

  RTPSender rtp_sender_;
  TestRtpSenderVideo rtp_sender_video_;
};

TEST_P(RtpSenderVideoTest, KeyFrameHasCVO) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_0;
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameKey, kPayload,
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
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(kRtpExtensionVideoTiming,
                                                      kVideoTimingExtensionId));

  const int64_t kCaptureTimestamp = fake_clock_.TimeInMilliseconds();

  RTPVideoHeader hdr;
  hdr.video_timing.flags = VideoSendTiming::kTriggeredByTimer;
  hdr.video_timing.encode_start_delta_ms = kEncodeStartDeltaMs;
  hdr.video_timing.encode_finish_delta_ms = kEncodeFinishDeltaMs;

  fake_clock_.AdvanceTimeMilliseconds(kPacketizationTimeMs);
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameKey, kPayload,
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
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayload, kTimestamp, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  hdr.rotation = kVideoRotation_0;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      VideoFrameType::kVideoFrameDelta, kPayload, kTimestamp + 1, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_0, rotation);
}

TEST_P(RtpSenderVideoTest, DeltaFrameHasCVOWhenNonZero) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));

  RTPVideoHeader hdr;
  hdr.rotation = kVideoRotation_90;
  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      VideoFrameType::kVideoFrameKey, kPayload, kTimestamp, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  EXPECT_TRUE(rtp_sender_video_.SendVideo(
      VideoFrameType::kVideoFrameDelta, kPayload, kTimestamp + 1, 0, kFrame,
      sizeof(kFrame), nullptr, &hdr, kDefaultExpectedRetransmissionTimeMs));

  VideoRotation rotation;
  EXPECT_TRUE(
      transport_.last_sent_packet().GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);
}

TEST_P(RtpSenderVideoTest, CheckH264FrameMarking) {
  uint8_t kFrame[kMaxPacketLength];
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(
                   kRtpExtensionFrameMarking, kFrameMarkingExtensionId));

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
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameDelta, kPayload,
                              kTimestamp, 0, kFrame, sizeof(kFrame), &frag,
                              &hdr, kDefaultExpectedRetransmissionTimeMs);

  FrameMarking fm;
  EXPECT_FALSE(
      transport_.last_sent_packet().GetExtension<FrameMarkingExtension>(&fm));

  hdr.frame_marking.temporal_id = 0;
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameDelta, kPayload,
                              kTimestamp + 1, 0, kFrame, sizeof(kFrame), &frag,
                              &hdr, kDefaultExpectedRetransmissionTimeMs);

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

void RtpSenderVideoTest::PopulateGenericFrameDescriptor(int version) {
  const RTPExtensionType ext_type =
      (version == 0) ? RTPExtensionType::kRtpExtensionGenericFrameDescriptor00
                     : RTPExtensionType::kRtpExtensionGenericFrameDescriptor01;
  const int ext_id =
      (version == 0) ? kGenericDescriptorId00 : kGenericDescriptorId01;

  const int64_t kFrameId = 100000;
  uint8_t kFrame[100];
  EXPECT_EQ(0, rtp_sender_.RegisterRtpHeaderExtension(ext_type, ext_id));

  RTPVideoHeader hdr;
  RTPVideoHeader::GenericDescriptorInfo& generic = hdr.generic.emplace();
  generic.frame_id = kFrameId;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.higher_spatial_layers.push_back(4);
  generic.dependencies.push_back(kFrameId - 1);
  generic.dependencies.push_back(kFrameId - 500);
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameDelta, kPayload,
                              kTimestamp, 0, kFrame, sizeof(kFrame), nullptr,
                              &hdr, kDefaultExpectedRetransmissionTimeMs);

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
  uint8_t spatial_bitmask = 0x14;
  EXPECT_EQ(spatial_bitmask, descriptor_wire.SpatialLayersBitmask());
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
    ASSERT_TRUE(rtp_sender_.RegisterRtpHeaderExtension(
        RtpGenericFrameDescriptorExtension00::kUri, kGenericDescriptorId00));
  } else {
    ASSERT_TRUE(rtp_sender_.RegisterRtpHeaderExtension(
        RtpGenericFrameDescriptorExtension01::kUri, kGenericDescriptorId01));
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
  rtp_sender_video_.RegisterPayloadType(kPayload, "vp8", /*raw_payload=*/false);
  rtp_sender_video_.SendVideo(VideoFrameType::kVideoFrameDelta, kPayload,
                              kTimestamp, 0, kFrame, sizeof(kFrame), nullptr,
                              &hdr, kDefaultExpectedRetransmissionTimeMs);

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

INSTANTIATE_TEST_SUITE_P(WithAndWithoutOverhead,
                         RtpSenderVideoTest,
                         ::testing::Bool());

}  // namespace webrtc
