/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "api/test/mock_video_encoder.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
using ::testing::Return;

namespace {
const int kWidth = 320;
const int kHeight = 240;
const int kNumCores = 2;
const uint32_t kFramerate = 30;
const size_t kMaxPayloadSize = 800;
const int kDefaultMinPixelsPerFrame = 320 * 180;
const int kLowThreshold = 10;
const int kHighThreshold = 20;

VideoEncoder::EncoderInfo GetEncoderInfo(bool trusted_rate_controller) {
  VideoEncoder::EncoderInfo info;
  info.has_trusted_rate_controller = trusted_rate_controller;
  return info;
}
}  // namespace

class VideoEncoderSoftwareFallbackWrapperTest : public ::testing::Test {
 protected:
  VideoEncoderSoftwareFallbackWrapperTest()
      : VideoEncoderSoftwareFallbackWrapperTest("") {}
  explicit VideoEncoderSoftwareFallbackWrapperTest(
      const std::string& field_trials)
      : override_field_trials_(field_trials),
        fake_encoder_(new CountingFakeEncoder()),
        fallback_wrapper_(CreateVideoEncoderSoftwareFallbackWrapper(
            std::unique_ptr<VideoEncoder>(VP8Encoder::Create()),
            std::unique_ptr<VideoEncoder>(fake_encoder_))) {}

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
        encode_complete_callback_->OnEncodedImage(EncodedImage(), nullptr,
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

    int32_t SetRateAllocation(const VideoBitrateAllocation& bitrate_allocation,
                              uint32_t framerate) override {
      ++set_rates_count_;
      return WEBRTC_VIDEO_CODEC_OK;
    }

    EncoderInfo GetEncoderInfo() const override {
      ++supports_native_handle_count_;
      EncoderInfo info;
      info.scaling_settings = ScalingSettings(kLowThreshold, kHighThreshold);
      info.supports_native_handle = supports_native_handle_;
      info.implementation_name = "fake-encoder";
      return info;
    }

    int init_encode_count_ = 0;
    int32_t init_encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int32_t encode_return_code_ = WEBRTC_VIDEO_CODEC_OK;
    int encode_count_ = 0;
    EncodedImageCallback* encode_complete_callback_ = nullptr;
    int release_count_ = 0;
    int set_rates_count_ = 0;
    mutable int supports_native_handle_count_ = 0;
    bool supports_native_handle_ = false;
  };

  class FakeEncodedImageCallback : public EncodedImageCallback {
   public:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* fragmentation) override {
      ++callback_count_;
      return Result(Result::OK, callback_count_);
    }
    int callback_count_ = 0;
  };

  void UtilizeFallbackEncoder();
  void FallbackFromEncodeRequest();
  void EncodeFrame();
  void EncodeFrame(int expected_ret);
  void CheckLastEncoderName(const char* expected_name) {
    EXPECT_EQ(expected_name,
              fallback_wrapper_->GetEncoderInfo().implementation_name);
  }

  test::ScopedFieldTrials override_field_trials_;
  FakeEncodedImageCallback callback_;
  // |fake_encoder_| is owned and released by |fallback_wrapper_|.
  CountingFakeEncoder* fake_encoder_;
  std::unique_ptr<VideoEncoder> fallback_wrapper_;
  VideoCodec codec_ = {};
  std::unique_ptr<VideoFrame> frame_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

void VideoEncoderSoftwareFallbackWrapperTest::EncodeFrame() {
  EncodeFrame(WEBRTC_VIDEO_CODEC_OK);
}

void VideoEncoderSoftwareFallbackWrapperTest::EncodeFrame(int expected_ret) {
  rtc::scoped_refptr<I420Buffer> buffer =
      I420Buffer::Create(codec_.width, codec_.height);
  I420Buffer::SetBlack(buffer);
  std::vector<FrameType> types(1, kVideoFrameKey);

  frame_.reset(
      new VideoFrame(buffer, webrtc::kVideoRotation_0, 0 /* timestamp_us */));
  EXPECT_EQ(expected_ret, fallback_wrapper_->Encode(*frame_, nullptr, &types));
}

void VideoEncoderSoftwareFallbackWrapperTest::UtilizeFallbackEncoder() {
  fallback_wrapper_->RegisterEncodeCompleteCallback(&callback_);
  EXPECT_EQ(&callback_, fake_encoder_->encode_complete_callback_);

  // Register with failing fake encoder. Should succeed with VP8 fallback.
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = kFramerate;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  rate_allocator_.reset(new SimulcastRateAllocator(codec_));

  fake_encoder_->init_encode_return_code_ = WEBRTC_VIDEO_CODEC_ERROR;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_->InitEncode(&codec_, kNumCores, kMaxPayloadSize));
  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      fallback_wrapper_->SetRateAllocation(
          rate_allocator_->GetAllocation(300000, kFramerate), kFramerate));

  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

void VideoEncoderSoftwareFallbackWrapperTest::FallbackFromEncodeRequest() {
  fallback_wrapper_->RegisterEncodeCompleteCallback(&callback_);
  codec_.codecType = kVideoCodecVP8;
  codec_.maxFramerate = kFramerate;
  codec_.width = kWidth;
  codec_.height = kHeight;
  codec_.VP8()->numberOfTemporalLayers = 1;
  rate_allocator_.reset(new SimulcastRateAllocator(codec_));
  fallback_wrapper_->InitEncode(&codec_, 2, kMaxPayloadSize);
  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      fallback_wrapper_->SetRateAllocation(
          rate_allocator_->GetAllocation(300000, kFramerate), kFramerate));
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);

  // Have the non-fallback encoder request a software fallback.
  fake_encoder_->encode_return_code_ = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  int callback_count = callback_.callback_count_;
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  // Single encode request, which returned failure.
  EXPECT_EQ(encode_count + 1, fake_encoder_->encode_count_);
  EXPECT_EQ(callback_count + 1, callback_.callback_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, InitializesEncoder) {
  VideoCodec codec = {};
  fallback_wrapper_->InitEncode(&codec, 2, kMaxPayloadSize);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, EncodeRequestsFallback) {
  FallbackFromEncodeRequest();
  // After fallback, further encodes shouldn't hit the fake encoder.
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, CanUtilizeFallbackEncoder) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderReleasedDuringFallback) {
  EXPECT_EQ(0, fake_encoder_->release_count_);
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_->release_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
  // No extra release when the fallback is released.
  EXPECT_EQ(1, fake_encoder_->release_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       InternalEncoderNotEncodingDuringFallback) {
  UtilizeFallbackEncoder();
  int encode_count = fake_encoder_->encode_count_;
  EncodeFrame();
  EXPECT_EQ(encode_count, fake_encoder_->encode_count_);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       CanRegisterCallbackWhileUsingFallbackEncoder) {
  UtilizeFallbackEncoder();
  // Registering an encode-complete callback should still work when fallback
  // encoder is being used.
  FakeEncodedImageCallback callback2;
  fallback_wrapper_->RegisterEncodeCompleteCallback(&callback2);
  EXPECT_EQ(&callback2, fake_encoder_->encode_complete_callback_);

  // Encoding a frame using the fallback should arrive at the new callback.
  std::vector<FrameType> types(1, kVideoFrameKey);
  frame_->set_timestamp(frame_->timestamp() + 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            fallback_wrapper_->Encode(*frame_, nullptr, &types));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SetRatesForwardedDuringFallback) {
  UtilizeFallbackEncoder();
  EXPECT_EQ(1, fake_encoder_->set_rates_count_);
  fallback_wrapper_->SetRateAllocation(VideoBitrateAllocation(), 1);
  EXPECT_EQ(2, fake_encoder_->set_rates_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleForwardedWithoutFallback) {
  fallback_wrapper_->GetEncoderInfo();
  EXPECT_EQ(1, fake_encoder_->supports_native_handle_count_);
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest,
       SupportsNativeHandleNotForwardedDuringFallback) {
  // Fake encoder signals support for native handle, default (libvpx) does not.
  fake_encoder_->supports_native_handle_ = true;
  EXPECT_TRUE(fallback_wrapper_->GetEncoderInfo().supports_native_handle);
  UtilizeFallbackEncoder();
  EXPECT_FALSE(fallback_wrapper_->GetEncoderInfo().supports_native_handle);
  // Both times, both encoders are queried.
  EXPECT_EQ(2, fake_encoder_->supports_native_handle_count_);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
}

TEST_F(VideoEncoderSoftwareFallbackWrapperTest, ReportsImplementationName) {
  codec_.width = kWidth;
  codec_.height = kHeight;
  fallback_wrapper_->RegisterEncodeCompleteCallback(&callback_);
  fallback_wrapper_->InitEncode(&codec_, kNumCores, kMaxPayloadSize);
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

namespace {
const int kBitrateKbps = 200;
const int kMinPixelsPerFrame = 1;
const char kFieldTrial[] = "WebRTC-VP8-Forced-Fallback-Encoder-v2";
}  // namespace

class ForcedFallbackTest : public VideoEncoderSoftwareFallbackWrapperTest {
 public:
  explicit ForcedFallbackTest(const std::string& field_trials)
      : VideoEncoderSoftwareFallbackWrapperTest(field_trials) {}

  ~ForcedFallbackTest() override {}

 protected:
  void SetUp() override {
    clock_.SetTimeMicros(1234);
    ConfigureVp8Codec();
  }

  void TearDown() override {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->Release());
  }

  void ConfigureVp8Codec() {
    fallback_wrapper_->RegisterEncodeCompleteCallback(&callback_);
    codec_.codecType = kVideoCodecVP8;
    codec_.maxFramerate = kFramerate;
    codec_.width = kWidth;
    codec_.height = kHeight;
    codec_.VP8()->numberOfTemporalLayers = 1;
    codec_.VP8()->automaticResizeOn = true;
    codec_.VP8()->frameDroppingOn = true;
    rate_allocator_.reset(new SimulcastRateAllocator(codec_));
  }

  void InitEncode(int width, int height) {
    codec_.width = width;
    codec_.height = height;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->InitEncode(
                                         &codec_, kNumCores, kMaxPayloadSize));
    SetRateAllocation(kBitrateKbps);
  }

  void SetRateAllocation(uint32_t bitrate_kbps) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, fallback_wrapper_->SetRateAllocation(
                                         rate_allocator_->GetAllocation(
                                             bitrate_kbps * 1000, kFramerate),
                                         kFramerate));
  }

  void EncodeFrameAndVerifyLastName(const char* expected_name) {
    EncodeFrameAndVerifyLastName(expected_name, WEBRTC_VIDEO_CODEC_OK);
  }

  void EncodeFrameAndVerifyLastName(const char* expected_name,
                                    int expected_ret) {
    EncodeFrame(expected_ret);
    CheckLastEncoderName(expected_name);
  }

  rtc::ScopedFakeClock clock_;
};

class ForcedFallbackTestEnabled : public ForcedFallbackTest {
 public:
  ForcedFallbackTestEnabled()
      : ForcedFallbackTest(std::string(kFieldTrial) + "/Enabled-" +
                           std::to_string(kMinPixelsPerFrame) + "," +
                           std::to_string(kWidth * kHeight) + ",30000/") {}
};

class ForcedFallbackTestDisabled : public ForcedFallbackTest {
 public:
  ForcedFallbackTestDisabled()
      : ForcedFallbackTest(std::string(kFieldTrial) + "/Disabled/") {}
};

TEST_F(ForcedFallbackTestDisabled, NoFallbackWithoutFieldTrial) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIfAtMaxResolutionLimit) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsKeptWhenInitEncodeIsCalled) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder, still expect fallback.
  InitEncode(kWidth / 2, kHeight / 2);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);  // No change.
  EncodeFrameAndVerifyLastName("libvpx");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsEndedWhenResolutionIsTooLarge) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with a larger resolution, expect no fallback.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(2, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, FallbackIsEndedForNonValidSettings) {
  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");

  // Re-initialize encoder with invalid setting, expect no fallback.
  codec_.VP8()->numberOfTemporalLayers = 2;
  InitEncode(kWidth, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Re-initialize encoder with valid setting but fallback disabled from now on.
  codec_.VP8()->numberOfTemporalLayers = 1;
  InitEncode(kWidth, kHeight);
  EXPECT_EQ(2, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");
}

TEST_F(ForcedFallbackTestEnabled, MultipleStartEndFallback) {
  const int kNumRuns = 5;
  for (int i = 1; i <= kNumRuns; ++i) {
    // Resolution at max threshold.
    InitEncode(kWidth, kHeight);
    EncodeFrameAndVerifyLastName("libvpx");
    // Resolution above max threshold.
    InitEncode(kWidth + 1, kHeight);
    EXPECT_EQ(i, fake_encoder_->init_encode_count_);
    EncodeFrameAndVerifyLastName("fake-encoder");
  }
}

TEST_F(ForcedFallbackTestDisabled, GetScaleSettings) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EXPECT_EQ(1, fake_encoder_->init_encode_count_);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Default min pixels per frame should be used.
  const auto settings = fallback_wrapper_->GetEncoderInfo().scaling_settings;
  EXPECT_TRUE(settings.thresholds.has_value());
  EXPECT_EQ(kDefaultMinPixelsPerFrame, settings.min_pixels_per_frame);
}

TEST_F(ForcedFallbackTestEnabled, GetScaleSettingsWithNoFallback) {
  // Resolution above max threshold.
  InitEncode(kWidth + 1, kHeight);
  EncodeFrameAndVerifyLastName("fake-encoder");

  // Configured min pixels per frame should be used.
  const auto settings = fallback_wrapper_->GetEncoderInfo().scaling_settings;
  EXPECT_EQ(kMinPixelsPerFrame, settings.min_pixels_per_frame);
  ASSERT_TRUE(settings.thresholds);
  EXPECT_EQ(kLowThreshold, settings.thresholds->low);
  EXPECT_EQ(kHighThreshold, settings.thresholds->high);
}

TEST_F(ForcedFallbackTestEnabled, GetScaleSettingsWithFallback) {
  // Resolution at max threshold.
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");

  // Configured min pixels per frame should be used.
  const auto settings = fallback_wrapper_->GetEncoderInfo().scaling_settings;
  EXPECT_TRUE(settings.thresholds.has_value());
  EXPECT_EQ(kMinPixelsPerFrame, settings.min_pixels_per_frame);
}

TEST_F(ForcedFallbackTestEnabled, ScalingDisabledIfResizeOff) {
  // Resolution at max threshold.
  codec_.VP8()->automaticResizeOn = false;
  InitEncode(kWidth, kHeight);
  EncodeFrameAndVerifyLastName("libvpx");

  // Should be disabled for automatic resize off.
  const auto settings = fallback_wrapper_->GetEncoderInfo().scaling_settings;
  EXPECT_FALSE(settings.thresholds.has_value());
}

TEST(SoftwareFallbackEncoderTest, BothRateControllersNotTrusted) {
  auto* sw_encoder = new testing::NiceMock<MockVideoEncoder>();
  auto* hw_encoder = new testing::NiceMock<MockVideoEncoder>();

  EXPECT_CALL(*sw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(false)));
  EXPECT_CALL(*hw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(false)));

  std::unique_ptr<VideoEncoder> wrapper =
      CreateVideoEncoderSoftwareFallbackWrapper(
          std::unique_ptr<VideoEncoder>(sw_encoder),
          std::unique_ptr<VideoEncoder>(hw_encoder));
  EXPECT_FALSE(wrapper->GetEncoderInfo().has_trusted_rate_controller);
}

TEST(SoftwareFallbackEncoderTest, SwRateControllerTrusted) {
  auto* sw_encoder = new testing::NiceMock<MockVideoEncoder>();
  auto* hw_encoder = new testing::NiceMock<MockVideoEncoder>();
  EXPECT_CALL(*sw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(true)));
  EXPECT_CALL(*hw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(false)));

  std::unique_ptr<VideoEncoder> wrapper =
      CreateVideoEncoderSoftwareFallbackWrapper(
          std::unique_ptr<VideoEncoder>(sw_encoder),
          std::unique_ptr<VideoEncoder>(hw_encoder));
  EXPECT_FALSE(wrapper->GetEncoderInfo().has_trusted_rate_controller);
}

TEST(SoftwareFallbackEncoderTest, HwRateControllerTrusted) {
  auto* sw_encoder = new testing::NiceMock<MockVideoEncoder>();
  auto* hw_encoder = new testing::NiceMock<MockVideoEncoder>();
  EXPECT_CALL(*sw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(false)));
  EXPECT_CALL(*hw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(true)));

  std::unique_ptr<VideoEncoder> wrapper =
      CreateVideoEncoderSoftwareFallbackWrapper(
          std::unique_ptr<VideoEncoder>(sw_encoder),
          std::unique_ptr<VideoEncoder>(hw_encoder));
  EXPECT_TRUE(wrapper->GetEncoderInfo().has_trusted_rate_controller);

  // Trigger fallback to software.
  EXPECT_CALL(*hw_encoder, Encode)
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE));
  VideoFrame frame = VideoFrame::Builder().build();
  wrapper->Encode(frame, nullptr, nullptr);

  EXPECT_FALSE(wrapper->GetEncoderInfo().has_trusted_rate_controller);
}

TEST(SoftwareFallbackEncoderTest, BothRateControllersTrusted) {
  auto* sw_encoder = new testing::NiceMock<MockVideoEncoder>();
  auto* hw_encoder = new testing::NiceMock<MockVideoEncoder>();
  EXPECT_CALL(*sw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(true)));
  EXPECT_CALL(*hw_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(GetEncoderInfo(true)));

  std::unique_ptr<VideoEncoder> wrapper =
      CreateVideoEncoderSoftwareFallbackWrapper(
          std::unique_ptr<VideoEncoder>(sw_encoder),
          std::unique_ptr<VideoEncoder>(hw_encoder));
  EXPECT_TRUE(wrapper->GetEncoderInfo().has_trusted_rate_controller);
}

}  // namespace webrtc
