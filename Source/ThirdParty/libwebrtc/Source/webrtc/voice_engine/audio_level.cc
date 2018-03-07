/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/audio_level.h"

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/include/module_common_types.h"

namespace webrtc {
namespace voe {

// Number of bars on the indicator.
// Note that the number of elements is specified because we are indexing it
// in the range of 0-32
constexpr int8_t kPermutation[33] = {0, 1, 2, 3, 4, 4, 5, 5, 5, 5, 6,
                                     6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8,
                                     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};

AudioLevel::AudioLevel()
    : abs_max_(0), count_(0), current_level_(0), current_level_full_range_(0) {
  WebRtcSpl_Init();
}

AudioLevel::~AudioLevel() {}

int8_t AudioLevel::Level() const {
  rtc::CritScope cs(&crit_sect_);
  return current_level_;
}

int16_t AudioLevel::LevelFullRange() const {
  rtc::CritScope cs(&crit_sect_);
  return current_level_full_range_;
}

void AudioLevel::Clear() {
  rtc::CritScope cs(&crit_sect_);
  abs_max_ = 0;
  count_ = 0;
  current_level_ = 0;
  current_level_full_range_ = 0;
}

double AudioLevel::TotalEnergy() const {
  rtc::CritScope cs(&crit_sect_);
  return total_energy_;
}

double AudioLevel::TotalDuration() const {
  rtc::CritScope cs(&crit_sect_);
  return total_duration_;
}

void AudioLevel::ComputeLevel(const AudioFrame& audioFrame, double duration) {
  // Check speech level (works for 2 channels as well)
  int16_t abs_value = audioFrame.muted() ? 0 :
      WebRtcSpl_MaxAbsValueW16(
          audioFrame.data(),
          audioFrame.samples_per_channel_ * audioFrame.num_channels_);

  // Protect member access using a lock since this method is called on a
  // dedicated audio thread in the RecordedDataIsAvailable() callback.
  rtc::CritScope cs(&crit_sect_);

  if (abs_value > abs_max_)
    abs_max_ = abs_value;

  // Update level approximately 10 times per second
  if (count_++ == kUpdateFrequency) {
    current_level_full_range_ = abs_max_;

    count_ = 0;

    // Highest value for a int16_t is 0x7fff = 32767
    // Divide with 1000 to get in the range of 0-32 which is the range of the
    // permutation vector
    int32_t position = abs_max_ / 1000;

    // Make it less likely that the bar stays at position 0. I.e. only if it's
    // in the range 0-250 (instead of 0-1000)
    if ((position == 0) && (abs_max_ > 250)) {
      position = 1;
    }
    current_level_ = kPermutation[position];

    // Decay the absolute maximum (divide by 4)
    abs_max_ >>= 2;
  }

  // See the description for "totalAudioEnergy" in the WebRTC stats spec
  // (https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy)
  // for an explanation of these formulas. In short, we need a value that can
  // be used to compute RMS audio levels over different time intervals, by
  // taking the difference between the results from two getStats calls. To do
  // this, the value needs to be of units "squared sample value * time".
  double additional_energy =
      static_cast<double>(current_level_full_range_) / INT16_MAX;
  additional_energy *= additional_energy;
  total_energy_ += additional_energy * duration;
  total_duration_ += duration;
}

}  // namespace voe
}  // namespace webrtc
