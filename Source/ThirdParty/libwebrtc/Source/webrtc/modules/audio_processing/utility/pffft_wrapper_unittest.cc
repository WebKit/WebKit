/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/utility/pffft_wrapper.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "test/gtest.h"
#include "third_party/pffft/src/pffft.h"

namespace webrtc {
namespace test {
namespace {

constexpr size_t kMaxValidSizeCheck = 1024;

static constexpr int kFftSizes[] = {
    16,  32,      64,  96,  128,  160,  192,  256,  288,  384,   5 * 96, 512,
    576, 5 * 128, 800, 864, 1024, 2048, 2592, 4000, 4096, 12000, 36864};

void CreatePffftWrapper(size_t fft_size, Pffft::FftType fft_type) {
  Pffft pffft_wrapper(fft_size, fft_type);
}

float* AllocateScratchBuffer(size_t fft_size, bool complex_fft) {
  return static_cast<float*>(
      pffft_aligned_malloc(fft_size * (complex_fft ? 2 : 1) * sizeof(float)));
}

double frand() {
  return std::rand() / static_cast<double>(RAND_MAX);
}

void ExpectArrayViewsEquality(rtc::ArrayView<const float> a,
                              rtc::ArrayView<const float> b) {
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(a[i], b[i]);
  }
}

// Compares the output of the PFFFT C++ wrapper to that of the C PFFFT.
// Bit-exactness is expected.
void PffftValidateWrapper(size_t fft_size, bool complex_fft) {
  // Always use the same seed to avoid flakiness.
  std::srand(0);

  // Init PFFFT.
  PFFFT_Setup* pffft_status =
      pffft_new_setup(fft_size, complex_fft ? PFFFT_COMPLEX : PFFFT_REAL);
  ASSERT_TRUE(pffft_status) << "FFT size (" << fft_size << ") not supported.";
  size_t num_floats = fft_size * (complex_fft ? 2 : 1);
  int num_bytes = static_cast<int>(num_floats) * sizeof(float);
  float* in = static_cast<float*>(pffft_aligned_malloc(num_bytes));
  float* out = static_cast<float*>(pffft_aligned_malloc(num_bytes));
  float* scratch = AllocateScratchBuffer(fft_size, complex_fft);

  // Init PFFFT C++ wrapper.
  Pffft::FftType fft_type =
      complex_fft ? Pffft::FftType::kComplex : Pffft::FftType::kReal;
  ASSERT_TRUE(Pffft::IsValidFftSize(fft_size, fft_type));
  Pffft pffft_wrapper(fft_size, fft_type);
  auto in_wrapper = pffft_wrapper.CreateBuffer();
  auto out_wrapper = pffft_wrapper.CreateBuffer();

  // Input and output buffers views.
  rtc::ArrayView<float> in_view(in, num_floats);
  rtc::ArrayView<float> out_view(out, num_floats);
  auto in_wrapper_view = in_wrapper->GetView();
  EXPECT_EQ(in_wrapper_view.size(), num_floats);
  auto out_wrapper_view = out_wrapper->GetConstView();
  EXPECT_EQ(out_wrapper_view.size(), num_floats);

  // Random input data.
  for (size_t i = 0; i < num_floats; ++i) {
    in_wrapper_view[i] = in[i] = static_cast<float>(frand() * 2.0 - 1.0);
  }

  // Forward transform.
  pffft_transform(pffft_status, in, out, scratch, PFFFT_FORWARD);
  pffft_wrapper.ForwardTransform(*in_wrapper, out_wrapper.get(),
                                 /*ordered=*/false);
  ExpectArrayViewsEquality(out_view, out_wrapper_view);

  // Copy the FFT results into the input buffers to compute the backward FFT.
  std::copy(out_view.begin(), out_view.end(), in_view.begin());
  std::copy(out_wrapper_view.begin(), out_wrapper_view.end(),
            in_wrapper_view.begin());

  // Backward transform.
  pffft_transform(pffft_status, in, out, scratch, PFFFT_BACKWARD);
  pffft_wrapper.BackwardTransform(*in_wrapper, out_wrapper.get(),
                                  /*ordered=*/false);
  ExpectArrayViewsEquality(out_view, out_wrapper_view);

  pffft_destroy_setup(pffft_status);
  pffft_aligned_free(in);
  pffft_aligned_free(out);
  pffft_aligned_free(scratch);
}

}  // namespace

TEST(PffftTest, CreateWrapperWithValidSize) {
  for (size_t fft_size = 0; fft_size < kMaxValidSizeCheck; ++fft_size) {
    SCOPED_TRACE(fft_size);
    if (Pffft::IsValidFftSize(fft_size, Pffft::FftType::kReal)) {
      CreatePffftWrapper(fft_size, Pffft::FftType::kReal);
    }
    if (Pffft::IsValidFftSize(fft_size, Pffft::FftType::kComplex)) {
      CreatePffftWrapper(fft_size, Pffft::FftType::kComplex);
    }
  }
}

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

class PffftInvalidSizeDeathTest : public ::testing::Test,
                                  public ::testing::WithParamInterface<size_t> {
};

TEST_P(PffftInvalidSizeDeathTest, DoNotCreateRealWrapper) {
  size_t fft_size = GetParam();
  ASSERT_FALSE(Pffft::IsValidFftSize(fft_size, Pffft::FftType::kReal));
  EXPECT_DEATH(CreatePffftWrapper(fft_size, Pffft::FftType::kReal), "");
}

TEST_P(PffftInvalidSizeDeathTest, DoNotCreateComplexWrapper) {
  size_t fft_size = GetParam();
  ASSERT_FALSE(Pffft::IsValidFftSize(fft_size, Pffft::FftType::kComplex));
  EXPECT_DEATH(CreatePffftWrapper(fft_size, Pffft::FftType::kComplex), "");
}

INSTANTIATE_TEST_SUITE_P(PffftTest,
                         PffftInvalidSizeDeathTest,
                         ::testing::Values(17,
                                           33,
                                           65,
                                           97,
                                           129,
                                           161,
                                           193,
                                           257,
                                           289,
                                           385,
                                           481,
                                           513,
                                           577,
                                           641,
                                           801,
                                           865,
                                           1025));

#endif

// TODO(https://crbug.com/webrtc/9577): Enable once SIMD is always enabled.
TEST(PffftTest, DISABLED_CheckSimd) {
  EXPECT_TRUE(Pffft::IsSimdEnabled());
}

TEST(PffftTest, FftBitExactness) {
  for (int fft_size : kFftSizes) {
    SCOPED_TRACE(fft_size);
    if (fft_size != 16) {
      PffftValidateWrapper(fft_size, /*complex_fft=*/false);
    }
    PffftValidateWrapper(fft_size, /*complex_fft=*/true);
  }
}

}  // namespace test
}  // namespace webrtc
