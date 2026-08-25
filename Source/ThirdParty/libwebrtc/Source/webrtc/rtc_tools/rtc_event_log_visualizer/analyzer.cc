/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyzer.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment_factory.h"
#include "api/function_view.h"
#include "api/neteq/neteq.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/logging.h"
#include "rtc_tools/rtc_event_log_visualizer/analyze_audio.h"
#include "rtc_tools/rtc_event_log_visualizer/analyze_bwe.h"
#include "rtc_tools/rtc_event_log_visualizer/analyze_connectivity.h"
#include "rtc_tools/rtc_event_log_visualizer/analyze_rtp_rtcp.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& parsed_log,
                                   bool normalize_time)
    : parsed_log_(parsed_log),
      config_(CreateEnvironment(), parsed_log, normalize_time) {
  config_.window_duration_ = TimeDelta::Millis(250);
  config_.step_ = TimeDelta::Millis(10);
  if (config_.end_time_ < config_.begin_time_) {
    RTC_LOG(LS_WARNING) << "No useful events in the log.";
    config_.begin_time_ = config_.end_time_ = Timestamp::Zero();
  }
  neteq_simulator_ = std::make_unique<LazyNetEqSimulator>(parsed_log_, config_);

  if (parsed_log_.first_timestamp().IsFinite() &&
      parsed_log_.last_timestamp().IsFinite()) {
    RTC_LOG(LS_INFO) << "Log is "
                     << (parsed_log_.last_timestamp().ms() -
                         parsed_log_.first_timestamp().ms()) /
                            1000
                     << " seconds long.";
  }
}

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config)
    : parsed_log_(parsed_log), config_(config) {
  neteq_simulator_ = std::make_unique<LazyNetEqSimulator>(parsed_log_, config_);
  if (parsed_log_.first_timestamp().IsFinite() &&
      parsed_log_.last_timestamp().IsFinite()) {
    RTC_LOG(LS_INFO) << "Log is "
                     << (parsed_log_.last_timestamp().ms() -
                         parsed_log_.first_timestamp().ms()) /
                            1000
                     << " seconds long.";
  }
}

EventLogAnalyzer::~EventLogAnalyzer() = default;

void EventLogAnalyzer::SetNetEqReplacementFile(
    absl::string_view replacement_file_name,
    int file_sample_rate_hz) {
  neteq_simulator_->SetReplacementAudioFile(replacement_file_name,
                                            file_sample_rate_hz);
}

void EventLogAnalyzer::CreateGraphsByName(const std::vector<std::string>& names,
                                          PlotCollection* collection) const {
  for (absl::string_view name : names) {
    auto plot = absl::c_find_if(plots_, [name](const PlotDeclaration& plot) {
      return plot.label == name;
    });
    if (plot != plots_.end()) {
      plot->plot_func(collection);
    }
  }
}

void EventLogAnalyzer::InitializeMapOfNamedGraphs(bool show_detector_state,
                                                  bool show_alr_state,
                                                  bool show_link_capacity,
                                                  bool include_overhead) {
  plots_.RegisterPlot("incoming_packet_sizes", [this](Plot* plot) {
    this->CreatePacketGraph(kIncomingPacket, plot);
  });

  plots_.RegisterPlot("outgoing_packet_sizes", [this](Plot* plot) {
    this->CreatePacketGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("incoming_rtcp_types", [this](Plot* plot) {
    this->CreateRtcpTypeGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_rtcp_types", [this](Plot* plot) {
    this->CreateRtcpTypeGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("incoming_packet_count", [this](Plot* plot) {
    this->CreateAccumulatedPacketsGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_packet_count", [this](Plot* plot) {
    this->CreateAccumulatedPacketsGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("incoming_packet_rate", [this](Plot* plot) {
    this->CreatePacketRateGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_packet_rate", [this](Plot* plot) {
    this->CreatePacketRateGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("total_incoming_packet_rate", [this](Plot* plot) {
    this->CreateTotalPacketRateGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("total_outgoing_packet_rate", [this](Plot* plot) {
    this->CreateTotalPacketRateGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("audio_playout",
                      [this](Plot* plot) { this->CreatePlayoutGraph(plot); });

  plots_.RegisterPlot("neteq_set_minimum_delay", [this](Plot* plot) {
    this->CreateNetEqSetMinimumDelay(plot);
  });

  plots_.RegisterPlot("incoming_audio_level", [this](Plot* plot) {
    this->CreateAudioLevelGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_audio_level", [this](Plot* plot) {
    this->CreateAudioLevelGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("incoming_sequence_number_delta", [this](Plot* plot) {
    this->CreateSequenceNumberGraph(plot);
  });
  plots_.RegisterPlot("incoming_delay", [this](Plot* plot) {
    this->CreateIncomingDelayGraph(plot);
  });
  plots_.RegisterPlot("incoming_loss_rate", [this](Plot* plot) {
    this->CreateIncomingPacketLossGraph(plot);
  });
  plots_.RegisterPlot("incoming_bitrate", [this, include_overhead](Plot* plot) {
    this->CreateTotalIncomingBitrateGraph(plot, include_overhead);
  });
  plots_.RegisterPlot("outgoing_bitrate", [this, show_detector_state,
                                           show_alr_state, show_link_capacity,
                                           include_overhead](Plot* plot) {
    this->CreateTotalOutgoingBitrateGraph(plot, show_detector_state,
                                          show_alr_state, show_link_capacity,
                                          include_overhead);
  });
  plots_.RegisterPlot("incoming_stream_bitrate", [this](Plot* plot) {
    this->CreateStreamBitrateGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_stream_bitrate", [this](Plot* plot) {
    this->CreateStreamBitrateGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("incoming_layer_bitrate_allocation", [this](Plot* plot) {
    this->CreateBitrateAllocationGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_layer_bitrate_allocation", [this](Plot* plot) {
    this->CreateBitrateAllocationGraph(kOutgoingPacket, plot);
  });
  plots_.RegisterPlot("simulated_receiveside_bwe", [this](Plot* plot) {
    this->CreateReceiveSideBweSimulationGraph(plot);
  });
  plots_.RegisterPlot("simulated_sendside_bwe", [this](Plot* plot) {
    this->CreateSendSideBweSimulationGraph(plot);
  });
  plots_.RegisterPlot("simulated_goog_cc", [this](Plot* plot) {
    this->CreateGoogCcSimulationGraph(plot);
  });
  plots_.RegisterPlot("simulated_scream_delay", [this](Plot* plot) {
    this->CreateScreamSimulationDelayGraph(plot);
  });
  plots_.RegisterPlot("simulated_scream_bitrates", [this](Plot* plot) {
    this->CreateScreamSimulationBitrateGraph(plot);
  });
  plots_.RegisterPlot("simulated_scream_ref_window", [this](Plot* plot) {
    this->CreateScreamSimulationRefWindowGraph(plot);
  });
  plots_.RegisterPlot("simulated_scream_ratios", [this](Plot* plot) {
    this->CreateScreamSimulationRatiosGraph(plot);
  });
  plots_.RegisterPlot(
      "simulated_scream_feedback_events_per_rtt", [this](Plot* plot) {
        this->CreateScreamSimulationFeedbackEventsPerRttGraph(plot);
      });
  plots_.RegisterPlot("outgoing_loss", [this](Plot* plot) {
    this->CreateOutgoingLossRateGraph(plot);
  });
  plots_.RegisterPlot("outgoing_twcc_loss", [this](Plot* plot) {
    this->CreateOutgoingLossRateGraph(plot);
  });
  plots_.RegisterPlot("outgoing_ecn_feedback", [this](Plot* plot) {
    this->CreateOutgoingEcnFeedbackGraph(plot);
  });
  plots_.RegisterPlot("incoming_ecn_feedback", [this](Plot* plot) {
    this->CreateIncomingEcnFeedbackGraph(plot);
  });
  plots_.RegisterPlot("scream_ref_window", [this](Plot* plot) {
    this->CreateScreamRefWindowGraph(plot);
  });
  plots_.RegisterPlot("scream_delay_estimates", [this](Plot* plot) {
    this->CreateScreamDelayEstimateGraph(plot);
  });
  plots_.RegisterPlot("network_delay_feedback", [this](Plot* plot) {
    this->CreateNetworkDelayFeedbackGraph(plot);
  });
  plots_.RegisterPlot("fraction_loss_feedback", [this](Plot* plot) {
    this->CreateFractionLossGraph(plot);
  });
  plots_.RegisterPlot("incoming_timestamps", [this](Plot* plot) {
    this->CreateTimestampGraph(kIncomingPacket, plot);
  });
  plots_.RegisterPlot("outgoing_timestamps", [this](Plot* plot) {
    this->CreateTimestampGraph(kOutgoingPacket, plot);
  });

  plots_.RegisterPlot("incoming_rtcp_fraction_lost", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(kIncomingPacket, GetFractionLost,
                                            "Fraction lost (incoming RTCP)",
                                            "Loss rate (percent)", plot);
  });
  plots_.RegisterPlot("outgoing_rtcp_fraction_lost", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(kOutgoingPacket, GetFractionLost,
                                            "Fraction lost (outgoing RTCP)",
                                            "Loss rate (percent)", plot);
  });

  plots_.RegisterPlot("incoming_rtcp_cumulative_lost", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kIncomingPacket, GetCumulativeLost,
        "Cumulative lost packets (incoming RTCP)", "Packets", plot);
  });
  plots_.RegisterPlot("outgoing_rtcp_cumulative_lost", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kOutgoingPacket, GetCumulativeLost,
        "Cumulative lost packets (outgoing RTCP)", "Packets", plot);
  });

  plots_.RegisterPlot("incoming_rtcp_highest_seq_number", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kIncomingPacket, GetHighestSeqNumber,
        "Highest sequence number (incoming RTCP)", "Sequence number", plot);
  });
  plots_.RegisterPlot("outgoing_rtcp_highest_seq_number", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kOutgoingPacket, GetHighestSeqNumber,
        "Highest sequence number (outgoing RTCP)", "Sequence number", plot);
  });

  plots_.RegisterPlot("incoming_rtcp_delay_since_last_sr", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kIncomingPacket, DelaySinceLastSr,
        "Delay since last received sender report (incoming RTCP)", "Time (s)",
        plot);
  });
  plots_.RegisterPlot("outgoing_rtcp_delay_since_last_sr", [this](Plot* plot) {
    this->CreateSenderAndReceiverReportPlot(
        kOutgoingPacket, DelaySinceLastSr,
        "Delay since last received sender report (outgoing RTCP)", "Time (s)",
        plot);
  });

  plots_.RegisterPlot(
      "pacer_delay", [this](Plot* plot) { this->CreatePacerDelayGraph(plot); });

  plots_.RegisterPlot("audio_encoder_bitrate", [this](Plot* plot) {
    CreateAudioEncoderTargetBitrateGraph(this->parsed_log_, this->config_,
                                         plot);
  });
  plots_.RegisterPlot("audio_encoder_frame_length", [this](Plot* plot) {
    CreateAudioEncoderFrameLengthGraph(this->parsed_log_, this->config_, plot);
  });
  plots_.RegisterPlot("audio_encoder_packet_loss", [this](Plot* plot) {
    CreateAudioEncoderPacketLossGraph(this->parsed_log_, this->config_, plot);
  });
  plots_.RegisterPlot("audio_encoder_fec", [this](Plot* plot) {
    CreateAudioEncoderEnableFecGraph(this->parsed_log_, this->config_, plot);
  });
  plots_.RegisterPlot("audio_encoder_dtx", [this](Plot* plot) {
    CreateAudioEncoderEnableDtxGraph(this->parsed_log_, this->config_, plot);
  });
  plots_.RegisterPlot("audio_encoder_num_channels", [this](Plot* plot) {
    CreateAudioEncoderNumChannelsGraph(this->parsed_log_, this->config_, plot);
  });

  plots_.RegisterPlot("ice_candidate_pair_config", [this](Plot* plot) {
    this->CreateIceCandidatePairConfigGraph(plot);
  });
  plots_.RegisterPlot("ice_connectivity_check", [this](Plot* plot) {
    this->CreateIceConnectivityCheckGraph(plot);
  });
  plots_.RegisterPlot("dtls_transport_state", [this](Plot* plot) {
    this->CreateDtlsTransportStateGraph(plot);
  });
  plots_.RegisterPlot("dtls_writable_state", [this](Plot* plot) {
    this->CreateDtlsWritableStateGraph(plot);
  });
  plots_.RegisterPlot(
      "simulated_neteq_expand_rate", [this](PlotCollection* collection) {
        CreateNetEqNetworkStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqNetworkStatistics& stats) {
              return stats.expand_rate / 16384.f;
            },
            "Expand rate",
            collection->AppendNewPlot("simulated_neteq_expand_rate"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_speech_expand_rate", [this](PlotCollection* collection) {
        CreateNetEqNetworkStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqNetworkStatistics& stats) {
              return stats.speech_expand_rate / 16384.f;
            },
            "Speech expand rate",
            collection->AppendNewPlot("simulated_neteq_speech_expand_rate"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_accelerate_rate", [this](PlotCollection* collection) {
        CreateNetEqNetworkStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqNetworkStatistics& stats) {
              return stats.accelerate_rate / 16384.f;
            },
            "Accelerate rate",
            collection->AppendNewPlot("simulated_neteq_accelerate_rate"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_preemptive_rate", [this](PlotCollection* collection) {
        CreateNetEqNetworkStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqNetworkStatistics& stats) {
              return stats.preemptive_rate / 16384.f;
            },
            "Preemptive rate",
            collection->AppendNewPlot("simulated_neteq_preemptive_rate"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_concealment_events", [this](PlotCollection* collection) {
        CreateNetEqLifetimeStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqLifetimeStatistics& stats) {
              return static_cast<float>(stats.concealment_events);
            },
            "Concealment events",
            collection->AppendNewPlot("simulated_neteq_concealment_events"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_preferred_buffer_size",
      [this](PlotCollection* collection) {
        CreateNetEqNetworkStatsGraph(
            parsed_log_, config_, neteq_simulator_->GetStats(),
            [](const NetEqNetworkStatistics& stats) {
              return stats.preferred_buffer_size_ms;
            },
            "Preferred buffer size (ms)",
            collection->AppendNewPlot("simulated_neteq_preferred_buffer_size"));
      });
  plots_.RegisterPlot(
      "simulated_neteq_jitter_buffer_delay",
      [this](PlotCollection* collection) {
        for (const auto& st : neteq_simulator_->GetStats()) {
          CreateAudioJitterBufferGraph(
              parsed_log_, config_, st.first, st.second.get(),
              collection->AppendNewPlot("simulated_neteq_jitter_buffer_delay"));
        }
      });
}
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) const {
  webrtc::CreatePlayoutGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateNetEqSetMinimumDelay(Plot* plot) const {
  webrtc::CreateNetEqSetMinimumDelay(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateAudioLevelGraph(PacketDirection direction,
                                             Plot* plot) const {
  webrtc::CreateAudioLevelGraph(parsed_log_, config_, direction, plot);
}

void EventLogAnalyzer::CreateIncomingDelayGraph(Plot* plot) const {
  webrtc::CreateIncomingDelayGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateFractionLossGraph(Plot* plot) const {
  webrtc::CreateFractionLossGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateTotalIncomingBitrateGraph(
    Plot* plot,
    bool include_overhead) const {
  webrtc::CreateTotalIncomingBitrateGraph(parsed_log_, config_, plot,
                                          include_overhead);
}

void EventLogAnalyzer::CreateTotalOutgoingBitrateGraph(
    Plot* plot,
    bool show_detector_state,
    bool show_alr_state,
    bool show_link_capacity,
    bool include_overhead) const {
  webrtc::CreateTotalOutgoingBitrateGraph(parsed_log_, config_, plot,
                                          show_detector_state, show_alr_state,
                                          show_link_capacity, include_overhead);
}

void EventLogAnalyzer::CreateGoogCcSimulationGraph(Plot* plot) const {
  webrtc::CreateGoogCcSimulationGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamSimulationDelayGraph(Plot* plot) const {
  webrtc::CreateScreamSimulationDelayGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamSimulationBitrateGraph(Plot* plot) const {
  webrtc::CreateScreamSimulationBitrateGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamSimulationRefWindowGraph(Plot* plot) const {
  webrtc::CreateScreamSimulationRefWindowGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamSimulationRatiosGraph(Plot* plot) const {
  webrtc::CreateScreamSimulationRatiosGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamSimulationFeedbackEventsPerRttGraph(
    Plot* plot) const {
  webrtc::CreateScreamSimulationFeedbackEventsPerRttGraph(parsed_log_, config_,
                                                          plot);
}

void EventLogAnalyzer::CreateScreamRefWindowGraph(Plot* plot) const {
  webrtc::CreateScreamRefWindowGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateScreamDelayEstimateGraph(Plot* plot) const {
  webrtc::CreateScreamDelayEstimateGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateSendSideBweSimulationGraph(Plot* plot) const {
  webrtc::CreateSendSideBweSimulationGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateReceiveSideBweSimulationGraph(Plot* plot) const {
  webrtc::CreateReceiveSideBweSimulationGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateNetworkDelayFeedbackGraph(Plot* plot) const {
  webrtc::CreateNetworkDelayFeedbackGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreatePacerDelayGraph(Plot* plot) const {
  webrtc::CreatePacerDelayGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateIceCandidatePairConfigGraph(Plot* plot) const {
  webrtc::CreateIceCandidatePairConfigGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateIceConnectivityCheckGraph(Plot* plot) const {
  webrtc::CreateIceConnectivityCheckGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateDtlsTransportStateGraph(Plot* plot) const {
  webrtc::CreateDtlsTransportStateGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateDtlsWritableStateGraph(Plot* plot) const {
  webrtc::CreateDtlsWritableStateGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreatePacketGraph(PacketDirection direction,
                                         Plot* plot) const {
  webrtc::CreatePacketGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateRtcpTypeGraph(PacketDirection direction,
                                           Plot* plot) const {
  webrtc::CreateRtcpTypeGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateAccumulatedPacketsGraph(PacketDirection direction,
                                                     Plot* plot) const {
  webrtc::CreateAccumulatedPacketsGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreatePacketRateGraph(PacketDirection direction,
                                             Plot* plot) const {
  webrtc::CreatePacketRateGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateTotalPacketRateGraph(PacketDirection direction,
                                                  Plot* plot) const {
  webrtc::CreateTotalPacketRateGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) const {
  webrtc::CreateSequenceNumberGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateIncomingPacketLossGraph(Plot* plot) const {
  webrtc::CreateIncomingPacketLossGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateStreamBitrateGraph(PacketDirection direction,
                                                Plot* plot) const {
  webrtc::CreateStreamBitrateGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateBitrateAllocationGraph(PacketDirection direction,
                                                    Plot* plot) const {
  webrtc::CreateBitrateAllocationGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateOutgoingEcnFeedbackGraph(Plot* plot) const {
  webrtc::CreateOutgoingEcnFeedbackGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateIncomingEcnFeedbackGraph(Plot* plot) const {
  webrtc::CreateIncomingEcnFeedbackGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateOutgoingLossRateGraph(Plot* plot) const {
  webrtc::CreateOutgoingLossRateGraph(parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateTimestampGraph(PacketDirection direction,
                                            Plot* plot) const {
  webrtc::CreateTimestampGraph(direction, parsed_log_, config_, plot);
}

void EventLogAnalyzer::CreateSenderAndReceiverReportPlot(
    PacketDirection direction,
    FunctionView<float(const rtcp::ReportBlock&)> fy,
    std::string title,
    std::string yaxis_label,
    Plot* plot) const {
  webrtc::CreateSenderAndReceiverReportPlot(direction, fy, title, yaxis_label,
                                            parsed_log_, config_, plot);
}

}  // namespace webrtc
