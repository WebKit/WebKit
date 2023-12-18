/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

using Decoder = VideoCodecTester::Decoder;
using Encoder = VideoCodecTester::Encoder;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using RawVideoSource = VideoCodecTester::RawVideoSource;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

struct PacingTestParams {
  PacingSettings pacing_settings;
  Frequency framerate;
  int num_frames;
  std::vector<int> expected_delta_ms;
};

VideoFrame CreateVideoFrame(uint32_t timestamp_rtp) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));
  return VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp_rtp)
      .build();
}

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(timestamp_rtp);
  return encoded_image;
}

class MockRawVideoSource : public RawVideoSource {
 public:
  MockRawVideoSource(int num_frames, Frequency framerate)
      : num_frames_(num_frames), frame_num_(0), framerate_(framerate) {}

  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;
    }
    uint32_t timestamp_rtp = frame_num_ * k90kHz / framerate_;
    ++frame_num_;
    return CreateVideoFrame(timestamp_rtp);
  }

  MOCK_METHOD(VideoFrame,
              GetFrame,
              (uint32_t timestamp_rtp, Resolution),
              (override));

 private:
  int num_frames_;
  int frame_num_;
  Frequency framerate_;
};

class MockCodedVideoSource : public CodedVideoSource {
 public:
  MockCodedVideoSource(int num_frames, Frequency framerate)
      : num_frames_(num_frames), frame_num_(0), framerate_(framerate) {}

  absl::optional<EncodedImage> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;
    }
    uint32_t timestamp_rtp = frame_num_ * k90kHz / framerate_;
    ++frame_num_;
    return CreateEncodedImage(timestamp_rtp);
  }

 private:
  int num_frames_;
  int frame_num_;
  Frequency framerate_;
};

class MockDecoder : public Decoder {
 public:
  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(void,
              Decode,
              (const EncodedImage& frame, DecodeCallback callback),
              (override));
  MOCK_METHOD(void, Flush, (), (override));
};

class MockEncoder : public Encoder {
 public:
  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(void,
              Encode,
              (const VideoFrame& frame, EncodeCallback callback),
              (override));
  MOCK_METHOD(void, Flush, (), (override));
};

}  // namespace

class VideoCodecTesterImplPacingTest
    : public ::testing::TestWithParam<PacingTestParams> {
 public:
  VideoCodecTesterImplPacingTest() : test_params_(GetParam()) {}

 protected:
  PacingTestParams test_params_;
};

TEST_P(VideoCodecTesterImplPacingTest, PaceEncode) {
  MockRawVideoSource video_source(test_params_.num_frames,
                                  test_params_.framerate);
  MockEncoder encoder;
  EncoderSettings encoder_settings;
  encoder_settings.pacing = test_params_.pacing_settings;

  VideoCodecTesterImpl tester;
  auto fs =
      tester.RunEncodeTest(&video_source, &encoder, encoder_settings)->Slice();
  ASSERT_EQ(static_cast<int>(fs.size()), test_params_.num_frames);

  for (size_t i = 1; i < fs.size(); ++i) {
    int delta_ms = (fs[i].encode_start - fs[i - 1].encode_start).ms();
    EXPECT_NEAR(delta_ms, test_params_.expected_delta_ms[i - 1], 10);
  }
}

TEST_P(VideoCodecTesterImplPacingTest, PaceDecode) {
  MockCodedVideoSource video_source(test_params_.num_frames,
                                    test_params_.framerate);
  MockDecoder decoder;
  DecoderSettings decoder_settings;
  decoder_settings.pacing = test_params_.pacing_settings;

  VideoCodecTesterImpl tester;
  auto fs =
      tester.RunDecodeTest(&video_source, &decoder, decoder_settings)->Slice();
  ASSERT_EQ(static_cast<int>(fs.size()), test_params_.num_frames);

  for (size_t i = 1; i < fs.size(); ++i) {
    int delta_ms = (fs[i].decode_start - fs[i - 1].decode_start).ms();
    EXPECT_NEAR(delta_ms, test_params_.expected_delta_ms[i - 1], 20);
  }
}

INSTANTIATE_TEST_SUITE_P(
    DISABLED_All,
    VideoCodecTesterImplPacingTest,
    ::testing::ValuesIn(
        {// No pacing.
         PacingTestParams({.pacing_settings = {.mode = PacingMode::kNoPacing},
                           .framerate = Frequency::Hertz(10),
                           .num_frames = 3,
                           .expected_delta_ms = {0, 0}}),
         // Real-time pacing.
         PacingTestParams({.pacing_settings = {.mode = PacingMode::kRealTime},
                           .framerate = Frequency::Hertz(10),
                           .num_frames = 3,
                           .expected_delta_ms = {100, 100}}),
         // Pace with specified constant rate.
         PacingTestParams(
             {.pacing_settings = {.mode = PacingMode::kConstantRate,
                                  .constant_rate = Frequency::Hertz(20)},
              .framerate = Frequency::Hertz(10),
              .num_frames = 3,
              .expected_delta_ms = {50, 50}})}));
}  // namespace test
}  // namespace webrtc
