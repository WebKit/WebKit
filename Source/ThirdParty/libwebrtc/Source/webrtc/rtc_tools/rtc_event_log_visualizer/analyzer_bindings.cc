/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyzer_bindings.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment_factory.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#else
#include "rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#endif

namespace {
std::vector<std::string> StrSplit(const std::string& s,
                                  const std::string& delimiter) {
  std::vector<std::string> v;
  size_t pos = 0;
  while (pos < s.length()) {
    const std::string token = s.substr(pos, s.find(delimiter, pos) - pos);
    pos += token.length() + delimiter.length();
    v.push_back(token);
  }
  return v;
}
}  // namespace

void analyze_rtc_event_log(const char* log_contents,
                           size_t log_size,
                           const char* selection,
                           size_t selection_size,
                           char* output,
                           uint32_t* output_size) {
  webrtc::ParsedRtcEventLog parsed_log(
      webrtc::ParsedRtcEventLog::UnconfiguredHeaderExtensions::kDontParse,
      /*allow_incomplete_logs*/ false);

  absl::string_view log_view(log_contents, log_size);
  auto status = parsed_log.ParseString(log_view);
  if (!status.ok()) {
    std::cerr << "Failed to parse log: " << status.message() << std::endl;
    *output_size = 0;
    return;
  }

  webrtc::AnalyzerConfig config;
  config.window_duration_ = webrtc::TimeDelta::Millis(250);
  config.step_ = webrtc::TimeDelta::Millis(10);
  if (!parsed_log.start_log_events().empty()) {
    config.rtc_to_utc_offset_ = parsed_log.start_log_events()[0].utc_time() -
                                parsed_log.start_log_events()[0].log_time();
  }
  config.normalize_time_ = true;
  config.begin_time_ = parsed_log.first_timestamp();
  config.end_time_ = parsed_log.last_timestamp();
  if (config.end_time_ < config.begin_time_) {
    std::cerr << "Log end time " << config.end_time_.ms()
              << " not after begin time " << config.begin_time_.ms()
              << ". Nothing to analyze. Is the log broken?";
    *output_size = 0;
    return;
  }

  webrtc::EventLogAnalyzer analyzer(webrtc::CreateEnvironment(), parsed_log,
                                    config);
  analyzer.InitializeMapOfNamedGraphs(/*show_detector_state=*/false,
                                      /*show_alr_state=*/false,
                                      /*show_link_capacity=*/false);

  webrtc::PlotCollection collection;
  std::vector<std::string> plot_names =
      StrSplit(std::string(selection, selection_size), ",");
  analyzer.CreateGraphsByName(plot_names, &collection);
  collection.SetCallTimeToUtcOffsetMs(config.CallTimeToUtcOffsetMs());

  webrtc::analytics::ChartCollection proto_charts;
  collection.ExportProtobuf(&proto_charts);
  std::string serialized_charts = proto_charts.SerializeAsString();
  if (webrtc::checked_cast<uint32_t>(serialized_charts.size()) > *output_size) {
    std::cerr << "Serialized charts larger than available output buffer: "
              << serialized_charts.size() << " vs " << *output_size
              << std::endl;
    *output_size = 0;
    return;
  }

  memcpy(output, serialized_charts.data(), serialized_charts.size());
  *output_size = webrtc::checked_cast<uint32_t>(serialized_charts.size());
}
