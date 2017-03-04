/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <numeric>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/level_controller/level_controller.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/bitexactness_tools.h"
#include "webrtc/modules/audio_processing/test/performance_timer.h"
#include "webrtc/modules/audio_processing/test/simulator_buffers.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/perf_test.h"

namespace webrtc {
namespace {

const size_t kNumFramesToProcess = 300;
const size_t kNumFramesToProcessAtWarmup = 300;
const size_t kToTalNumFrames =
    kNumFramesToProcess + kNumFramesToProcessAtWarmup;

std::string FormPerformanceMeasureString(const test::PerformanceTimer& timer) {
  std::string s = std::to_string(timer.GetDurationAverage());
  s += ", ";
  s += std::to_string(timer.GetDurationStandardDeviation());
  return s;
}

void RunStandaloneSubmodule(int sample_rate_hz, size_t num_channels) {
  test::SimulatorBuffers buffers(sample_rate_hz, sample_rate_hz, sample_rate_hz,
                                 sample_rate_hz, num_channels, num_channels,
                                 num_channels, num_channels);
  test::PerformanceTimer timer(kNumFramesToProcess);

  LevelController level_controller;
  level_controller.Initialize(sample_rate_hz);

  for (size_t frame_no = 0; frame_no < kToTalNumFrames; ++frame_no) {
    buffers.UpdateInputBuffers();

    if (frame_no >= kNumFramesToProcessAtWarmup) {
      timer.StartTimer();
    }
    level_controller.Process(buffers.capture_input_buffer.get());
    if (frame_no >= kNumFramesToProcessAtWarmup) {
      timer.StopTimer();
    }
  }
  webrtc::test::PrintResultMeanAndError(
      "level_controller_call_durations",
      "_" + std::to_string(sample_rate_hz) + "Hz_" +
          std::to_string(num_channels) + "_channels",
      "StandaloneLevelControl", FormPerformanceMeasureString(timer), "us",
      false);
}

void RunTogetherWithApm(std::string test_description,
                        int render_input_sample_rate_hz,
                        int render_output_sample_rate_hz,
                        int capture_input_sample_rate_hz,
                        int capture_output_sample_rate_hz,
                        size_t num_channels,
                        bool use_mobile_aec,
                        bool include_default_apm_processing) {
  test::SimulatorBuffers buffers(
      render_input_sample_rate_hz, capture_input_sample_rate_hz,
      render_output_sample_rate_hz, capture_output_sample_rate_hz, num_channels,
      num_channels, num_channels, num_channels);
  test::PerformanceTimer render_timer(kNumFramesToProcess);
  test::PerformanceTimer capture_timer(kNumFramesToProcess);
  test::PerformanceTimer total_timer(kNumFramesToProcess);

  webrtc::Config config;
  AudioProcessing::Config apm_config;
  if (include_default_apm_processing) {
    config.Set<DelayAgnostic>(new DelayAgnostic(true));
    config.Set<ExtendedFilter>(new ExtendedFilter(true));
  }
  apm_config.level_controller.enabled = true;
  apm_config.residual_echo_detector.enabled = include_default_apm_processing;

  std::unique_ptr<AudioProcessing> apm;
  apm.reset(AudioProcessing::Create(config));
  ASSERT_TRUE(apm.get());
  apm->ApplyConfig(apm_config);

  ASSERT_EQ(AudioProcessing::kNoError,
            apm->gain_control()->Enable(include_default_apm_processing));
  if (use_mobile_aec) {
    ASSERT_EQ(AudioProcessing::kNoError,
              apm->echo_cancellation()->Enable(false));
    ASSERT_EQ(AudioProcessing::kNoError, apm->echo_control_mobile()->Enable(
                                             include_default_apm_processing));
  } else {
    ASSERT_EQ(AudioProcessing::kNoError,
              apm->echo_cancellation()->Enable(include_default_apm_processing));
    ASSERT_EQ(AudioProcessing::kNoError,
              apm->echo_control_mobile()->Enable(false));
  }
  apm_config.high_pass_filter.enabled = include_default_apm_processing;
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->noise_suppression()->Enable(include_default_apm_processing));
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->voice_detection()->Enable(include_default_apm_processing));
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->level_estimator()->Enable(include_default_apm_processing));

  StreamConfig render_input_config(render_input_sample_rate_hz, num_channels,
                                   false);
  StreamConfig render_output_config(render_output_sample_rate_hz, num_channels,
                                    false);
  StreamConfig capture_input_config(capture_input_sample_rate_hz, num_channels,
                                    false);
  StreamConfig capture_output_config(capture_output_sample_rate_hz,
                                     num_channels, false);

  for (size_t frame_no = 0; frame_no < kToTalNumFrames; ++frame_no) {
    buffers.UpdateInputBuffers();

    if (frame_no >= kNumFramesToProcessAtWarmup) {
      total_timer.StartTimer();
      render_timer.StartTimer();
    }
    ASSERT_EQ(AudioProcessing::kNoError,
              apm->ProcessReverseStream(
                  &buffers.render_input[0], render_input_config,
                  render_output_config, &buffers.render_output[0]));

    if (frame_no >= kNumFramesToProcessAtWarmup) {
      render_timer.StopTimer();

      capture_timer.StartTimer();
    }

    ASSERT_EQ(AudioProcessing::kNoError, apm->set_stream_delay_ms(0));
    ASSERT_EQ(
        AudioProcessing::kNoError,
        apm->ProcessStream(&buffers.capture_input[0], capture_input_config,
                           capture_output_config, &buffers.capture_output[0]));

    if (frame_no >= kNumFramesToProcessAtWarmup) {
      capture_timer.StopTimer();
      total_timer.StopTimer();
    }
  }

  webrtc::test::PrintResultMeanAndError(
      "level_controller_call_durations",
      "_" + std::to_string(render_input_sample_rate_hz) + "_" +
          std::to_string(render_output_sample_rate_hz) + "_" +
          std::to_string(capture_input_sample_rate_hz) + "_" +
          std::to_string(capture_output_sample_rate_hz) + "Hz_" +
          std::to_string(num_channels) + "_channels" + "_render",
      test_description, FormPerformanceMeasureString(render_timer), "us",
      false);
  webrtc::test::PrintResultMeanAndError(
      "level_controller_call_durations",
      "_" + std::to_string(render_input_sample_rate_hz) + "_" +
          std::to_string(render_output_sample_rate_hz) + "_" +
          std::to_string(capture_input_sample_rate_hz) + "_" +
          std::to_string(capture_output_sample_rate_hz) + "Hz_" +
          std::to_string(num_channels) + "_channels" + "_capture",
      test_description, FormPerformanceMeasureString(capture_timer), "us",
      false);
  webrtc::test::PrintResultMeanAndError(
      "level_controller_call_durations",
      "_" + std::to_string(render_input_sample_rate_hz) + "_" +
          std::to_string(render_output_sample_rate_hz) + "_" +
          std::to_string(capture_input_sample_rate_hz) + "_" +
          std::to_string(capture_output_sample_rate_hz) + "Hz_" +
          std::to_string(num_channels) + "_channels" + "_total",
      test_description, FormPerformanceMeasureString(total_timer), "us", false);
}

}  // namespace

TEST(LevelControllerPerformanceTest, StandaloneProcessing) {
  int sample_rates_to_test[] = {
      AudioProcessing::kSampleRate8kHz, AudioProcessing::kSampleRate16kHz,
      AudioProcessing::kSampleRate32kHz, AudioProcessing::kSampleRate48kHz};
  for (auto sample_rate : sample_rates_to_test) {
    for (size_t num_channels = 1; num_channels <= 2; ++num_channels) {
      RunStandaloneSubmodule(sample_rate, num_channels);
    }
  }
}

void TestSomeSampleRatesWithApm(const std::string& test_name,
                                bool use_mobile_agc,
                                bool include_default_apm_processing) {
  // Test some stereo combinations first.
  size_t num_channels = 2;
  RunTogetherWithApm(test_name, 48000, 48000, AudioProcessing::kSampleRate16kHz,
                     AudioProcessing::kSampleRate32kHz, num_channels,
                     use_mobile_agc, include_default_apm_processing);
  RunTogetherWithApm(test_name, 48000, 48000, AudioProcessing::kSampleRate48kHz,
                     AudioProcessing::kSampleRate8kHz, num_channels,
                     use_mobile_agc, include_default_apm_processing);
  RunTogetherWithApm(test_name, 48000, 48000, 44100, 44100, num_channels,
                     use_mobile_agc, include_default_apm_processing);

  // Then test mono combinations.
  num_channels = 1;
  RunTogetherWithApm(test_name, 48000, 48000, AudioProcessing::kSampleRate48kHz,
                     AudioProcessing::kSampleRate48kHz, num_channels,
                     use_mobile_agc, include_default_apm_processing);
}

#if !defined(WEBRTC_ANDROID)
TEST(LevelControllerPerformanceTest, ProcessingViaApm) {
#else
TEST(LevelControllerPerformanceTest, DISABLED_ProcessingViaApm) {
#endif
  // Run without default APM processing and desktop AGC.
  TestSomeSampleRatesWithApm("SimpleLevelControlViaApm", false, false);
}

#if !defined(WEBRTC_ANDROID)
TEST(LevelControllerPerformanceTest, InteractionWithDefaultApm) {
#else
TEST(LevelControllerPerformanceTest, DISABLED_InteractionWithDefaultApm) {
#endif
  bool include_default_apm_processing = true;
  TestSomeSampleRatesWithApm("LevelControlAndDefaultDesktopApm", false,
                             include_default_apm_processing);
  TestSomeSampleRatesWithApm("LevelControlAndDefaultMobileApm", true,
                             include_default_apm_processing);
}

}  // namespace webrtc
