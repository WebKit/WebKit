/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/test/parsed_rtc_event_log_from_resources.h"

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"

namespace webrtc::video_timing_simulator {

namespace {

constexpr absl::string_view kResourcePathDir = "video/timing/simulator";
constexpr absl::string_view kRtcEventLogExtension = "rtceventlog";

}  // namespace

std::unique_ptr<ParsedRtcEventLog> ParsedRtcEventLogFromResources(
    absl::string_view resource_file_name) {
  std::string relative_path =
      test::JoinFilename(kResourcePathDir, resource_file_name);
  std::string absolute_path =
      test::ResourcePath(relative_path, kRtcEventLogExtension);
  RTC_CHECK(test::FileExists(absolute_path));
  auto parsed_log = std::make_unique<ParsedRtcEventLog>();
  ParsedRtcEventLog::ParseStatus status = parsed_log->ParseFile(absolute_path);
  RTC_CHECK(status.ok());
  return parsed_log;
}

}  // namespace webrtc::video_timing_simulator
