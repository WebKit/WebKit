/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_LOG_CLASSIFIERS_H_
#define VIDEO_TIMING_SIMULATOR_LOG_CLASSIFIERS_H_

#include <optional>

#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc::video_timing_simulator {

// The enum represents the logging status of RTX original sequence numbers, as
// aggregated across the entire log.
//
// Prior to https://webrtc-review.googlesource.com/c/src/+/442320, RTX
// OSN were not logged at all. After that CL, all RTX OSNs should be logged for
// all video RTX packets. But since the value is represented as an optional (to
// handle both cases), there could also be degenerate cases where RTX OSNs are
// logged for some video RTX packets.
//
// This helper function determines which of three cases holds for a given log.
enum class RtxOsnLoggingStatus {
  kNoRtxOsnLogged,    // Log from before the RTX OSN logging change.
  kSomeRtxOsnLogged,  // Degenerate case -- should not happen.
  kAllRtxOsnLogged,   // Log from after the RTX OSN logging change.
};

// Returns the RTX OSN logging status for the provided log. If there were no
// video RTX packets in the log, the unset value is returned.
std::optional<RtxOsnLoggingStatus> GetRtxOsnLoggingStatus(
    const ParsedRtcEventLog& parsed_log);

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_LOG_CLASSIFIERS_H_
