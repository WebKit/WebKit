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

#include "api/video/i420_buffer.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/test/mock/mock_packet_manipulator.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/ptr_util.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/mock/mock_frame_reader.h"
#include "test/testsupport/packet_reader.h"
#include "test/testsupport/unittest_utils.h"
#include "test/video_codec_settings.h"
#include "typedefs.h"  // NOLINT(build/include)

using ::testing::_;
using ::testing::ElementsAre;
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
  VideoProcessorTest() {
    // Get a codec configuration struct and configure it.
    webrtc::test::CodecSettings(kVideoCodecVP8, &config_.codec_settings);
    config_.codec_settings.width = kWidth;
    config_.codec_settings.height = kHeight;

    ExpectInit();
    EXPECT_CALL(frame_reader_mock_, FrameLength())
        .WillRepeatedly(Return(kFrameSize));
    video_processor_ = rtc::MakeUnique<VideoProcessor>(
        &encoder_mock_, &decoder_mock_, &frame_reader_mock_,
        &packet_manipulator_mock_, config_, &stats_,
        nullptr /* encoded_frame_writer */, nullptr /* decoded_frame_writer */);
  }

  void ExpectInit() {
    EXPECT_CALL(encoder_mock_, InitEncode(_, _, _)).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_)).Times(1);
    EXPECT_CALL(decoder_mock_, InitDecode(_, _)).Times(1);
    EXPECT_CALL(decoder_mock_, RegisterDecodeCompleteCallback(_)).Times(1);
  }

  void ExpectRelease() {
    EXPECT_CALL(encoder_mock_, Release()).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_)).Times(1);
    EXPECT_CALL(decoder_mock_, Release()).Times(1);
    EXPECT_CALL(decoder_mock_, RegisterDecodeCompleteCallback(_)).Times(1);
  }

  TestConfig config_;

  MockVideoEncoder encoder_mock_;
  MockVideoDecoder decoder_mock_;
  MockFrameReader frame_reader_mock_;
  MockPacketManipulator packet_manipulator_mock_;
  Stats stats_;
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
  video_processor_->SetRates(kBitrateKbps, kFramerateFps);

  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillRepeatedly(Return(I420Buffer::Create(kWidth, kHeight)));
  EXPECT_CALL(
      encoder_mock_,
      Encode(Property(&VideoFrame::timestamp, 1 * 90000 / kFramerateFps), _, _))
      .Times(1);
  video_processor_->ProcessFrame();

  EXPECT_CALL(
      encoder_mock_,
      Encode(Property(&VideoFrame::timestamp, 2 * 90000 / kFramerateFps), _, _))
      .Times(1);
  video_processor_->ProcessFrame();

  ExpectRelease();
}

TEST_F(VideoProcessorTest, ProcessFrames_VariableFramerate) {
  const int kBitrateKbps = 456;
  const int kStartFramerateFps = 27;
  EXPECT_CALL(encoder_mock_, SetRateAllocation(_, kStartFramerateFps))
      .Times(1)
      .WillOnce(Return(0));
  video_processor_->SetRates(kBitrateKbps, kStartFramerateFps);

  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillRepeatedly(Return(I420Buffer::Create(kWidth, kHeight)));
  EXPECT_CALL(encoder_mock_, Encode(Property(&VideoFrame::timestamp,
                                             1 * 90000 / kStartFramerateFps),
                                    _, _))
      .Times(1);
  video_processor_->ProcessFrame();

  const int kNewFramerateFps = 13;
  EXPECT_CALL(encoder_mock_, SetRateAllocation(_, kNewFramerateFps))
      .Times(1)
      .WillOnce(Return(0));
  video_processor_->SetRates(kBitrateKbps, kNewFramerateFps);

  EXPECT_CALL(encoder_mock_, Encode(Property(&VideoFrame::timestamp,
                                             2 * 90000 / kNewFramerateFps),
                                    _, _))
      .Times(1);
  video_processor_->ProcessFrame();

  ExpectRelease();
}

TEST_F(VideoProcessorTest, SetRates) {
  const int kBitrateKbps = 123;
  const int kFramerateFps = 17;
  EXPECT_CALL(encoder_mock_,
              SetRateAllocation(
                  Property(&BitrateAllocation::get_sum_kbps, kBitrateKbps),
                  kFramerateFps))
      .Times(1);
  video_processor_->SetRates(kBitrateKbps, kFramerateFps);
  EXPECT_THAT(video_processor_->NumberDroppedFramesPerRateUpdate(),
              ElementsAre(0));
  EXPECT_THAT(video_processor_->NumberSpatialResizesPerRateUpdate(),
              ElementsAre(0));

  const int kNewBitrateKbps = 456;
  const int kNewFramerateFps = 34;
  EXPECT_CALL(encoder_mock_,
              SetRateAllocation(
                  Property(&BitrateAllocation::get_sum_kbps, kNewBitrateKbps),
                  kNewFramerateFps))
      .Times(1);
  video_processor_->SetRates(kNewBitrateKbps, kNewFramerateFps);
  EXPECT_THAT(video_processor_->NumberDroppedFramesPerRateUpdate(),
              ElementsAre(0, 0));
  EXPECT_THAT(video_processor_->NumberSpatialResizesPerRateUpdate(),
              ElementsAre(0, 0));

  ExpectRelease();
}

}  // namespace test
}  // namespace webrtc
