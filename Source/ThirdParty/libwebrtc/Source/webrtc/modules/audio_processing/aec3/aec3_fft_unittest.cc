/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec3_fft.h"

#include <algorithm>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null input in Fft works.
TEST(Aec3Fft, NullFftInput) {
  Aec3Fft fft;
  FftData X;
  EXPECT_DEATH(fft.Fft(nullptr, &X), "");
}

// Verifies that the check for non-null input in Fft works.
TEST(Aec3Fft, NullFftOutput) {
  Aec3Fft fft;
  std::array<float, kFftLength> x;
  EXPECT_DEATH(fft.Fft(&x, nullptr), "");
}

// Verifies that the check for non-null output in Ifft works.
TEST(Aec3Fft, NullIfftOutput) {
  Aec3Fft fft;
  FftData X;
  EXPECT_DEATH(fft.Ifft(X, nullptr), "");
}

// Verifies that the check for non-null output in ZeroPaddedFft works.
TEST(Aec3Fft, NullZeroPaddedFftOutput) {
  Aec3Fft fft;
  std::array<float, kFftLengthBy2> x;
  EXPECT_DEATH(fft.ZeroPaddedFft(x, nullptr), "");
}

// Verifies that the check for input length in ZeroPaddedFft works.
TEST(Aec3Fft, ZeroPaddedFftWrongInputLength) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2 - 1> x;
  EXPECT_DEATH(fft.ZeroPaddedFft(x, &X), "");
}

// Verifies that the check for non-null output in PaddedFft works.
TEST(Aec3Fft, NullPaddedFftOutput) {
  Aec3Fft fft;
  std::array<float, kFftLengthBy2> x;
  std::array<float, kFftLengthBy2> x_old;
  EXPECT_DEATH(fft.PaddedFft(x, x_old, nullptr), "");
}

// Verifies that the check for input length in PaddedFft works.
TEST(Aec3Fft, PaddedFftWrongInputLength) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2 - 1> x;
  std::array<float, kFftLengthBy2> x_old;
  EXPECT_DEATH(fft.PaddedFft(x, x_old, &X), "");
}

// Verifies that the check for length in the old value in PaddedFft works.
TEST(Aec3Fft, PaddedFftWrongOldValuesLength) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2> x;
  std::array<float, kFftLengthBy2 - 1> x_old;
  EXPECT_DEATH(fft.PaddedFft(x, x_old, &X), "");
}

#endif

// Verifies that Fft works as intended.
TEST(Aec3Fft, Fft) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLength> x;
  x.fill(0.f);
  fft.Fft(&x, &X);
  EXPECT_THAT(X.re, ::testing::Each(0.f));
  EXPECT_THAT(X.im, ::testing::Each(0.f));

  x.fill(0.f);
  x[0] = 1.f;
  fft.Fft(&x, &X);
  EXPECT_THAT(X.re, ::testing::Each(1.f));
  EXPECT_THAT(X.im, ::testing::Each(0.f));

  x.fill(1.f);
  fft.Fft(&x, &X);
  EXPECT_EQ(128.f, X.re[0]);
  std::for_each(X.re.begin() + 1, X.re.end(),
                [](float a) { EXPECT_EQ(0.f, a); });
  EXPECT_THAT(X.im, ::testing::Each(0.f));
}

// Verifies that InverseFft works as intended.
TEST(Aec3Fft, Ifft) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLength> x;

  X.re.fill(0.f);
  X.im.fill(0.f);
  fft.Ifft(X, &x);
  EXPECT_THAT(x, ::testing::Each(0.f));

  X.re.fill(1.f);
  X.im.fill(0.f);
  fft.Ifft(X, &x);
  EXPECT_EQ(64.f, x[0]);
  std::for_each(x.begin() + 1, x.end(), [](float a) { EXPECT_EQ(0.f, a); });

  X.re.fill(0.f);
  X.re[0] = 128;
  X.im.fill(0.f);
  fft.Ifft(X, &x);
  EXPECT_THAT(x, ::testing::Each(64.f));
}

// Verifies that InverseFft and Fft work as intended.
TEST(Aec3Fft, FftAndIfft) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLength> x;
  std::array<float, kFftLength> x_ref;

  int v = 0;
  for (int k = 0; k < 20; ++k) {
    for (size_t j = 0; j < x.size(); ++j) {
      x[j] = v++;
      x_ref[j] = x[j] * 64.f;
    }
    fft.Fft(&x, &X);
    fft.Ifft(X, &x);
    for (size_t j = 0; j < x.size(); ++j) {
      EXPECT_NEAR(x_ref[j], x[j], 0.001f);
    }
  }
}

// Verifies that ZeroPaddedFft work as intended.
TEST(Aec3Fft, ZeroPaddedFft) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2> x_in;
  std::array<float, kFftLength> x_ref;
  std::array<float, kFftLength> x_out;

  int v = 0;
  x_ref.fill(0.f);
  for (int k = 0; k < 20; ++k) {
    for (size_t j = 0; j < x_in.size(); ++j) {
      x_in[j] = v++;
      x_ref[j + kFftLengthBy2] = x_in[j] * 64.f;
    }
    fft.ZeroPaddedFft(x_in, &X);
    fft.Ifft(X, &x_out);
    for (size_t j = 0; j < x_out.size(); ++j) {
      EXPECT_NEAR(x_ref[j], x_out[j], 0.1f);
    }
  }
}

// Verifies that ZeroPaddedFft work as intended.
TEST(Aec3Fft, PaddedFft) {
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2> x_in;
  std::array<float, kFftLength> x_out;
  std::array<float, kFftLengthBy2> x_old;
  std::array<float, kFftLengthBy2> x_old_ref;
  std::array<float, kFftLength> x_ref;

  int v = 0;
  x_old.fill(0.f);
  for (int k = 0; k < 20; ++k) {
    for (size_t j = 0; j < x_in.size(); ++j) {
      x_in[j] = v++;
    }

    std::copy(x_old.begin(), x_old.end(), x_ref.begin());
    std::copy(x_in.begin(), x_in.end(), x_ref.begin() + kFftLengthBy2);
    std::copy(x_in.begin(), x_in.end(), x_old_ref.begin());
    std::for_each(x_ref.begin(), x_ref.end(), [](float& a) { a *= 64.f; });

    fft.PaddedFft(x_in, x_old, &X);
    fft.Ifft(X, &x_out);

    for (size_t j = 0; j < x_out.size(); ++j) {
      EXPECT_NEAR(x_ref[j], x_out[j], 0.1f);
    }

    EXPECT_EQ(x_old_ref, x_old);
  }
}

}  // namespace webrtc
