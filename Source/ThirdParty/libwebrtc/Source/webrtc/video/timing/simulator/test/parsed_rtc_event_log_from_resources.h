/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_FROM_RESOURCES_H_
#define VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_FROM_RESOURCES_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc::video_timing_simulator {

// Returns a `ParsedRtcEventLog` corresponding to the `resource_file_name` in
// the `resources/` directory.
absl_nonnull std::unique_ptr<ParsedRtcEventLog> ParsedRtcEventLogFromResources(
    absl::string_view resource_file_name);

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_FROM_RESOURCES_H_
