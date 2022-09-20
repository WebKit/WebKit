/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internal_encoder_factory.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/media_constants.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::Contains;
using ::testing::Field;
using ::testing::Not;

#ifdef RTC_ENABLE_VP9
constexpr bool kVp9Enabled = true;
#else
constexpr bool kVp9Enabled = false;
#endif
#ifdef WEBRTC_USE_H264
constexpr bool kH264Enabled = true;
#else
constexpr bool kH264Enabled = false;
#endif
constexpr VideoEncoderFactory::CodecSupport kSupported = {
    /*is_supported=*/true, /*is_power_efficient=*/false};
constexpr VideoEncoderFactory::CodecSupport kUnsupported = {
    /*is_supported=*/false, /*is_power_efficient=*/false};

MATCHER_P(Support, expected, "") {
  return arg.is_supported == expected.is_supported &&
         arg.is_power_efficient == expected.is_power_efficient;
}

TEST(InternalEncoderFactoryTest, Vp8) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(cricket::kVp8CodecName));
  EXPECT_TRUE(encoder);
}

TEST(InternalEncoderFactoryTest, Vp9Profile0) {
  InternalEncoderFactory factory;
  if (kVp9Enabled) {
    std::unique_ptr<VideoEncoder> encoder =
        factory.CreateVideoEncoder(SdpVideoFormat(
            cricket::kVp9CodecName,
            {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}}));
    EXPECT_TRUE(encoder);
  } else {
    EXPECT_THAT(
        factory.GetSupportedFormats(),
        Not(Contains(Field(&SdpVideoFormat::name, cricket::kVp9CodecName))));
  }
}

TEST(InternalEncoderFactoryTest, H264) {
  InternalEncoderFactory factory;
  if (kH264Enabled) {
    std::unique_ptr<VideoEncoder> encoder =
        factory.CreateVideoEncoder(SdpVideoFormat(cricket::kH264CodecName));
    EXPECT_TRUE(encoder);
  } else {
    EXPECT_THAT(
        factory.GetSupportedFormats(),
        Not(Contains(Field(&SdpVideoFormat::name, cricket::kH264CodecName))));
  }
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportWithScalabilityMode) {
  InternalEncoderFactory factory;
  // VP8 and VP9 supported for singles spatial layers.
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName), "L1T2"),
      Support(kSupported));
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName), "L1T3"),
      Support(kVp9Enabled ? kSupported : kUnsupported));

  // VP9 support for spatial layers.
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName), "L3T3"),
      Support(kVp9Enabled ? kSupported : kUnsupported));

  // Invalid scalability modes even though VP8 and H264 are supported.
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat(cricket::kH264CodecName),
                                        "L2T2"),
              Support(kUnsupported));
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName), "L3T3"),
      Support(kUnsupported));
}

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
TEST(InternalEncoderFactoryTest, Av1) {
  InternalEncoderFactory factory;
  EXPECT_THAT(factory.GetSupportedFormats(),
              Contains(Field(&SdpVideoFormat::name, cricket::kAv1CodecName)));
  EXPECT_TRUE(
      factory.CreateVideoEncoder(SdpVideoFormat(cricket::kAv1CodecName)));
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportNoScalabilityModeAv1) {
  InternalEncoderFactory factory;
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat(cricket::kAv1CodecName),
                                        /*scalability_mode=*/absl::nullopt),
              Support(kSupported));
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportNoScalabilityMode) {
  InternalEncoderFactory factory;
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName),
                                        /*scalability_mode=*/absl::nullopt),
              Support(kSupported));
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName),
                                        /*scalability_mode=*/absl::nullopt),
              Support(kVp9Enabled ? kSupported : kUnsupported));
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportWithScalabilityModeAv1) {
  InternalEncoderFactory factory;
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kAv1CodecName), "L2T1"),
      Support(kSupported));
}
#endif  // defined(RTC_USE_LIBAOM_AV1_ENCODER)

}  // namespace
}  // namespace webrtc
