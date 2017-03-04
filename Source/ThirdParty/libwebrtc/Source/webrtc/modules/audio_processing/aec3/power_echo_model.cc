/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/power_echo_model.h"

#include <string.h>
#include <algorithm>

#include "webrtc/base/optional.h"

namespace webrtc {
namespace {

// Computes the spectral power over that last 20 frames.
void RecentMaximum(const FftBuffer& X_buffer,
                   std::array<float, kFftLengthBy2Plus1>* R2) {
  R2->fill(0.f);
  for (size_t j = 0; j < 20; ++j) {
    std::transform(R2->begin(), R2->end(), X_buffer.Spectrum(j).begin(),
                   R2->begin(),
                   [](float a, float b) { return std::max(a, b); });
  }
}

constexpr float kHInitial = 10.f;
constexpr int kUpdateCounterInitial = 300;

}  // namespace

PowerEchoModel::PowerEchoModel() {
  H2_.fill(CountedFloat(kHInitial, kUpdateCounterInitial));
}

PowerEchoModel::~PowerEchoModel() = default;

void PowerEchoModel::HandleEchoPathChange(
    const EchoPathVariability& variability) {
  if (variability.gain_change) {
    H2_.fill(CountedFloat(kHInitial, kUpdateCounterInitial));
  }
}

void PowerEchoModel::EstimateEcho(
    const FftBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& capture_spectrum,
    const AecState& aec_state,
    std::array<float, kFftLengthBy2Plus1>* echo_spectrum) {
  RTC_DCHECK(echo_spectrum);

  const FftBuffer& X_buffer = render_buffer;
  const auto& Y2 = capture_spectrum;
  std::array<float, kFftLengthBy2Plus1>* S2 = echo_spectrum;

  // Choose delay to use.
  const rtc::Optional<size_t> delay =
      aec_state.FilterDelay()
          ? aec_state.FilterDelay()
          : (aec_state.ExternalDelay() ? rtc::Optional<size_t>(std::min<size_t>(
                                             *aec_state.ExternalDelay(),
                                             X_buffer.Buffer().size() - 1))
                                       : rtc::Optional<size_t>());

  // Compute R2.
  std::array<float, kFftLengthBy2Plus1> render_max;
  if (!delay) {
    RecentMaximum(render_buffer, &render_max);
  }
  const std::array<float, kFftLengthBy2Plus1>& X2_active =
      delay ? render_buffer.Spectrum(*delay) : render_max;

  if (!aec_state.SaturatedCapture()) {
    // Corresponds of WGN of power -46dBFS.
    constexpr float kX2Min = 44015068.0f;
    const int max_update_counter_value = delay ? 300 : 500;

    std::array<float, kFftLengthBy2Plus1> new_H2;

    // new_H2 = Y2 / X2.
    std::transform(X2_active.begin(), X2_active.end(), Y2.begin(),
                   new_H2.begin(),
                   [&](float a, float b) { return a > kX2Min ? b / a : -1.f; });

    // Lambda for updating H2 in a maximum statistics manner.
    auto H2_updater = [&](float a, CountedFloat b) {
      if (a > 0) {
        if (a > b.value) {
          b.counter = max_update_counter_value;
          b.value = a;
        } else if (--b.counter <= 0) {
          b.value = std::max(b.value * 0.9f, 1.f);
        }
      }
      return b;
    };

    std::transform(new_H2.begin(), new_H2.end(), H2_.begin(), H2_.begin(),
                   H2_updater);
  }

  // S2 = H2*X2_active.
  std::transform(H2_.begin(), H2_.end(), X2_active.begin(), S2->begin(),
                 [](CountedFloat a, float b) { return a.value * b; });
}

}  // namespace webrtc
