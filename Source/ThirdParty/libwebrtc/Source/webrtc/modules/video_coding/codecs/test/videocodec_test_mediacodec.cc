/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <tuple>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/create_videocodec_test_fixture.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#include "modules/video_coding/codecs/test/videocodec_test_fixture_impl.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kForemanNumFrames = 300;
const int kForemanFramerateFps = 30;

VideoCodecTestFixture::Config CreateConfig() {
  VideoCodecTestFixture::Config config;
  config.filename = "foreman_cif";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kForemanNumFrames;
  // In order to not overwhelm the OpenMAX buffers in the Android MediaCodec.
  config.encode_in_real_time = true;
  return config;
}

std::unique_ptr<VideoCodecTestFixture> CreateTestFixtureWithConfig(
    VideoCodecTestFixture::Config config) {
  InitializeAndroidObjects();  // Idempotent.
  auto encoder_factory = CreateAndroidEncoderFactory();
  auto decoder_factory = CreateAndroidDecoderFactory();
  return CreateVideoCodecTestFixture(config, std::move(decoder_factory),
                                     std::move(encoder_factory));
}
}  // namespace

TEST(VideoCodecTestMediaCodec, ForemanCif500kbpsVp8) {
  auto config = CreateConfig();
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestMediaCodec, ForemanCif500kbpsH264CBP) {
  auto config = CreateConfig();
  const auto frame_checker =
      absl::make_unique<VideoCodecTestFixtureImpl::H264KeyframeChecker>();
  config.encoded_frame_checker = frame_checker.get();
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

// TODO(brandtr): Enable this test when we have trybots/buildbots with
// HW encoders that support CHP.
TEST(VideoCodecTestMediaCodec, DISABLED_ForemanCif500kbpsH264CHP) {
  auto config = CreateConfig();
  const auto frame_checker =
      absl::make_unique<VideoCodecTestFixtureImpl::H264KeyframeChecker>();

  config.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  config.encoded_frame_checker = frame_checker.get();
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {
      {500, kForemanFramerateFps, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestMediaCodec, ForemanMixedRes100kbpsVp8H264) {
  auto config = CreateConfig();
  const int kNumFrames = 30;
  const std::vector<std::string> codecs = {cricket::kVp8CodecName,
                                           cricket::kH264CodecName};
  const std::vector<std::tuple<int, int>> resolutions = {
      {128, 96}, {160, 120}, {176, 144}, {240, 136}, {320, 240}, {480, 272}};
  const std::vector<RateProfile> rate_profiles = {
      {100, kForemanFramerateFps, kNumFrames}};
  const std::vector<QualityThresholds> quality_thresholds = {
      {29, 26, 0.8, 0.75}};

  for (const auto& codec : codecs) {
    for (const auto& resolution : resolutions) {
      const int width = std::get<0>(resolution);
      const int height = std::get<1>(resolution);
      config.filename = std::string("foreman_") + std::to_string(width) + "x" +
                        std::to_string(height);
      config.filepath = ResourcePath(config.filename, "yuv");
      config.num_frames = kNumFrames;
      config.SetCodecSettings(codec, 1, 1, 1, false, false, false, width,
                              height);

      auto fixture = CreateTestFixtureWithConfig(config);
      fixture->RunTest(rate_profiles, nullptr /* rc_thresholds */,
                       &quality_thresholds, nullptr /* bs_thresholds */);
    }
  }
}

}  // namespace test
}  // namespace webrtc
