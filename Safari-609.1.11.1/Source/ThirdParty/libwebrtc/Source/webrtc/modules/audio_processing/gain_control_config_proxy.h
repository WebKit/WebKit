/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_GAIN_CONTROL_CONFIG_PROXY_H_
#define MODULES_AUDIO_PROCESSING_GAIN_CONTROL_CONFIG_PROXY_H_

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/gain_control.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This class forwards all gain control configuration to the audio processing
// module, for compatibility with AudioProcessing::Config.
class GainControlConfigProxy : public GainControl {
 public:
  GainControlConfigProxy(rtc::CriticalSection* crit_capture,
                         AudioProcessing* apm,
                         GainControl* agc);
  GainControlConfigProxy(const GainControlConfigProxy&) = delete;
  GainControlConfigProxy& operator=(const GainControlConfigProxy&) = delete;

  ~GainControlConfigProxy() override;

 private:
  // GainControl API during processing.
  int set_stream_analog_level(int level) override;
  int stream_analog_level() const override;

  // GainControl config setters.
  int Enable(bool enable) override;
  int set_mode(Mode mode) override;
  int set_target_level_dbfs(int level) override;
  int set_compression_gain_db(int gain) override;
  int enable_limiter(bool enable) override;
  int set_analog_level_limits(int minimum, int maximum) override;

  // GainControl config getters.
  bool is_enabled() const override;
  bool is_limiter_enabled() const override;
  int compression_gain_db() const override;
  int target_level_dbfs() const override;
  int analog_level_minimum() const override;
  int analog_level_maximum() const override;
  bool stream_is_saturated() const override;
  Mode mode() const override;

  rtc::CriticalSection* crit_capture_ = nullptr;
  AudioProcessing* apm_ = nullptr;
  GainControl* agc_ RTC_GUARDED_BY(crit_capture_) = nullptr;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_GAIN_CONTROL_CONFIG_PROXY_H_
