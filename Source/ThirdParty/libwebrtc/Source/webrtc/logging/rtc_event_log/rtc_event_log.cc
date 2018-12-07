/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log.h"

#include "absl/memory/memory.h"

namespace webrtc {

std::unique_ptr<RtcEventLog> RtcEventLog::CreateNull() {
  return absl::make_unique<RtcEventLogNullImpl>();
}

bool RtcEventLogNullImpl::StartLogging(
    std::unique_ptr<RtcEventLogOutput> output,
    int64_t output_period_ms) {
  return false;
}

}  // namespace webrtc
