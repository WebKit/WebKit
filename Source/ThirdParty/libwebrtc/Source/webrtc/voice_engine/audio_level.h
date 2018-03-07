/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_AUDIO_LEVEL_H_
#define VOICE_ENGINE_AUDIO_LEVEL_H_

#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class AudioFrame;
namespace voe {

class AudioLevel {
 public:
  AudioLevel();
  ~AudioLevel();

  // Called on "API thread(s)" from APIs like VoEBase::CreateChannel(),
  // VoEBase::StopSend(), VoEVolumeControl::GetSpeechOutputLevel().
  int8_t Level() const;
  int16_t LevelFullRange() const;
  void Clear();
  // See the description for "totalAudioEnergy" in the WebRTC stats spec
  // (https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy)
  double TotalEnergy() const;
  double TotalDuration() const;

  // Called on a native capture audio thread (platform dependent) from the
  // AudioTransport::RecordedDataIsAvailable() callback.
  // In Chrome, this method is called on the AudioInputDevice thread.
  void ComputeLevel(const AudioFrame& audioFrame, double duration);

 private:
  enum { kUpdateFrequency = 10 };

  rtc::CriticalSection crit_sect_;

  int16_t abs_max_ RTC_GUARDED_BY(crit_sect_);
  int16_t count_ RTC_GUARDED_BY(crit_sect_);
  int8_t current_level_ RTC_GUARDED_BY(crit_sect_);
  int16_t current_level_full_range_ RTC_GUARDED_BY(crit_sect_);

  double total_energy_ RTC_GUARDED_BY(crit_sect_) = 0.0;
  double total_duration_ RTC_GUARDED_BY(crit_sect_) = 0.0;
};

}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_AUDIO_LEVEL_H_
