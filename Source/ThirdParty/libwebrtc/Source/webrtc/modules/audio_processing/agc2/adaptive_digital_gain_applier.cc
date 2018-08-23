/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

// This function maps input level to desired applied gain. We want to
// boost the signal so that peaks are at -kHeadroomDbfs. We can't
// apply more than kMaxGainDb gain.
float ComputeGainDb(float input_level_dbfs) {
  // If the level is very low, boost it as much as we can.
  if (input_level_dbfs < -(kHeadroomDbfs + kMaxGainDb)) {
    return kMaxGainDb;
  }

  // We expect to end up here most of the time: the level is below
  // -headroom, but we can boost it to -headroom.
  if (input_level_dbfs < -kHeadroomDbfs) {
    return -kHeadroomDbfs - input_level_dbfs;
  }

  // Otherwise, the level is too high and we can't boost. The
  // LevelEstimator is responsible for not reporting bogus gain
  // values.
  RTC_DCHECK_LE(input_level_dbfs, 0.f);
  return 0.f;
}

// We require 'gain + noise_level <= kMaxNoiseLevelDbfs'.
float LimitGainByNoise(float target_gain,
                       float input_noise_level_dbfs,
                       ApmDataDumper* apm_data_dumper) {
  const float noise_headroom_db = kMaxNoiseLevelDbfs - input_noise_level_dbfs;
  apm_data_dumper->DumpRaw("agc2_noise_headroom_db", noise_headroom_db);
  return std::min(target_gain, std::max(noise_headroom_db, 0.f));
}

// Computes how the gain should change during this frame.
// Return the gain difference in db to 'last_gain_db'.
float ComputeGainChangeThisFrameDb(float target_gain_db,
                                   float last_gain_db,
                                   bool gain_increase_allowed) {
  float target_gain_difference_db = target_gain_db - last_gain_db;
  if (!gain_increase_allowed) {
    target_gain_difference_db = std::min(target_gain_difference_db, 0.f);
  }

  return rtc::SafeClamp(target_gain_difference_db, -kMaxGainChangePerFrameDb,
                        kMaxGainChangePerFrameDb);
}
}  // namespace

AdaptiveDigitalGainApplier::AdaptiveDigitalGainApplier(
    ApmDataDumper* apm_data_dumper)
    : gain_applier_(false, DbToRatio(last_gain_db_)),
      apm_data_dumper_(apm_data_dumper) {}

void AdaptiveDigitalGainApplier::Process(
    float input_level_dbfs,
    float input_noise_level_dbfs,
    const VadWithLevel::LevelAndProbability vad_result,
    AudioFrameView<float> float_frame) {
  calls_since_last_gain_log_++;
  if (calls_since_last_gain_log_ == 100) {
    calls_since_last_gain_log_ = 0;
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.Agc2.DigitalGainApplied",
                                last_gain_db_, 0, kMaxGainDb, kMaxGainDb + 1);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.Agc2.EstimatedNoiseLevel",
                                input_noise_level_dbfs, 0, 100, 101);
  }

  input_level_dbfs = std::min(input_level_dbfs, 0.f);

  RTC_DCHECK_GE(input_level_dbfs, -150.f);
  RTC_DCHECK_GE(float_frame.num_channels(), 1);
  RTC_DCHECK_GE(float_frame.samples_per_channel(), 1);

  const float target_gain_db =
      LimitGainByNoise(ComputeGainDb(input_level_dbfs), input_noise_level_dbfs,
                       apm_data_dumper_);

  // Forbid increasing the gain when there is no speech.
  gain_increase_allowed_ =
      vad_result.speech_probability > kVadConfidenceThreshold;

  const float gain_change_this_frame_db = ComputeGainChangeThisFrameDb(
      target_gain_db, last_gain_db_, gain_increase_allowed_);

  apm_data_dumper_->DumpRaw("agc2_want_to_change_by_db",
                            target_gain_db - last_gain_db_);
  apm_data_dumper_->DumpRaw("agc2_will_change_by_db",
                            gain_change_this_frame_db);

  // Optimization: avoid calling math functions if gain does not
  // change.
  if (gain_change_this_frame_db != 0.f) {
    gain_applier_.SetGainFactor(
        DbToRatio(last_gain_db_ + gain_change_this_frame_db));
  }
  gain_applier_.ApplyGain(float_frame);

  // Remember that the gain has changed for the next iteration.
  last_gain_db_ = last_gain_db_ + gain_change_this_frame_db;
  apm_data_dumper_->DumpRaw("agc2_applied_gain_db", last_gain_db_);
}
}  // namespace webrtc
