/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_

#include <array>
#include <vector>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/coherence_gain.h"
#include "modules/audio_processing/aec3/moving_average.h"
#include "modules/audio_processing/aec3/render_signal_analyzer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class SuppressionGain {
 public:
  SuppressionGain(const EchoCanceller3Config& config,
                  Aec3Optimization optimization,
                  int sample_rate_hz);
  ~SuppressionGain();
  void GetGain(
      const std::array<float, kFftLengthBy2Plus1>& nearend_spectrum,
      const std::array<float, kFftLengthBy2Plus1>& echo_spectrum,
      const std::array<float, kFftLengthBy2Plus1>& comfort_noise_spectrum,
      const FftData& linear_aec_fft,
      const FftData& render_fft,
      const FftData& capture_fft,
      const RenderSignalAnalyzer& render_signal_analyzer,
      const AecState& aec_state,
      const std::vector<std::vector<float>>& render,
      float* high_bands_gain,
      std::array<float, kFftLengthBy2Plus1>* low_band_gain);

  // Toggles the usage of the initial state.
  void SetInitialState(bool state);

 private:
  void GainToNoAudibleEcho(
      const std::array<float, kFftLengthBy2Plus1>& nearend,
      const std::array<float, kFftLengthBy2Plus1>& echo,
      const std::array<float, kFftLengthBy2Plus1>& masker,
      const std::array<float, kFftLengthBy2Plus1>& min_gain,
      const std::array<float, kFftLengthBy2Plus1>& max_gain,
      std::array<float, kFftLengthBy2Plus1>* gain) const;

  void LowerBandGain(bool stationary_with_low_power,
                     const AecState& aec_state,
                     const std::array<float, kFftLengthBy2Plus1>& nearend,
                     const std::array<float, kFftLengthBy2Plus1>& echo,
                     const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
                     std::array<float, kFftLengthBy2Plus1>* gain);

  // Limits the gain increase.
  void UpdateGainIncrease(
      bool low_noise_render,
      bool linear_echo_estimate,
      bool saturated_echo,
      const std::array<float, kFftLengthBy2Plus1>& echo,
      const std::array<float, kFftLengthBy2Plus1>& new_gain);

  class LowNoiseRenderDetector {
   public:
    bool Detect(const std::vector<std::vector<float>>& render);

   private:
    float average_power_ = 32768.f * 32768.f;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  const int state_change_duration_blocks_;
  float one_by_state_change_duration_blocks_;
  std::array<float, kFftLengthBy2Plus1> last_gain_;
  std::array<float, kFftLengthBy2Plus1> last_masker_;
  std::array<float, kFftLengthBy2Plus1> gain_increase_;
  std::array<float, kFftLengthBy2Plus1> last_nearend_;
  std::array<float, kFftLengthBy2Plus1> last_echo_;
  std::array<float, kFftLengthBy2Plus1> enr_transparent_;
  std::array<float, kFftLengthBy2Plus1> enr_suppress_;
  std::array<float, kFftLengthBy2Plus1> emr_transparent_;
  LowNoiseRenderDetector low_render_detector_;
  bool initial_state_ = true;
  int initial_state_change_counter_ = 0;
  CoherenceGain coherence_gain_;
  const bool enable_transparency_improvements_;
  const bool enable_new_suppression_;
  aec3::MovingAverage moving_average_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SuppressionGain);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_
