/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/rnn.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "third_party/rnnoise/src/rnn_activations.h"
#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

using rnnoise::RectifiedLinearUnit;
using rnnoise::SigmoidApproximated;

namespace {

void TestFullyConnectedLayer(FullyConnectedLayer* fc,
                             rtc::ArrayView<const float> input_vector,
                             const float expected_output) {
  RTC_CHECK(fc);
  fc->ComputeOutput(input_vector);
  const auto output = fc->GetOutput();
  EXPECT_NEAR(expected_output, output[0], 3e-6f);
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

}  // namespace

// Bit-exactness check for fully connected layers.
TEST(RnnVadTest, CheckFullyConnectedLayerOutput) {
  const std::array<int8_t, 1> bias = {-50};
  const std::array<int8_t, 24> weights = {
      127,  127,  127, 127,  127,  20,  127,  -126, -126, -54, 14,  125,
      -126, -126, 127, -125, -126, 127, -127, -127, -57,  -30, 127, 80};
  FullyConnectedLayer fc(24, 1, bias, weights, SigmoidApproximated);
  // Test on different inputs.
  {
    const std::array<float, 24> input_vector = {
        0.f,           0.f,           0.f,          0.f,          0.f,
        0.f,           0.215833917f,  0.290601075f, 0.238759011f, 0.244751841f,
        0.f,           0.0461241305f, 0.106401242f, 0.223070428f, 0.630603909f,
        0.690453172f,  0.f,           0.387645692f, 0.166913897f, 0.f,
        0.0327451192f, 0.f,           0.136149868f, 0.446351469f};
    TestFullyConnectedLayer(&fc, input_vector, 0.436567038f);
  }
  {
    const std::array<float, 24> input_vector = {
        0.592162728f,  0.529089332f,  1.18205106f,
        1.21736848f,   0.f,           0.470851123f,
        0.130675942f,  0.320903003f,  0.305496395f,
        0.0571633279f, 1.57001138f,   0.0182026215f,
        0.0977443159f, 0.347477973f,  0.493206412f,
        0.9688586f,    0.0320267938f, 0.244722098f,
        0.312745273f,  0.f,           0.00650715502f,
        0.312553257f,  1.62619662f,   0.782880902f};
    TestFullyConnectedLayer(&fc, input_vector, 0.874741316f);
  }
  {
    const std::array<float, 24> input_vector = {
        0.395022154f,  0.333681047f,  0.76302278f,
        0.965480626f,  0.f,           0.941198349f,
        0.0892967582f, 0.745046318f,  0.635769248f,
        0.238564298f,  0.970656633f,  0.014159563f,
        0.094203949f,  0.446816623f,  0.640755892f,
        1.20532358f,   0.0254284926f, 0.283327013f,
        0.726210058f,  0.0550272502f, 0.000344108557f,
        0.369803518f,  1.56680179f,   0.997883797f};
    TestFullyConnectedLayer(&fc, input_vector, 0.672785878f);
  }
}

TEST(RnnVadTest, CheckGatedRecurrentLayer) {
  const std::array<int8_t, 12> bias = {96,   -99, -81, -114, 49,  119,
                                       -118, 68,  -76, 91,   121, 125};
  const std::array<int8_t, 60> weights = {
      124, 9,    1,    116, -66, -21, -118, -110, 104,  75,  -23,  -51,
      -72, -111, 47,   93,  77,  -98, 41,   -8,   40,   -23, -43,  -107,
      9,   -73,  30,   -32, -2,  64,  -26,  91,   -48,  -24, -28,  -104,
      74,  -46,  116,  15,  32,  52,  -126, -38,  -121, 12,  -16,  110,
      -95, 66,   -103, -35, -38, 3,   -126, -61,  28,   98,  -117, -43};
  const std::array<int8_t, 60> recurrent_weights = {
      -3,  87,  50,  51,  -22,  27,  -39, 62,   31,  -83, -52,  -48,
      -6,  83,  -19, 104, 105,  48,  23,  68,   23,  40,  7,    -120,
      64,  -62, 117, 85,  -51,  -43, 54,  -105, 120, 56,  -128, -107,
      39,  50,  -17, -47, -117, 14,  108, 12,   -7,  -72, 103,  -87,
      -66, 82,  84,  100, -98,  102, -49, 44,   122, 106, -20,  -69};
  GatedRecurrentLayer gru(5, 4, bias, weights, recurrent_weights,
                          RectifiedLinearUnit);
  // Test on different inputs.
  {
    const std::array<float, 20> input_sequence = {
        0.89395463f, 0.93224651f, 0.55788344f, 0.32341808f, 0.93355054f,
        0.13475326f, 0.97370994f, 0.14253306f, 0.93710381f, 0.76093364f,
        0.65780413f, 0.41657975f, 0.49403164f, 0.46843281f, 0.75138855f,
        0.24517593f, 0.47657707f, 0.57064998f, 0.435184f,   0.19319285f};
    const std::array<float, 16> expected_output_sequence = {
        0.0239123f,  0.5773077f,  0.f,         0.f,
        0.01282811f, 0.64330572f, 0.f,         0.04863098f,
        0.00781069f, 0.75267816f, 0.f,         0.02579715f,
        0.00471378f, 0.59162533f, 0.11087593f, 0.01334511f};
    TestGatedRecurrentLayer(&gru, input_sequence, expected_output_sequence);
  }
}

// TODO(bugs.webrtc.org/9076): Remove when the issue is fixed.
// Bit-exactness test checking that precomputed frame-wise features lead to the
// expected VAD probabilities.
TEST(RnnVadTest, RnnBitExactness) {
  // Init.
  auto features_reader = CreateSilenceFlagsFeatureMatrixReader();
  auto vad_probs_reader = CreateVadProbsReader();
  ASSERT_EQ(features_reader.second, vad_probs_reader.second);
  const size_t num_frames = features_reader.second;
  // Frame-wise buffers.
  float expected_vad_probability;
  float is_silence;
  std::array<float, kFeatureVectorSize> features;

  // Compute VAD probability using the precomputed features.
  RnnBasedVad vad;
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read frame data.
    RTC_CHECK(vad_probs_reader.first->ReadValue(&expected_vad_probability));
    // The features file also includes a silence flag for each frame.
    RTC_CHECK(features_reader.first->ReadValue(&is_silence));
    RTC_CHECK(features_reader.first->ReadChunk(features));
    // Compute and check VAD probability.
    float vad_probability = vad.ComputeVadProbability(features, is_silence);
    ASSERT_TRUE(is_silence == 0.f || is_silence == 1.f);
    if (is_silence == 1.f) {
      ASSERT_EQ(0.f, expected_vad_probability);
      EXPECT_EQ(0.f, vad_probability);
    } else {
      EXPECT_NEAR(expected_vad_probability, vad_probability, 3e-6f);
    }
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
