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

#include "absl/memory/memory.h"
#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_encoder.h"
#include "api/test/videocodec_test_fixture.h"
#include "api/video/i420_buffer.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/test/videocodec_test_stats_impl.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/mock/mock_frame_reader.h"

using ::testing::_;
using ::testing::Property;
using ::testing::Return;

namespace webrtc {
namespace test {

namespace {

const int kWidth = 352;
const int kHeight = 288;
const int kFrameSize = kWidth * kHeight * 3 / 2;  // I420.

}  // namespace

class VideoProcessorTest : public testing::Test {
 protected:
  VideoProcessorTest() : q_("VP queue") {
    config_.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1, false, false,
                             false, kWidth, kHeight);

    decoder_mock_ = new MockVideoDecoder();
    decoders_.push_back(std::unique_ptr<VideoDecoder>(decoder_mock_));

    ExpectInit();
    EXPECT_CALL(frame_reader_mock_, FrameLength())
        .WillRepeatedly(Return(kFrameSize));
    q_.SendTask([this] {
      video_processor_ = absl::make_unique<VideoProcessor>(
          &encoder_mock_, &decoders_, &frame_reader_mock_, config_, &stats_,
          nullptr /* encoded_frame_writer */,
          nullptr /* decoded_frame_writer */);
    });
  }

  ~VideoProcessorTest() {
    q_.SendTask([this] { video_processor_.reset(); });
  }

  void ExpectInit() {
    EXPECT_CALL(encoder_mock_, InitEncode(_, _, _)).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_)).Times(1);
    EXPECT_CALL(*decoder_mock_, InitDecode(_, _)).Times(1);
    EXPECT_CALL(*decoder_mock_, RegisterDecodeCompleteCallback(_)).Times(1);
  }

  void ExpectRelease() {
    EXPECT_CALL(encoder_mock_, Release()).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_)).Times(1);
    EXPECT_CALL(*decoder_mock_, Release()).Times(1);
    EXPECT_CALL(*decoder_mock_, RegisterDecodeCompleteCallback(_)).Times(1);
  }

  rtc::test::TaskQueueForTest q_;

  VideoCodecTestFixture::Config config_;

  MockVideoEncoder encoder_mock_;
  MockVideoDecoder* decoder_mock_;
  std::vector<std::unique_ptr<VideoDecoder>> decoders_;
  MockFrameReader frame_reader_mock_;
  VideoCodecTestStatsImpl stats_;
  std::unique_ptr<VideoProcessor> video_processor_;
};

TEST_F(VideoProcessorTest, InitRelease) {
  ExpectRelease();
}

TEST_F(VideoProcessorTest, ProcessFrames_FixedFramerate) {
  const int kBitrateKbps = 456;
  const int kFramerateFps = 31;
  EXPECT_CALL(encoder_mock_, SetRateAllocation(_, kFramerateFps))
      .Times(1)
      .WillOnce(Return(0));
  q_.SendTask([=] { video_processor_->SetRates(kBitrateKbps, kFramerateFps); });

  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillRepeatedly(Return(I420Buffer::Create(kWidth, kHeight)));
  EXPECT_CALL(
      encoder_mock_,
      Encode(Property(&VideoFrame::timestamp, 1 * 90000 / kFramerateFps), _, _))
      .Times(1);
  q_.SendTask([this] { video_processor_->ProcessFrame(); });

  EXPECT_CALL(
      encoder_mock_,
      Encode(Property(&VideoFrame::timestamp, 2 * 90000 / kFramerateFps), _, _))
      .Times(1);
  q_.SendTask([this] { video_processor_->ProcessFrame(); });

  ExpectRelease();
}

TEST_F(VideoProcessorTest, ProcessFrames_VariableFramerate) {
  const int kBitrateKbps = 456;
  const int kStartFramerateFps = 27;
  const int kStartTimestamp = 90000 / kStartFramerateFps;
  EXPECT_CALL(encoder_mock_, SetRateAllocation(_, kStartFramerateFps))
      .Times(1)
      .WillOnce(Return(0));
  q_.SendTask(
      [=] { video_processor_->SetRates(kBitrateKbps, kStartFramerateFps); });

  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillRepeatedly(Return(I420Buffer::Create(kWidth, kHeight)));
  EXPECT_CALL(encoder_mock_,
              Encode(Property(&VideoFrame::timestamp, kStartTimestamp), _, _))
      .Times(1);
  q_.SendTask([this] { video_processor_->ProcessFrame(); });

  const int kNewFramerateFps = 13;
  EXPECT_CALL(encoder_mock_, SetRateAllocation(_, kNewFramerateFps))
      .Times(1)
      .WillOnce(Return(0));
  q_.SendTask(
      [=] { video_processor_->SetRates(kBitrateKbps, kNewFramerateFps); });

  EXPECT_CALL(encoder_mock_,
              Encode(Property(&VideoFrame::timestamp,
                              kStartTimestamp + 90000 / kNewFramerateFps),
                     _, _))
      .Times(1);
  q_.SendTask([this] { video_processor_->ProcessFrame(); });

  ExpectRelease();
}

TEST_F(VideoProcessorTest, SetRates) {
  const int kBitrateKbps = 123;
  const int kFramerateFps = 17;
  EXPECT_CALL(encoder_mock_,
              SetRateAllocation(
                  Property(&VideoBitrateAllocation::get_sum_kbps, kBitrateKbps),
                  kFramerateFps))
      .Times(1);
  q_.SendTask([=] { video_processor_->SetRates(kBitrateKbps, kFramerateFps); });

  const int kNewBitrateKbps = 456;
  const int kNewFramerateFps = 34;
  EXPECT_CALL(encoder_mock_,
              SetRateAllocation(Property(&VideoBitrateAllocation::get_sum_kbps,
                                         kNewBitrateKbps),
                                kNewFramerateFps))
      .Times(1);
  q_.SendTask(
      [=] { video_processor_->SetRates(kNewBitrateKbps, kNewFramerateFps); });

  ExpectRelease();
}

}  // namespace test
}  // namespace webrtc
