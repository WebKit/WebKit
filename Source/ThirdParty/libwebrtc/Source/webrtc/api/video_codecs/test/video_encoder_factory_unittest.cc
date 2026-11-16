/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp9_profile.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr VideoEncoderFactory::CodecSupport kSupported = {
    .is_supported = true,
    .is_power_efficient = false};
constexpr VideoEncoderFactory::CodecSupport kUnsupported = {
    .is_supported = false,
    .is_power_efficient = false};

MATCHER_P(SupportIs, expected, "") {
  return arg.is_supported == expected.is_supported &&
         arg.is_power_efficient == expected.is_power_efficient;
}

class TestVideoEncoderFactory : public VideoEncoderFactory {
 public:
  explicit TestVideoEncoderFactory(std::vector<SdpVideoFormat> formats)
      : formats_(std::move(formats)) {}

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return formats_;
  }

  std::unique_ptr<VideoEncoder> Create(const Environment& env,
                                       const SdpVideoFormat& format) override {
    return nullptr;
  }

 private:
  std::vector<SdpVideoFormat> formats_;
};

TEST(VideoEncoderFactoryTest, QueryCodecSupportNoScalabilityMode) {
  TestVideoEncoderFactory factory({SdpVideoFormat("VP8")});
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat("VP8"), std::nullopt,
                                        std::nullopt),
              SupportIs(kSupported));
}

TEST(VideoEncoderFactoryTest, QueryCodecSupportUnsupportedFormat) {
  TestVideoEncoderFactory factory({SdpVideoFormat("VP8")});
  EXPECT_THAT(factory.QueryCodecSupport(SdpVideoFormat("H264"), std::nullopt,
                                        std::nullopt),
              SupportIs(kUnsupported));
}

TEST(VideoEncoderFactoryTest,
     QueryCodecSupportWithScalabilityModeAndEmptyModeList) {
  TestVideoEncoderFactory factory({SdpVideoFormat("VP8")});
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat("VP8"), "L1T2", std::nullopt),
      SupportIs(kUnsupported));
}

TEST(VideoEncoderFactoryTest, QueryCodecSupportWithMatchingScalabilityMode) {
  SdpVideoFormat format("VP8", {}, {ScalabilityMode::kL1T2});
  TestVideoEncoderFactory factory({format});
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat("VP8"), "L1T2", std::nullopt),
      SupportIs(kSupported));
}

TEST(VideoEncoderFactoryTest, QueryCodecSupportWithNonMatchingScalabilityMode) {
  SdpVideoFormat format("VP8", {}, {ScalabilityMode::kL1T2});
  TestVideoEncoderFactory factory({format});
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat("VP8"), "L3T3", std::nullopt),
      SupportIs(kUnsupported));
}

TEST(VideoEncoderFactoryTest, QueryCodecSupportWithInvalidScalabilityMode) {
  SdpVideoFormat format("VP8", {}, {ScalabilityMode::kL1T2});
  TestVideoEncoderFactory factory({format});
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat("VP8"), "INVALID", std::nullopt),
      SupportIs(kUnsupported));
}

TEST(VideoEncoderFactoryTest,
     QueryCodecSupportScalabilityModeUnsupportedFormat) {
  SdpVideoFormat format("VP8", {}, {ScalabilityMode::kL1T2});
  TestVideoEncoderFactory factory({format});
  EXPECT_THAT(
      factory.QueryCodecSupport(SdpVideoFormat("H264"), "L1T2", std::nullopt),
      SupportIs(kUnsupported));
}

TEST(VideoEncoderFactoryTest,
     QueryCodecSupportDistinguishesDifferentCodecParameters) {
  SdpVideoFormat vp9_profile0("VP9", {{kVP9FmtpProfileId, "0"}},
                              {ScalabilityMode::kL1T2, ScalabilityMode::kL3T3});
  SdpVideoFormat vp9_profile2("VP9", {{kVP9FmtpProfileId, "2"}},
                              {ScalabilityMode::kL1T2});
  TestVideoEncoderFactory factory({vp9_profile0, vp9_profile2});

  EXPECT_THAT(factory.QueryCodecSupport(
                  SdpVideoFormat("VP9", {{kVP9FmtpProfileId, "0"}}), "L3T3",
                  std::nullopt),
              SupportIs(kSupported));
  EXPECT_THAT(factory.QueryCodecSupport(
                  SdpVideoFormat("VP9", {{kVP9FmtpProfileId, "2"}}), "L3T3",
                  std::nullopt),
              SupportIs(kUnsupported));
}

}  // namespace
}  // namespace webrtc
