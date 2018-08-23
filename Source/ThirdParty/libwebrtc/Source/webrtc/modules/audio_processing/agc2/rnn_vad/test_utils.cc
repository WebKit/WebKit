/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

using ReaderPairType =
    std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>;

}  // namespace

using webrtc::test::ResourcePath;

void ExpectEqualFloatArray(rtc::ArrayView<const float> expected,
                           rtc::ArrayView<const float> computed) {
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FLOAT_EQ(expected[i], computed[i]);
  }
}

void ExpectNearAbsolute(rtc::ArrayView<const float> expected,
                        rtc::ArrayView<const float> computed,
                        float tolerance) {
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(expected[i], computed[i], tolerance);
  }
}

std::unique_ptr<BinaryFileReader<float>> CreatePitchSearchTestDataReader() {
  constexpr size_t cols = 1396;
  return absl::make_unique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_search_int", "dat"),
      cols);
}

std::pair<std::unique_ptr<BinaryFileReader<int16_t, float>>, const size_t>
CreatePcmSamplesReader(const size_t frame_length) {
  auto ptr = absl::make_unique<BinaryFileReader<int16_t, float>>(
      test::ResourcePath("audio_processing/agc2/rnn_vad/samples", "pcm"),
      frame_length);
  // The last incomplete frame is ignored.
  return {std::move(ptr), ptr->data_length() / frame_length};
}

ReaderPairType CreatePitchBuffer24kHzReader() {
  constexpr size_t cols = 864;
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_buf_24k", "dat"), cols);
  return {std::move(ptr), rtc::CheckedDivExact(ptr->data_length(), cols)};
}

ReaderPairType CreateLpResidualAndPitchPeriodGainReader() {
  constexpr size_t num_lp_residual_coeffs = 864;
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_lp_res", "dat"),
      num_lp_residual_coeffs);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), 2 + num_lp_residual_coeffs)};
}

ReaderPairType CreateFftCoeffsReader() {
  constexpr size_t num_fft_points = 481;
  constexpr size_t row_size = 2 * num_fft_points;  // Real and imaginary values.
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      test::ResourcePath("audio_processing/agc2/rnn_vad/fft", "dat"),
      num_fft_points);
  return {std::move(ptr), rtc::CheckedDivExact(ptr->data_length(), row_size)};
}

ReaderPairType CreateBandEnergyCoeffsReader() {
  constexpr size_t num_bands = 22;
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      test::ResourcePath("audio_processing/agc2/rnn_vad/band_energies", "dat"),
      num_bands);
  return {std::move(ptr), rtc::CheckedDivExact(ptr->data_length(), num_bands)};
}

ReaderPairType CreateSilenceFlagsFeatureMatrixReader() {
  constexpr size_t feature_vector_size = 42;
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      test::ResourcePath("audio_processing/agc2/rnn_vad/sil_features", "dat"),
      feature_vector_size);
  // Features and silence flag.
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), feature_vector_size + 1)};
}

ReaderPairType CreateVadProbsReader() {
  auto ptr = absl::make_unique<BinaryFileReader<float>>(
      test::ResourcePath("audio_processing/agc2/rnn_vad/vad_prob", "dat"));
  return {std::move(ptr), ptr->data_length()};
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
