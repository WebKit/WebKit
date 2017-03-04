/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/modules/video_coding/codecs/test/mock/mock_packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/mock/mock_frame_reader.h"
#include "webrtc/test/testsupport/mock/mock_frame_writer.h"
#include "webrtc/test/testsupport/packet_reader.h"
#include "webrtc/test/testsupport/unittest_utils.h"
#include "webrtc/typedefs.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace webrtc {
namespace test {

// Very basic testing for VideoProcessor. It's mostly tested by running the
// video_quality_measurement program.
class VideoProcessorTest : public testing::Test {
 protected:
  MockVideoEncoder encoder_mock_;
  MockVideoDecoder decoder_mock_;
  MockFrameReader frame_reader_mock_;
  MockFrameWriter frame_writer_mock_;
  MockPacketManipulator packet_manipulator_mock_;
  Stats stats_;
  TestConfig config_;
  VideoCodec codec_settings_;

  VideoProcessorTest() {}
  virtual ~VideoProcessorTest() {}
  void SetUp() {
    // Get a codec configuration struct and configure it.
    VideoCodingModule::Codec(kVideoCodecVP8, &codec_settings_);
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = 100;
    config_.codec_settings->width = 352;
    config_.codec_settings->height = 288;
  }
  void TearDown() {}

  void ExpectInit() {
    EXPECT_CALL(encoder_mock_, InitEncode(_, _, _)).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_))
        .Times(AtLeast(1));
    EXPECT_CALL(decoder_mock_, InitDecode(_, _)).Times(1);
    EXPECT_CALL(decoder_mock_, RegisterDecodeCompleteCallback(_))
        .Times(AtLeast(1));
    EXPECT_CALL(frame_reader_mock_, NumberOfFrames()).WillOnce(Return(1));
    EXPECT_CALL(frame_reader_mock_, FrameLength()).WillOnce(Return(152064));
  }
};

TEST_F(VideoProcessorTest, Init) {
  ExpectInit();
  VideoProcessorImpl video_processor(
      &encoder_mock_, &decoder_mock_, &frame_reader_mock_, &frame_writer_mock_,
      &packet_manipulator_mock_, config_, &stats_,
      nullptr /* source_frame_writer */, nullptr /* encoded_frame_writer */,
      nullptr /* decoded_frame_writer */);
  ASSERT_TRUE(video_processor.Init());
}

TEST_F(VideoProcessorTest, ProcessFrame) {
  ExpectInit();
  EXPECT_CALL(encoder_mock_, Encode(_, _, _)).Times(1);
  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillOnce(Return(I420Buffer::Create(50, 50)));
  // Since we don't return any callback from the mock, the decoder will not
  // be more than initialized...
  VideoProcessorImpl video_processor(
      &encoder_mock_, &decoder_mock_, &frame_reader_mock_, &frame_writer_mock_,
      &packet_manipulator_mock_, config_, &stats_,
      nullptr /* source_frame_writer */, nullptr /* encoded_frame_writer */,
      nullptr /* decoded_frame_writer */);
  ASSERT_TRUE(video_processor.Init());
  video_processor.ProcessFrame(0);
}

}  // namespace test
}  // namespace webrtc
