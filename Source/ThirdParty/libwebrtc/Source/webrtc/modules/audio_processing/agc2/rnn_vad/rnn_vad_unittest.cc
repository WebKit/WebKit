/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "common_audio/resampler/push_sinc_resampler.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/features_extraction.h"
#include "modules/audio_processing/agc2/rnn_vad/rnn.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "modules/audio_processing/test/performance_timer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kFrameSize10ms48kHz = 480;

void DumpPerfStats(int num_samples,
                   int sample_rate,
                   double average_us,
                   double standard_deviation) {
  float audio_track_length_ms =
      1e3f * static_cast<float>(num_samples) / static_cast<float>(sample_rate);
  float average_ms = static_cast<float>(average_us) / 1e3f;
  float speed = audio_track_length_ms / average_ms;
  RTC_LOG(LS_INFO) << "track duration (ms): " << audio_track_length_ms;
  RTC_LOG(LS_INFO) << "average processing time (ms): " << average_ms << " +/- "
                   << (standard_deviation / 1e3);
  RTC_LOG(LS_INFO) << "speed: " << speed << "x";
}

// When the RNN VAD model is updated and the expected output changes, set the
// constant below to true in order to write new expected output binary files.
constexpr bool kWriteComputedOutputToFile = false;

// Avoids that one forgets to set `kWriteComputedOutputToFile` back to false
// when the expected output files are re-exported.
TEST(RnnVadTest, CheckWriteComputedOutputIsFalse) {
  ASSERT_FALSE(kWriteComputedOutputToFile)
      << "Cannot land if kWriteComputedOutput is true.";
}

class RnnVadProbabilityParametrization
    : public ::testing::TestWithParam<AvailableCpuFeatures> {};

// Checks that the computed VAD probability for a test input sequence sampled at
// 48 kHz is within tolerance.
TEST_P(RnnVadProbabilityParametrization, RnnVadProbabilityWithinTolerance) {
  // Init resampler, feature extractor and RNN.
  PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
  const AvailableCpuFeatures cpu_features = GetParam();
  FeaturesExtractor features_extractor(cpu_features);
  RnnVad rnn_vad(cpu_features);

  // Init input samples and expected output readers.
  std::unique_ptr<FileReader> samples_reader = CreatePcmSamplesReader();
  std::unique_ptr<FileReader> expected_vad_prob_reader = CreateVadProbsReader();

  // Input length. The last incomplete frame is ignored.
  const int num_frames = samples_reader->size() / kFrameSize10ms48kHz;

  // Init buffers.
  std::vector<float> samples_48k(kFrameSize10ms48kHz);
  std::vector<float> samples_24k(kFrameSize10ms24kHz);
  std::vector<float> feature_vector(kFeatureVectorSize);
  std::vector<float> computed_vad_prob(num_frames);
  std::vector<float> expected_vad_prob(num_frames);

  // Read expected output.
  ASSERT_TRUE(expected_vad_prob_reader->ReadChunk(expected_vad_prob));

  // Compute VAD probabilities on the downsampled input.
  float cumulative_error = 0.f;
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_TRUE(samples_reader->ReadChunk(samples_48k));
    decimator.Resample(samples_48k.data(), samples_48k.size(),
                       samples_24k.data(), samples_24k.size());
    bool is_silence = features_extractor.CheckSilenceComputeFeatures(
        {samples_24k.data(), kFrameSize10ms24kHz},
        {feature_vector.data(), kFeatureVectorSize});
    computed_vad_prob[i] = rnn_vad.ComputeVadProbability(
        {feature_vector.data(), kFeatureVectorSize}, is_silence);
    EXPECT_NEAR(computed_vad_prob[i], expected_vad_prob[i], 1e-3f);
    cumulative_error += std::abs(computed_vad_prob[i] - expected_vad_prob[i]);
  }
  // Check average error.
  EXPECT_LT(cumulative_error / num_frames, 1e-4f);

  if (kWriteComputedOutputToFile) {
    FileWriter vad_prob_writer("new_vad_prob.dat");
    vad_prob_writer.WriteChunk(computed_vad_prob);
  }
}

// Performance test for the RNN VAD (pre-fetching and downsampling are
// excluded). Keep disabled and only enable locally to measure performance as
// follows:
// - on desktop: run the this unit test adding "--logs";
// - on android: run the this unit test adding "--logcat-output-file".
TEST_P(RnnVadProbabilityParametrization, DISABLED_RnnVadPerformance) {
  // PCM samples reader and buffers.
  std::unique_ptr<FileReader> samples_reader = CreatePcmSamplesReader();
  // The last incomplete frame is ignored.
  const int num_frames = samples_reader->size() / kFrameSize10ms48kHz;
  std::array<float, kFrameSize10ms48kHz> samples;
  // Pre-fetch and decimate samples.
  PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
  std::vector<float> prefetched_decimated_samples;
  prefetched_decimated_samples.resize(num_frames * kFrameSize10ms24kHz);
  for (int i = 0; i < num_frames; ++i) {
    ASSERT_TRUE(samples_reader->ReadChunk(samples));
    decimator.Resample(samples.data(), samples.size(),
                       &prefetched_decimated_samples[i * kFrameSize10ms24kHz],
                       kFrameSize10ms24kHz);
  }
  // Initialize.
  const AvailableCpuFeatures cpu_features = GetParam();
  FeaturesExtractor features_extractor(cpu_features);
  std::array<float, kFeatureVectorSize> feature_vector;
  RnnVad rnn_vad(cpu_features);
  constexpr int number_of_tests = 100;
  ::webrtc::test::PerformanceTimer perf_timer(number_of_tests);
  for (int k = 0; k < number_of_tests; ++k) {
    features_extractor.Reset();
    rnn_vad.Reset();
    // Process frames.
    perf_timer.StartTimer();
    for (int i = 0; i < num_frames; ++i) {
      bool is_silence = features_extractor.CheckSilenceComputeFeatures(
          {&prefetched_decimated_samples[i * kFrameSize10ms24kHz],
           kFrameSize10ms24kHz},
          feature_vector);
      rnn_vad.ComputeVadProbability(feature_vector, is_silence);
    }
    perf_timer.StopTimer();
  }
  DumpPerfStats(num_frames * kFrameSize10ms24kHz, kSampleRate24kHz,
                perf_timer.GetDurationAverage(),
                perf_timer.GetDurationStandardDeviation());
}

// Finds the relevant CPU features combinations to test.
std::vector<AvailableCpuFeatures> GetCpuFeaturesToTest() {
  std::vector<AvailableCpuFeatures> v;
  v.push_back(NoAvailableCpuFeatures());
  AvailableCpuFeatures available = GetAvailableCpuFeatures();
  if (available.avx2 && available.sse2) {
    v.push_back({/*sse2=*/true, /*avx2=*/true, /*neon=*/false});
  }
  if (available.sse2) {
    v.push_back({/*sse2=*/true, /*avx2=*/false, /*neon=*/false});
  }
  if (available.neon) {
    v.push_back({/*sse2=*/false, /*avx2=*/false, /*neon=*/true});
  }
  return v;
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    RnnVadProbabilityParametrization,
    ::testing::ValuesIn(GetCpuFeaturesToTest()),
    [](const ::testing::TestParamInfo<AvailableCpuFeatures>& info) {
      return info.param.ToString();
    });

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
