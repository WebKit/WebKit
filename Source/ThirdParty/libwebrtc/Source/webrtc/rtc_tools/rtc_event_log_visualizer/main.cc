/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/match.h"
#include "api/neteq/neteq.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_tools/rtc_event_log_visualizer/alerts.h"
#include "rtc_tools/rtc_event_log_visualizer/analyze_audio.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/testsupport/file_utils.h"

ABSL_FLAG(std::string,
          plot,
          "default",
          "A comma separated list of plot names. See --list_plots for valid "
          "options.");

ABSL_FLAG(
    std::string,
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enabled/"
    " will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
ABSL_FLAG(std::string,
          wav_filename,
          "",
          "Path to wav file used for simulation of jitter buffer");

ABSL_FLAG(bool,
          show_detector_state,
          false,
          "Show the state of the delay based BWE detector on the total "
          "bitrate graph");

ABSL_FLAG(bool,
          show_alr_state,
          false,
          "Show the state ALR state on the total bitrate graph");

ABSL_FLAG(bool,
          parse_unconfigured_header_extensions,
          true,
          "Attempt to parse unconfigured header extensions using the default "
          "WebRTC mapping. This can give very misleading results if the "
          "application negotiates a different mapping.");

ABSL_FLAG(bool,
          print_triage_alerts,
          true,
          "Print triage alerts, i.e. a list of potential problems.");

ABSL_FLAG(bool,
          normalize_time,
          true,
          "Normalize the log timestamps so that the call starts at time 0.");

ABSL_FLAG(bool,
          shared_xaxis,
          false,
          "Share x-axis between all plots so that zooming in one plot "
          "updates all the others too. A downside is that certain "
          "operations like panning become much slower.");

ABSL_FLAG(bool,
          protobuf_output,
          false,
          "Output charts as protobuf instead of python code.");

ABSL_FLAG(bool,
          list_plots,
          false,
          "List of registered plots (for use with the --plot flag)");

using webrtc::Plot;

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

struct PlotDeclaration {
  PlotDeclaration(const std::string& label, std::function<void(Plot*)> f)
      : label(label), enabled(false), plot_func(f) {}
  const std::string label;
  bool enabled;
  // TODO(terelius): Add a help text/explanation.
  const std::function<void(Plot*)> plot_func;
};

class PlotMap {
 public:
  void RegisterPlot(const std::string& label, std::function<void(Plot*)> f) {
    for (const auto& plot : plots_) {
      RTC_DCHECK(plot.label != label)
          << "Can't use the same label for multiple plots";
    }
    plots_.push_back({label, f});
  }

  bool EnablePlotsByFlags(
      const std::vector<std::string>& flags,
      const std::map<std::string, std::vector<std::string>>& flag_aliases) {
    bool status = true;
    for (const std::string& flag : flags) {
      auto alias_it = flag_aliases.find(flag);
      if (alias_it != flag_aliases.end()) {
        const auto& replacements = alias_it->second;
        for (const auto& replacement : replacements) {
          status &= EnablePlotByFlag(replacement);
        }
      } else {
        status &= EnablePlotByFlag(flag);
      }
    }
    return status;
  }

  void EnableAllPlots() {
    for (auto& plot : plots_) {
      plot.enabled = true;
    }
  }

  std::vector<PlotDeclaration>::iterator begin() { return plots_.begin(); }
  std::vector<PlotDeclaration>::iterator end() { return plots_.end(); }

 private:
  bool EnablePlotByFlag(const std::string& flag) {
    for (auto& plot : plots_) {
      if (plot.label == flag) {
        plot.enabled = true;
        return true;
      }
    }
    if (flag == "simulated_neteq_jitter_buffer_delay") {
      // This flag is handled separately.
      return true;
    }
    std::cerr << "Unrecognized plot name \'" << flag << "\'. Aborting."
              << std::endl;
    return false;
  }

  std::vector<PlotDeclaration> plots_;
};

bool ContainsHelppackageFlags(absl::string_view filename) {
  return absl::EndsWith(filename, "main.cc");
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "A tool for visualizing WebRTC event logs.\n"
      "Example usage:\n"
      "./event_log_visualizer <logfile> | python\n");
  absl::FlagsUsageConfig flag_config;
  flag_config.contains_help_flags = &ContainsHelppackageFlags;
  absl::SetFlagsUsageConfig(flag_config);
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);

  // Print RTC_LOG warnings and errors even in release builds.
  if (rtc::LogMessage::GetLogToDebug() > rtc::LS_WARNING) {
    rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
  }
  rtc::LogMessage::SetLogToStderr(true);

  // Flag replacements
  std::map<std::string, std::vector<std::string>> flag_aliases = {
      {"default",
       {"incoming_delay", "incoming_loss_rate", "incoming_bitrate",
        "outgoing_bitrate", "incoming_stream_bitrate",
        "outgoing_stream_bitrate", "network_delay_feedback",
        "fraction_loss_feedback"}},
      {"sendside_bwe",
       {"outgoing_packet_sizes", "outgoing_bitrate", "outgoing_stream_bitrate",
        "simulated_sendside_bwe", "network_delay_feedback",
        "fraction_loss_feedback"}},
      {"receiveside_bwe",
       {"incoming_packet_sizes", "incoming_delay", "incoming_loss_rate",
        "incoming_bitrate", "incoming_stream_bitrate",
        "simulated_receiveside_bwe"}},
      {"rtcp_details",
       {"incoming_rtcp_fraction_lost", "outgoing_rtcp_fraction_lost",
        "incoming_rtcp_cumulative_lost", "outgoing_rtcp_cumulative_lost",
        "incoming_rtcp_highest_seq_number", "outgoing_rtcp_highest_seq_number",
        "incoming_rtcp_delay_since_last_sr",
        "outgoing_rtcp_delay_since_last_sr"}},
      {"simulated_neteq_stats",
       {"simulated_neteq_jitter_buffer_delay",
        "simulated_neteq_preferred_buffer_size",
        "simulated_neteq_concealment_events", "simulated_neteq_preemptive_rate",
        "simulated_neteq_accelerate_rate", "simulated_neteq_speech_expand_rate",
        "simulated_neteq_expand_rate"}}};

  std::vector<std::string> plot_flags =
      StrSplit(absl::GetFlag(FLAGS_plot), ",");

  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  const std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  webrtc::ParsedRtcEventLog::UnconfiguredHeaderExtensions header_extensions =
      webrtc::ParsedRtcEventLog::UnconfiguredHeaderExtensions::kDontParse;
  if (absl::GetFlag(FLAGS_parse_unconfigured_header_extensions)) {
    header_extensions = webrtc::ParsedRtcEventLog::
        UnconfiguredHeaderExtensions::kAttemptWebrtcDefaultConfig;
  }
  webrtc::ParsedRtcEventLog parsed_log(header_extensions,
                                       /*allow_incomplete_logs*/ true);

  if (args.size() == 2) {
    std::string filename = args[1];
    auto status = parsed_log.ParseFile(filename);
    if (!status.ok()) {
      std::cerr << "Failed to parse " << filename << ": " << status.message()
                << std::endl;
      return -1;
    }
  }

  webrtc::AnalyzerConfig config;
  config.window_duration_ = 250000;
  config.step_ = 10000;
  config.normalize_time_ = absl::GetFlag(FLAGS_normalize_time);
  config.begin_time_ = parsed_log.first_timestamp();
  config.end_time_ = parsed_log.last_timestamp();
  if (config.end_time_ < config.begin_time_) {
    RTC_LOG(LS_WARNING) << "Log end time " << config.end_time_
                        << " not after begin time " << config.begin_time_
                        << ". Nothing to analyze. Is the log broken?";
    return -1;
  }

  webrtc::EventLogAnalyzer analyzer(parsed_log, config);
  webrtc::PlotCollection collection;

  PlotMap plots;
  plots.RegisterPlot("incoming_packet_sizes", [&](Plot* plot) {
    analyzer.CreatePacketGraph(webrtc::kIncomingPacket, plot);
  });

  plots.RegisterPlot("outgoing_packet_sizes", [&](Plot* plot) {
    analyzer.CreatePacketGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("incoming_rtcp_types", [&](Plot* plot) {
    analyzer.CreateRtcpTypeGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_rtcp_types", [&](Plot* plot) {
    analyzer.CreateRtcpTypeGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("incoming_packet_count", [&](Plot* plot) {
    analyzer.CreateAccumulatedPacketsGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_packet_count", [&](Plot* plot) {
    analyzer.CreateAccumulatedPacketsGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("incoming_packet_rate", [&](Plot* plot) {
    analyzer.CreatePacketRateGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_packet_rate", [&](Plot* plot) {
    analyzer.CreatePacketRateGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("total_incoming_packet_rate", [&](Plot* plot) {
    analyzer.CreateTotalPacketRateGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("total_outgoing_packet_rate", [&](Plot* plot) {
    analyzer.CreateTotalPacketRateGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("audio_playout",
                     [&](Plot* plot) { analyzer.CreatePlayoutGraph(plot); });
  plots.RegisterPlot("incoming_audio_level", [&](Plot* plot) {
    analyzer.CreateAudioLevelGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_audio_level", [&](Plot* plot) {
    analyzer.CreateAudioLevelGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("incoming_sequence_number_delta", [&](Plot* plot) {
    analyzer.CreateSequenceNumberGraph(plot);
  });
  plots.RegisterPlot("incoming_delay", [&](Plot* plot) {
    analyzer.CreateIncomingDelayGraph(plot);
  });
  plots.RegisterPlot("incoming_loss_rate", [&](Plot* plot) {
    analyzer.CreateIncomingPacketLossGraph(plot);
  });
  plots.RegisterPlot("incoming_bitrate", [&](Plot* plot) {
    analyzer.CreateTotalIncomingBitrateGraph(plot);
  });
  plots.RegisterPlot("outgoing_bitrate", [&](Plot* plot) {
    analyzer.CreateTotalOutgoingBitrateGraph(
        plot, absl::GetFlag(FLAGS_show_detector_state),
        absl::GetFlag(FLAGS_show_alr_state));
  });
  plots.RegisterPlot("incoming_stream_bitrate", [&](Plot* plot) {
    analyzer.CreateStreamBitrateGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_stream_bitrate", [&](Plot* plot) {
    analyzer.CreateStreamBitrateGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("incoming_layer_bitrate_allocation", [&](Plot* plot) {
    analyzer.CreateBitrateAllocationGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_layer_bitrate_allocation", [&](Plot* plot) {
    analyzer.CreateBitrateAllocationGraph(webrtc::kOutgoingPacket, plot);
  });
  plots.RegisterPlot("simulated_receiveside_bwe", [&](Plot* plot) {
    analyzer.CreateReceiveSideBweSimulationGraph(plot);
  });
  plots.RegisterPlot("simulated_sendside_bwe", [&](Plot* plot) {
    analyzer.CreateSendSideBweSimulationGraph(plot);
  });
  plots.RegisterPlot("simulated_goog_cc", [&](Plot* plot) {
    analyzer.CreateGoogCcSimulationGraph(plot);
  });
  plots.RegisterPlot("network_delay_feedback", [&](Plot* plot) {
    analyzer.CreateNetworkDelayFeedbackGraph(plot);
  });
  plots.RegisterPlot("fraction_loss_feedback", [&](Plot* plot) {
    analyzer.CreateFractionLossGraph(plot);
  });
  plots.RegisterPlot("incoming_timestamps", [&](Plot* plot) {
    analyzer.CreateTimestampGraph(webrtc::kIncomingPacket, plot);
  });
  plots.RegisterPlot("outgoing_timestamps", [&](Plot* plot) {
    analyzer.CreateTimestampGraph(webrtc::kOutgoingPacket, plot);
  });

  auto GetFractionLost = [](const webrtc::rtcp::ReportBlock& block) -> float {
    return static_cast<double>(block.fraction_lost()) / 256 * 100;
  };
  plots.RegisterPlot("incoming_rtcp_fraction_lost", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kIncomingPacket, GetFractionLost,
        "Fraction lost (incoming RTCP)", "Loss rate (percent)", plot);
  });
  plots.RegisterPlot("outgoing_rtcp_fraction_lost", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kOutgoingPacket, GetFractionLost,
        "Fraction lost (outgoing RTCP)", "Loss rate (percent)", plot);
  });
  auto GetCumulativeLost = [](const webrtc::rtcp::ReportBlock& block) -> float {
    return block.cumulative_lost_signed();
  };
  plots.RegisterPlot("incoming_rtcp_cumulative_lost", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kIncomingPacket, GetCumulativeLost,
        "Cumulative lost packets (incoming RTCP)", "Packets", plot);
  });
  plots.RegisterPlot("outgoing_rtcp_cumulative_lost", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kOutgoingPacket, GetCumulativeLost,
        "Cumulative lost packets (outgoing RTCP)", "Packets", plot);
  });

  auto GetHighestSeqNumber =
      [](const webrtc::rtcp::ReportBlock& block) -> float {
    return block.extended_high_seq_num();
  };
  plots.RegisterPlot("incoming_rtcp_highest_seq_number", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kIncomingPacket, GetHighestSeqNumber,
        "Highest sequence number (incoming RTCP)", "Sequence number", plot);
  });
  plots.RegisterPlot("outgoing_rtcp_highest_seq_number", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kOutgoingPacket, GetHighestSeqNumber,
        "Highest sequence number (outgoing RTCP)", "Sequence number", plot);
  });

  auto DelaySinceLastSr = [](const webrtc::rtcp::ReportBlock& block) -> float {
    return static_cast<double>(block.delay_since_last_sr()) / 65536;
  };
  plots.RegisterPlot("incoming_rtcp_delay_since_last_sr", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kIncomingPacket, DelaySinceLastSr,
        "Delay since last received sender report (incoming RTCP)", "Time (s)",
        plot);
  });
  plots.RegisterPlot("outgoing_rtcp_delay_since_last_sr", [&](Plot* plot) {
    analyzer.CreateSenderAndReceiverReportPlot(
        webrtc::kOutgoingPacket, DelaySinceLastSr,
        "Delay since last received sender report (outgoing RTCP)", "Time (s)",
        plot);
  });

  plots.RegisterPlot("pacer_delay",
                     [&](Plot* plot) { analyzer.CreatePacerDelayGraph(plot); });
  plots.RegisterPlot("audio_encoder_bitrate", [&](Plot* plot) {
    CreateAudioEncoderTargetBitrateGraph(parsed_log, config, plot);
  });
  plots.RegisterPlot("audio_encoder_frame_length", [&](Plot* plot) {
    CreateAudioEncoderFrameLengthGraph(parsed_log, config, plot);
  });
  plots.RegisterPlot("audio_encoder_packet_loss", [&](Plot* plot) {
    CreateAudioEncoderPacketLossGraph(parsed_log, config, plot);
  });
  plots.RegisterPlot("audio_encoder_fec", [&](Plot* plot) {
    CreateAudioEncoderEnableFecGraph(parsed_log, config, plot);
  });
  plots.RegisterPlot("audio_encoder_dtx", [&](Plot* plot) {
    CreateAudioEncoderEnableDtxGraph(parsed_log, config, plot);
  });
  plots.RegisterPlot("audio_encoder_num_channels", [&](Plot* plot) {
    CreateAudioEncoderNumChannelsGraph(parsed_log, config, plot);
  });

  plots.RegisterPlot("ice_candidate_pair_config", [&](Plot* plot) {
    analyzer.CreateIceCandidatePairConfigGraph(plot);
  });
  plots.RegisterPlot("ice_connectivity_check", [&](Plot* plot) {
    analyzer.CreateIceConnectivityCheckGraph(plot);
  });
  plots.RegisterPlot("dtls_transport_state", [&](Plot* plot) {
    analyzer.CreateDtlsTransportStateGraph(plot);
  });
  plots.RegisterPlot("dtls_writable_state", [&](Plot* plot) {
    analyzer.CreateDtlsWritableStateGraph(plot);
  });

  std::string wav_path;
  if (!absl::GetFlag(FLAGS_wav_filename).empty()) {
    wav_path = absl::GetFlag(FLAGS_wav_filename);
  } else {
    wav_path = webrtc::test::ResourcePath(
        "audio_processing/conversational_speech/EN_script2_F_sp2_B1", "wav");
  }
  absl::optional<webrtc::NetEqStatsGetterMap> neteq_stats;

  plots.RegisterPlot("simulated_neteq_expand_rate", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.expand_rate / 16384.f;
        },
        "Expand rate", plot);
  });

  plots.RegisterPlot("simulated_neteq_speech_expand_rate", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.speech_expand_rate / 16384.f;
        },
        "Speech expand rate", plot);
  });

  plots.RegisterPlot("simulated_neteq_accelerate_rate", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.accelerate_rate / 16384.f;
        },
        "Accelerate rate", plot);
  });

  plots.RegisterPlot("simulated_neteq_preemptive_rate", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.preemptive_rate / 16384.f;
        },
        "Preemptive rate", plot);
  });

  plots.RegisterPlot("simulated_neteq_concealment_events", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqLifetimeStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqLifetimeStatistics& stats) {
          return static_cast<float>(stats.concealment_events);
        },
        "Concealment events", plot);
  });

  plots.RegisterPlot("simulated_neteq_preferred_buffer_size", [&](Plot* plot) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.preferred_buffer_size_ms;
        },
        "Preferred buffer size (ms)", plot);
  });

  if (absl::c_find(plot_flags, "all") != plot_flags.end()) {
    plots.EnableAllPlots();
    // Treated separately since it isn't registered like the other plots.
    plot_flags.push_back("simulated_neteq_jitter_buffer_delay");
  } else {
    bool success = plots.EnablePlotsByFlags(plot_flags, flag_aliases);
    if (!success) {
      return 1;
    }
  }

  if (absl::GetFlag(FLAGS_list_plots)) {
    std::cerr << "List of registered plots (for use with the --plot flag):"
              << std::endl;
    for (const auto& plot : plots) {
      // TODO(terelius): Also print a help text.
      std::cerr << "  " << plot.label << std::endl;
    }
    // The following flag doesn't fit the model used for the other plots.
    std::cerr << "simulated_neteq_jitter_buffer_delay" << std::endl;
    std::cerr << "List of plot aliases (for use with the --plot flag):"
              << std::endl;
    std::cerr << "  all = every registered plot" << std::endl;
    for (const auto& alias : flag_aliases) {
      std::cerr << "  " << alias.first << " = ";
      for (const auto& replacement : alias.second) {
        std::cerr << replacement << ",";
      }
      std::cerr << std::endl;
    }
    return 0;
  }
  if (args.size() != 2) {
    // Print usage information.
    std::cerr << absl::ProgramUsageMessage();
    return 1;
  }

  for (const auto& plot : plots) {
    if (plot.enabled) {
      Plot* output = collection.AppendNewPlot();
      plot.plot_func(output);
      output->SetId(plot.label);
    }
  }

  // The model we use for registering plots assumes that the each plot label
  // can be mapped to a lambda that will produce exactly one plot. The
  // simulated_neteq_jitter_buffer_delay plot doesn't fit this model since it
  // creates multiple plots, and would need some state kept between the lambda
  // calls.
  if (absl::c_find(plot_flags, "simulated_neteq_jitter_buffer_delay") !=
      plot_flags.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    for (webrtc::NetEqStatsGetterMap::const_iterator it = neteq_stats->cbegin();
         it != neteq_stats->cend(); ++it) {
      webrtc::CreateAudioJitterBufferGraph(parsed_log, config, it->first,
                                           it->second.get(),
                                           collection.AppendNewPlot());
    }
  }

  if (absl::GetFlag(FLAGS_protobuf_output)) {
    webrtc::analytics::ChartCollection proto_charts;
    collection.ExportProtobuf(&proto_charts);
    std::cout << proto_charts.SerializeAsString();
  } else {
    collection.PrintPythonCode(absl::GetFlag(FLAGS_shared_xaxis));
  }

  if (absl::GetFlag(FLAGS_print_triage_alerts)) {
    webrtc::TriageHelper triage_alerts(config);
    triage_alerts.AnalyzeLog(parsed_log);
    triage_alerts.Print(stderr);
  }

  return 0;
}
