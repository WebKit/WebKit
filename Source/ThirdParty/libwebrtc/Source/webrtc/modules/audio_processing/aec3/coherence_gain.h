/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_COHERENCE_GAIN_H_
#define MODULES_AUDIO_PROCESSING_AEC3_COHERENCE_GAIN_H_

#include <array>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Class for computing an echo suppression gain based on the coherence measure.
class CoherenceGain {
 public:
  CoherenceGain(int sample_rate_hz, size_t num_bands_to_compute);
  ~CoherenceGain();

  // Computes the gain based on the FFTs of the filter error output signal, the
  // render signal and the capture signal.
  void ComputeGain(const FftData& E,
                   const FftData& X,
                   const FftData& Y,
                   rtc::ArrayView<float> gain);

 private:
  struct {
    FftData Cye;
    FftData Cxy;
    std::array<float, kFftLengthBy2Plus1> Px;
    std::array<float, kFftLengthBy2Plus1> Py;
    std::array<float, kFftLengthBy2Plus1> Pe;
  } spectra_;

  struct {
    float h_nl_fb_min = 1;
    float h_nl_fb_local_min = 1;
    float h_nl_xd_avg_min = 1.f;
    int h_nl_new_min = 0;
    float h_nl_min_ctr = 0;
    float overdrive = 2;
    float overdrive_scaling = 2;
    bool near_state = false;
  } gain_state_;

  const Aec3Fft fft_;
  const size_t num_bands_to_compute_;
  const int sample_rate_scaler_;

  // Updates the spectral estimates used for the coherence computation.
  void UpdateCoherenceSpectra(const FftData& E,
                              const FftData& X,
                              const FftData& Y);

  // Compute the suppression gain based on the coherence.
  void FormSuppressionGain(rtc::ArrayView<const float> coherence_ye,
                           rtc::ArrayView<const float> coherence_xy,
                           rtc::ArrayView<float> h_nl);

  // Compute the coherence.
  void ComputeCoherence(rtc::ArrayView<float> coherence_ye,
                        rtc::ArrayView<float> coherence_xy) const;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CoherenceGain);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_COHERENCE_GAIN_H_
