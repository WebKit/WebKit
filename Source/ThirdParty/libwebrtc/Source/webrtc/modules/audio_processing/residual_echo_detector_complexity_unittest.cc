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
#include "webrtc/modules/audio_processing/residual_echo_detector.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"
#include "webrtc/modules/audio_processing/test/performance_timer.h"
#include "webrtc/modules/audio_processing/test/simulator_buffers.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/perf_test.h"

namespace webrtc {
namespace {

constexpr size_t kNumFramesToProcess = 20000;
constexpr size_t kNumFramesToProcessStandalone = 50 * kNumFramesToProcess;
constexpr size_t kProcessingBatchSize = 200;
constexpr size_t kProcessingBatchSizeStandalone = 50 * kProcessingBatchSize;
constexpr size_t kNumberOfWarmupMeasurements =
    (kNumFramesToProcess / kProcessingBatchSize) / 2;
constexpr size_t kNumberOfWarmupMeasurementsStandalone =
    (kNumFramesToProcessStandalone / kProcessingBatchSizeStandalone) / 2;
constexpr int kSampleRate = AudioProcessing::kSampleRate48kHz;
constexpr int kNumberOfChannels = 1;

std::string FormPerformanceMeasureString(const test::PerformanceTimer& timer,
                                         int number_of_warmup_samples) {
  std::string s =
      std::to_string(timer.GetDurationAverage(number_of_warmup_samples));
  s += ", ";
  s += std::to_string(
      timer.GetDurationStandardDeviation(number_of_warmup_samples));
  return s;
}

void RunStandaloneSubmodule() {
  test::SimulatorBuffers buffers(
      kSampleRate, kSampleRate, kSampleRate, kSampleRate, kNumberOfChannels,
      kNumberOfChannels, kNumberOfChannels, kNumberOfChannels);
  test::PerformanceTimer timer(kNumFramesToProcessStandalone /
                               kProcessingBatchSizeStandalone);

  ResidualEchoDetector echo_detector;
  echo_detector.Initialize();
  float sum = 0.f;

  for (size_t frame_no = 0; frame_no < kNumFramesToProcessStandalone;
       ++frame_no) {
    // The first batch of frames are for warming up, and are not part of the
    // benchmark. After that the processing time is measured in chunks of
    // kProcessingBatchSize frames.
    if (frame_no % kProcessingBatchSizeStandalone == 0) {
      timer.StartTimer();
    }

    buffers.UpdateInputBuffers();
    echo_detector.AnalyzeRenderAudio(rtc::ArrayView<const float>(
        buffers.render_input_buffer->split_bands_const_f(0)[kBand0To8kHz],
        buffers.render_input_buffer->num_frames_per_band()));
    echo_detector.AnalyzeCaptureAudio(rtc::ArrayView<const float>(
        buffers.capture_input_buffer->split_bands_const_f(0)[kBand0To8kHz],
        buffers.capture_input_buffer->num_frames_per_band()));
    sum += echo_detector.echo_likelihood();

    if (frame_no % kProcessingBatchSizeStandalone ==
        kProcessingBatchSizeStandalone - 1) {
      timer.StopTimer();
    }
  }
  EXPECT_EQ(0.0f, sum);
  webrtc::test::PrintResultMeanAndError(
      "echo_detector_call_durations", "", "StandaloneEchoDetector",
      FormPerformanceMeasureString(timer,
                                   kNumberOfWarmupMeasurementsStandalone),
      "us", false);
}

void RunTogetherWithApm(std::string test_description,
                        bool use_mobile_aec,
                        bool include_default_apm_processing) {
  test::SimulatorBuffers buffers(
      kSampleRate, kSampleRate, kSampleRate, kSampleRate, kNumberOfChannels,
      kNumberOfChannels, kNumberOfChannels, kNumberOfChannels);
  test::PerformanceTimer timer(kNumFramesToProcess / kProcessingBatchSize);

  webrtc::Config config;
  AudioProcessing::Config apm_config;
  if (include_default_apm_processing) {
    config.Set<DelayAgnostic>(new DelayAgnostic(true));
    config.Set<ExtendedFilter>(new ExtendedFilter(true));
  }
  apm_config.level_controller.enabled = include_default_apm_processing;
  apm_config.residual_echo_detector.enabled = true;

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
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->high_pass_filter()->Enable(include_default_apm_processing));
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->noise_suppression()->Enable(include_default_apm_processing));
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->voice_detection()->Enable(include_default_apm_processing));
  ASSERT_EQ(AudioProcessing::kNoError,
            apm->level_estimator()->Enable(include_default_apm_processing));

  StreamConfig stream_config(kSampleRate, kNumberOfChannels, false);

  for (size_t frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    // The first batch of frames are for warming up, and are not part of the
    // benchmark. After that the processing time is measured in chunks of
    // kProcessingBatchSize frames.
    if (frame_no % kProcessingBatchSize == 0) {
      timer.StartTimer();
    }

    buffers.UpdateInputBuffers();

    ASSERT_EQ(
        AudioProcessing::kNoError,
        apm->ProcessReverseStream(&buffers.render_input[0], stream_config,
                                  stream_config, &buffers.render_output[0]));

    ASSERT_EQ(AudioProcessing::kNoError, apm->set_stream_delay_ms(0));
    if (include_default_apm_processing) {
      apm->gain_control()->set_stream_analog_level(0);
      if (!use_mobile_aec) {
        apm->echo_cancellation()->set_stream_drift_samples(0);
      }
    }
    ASSERT_EQ(AudioProcessing::kNoError,
              apm->ProcessStream(&buffers.capture_input[0], stream_config,
                                 stream_config, &buffers.capture_output[0]));

    if (frame_no % kProcessingBatchSize == kProcessingBatchSize - 1) {
      timer.StopTimer();
    }
  }

  webrtc::test::PrintResultMeanAndError(
      "echo_detector_call_durations", "_total", test_description,
      FormPerformanceMeasureString(timer, kNumberOfWarmupMeasurements), "us",
      false);
}

}  // namespace

TEST(EchoDetectorPerformanceTest, StandaloneProcessing) {
  RunStandaloneSubmodule();
}

TEST(EchoDetectorPerformanceTest, ProcessingViaApm) {
  RunTogetherWithApm("SimpleEchoDetectorViaApm", false, false);
}

TEST(EchoDetectorPerformanceTest, InteractionWithDefaultApm) {
  RunTogetherWithApm("EchoDetectorAndDefaultDesktopApm", false, true);
  RunTogetherWithApm("EchoDetectorAndDefaultMobileApm", true, true);
}

}  // namespace webrtc
