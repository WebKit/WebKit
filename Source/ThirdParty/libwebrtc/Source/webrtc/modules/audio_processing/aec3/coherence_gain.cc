/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/coherence_gain.h"

#include <math.h>

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Matlab code to produce table:
// overDriveCurve = [sqrt(linspace(0,1,65))' + 1];
// fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', overDriveCurve);
const float kOverDriveCurve[kFftLengthBy2Plus1] = {
    1.0000f, 1.1250f, 1.1768f, 1.2165f, 1.2500f, 1.2795f, 1.3062f, 1.3307f,
    1.3536f, 1.3750f, 1.3953f, 1.4146f, 1.4330f, 1.4507f, 1.4677f, 1.4841f,
    1.5000f, 1.5154f, 1.5303f, 1.5449f, 1.5590f, 1.5728f, 1.5863f, 1.5995f,
    1.6124f, 1.6250f, 1.6374f, 1.6495f, 1.6614f, 1.6731f, 1.6847f, 1.6960f,
    1.7071f, 1.7181f, 1.7289f, 1.7395f, 1.7500f, 1.7603f, 1.7706f, 1.7806f,
    1.7906f, 1.8004f, 1.8101f, 1.8197f, 1.8292f, 1.8385f, 1.8478f, 1.8570f,
    1.8660f, 1.8750f, 1.8839f, 1.8927f, 1.9014f, 1.9100f, 1.9186f, 1.9270f,
    1.9354f, 1.9437f, 1.9520f, 1.9601f, 1.9682f, 1.9763f, 1.9843f, 1.9922f,
    2.0000f};

// Matlab code to produce table:
// weightCurve = [0 ; 0.3 * sqrt(linspace(0,1,64))' + 0.1];
// fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', weightCurve);
const float kWeightCurve[kFftLengthBy2Plus1] = {
    0.0000f, 0.1000f, 0.1378f, 0.1535f, 0.1655f, 0.1756f, 0.1845f, 0.1926f,
    0.2000f, 0.2069f, 0.2134f, 0.2195f, 0.2254f, 0.2309f, 0.2363f, 0.2414f,
    0.2464f, 0.2512f, 0.2558f, 0.2604f, 0.2648f, 0.2690f, 0.2732f, 0.2773f,
    0.2813f, 0.2852f, 0.2890f, 0.2927f, 0.2964f, 0.3000f, 0.3035f, 0.3070f,
    0.3104f, 0.3138f, 0.3171f, 0.3204f, 0.3236f, 0.3268f, 0.3299f, 0.3330f,
    0.3360f, 0.3390f, 0.3420f, 0.3449f, 0.3478f, 0.3507f, 0.3535f, 0.3563f,
    0.3591f, 0.3619f, 0.3646f, 0.3673f, 0.3699f, 0.3726f, 0.3752f, 0.3777f,
    0.3803f, 0.3828f, 0.3854f, 0.3878f, 0.3903f, 0.3928f, 0.3952f, 0.3976f,
    0.4000f};

int CmpFloat(const void* a, const void* b) {
  const float* da = static_cast<const float*>(a);
  const float* db = static_cast<const float*>(b);
  return (*da > *db) - (*da < *db);
}

}  // namespace

CoherenceGain::CoherenceGain(int sample_rate_hz, size_t num_bands_to_compute)
    : num_bands_to_compute_(num_bands_to_compute),
      sample_rate_scaler_(sample_rate_hz >= 16000 ? 2 : 1) {
  spectra_.Cye.Clear();
  spectra_.Cxy.Clear();
  spectra_.Pe.fill(0.f);
  // Initialize to 1 in order to prevent numerical instability in the first
  // block.
  spectra_.Py.fill(1.f);
  spectra_.Px.fill(1.f);
}

CoherenceGain::~CoherenceGain() = default;

void CoherenceGain::ComputeGain(const FftData& E,
                                const FftData& X,
                                const FftData& Y,
                                rtc::ArrayView<float> gain) {
  std::array<float, kFftLengthBy2Plus1> coherence_ye;
  std::array<float, kFftLengthBy2Plus1> coherence_xy;

  UpdateCoherenceSpectra(E, X, Y);
  ComputeCoherence(coherence_ye, coherence_xy);
  FormSuppressionGain(coherence_ye, coherence_xy, gain);
}

// Updates the following smoothed Power Spectral Densities (PSD):
//  - sd  : near-end
//  - se  : residual echo
//  - sx  : far-end
//  - sde : cross-PSD of near-end and residual echo
//  - sxd : cross-PSD of near-end and far-end
//
void CoherenceGain::UpdateCoherenceSpectra(const FftData& E,
                                           const FftData& X,
                                           const FftData& Y) {
  const float s = sample_rate_scaler_ == 1 ? 0.9f : 0.92f;
  const float one_minus_s = 1.f - s;
  auto& c = spectra_;

  for (size_t i = 0; i < c.Py.size(); i++) {
    c.Py[i] =
        s * c.Py[i] + one_minus_s * (Y.re[i] * Y.re[i] + Y.im[i] * Y.im[i]);
    c.Pe[i] =
        s * c.Pe[i] + one_minus_s * (E.re[i] * E.re[i] + E.im[i] * E.im[i]);
    // We threshold here to protect against the ill-effects of a zero farend.
    // The threshold is not arbitrarily chosen, but balances protection and
    // adverse interaction with the algorithm's tuning.

    // Threshold to protect against the ill-effects of a zero far-end.
    c.Px[i] =
        s * c.Px[i] +
        one_minus_s * std::max(X.re[i] * X.re[i] + X.im[i] * X.im[i], 15.f);

    c.Cye.re[i] =
        s * c.Cye.re[i] + one_minus_s * (Y.re[i] * E.re[i] + Y.im[i] * E.im[i]);
    c.Cye.im[i] =
        s * c.Cye.im[i] + one_minus_s * (Y.re[i] * E.im[i] - Y.im[i] * E.re[i]);

    c.Cxy.re[i] =
        s * c.Cxy.re[i] + one_minus_s * (Y.re[i] * X.re[i] + Y.im[i] * X.im[i]);
    c.Cxy.im[i] =
        s * c.Cxy.im[i] + one_minus_s * (Y.re[i] * X.im[i] - Y.im[i] * X.re[i]);
  }
}

void CoherenceGain::FormSuppressionGain(
    rtc::ArrayView<const float> coherence_ye,
    rtc::ArrayView<const float> coherence_xy,
    rtc::ArrayView<float> gain) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, coherence_ye.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, coherence_xy.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, gain.size());
  constexpr int kPrefBandSize = 24;
  auto& gs = gain_state_;
  std::array<float, kPrefBandSize> h_nl_pref;
  float h_nl_fb = 0;
  float h_nl_fb_low = 0;
  const int pref_band_size = kPrefBandSize / sample_rate_scaler_;
  const int min_pref_band = 4 / sample_rate_scaler_;

  float h_nl_de_avg = 0.f;
  float h_nl_xd_avg = 0.f;
  for (int i = min_pref_band; i < pref_band_size + min_pref_band; ++i) {
    h_nl_xd_avg += coherence_xy[i];
    h_nl_de_avg += coherence_ye[i];
  }
  h_nl_xd_avg /= pref_band_size;
  h_nl_xd_avg = 1 - h_nl_xd_avg;
  h_nl_de_avg /= pref_band_size;

  if (h_nl_xd_avg < 0.75f && h_nl_xd_avg < gs.h_nl_xd_avg_min) {
    gs.h_nl_xd_avg_min = h_nl_xd_avg;
  }

  if (h_nl_de_avg > 0.98f && h_nl_xd_avg > 0.9f) {
    gs.near_state = true;
  } else if (h_nl_de_avg < 0.95f || h_nl_xd_avg < 0.8f) {
    gs.near_state = false;
  }

  std::array<float, kFftLengthBy2Plus1> h_nl;
  if (gs.h_nl_xd_avg_min == 1) {
    gs.overdrive = 15.f;

    if (gs.near_state) {
      std::copy(coherence_ye.begin(), coherence_ye.end(), h_nl.begin());
      h_nl_fb = h_nl_de_avg;
      h_nl_fb_low = h_nl_de_avg;
    } else {
      for (size_t i = 0; i < h_nl.size(); ++i) {
        h_nl[i] = 1 - coherence_xy[i];
        h_nl[i] = std::max(h_nl[i], 0.f);
      }
      h_nl_fb = h_nl_xd_avg;
      h_nl_fb_low = h_nl_xd_avg;
    }
  } else {
    if (gs.near_state) {
      std::copy(coherence_ye.begin(), coherence_ye.end(), h_nl.begin());
      h_nl_fb = h_nl_de_avg;
      h_nl_fb_low = h_nl_de_avg;
    } else {
      for (size_t i = 0; i < h_nl.size(); ++i) {
        h_nl[i] = std::min(coherence_ye[i], 1 - coherence_xy[i]);
        h_nl[i] = std::max(h_nl[i], 0.f);
      }

      // Select an order statistic from the preferred bands.
      // TODO(peah): Using quicksort now, but a selection algorithm may be
      // preferred.
      std::copy(h_nl.begin() + min_pref_band,
                h_nl.begin() + min_pref_band + pref_band_size,
                h_nl_pref.begin());
      std::qsort(h_nl_pref.data(), pref_band_size, sizeof(float), CmpFloat);

      constexpr float kPrefBandQuant = 0.75f;
      h_nl_fb = h_nl_pref[static_cast<int>(
          floor(kPrefBandQuant * (pref_band_size - 1)))];
      constexpr float kPrefBandQuantLow = 0.5f;
      h_nl_fb_low = h_nl_pref[static_cast<int>(
          floor(kPrefBandQuantLow * (pref_band_size - 1)))];
    }
  }

  // Track the local filter minimum to determine suppression overdrive.
  if (h_nl_fb_low < 0.6f && h_nl_fb_low < gs.h_nl_fb_local_min) {
    gs.h_nl_fb_local_min = h_nl_fb_low;
    gs.h_nl_fb_min = h_nl_fb_low;
    gs.h_nl_new_min = 1;
    gs.h_nl_min_ctr = 0;
  }
  gs.h_nl_fb_local_min =
      std::min(gs.h_nl_fb_local_min + 0.0008f / sample_rate_scaler_, 1.f);
  gs.h_nl_xd_avg_min =
      std::min(gs.h_nl_xd_avg_min + 0.0006f / sample_rate_scaler_, 1.f);

  if (gs.h_nl_new_min == 1) {
    ++gs.h_nl_min_ctr;
  }
  if (gs.h_nl_min_ctr == 2) {
    gs.h_nl_new_min = 0;
    gs.h_nl_min_ctr = 0;
    constexpr float epsilon = 1e-10f;
    gs.overdrive = std::max(
        -18.4f / static_cast<float>(log(gs.h_nl_fb_min + epsilon) + epsilon),
        15.f);
  }

  // Smooth the overdrive.
  if (gs.overdrive < gs.overdrive_scaling) {
    gs.overdrive_scaling = 0.99f * gs.overdrive_scaling + 0.01f * gs.overdrive;
  } else {
    gs.overdrive_scaling = 0.9f * gs.overdrive_scaling + 0.1f * gs.overdrive;
  }

  // Apply the overdrive.
  RTC_DCHECK_LE(num_bands_to_compute_, gain.size());
  for (size_t i = 0; i < num_bands_to_compute_; ++i) {
    if (h_nl[i] > h_nl_fb) {
      h_nl[i] = kWeightCurve[i] * h_nl_fb + (1 - kWeightCurve[i]) * h_nl[i];
    }
    gain[i] = powf(h_nl[i], gs.overdrive_scaling * kOverDriveCurve[i]);
  }
}

void CoherenceGain::ComputeCoherence(rtc::ArrayView<float> coherence_ye,
                                     rtc::ArrayView<float> coherence_xy) const {
  const auto& c = spectra_;
  constexpr float epsilon = 1e-10f;
  for (size_t i = 0; i < coherence_ye.size(); ++i) {
    coherence_ye[i] = (c.Cye.re[i] * c.Cye.re[i] + c.Cye.im[i] * c.Cye.im[i]) /
                      (c.Py[i] * c.Pe[i] + epsilon);
    coherence_xy[i] = (c.Cxy.re[i] * c.Cxy.re[i] + c.Cxy.im[i] * c.Cxy.im[i]) /
                      (c.Px[i] * c.Py[i] + epsilon);
  }
}

}  // namespace webrtc
