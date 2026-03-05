/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/evaluation/webrtc_picture_pair_provider.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/units/data_rate.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/svc_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/corruption_detection/evaluation/picture_pair_provider.h"
#include "video/corruption_detection/evaluation/test_clip.h"
#include "video/corruption_detection/evaluation/utils.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::WithoutArgs;

// An arbitrary byte sequence which is used to represent an encoded frame.
constexpr uint8_t kEncodedFrame[] = {0x12, 0x0,  0xa,  0xa,  0x0,  0x0,  0x0,
                                     0x2,  0x27, 0xfe, 0xff, 0xfc, 0xc0, 0x20,
                                     0x32, 0x93, 0x2,  0x10, 0x0,  0xa8, 0x80,
                                     0x0,  0x3,  0x0,  0x10, 0x10, 0x30};
constexpr size_t kEncodedFrameSize =
    sizeof(kEncodedFrame) / sizeof(kEncodedFrame[0]);
// An arbitrary QP value. It is not connected with the `kEncodedFrame`.
constexpr int kQp = 31;

// An arbitrary byte sequence which is used to represent a 2x2 decoded raw
// YUV420 frame.
constexpr uint8_t kDecodedChannelYContent[4] = {0x16, 0x21, 0x59, 0x11};
constexpr uint8_t kDecodedChannelUContent[1] = {0x65};
constexpr uint8_t kDecodedChannelVContent[1] = {0x89};
constexpr int kDecodedWidth = 2;
constexpr int kDecodedHeight = 2;
constexpr int kDecodedStrideY = 2;
constexpr int kDecodedStrideU = 1;
constexpr int kDecodedStrideV = 1;

constexpr int kDummyVideoWidth = 2;
constexpr int kDummyVideoHeight = 2;
constexpr int kNumFrames = 2;
// Two frames of dummy video.
constexpr uint8_t kDummyFileContent[kDummyVideoWidth * kDummyVideoHeight * 3] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

// Test file name and information about it.
constexpr absl::string_view kFilename = "ConferenceMotion_1280_720_50";
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kFramerate = 50;

// 90kHz clock for video.
constexpr uint32_t kRtpTimestampInterval = 90'000 / kFramerate;
constexpr VideoCodecMode kCodecMode = VideoCodecMode::kRealtimeVideo;

constexpr float kBitsPerPixel = 0.1;
constexpr DataRate kDefaultBitrate =
    DataRate::BitsPerSec(kWidth * kHeight * kFramerate * kBitsPerPixel);

constexpr int kFramesToLoop = 5;

// According to "Bankoski, J., Wilkins, P., & Xu, Y. (2011, July). Technical
// overview of VP8, an open source video codec for the web. In 2011 IEEE
// International Conference on Multimedia and Expo (pp. 1-6). IEEE.", a
// compressed video should have a PSNR higher than ~30 dB to be watchable.
// Hence, with `kBitsPerPixel` given to be 0.1 we should expect to have a PSNR >
// 30 dB.
constexpr double kWatchablePsnrDb = 30.0;
constexpr double kGoodPsnrDb = 40.0;

TestClip GetTestClip() {
  return TestClip::CreateYuvClip(kFilename, kWidth, kHeight, kFramerate,
                                 kCodecMode);
}

class WebRtcPicturePairProviderTest : public TestWithParam<VideoCodecType> {
 protected:
  WebRtcPicturePairProviderTest()
      : encoder_factory_(std::make_unique<MockVideoEncoderFactory>()),
        decoder_factory_(std::make_unique<MockVideoDecoderFactory>()),
        codec_type_(GetParam()) {}

  ~WebRtcPicturePairProviderTest() override = default;

  void ExpectedCallsInConstructor() {
    RTC_CHECK(encoder_factory_);
    RTC_CHECK(decoder_factory_);

    EXPECT_CALL(*encoder_factory_, Create).WillOnce(WithoutArgs([&] {
      std::unique_ptr<VideoEncoder> encoder =
          std::make_unique<MockVideoEncoder>();
      encoder_ = static_cast<MockVideoEncoder*>(encoder.get());

      EXPECT_CALL(*encoder_, RegisterEncodeCompleteCallback)
          .WillOnce([&](EncodedImageCallback* callback) {
            encode_callback_ = callback;
            return WEBRTC_VIDEO_CODEC_OK;
          });

      return encoder;
    }));

    EXPECT_CALL(*decoder_factory_, Create).WillOnce(WithoutArgs([&] {
      std::unique_ptr<VideoDecoder> decoder =
          std::make_unique<MockVideoDecoder>();
      decoder_ = static_cast<MockVideoDecoder*>(decoder.get());

      EXPECT_CALL(*decoder_, RegisterDecodeCompleteCallback)
          .WillOnce([&](DecodedImageCallback* callback) {
            decode_callback_ = callback;
            return WEBRTC_VIDEO_CODEC_OK;
          });

      return decoder;
    }));
  }

  void CreatePicturePairProvider(VideoCodecType codec_type) {
    picture_pair_provider_ =
        std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
            codec_type, std::move(encoder_factory_),
            std::move(decoder_factory_));
  }

  void TearDown() override {
    if (!skipped_test_) {
      RTC_DCHECK(picture_pair_provider_ != nullptr);

      EXPECT_CALL(*encoder_, Release);
      EXPECT_CALL(*decoder_, Release);

      EXPECT_CALL(*encoder_, RegisterEncodeCompleteCallback(nullptr));
      EXPECT_CALL(*decoder_, RegisterDecodeCompleteCallback(nullptr));
    }
  }

  std::unique_ptr<WebRtcEncoderDecoderPicturePairProvider>
      picture_pair_provider_ = nullptr;

  std::unique_ptr<MockVideoEncoderFactory> encoder_factory_;
  std::unique_ptr<MockVideoDecoderFactory> decoder_factory_;

  MockVideoEncoder* encoder_ = nullptr;
  MockVideoDecoder* decoder_ = nullptr;

  EncodedImageCallback* encode_callback_ = nullptr;
  DecodedImageCallback* decode_callback_ = nullptr;

  const VideoCodecType codec_type_;
  bool skipped_test_ = false;
};

INSTANTIATE_TEST_SUITE_P(DifferentCodecTypes,
                         WebRtcPicturePairProviderTest,
                         Values(VideoCodecType::kVideoCodecVP8,
                                VideoCodecType::kVideoCodecVP9,
                                VideoCodecType::kVideoCodecAV1,
                                VideoCodecType::kVideoCodecH264));

#if GTEST_HAS_DEATH_TEST
TEST_P(WebRtcPicturePairProviderTest, RaiseErrorIfConfigureHasNotBeenCalled) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  EXPECT_DEATH(picture_pair_provider_->GetNextPicturePair(),
               HasSubstr("Encoder and decoder have not been initialized. Try "
                         "calling Configure first"));
}
#endif  // GTEST_HAS_DEATH_TEST

TEST_P(WebRtcPicturePairProviderTest, ConfigurationIsSuccesfull) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, ConfigureWhenInitialized) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(VideoCodecType::kVideoCodecAV1);

  ASSERT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));

  // If initiated encoder_ and decoder_ would have been released, still the
  // configuration would work.
  EXPECT_CALL(*encoder_, Release);
  EXPECT_CALL(*decoder_, Release);
  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

#if GTEST_HAS_DEATH_TEST
TEST_P(WebRtcPicturePairProviderTest, NonExistentFileShouldRaiseError) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  EXPECT_DEATH(TestClip::CreateY4mClip("does_not_exist", kCodecMode), _);
}
#endif  // GTEST_HAS_DEATH_TEST

TEST_P(WebRtcPicturePairProviderTest, InitializeEncoder) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);

  // Tests that ConfigureEncoderSettings() and InitializeEncoder() work
  // properly. Need to compare fields because the VideoCodec class has
  // deleted the equality operator.
  if (codec_type_ == kVideoCodecVP9 || codec_type_ == kVideoCodecAV1) {
    EXPECT_CALL(
        *encoder_,
        InitEncode(
            AllOf(Field(&VideoCodec::width, kWidth),
                  Field(&VideoCodec::height, kHeight),
                  Field(&VideoCodec::maxFramerate, kFramerate),
                  Field(&VideoCodec::codecType, codec_type_),
                  Field(&VideoCodec::minBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::startBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::maxBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::mode, kCodecMode),
                  Property(&VideoCodec::GetScalabilityMode,
                           ScalabilityMode::kL1T3),
                  Field(&VideoCodec::qpMax, 63)),
            AllOf(Field(&VideoEncoder::Settings::number_of_cores, 1),
                  Field(&VideoEncoder::Settings::max_payload_size, 1500))));
  } else {
    EXPECT_CALL(
        *encoder_,
        InitEncode(
            AllOf(Field(&VideoCodec::width, kWidth),
                  Field(&VideoCodec::height, kHeight),
                  Field(&VideoCodec::maxFramerate, kFramerate),
                  Field(&VideoCodec::codecType, codec_type_),
                  Field(&VideoCodec::minBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::startBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::maxBitrate, kDefaultBitrate.kbps()),
                  Field(&VideoCodec::mode, kCodecMode),
                  Field(&VideoCodec::qpMax,
                        codec_type_ == kVideoCodecH264 ? 51 : 63)),
            AllOf(Field(&VideoEncoder::Settings::number_of_cores, 1),
                  Field(&VideoEncoder::Settings::max_payload_size, 1500))));
  }

  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, FailEncoderInitialization) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  EXPECT_CALL(*encoder_, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_ERROR));
  EXPECT_FALSE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, SetRatesWithSimulcastRateAllocator) {
  if (codec_type_ != VideoCodecType::kVideoCodecVP8 &&
      codec_type_ != VideoCodecType::kVideoCodecH264) {
    skipped_test_ = true;
    GTEST_SKIP() << "SimulcastRateAllocator is only used for VP8 and H264.";
  }
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);

  VideoCodec codec_config;
  codec_config.width = kWidth;
  codec_config.height = kHeight;
  codec_config.maxFramerate = kFramerate;
  codec_config.codecType = codec_type_;
  codec_config.minBitrate = kDefaultBitrate.kbps();
  codec_config.startBitrate = kDefaultBitrate.kbps();
  codec_config.maxBitrate = kDefaultBitrate.kbps();
  codec_config.mode = kCodecMode;

  if (codec_type_ == VideoCodecType::kVideoCodecH264) {
    codec_config.H264()->numberOfTemporalLayers = 3;
    codec_config.qpMax = 51;
    // For H264 one needs to specify the number of temporal layers for each
    // spatial layer, which we specifically test here. This is because of how
    // `SimulcastRateAllocator::NumTemporalStreams` is in
    // modules/video_coding/utility/simulcast_rate_allocator.cc.
    codec_config.simulcastStream[0].numberOfTemporalLayers = 3;
  } else {
    codec_config.VP8()->numberOfTemporalLayers = 3;
    codec_config.qpMax = 63;
  }

  SimulcastRateAllocator simulcast_rate_allocator(CreateEnvironment(),
                                                  codec_config);
  VideoEncoder::RateControlParameters rate_params(
      simulcast_rate_allocator.GetAllocation(kDefaultBitrate.bps(), kFramerate),
      kFramerate, kDefaultBitrate);

  // Tests that SetRates() works properly with SimulcastRateAllocator.
  EXPECT_CALL(*encoder_, SetRates(rate_params));

  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, SetRatesWithSvcRateAllocator) {
  if (codec_type_ != VideoCodecType::kVideoCodecVP9 &&
      codec_type_ != VideoCodecType::kVideoCodecAV1) {
    skipped_test_ = true;
    GTEST_SKIP() << "SvcRateAllocator is only used for VP9 and AV1.";
  }
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);

  VideoCodec codec_config;
  codec_config.width = kWidth;
  codec_config.height = kHeight;
  codec_config.maxFramerate = kFramerate;
  codec_config.codecType = codec_type_;
  codec_config.minBitrate = kDefaultBitrate.kbps();
  codec_config.startBitrate = kDefaultBitrate.kbps();
  codec_config.maxBitrate = kDefaultBitrate.kbps();
  codec_config.mode = kCodecMode;
  codec_config.SetScalabilityMode(ScalabilityMode::kL1T3);
  codec_config.qpMax = 63;

  unsigned int bitrate_kbps = static_cast<unsigned int>(kDefaultBitrate.kbps());
  codec_config.spatialLayers[0].targetBitrate = bitrate_kbps;
  codec_config.spatialLayers[0].maxBitrate = bitrate_kbps;
  codec_config.spatialLayers[0].active = true;

  SvcRateAllocator svc_rate_allocator(codec_config, FieldTrials(""));
  VideoEncoder::RateControlParameters rate_params =
      VideoEncoder::RateControlParameters(
          svc_rate_allocator.GetAllocation(kDefaultBitrate.bps(), kFramerate),
          kFramerate, kDefaultBitrate);

  // Tests that SetRates() works properly with SvcRateAllocator.
  EXPECT_CALL(*encoder_, SetRates(rate_params));

  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, ConfigureDecoderTest) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);

  EXPECT_CALL(*decoder_,
              Configure(AllOf(
                  Property(&VideoDecoder::Settings::codec_type, codec_type_))));

  EXPECT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, ConfigureDecoderFailure) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);

  EXPECT_CALL(*decoder_, Configure).WillOnce(Return(false));

  EXPECT_FALSE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));
}

TEST_P(WebRtcPicturePairProviderTest, EncodeTest) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  ASSERT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));

  constexpr int call_picture_pair_provider = 3;

  {
    InSequence s;

    // VideoFrame does not have a Matcher operator, hence fields need to
    // be checked.
    EXPECT_CALL(*encoder_,
                Encode(AllOf(Property(&VideoFrame::width, kWidth),
                             Property(&VideoFrame::height, kHeight),
                             Property(&VideoFrame::rtp_timestamp, 0)),
                       // Only the first frame should be a key frame.
                       Pointee(ElementsAre(VideoFrameType::kVideoFrameKey))));

    EXPECT_CALL(*encoder_,
                Encode(AllOf(Property(&VideoFrame::width, kWidth),
                             Property(&VideoFrame::height, kHeight),
                             Property(&VideoFrame::rtp_timestamp,
                                      kRtpTimestampInterval)),
                       Pointee(ElementsAre(VideoFrameType::kVideoFrameDelta))));

    EXPECT_CALL(*encoder_,
                Encode(AllOf(Property(&VideoFrame::width, kWidth),
                             Property(&VideoFrame::height, kHeight),
                             Property(&VideoFrame::rtp_timestamp,
                                      kRtpTimestampInterval * 2)),
                       Pointee(ElementsAre(VideoFrameType::kVideoFrameDelta))));
  }

  for (int i = 0; i < call_picture_pair_provider; ++i) {
    // `encoded_image_` should not have a value, and therefore `std::nullopt`
    // is expected to be returned.
    EXPECT_EQ(picture_pair_provider_->GetNextPicturePair(), std::nullopt);
  }
}

TEST_P(WebRtcPicturePairProviderTest, EncodeFailure) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  ASSERT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));

  EXPECT_CALL(*encoder_, Encode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_ERROR));

  EXPECT_EQ(picture_pair_provider_->GetNextPicturePair(), std::nullopt);
}

// Populates `encoded_image` with arbitrary values.
void PopulateEncodedImage(EncodedImage* encoded_image,
                          VideoCodecType codec_type) {
  scoped_refptr<EncodedImageBuffer> encoded_image_buffer =
      EncodedImageBuffer::Create(kEncodedFrame, kEncodedFrameSize);

  encoded_image->SetEncodedData(encoded_image_buffer);
}

TEST_P(WebRtcPicturePairProviderTest, DecodeFailureTest) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  ASSERT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));

  EncodedImage encoded_image;
  PopulateEncodedImage(&encoded_image, codec_type_);
  CodecSpecificInfo codec_specific_info;

  EXPECT_CALL(*encoder_, Encode(_, _))
      .WillOnce([&](const VideoFrame& frame,
                    const std::vector<VideoFrameType>* frame_types) {
        encode_callback_->OnEncodedImage(encoded_image, &codec_specific_info);
        return WEBRTC_VIDEO_CODEC_OK;
      });

  EXPECT_CALL(*decoder_, Decode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_ERROR));

  EXPECT_EQ(picture_pair_provider_->GetNextPicturePair(), std::nullopt);
}

TEST_P(WebRtcPicturePairProviderTest, ProperDecodeTest) {
  ExpectedCallsInConstructor();
  CreatePicturePairProvider(codec_type_);
  ASSERT_TRUE(
      picture_pair_provider_->Configure(GetTestClip(), kDefaultBitrate));

  EncodedImage encoded_image;
  PopulateEncodedImage(&encoded_image, codec_type_);
  CodecSpecificInfo codec_specific_info;

  EXPECT_CALL(*encoder_, Encode(_, _))
      .WillOnce([&](const VideoFrame& frame,
                    const std::vector<VideoFrameType>* frame_types) {
        encoded_image.qp_ = kQp;
        encode_callback_->OnEncodedImage(encoded_image, &codec_specific_info);
        return WEBRTC_VIDEO_CODEC_OK;
      });

  scoped_refptr<I420Buffer> decoded_frame = I420Buffer::Copy(
      kDecodedWidth, kDecodedHeight, kDecodedChannelYContent, kDecodedStrideY,
      kDecodedChannelUContent, kDecodedStrideU, kDecodedChannelVContent,
      kDecodedStrideV);

  VideoFrame decoded_image = VideoFrame::Builder()
                                 .set_video_frame_buffer(decoded_frame)
                                 .set_rtp_timestamp(0)
                                 .build();

  EXPECT_CALL(*decoder_, Decode(_, _))
      .WillOnce([&](const EncodedImage& input_image, int64_t render_time_ms) {
        decode_callback_->Decoded(decoded_image);
        return WEBRTC_VIDEO_CODEC_OK;
      });

  std::optional<OriginalCompressedPicturePair> picture_pair =
      picture_pair_provider_->GetNextPicturePair();
  ASSERT_NE(picture_pair, std::nullopt);

  // `picture_pair->compressed_image` must be the same as the toy decoded raw
  // video.
  for (int i = 0; i < kDecodedWidth * kDecodedHeight; ++i) {
    EXPECT_EQ(picture_pair->compressed_image.video_frame_buffer()
                  ->ToI420()
                  ->DataY()[i],
              kDecodedChannelYContent[i]);
  }
  EXPECT_EQ(
      picture_pair->compressed_image.video_frame_buffer()->ToI420()->DataU()[0],
      kDecodedChannelUContent[0]);
  EXPECT_EQ(
      picture_pair->compressed_image.video_frame_buffer()->ToI420()->DataV()[0],
      kDecodedChannelVContent[0]);
  EXPECT_EQ(picture_pair->frame_average_qp, kQp);
}

class WebRtcPicturePairProviderEnd2EndTest
    : public TestWithParam<VideoCodecType> {
 protected:
  WebRtcPicturePairProviderEnd2EndTest() : codec_type_(GetParam()) {}

  ~WebRtcPicturePairProviderEnd2EndTest() override = default;

  int GetMapQP() {
    switch (codec_type_) {
      case VideoCodecType::kVideoCodecVP8:
        return 127;
      case VideoCodecType::kVideoCodecAV1:
      case VideoCodecType::kVideoCodecVP9:
        return 255;
      case VideoCodecType::kVideoCodecH264:
        return 51;
      default:
        RTC_DCHECK_NOTREACHED();
        return 0;
    }
  }

  VideoCodecType codec_type_;
};

// H264 is not built in WebRTC.
INSTANTIATE_TEST_SUITE_P(DifferentCodecTypes,
                         WebRtcPicturePairProviderEnd2EndTest,
                         Values(VideoCodecType::kVideoCodecVP8,
                                VideoCodecType::kVideoCodecVP9,
                                // VideoCodecType::kVideoCodecH264,
                                VideoCodecType::kVideoCodecAV1));

TEST_P(WebRtcPicturePairProviderEnd2EndTest, PsnrIsInExpectedRange) {
  std::unique_ptr<WebRtcEncoderDecoderPicturePairProvider> webrtc_provider;
  if (codec_type_ != VideoCodecType::kVideoCodecAV1) {
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_, CreateBuiltinVideoEncoderFactory(),
        CreateBuiltinVideoDecoderFactory());
  } else {
    // AV1 is injectible
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_,
        std::make_unique<
            VideoEncoderFactoryTemplate<LibaomAv1EncoderTemplateAdapter>>(),
        std::make_unique<
            VideoDecoderFactoryTemplate<Dav1dDecoderTemplateAdapter>>());
  }

  ASSERT_TRUE(webrtc_provider->Configure(GetTestClip(), kDefaultBitrate));

  for (int i = 0; i < kFramesToLoop; ++i) {
    std::optional<OriginalCompressedPicturePair> picture_pair =
        webrtc_provider->GetNextPicturePair();

    EXPECT_NE(picture_pair, std::nullopt);
    EXPECT_GE(picture_pair->frame_average_qp, 0);
    EXPECT_LE(picture_pair->frame_average_qp, GetMapQP());

    // Calculate the PSNR between the original and compressed image.
    double psnr = I420PSNR(
        *picture_pair->original_image.video_frame_buffer()->GetI420(),
        *picture_pair->compressed_image.video_frame_buffer()->GetI420());
    EXPECT_GE(psnr, kWatchablePsnrDb);

    // Test timestamp.
    EXPECT_EQ(picture_pair->original_image.rtp_timestamp(),
              kRtpTimestampInterval * i);
    EXPECT_EQ(picture_pair->compressed_image.rtp_timestamp(),
              kRtpTimestampInterval * i);
  }
}

TEST_P(WebRtcPicturePairProviderEnd2EndTest, PsnrIsGoodWhenBitrateIsHigh) {
  std::unique_ptr<WebRtcEncoderDecoderPicturePairProvider> webrtc_provider;
  if (codec_type_ != VideoCodecType::kVideoCodecAV1) {
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_, CreateBuiltinVideoEncoderFactory(),
        CreateBuiltinVideoDecoderFactory());
  } else {
    // AV1 is injectible
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_,
        std::make_unique<
            VideoEncoderFactoryTemplate<LibaomAv1EncoderTemplateAdapter>>(),
        std::make_unique<
            VideoDecoderFactoryTemplate<Dav1dDecoderTemplateAdapter>>());
  }

  const DataRate high_bitrate =
      DataRate::BitsPerSec(kWidth * kHeight * kFramerate * 10);

  ASSERT_TRUE(webrtc_provider->Configure(GetTestClip(), high_bitrate));

  for (int i = 0; i < kFramesToLoop; ++i) {
    std::optional<OriginalCompressedPicturePair> picture_pair =
        webrtc_provider->GetNextPicturePair();

    EXPECT_NE(picture_pair, std::nullopt);
    EXPECT_GE(picture_pair->frame_average_qp, 0);
    EXPECT_LE(picture_pair->frame_average_qp, GetMapQP());

    double psnr = I420PSNR(
        *picture_pair->original_image.video_frame_buffer()->GetI420(),
        *picture_pair->compressed_image.video_frame_buffer()->GetI420());

    // For high enough bitrate the `I420PSNR` should return quite high PSNR
    // score.
    EXPECT_GE(psnr, kGoodPsnrDb);
  }
}

TEST_P(WebRtcPicturePairProviderEnd2EndTest, Y4mVideoTest) {
  std::unique_ptr<WebRtcEncoderDecoderPicturePairProvider> webrtc_provider;
  if (codec_type_ != VideoCodecType::kVideoCodecAV1) {
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_, CreateBuiltinVideoEncoderFactory(),
        CreateBuiltinVideoDecoderFactory());
  } else {
    // AV1 is injectible
    webrtc_provider = std::make_unique<WebRtcEncoderDecoderPicturePairProvider>(
        codec_type_,
        std::make_unique<
            VideoEncoderFactoryTemplate<LibaomAv1EncoderTemplateAdapter>>(),
        std::make_unique<
            VideoDecoderFactoryTemplate<Dav1dDecoderTemplateAdapter>>());
  }

  // An Y4M file with two frames.
  TempY4mFileCreator temp_y4m_file_creator(kDummyVideoWidth, kDummyVideoHeight,
                                           kFramerate);
  temp_y4m_file_creator.CreateTempY4mFile(kDummyFileContent);
  const absl::string_view y4m_filepath = temp_y4m_file_creator.y4m_filepath();
  const TestClip y4m_test_clip =
      TestClip::CreateY4mClip(y4m_filepath, kCodecMode);
  ASSERT_TRUE(webrtc_provider->Configure(y4m_test_clip, kDefaultBitrate));

  for (int i = 0; i < kNumFrames; ++i) {
    std::optional<OriginalCompressedPicturePair> picture_pair =
        webrtc_provider->GetNextPicturePair();

    EXPECT_NE(picture_pair, std::nullopt);
    EXPECT_GE(picture_pair->frame_average_qp, 0);
    std::cout << "Avg QP: " << picture_pair->frame_average_qp << std::endl;
    EXPECT_LE(picture_pair->frame_average_qp, GetMapQP());

    // Calculate the PSNR between the original and compressed image.
    double psnr = I420PSNR(
        *picture_pair->original_image.video_frame_buffer()->GetI420(),
        *picture_pair->compressed_image.video_frame_buffer()->GetI420());
    EXPECT_GE(psnr, kWatchablePsnrDb);
    std::cout << "PSNR: " << psnr << std::endl;

    // Test timestamp.
    EXPECT_EQ(picture_pair->original_image.rtp_timestamp(),
              kRtpTimestampInterval * i);
    EXPECT_EQ(picture_pair->compressed_image.rtp_timestamp(),
              kRtpTimestampInterval * i);
  }

  // We need to destruct the `webrtc_provider` because of it keeping the
  // read video file open. If we don't do this, we get an error when trying to
  // delete the file on Windows
  webrtc_provider.reset();
}

}  // namespace
}  // namespace webrtc
