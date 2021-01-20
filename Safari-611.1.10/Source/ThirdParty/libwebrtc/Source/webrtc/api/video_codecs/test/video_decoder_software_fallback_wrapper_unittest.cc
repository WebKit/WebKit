/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_software_fallback_wrapper.h"

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

class VideoDecoderSoftwareFallbackWrapperTest : public ::testing::Test {
 protected:
  VideoDecoderSoftwareFallbackWrapperTest()
      : VideoDecoderSoftwareFallbackWrapperTest("") {}
  explicit VideoDecoderSoftwareFallbackWrapperTest(
      const std::string& field_trials)
      : override_field_trials_(field_trials),
        fake_decoder_(new CountingFakeDecoder()),
        fallback_wrapper_(CreateVideoDecoderSoftwareFallbackWrapper(
            std::unique_ptr<VideoDecoder>(VP8Decoder::Create()),
            std::unique_ptr<VideoDecoder>(fake_decoder_))) {}

  class CountingFakeDecoder : public VideoDecoder {
   public:
    int32_t InitDecode(const VideoCodec* codec_settings,
                       int32_t number_of_cores) override {
      ++init_decode_count_;
      return init_decode_return_code_;
    }

    int32_t Decode(const EncodedImage& input_image,
                   bool missing_frames,
                   int64_t render_time_ms) override {
      ++decode_count_;
      return decode_return_code_;
    }

    int32_t RegisterDecodeCompleteCallback(
        DecodedImageCallback* callback) override {
      decode_complete_callback_ = callback;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    int32_t Release() override {
      ++release_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    const char* ImplementationName() const override { return "fake-decoder"; }

    int init_decode_count_ = 0;
    int decode_count_ = 0;
    int32_t init_decode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int32_t decode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    DecodedImageCallback* decode_complete_callback_ = nullptr;
    int release_count_ = 0;
    int reset_count_ = 0;
  };
  test::ScopedFieldTrials override_field_trials_;
  // |fake_decoder_| is owned and released by |fallback_wrapper_|.
  CountingFakeDecoder* fake_decoder_;
  std::unique_ptr<VideoDecoder> fallback_wrapper_;
};

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, InitializesDecoder) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);
  EXPECT_EQ(1, fake_decoder_->init_decode_count_);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, fake_decoder_->init_decode_count_)
      << "Initialized decoder should not be reinitialized.";
  EXPECT_EQ(1, fake_decoder_->decode_count_);
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest,
       UsesFallbackDecoderAfterAnyInitDecodeFailure) {
  VideoCodec codec = {};
  fake_decoder_->init_decode_return_code_ = WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  fallback_wrapper_->InitDecode(&codec, 2);
  EXPECT_EQ(1, fake_decoder_->init_decode_count_);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, fake_decoder_->init_decode_count_)
      << "Should not have attempted reinitializing the fallback decoder on "
         "keyframe.";
  // Unfortunately faking a VP8 frame is hard. Rely on no Decode -> using SW
  // decoder.
  EXPECT_EQ(0, fake_decoder_->decode_count_)
      << "Decoder used even though no InitDecode had succeeded.";
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, IsSoftwareFallbackSticky) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  EncodedImage encoded_image;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, fake_decoder_->decode_count_);

  // Software fallback should be sticky, fake_decoder_ shouldn't be used.
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, fake_decoder_->decode_count_)
      << "Decoder shouldn't be used after failure.";

  // fake_decoder_ should have only been initialized once during the test.
  EXPECT_EQ(1, fake_decoder_->init_decode_count_);
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, DoesNotFallbackOnEveryError) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);
  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EncodedImage encoded_image;
  EXPECT_EQ(fake_decoder_->decode_return_code_,
            fallback_wrapper_->Decode(encoded_image, false, -1));
  EXPECT_EQ(1, fake_decoder_->decode_count_);

  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(2, fake_decoder_->decode_count_)
      << "Decoder should be active even though previous decode failed.";
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, UsesHwDecoderAfterReinit) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  EncodedImage encoded_image;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, fake_decoder_->decode_count_);

  fallback_wrapper_->Release();
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(2, fake_decoder_->decode_count_)
      << "Should not be using fallback after reinit.";
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, ForwardsReleaseCall) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);
  fallback_wrapper_->Release();
  EXPECT_EQ(1, fake_decoder_->release_count_);

  fallback_wrapper_->InitDecode(&codec, 2);
  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  EncodedImage encoded_image;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(2, fake_decoder_->release_count_)
      << "Decoder should be released during fallback.";
  fallback_wrapper_->Release();
  EXPECT_EQ(2, fake_decoder_->release_count_);
}

// TODO(pbos): Fake a VP8 frame well enough to actually receive a callback from
// the software decoder.
TEST_F(VideoDecoderSoftwareFallbackWrapperTest,
       ForwardsRegisterDecodeCompleteCallback) {
  class FakeDecodedImageCallback : public DecodedImageCallback {
    int32_t Decoded(VideoFrame& decodedImage) override { return 0; }
    int32_t Decoded(webrtc::VideoFrame& decodedImage,
                    int64_t decode_time_ms) override {
      RTC_NOTREACHED();
      return -1;
    }
    void Decoded(webrtc::VideoFrame& decodedImage,
                 absl::optional<int32_t> decode_time_ms,
                 absl::optional<uint8_t> qp) override {
      RTC_NOTREACHED();
    }
  } callback;

  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);
  fallback_wrapper_->RegisterDecodeCompleteCallback(&callback);
  EXPECT_EQ(&callback, fake_decoder_->decode_complete_callback_);
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest,
       ReportsFallbackImplementationName) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  EncodedImage encoded_image;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  // Hard coded expected value since libvpx is the software implementation name
  // for VP8. Change accordingly if the underlying implementation does.
  EXPECT_STREQ("libvpx (fallback from: fake-decoder)",
               fallback_wrapper_->ImplementationName());
  fallback_wrapper_->Release();
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest, FallbacksOnTooManyErrors) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  // Doesn't fallback from a single error.
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_STREQ("fake-decoder", fallback_wrapper_->ImplementationName());

  // However, many frames with the same error, fallback should happen.
  const int kNumFramesToEncode = 10;
  for (int i = 0; i < kNumFramesToEncode; ++i) {
    fallback_wrapper_->Decode(encoded_image, false, -1);
  }
  // Hard coded expected value since libvpx is the software implementation name
  // for VP8. Change accordingly if the underlying implementation does.
  EXPECT_STREQ("libvpx (fallback from: fake-decoder)",
               fallback_wrapper_->ImplementationName());
  fallback_wrapper_->Release();
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest,
       DoesNotFallbackOnDeltaFramesErrors) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameDelta;

  // Many decoded frames with the same error
  const int kNumFramesToEncode = 10;
  for (int i = 0; i < kNumFramesToEncode; ++i) {
    fallback_wrapper_->Decode(encoded_image, false, -1);
  }
  EXPECT_STREQ("fake-decoder", fallback_wrapper_->ImplementationName());

  fallback_wrapper_->Release();
}

TEST_F(VideoDecoderSoftwareFallbackWrapperTest,
       DoesNotFallbacksOnNonConsequtiveErrors) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;

  const int kNumFramesToEncode = 10;
  for (int i = 0; i < kNumFramesToEncode; ++i) {
    // Interleaved errors and successful decodes.
    fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
    fallback_wrapper_->Decode(encoded_image, false, -1);
    fake_decoder_->decode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    fallback_wrapper_->Decode(encoded_image, false, -1);
  }
  EXPECT_STREQ("fake-decoder", fallback_wrapper_->ImplementationName());
  fallback_wrapper_->Release();
}

class ForcedSoftwareDecoderFallbackTest
    : public VideoDecoderSoftwareFallbackWrapperTest {
 public:
  ForcedSoftwareDecoderFallbackTest()
      : VideoDecoderSoftwareFallbackWrapperTest(
            "WebRTC-Video-ForcedSwDecoderFallback/Enabled/") {
    fake_decoder_ = new CountingFakeDecoder();
    sw_fallback_decoder_ = new CountingFakeDecoder();
    fallback_wrapper_ = CreateVideoDecoderSoftwareFallbackWrapper(
        std::unique_ptr<VideoDecoder>(sw_fallback_decoder_),
        std::unique_ptr<VideoDecoder>(fake_decoder_));
  }

  CountingFakeDecoder* sw_fallback_decoder_;
};

TEST_F(ForcedSoftwareDecoderFallbackTest, UsesForcedFallback) {
  VideoCodec codec = {};
  fallback_wrapper_->InitDecode(&codec, 2);
  EXPECT_EQ(1, sw_fallback_decoder_->init_decode_count_);

  EncodedImage encoded_image;
  encoded_image._frameType = VideoFrameType::kVideoFrameKey;
  fallback_wrapper_->Decode(encoded_image, false, -1);
  EXPECT_EQ(1, sw_fallback_decoder_->init_decode_count_);
  EXPECT_EQ(1, sw_fallback_decoder_->decode_count_);

  fallback_wrapper_->Release();
  EXPECT_EQ(1, sw_fallback_decoder_->release_count_);

  // Only fallback decoder should have been used.
  EXPECT_EQ(0, fake_decoder_->init_decode_count_);
  EXPECT_EQ(0, fake_decoder_->decode_count_);
  EXPECT_EQ(0, fake_decoder_->release_count_);
}

}  // namespace webrtc
