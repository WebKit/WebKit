/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/level_controller/gain_applier.h"

#include <algorithm>

#include "api/array_view.h"
#include "rtc_base/checks.h"

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

const float kMaxSampleValue = 32767.f;
const float kMinSampleValue = -32767.f;

int CountSaturations(rtc::ArrayView<const float> in) {
  return std::count_if(in.begin(), in.end(), [](const float& v) {
    return v >= kMaxSampleValue || v <= kMinSampleValue;
  });
}

int CountSaturations(const AudioBuffer& audio) {
  int num_saturations = 0;
  for (size_t k = 0; k < audio.num_channels(); ++k) {
    num_saturations += CountSaturations(rtc::ArrayView<const float>(
        audio.channels_const_f()[k], audio.num_frames()));
  }
  return num_saturations;
}

void LimitToAllowedRange(rtc::ArrayView<float> x) {
  for (auto& v : x) {
    v = std::max(kMinSampleValue, v);
    v = std::min(kMaxSampleValue, v);
  }
}

void LimitToAllowedRange(AudioBuffer* audio) {
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    LimitToAllowedRange(
        rtc::ArrayView<float>(audio->channels_f()[k], audio->num_frames()));
  }
}

float ApplyIncreasingGain(float new_gain,
                          float old_gain,
                          float step_size,
                          rtc::ArrayView<float> x) {
  RTC_DCHECK_LT(0.f, step_size);
  float gain = old_gain;
  for (auto& v : x) {
    gain = std::min(new_gain, gain + step_size);
    v *= gain;
  }
  return gain;
}

float ApplyDecreasingGain(float new_gain,
                          float old_gain,
                          float step_size,
                          rtc::ArrayView<float> x) {
  RTC_DCHECK_GT(0.f, step_size);
  float gain = old_gain;
  for (auto& v : x) {
    gain = std::max(new_gain, gain + step_size);
    v *= gain;
  }
  return gain;
}

float ApplyConstantGain(float gain, rtc::ArrayView<float> x) {
  for (auto& v : x) {
    v *= gain;
  }

  return gain;
}

float ApplyGain(float new_gain,
                float old_gain,
                float increase_step_size,
                float decrease_step_size,
                rtc::ArrayView<float> x) {
  RTC_DCHECK_LT(0.f, increase_step_size);
  RTC_DCHECK_GT(0.f, decrease_step_size);
  if (new_gain == old_gain) {
    return ApplyConstantGain(new_gain, x);
  } else if (new_gain > old_gain) {
    return ApplyIncreasingGain(new_gain, old_gain, increase_step_size, x);
  } else {
    return ApplyDecreasingGain(new_gain, old_gain, decrease_step_size, x);
  }
}

}  // namespace

GainApplier::GainApplier(ApmDataDumper* data_dumper)
    : data_dumper_(data_dumper) {}

void GainApplier::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  const float kGainIncreaseStepSize48kHz = 0.0001f;
  const float kGainDecreaseStepSize48kHz = -0.01f;
  const float kGainSaturatedDecreaseStepSize48kHz = -0.05f;

  last_frame_was_saturated_ = false;
  old_gain_ = 1.f;
  gain_increase_step_size_ =
      kGainIncreaseStepSize48kHz *
      (static_cast<float>(AudioProcessing::kSampleRate48kHz) / sample_rate_hz);
  gain_normal_decrease_step_size_ =
      kGainDecreaseStepSize48kHz *
      (static_cast<float>(AudioProcessing::kSampleRate48kHz) / sample_rate_hz);
  gain_saturated_decrease_step_size_ =
      kGainSaturatedDecreaseStepSize48kHz *
      (static_cast<float>(AudioProcessing::kSampleRate48kHz) / sample_rate_hz);
}

int GainApplier::Process(float new_gain, AudioBuffer* audio) {
  RTC_CHECK_NE(0.f, gain_increase_step_size_);
  RTC_CHECK_NE(0.f, gain_normal_decrease_step_size_);
  RTC_CHECK_NE(0.f, gain_saturated_decrease_step_size_);
  int num_saturations = 0;
  if (new_gain != 1.f) {
    float last_applied_gain = 1.f;
    float gain_decrease_step_size = last_frame_was_saturated_
                                        ? gain_saturated_decrease_step_size_
                                        : gain_normal_decrease_step_size_;
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      last_applied_gain = ApplyGain(
          new_gain, old_gain_, gain_increase_step_size_,
          gain_decrease_step_size,
          rtc::ArrayView<float>(audio->channels_f()[k], audio->num_frames()));
    }

    num_saturations = CountSaturations(*audio);
    LimitToAllowedRange(audio);
    old_gain_ = last_applied_gain;
  }

  data_dumper_->DumpRaw("lc_last_applied_gain", 1, &old_gain_);

  return num_saturations;
}

}  // namespace webrtc
