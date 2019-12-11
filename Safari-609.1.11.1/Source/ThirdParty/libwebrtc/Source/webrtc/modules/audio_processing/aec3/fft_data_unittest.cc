/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/fft_data.h"

#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods are bitexact to their reference
// counterparts.
TEST(FftData, TestOptimizations) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    FftData x;

    for (size_t k = 0; k < x.re.size(); ++k) {
      x.re[k] = k + 1;
    }

    x.im[0] = x.im[x.im.size() - 1] = 0.f;
    for (size_t k = 1; k < x.im.size() - 1; ++k) {
      x.im[k] = 2.f * (k + 1);
    }

    std::array<float, kFftLengthBy2Plus1> spectrum;
    std::array<float, kFftLengthBy2Plus1> spectrum_sse2;
    x.Spectrum(Aec3Optimization::kNone, spectrum);
    x.Spectrum(Aec3Optimization::kSse2, spectrum_sse2);
    EXPECT_EQ(spectrum, spectrum_sse2);
  }
}
#endif

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for null output in CopyToPackedArray.
TEST(FftData, NonNullCopyToPackedArrayOutput) {
  EXPECT_DEATH(FftData().CopyToPackedArray(nullptr), "");
}

// Verifies the check for null output in Spectrum.
TEST(FftData, NonNullSpectrumOutput) {
  EXPECT_DEATH(FftData().Spectrum(Aec3Optimization::kNone, nullptr), "");
}

#endif

// Verifies that the Assign method properly copies the data from the source and
// ensures that the imaginary components for the DC and Nyquist bins are 0.
TEST(FftData, Assign) {
  FftData x;
  FftData y;

  x.re.fill(1.f);
  x.im.fill(2.f);
  y.Assign(x);
  EXPECT_EQ(x.re, y.re);
  EXPECT_EQ(0.f, y.im[0]);
  EXPECT_EQ(0.f, y.im[x.im.size() - 1]);
  for (size_t k = 1; k < x.im.size() - 1; ++k) {
    EXPECT_EQ(x.im[k], y.im[k]);
  }
}

// Verifies that the Clear method properly clears all the data.
TEST(FftData, Clear) {
  FftData x_ref;
  FftData x;

  x_ref.re.fill(0.f);
  x_ref.im.fill(0.f);

  x.re.fill(1.f);
  x.im.fill(2.f);
  x.Clear();

  EXPECT_EQ(x_ref.re, x.re);
  EXPECT_EQ(x_ref.im, x.im);
}

// Verifies that the spectrum is correctly computed.
TEST(FftData, Spectrum) {
  FftData x;

  for (size_t k = 0; k < x.re.size(); ++k) {
    x.re[k] = k + 1;
  }

  x.im[0] = x.im[x.im.size() - 1] = 0.f;
  for (size_t k = 1; k < x.im.size() - 1; ++k) {
    x.im[k] = 2.f * (k + 1);
  }

  std::array<float, kFftLengthBy2Plus1> spectrum;
  x.Spectrum(Aec3Optimization::kNone, spectrum);

  EXPECT_EQ(x.re[0] * x.re[0], spectrum[0]);
  EXPECT_EQ(x.re[spectrum.size() - 1] * x.re[spectrum.size() - 1],
            spectrum[spectrum.size() - 1]);
  for (size_t k = 1; k < spectrum.size() - 1; ++k) {
    EXPECT_EQ(x.re[k] * x.re[k] + x.im[k] * x.im[k], spectrum[k]);
  }
}

// Verifies that the functionality in CopyToPackedArray works as intended.
TEST(FftData, CopyToPackedArray) {
  FftData x;
  std::array<float, kFftLength> x_packed;

  for (size_t k = 0; k < x.re.size(); ++k) {
    x.re[k] = k + 1;
  }

  x.im[0] = x.im[x.im.size() - 1] = 0.f;
  for (size_t k = 1; k < x.im.size() - 1; ++k) {
    x.im[k] = 2.f * (k + 1);
  }

  x.CopyToPackedArray(&x_packed);

  EXPECT_EQ(x.re[0], x_packed[0]);
  EXPECT_EQ(x.re[x.re.size() - 1], x_packed[1]);
  for (size_t k = 1; k < x_packed.size() / 2; ++k) {
    EXPECT_EQ(x.re[k], x_packed[2 * k]);
    EXPECT_EQ(x.im[k], x_packed[2 * k + 1]);
  }
}

// Verifies that the functionality in CopyFromPackedArray works as intended
// (relies on that the functionality in CopyToPackedArray has been verified in
// the test above).
TEST(FftData, CopyFromPackedArray) {
  FftData x_ref;
  FftData x;
  std::array<float, kFftLength> x_packed;

  for (size_t k = 0; k < x_ref.re.size(); ++k) {
    x_ref.re[k] = k + 1;
  }

  x_ref.im[0] = x_ref.im[x_ref.im.size() - 1] = 0.f;
  for (size_t k = 1; k < x_ref.im.size() - 1; ++k) {
    x_ref.im[k] = 2.f * (k + 1);
  }

  x_ref.CopyToPackedArray(&x_packed);
  x.CopyFromPackedArray(x_packed);

  EXPECT_EQ(x_ref.re, x.re);
  EXPECT_EQ(x_ref.im, x.im);
}

}  // namespace webrtc
