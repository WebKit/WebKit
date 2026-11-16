/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_RTP_RTCP_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_RTP_RTCP_H_

#include <string>

#include "api/function_view.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

float GetHighestSeqNumber(const rtcp::ReportBlock& block);
float GetFractionLost(const rtcp::ReportBlock& block);
float GetCumulativeLost(const rtcp::ReportBlock& block);
float DelaySinceLastSr(const rtcp::ReportBlock& block);

void CreatePacketGraph(PacketDirection direction,
                       const ParsedRtcEventLog& parsed_log,
                       const AnalyzerConfig& config,
                       Plot* plot);

void CreateRtcpTypeGraph(PacketDirection direction,
                         const ParsedRtcEventLog& parsed_log,
                         const AnalyzerConfig& config,
                         Plot* plot);

void CreateAccumulatedPacketsGraph(PacketDirection direction,
                                   const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot);

void CreatePacketRateGraph(PacketDirection direction,
                           const ParsedRtcEventLog& parsed_log,
                           const AnalyzerConfig& config,
                           Plot* plot);

void CreateTotalPacketRateGraph(PacketDirection direction,
                                const ParsedRtcEventLog& parsed_log,
                                const AnalyzerConfig& config,
                                Plot* plot);

void CreateSequenceNumberGraph(const ParsedRtcEventLog& parsed_log,
                               const AnalyzerConfig& config,
                               Plot* plot);

void CreateIncomingPacketLossGraph(const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot);

void CreateStreamBitrateGraph(PacketDirection direction,
                              const ParsedRtcEventLog& parsed_log,
                              const AnalyzerConfig& config,
                              Plot* plot);

void CreateBitrateAllocationGraph(PacketDirection direction,
                                  const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  Plot* plot);

void CreateOutgoingEcnFeedbackGraph(const ParsedRtcEventLog& parsed_log,
                                    const AnalyzerConfig& config,
                                    Plot* plot);

void CreateIncomingEcnFeedbackGraph(const ParsedRtcEventLog& parsed_log,
                                    const AnalyzerConfig& config,
                                    Plot* plot);

void CreateOutgoingLossRateGraph(const ParsedRtcEventLog& parsed_log,
                                 const AnalyzerConfig& config,
                                 Plot* plot);

void CreateTimestampGraph(PacketDirection direction,
                          const ParsedRtcEventLog& parsed_log,
                          const AnalyzerConfig& config,
                          Plot* plot);

void CreateSenderAndReceiverReportPlot(
    PacketDirection direction,
    FunctionView<float(const rtcp::ReportBlock&)> fy,
    std::string title,
    std::string yaxis_label,
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    Plot* plot);

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZE_RTP_RTCP_H_
