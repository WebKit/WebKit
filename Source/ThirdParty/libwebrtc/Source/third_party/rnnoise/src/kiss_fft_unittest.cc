// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/rnnoise/src/kiss_fft.h"

namespace rnnoise {
namespace test {
namespace {

const double kPi = std::acos(-1.0);

void FillFftInputBuffer(const size_t num_samples,
                        const float* samples,
                        std::complex<float>* input_buf) {
  for (size_t i = 0; i < num_samples; ++i)
    input_buf[i].real(samples[i]);
}

void CheckFftResult(const size_t num_fft_points,
                    const float* expected_real,
                    const float* expected_imag,
                    const std::complex<float>* computed,
                    const float tolerance) {
  for (size_t i = 0; i < num_fft_points; ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(expected_real[i], computed[i].real(), tolerance);
    EXPECT_NEAR(expected_imag[i], computed[i].imag(), tolerance);
  }
}

}  // namespace

class RnnVadTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, float, float>> {};

// Check that IFFT(FFT(x)) == x (tolerating round-off errors).
TEST_P(RnnVadTest, KissFftForwardReverseCheckIdentity) {
  const auto params = GetParam();
  const float amplitude = std::get<0>(params);
  const size_t num_fft = std::get<1>(params);
  const float tolerance = std::get<2>(params);
  std::vector<float> samples;
  std::vector<float> zeros;
  samples.resize(num_fft);
  zeros.resize(num_fft);
  for (size_t i = 0; i < num_fft; ++i) {
    samples[i] = amplitude * std::sin(2.f * kPi * 10 * i / num_fft);
    zeros[i] = 0.f;
  }

  KissFft fft(num_fft);
  std::vector<std::complex<float>> fft_buf_1;
  fft_buf_1.resize(num_fft);
  std::vector<std::complex<float>> fft_buf_2;
  fft_buf_2.resize(num_fft);

  FillFftInputBuffer(samples.size(), samples.data(), fft_buf_1.data());
  {
    // TODO(alessiob): Underflow with non power of 2 frame sizes.
    // FloatingPointExceptionObserver fpe_observer;

    fft.ForwardFft(fft_buf_1.size(), fft_buf_1.data(), fft_buf_2.size(),
                   fft_buf_2.data());
    fft.ReverseFft(fft_buf_2.size(), fft_buf_2.data(), fft_buf_1.size(),
                   fft_buf_1.data());
  }
  CheckFftResult(samples.size(), samples.data(), zeros.data(), fft_buf_1.data(),
                 tolerance);
}

INSTANTIATE_TEST_CASE_P(FftPoints,
                        RnnVadTest,
                        ::testing::Values(std::make_tuple(1.f, 240, 3e-7f),
                                          std::make_tuple(1.f, 256, 3e-7f),
                                          std::make_tuple(1.f, 480, 3e-7f),
                                          std::make_tuple(1.f, 512, 3e-7f),
                                          std::make_tuple(1.f, 960, 4e-7f),
                                          std::make_tuple(1.f, 1024, 3e-7f),
                                          std::make_tuple(30.f, 240, 5e-6f),
                                          std::make_tuple(30.f, 256, 5e-6f),
                                          std::make_tuple(30.f, 480, 6e-6f),
                                          std::make_tuple(30.f, 512, 6e-6f),
                                          std::make_tuple(30.f, 960, 8e-6f),
                                          std::make_tuple(30.f, 1024, 6e-6f)));

TEST(RnnVadTest, KissFftBitExactness) {
  constexpr std::array<float, 32> samples = {
      {0.3524301946163177490234375f,  0.891803801059722900390625f,
       0.07706542313098907470703125f, 0.699530780315399169921875f,
       0.3789891898632049560546875f,  0.5438187122344970703125f,
       0.332781612873077392578125f,   0.449340641498565673828125f,
       0.105229437351226806640625f,   0.722373783588409423828125f,
       0.13155306875705718994140625f, 0.340857982635498046875f,
       0.970204889774322509765625f,   0.53061950206756591796875f,
       0.91507828235626220703125f,    0.830274522304534912109375f,
       0.74468600749969482421875f,    0.24320767819881439208984375f,
       0.743998110294342041015625f,   0.17574800550937652587890625f,
       0.1834825575351715087890625f,  0.63317775726318359375f,
       0.11414264142513275146484375f, 0.1612723171710968017578125f,
       0.80316197872161865234375f,    0.4979794919490814208984375f,
       0.554282128810882568359375f,   0.67189347743988037109375f,
       0.06660757958889007568359375f, 0.89568817615509033203125f,
       0.29327380657196044921875f,    0.3472573757171630859375f}};
  constexpr std::array<float, 17> expected_real = {
      {0.4813065826892852783203125f, -0.0246877372264862060546875f,
       0.04095232486724853515625f, -0.0401695556938648223876953125f,
       0.00500857271254062652587890625f, 0.0160773508250713348388671875f,
       -0.011385642923414707183837890625f, -0.008461721241474151611328125f,
       0.01383177936077117919921875f, 0.0117270611226558685302734375f,
       -0.0164460353553295135498046875f, 0.0585579685866832733154296875f,
       0.02038039825856685638427734375f, -0.0209107734262943267822265625f,
       0.01046995259821414947509765625f, -0.09019653499126434326171875f,
       -0.0583711564540863037109375f}};
  constexpr std::array<float, 17> expected_imag = {
      {0.f, -0.010482530109584331512451171875f, 0.04762755334377288818359375f,
       -0.0558677613735198974609375f, 0.007908363826572895050048828125f,
       -0.0071932487189769744873046875f, 0.01322011835873126983642578125f,
       -0.011227893643081188201904296875f, -0.0400779247283935546875f,
       -0.0290451310575008392333984375f, 0.01519204117357730865478515625f,
       -0.09711246192455291748046875f, -0.00136523949913680553436279296875f,
       0.038602568209171295166015625f, -0.009693108499050140380859375f,
       -0.0183933563530445098876953125f, 0.f}};

  KissFft fft(32);
  std::array<std::complex<float>, 32> fft_buf_in;
  std::array<std::complex<float>, 32> fft_buf_out;
  FillFftInputBuffer(samples.size(), samples.data(), fft_buf_in.data());
  fft.ForwardFft(fft_buf_in.size(), fft_buf_in.data(), fft_buf_out.size(),
                 fft_buf_out.data());
  CheckFftResult(expected_real.size(), expected_real.data(),
                 expected_imag.data(), fft_buf_out.data(),
                 std::numeric_limits<float>::min());
}

}  // namespace test
}  // namespace rnnoise
