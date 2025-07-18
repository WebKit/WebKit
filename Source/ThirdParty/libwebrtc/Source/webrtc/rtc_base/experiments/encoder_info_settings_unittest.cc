/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/encoder_info_settings.h"

#include "rtc_base/gunit.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"

namespace webrtc {

using test::ExplicitKeyValueConfig;

TEST(SimulcastEncoderAdapterSettingsTest, NoValuesWithoutFieldTrial) {
  ExplicitKeyValueConfig field_trials("");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_EQ(std::nullopt, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, NoValueForInvalidAlignment) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:0/");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_EQ(std::nullopt, settings.requested_resolution_alignment());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionAlignment) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:2/");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_EQ(2u, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetApplyAlignment) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:3,"
      "apply_alignment_to_all_simulcast_layers/");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_EQ(3u, settings.requested_resolution_alignment());
  EXPECT_TRUE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionBitrateLimits) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "frame_size_pixels:123,"
      "min_start_bitrate_bps:11000,"
      "min_bitrate_bps:44000,"
      "max_bitrate_bps:77000/");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_EQ(std::nullopt, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_THAT(settings.resolution_bitrate_limits(),
              ::testing::ElementsAre(VideoEncoder::ResolutionBitrateLimits{
                  123, 11000, 44000, 77000}));
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionBitrateLimitsWithList) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "frame_size_pixels:123|456|789,"
      "min_start_bitrate_bps:11000|22000|33000,"
      "min_bitrate_bps:44000|55000|66000,"
      "max_bitrate_bps:77000|88000|99000/");

  SimulcastEncoderAdapterEncoderInfoSettings settings(field_trials);
  EXPECT_THAT(
      settings.resolution_bitrate_limits(),
      ::testing::ElementsAre(
          VideoEncoder::ResolutionBitrateLimits{123, 11000, 44000, 77000},
          VideoEncoder::ResolutionBitrateLimits{456, 22000, 55000, 88000},
          VideoEncoder::ResolutionBitrateLimits{789, 33000, 66000, 99000}));
}

TEST(EncoderSettingsTest, CommonSettingsUsedIfEncoderNameUnspecified) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-VP8-GetEncoderInfoOverride/requested_resolution_alignment:2/"
      "WebRTC-GetEncoderInfoOverride/requested_resolution_alignment:3/");

  LibvpxVp8EncoderInfoSettings vp8_settings(field_trials);
  EXPECT_EQ(2u, vp8_settings.requested_resolution_alignment());
  LibvpxVp9EncoderInfoSettings vp9_settings(field_trials);
  EXPECT_EQ(3u, vp9_settings.requested_resolution_alignment());
}

TEST(GetSinglecastBitrateLimitForResolutionWhenQpIsUntrustedTests,
     LinearInterpolationUnderflow) {
  std::optional<int> frame_size_pixels = 480 * 360;
  std::vector<VideoEncoder::ResolutionBitrateLimits> resolution_bitrate_limits(
      {{1280 * 720, 1500000, 30000, 2500000},
       {1920 * 1080, 2500000, 30000, 4000000}});

  const auto resolutionBitrateLimit = EncoderInfoSettings::
      GetSinglecastBitrateLimitForResolutionWhenQpIsUntrusted(
          frame_size_pixels, resolution_bitrate_limits);
  EXPECT_TRUE(resolutionBitrateLimit.has_value());
  EXPECT_EQ(resolutionBitrateLimit.value(), resolution_bitrate_limits.front());
}

TEST(GetSinglecastBitrateLimitForResolutionWhenQpIsUntrustedTests,
     LinearInterpolationOverflow) {
  std::optional<int> frame_size_pixels = 4096 * 2160;
  std::vector<VideoEncoder::ResolutionBitrateLimits> resolution_bitrate_limits(
      {{1280 * 720, 1500000, 30000, 2500000},
       {1920 * 1080, 2500000, 30000, 4000000}});

  const auto resolutionBitrateLimit = EncoderInfoSettings::
      GetSinglecastBitrateLimitForResolutionWhenQpIsUntrusted(
          frame_size_pixels, resolution_bitrate_limits);
  EXPECT_TRUE(resolutionBitrateLimit.has_value());
  EXPECT_EQ(resolutionBitrateLimit.value(), resolution_bitrate_limits.back());
}

TEST(GetSinglecastBitrateLimitForResolutionWhenQpIsUntrustedTests,
     LinearInterpolationExactMatch) {
  std::optional<int> frame_size_pixels = 1920 * 1080;
  VideoEncoder::ResolutionBitrateLimits expected_match{1920 * 1080, 2500000,
                                                       30000, 4000000};
  std::vector<VideoEncoder::ResolutionBitrateLimits> resolution_bitrate_limits(
      {{1280 * 720, 1500000, 30000, 2500000},
       expected_match,
       {4096 * 2160, 4000000, 30000, 8000000}});

  const auto resolutionBitrateLimit = EncoderInfoSettings::
      GetSinglecastBitrateLimitForResolutionWhenQpIsUntrusted(
          frame_size_pixels, resolution_bitrate_limits);
  EXPECT_TRUE(resolutionBitrateLimit.has_value());
  EXPECT_EQ(resolutionBitrateLimit.value(), expected_match);
}

}  // namespace webrtc
