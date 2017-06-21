/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/videoencodersoftwarefallbackwrapper.h"

#include <utility>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

const int kWidth = 320;
const int kHeight = 240;
const size_t kMaxPayloadSize = 800;

class VideoEncoderSoftwareFallbackWrapperTest : public ::testing::Test {
 protected:
  VideoEncoderSoftwareFallbackWrapperTest()
      : fallback_wrapper_(cricket::VideoCodec("VP8"), &fake_encoder_) {}

  class CountingFakeEncoder : public VideoEncoder {
   public:
    int32_t InitEncode(const VideoCodec* codec_settings,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      ++init_encode_count_;
      return init_encode_return_code_;
    }
    int32_t Encode(const VideoFrame& frame,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      ++encode_count_;
      if (encode_complete_callback_ &&
          encode_return_code_ == WEBRTC_VIDEO_CODEC_OK) {
        CodecSpecificInfo info;
        info.codec_name = ImplementationName();
        encode_complete_callback_->OnEncodedImage(EncodedImage(), &info,
                                                  nullptr);
      }
      return encode_return_code_;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      encode_complete_callback_ = callback;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() override {
      ++release_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {
      ++set_channel_parameters_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t SetRateAllocation(const BitrateAllocation& bitrate_allocation,
                              uint32_t framerate) override {
      ++set_rates_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    bool SupportsNativeHandle() const override {
      ++supports_native_handle_count_;
      return false;
    }

    const char* ImplementationName() const override {
      return "fake-encoder";
    }

    int init_encode_count_ = 0;
    int32_t init_encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int32_t encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int encode_count_ = 0;
    EncodedImageCallback* encode_complete_callback_ = nullptr;
    int release_count_ = 0;
    int set_channel_parameters_count_ = 0;
    int set_rates_count_ = 0;
    mutable int supports_native_handle_count_ = 0;
  };

  class FakeEncodedImageCallback : public EncodedImageCallback {
   public:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* fragmentation) override {
      ++callback_count_;
      last_codec_name_ = codec_specific_info->codec_name;
      return Result(Result::OK, callback_count_);
    }
    int callback_count_ = 0;
    std::string last_codec_name_;
  };

  void UtilizeFallbackEncoder();
  void FallbackFromEncodeRequest();
  void EncodeFrame();
  void CheckLastEncoderName(const char* expected_name) {
    EXPECT_STREQ(expected_name, callback_.last_codec_name_.c_str());
  }

  FakeEncodedImageCallback callback_;
  CountingFakeEncoder fake_encoder_;
  VideoEncoderSoftwareFallbackWrapper fallback_wrapper_;
  VideoCodec codec_ = {};
  std::unique_ptr<VideoFrame> frame_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

void VideoEncoderSoftwareFallbackWrapperTest::EncodeFrame() {
  rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(kWidth, kHeight);
  I420Buffer::SetBlack(buffer);
  std::vector<FrameType> types(1, kVideoFrameKey);

  frame_.reset(new VideoFrame(buffer, 0, 0, webrtc::kVideoRotation_0));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.Encode(*frame_, nullptr, &types));
}

void VideoEncoderSoftwareFallbackWrapperTest::UtilizeFallbackEncoder() {
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  EXPECT_EQ(&callback_, fake_encoder_.encode_complete_callback_);

  // Register with failing fake encoder. Should succeed with VP8 fallback.
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = 30;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  std::unique_ptr<TemporalLayersFactory> tl_factory(
      new TemporalLayersFactory());
  codec_.VP8()->tl_factory = tl_factory.get();
  rate_allocator_.reset(
      new SimulcastRateAllocator(codec_, std::move(tl_factory)));

  fake_encoder_.init_encode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.InitEncode(&codec_, 2, kMaxPayloadSize));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.SetRateAllocation(
                rate_allocator_->GetAllocation(300000, 30), 30));

  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_.encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_.encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

void VideoEncoderSoftwareFallbackWrapperTest::FallbackFromEncodeRequest() {
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = 30;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  std::unique_ptr<TemporalLayersFactory> tl_factory(
      new TemporalLayersFactory());
  codec_.VP8()->tl_factory = tl_factory.get();
  rate_allocator_.reset(
      new SimulcastRateAllocator(codec_, std::move(tl_factory)));
  fallback_wrapper_.InitEncode(&codec_, 2, kMaxPayloadSize);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.SetRateAllocation(
                rate_allocator_->GetAllocation(300000, 30), 30));
  EXPECT_EQ(1, fake_encoder_.init_encode_count_);

  // Have the non-fallback encoder request a software fallback.
  fake_encoder_.encode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_.encode_count_;
  EncodeFrame();
  // Single encode request, which returned failure.
  EXPECT_EQ(encode_count + 1, fake_encoder_.encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, InitializesEncoder) {
  VideoCodec codec = {};
  fallback_wrapper_.InitEncode(&codec, 2, kMaxPayloadSize);
  EXPECT_EQ(1, fake_encoder_.init_encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, EncodeRequestsFallback) {
  FallbackFromEncodeRequest();
  // After fallback, further encodes shouldn't hit the fake encoder.
  int encode_count = fake_encoder_.encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_.encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, CanUtilizeFallbackEncoder) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderReleasedDuringFallback) {
  EXPECT_EQ(0, fake_encoder_.release_count_);
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_.release_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
  // No extra release when the fallback is released.
  EXPECT_EQ(1, fake_encoder_.release_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderNotEncodingDuringFallback) {
  UtilizeFallbackEncoder();
  int encode_count = fake_encoder_.encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_.encode_count_);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       CanRegisterCallbackWhileUsingFallbackEncoder) {
  UtilizeFallbackEncoder();
  // Registering an encode-complete callback should still work when fallback
  // encoder is being used.
  FakeEncodedImageCallback callback2;
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback2);
  EXPECT_EQ(&callback2, fake_encoder_.encode_complete_callback_);

  // Encoding a frame using the fallback should arrive at the new callback.
  std::vector<FrameType> types(1, kVideoFrameKey);
  frame_->set_timestamp(frame_->timestamp() + 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_.Encode(*frame_, nullptr, &types));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SetChannelParametersForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(0, fake_encoder_.set_channel_parameters_count_);
  fallback_wrapper_.SetChannelParameters(1, 1);
  EXPECT_EQ(1, fake_encoder_.set_channel_parameters_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SetRatesForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_.set_rates_count_);
  fallback_wrapper_.SetRateAllocation(BitrateAllocation(), 1);
  EXPECT_EQ(2, fake_encoder_.set_rates_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleForwardedWithoutFallback) {
  fallback_wrapper_.SupportsNativeHandle();
  EXPECT_EQ(1, fake_encoder_.supports_native_handle_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleNotForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  fallback_wrapper_.SupportsNativeHandle();
  EXPECT_EQ(0, fake_encoder_.supports_native_handle_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_.Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, ReportsImplementationName) {
  VideoCodec codec = {};
  fallback_wrapper_.RegisterEncodeCompleteCallback(&callback_);
  fallback_wrapper_.InitEncode(&codec, 2, kMaxPayloadSize);
  EncodeFrame();
  CheckLastEncoderName("fake-encoder");
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       ReportsFallbackImplementationName) {
  UtilizeFallbackEncoder();
  // Hard coded expected value since libvpx is the software implementation name
  // for VP8. Change accordingly if the underlying implementation does.
  CheckLastEncoderName("libvpx");
}

}  // namespace webrtc
