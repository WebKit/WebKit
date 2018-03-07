/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "test/field_trial.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_IOS)

namespace {
const int kForemanNumFrames = 300;
const std::nullptr_t kNoVisualizationParams = nullptr;
}  // namespace

class VideoProcessorIntegrationTestVideoToolbox
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestVideoToolbox() {
    config_.filename = "foreman_cif";
    config_.input_filename = ResourcePath(config_.filename, "yuv");
    config_.output_filename = TempFilename(
        OutputPath(), "videoprocessor_integrationtest_videotoolbox");
    config_.num_frames = kForemanNumFrames;
    config_.hw_encoder = true;
    config_.hw_decoder = true;
    config_.encoded_frame_checker = &h264_keyframe_checker_;
  }
};

// Since we don't currently run the iOS tests on physical devices on the bots,
// the tests are disabled.

TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       DISABLED_ForemanCif500kbpsH264CBP) {
  config_.SetCodecSettings(kVideoCodecH264, 1, false, false, false, false,
                           false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {20, 95, 60, 60, 10, 0, 1}};

  QualityThresholds quality_thresholds(30.0, 14.0, 0.86, 0.39);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       DISABLED_ForemanCif500kbpsH264CHP) {
  ScopedFieldTrials override_field_trials("WebRTC-H264HighProfile/Enabled/");

  config_.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  config_.SetCodecSettings(kVideoCodecH264, 1, false, false, false, false,
                           false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {{5, 75, 65, 60, 6, 0, 1}};

  QualityThresholds quality_thresholds(30.0, 14.0, 0.86, 0.39);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

#endif  // defined(WEBRTC_IOS)

}  // namespace test
}  // namespace webrtc
