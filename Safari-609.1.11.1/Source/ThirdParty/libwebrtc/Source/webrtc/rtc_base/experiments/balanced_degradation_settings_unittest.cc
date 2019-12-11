/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/balanced_degradation_settings.h"

#include <limits>

#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

void VerifyIsDefault(
    const std::vector<BalancedDegradationSettings::Config>& config) {
  EXPECT_THAT(config, ::testing::ElementsAre(
                          BalancedDegradationSettings::Config{
                              320 * 240,
                              7,
                              0,
                              0,
                              BalancedDegradationSettings::kNoFpsDiff,
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0}},
                          BalancedDegradationSettings::Config{
                              480 * 270,
                              10,
                              0,
                              0,
                              BalancedDegradationSettings::kNoFpsDiff,
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0}},
                          BalancedDegradationSettings::Config{
                              640 * 480,
                              15,
                              0,
                              0,
                              BalancedDegradationSettings::kNoFpsDiff,
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0},
                              {0, 0, 0}}));
}
}  // namespace

TEST(BalancedDegradationSettings, GetsDefaultConfigIfNoList) {
  webrtc::test::ScopedFieldTrials field_trials("");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
  EXPECT_FALSE(settings.NextHigherBitrateKbps(1));
  EXPECT_FALSE(settings.ResolutionNextHigherBitrateKbps(1));
  EXPECT_TRUE(settings.CanAdaptUp(1, /*bitrate_bps*/ 1));
  EXPECT_TRUE(settings.CanAdaptUpResolution(1, /*bitrate_bps*/ 1));
  EXPECT_FALSE(settings.MinFpsDiff(1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP9, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecGeneric, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecMultiplex, 1));
}

TEST(BalancedDegradationSettings, GetsConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,other:4|5|6/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      11,
                      5,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}},
                  BalancedDegradationSettings::Config{
                      22,
                      15,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}},
                  BalancedDegradationSettings::Config{
                      33,
                      25,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroFpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:0|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfPixelsDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|999|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfFramerateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|4|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsConfigWithSpecificFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_fps:7|8|9,vp9_fps:9|10|11,"
      "h264_fps:11|12|13,generic_fps:13|14|15/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      1000,
                      5,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 7},
                      {0, 0, 9},
                      {0, 0, 11},
                      {0, 0, 13}},
                  BalancedDegradationSettings::Config{
                      2000,
                      15,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 8},
                      {0, 0, 10},
                      {0, 0, 12},
                      {0, 0, 14}},
                  BalancedDegradationSettings::Config{
                      3000,
                      25,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 9},
                      {0, 0, 11},
                      {0, 0, 13},
                      {0, 0, 15}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroVp8FpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:7|15|25,vp8_fps:0|15|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForInvalidFpsValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:7|15|25,vp8_fps:10|15|2000/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfVp8FramerateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:4|5|25,vp8_fps:5|4|25/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsMinFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(5, settings.MinFps(kVideoCodecVP8, 1));
  EXPECT_EQ(5, settings.MinFps(kVideoCodecVP8, 1000));
  EXPECT_EQ(15, settings.MinFps(kVideoCodecVP8, 1001));
  EXPECT_EQ(15, settings.MinFps(kVideoCodecVP8, 2000));
  EXPECT_EQ(25, settings.MinFps(kVideoCodecVP8, 2001));
  EXPECT_EQ(25, settings.MinFps(kVideoCodecVP8, 3000));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            settings.MinFps(kVideoCodecVP8, 3001));
}

TEST(BalancedDegradationSettings, GetsVp8MinFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_fps:7|10|12/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(7, settings.MinFps(kVideoCodecVP8, 1));
  EXPECT_EQ(7, settings.MinFps(kVideoCodecVP8, 1000));
  EXPECT_EQ(10, settings.MinFps(kVideoCodecVP8, 1001));
  EXPECT_EQ(10, settings.MinFps(kVideoCodecVP8, 2000));
  EXPECT_EQ(12, settings.MinFps(kVideoCodecVP8, 2001));
  EXPECT_EQ(12, settings.MinFps(kVideoCodecVP8, 3000));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            settings.MinFps(kVideoCodecVP8, 3001));
}

TEST(BalancedDegradationSettings, GetsMaxFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(15, settings.MaxFps(kVideoCodecVP8, 1));
  EXPECT_EQ(15, settings.MaxFps(kVideoCodecVP8, 1000));
  EXPECT_EQ(25, settings.MaxFps(kVideoCodecVP8, 1001));
  EXPECT_EQ(25, settings.MaxFps(kVideoCodecVP8, 2000));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            settings.MaxFps(kVideoCodecVP8, 2001));
}

TEST(BalancedDegradationSettings, GetsVp8MaxFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_fps:7|10|12/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(10, settings.MaxFps(kVideoCodecVP8, 1));
  EXPECT_EQ(10, settings.MaxFps(kVideoCodecVP8, 1000));
  EXPECT_EQ(12, settings.MaxFps(kVideoCodecVP8, 1001));
  EXPECT_EQ(12, settings.MaxFps(kVideoCodecVP8, 2000));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            settings.MaxFps(kVideoCodecVP8, 2001));
}

TEST(BalancedDegradationSettings, GetsVp9Fps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp9_fps:7|10|12/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(7, settings.MinFps(kVideoCodecVP9, 1000));
  EXPECT_EQ(10, settings.MaxFps(kVideoCodecVP9, 1000));
}

TEST(BalancedDegradationSettings, GetsH264Fps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,h264_fps:8|11|13/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(11, settings.MinFps(kVideoCodecH264, 2000));
  EXPECT_EQ(13, settings.MaxFps(kVideoCodecH264, 2000));
}

TEST(BalancedDegradationSettings, GetsGenericFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,generic_fps:9|12|14/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(14, settings.MinFps(kVideoCodecGeneric, 3000));
  EXPECT_EQ(std::numeric_limits<int>::max(),
            settings.MaxFps(kVideoCodecGeneric, 3000));
}

TEST(BalancedDegradationSettings, GetsUnlimitedForMaxValidFps) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|100,vp8_fps:30|100|100/");
  const int kUnlimitedFps = std::numeric_limits<int>::max();
  BalancedDegradationSettings settings;
  EXPECT_EQ(15, settings.MinFps(kVideoCodecH264, 2000));
  EXPECT_EQ(kUnlimitedFps, settings.MinFps(kVideoCodecH264, 2001));
  EXPECT_EQ(30, settings.MinFps(kVideoCodecVP8, 1000));
  EXPECT_EQ(kUnlimitedFps, settings.MinFps(kVideoCodecVP8, 1001));
}

TEST(BalancedDegradationSettings, GetsConfigWithBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,kbps:44|88|99,kbps_res:55|111|222/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      11,
                      5,
                      44,
                      55,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}},
                  BalancedDegradationSettings::Config{
                      22,
                      15,
                      88,
                      111,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}},
                  BalancedDegradationSettings::Config{
                      33,
                      25,
                      99,
                      222,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0},
                      {0, 0, 0}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfBitrateDecreases) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,kbps:44|43|99/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings,
     GetsDefaultConfigIfBitrateDecreasesWithUnsetValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:11|22|33,fps:5|15|25,kbps:44|0|43/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsNextHigherBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,kbps:44|88|99/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(88, settings.NextHigherBitrateKbps(1));
  EXPECT_EQ(88, settings.NextHigherBitrateKbps(1000));
  EXPECT_EQ(99, settings.NextHigherBitrateKbps(1001));
  EXPECT_EQ(99, settings.NextHigherBitrateKbps(2000));
  EXPECT_FALSE(settings.NextHigherBitrateKbps(2001));
}

TEST(BalancedDegradationSettings, GetsNextHigherBitrateWithUnsetValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,kbps:10|0|20/");
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.NextHigherBitrateKbps(1));
  EXPECT_FALSE(settings.NextHigherBitrateKbps(1000));
  EXPECT_EQ(20, settings.NextHigherBitrateKbps(1001));
  EXPECT_EQ(20, settings.NextHigherBitrateKbps(2000));
  EXPECT_FALSE(settings.NextHigherBitrateKbps(2001));
}

TEST(BalancedDegradationSettings, CanAdaptUpIfBitrateGeNextHigherKbpsLimit) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000|4000,fps:5|15|25|30,kbps:0|80|0|90/");
  BalancedDegradationSettings settings;
  EXPECT_TRUE(settings.CanAdaptUp(1000, 0));  // No bitrate provided.
  EXPECT_FALSE(settings.CanAdaptUp(1000, 79000));
  EXPECT_TRUE(settings.CanAdaptUp(1000, 80000));
  EXPECT_TRUE(settings.CanAdaptUp(1001, 1));  // No limit configured.
  EXPECT_FALSE(settings.CanAdaptUp(3000, 89000));
  EXPECT_TRUE(settings.CanAdaptUp(3000, 90000));
  EXPECT_TRUE(settings.CanAdaptUp(3001, 1));  // No limit.
}

TEST(BalancedDegradationSettings, GetsResolutionNextHigherBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,kbps_res:44|88|99/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(88, settings.ResolutionNextHigherBitrateKbps(1));
  EXPECT_EQ(88, settings.ResolutionNextHigherBitrateKbps(1000));
  EXPECT_EQ(99, settings.ResolutionNextHigherBitrateKbps(1001));
  EXPECT_EQ(99, settings.ResolutionNextHigherBitrateKbps(2000));
  EXPECT_FALSE(settings.ResolutionNextHigherBitrateKbps(2001));
}

TEST(BalancedDegradationSettings,
     GetsResolutionNextHigherBitrateWithUnsetValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,kbps_res:10|0|20/");
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.ResolutionNextHigherBitrateKbps(1));
  EXPECT_FALSE(settings.ResolutionNextHigherBitrateKbps(1000));
  EXPECT_EQ(20, settings.ResolutionNextHigherBitrateKbps(1001));
  EXPECT_EQ(20, settings.ResolutionNextHigherBitrateKbps(2000));
  EXPECT_FALSE(settings.ResolutionNextHigherBitrateKbps(2001));
}

TEST(BalancedDegradationSettings,
     CanAdaptUpResolutionIfBitrateGeNextHigherKbpsLimit) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000|4000,fps:5|15|25|30,kbps_res:0|80|0|90/");
  BalancedDegradationSettings settings;
  EXPECT_TRUE(settings.CanAdaptUpResolution(1000, 0));  // No bitrate provided.
  EXPECT_FALSE(settings.CanAdaptUpResolution(1000, 79000));
  EXPECT_TRUE(settings.CanAdaptUpResolution(1000, 80000));
  EXPECT_TRUE(settings.CanAdaptUpResolution(1001, 1));  // No limit configured.
  EXPECT_FALSE(settings.CanAdaptUpResolution(3000, 89000));
  EXPECT_TRUE(settings.CanAdaptUpResolution(3000, 90000));
  EXPECT_TRUE(settings.CanAdaptUpResolution(3001, 1));  // No limit.
}

TEST(BalancedDegradationSettings, GetsFpsDiff) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,fps_diff:0|-2|3/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(0, settings.MinFpsDiff(1));
  EXPECT_EQ(0, settings.MinFpsDiff(1000));
  EXPECT_EQ(-2, settings.MinFpsDiff(1001));
  EXPECT_EQ(-2, settings.MinFpsDiff(2000));
  EXPECT_EQ(3, settings.MinFpsDiff(2001));
  EXPECT_EQ(3, settings.MinFpsDiff(3000));
  EXPECT_FALSE(settings.MinFpsDiff(3001));
}

TEST(BalancedDegradationSettings, GetsNoFpsDiffIfValueBelowMinSetting) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,fps_diff:-100|-99|-101/");
  // Min valid fps_diff setting: -99.
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.MinFpsDiff(1000));
  EXPECT_EQ(-99, settings.MinFpsDiff(2000));
  EXPECT_FALSE(settings.MinFpsDiff(3000));
}

TEST(BalancedDegradationSettings, QpThresholdsNotSetByDefault) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25/");
  BalancedDegradationSettings settings;
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP8, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecVP9, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecH264, 1));
  EXPECT_FALSE(settings.GetQpThresholds(kVideoCodecGeneric, 1));
}

TEST(BalancedDegradationSettings, GetsConfigWithQpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:89|90|88,"
      "vp8_qp_high:90|91|92,vp9_qp_low:27|28|29,vp9_qp_high:120|130|140,"
      "h264_qp_low:12|13|14,h264_qp_high:20|30|40,generic_qp_low:7|6|5,"
      "generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  EXPECT_THAT(settings.GetConfigs(),
              ::testing::ElementsAre(
                  BalancedDegradationSettings::Config{
                      1000,
                      5,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {89, 90, 0},
                      {27, 120, 0},
                      {12, 20, 0},
                      {7, 22, 0}},
                  BalancedDegradationSettings::Config{
                      2000,
                      15,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {90, 91, 0},
                      {28, 130, 0},
                      {13, 30, 0},
                      {6, 23, 0}},
                  BalancedDegradationSettings::Config{
                      3000,
                      25,
                      0,
                      0,
                      BalancedDegradationSettings::kNoFpsDiff,
                      {88, 92, 0},
                      {29, 140, 0},
                      {14, 40, 0},
                      {5, 24, 0}}));
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfOnlyHasLowThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_low:89|90|88/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfOnlyHasHighThreshold) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfLowEqualsHigh) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|88/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigIfLowGreaterThanHigh) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|87/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsDefaultConfigForZeroQpValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|0|88,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  VerifyIsDefault(settings.GetConfigs());
}

TEST(BalancedDegradationSettings, GetsVp8QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp8_qp_low:89|90|88,vp8_qp_high:90|91|92/");
  BalancedDegradationSettings settings;
  EXPECT_EQ(89, settings.GetQpThresholds(kVideoCodecVP8, 1)->low);
  EXPECT_EQ(90, settings.GetQpThresholds(kVideoCodecVP8, 1)->high);
  EXPECT_EQ(90, settings.GetQpThresholds(kVideoCodecVP8, 1000)->high);
  EXPECT_EQ(91, settings.GetQpThresholds(kVideoCodecVP8, 1001)->high);
  EXPECT_EQ(91, settings.GetQpThresholds(kVideoCodecVP8, 2000)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 2001)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 3000)->high);
  EXPECT_EQ(92, settings.GetQpThresholds(kVideoCodecVP8, 3001)->high);
}

TEST(BalancedDegradationSettings, GetsVp9QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "vp9_qp_low:55|56|57,vp9_qp_high:155|156|157/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecVP9, 1000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(55, thresholds->low);
  EXPECT_EQ(155, thresholds->high);
}

TEST(BalancedDegradationSettings, GetsH264QpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "h264_qp_low:21|22|23,h264_qp_high:41|43|42/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecH264, 2000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(22, thresholds->low);
  EXPECT_EQ(43, thresholds->high);
}

TEST(BalancedDegradationSettings, GetsGenericQpThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:1000|2000|3000,fps:5|15|25,"
      "generic_qp_low:2|3|4,generic_qp_high:22|23|24/");
  BalancedDegradationSettings settings;
  const auto thresholds = settings.GetQpThresholds(kVideoCodecGeneric, 3000);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(4, thresholds->low);
  EXPECT_EQ(24, thresholds->high);
}

}  // namespace webrtc
