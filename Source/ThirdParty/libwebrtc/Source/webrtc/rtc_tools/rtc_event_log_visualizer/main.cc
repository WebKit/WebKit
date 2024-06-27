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

#include <cstdio>
#include <fstream>
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
#include "rtc_tools/rtc_event_log_visualizer/conversational_speech_en.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"
#include "system_wrappers/include/field_trial.h"

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
          show_link_capacity,
          true,
          "Show the lower and upper link capacity on the outgoing bitrate "
          "graph");

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

ABSL_FLAG(std::string,
          figure_output_path,
          "",
          "A path to output the python plots into");

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

bool KnownPlotName(absl::string_view name,
                   const std::vector<std::string>& known_plots) {
  return absl::c_find(known_plots, name) != known_plots.end();
}

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
  config.window_duration_ = webrtc::TimeDelta::Millis(250);
  config.step_ = webrtc::TimeDelta::Millis(10);
  if (!parsed_log.start_log_events().empty()) {
    config.rtc_to_utc_offset_ = parsed_log.start_log_events()[0].utc_time() -
                                parsed_log.start_log_events()[0].log_time();
  }
  config.normalize_time_ = absl::GetFlag(FLAGS_normalize_time);
  config.begin_time_ = parsed_log.first_timestamp();
  config.end_time_ = parsed_log.last_timestamp();
  if (config.end_time_ < config.begin_time_) {
    RTC_LOG(LS_WARNING) << "Log end time " << config.end_time_
                        << " not after begin time " << config.begin_time_
                        << ". Nothing to analyze. Is the log broken?";
    return -1;
  }

  std::string wav_path;
  bool has_generated_wav_file = false;
  if (!absl::GetFlag(FLAGS_wav_filename).empty()) {
    wav_path = absl::GetFlag(FLAGS_wav_filename);
  } else {
    // TODO(bugs.webrtc.org/14248): Remove the need to generate a file
    // and read the file directly from memory.
    wav_path = std::tmpnam(nullptr);
    std::ofstream out_wav_file(wav_path);
    out_wav_file.write(
        reinterpret_cast<char*>(&webrtc::conversational_speech_en_wav[0]),
        webrtc::conversational_speech_en_wav_len);
    has_generated_wav_file = true;
  }

  webrtc::EventLogAnalyzer analyzer(parsed_log, config);
  analyzer.InitializeMapOfNamedGraphs(absl::GetFlag(FLAGS_show_detector_state),
                                      absl::GetFlag(FLAGS_show_alr_state),
                                      absl::GetFlag(FLAGS_show_link_capacity));

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
        "fraction_loss_feedback", "outgoing_twcc_loss"}},
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

  if (absl::GetFlag(FLAGS_list_plots)) {
    std::cerr << "List of registered plots (for use with the --plot flag):"
              << std::endl;
    for (const auto& plot_name : analyzer.GetGraphNames()) {
      // TODO(terelius): Also print a help text.
      std::cerr << "  " << plot_name;
    }
    // The following flags don't fit the model used for the other plots.
    for (const auto& plot_name : flag_aliases["simulated_neteq_stats"]) {
      std::cerr << " " << plot_name;
    }
    std::cerr << std::endl;

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

  // Select which plots to output
  std::vector<std::string> plot_flags =
      StrSplit(absl::GetFlag(FLAGS_plot), ",");
  std::vector<std::string> plot_names;
  const std::vector<std::string> known_analyzer_plots =
      analyzer.GetGraphNames();
  const std::vector<std::string> known_neteq_plots =
      flag_aliases["simulated_neteq_stats"];
  std::vector<std::string> all_known_plots = known_analyzer_plots;
  all_known_plots.insert(all_known_plots.end(), known_neteq_plots.begin(),
                         known_neteq_plots.end());
  for (const std::string& flag : plot_flags) {
    if (flag == "all") {
      plot_names = all_known_plots;
      break;
    }
    auto alias_it = flag_aliases.find(flag);
    if (alias_it != flag_aliases.end()) {
      for (std::string& replacement : alias_it->second) {
        if (!KnownPlotName(replacement, all_known_plots)) {
          std::cerr << "Unknown plot name \"" << replacement << "\""
                    << std::endl;
          return 1;
        }
        plot_names.push_back(replacement);
      }
    } else {
      plot_names.push_back(flag);
      if (!KnownPlotName(flag, all_known_plots)) {
        std::cerr << "Unknown plot name \"" << flag << "\"" << std::endl;
        return 1;
      }
    }
  }

  webrtc::PlotCollection collection;
  analyzer.CreateGraphsByName(plot_names, &collection);

  // The simulated neteq charts are treated separately because they have a
  // different behavior compared to all other plots. In particular, the neteq
  // plots
  //  * cache the simulation results between different plots
  //  * open and read files
  //  * dont have a 1-to-1 mapping between IDs and charts.
  absl::optional<webrtc::NetEqStatsGetterMap> neteq_stats;
  if (absl::c_find(plot_names, "simulated_neteq_expand_rate") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.expand_rate / 16384.f;
        },
        "Expand rate", collection.AppendNewPlot("simulated_neteq_expand_rate"));
  }
  if (absl::c_find(plot_names, "simulated_neteq_speech_expand_rate") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.speech_expand_rate / 16384.f;
        },
        "Speech expand rate",
        collection.AppendNewPlot("simulated_neteq_speech_expand_rate"));
  }
  if (absl::c_find(plot_names, "simulated_neteq_accelerate_rate") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.accelerate_rate / 16384.f;
        },
        "Accelerate rate",
        collection.AppendNewPlot("simulated_neteq_accelerate_rate"));
  }
  if (absl::c_find(plot_names, "simulated_neteq_preemptive_rate") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.preemptive_rate / 16384.f;
        },
        "Preemptive rate",
        collection.AppendNewPlot("simulated_neteq_preemptive_rate"));
  }
  if (absl::c_find(plot_names, "simulated_neteq_concealment_events") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqLifetimeStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqLifetimeStatistics& stats) {
          return static_cast<float>(stats.concealment_events);
        },
        "Concealment events",
        collection.AppendNewPlot("simulated_neteq_concealment_events"));
  }
  if (absl::c_find(plot_names, "simulated_neteq_preferred_buffer_size") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    webrtc::CreateNetEqNetworkStatsGraph(
        parsed_log, config, *neteq_stats,
        [](const webrtc::NetEqNetworkStatistics& stats) {
          return stats.preferred_buffer_size_ms;
        },
        "Preferred buffer size (ms)",
        collection.AppendNewPlot("simulated_neteq_preferred_buffer_size"));
  }

  // The model we use for registering plots assumes that the each plot label
  // can be mapped to a lambda that will produce exactly one plot. The
  // simulated_neteq_jitter_buffer_delay plot doesn't fit this model since it
  // creates multiple plots, and would need some state kept between the lambda
  // calls.
  if (absl::c_find(plot_names, "simulated_neteq_jitter_buffer_delay") !=
      plot_names.end()) {
    if (!neteq_stats) {
      neteq_stats = webrtc::SimulateNetEq(parsed_log, config, wav_path, 48000);
    }
    for (auto it = neteq_stats->cbegin(); it != neteq_stats->cend(); ++it) {
      webrtc::CreateAudioJitterBufferGraph(
          parsed_log, config, it->first, it->second.get(),
          collection.AppendNewPlot("simulated_neteq_jitter_buffer_delay"));
    }
  }

  collection.SetCallTimeToUtcOffsetMs(config.CallTimeToUtcOffsetMs());

  if (absl::GetFlag(FLAGS_protobuf_output)) {
    webrtc::analytics::ChartCollection proto_charts;
    collection.ExportProtobuf(&proto_charts);
    std::cout << proto_charts.SerializeAsString();
  } else {
    collection.PrintPythonCode(absl::GetFlag(FLAGS_shared_xaxis),
                               absl::GetFlag(FLAGS_figure_output_path));
  }

  if (absl::GetFlag(FLAGS_print_triage_alerts)) {
    webrtc::TriageHelper triage_alerts(config);
    triage_alerts.AnalyzeLog(parsed_log);
    triage_alerts.Print(stderr);
  }

  // TODO(bugs.webrtc.org/14248): Remove the need to generate a file
  // and read the file directly from memory.
  if (has_generated_wav_file) {
    RTC_CHECK_EQ(std::remove(wav_path.c_str()), 0)
        << "Failed to remove " << wav_path;
  }
  return 0;
}
