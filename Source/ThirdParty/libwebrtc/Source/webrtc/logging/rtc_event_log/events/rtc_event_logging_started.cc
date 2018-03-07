/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_logging_started.h"

namespace webrtc {

RtcEvent::Type RtcEventLoggingStarted::GetType() const {
  return RtcEvent::Type::LoggingStarted;
}

bool RtcEventLoggingStarted::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
