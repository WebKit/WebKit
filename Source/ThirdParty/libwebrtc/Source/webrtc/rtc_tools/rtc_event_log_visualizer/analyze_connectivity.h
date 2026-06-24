/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_CONNECTIVITY_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_CONNECTIVITY_H_

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

void CreateIceCandidatePairConfigGraph(const ParsedRtcEventLog& parsed_log,
                                       const AnalyzerConfig& config,
                                       Plot* plot);

void CreateIceConnectivityCheckGraph(const ParsedRtcEventLog& parsed_log,
                                     const AnalyzerConfig& config,
                                     Plot* plot);

void CreateDtlsTransportStateGraph(const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot);

void CreateDtlsWritableStateGraph(const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  Plot* plot);

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_CONNECTIVITY_H_
