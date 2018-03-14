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

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

const float kHanning64[kFftLengthBy2] = {
    0.f,         0.00248461f, 0.00991376f, 0.0222136f,  0.03926189f,
    0.06088921f, 0.08688061f, 0.11697778f, 0.15088159f, 0.1882551f,
    0.22872687f, 0.27189467f, 0.31732949f, 0.36457977f, 0.41317591f,
    0.46263495f, 0.51246535f, 0.56217185f, 0.61126047f, 0.65924333f,
    0.70564355f, 0.75f,       0.79187184f, 0.83084292f, 0.86652594f,
    0.89856625f, 0.92664544f, 0.95048443f, 0.96984631f, 0.98453864f,
    0.99441541f, 0.99937846f, 0.99937846f, 0.99441541f, 0.98453864f,
    0.96984631f, 0.95048443f, 0.92664544f, 0.89856625f, 0.86652594f,
    0.83084292f, 0.79187184f, 0.75f,       0.70564355f, 0.65924333f,
    0.61126047f, 0.56217185f, 0.51246535f, 0.46263495f, 0.41317591f,
    0.36457977f, 0.31732949f, 0.27189467f, 0.22872687f, 0.1882551f,
    0.15088159f, 0.11697778f, 0.08688061f, 0.06088921f, 0.03926189f,
    0.0222136f,  0.00991376f, 0.00248461f, 0.f};

}  // namespace

// TODO(peah): Change x to be std::array once the rest of the code allows this.
void Aec3Fft::ZeroPaddedFft(rtc::ArrayView<const float> x,
                            Window window,
                            FftData* X) const {
  RTC_DCHECK(X);
  RTC_DCHECK_EQ(kFftLengthBy2, x.size());
  std::array<float, kFftLength> fft;
  std::fill(fft.begin(), fft.begin() + kFftLengthBy2, 0.f);
  switch (window) {
    case Window::kRectangular:
      std::copy(x.begin(), x.end(), fft.begin() + kFftLengthBy2);
      break;
    case Window::kHanning:
      std::transform(x.begin(), x.end(), std::begin(kHanning64),
                     fft.begin() + kFftLengthBy2,
                     [](float a, float b) { return a * b; });
      break;
    default:
      RTC_NOTREACHED();
  }

  Fft(&fft, X);
}

void Aec3Fft::PaddedFft(rtc::ArrayView<const float> x,
                        rtc::ArrayView<float> x_old,
                        FftData* X) const {
  RTC_DCHECK(X);
  RTC_DCHECK_EQ(kFftLengthBy2, x.size());
  RTC_DCHECK_EQ(kFftLengthBy2, x_old.size());
  std::array<float, kFftLength> fft;
  std::copy(x_old.begin(), x_old.end(), fft.begin());
  std::copy(x.begin(), x.end(), fft.begin() + x_old.size());
  std::copy(x.begin(), x.end(), x_old.begin());
  Fft(&fft, X);
}

}  // namespace webrtc
