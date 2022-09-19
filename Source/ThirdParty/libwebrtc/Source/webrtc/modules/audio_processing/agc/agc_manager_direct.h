/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_
#define MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_

#include <atomic>
#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/agc/clipping_predictor.h"
#include "modules/audio_processing/agc/clipping_predictor_evaluator.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class MonoAgc;
class GainControl;

// Adaptive Gain Controller (AGC) that combines an analog and digital gain
// controller. The digital controller determines and applies the digital
// compression gain. The analog controller recommends what input volume (a.k.a.,
// analog level) to use, handles input volume changes and input clipping. In
// particular, it handles input volume changes triggered by the user (e.g.,
// input volume set to zero by a HW mute button). This class is not thread-safe.
class AgcManagerDirect final {
 public:
  // Ctor. `num_capture_channels` specifies the number of channels for the audio
  // passed to `AnalyzePreProcess()` and `Process()`. Clamps
  // `analog_config.startup_min_level` in the [12, 255] range.
  AgcManagerDirect(
      int num_capture_channels,
      const AudioProcessing::Config::GainController1::AnalogGainController&
          analog_config);

  ~AgcManagerDirect();
  AgcManagerDirect(const AgcManagerDirect&) = delete;
  AgcManagerDirect& operator=(const AgcManagerDirect&) = delete;

  void Initialize();

  // Configures `gain_control` to work as a fixed digital controller so that the
  // adaptive part is only handled by this gain controller. Must be called if
  // `gain_control` is also used to avoid the side-effects of running two AGCs.
  void SetupDigitalGainControl(GainControl& gain_control) const;

  // Analyzes `audio` before `Process()` is called so that the analysis can be
  // performed before external digital processing operations take place (e.g.,
  // echo cancellation). The analysis consists of input clipping detection and
  // prediction (if enabled).
  void AnalyzePreProcess(const AudioBuffer* audio);

  // Processes `audio`. Chooses and applies a digital compression gain on each
  // channel and chooses the new input volume to recommend. Undefined behavior
  // if `AnalyzePreProcess()` is not called beforehand.
  void Process(const AudioBuffer* audio);

  // Call when the capture stream output has been flagged to be used/not-used.
  // If unused, the manager  disregards all incoming audio.
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  float voice_probability() const;

  // Returns the recommended input volume.
  int stream_analog_level() const { return stream_analog_level_; }

  // Sets the current input volume.
  void set_stream_analog_level(int level);

  int num_channels() const { return num_capture_channels_; }

  // If available, returns the latest digital compression gain that has been
  // applied.
  absl::optional<int> GetDigitalComressionGain();

  // Returns true if clipping prediction is enabled.
  bool clipping_predictor_enabled() const { return !!clipping_predictor_; }

  // Returns true if clipping prediction is used to adjust the analog gain.
  bool use_clipping_predictor_step() const {
    return use_clipping_predictor_step_;
  }

 private:
  friend class AgcManagerDirectTestHelper;

  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest, DisableDigitalDisablesDigital);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentDefault);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentDisabled);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentOutOfRangeAbove);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentOutOfRangeBelow);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentEnabled50);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentEnabledAboveStartupLevel);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           ClippingParametersVerified);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           DisableClippingPredictorDoesNotLowerVolume);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           UsedClippingPredictionsProduceLowerAnalogLevels);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           UnusedClippingPredictionsProduceEqualAnalogLevels);

  // Ctor that creates a single channel AGC and by injecting `agc`.
  // `agc` will be owned by this class; hence, do not delete it.
  AgcManagerDirect(
      const AudioProcessing::Config::GainController1::AnalogGainController&
          analog_config,
      Agc* agc);

  void AnalyzePreProcess(const float* const* audio, size_t samples_per_channel);

  void AggregateChannelLevels();

  const absl::optional<int> min_mic_level_override_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  static std::atomic<int> instance_counter_;
  const bool use_min_channel_level_;
  const int num_capture_channels_;
  const bool disable_digital_adaptive_;

  int frames_since_clipped_;
  int stream_analog_level_ = 0;
  bool capture_output_used_;
  int channel_controlling_gain_ = 0;

  const int clipped_level_step_;
  const float clipped_ratio_threshold_;
  const int clipped_wait_frames_;

  std::vector<std::unique_ptr<MonoAgc>> channel_agcs_;
  std::vector<absl::optional<int>> new_compressions_to_set_;

  const std::unique_ptr<ClippingPredictor> clipping_predictor_;
  const bool use_clipping_predictor_step_;
  ClippingPredictorEvaluator clipping_predictor_evaluator_;
  int clipping_predictor_log_counter_;
  float clipping_rate_log_;
  int clipping_rate_log_counter_;
};

class MonoAgc {
 public:
  MonoAgc(ApmDataDumper* data_dumper,
          int startup_min_level,
          int clipped_level_min,
          bool disable_digital_adaptive,
          int min_mic_level);
  ~MonoAgc();
  MonoAgc(const MonoAgc&) = delete;
  MonoAgc& operator=(const MonoAgc&) = delete;

  void Initialize();
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  void HandleClipping(int clipped_level_step);

  void Process(rtc::ArrayView<const int16_t> audio);

  void set_stream_analog_level(int level) { stream_analog_level_ = level; }
  int stream_analog_level() const { return stream_analog_level_; }
  float voice_probability() const { return agc_->voice_probability(); }
  void ActivateLogging() { log_to_histograms_ = true; }
  absl::optional<int> new_compression() const {
    return new_compression_to_set_;
  }

  // Only used for testing.
  void set_agc(Agc* agc) { agc_.reset(agc); }
  int min_mic_level() const { return min_mic_level_; }
  int startup_min_level() const { return startup_min_level_; }

 private:
  // Sets a new microphone level, after first checking that it hasn't been
  // updated by the user, in which case no action is taken.
  void SetLevel(int new_level);

  // Set the maximum level the AGC is allowed to apply. Also updates the
  // maximum compression gain to compensate. The level must be at least
  // `kClippedLevelMin`.
  void SetMaxLevel(int level);

  int CheckVolumeAndReset();
  void UpdateGain();
  void UpdateCompressor();

  const int min_mic_level_;
  const bool disable_digital_adaptive_;
  std::unique_ptr<Agc> agc_;
  int level_ = 0;
  int max_level_;
  int max_compression_gain_;
  int target_compression_;
  int compression_;
  float compression_accumulator_;
  bool capture_output_used_ = true;
  bool check_volume_on_next_process_ = true;
  bool startup_ = true;
  int startup_min_level_;
  int calls_since_last_gain_log_ = 0;
  int stream_analog_level_ = 0;
  absl::optional<int> new_compression_to_set_;
  bool log_to_histograms_ = false;
  const int clipped_level_min_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_
