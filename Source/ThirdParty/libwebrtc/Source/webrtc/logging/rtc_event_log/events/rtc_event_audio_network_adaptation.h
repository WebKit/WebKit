/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_

#include <memory>

#include "api/rtc_event_log/rtc_event.h"
#include "api/units/timestamp.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"

namespace webrtc {

struct AudioEncoderRuntimeConfig;

class RtcEventAudioNetworkAdaptation final : public RtcEvent {
 public:
  static constexpr Type kType = Type::AudioNetworkAdaptation;

  explicit RtcEventAudioNetworkAdaptation(
      std::unique_ptr<AudioEncoderRuntimeConfig> config);
  ~RtcEventAudioNetworkAdaptation() override;

  Type GetType() const override { return kType; }
  bool IsConfigEvent() const override { return false; }

  std::unique_ptr<RtcEventAudioNetworkAdaptation> Copy() const;

  const AudioEncoderRuntimeConfig& config() const { return *config_; }

 private:
  RtcEventAudioNetworkAdaptation(const RtcEventAudioNetworkAdaptation& other);

  const std::unique_ptr<const AudioEncoderRuntimeConfig> config_;
};

struct LoggedAudioNetworkAdaptationEvent {
  LoggedAudioNetworkAdaptationEvent() = default;
  LoggedAudioNetworkAdaptationEvent(Timestamp timestamp,
                                    const AudioEncoderRuntimeConfig& config)
      : timestamp(timestamp), config(config) {}

  int64_t log_time_us() const { return timestamp.us(); }
  int64_t log_time_ms() const { return timestamp.ms(); }

  Timestamp timestamp = Timestamp::MinusInfinity();
  AudioEncoderRuntimeConfig config;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_
