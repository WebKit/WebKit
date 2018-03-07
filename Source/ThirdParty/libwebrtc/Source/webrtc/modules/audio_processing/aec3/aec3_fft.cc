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

// TODO(peah): Change x to be std::array once the rest of the code allows this.
void Aec3Fft::ZeroPaddedFft(rtc::ArrayView<const float> x, FftData* X) const {
  RTC_DCHECK(X);
  RTC_DCHECK_EQ(kFftLengthBy2, x.size());
  std::array<float, kFftLength> fft;
  std::fill(fft.begin(), fft.begin() + kFftLengthBy2, 0.f);
  std::copy(x.begin(), x.end(), fft.begin() + kFftLengthBy2);
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
