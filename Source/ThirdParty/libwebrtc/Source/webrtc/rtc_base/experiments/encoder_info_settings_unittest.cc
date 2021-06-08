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
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {

TEST(SimulcastEncoderAdapterSettingsTest, NoValuesWithoutFieldTrial) {
  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_EQ(absl::nullopt, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, NoValueForInvalidAlignment) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:0/");

  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_EQ(absl::nullopt, settings.requested_resolution_alignment());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionAlignment) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:2/");

  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_EQ(2, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetApplyAlignment) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "requested_resolution_alignment:3,"
      "apply_alignment_to_all_simulcast_layers/");

  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_EQ(3, settings.requested_resolution_alignment());
  EXPECT_TRUE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_TRUE(settings.resolution_bitrate_limits().empty());
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionBitrateLimits) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "frame_size_pixels:123,"
      "min_start_bitrate_bps:11000,"
      "min_bitrate_bps:44000,"
      "max_bitrate_bps:77000/");

  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_EQ(absl::nullopt, settings.requested_resolution_alignment());
  EXPECT_FALSE(settings.apply_alignment_to_all_simulcast_layers());
  EXPECT_THAT(settings.resolution_bitrate_limits(),
              ::testing::ElementsAre(VideoEncoder::ResolutionBitrateLimits{
                  123, 11000, 44000, 77000}));
}

TEST(SimulcastEncoderAdapterSettingsTest, GetResolutionBitrateLimitsWithList) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride/"
      "frame_size_pixels:123|456|789,"
      "min_start_bitrate_bps:11000|22000|33000,"
      "min_bitrate_bps:44000|55000|66000,"
      "max_bitrate_bps:77000|88000|99000/");

  SimulcastEncoderAdapterEncoderInfoSettings settings;
  EXPECT_THAT(
      settings.resolution_bitrate_limits(),
      ::testing::ElementsAre(
          VideoEncoder::ResolutionBitrateLimits{123, 11000, 44000, 77000},
          VideoEncoder::ResolutionBitrateLimits{456, 22000, 55000, 88000},
          VideoEncoder::ResolutionBitrateLimits{789, 33000, 66000, 99000}));
}

TEST(EncoderSettingsTest, CommonSettingsUsedIfEncoderNameUnspecified) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-GetEncoderInfoOverride/requested_resolution_alignment:2/"
      "WebRTC-GetEncoderInfoOverride/requested_resolution_alignment:3/");

  LibvpxVp8EncoderInfoSettings vp8_settings;
  EXPECT_EQ(2, vp8_settings.requested_resolution_alignment());
  LibvpxVp9EncoderInfoSettings vp9_settings;
  EXPECT_EQ(3, vp9_settings.requested_resolution_alignment());
}

}  // namespace webrtc
