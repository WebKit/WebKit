/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/agc2/gain_controller2.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

namespace {

constexpr float kGain = 0.5f;

}  // namespace

int GainController2::instance_count_ = 0;

GainController2::GainController2(int sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz),
      data_dumper_(new ApmDataDumper(
        rtc::AtomicOps::Increment(&instance_count_))),
      digital_gain_applier_(),
      gain_(kGain) {
  RTC_DCHECK(sample_rate_hz_ == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz_ == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz_ == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz_ == AudioProcessing::kSampleRate48kHz);
  data_dumper_->InitiateNewSetOfRecordings();
  data_dumper_->DumpRaw("gain_", 1, &gain_);
}

GainController2::~GainController2() = default;

void GainController2::Process(AudioBuffer* audio) {
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    auto channel_view = rtc::ArrayView<float>(
        audio->channels_f()[k], audio->num_frames());
    digital_gain_applier_.Process(gain_, channel_view);
  }
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  return true;
}

std::string GainController2::ToString(
    const AudioProcessing::Config::GainController2& config) {
  std::stringstream ss;
  ss << "{"
     << "enabled: " << (config.enabled ? "true" : "false") << "}";
  return ss.str();
}

}  // namespace webrtc
