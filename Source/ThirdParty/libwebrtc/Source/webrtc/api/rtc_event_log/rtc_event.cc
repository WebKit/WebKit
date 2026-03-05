/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_event_log/rtc_event.h"

#include "rtc_base/time_utils.h"

namespace webrtc {

// TODO: bugs.webrtc.org/42223992 - Remove use of the global clock
// after November 1, 2025, assuming by that time all RtcEventLog
// implementations are updated to set timestamp using propagated clock.
RtcEvent::RtcEvent() : timestamp_us_(TimeMillis() * 1000) {}

}  // namespace webrtc
