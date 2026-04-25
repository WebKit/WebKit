/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <vector>

#include "api/test/create_videocodec_test_fixture.h"
#include "api/test/videocodec_test_fixture.h"
#include "api/video_codecs/scalability_mode.h"
#include "media/base/media_constants.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {
// Test clips settings.
constexpr int kCifWidth = 352;
constexpr int kCifHeight = 288;
constexpr int kNumFramesLong = 300;

VideoCodecTestFixture::Config CreateConfig(std::string filename) {
  VideoCodecTestFixture::Config config;
  config.filename = filename;
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.use_single_core = true;
  return config;
}

TEST(VideoCodecTestAv1, HighBitrate) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(kAv1CodecName, 1, 1, 1, false, true, true, kCifWidth,
                          kCifHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {
      {.target_kbps = 500, .input_fps = 30, .frame_num = 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {.max_avg_bitrate_mismatch_percent = 12,
       .max_time_to_reach_target_bitrate_sec = 1,
       .max_avg_framerate_mismatch_percent = 0,
       .max_avg_buffer_level_sec = 1,
       .max_max_key_frame_delay_sec = 0.3,
       .max_max_delta_frame_delay_sec = 0.1,
       .max_num_spatial_resizes = 0,
       .max_num_key_frames = 1}};

  std::vector<QualityThresholds> quality_thresholds = {{.min_avg_psnr = 37,
                                                        .min_min_psnr = 34,
                                                        .min_avg_ssim = 0.94,
                                                        .min_min_ssim = 0.91}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestAv1, VeryLowBitrate) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(kAv1CodecName, 1, 1, 1, false, true, true, kCifWidth,
                          kCifHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {
      {.target_kbps = 50, .input_fps = 30, .frame_num = 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {.max_avg_bitrate_mismatch_percent = 15,
       .max_time_to_reach_target_bitrate_sec = 8,
       .max_avg_framerate_mismatch_percent = 75,
       .max_avg_buffer_level_sec = 2,
       .max_max_key_frame_delay_sec = 2,
       .max_max_delta_frame_delay_sec = 2,
       .max_num_spatial_resizes = 2,
       .max_num_key_frames = 1}};

  std::vector<QualityThresholds> quality_thresholds = {{.min_avg_psnr = 28,
                                                        .min_min_psnr = 24.8,
                                                        .min_avg_ssim = 0.70,
                                                        .min_min_ssim = 0.55}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

#if !defined(WEBRTC_ANDROID)
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
TEST(VideoCodecTestAv1, Hd) {
  auto config = CreateConfig("ConferenceMotion_1280_720_50");
  config.SetCodecSettings(kAv1CodecName, 1, 1, 1, false, true, true, kHdWidth,
                          kHdHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {
      {.target_kbps = 1000, .input_fps = 50, .frame_num = 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {.max_avg_bitrate_mismatch_percent = 13,
       .max_time_to_reach_target_bitrate_sec = 3,
       .max_avg_framerate_mismatch_percent = 0,
       .max_avg_buffer_level_sec = 1,
       .max_max_key_frame_delay_sec = 0.3,
       .max_max_delta_frame_delay_sec = 0.1,
       .max_num_spatial_resizes = 0,
       .max_num_key_frames = 1}};

  std::vector<QualityThresholds> quality_thresholds = {{.min_avg_psnr = 35.9,
                                                        .min_min_psnr = 31.5,
                                                        .min_avg_ssim = 0.925,
                                                        .min_min_ssim = 0.865}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}
#endif

}  // namespace
}  // namespace test
}  // namespace webrtc
