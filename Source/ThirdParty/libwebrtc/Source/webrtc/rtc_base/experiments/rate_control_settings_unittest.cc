/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rate_control_settings.h"

#include "api/video_codecs/video_codec.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

namespace {

using test::ExplicitKeyValueConfig;
using ::testing::DoubleEq;
using ::testing::Optional;

RateControlSettings ParseFrom(absl::string_view field_trials) {
  return RateControlSettings(ExplicitKeyValueConfig(field_trials));
}

TEST(RateControlSettingsTest, CongestionWindow) {
  EXPECT_TRUE(ParseFrom("").UseCongestionWindow());

  const RateControlSettings settings =
      ParseFrom("WebRTC-CongestionWindow/QueueSize:100/");
  EXPECT_TRUE(settings.UseCongestionWindow());
  EXPECT_EQ(settings.GetCongestionWindowAdditionalTimeMs(), 100);
}

TEST(RateControlSettingsTest, CongestionWindowPushback) {
  EXPECT_TRUE(ParseFrom("").UseCongestionWindowPushback());

  const RateControlSettings settings =
      ParseFrom("WebRTC-CongestionWindow/QueueSize:100,MinBitrate:100000/");
  EXPECT_TRUE(settings.UseCongestionWindowPushback());
  EXPECT_EQ(settings.CongestionWindowMinPushbackTargetBitrateBps(), 100000u);
}

TEST(RateControlSettingsTest, CongestionWindowPushbackDropframe) {
  EXPECT_TRUE(ParseFrom("").UseCongestionWindowPushback());

  const RateControlSettings settings = ParseFrom(
      "WebRTC-CongestionWindow/"
      "QueueSize:100,MinBitrate:100000,DropFrame:true/");
  EXPECT_TRUE(settings.UseCongestionWindowPushback());
  EXPECT_EQ(settings.CongestionWindowMinPushbackTargetBitrateBps(), 100000u);
  EXPECT_TRUE(settings.UseCongestionWindowDropFrameOnly());
}

TEST(RateControlSettingsTest, CongestionWindowPushbackDefaultConfig) {
  const RateControlSettings settings = ParseFrom("");
  EXPECT_TRUE(settings.UseCongestionWindowPushback());
  EXPECT_EQ(settings.CongestionWindowMinPushbackTargetBitrateBps(), 30000u);
  EXPECT_TRUE(settings.UseCongestionWindowDropFrameOnly());
}

TEST(RateControlSettingsTest, PacingFactor) {
  EXPECT_FALSE(ParseFrom("").GetPacingFactor());

  EXPECT_THAT(
      ParseFrom("WebRTC-VideoRateControl/pacing_factor:1.2/").GetPacingFactor(),
      Optional(DoubleEq(1.2)));
}

TEST(RateControlSettingsTest, AlrProbing) {
  EXPECT_FALSE(ParseFrom("").UseAlrProbing());

  EXPECT_TRUE(
      ParseFrom("WebRTC-VideoRateControl/alr_probing:1/").UseAlrProbing());
}

TEST(RateControlSettingsTest, LibvpxVp8QpMax) {
  EXPECT_FALSE(ParseFrom("").LibvpxVp8QpMax());

  EXPECT_EQ(
      ParseFrom("WebRTC-VideoRateControl/vp8_qp_max:50/").LibvpxVp8QpMax(), 50);
}

TEST(RateControlSettingsTest, DoesNotGetTooLargeLibvpxVp8QpMaxValue) {
  EXPECT_FALSE(
      ParseFrom("WebRTC-VideoRateControl/vp8_qp_max:70/").LibvpxVp8QpMax());
}

TEST(RateControlSettingsTest, LibvpxVp8MinPixels) {
  EXPECT_FALSE(ParseFrom("").LibvpxVp8MinPixels());

  EXPECT_EQ(ParseFrom("WebRTC-VideoRateControl/vp8_min_pixels:50000/")
                .LibvpxVp8MinPixels(),
            50000);
}

TEST(RateControlSettingsTest, DoesNotGetTooSmallLibvpxVp8MinPixelValue) {
  EXPECT_FALSE(ParseFrom("WebRTC-VideoRateControl/vp8_min_pixels:0/")
                   .LibvpxVp8MinPixels());
}

TEST(RateControlSettingsTest, LibvpxTrustedRateController) {
  const RateControlSettings default_settings = ParseFrom("");
  EXPECT_TRUE(default_settings.LibvpxVp8TrustedRateController());
  EXPECT_TRUE(default_settings.LibvpxVp9TrustedRateController());

  const RateControlSettings settings =
      ParseFrom("WebRTC-VideoRateControl/trust_vp8:0,trust_vp9:0/");
  EXPECT_FALSE(settings.LibvpxVp8TrustedRateController());
  EXPECT_FALSE(settings.LibvpxVp9TrustedRateController());
}

TEST(RateControlSettingsTest, Vp8BaseHeavyTl3RateAllocationLegacyKey) {
  EXPECT_FALSE(ParseFrom("").Vp8BaseHeavyTl3RateAllocation());

  EXPECT_TRUE(ParseFrom("WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/")
                  .Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest,
     Vp8BaseHeavyTl3RateAllocationVideoRateControlKey) {
  EXPECT_FALSE(ParseFrom("").Vp8BaseHeavyTl3RateAllocation());

  EXPECT_TRUE(ParseFrom("WebRTC-VideoRateControl/vp8_base_heavy_tl3_alloc:1/")
                  .Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest,
     Vp8BaseHeavyTl3RateAllocationVideoRateControlKeyOverridesLegacyKey) {
  EXPECT_FALSE(ParseFrom("").Vp8BaseHeavyTl3RateAllocation());

  EXPECT_FALSE(ParseFrom("WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/"
                         "WebRTC-VideoRateControl/vp8_base_heavy_tl3_alloc:0/")
                   .Vp8BaseHeavyTl3RateAllocation());
}

TEST(RateControlSettingsTest, UseEncoderBitrateAdjuster) {
  EXPECT_TRUE(ParseFrom("").UseEncoderBitrateAdjuster());

  EXPECT_FALSE(ParseFrom("WebRTC-VideoRateControl/bitrate_adjuster:false/")
                   .UseEncoderBitrateAdjuster());
}

}  // namespace

}  // namespace webrtc
