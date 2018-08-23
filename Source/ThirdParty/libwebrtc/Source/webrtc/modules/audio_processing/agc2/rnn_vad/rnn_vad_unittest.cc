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
#include <vector>

#include "common_audio/resampler/push_sinc_resampler.h"
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
namespace test {
namespace {

constexpr size_t kFrameSize10ms48kHz = 480;

void DumpPerfStats(size_t num_samples,
                   size_t sample_rate,
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

}  // namespace

// Performance test for the RNN VAD (pre-fetching and downsampling are
// excluded). Keep disabled and only enable locally to measure performance as
// follows:
// - on desktop: run the this unit test adding "--logs";
// - on android: run the this unit test adding "--logcat-output-file".
TEST(RnnVadTest, DISABLED_RnnVadPerformance) {
  // PCM samples reader and buffers.
  auto samples_reader = CreatePcmSamplesReader(kFrameSize10ms48kHz);
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples;
  // Pre-fetch and decimate samples.
  PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
  std::vector<float> prefetched_decimated_samples;
  prefetched_decimated_samples.resize(num_frames * kFrameSize10ms24kHz);
  for (size_t i = 0; i < num_frames; ++i) {
    samples_reader.first->ReadChunk(samples);
    decimator.Resample(samples.data(), samples.size(),
                       &prefetched_decimated_samples[i * kFrameSize10ms24kHz],
                       kFrameSize10ms24kHz);
  }
  // Initialize.
  FeaturesExtractor features_extractor;
  std::array<float, kFeatureVectorSize> feature_vector;
  RnnBasedVad rnn_vad;
  constexpr size_t number_of_tests = 100;
  ::webrtc::test::PerformanceTimer perf_timer(number_of_tests);
  for (size_t k = 0; k < number_of_tests; ++k) {
    features_extractor.Reset();
    rnn_vad.Reset();
    // Process frames.
    perf_timer.StartTimer();
    for (size_t i = 0; i < num_frames; ++i) {
      bool is_silence = features_extractor.CheckSilenceComputeFeatures(
          {&prefetched_decimated_samples[i * kFrameSize10ms24kHz],
           kFrameSize10ms24kHz},
          feature_vector);
      rnn_vad.ComputeVadProbability(feature_vector, is_silence);
    }
    perf_timer.StopTimer();
    samples_reader.first->SeekBeginning();
  }
  DumpPerfStats(num_frames * kFrameSize10ms24kHz, kSampleRate24kHz,
                perf_timer.GetDurationAverage(),
                perf_timer.GetDurationStandardDeviation());
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
