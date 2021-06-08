/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn.h"

#include <array>
#include <memory>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "modules/audio_processing/test/performance_timer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/arch.h"
#include "test/gtest.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

namespace {

void TestFullyConnectedLayer(FullyConnectedLayer* fc,
                             rtc::ArrayView<const float> input_vector,
                             rtc::ArrayView<const float> expected_output) {
  RTC_CHECK(fc);
  fc->ComputeOutput(input_vector);
  ExpectNearAbsolute(expected_output, fc->GetOutput(), 1e-5f);
}

void TestGatedRecurrentLayer(
    GatedRecurrentLayer* gru,
    rtc::ArrayView<const float> input_sequence,
    rtc::ArrayView<const float> expected_output_sequence) {
  RTC_CHECK(gru);
  auto gru_output_view = gru->GetOutput();
  const size_t input_sequence_length =
      rtc::CheckedDivExact(input_sequence.size(), gru->input_size());
  const size_t output_sequence_length =
      rtc::CheckedDivExact(expected_output_sequence.size(), gru->output_size());
  ASSERT_EQ(input_sequence_length, output_sequence_length)
      << "The test data length is invalid.";
  // Feed the GRU layer and check the output at every step.
  gru->Reset();
  for (size_t i = 0; i < input_sequence_length; ++i) {
    SCOPED_TRACE(i);
    gru->ComputeOutput(
        input_sequence.subview(i * gru->input_size(), gru->input_size()));
    const auto expected_output = expected_output_sequence.subview(
        i * gru->output_size(), gru->output_size());
    ExpectNearAbsolute(expected_output, gru_output_view, 3e-6f);
  }
}

// Fully connected layer test data.
constexpr std::array<float, 42> kFullyConnectedInputVector = {
    -1.00131f,   -0.627069f, -7.81097f,  7.86285f,    -2.87145f,  3.32365f,
    -0.653161f,  0.529839f,  -0.425307f, 0.25583f,    0.235094f,  0.230527f,
    -0.144687f,  0.182785f,  0.57102f,   0.125039f,   0.479482f,  -0.0255439f,
    -0.0073141f, -0.147346f, -0.217106f, -0.0846906f, -8.34943f,  3.09065f,
    1.42628f,    -0.85235f,  -0.220207f, -0.811163f,  2.09032f,   -2.01425f,
    -0.690268f,  -0.925327f, -0.541354f, 0.58455f,    -0.606726f, -0.0372358f,
    0.565991f,   0.435854f,  0.420812f,  0.162198f,   -2.13f,     10.0089f};
constexpr std::array<float, 24> kFullyConnectedExpectedOutput = {
    -0.623293f, -0.988299f, 0.999378f,  0.967168f,  0.103087f,  -0.978545f,
    -0.856347f, 0.346675f,  1.f,        -0.717442f, -0.544176f, 0.960363f,
    0.983443f,  0.999991f,  -0.824335f, 0.984742f,  0.990208f,  0.938179f,
    0.875092f,  0.999846f,  0.997707f,  -0.999382f, 0.973153f,  -0.966605f};

// Gated recurrent units layer test data.
constexpr size_t kGruInputSize = 5;
constexpr size_t kGruOutputSize = 4;
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

std::string GetOptimizationName(Optimization optimization) {
  switch (optimization) {
    case Optimization::kSse2:
      return "SSE2";
    case Optimization::kNeon:
      return "NEON";
    case Optimization::kNone:
      return "none";
  }
}

struct Result {
  Optimization optimization;
  double average_us;
  double std_dev_us;
};

}  // namespace

// Checks that the output of a fully connected layer is within tolerance given
// test input data.
TEST(RnnVadTest, CheckFullyConnectedLayerOutput) {
  FullyConnectedLayer fc(rnnoise::kInputLayerInputSize,
                         rnnoise::kInputLayerOutputSize,
                         rnnoise::kInputDenseBias, rnnoise::kInputDenseWeights,
                         rnnoise::TansigApproximated, Optimization::kNone);
  TestFullyConnectedLayer(&fc, kFullyConnectedInputVector,
                          kFullyConnectedExpectedOutput);
}

// Checks that the output of a GRU layer is within tolerance given test input
// data.
TEST(RnnVadTest, CheckGatedRecurrentLayer) {
  GatedRecurrentLayer gru(kGruInputSize, kGruOutputSize, kGruBias, kGruWeights,
                          kGruRecurrentWeights, Optimization::kNone);
  TestGatedRecurrentLayer(&gru, kGruInputSequence, kGruExpectedOutputSequence);
}

#if defined(WEBRTC_ARCH_X86_FAMILY)

// Like CheckFullyConnectedLayerOutput, but testing the SSE2 implementation.
TEST(RnnVadTest, CheckFullyConnectedLayerOutputSse2) {
  if (!IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }

  FullyConnectedLayer fc(rnnoise::kInputLayerInputSize,
                         rnnoise::kInputLayerOutputSize,
                         rnnoise::kInputDenseBias, rnnoise::kInputDenseWeights,
                         rnnoise::TansigApproximated, Optimization::kSse2);
  TestFullyConnectedLayer(&fc, kFullyConnectedInputVector,
                          kFullyConnectedExpectedOutput);
}

// Like CheckGatedRecurrentLayer, but testing the SSE2 implementation.
TEST(RnnVadTest, CheckGatedRecurrentLayerSse2) {
  if (!IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }

  GatedRecurrentLayer gru(kGruInputSize, kGruOutputSize, kGruBias, kGruWeights,
                          kGruRecurrentWeights, Optimization::kSse2);
  TestGatedRecurrentLayer(&gru, kGruInputSequence, kGruExpectedOutputSequence);
}

#endif  // WEBRTC_ARCH_X86_FAMILY

TEST(RnnVadTest, DISABLED_BenchmarkFullyConnectedLayer) {
  std::vector<std::unique_ptr<FullyConnectedLayer>> implementations;
  implementations.emplace_back(std::make_unique<FullyConnectedLayer>(
      rnnoise::kInputLayerInputSize, rnnoise::kInputLayerOutputSize,
      rnnoise::kInputDenseBias, rnnoise::kInputDenseWeights,
      rnnoise::TansigApproximated, Optimization::kNone));
  if (IsOptimizationAvailable(Optimization::kSse2)) {
    implementations.emplace_back(std::make_unique<FullyConnectedLayer>(
        rnnoise::kInputLayerInputSize, rnnoise::kInputLayerOutputSize,
        rnnoise::kInputDenseBias, rnnoise::kInputDenseWeights,
        rnnoise::TansigApproximated, Optimization::kSse2));
  }

  std::vector<Result> results;
  constexpr size_t number_of_tests = 10000;
  for (auto& fc : implementations) {
    ::webrtc::test::PerformanceTimer perf_timer(number_of_tests);
    for (size_t k = 0; k < number_of_tests; ++k) {
      perf_timer.StartTimer();
      fc->ComputeOutput(kFullyConnectedInputVector);
      perf_timer.StopTimer();
    }
    results.push_back({fc->optimization(), perf_timer.GetDurationAverage(),
                       perf_timer.GetDurationStandardDeviation()});
  }

  for (const auto& result : results) {
    RTC_LOG(LS_INFO) << GetOptimizationName(result.optimization) << ": "
                     << (result.average_us / 1e3) << " +/- "
                     << (result.std_dev_us / 1e3) << " ms";
  }
}

TEST(RnnVadTest, DISABLED_BenchmarkGatedRecurrentLayer) {
  std::vector<std::unique_ptr<GatedRecurrentLayer>> implementations;
  implementations.emplace_back(std::make_unique<GatedRecurrentLayer>(
      kGruInputSize, kGruOutputSize, kGruBias, kGruWeights,
      kGruRecurrentWeights, Optimization::kNone));

  rtc::ArrayView<const float> input_sequence(kGruInputSequence);
  static_assert(kGruInputSequence.size() % kGruInputSize == 0, "");
  constexpr size_t input_sequence_length =
      kGruInputSequence.size() / kGruInputSize;

  std::vector<Result> results;
  constexpr size_t number_of_tests = 10000;
  for (auto& gru : implementations) {
    ::webrtc::test::PerformanceTimer perf_timer(number_of_tests);
    gru->Reset();
    for (size_t k = 0; k < number_of_tests; ++k) {
      perf_timer.StartTimer();
      for (size_t i = 0; i < input_sequence_length; ++i) {
        gru->ComputeOutput(
            input_sequence.subview(i * gru->input_size(), gru->input_size()));
      }
      perf_timer.StopTimer();
    }
    results.push_back({gru->optimization(), perf_timer.GetDurationAverage(),
                       perf_timer.GetDurationStandardDeviation()});
  }

  for (const auto& result : results) {
    RTC_LOG(LS_INFO) << GetOptimizationName(result.optimization) << ": "
                     << (result.average_us / 1e3) << " +/- "
                     << (result.std_dev_us / 1e3) << " ms";
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
