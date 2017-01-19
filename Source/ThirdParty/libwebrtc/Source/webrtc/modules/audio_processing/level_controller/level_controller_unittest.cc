/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/level_controller/level_controller.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

// Processes a specified amount of frames, verifies the results and reports
// any errors.
void RunBitexactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         rtc::Optional<float> initial_peak_level_dbfs,
                         rtc::ArrayView<const float> output_reference) {
  LevelController level_controller;
  level_controller.Initialize(sample_rate_hz);
  if (initial_peak_level_dbfs) {
    AudioProcessing::Config::LevelController config;
    config.initial_peak_level_dbfs = *initial_peak_level_dbfs;
    level_controller.ApplyConfig(config);
  }

  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig capture_config(sample_rate_hz, num_channels, false);
  AudioBuffer capture_buffer(
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);
  for (size_t frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    level_controller.Process(&capture_buffer);
  }

  // Extract test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kVectorElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kVectorElementErrorBound));
}

}  // namespace

TEST(LevelControllerConfig, ToString) {
  AudioProcessing::Config config;
  config.level_controller.enabled = true;
  config.level_controller.initial_peak_level_dbfs = -6.0206f;
  EXPECT_EQ("{enabled: true, initial_peak_level_dbfs: -6.0206}",
            LevelController::ToString(config.level_controller));

  config.level_controller.enabled = false;
  config.level_controller.initial_peak_level_dbfs = -50.f;
  EXPECT_EQ("{enabled: false, initial_peak_level_dbfs: -50}",
            LevelController::ToString(config.level_controller));
}

TEST(LevelControlBitExactnessTest, DISABLED_Mono8kHz) {
  const float kOutputReference[] = {-0.013939f, -0.012154f, -0.009054f};
  RunBitexactnessTest(AudioProcessing::kSampleRate8kHz, 1,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Mono16kHz) {
  const float kOutputReference[] = {-0.013706f, -0.013215f, -0.013018f};
  RunBitexactnessTest(AudioProcessing::kSampleRate16kHz, 1,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Mono32kHz) {
  const float kOutputReference[] = {-0.014495f, -0.016425f, -0.016085f};
  RunBitexactnessTest(AudioProcessing::kSampleRate32kHz, 1,
                      rtc::Optional<float>(), kOutputReference);
}

// TODO(peah): Investigate why this particular testcase differ between Android
// and the rest of the platforms.
TEST(LevelControlBitExactnessTest, DISABLED_Mono48kHz) {
#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
  const float kOutputReference[] = {-0.014277f, -0.015180f, -0.017437f};
#else
  const float kOutputReference[] = {-0.015949f, -0.016957f, -0.019478f};
#endif
  RunBitexactnessTest(AudioProcessing::kSampleRate48kHz, 1,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo8kHz) {
  const float kOutputReference[] = {-0.014063f, -0.008450f, -0.012159f,
                                    -0.051967f, -0.023202f, -0.047858f};
  RunBitexactnessTest(AudioProcessing::kSampleRate8kHz, 2,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo16kHz) {
  const float kOutputReference[] = {-0.012714f, -0.005896f, -0.012220f,
                                    -0.053306f, -0.024549f, -0.051527f};
  RunBitexactnessTest(AudioProcessing::kSampleRate16kHz, 2,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo32kHz) {
  const float kOutputReference[] = {-0.011737f, -0.007018f, -0.013446f,
                                    -0.053505f, -0.026292f, -0.056221f};
  RunBitexactnessTest(AudioProcessing::kSampleRate32kHz, 2,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_Stereo48kHz) {
  const float kOutputReference[] = {-0.010643f, -0.006334f, -0.011377f,
                                    -0.049088f, -0.023600f, -0.050465f};
  RunBitexactnessTest(AudioProcessing::kSampleRate48kHz, 2,
                      rtc::Optional<float>(), kOutputReference);
}

TEST(LevelControlBitExactnessTest, DISABLED_MonoInitial48kHz) {
  const float kOutputReference[] = {-0.013753f, -0.014623f, -0.016797f};
  RunBitexactnessTest(AudioProcessing::kSampleRate48kHz, 1,
                      rtc::Optional<float>(-50), kOutputReference);
}

}  // namespace webrtc
