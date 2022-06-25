/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn_gru.h"

#include <array>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "modules/audio_processing/test/performance_timer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

void TestGatedRecurrentLayer(
    GatedRecurrentLayer& gru,
    rtc::ArrayView<const float> input_sequence,
    rtc::ArrayView<const float> expected_output_sequence) {
  const int input_sequence_length = rtc::CheckedDivExact(
      rtc::dchecked_cast<int>(input_sequence.size()), gru.input_size());
  const int output_sequence_length = rtc::CheckedDivExact(
      rtc::dchecked_cast<int>(expected_output_sequence.size()), gru.size());
  ASSERT_EQ(input_sequence_length, output_sequence_length)
      << "The test data length is invalid.";
  // Feed the GRU layer and check the output at every step.
  gru.Reset();
  for (int i = 0; i < input_sequence_length; ++i) {
    SCOPED_TRACE(i);
    gru.ComputeOutput(
        input_sequence.subview(i * gru.input_size(), gru.input_size()));
    const auto expected_output =
        expected_output_sequence.subview(i * gru.size(), gru.size());
    ExpectNearAbsolute(expected_output, gru, 3e-6f);
  }
}

// Gated recurrent units layer test data.
constexpr int kGruInputSize = 5;
constexpr int kGruOutputSize = 4;
constexpr std::array<int8_t, 12> kGruBias = {96,   -99, -81, -114, 49,  119,
                                             -118, 68,  -76, 91,   121, 125};
constexpr std::array<int8_t, 60> kGruWeights = {
    // Input 0.
    124, 9, 1, 116,        // Update.
    -66, -21, -118, -110,  // Reset.
    104, 75, -23, -51,     // Output.
    // Input 1.
    -72, -111, 47, 93,   // Update.
    77, -98, 41, -8,     // Reset.
    40, -23, -43, -107,  // Output.
    // Input 2.
    9, -73, 30, -32,      // Update.
    -2, 64, -26, 91,      // Reset.
    -48, -24, -28, -104,  // Output.
    // Input 3.
    74, -46, 116, 15,    // Update.
    32, 52, -126, -38,   // Reset.
    -121, 12, -16, 110,  // Output.
    // Input 4.
    -95, 66, -103, -35,  // Update.
    -38, 3, -126, -61,   // Reset.
    28, 98, -117, -43    // Output.
};
constexpr std::array<int8_t, 48> kGruRecurrentWeights = {
    // Output 0.
    -3, 87, 50, 51,     // Update.
    -22, 27, -39, 62,   // Reset.
    31, -83, -52, -48,  // Output.
    // Output 1.
    -6, 83, -19, 104,  // Update.
    105, 48, 23, 68,   // Reset.
    23, 40, 7, -120,   // Output.
    // Output 2.
    64, -62, 117, 85,     // Update.
    51, -43, 54, -105,    // Reset.
    120, 56, -128, -107,  // Output.
    // Output 3.
    39, 50, -17, -47,   // Update.
    -117, 14, 108, 12,  // Reset.
    -7, -72, 103, -87,  // Output.
};
constexpr std::array<float, 20> kGruInputSequence = {
    0.89395463f, 0.93224651f, 0.55788344f, 0.32341808f, 0.93355054f,
    0.13475326f, 0.97370994f, 0.14253306f, 0.93710381f, 0.76093364f,
    0.65780413f, 0.41657975f, 0.49403164f, 0.46843281f, 0.75138855f,
    0.24517593f, 0.47657707f, 0.57064998f, 0.435184f,   0.19319285f};
constexpr std::array<float, 16> kGruExpectedOutputSequence = {
    0.0239123f,  0.5773077f,  0.f,         0.f,
    0.01282811f, 0.64330572f, 0.f,         0.04863098f,
    0.00781069f, 0.75267816f, 0.f,         0.02579715f,
    0.00471378f, 0.59162533f, 0.11087593f, 0.01334511f};

class RnnGruParametrization
    : public ::testing::TestWithParam<AvailableCpuFeatures> {};

// Checks that the output of a GRU layer is within tolerance given test input
// data.
TEST_P(RnnGruParametrization, CheckGatedRecurrentLayer) {
  GatedRecurrentLayer gru(kGruInputSize, kGruOutputSize, kGruBias, kGruWeights,
                          kGruRecurrentWeights,
                          /*cpu_features=*/GetParam(),
                          /*layer_name=*/"GRU");
  TestGatedRecurrentLayer(gru, kGruInputSequence, kGruExpectedOutputSequence);
}

TEST_P(RnnGruParametrization, DISABLED_BenchmarkGatedRecurrentLayer) {
  // Prefetch test data.
  std::unique_ptr<FileReader> reader = CreateGruInputReader();
  std::vector<float> gru_input_sequence(reader->size());
  reader->ReadChunk(gru_input_sequence);

  using ::rnnoise::kHiddenGruBias;
  using ::rnnoise::kHiddenGruRecurrentWeights;
  using ::rnnoise::kHiddenGruWeights;
  using ::rnnoise::kHiddenLayerOutputSize;
  using ::rnnoise::kInputLayerOutputSize;

  GatedRecurrentLayer gru(kInputLayerOutputSize, kHiddenLayerOutputSize,
                          kHiddenGruBias, kHiddenGruWeights,
                          kHiddenGruRecurrentWeights,
                          /*cpu_features=*/GetParam(),
                          /*layer_name=*/"GRU");

  rtc::ArrayView<const float> input_sequence(gru_input_sequence);
  ASSERT_EQ(input_sequence.size() % kInputLayerOutputSize,
            static_cast<size_t>(0));
  const int input_sequence_length =
      input_sequence.size() / kInputLayerOutputSize;

  constexpr int kNumTests = 100;
  ::webrtc::test::PerformanceTimer perf_timer(kNumTests);
  for (int k = 0; k < kNumTests; ++k) {
    perf_timer.StartTimer();
    for (int i = 0; i < input_sequence_length; ++i) {
      gru.ComputeOutput(
          input_sequence.subview(i * gru.input_size(), gru.input_size()));
    }
    perf_timer.StopTimer();
  }
  RTC_LOG(LS_INFO) << (perf_timer.GetDurationAverage() / 1000) << " +/- "
                   << (perf_timer.GetDurationStandardDeviation() / 1000)
                   << " ms";
}

// Finds the relevant CPU features combinations to test.
std::vector<AvailableCpuFeatures> GetCpuFeaturesToTest() {
  std::vector<AvailableCpuFeatures> v;
  v.push_back(NoAvailableCpuFeatures());
  AvailableCpuFeatures available = GetAvailableCpuFeatures();
  if (available.sse2) {
    v.push_back({/*sse2=*/true, /*avx2=*/false, /*neon=*/false});
  }
  if (available.avx2) {
    v.push_back({/*sse2=*/false, /*avx2=*/true, /*neon=*/false});
  }
  if (available.neon) {
    v.push_back({/*sse2=*/false, /*avx2=*/false, /*neon=*/true});
  }
  return v;
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    RnnGruParametrization,
    ::testing::ValuesIn(GetCpuFeaturesToTest()),
    [](const ::testing::TestParamInfo<AvailableCpuFeatures>& info) {
      return info.param.ToString();
    });

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
