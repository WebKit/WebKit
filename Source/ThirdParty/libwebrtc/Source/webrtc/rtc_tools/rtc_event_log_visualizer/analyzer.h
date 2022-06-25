/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_H_
#define RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/neteq/tools/neteq_stats_getter.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

class EventLogAnalyzer {
 public:
  // The EventLogAnalyzer keeps a reference to the ParsedRtcEventLogNew for the
  // duration of its lifetime. The ParsedRtcEventLogNew must not be destroyed or
  // modified while the EventLogAnalyzer is being used.
  EventLogAnalyzer(const ParsedRtcEventLog& log, bool normalize_time);
  EventLogAnalyzer(const ParsedRtcEventLog& log, const AnalyzerConfig& config);

  void CreatePacketGraph(PacketDirection direction, Plot* plot);

  void CreateRtcpTypeGraph(PacketDirection direction, Plot* plot);

  void CreateAccumulatedPacketsGraph(PacketDirection direction, Plot* plot);

  void CreatePacketRateGraph(PacketDirection direction, Plot* plot);

  void CreateTotalPacketRateGraph(PacketDirection direction, Plot* plot);

  void CreatePlayoutGraph(Plot* plot);

  void CreateAudioLevelGraph(PacketDirection direction, Plot* plot);

  void CreateSequenceNumberGraph(Plot* plot);

  void CreateIncomingPacketLossGraph(Plot* plot);

  void CreateIncomingDelayGraph(Plot* plot);

  void CreateFractionLossGraph(Plot* plot);

  void CreateTotalIncomingBitrateGraph(Plot* plot);
  void CreateTotalOutgoingBitrateGraph(Plot* plot,
                                       bool show_detector_state = false,
                                       bool show_alr_state = false);

  void CreateStreamBitrateGraph(PacketDirection direction, Plot* plot);
  void CreateBitrateAllocationGraph(PacketDirection direction, Plot* plot);

  void CreateGoogCcSimulationGraph(Plot* plot);
  void CreateSendSideBweSimulationGraph(Plot* plot);
  void CreateReceiveSideBweSimulationGraph(Plot* plot);

  void CreateNetworkDelayFeedbackGraph(Plot* plot);
  void CreatePacerDelayGraph(Plot* plot);

  void CreateTimestampGraph(PacketDirection direction, Plot* plot);
  void CreateSenderAndReceiverReportPlot(
      PacketDirection direction,
      rtc::FunctionView<float(const rtcp::ReportBlock&)> fy,
      std::string title,
      std::string yaxis_label,
      Plot* plot);

  void CreateIceCandidatePairConfigGraph(Plot* plot);
  void CreateIceConnectivityCheckGraph(Plot* plot);

  void CreateDtlsTransportStateGraph(Plot* plot);
  void CreateDtlsWritableStateGraph(Plot* plot);

  void CreateTriageNotifications();
  void PrintNotifications(FILE* file);

 private:
  template <typename IterableType>
  void CreateAccumulatedPacketsTimeSeries(Plot* plot,
                                          const IterableType& packets,
                                          const std::string& label);

  std::string GetCandidatePairLogDescriptionFromId(uint32_t candidate_pair_id);

  const ParsedRtcEventLog& parsed_log_;

  // A list of SSRCs we are interested in analysing.
  // If left empty, all SSRCs will be considered relevant.
  std::vector<uint32_t> desired_ssrc_;

  std::map<uint32_t, std::string> candidate_pair_desc_by_id_;

  AnalyzerConfig config_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_H_
