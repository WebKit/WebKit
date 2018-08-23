/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_H_

#include <memory>

#include "rtc_base/timeutils.h"

namespace webrtc {

// This class allows us to store unencoded RTC events. Subclasses of this class
// store the actual information. This allows us to keep all unencoded events,
// even when their type and associated information differ, in the same buffer.
// Additionally, it prevents dependency leaking - a module that only logs
// events of type RtcEvent_A doesn't need to know about anything associated
// with events of type RtcEvent_B.
class RtcEvent {
 public:
  // Subclasses of this class have to associate themselves with a unique value
  // of Type. This leaks the information of existing subclasses into the
  // superclass, but the *actual* information - rtclog::StreamConfig, etc. -
  // is kept separate.
  enum class Type {
    AlrStateEvent,
    AudioNetworkAdaptation,
    AudioPlayout,
    AudioReceiveStreamConfig,
    AudioSendStreamConfig,
    BweUpdateDelayBased,
    BweUpdateLossBased,
    IceCandidatePairConfig,
    IceCandidatePairEvent,
    ProbeClusterCreated,
    ProbeResultFailure,
    ProbeResultSuccess,
    RtcpPacketIncoming,
    RtcpPacketOutgoing,
    RtpPacketIncoming,
    RtpPacketOutgoing,
    VideoReceiveStreamConfig,
    VideoSendStreamConfig
  };

  RtcEvent() : timestamp_us_(rtc::TimeMicros()) {}
  virtual ~RtcEvent() = default;

  virtual Type GetType() const = 0;

  virtual bool IsConfigEvent() const = 0;

  virtual std::unique_ptr<RtcEvent> Copy() const = 0;

  const int64_t timestamp_us_;

 protected:
  explicit RtcEvent(int64_t timestamp_us) : timestamp_us_(timestamp_us) {}
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_H_
