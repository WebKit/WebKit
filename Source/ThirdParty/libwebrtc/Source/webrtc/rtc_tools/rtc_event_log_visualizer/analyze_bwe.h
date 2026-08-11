/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_BWE_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_BWE_H_

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

void CreateIncomingDelayGraph(const ParsedRtcEventLog& parsed_log,
                              const AnalyzerConfig& config,
                              Plot* plot);

void CreateFractionLossGraph(const ParsedRtcEventLog& parsed_log,
                             const AnalyzerConfig& config,
                             Plot* plot);

void CreateTotalIncomingBitrateGraph(const ParsedRtcEventLog& parsed_log,
                                     const AnalyzerConfig& config,
                                     Plot* plot);

void CreateTotalOutgoingBitrateGraph(const ParsedRtcEventLog& parsed_log,
                                     const AnalyzerConfig& config,
                                     Plot* plot,
                                     bool show_detector_state = false,
                                     bool show_alr_state = false,
                                     bool show_link_capacity = false);

void CreateGoogCcSimulationGraph(const ParsedRtcEventLog& parsed_log,
                                 const AnalyzerConfig& config,
                                 Plot* plot);

void CreateScreamSimulationDelayGraph(const ParsedRtcEventLog& parsed_log,
                                      const AnalyzerConfig& config,
                                      Plot* plot);

void CreateScreamSimulationBitrateGraph(const ParsedRtcEventLog& parsed_log,
                                        const AnalyzerConfig& config,
                                        Plot* plot);
void CreateScreamSimulationRefWindowGraph(const ParsedRtcEventLog& parsed_log,
                                          const AnalyzerConfig& config,
                                          Plot* plot);

void CreateScreamSimulationRatiosGraph(const ParsedRtcEventLog& parsed_log,
                                       const AnalyzerConfig& config,
                                       Plot* plot);

void CreateScreamSimulationFeedbackEventsPerRttGraph(
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    Plot* plot);

void CreateScreamRefWindowGraph(const ParsedRtcEventLog& parsed_log,
                                const AnalyzerConfig& config,
                                Plot* plot);

void CreateScreamDelayEstimateGraph(const ParsedRtcEventLog& parsed_log,
                                    const AnalyzerConfig& config,
                                    Plot* plot);

void CreateSendSideBweSimulationGraph(const ParsedRtcEventLog& parsed_log,
                                      const AnalyzerConfig& config,
                                      Plot* plot);

void CreateReceiveSideBweSimulationGraph(const ParsedRtcEventLog& parsed_log,
                                         const AnalyzerConfig& config,
                                         Plot* plot);

void CreateNetworkDelayFeedbackGraph(const ParsedRtcEventLog& parsed_log,
                                     const AnalyzerConfig& config,
                                     Plot* plot);

void CreatePacerDelayGraph(const ParsedRtcEventLog& parsed_log,
                           const AnalyzerConfig& config,
                           Plot* plot);

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_BWE_H_
