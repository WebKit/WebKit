/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_LOGGING_STOPPED_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_LOGGING_STOPPED_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventLoggingStopped final : public RtcEvent {
 public:
  ~RtcEventLoggingStopped() override = default;

  Type GetType() const override;

  bool IsConfigEvent() const override;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_LOGGING_STOPPED_H_
