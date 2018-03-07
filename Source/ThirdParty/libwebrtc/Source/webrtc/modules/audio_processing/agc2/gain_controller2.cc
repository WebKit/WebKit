/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/gain_controller2.h"

#include <cmath>

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

int GainController2::instance_count_ = 0;

GainController2::GainController2()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      sample_rate_hz_(AudioProcessing::kSampleRate48kHz),
      fixed_gain_(1.f) {}

GainController2::~GainController2() = default;

void GainController2::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  sample_rate_hz_ = sample_rate_hz;
  data_dumper_->InitiateNewSetOfRecordings();
  data_dumper_->DumpRaw("sample_rate_hz", sample_rate_hz_);
  data_dumper_->DumpRaw("fixed_gain_linear", fixed_gain_);
}

void GainController2::Process(AudioBuffer* audio) {
  if (fixed_gain_ == 1.f)
    return;

  for (size_t k = 0; k < audio->num_channels(); ++k) {
    for (size_t j = 0; j < audio->num_frames(); ++j) {
      audio->channels_f()[k][j] = rtc::SafeClamp(
          fixed_gain_ * audio->channels_f()[k][j], -32768.f, 32767.f);
    }
  }
}

void GainController2::ApplyConfig(
    const AudioProcessing::Config::GainController2& config) {
  RTC_DCHECK(Validate(config));
  fixed_gain_ = std::pow(10.f, config.fixed_gain_db / 20.f);
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  return config.fixed_gain_db >= 0.f;
}

std::string GainController2::ToString(
    const AudioProcessing::Config::GainController2& config) {
  std::stringstream ss;
  ss << "{enabled: " << (config.enabled ? "true" : "false") << ", "
     << "fixed_gain_dB: " << config.fixed_gain_db << "}";
  return ss.str();
}

}  // namespace webrtc
