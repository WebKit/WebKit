/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/agc/gain_control.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_processing.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kNumFramesToProcess = 100;

void ProcessOneFrame(int sample_rate_hz,
                     AudioBuffer* render_audio_buffer,
                     AudioBuffer* capture_audio_buffer,
                     GainControlImpl* gain_controller) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    render_audio_buffer->SplitIntoFrequencyBands();
    capture_audio_buffer->SplitIntoFrequencyBands();
  }

  std::vector<int16_t> render_audio;
  GainControlImpl::PackRenderAudioBuffer(*render_audio_buffer, &render_audio);
  gain_controller->ProcessRenderAudio(render_audio);
  gain_controller->AnalyzeCaptureAudio(*capture_audio_buffer);
  gain_controller->ProcessCaptureAudio(capture_audio_buffer, false);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_audio_buffer->MergeFrequencyBands();
  }
}

void SetupComponent(int sample_rate_hz,
                    GainControl::Mode mode,
                    int target_level_dbfs,
                    int stream_analog_level,
                    int compression_gain_db,
                    bool enable_limiter,
                    int analog_level_min,
                    int analog_level_max,
                    GainControlImpl* gain_controller) {
  gain_controller->Initialize(1, sample_rate_hz);
  GainControl* gc = static_cast<GainControl*>(gain_controller);
  gc->set_mode(mode);
  gc->set_stream_analog_level(stream_analog_level);
  gc->set_target_level_dbfs(target_level_dbfs);
  gc->set_compression_gain_db(compression_gain_db);
  gc->enable_limiter(enable_limiter);
  gc->set_analog_level_limits(analog_level_min, analog_level_max);
}

void RunBitExactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         GainControl::Mode mode,
                         int target_level_dbfs,
                         int stream_analog_level,
                         int compression_gain_db,
                         bool enable_limiter,
                         int analog_level_min,
                         int analog_level_max,
                         int achieved_stream_analog_level_reference,
                         ArrayView<const float> output_reference) {
  GainControlImpl gain_controller;
  SetupComponent(sample_rate_hz, mode, target_level_dbfs, stream_analog_level,
                 compression_gain_db, enable_limiter, analog_level_min,
                 analog_level_max, &gain_controller);

  const int samples_per_channel = CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig render_config(sample_rate_hz, num_channels);
  AudioBuffer render_buffer(
      render_config.sample_rate_hz(), render_config.num_channels(),
      render_config.sample_rate_hz(), 1, render_config.sample_rate_hz(), 1);
  test::InputAudioFile render_file(
      test::GetApmRenderTestVectorFileName(sample_rate_hz));
  std::vector<float> render_input(samples_per_channel * num_channels);

  const StreamConfig capture_config(sample_rate_hz, num_channels);
  AudioBuffer capture_buffer(
      capture_config.sample_rate_hz(), capture_config.num_channels(),
      capture_config.sample_rate_hz(), 1, capture_config.sample_rate_hz(), 1);
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);

  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &render_file, render_input);
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(render_config, render_input, &render_buffer);
    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    ProcessOneFrame(sample_rate_hz, &render_buffer, &capture_buffer,
                    &gain_controller);
  }

  // Extract and verify the test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  EXPECT_EQ(achieved_stream_analog_level_reference,
            gain_controller.stream_analog_level());

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceeding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kElementErrorBound));
}

}  // namespace

// TODO(peah): Activate all these tests for ARM and ARM64 once the issue on the
// Chromium ARM and ARM64 boths have been identified. This is tracked in the
// issue https://bugs.chromium.org/p/webrtc/issues/detail?id=5711.

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.006561f, -0.004608f, -0.002899f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Stereo16kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.027313f, -0.015900f, -0.028107f,
                                    -0.027313f, -0.015900f, -0.028107f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono32kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.010162f, -0.009155f, -0.008301f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono48kHz_AdaptiveAnalog_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.010162f, -0.009155f, -0.008301f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.003967f, -0.002777f, -0.001770f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Stereo16kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.015411f, -0.008972f, -0.015839f,
                                    -0.015411f, -0.008972f, -0.015839f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono32kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.006134f, -0.005524f, -0.005005f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono48kHz_AdaptiveDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.006134f, -0.005524f, -0.005005};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kAdaptiveDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.011749f, -0.008270f, -0.005219f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Stereo16kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.048896f, -0.028479f, -0.050345f,
                                    -0.048896f, -0.028479f, -0.050345f};
  RunBitExactnessTest(16000, 2, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono32kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.018158f, -0.016357f, -0.014832f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono48kHz_FixedDigital_Tl10_SL50_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 50;
  const float kOutputReference[] = {-0.018158f, -0.016357f, -0.014832f};
  RunBitExactnessTest(32000, 1, GainControl::Mode::kFixedDigital, 10, 50, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveAnalog_Tl10_SL10_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 12;
  const float kOutputReference[] = {-0.006561f, -0.004608f, -0.002899f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 10, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveAnalog_Tl10_SL100_CG5_Lim_AL70_80) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.003998f, -0.002808f, -0.001770f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveAnalog, 10, 100, 5,
                      true, 70, 80, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveDigital_Tl10_SL100_CG5_NoLim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.004028f, -0.002838f, -0.001770f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 100, 5,
                      false, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveDigital_Tl40_SL100_CG5_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.008728f, -0.006134f, -0.003845f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 40, 100, 5,
                      true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

TEST(GainControlBitExactnessTest,
     DISABLED_Mono16kHz_AdaptiveDigital_Tl10_SL100_CG30_Lim_AL0_100) {
  const int kStreamAnalogLevelReference = 100;
  const float kOutputReference[] = {-0.005859f, -0.004120f, -0.002594f};
  RunBitExactnessTest(16000, 1, GainControl::Mode::kAdaptiveDigital, 10, 100,
                      30, true, 0, 100, kStreamAnalogLevelReference,
                      kOutputReference);
}

}  // namespace webrtc
