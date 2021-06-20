/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder_impl.h"

#include <vector>

#include "api/video/i420_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {
using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

class MockVideoStreamDecoderCallbacks
    : public VideoStreamDecoderInterface::Callbacks {
 public:
  MOCK_METHOD(void, OnNonDecodableState, (), (override));
  MOCK_METHOD(void, OnContinuousUntil, (int64_t frame_id), (override));
  MOCK_METHOD(
      void,
      OnDecodedFrame,
      (VideoFrame frame,
       const VideoStreamDecoderInterface::Callbacks::FrameInfo& frame_info),
      (override));
};

class StubVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD(int32_t,
              InitDecode,
              (const VideoCodec*, int32_t number_of_cores),
              (override));

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    int32_t ret_code = DecodeCall(input_image, missing_frames, render_time_ms);
    if (ret_code == WEBRTC_VIDEO_CODEC_OK ||
        ret_code == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME) {
      VideoFrame frame = VideoFrame::Builder()
                             .set_video_frame_buffer(I420Buffer::Create(1, 1))
                             .build();
      callback_->Decoded(frame);
    }
    return ret_code;
  }

  MOCK_METHOD(int32_t,
              DecodeCall,
              (const EncodedImage& input_image,
               bool missing_frames,
               int64_t render_time_ms),
              ());

  int32_t Release() override { return 0; }

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    callback_ = callback;
    return 0;
  }

 private:
  DecodedImageCallback* callback_;
};

class WrappedVideoDecoder : public VideoDecoder {
 public:
  explicit WrappedVideoDecoder(StubVideoDecoder* decoder) : decoder_(decoder) {}

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    return decoder_->InitDecode(codec_settings, number_of_cores);
  }
  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    return decoder_->Decode(input_image, missing_frames, render_time_ms);
  }
  int32_t Release() override { return decoder_->Release(); }

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }

 private:
  StubVideoDecoder* decoder_;
};

class FakeVideoDecoderFactory : public VideoDecoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return {};
  }
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override {
    if (format.name == "VP8") {
      return std::make_unique<WrappedVideoDecoder>(&vp8_decoder_);
    }

    if (format.name == "AV1") {
      return std::make_unique<WrappedVideoDecoder>(&av1_decoder_);
    }

    return {};
  }

  StubVideoDecoder& Vp8Decoder() { return vp8_decoder_; }
  StubVideoDecoder& Av1Decoder() { return av1_decoder_; }

 private:
  NiceMock<StubVideoDecoder> vp8_decoder_;
  NiceMock<StubVideoDecoder> av1_decoder_;
};

class FakeEncodedFrame : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return 0; }

  // Setters for protected variables.
  void SetPayloadType(int payload_type) { _payloadType = payload_type; }
};

class FrameBuilder {
 public:
  FrameBuilder() : frame_(std::make_unique<FakeEncodedFrame>()) {}

  FrameBuilder& WithPayloadType(int payload_type) {
    frame_->SetPayloadType(payload_type);
    return *this;
  }

  FrameBuilder& WithPictureId(int picture_id) {
    frame_->SetId(picture_id);
    return *this;
  }

  std::unique_ptr<FakeEncodedFrame> Build() { return std::move(frame_); }

 private:
  std::unique_ptr<FakeEncodedFrame> frame_;
};

class VideoStreamDecoderImplTest : public ::testing::Test {
 public:
  VideoStreamDecoderImplTest()
      : time_controller_(Timestamp::Seconds(0)),
        video_stream_decoder_(&callbacks_,
                              &decoder_factory_,
                              time_controller_.GetTaskQueueFactory(),
                              {{1, std::make_pair(SdpVideoFormat("VP8"), 1)},
                               {2, std::make_pair(SdpVideoFormat("AV1"), 1)}}) {
  }

  NiceMock<MockVideoStreamDecoderCallbacks> callbacks_;
  FakeVideoDecoderFactory decoder_factory_;
  GlobalSimulatedTimeController time_controller_;
  VideoStreamDecoderImpl video_stream_decoder_;
};

TEST_F(VideoStreamDecoderImplTest, InsertAndDecodeFrame) {
  video_stream_decoder_.OnFrame(FrameBuilder().WithPayloadType(1).Build());
  EXPECT_CALL(callbacks_, OnDecodedFrame);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(VideoStreamDecoderImplTest, NonDecodableStateWaitingForKeyframe) {
  EXPECT_CALL(callbacks_, OnNonDecodableState);
  time_controller_.AdvanceTime(TimeDelta::Millis(200));
}

TEST_F(VideoStreamDecoderImplTest, NonDecodableStateWaitingForDeltaFrame) {
  video_stream_decoder_.OnFrame(FrameBuilder().WithPayloadType(1).Build());
  EXPECT_CALL(callbacks_, OnDecodedFrame);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_CALL(callbacks_, OnNonDecodableState);
  time_controller_.AdvanceTime(TimeDelta::Millis(3000));
}

TEST_F(VideoStreamDecoderImplTest, InsertAndDecodeFrameWithKeyframeRequest) {
  video_stream_decoder_.OnFrame(FrameBuilder().WithPayloadType(1).Build());
  EXPECT_CALL(decoder_factory_.Vp8Decoder(), DecodeCall)
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME));
  EXPECT_CALL(callbacks_, OnDecodedFrame);
  EXPECT_CALL(callbacks_, OnNonDecodableState);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(VideoStreamDecoderImplTest, FailToInitDecoder) {
  video_stream_decoder_.OnFrame(FrameBuilder().WithPayloadType(1).Build());
  ON_CALL(decoder_factory_.Vp8Decoder(), InitDecode)
      .WillByDefault(Return(WEBRTC_VIDEO_CODEC_ERROR));
  EXPECT_CALL(callbacks_, OnNonDecodableState);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(VideoStreamDecoderImplTest, FailToDecodeFrame) {
  video_stream_decoder_.OnFrame(FrameBuilder().WithPayloadType(1).Build());
  ON_CALL(decoder_factory_.Vp8Decoder(), DecodeCall)
      .WillByDefault(Return(WEBRTC_VIDEO_CODEC_ERROR));
  EXPECT_CALL(callbacks_, OnNonDecodableState);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(VideoStreamDecoderImplTest, ChangeFramePayloadType) {
  video_stream_decoder_.OnFrame(
      FrameBuilder().WithPayloadType(1).WithPictureId(0).Build());
  EXPECT_CALL(decoder_factory_.Vp8Decoder(), DecodeCall);
  EXPECT_CALL(callbacks_, OnDecodedFrame);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));

  video_stream_decoder_.OnFrame(
      FrameBuilder().WithPayloadType(2).WithPictureId(1).Build());
  EXPECT_CALL(decoder_factory_.Av1Decoder(), DecodeCall);
  EXPECT_CALL(callbacks_, OnDecodedFrame);
  time_controller_.AdvanceTime(TimeDelta::Millis(1));
}

}  // namespace
}  // namespace webrtc
