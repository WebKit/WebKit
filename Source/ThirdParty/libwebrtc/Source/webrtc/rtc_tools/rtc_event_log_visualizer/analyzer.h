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

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/function_view.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/checks.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

class LazyNetEqSimulator;

class EventLogAnalyzer {
  struct PlotDeclaration {
    PlotDeclaration(const std::string& label,
                    std::function<void(PlotCollection*)> f)
        : label(label), plot_func(f) {}
    const std::string label;
    // TODO(terelius): Add a help text/explanation.
    const std::function<void(PlotCollection*)> plot_func;
  };

  class PlotMap {
   public:
    void RegisterPlot(const std::string& label,
                      std::function<void(PlotCollection*)> f) {
      for (const auto& plot : plots_) {
        RTC_DCHECK(plot.label != label)
            << "Can't use the same label for multiple plots";
      }
      plots_.push_back({label, f});
    }

    void RegisterPlot(const std::string& label, std::function<void(Plot*)> f) {
      RegisterPlot(label, [f, label](PlotCollection* collection) {
        f(collection->AppendNewPlot(label));
      });
    }

    std::vector<PlotDeclaration>::const_iterator begin() const {
      return plots_.begin();
    }
    std::vector<PlotDeclaration>::const_iterator end() const {
      return plots_.end();
    }

   private:
    std::vector<PlotDeclaration> plots_;
  };

 public:
  // The EventLogAnalyzer keeps a reference to the ParsedRtcEventLog for the
  // duration of its lifetime. The ParsedRtcEventLog must not be destroyed or
  // modified while the EventLogAnalyzer is being used.
  EventLogAnalyzer(const ParsedRtcEventLog& log, bool normalize_time);
  EventLogAnalyzer(const ParsedRtcEventLog& log, const AnalyzerConfig& config);
  ~EventLogAnalyzer();

  void CreateGraphsByName(const std::vector<std::string>& names,
                          PlotCollection* collection) const;

  void InitializeMapOfNamedGraphs(bool show_detector_state,
                                  bool show_alr_state,
                                  bool show_link_capacity);

  std::vector<std::string> GetGraphNames() const {
    std::vector<std::string> plot_names;
    for (const auto& plot : plots_) {
      plot_names.push_back(plot.label);
    }
    return plot_names;
  }

  void CreatePacketGraph(PacketDirection direction, Plot* plot) const;

  void CreateRtcpTypeGraph(PacketDirection direction, Plot* plot) const;

  void CreateAccumulatedPacketsGraph(PacketDirection direction,
                                     Plot* plot) const;

  void CreatePacketRateGraph(PacketDirection direction, Plot* plot) const;

  void CreateTotalPacketRateGraph(PacketDirection direction, Plot* plot) const;

  void CreatePlayoutGraph(Plot* plot) const;

  void CreateNetEqSetMinimumDelay(Plot* plot) const;

  void CreateAudioLevelGraph(PacketDirection direction, Plot* plot) const;

  void CreateSequenceNumberGraph(Plot* plot) const;

  void CreateIncomingPacketLossGraph(Plot* plot) const;

  void CreateIncomingDelayGraph(Plot* plot) const;

  void CreateFractionLossGraph(Plot* plot) const;

  void CreateTotalIncomingBitrateGraph(Plot* plot) const;
  void CreateTotalOutgoingBitrateGraph(Plot* plot,
                                       bool show_detector_state = false,
                                       bool show_alr_state = false,
                                       bool show_link_capacity = false) const;

  void CreateStreamBitrateGraph(PacketDirection direction, Plot* plot) const;
  void CreateBitrateAllocationGraph(PacketDirection direction,
                                    Plot* plot) const;

  void CreateOutgoingLossRateGraph(Plot* plot) const;
  void CreateOutgoingEcnFeedbackGraph(Plot* plot) const;
  void CreateIncomingEcnFeedbackGraph(Plot* plot) const;
  void CreateScreamRefWindowGraph(Plot* plot) const;
  void CreateScreamDelayEstimateGraph(Plot* plot) const;
  void CreateGoogCcSimulationGraph(Plot* plot) const;
  void CreateScreamSimulationDelayGraph(Plot* plot) const;
  void CreateScreamSimulationBitrateGraph(Plot* plot) const;
  void CreateScreamSimulationRefWindowGraph(Plot* plot) const;
  void CreateScreamSimulationRatiosGraph(Plot* plot) const;
  void CreateScreamSimulationFeedbackEventsPerRttGraph(Plot* plot) const;
  void CreateSendSideBweSimulationGraph(Plot* plot) const;
  void CreateReceiveSideBweSimulationGraph(Plot* plot) const;

  void CreateNetworkDelayFeedbackGraph(Plot* plot) const;
  void CreatePacerDelayGraph(Plot* plot) const;

  void CreateTimestampGraph(PacketDirection direction, Plot* plot) const;
  void CreateSenderAndReceiverReportPlot(
      PacketDirection direction,
      FunctionView<float(const rtcp::ReportBlock&)> fy,
      std::string title,
      std::string yaxis_label,
      Plot* plot) const;

  void CreateIceCandidatePairConfigGraph(Plot* plot) const;
  void CreateIceConnectivityCheckGraph(Plot* plot) const;

  void CreateDtlsTransportStateGraph(Plot* plot) const;
  void CreateDtlsWritableStateGraph(Plot* plot) const;

  void CreateTriageNotifications() const;
  void PrintNotifications(FILE* file) const;

  void SetNetEqReplacementFile(absl::string_view replacement_file_name,
                               int file_sample_rate_hz);

 private:
  const ParsedRtcEventLog& parsed_log_;

  AnalyzerConfig config_;

  PlotMap plots_;

  std::unique_ptr<LazyNetEqSimulator> neteq_simulator_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_RTC_EVENT_LOG_VISUALIZER_ANALYZER_H_
