/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/generic_decoder.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/field_trials.h"
#include "api/rtp_packet_infos.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/color_space.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/encoded_frame.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/video_decoder.h"
#include "common_video/include/corruption_score_calculator.h"
#include "common_video/test/utilities.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/timing/timing.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_field_trials.h"
#include "test/fake_decoder.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::Eq;
using ::testing::Field;
using ::testing::Property;
using ::testing::Return;

namespace webrtc {
namespace video_coding {

class MockCorruptionScoreCalculator : public CorruptionScoreCalculator {
 public:
  MOCK_METHOD(void,
              CalculateCorruptionScore,
              (const VideoFrame& frame,
               const FrameInstrumentationData& frame_instrumentation_data,
               VideoContentType content_type),
              (override));
};

class ReceiveCallback : public VCMReceiveCallback {
 public:
  int32_t OnFrameToRender(const FrameToRender& arguments) override {
    frames_.push_back(arguments.video_frame);
    return 0;
  }

  std::optional<VideoFrame> PopLastFrame() {
    if (frames_.empty())
      return std::nullopt;
    auto ret = frames_.front();
    frames_.pop_back();
    return ret;
  }

  ArrayView<const VideoFrame> GetAllFrames() const { return frames_; }

  void OnDroppedFrames(uint32_t frames_dropped) override {
    frames_dropped_ += frames_dropped;
  }

  uint32_t frames_dropped() const { return frames_dropped_; }

 private:
  std::vector<VideoFrame> frames_;
  uint32_t frames_dropped_ = 0;
};

class GenericDecoderTest : public ::testing::Test {
 protected:
  GenericDecoderTest()
      : time_controller_(Timestamp::Zero()),
        clock_(time_controller_.GetClock()),
        field_trials_(CreateTestFieldTrials()),
        timing_(time_controller_.GetClock(), field_trials_),
        decoder_(time_controller_.GetTaskQueueFactory()),
        vcm_callback_(&timing_,
                      time_controller_.GetClock(),
                      field_trials_,
                      &corruption_score_calculator_),
        generic_decoder_(&decoder_) {}

  void SetUp() override {
    generic_decoder_.RegisterDecodeCompleteCallback(&vcm_callback_);
    vcm_callback_.SetUserReceiveCallback(&user_callback_);
    VideoDecoder::Settings settings;
    settings.set_codec_type(kVideoCodecVP8);
    settings.set_max_render_resolution({10, 10});
    settings.set_number_of_cores(4);
    generic_decoder_.Configure(settings);
  }

  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  FieldTrials field_trials_;
  VCMTiming timing_;
  test::FakeDecoder decoder_;
  VCMDecodedFrameCallback vcm_callback_;
  VCMGenericDecoder generic_decoder_;
  ReceiveCallback user_callback_;
  MockCorruptionScoreCalculator corruption_score_calculator_;
};

TEST_F(GenericDecoderTest, PassesPacketInfos) {
  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  EncodedFrame encoded_frame;
  encoded_frame.SetPacketInfos(packet_infos);
  generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_EQ(decoded_frame->packet_infos().size(), 3U);
}

TEST_F(GenericDecoderTest, FrameDroppedIfTooManyFramesInFlight) {
  constexpr int kMaxFramesInFlight = 10;
  decoder_.SetDelayedDecoding(10);
  for (int i = 0; i < kMaxFramesInFlight + 1; ++i) {
    EncodedFrame encoded_frame;
    encoded_frame.SetRtpTimestamp(90000 * i);
    generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  }

  time_controller_.AdvanceTime(TimeDelta::Millis(10));

  auto frames = user_callback_.GetAllFrames();
  ASSERT_EQ(10U, frames.size());
  // Expect that the first frame was dropped since all decodes released at the
  // same time and the oldest frame info is the first one dropped.
  EXPECT_EQ(frames[0].rtp_timestamp(), 90000u);
  EXPECT_EQ(1u, user_callback_.frames_dropped());
}

TEST_F(GenericDecoderTest, PassesPacketInfosForDelayedDecoders) {
  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  decoder_.SetDelayedDecoding(100);

  {
    // Ensure the original frame is destroyed before the decoding is completed.
    EncodedFrame encoded_frame;
    encoded_frame.SetPacketInfos(packet_infos);
    generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  }

  time_controller_.AdvanceTime(TimeDelta::Millis(200));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_EQ(decoded_frame->packet_infos().size(), 3U);
}

TEST_F(GenericDecoderTest, MaxCompositionDelayNotSetByDefault) {
  EncodedFrame encoded_frame;
  generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_THAT(
      decoded_frame->render_parameters().max_composition_delay_in_frames,
      testing::Eq(std::nullopt));
}

TEST_F(GenericDecoderTest, MaxCompositionDelayActivatedByPlayoutDelay) {
  EncodedFrame encoded_frame;
  // VideoReceiveStream2 would set MaxCompositionDelayInFrames if playout delay
  // is specified as X,Y, where X=0, Y>0.
  constexpr int kMaxCompositionDelayInFrames = 3;  // ~50 ms at 60 fps.
  timing_.SetMaxCompositionDelayInFrames(
      std::make_optional(kMaxCompositionDelayInFrames));
  generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_THAT(
      decoded_frame->render_parameters().max_composition_delay_in_frames,
      testing::Optional(kMaxCompositionDelayInFrames));
}

TEST_F(GenericDecoderTest, IsLowLatencyStreamFalseByDefault) {
  EncodedFrame encoded_frame;
  generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_FALSE(decoded_frame->render_parameters().use_low_latency_rendering);
}

TEST_F(GenericDecoderTest, IsLowLatencyStreamActivatedByPlayoutDelay) {
  EncodedFrame encoded_frame;
  const VideoPlayoutDelay kPlayoutDelay(TimeDelta::Zero(),
                                        TimeDelta::Millis(50));
  timing_.set_playout_delay(kPlayoutDelay);
  generic_decoder_.Decode(encoded_frame, clock_->CurrentTime());
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_TRUE(decoded_frame->render_parameters().use_low_latency_rendering);
}

TEST_F(GenericDecoderTest, CallCalculateCorruptionScoreInDecoded) {
  constexpr uint32_t kRtpTimestamp = 1;
  FrameInfo frame_info;
  frame_info.frame_instrumentation_data.emplace();
  frame_info.frame_instrumentation_data->SetSequenceIndex(1);
  frame_info.rtp_timestamp = kRtpTimestamp;
  frame_info.decode_start = Timestamp::Zero();
  frame_info.content_type = VideoContentType::SCREENSHARE;
  frame_info.frame_type = VideoFrameType::kVideoFrameDelta;
  VideoFrame video_frame = VideoFrame::Builder()
                               .set_video_frame_buffer(I420Buffer::Create(5, 5))
                               .set_rtp_timestamp(kRtpTimestamp)
                               .build();
  vcm_callback_.Map(std::move(frame_info));

  EXPECT_CALL(corruption_score_calculator_,
              CalculateCorruptionScore(
                  Property(&VideoFrame::rtp_timestamp, Eq(kRtpTimestamp)),
                  Property(&FrameInstrumentationData::sequence_index, Eq(1)),
                  VideoContentType::SCREENSHARE));
  vcm_callback_.Decoded(video_frame);
}

TEST_F(GenericDecoderTest, UsesMappedColorSpaceIfSet) {
  constexpr uint32_t kRtpTimestamp = 1;
  const ColorSpace kMappedColorSpace(webrtc::ColorSpace::PrimaryID::kSMPTE240M,
                                     webrtc::ColorSpace::TransferID::kSMPTE240M,
                                     webrtc::ColorSpace::MatrixID::kSMPTE240M,
                                     webrtc::ColorSpace::RangeID::kLimited);
  const ColorSpace kDecoderColorSpace(
      webrtc::ColorSpace::PrimaryID::kBT2020,
      webrtc::ColorSpace::TransferID::kBT2020_10,
      webrtc::ColorSpace::MatrixID::kBT2020_CL,
      webrtc::ColorSpace::RangeID::kFull);

  FrameInfo frame_info;
  frame_info.rtp_timestamp = kRtpTimestamp;
  frame_info.decode_start = Timestamp::Zero();
  frame_info.content_type = VideoContentType::UNSPECIFIED;
  frame_info.frame_type = VideoFrameType::kVideoFrameKey;
  frame_info.color_space = kMappedColorSpace;

  VideoFrame video_frame = VideoFrame::Builder()
                               .set_video_frame_buffer(I420Buffer::Create(5, 5))
                               .set_rtp_timestamp(kRtpTimestamp)
                               .set_color_space(kDecoderColorSpace)
                               .build();
  vcm_callback_.Map(std::move(frame_info));
  vcm_callback_.Decoded(video_frame);

  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_EQ(decoded_frame->color_space(), kMappedColorSpace);
}

TEST_F(GenericDecoderTest, UsesDecoderColorSpaceIfNoneMapped) {
  constexpr uint32_t kRtpTimestamp = 1;
  const ColorSpace kDecoderColorSpace(
      webrtc::ColorSpace::PrimaryID::kBT2020,
      webrtc::ColorSpace::TransferID::kBT2020_10,
      webrtc::ColorSpace::MatrixID::kBT2020_CL,
      webrtc::ColorSpace::RangeID::kFull);

  FrameInfo frame_info;
  frame_info.rtp_timestamp = kRtpTimestamp;
  frame_info.decode_start = Timestamp::Zero();
  frame_info.content_type = VideoContentType::UNSPECIFIED;
  frame_info.frame_type = VideoFrameType::kVideoFrameKey;
  frame_info.color_space = std::nullopt;

  VideoFrame video_frame = VideoFrame::Builder()
                               .set_video_frame_buffer(I420Buffer::Create(5, 5))
                               .set_rtp_timestamp(kRtpTimestamp)
                               .set_color_space(kDecoderColorSpace)
                               .build();
  vcm_callback_.Map(std::move(frame_info));
  vcm_callback_.Decoded(video_frame);

  std::optional<VideoFrame> decoded_frame = user_callback_.PopLastFrame();
  ASSERT_TRUE(decoded_frame.has_value());
  EXPECT_EQ(decoded_frame->color_space(), kDecoderColorSpace);
}

}  // namespace video_coding
}  // namespace webrtc
