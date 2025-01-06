/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyze_audio.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "api/function_view.h"
#include "api/make_ref_counted.h"
#include "api/neteq/neteq.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "modules/audio_coding/neteq/tools/neteq_stats_getter.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "rtc_base/checks.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

void CreateAudioEncoderTargetBitrateGraph(const ParsedRtcEventLog& parsed_log,
                                          const AnalyzerConfig& config,
                                          Plot* plot) {
  TimeSeries time_series("Audio encoder target bitrate", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaBitrateBps = [](const LoggedAudioNetworkAdaptationEvent& ana_event)
      -> std::optional<float> {
    if (ana_event.config.bitrate_bps)
      return std::optional<float>(
          static_cast<float>(*ana_event.config.bitrate_bps));
    return std::nullopt;
  };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaBitrateBps,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (bps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder target bitrate");
}

void CreateAudioEncoderFrameLengthGraph(const ParsedRtcEventLog& parsed_log,
                                        const AnalyzerConfig& config,
                                        Plot* plot) {
  TimeSeries time_series("Audio encoder frame length", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaFrameLengthMs =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.frame_length_ms)
          return std::optional<float>(
              static_cast<float>(*ana_event.config.frame_length_ms));
        return std::optional<float>();
      };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaFrameLengthMs,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Frame length (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder frame length");
}

void CreateAudioEncoderPacketLossGraph(const ParsedRtcEventLog& parsed_log,
                                       const AnalyzerConfig& config,
                                       Plot* plot) {
  TimeSeries time_series("Audio encoder uplink packet loss fraction",
                         LineStyle::kLine, PointStyle::kHighlight);
  auto GetAnaPacketLoss =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.uplink_packet_loss_fraction)
          return std::optional<float>(static_cast<float>(
              *ana_event.config.uplink_packet_loss_fraction));
        return std::optional<float>();
      };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaPacketLoss,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported audio encoder lost packets");
}

void CreateAudioEncoderEnableFecGraph(const ParsedRtcEventLog& parsed_log,
                                      const AnalyzerConfig& config,
                                      Plot* plot) {
  TimeSeries time_series("Audio encoder FEC", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaFecEnabled =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_fec)
          return std::optional<float>(
              static_cast<float>(*ana_event.config.enable_fec));
        return std::optional<float>();
      };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaFecEnabled,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "FEC (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder FEC");
}

void CreateAudioEncoderEnableDtxGraph(const ParsedRtcEventLog& parsed_log,
                                      const AnalyzerConfig& config,
                                      Plot* plot) {
  TimeSeries time_series("Audio encoder DTX", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaDtxEnabled =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_dtx)
          return std::optional<float>(
              static_cast<float>(*ana_event.config.enable_dtx));
        return std::optional<float>();
      };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaDtxEnabled,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "DTX (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder DTX");
}

void CreateAudioEncoderNumChannelsGraph(const ParsedRtcEventLog& parsed_log,
                                        const AnalyzerConfig& config,
                                        Plot* plot) {
  TimeSeries time_series("Audio encoder number of channels", LineStyle::kLine,
                         PointStyle::kHighlight);
  auto GetAnaNumChannels =
      [](const LoggedAudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.num_channels)
          return std::optional<float>(
              static_cast<float>(*ana_event.config.num_channels));
        return std::optional<float>();
      };
  auto ToCallTime = [config](const LoggedAudioNetworkAdaptationEvent& packet) {
    return config.GetCallTimeSec(packet.log_time());
  };
  ProcessPoints<LoggedAudioNetworkAdaptationEvent>(
      ToCallTime, GetAnaNumChannels,
      parsed_log.audio_network_adaptation_events(), &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Number of channels (1 (mono)/2 (stereo))",
                          kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder number of channels");
}

namespace {

// Factory to create a "replacement decoder" that produces the decoded audio
// by reading from a file rather than from the encoded payloads.
class ReplacementAudioDecoderFactory : public AudioDecoderFactory {
 public:
  ReplacementAudioDecoderFactory(const absl::string_view replacement_file_name,
                                 int file_sample_rate_hz)
      : replacement_file_name_(replacement_file_name),
        file_sample_rate_hz_(file_sample_rate_hz) {}

  std::vector<AudioCodecSpec> GetSupportedDecoders() override {
    RTC_DCHECK_NOTREACHED();
    return {};
  }

  bool IsSupportedDecoder(const SdpAudioFormat& format) override {
    return true;
  }

  std::unique_ptr<AudioDecoder> Create(
      const Environment& env,
      const SdpAudioFormat& format,
      std::optional<AudioCodecPairId> codec_pair_id) override {
    auto replacement_file = std::make_unique<test::ResampleInputAudioFile>(
        replacement_file_name_, file_sample_rate_hz_);
    replacement_file->set_output_rate_hz(48000);
    return std::make_unique<test::FakeDecodeFromFile>(
        std::move(replacement_file), 48000, false);
  }

 private:
  const std::string replacement_file_name_;
  const int file_sample_rate_hz_;
};

// Creates a NetEq test object and all necessary input and output helpers. Runs
// the test and returns the NetEqDelayAnalyzer object that was used to
// instrument the test.
std::unique_ptr<test::NetEqStatsGetter> CreateNetEqTestAndRun(
    ParsedRtcEventLog parsed_log,
    uint32_t ssrc,
    const std::string& replacement_file_name,
    int file_sample_rate_hz) {
  std::unique_ptr<test::NetEqInput> input =
      test::CreateNetEqEventLogInput(parsed_log, ssrc);
  if (!input) {
    return nullptr;
  }

  constexpr int kReplacementPt = 127;
  std::set<uint8_t> cn_types;
  std::set<uint8_t> forbidden_types;
  input.reset(new test::NetEqReplacementInput(std::move(input), kReplacementPt,
                                              cn_types, forbidden_types));

  std::unique_ptr<test::VoidAudioSink> output(new test::VoidAudioSink());

  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory =
      rtc::make_ref_counted<ReplacementAudioDecoderFactory>(
          replacement_file_name, file_sample_rate_hz);

  test::NetEqTest::DecoderMap codecs = {
      {kReplacementPt, SdpAudioFormat("l16", 48000, 1)}};

  std::unique_ptr<test::NetEqDelayAnalyzer> delay_cb(
      new test::NetEqDelayAnalyzer);
  std::unique_ptr<test::NetEqStatsGetter> neteq_stats_getter(
      new test::NetEqStatsGetter(std::move(delay_cb)));
  test::DefaultNetEqTestErrorCallback error_cb;
  test::NetEqTest::Callbacks callbacks;
  callbacks.error_callback = &error_cb;
  callbacks.post_insert_packet = neteq_stats_getter->delay_analyzer();
  callbacks.get_audio_callback = neteq_stats_getter.get();

  NetEq::Config config;
  test::NetEqTest test(config, decoder_factory, codecs, /*text_log=*/nullptr,
                       /*factory=*/nullptr, std::move(input), std::move(output),
                       callbacks, field_trial::GetFieldTrialString());
  test.Run();
  return neteq_stats_getter;
}
}  // namespace

NetEqStatsGetterMap SimulateNetEq(const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  const std::string& replacement_file_name,
                                  int file_sample_rate_hz) {
  NetEqStatsGetterMap neteq_stats;
  for (uint32_t ssrc : parsed_log.incoming_audio_ssrcs()) {
    std::unique_ptr<test::NetEqStatsGetter> stats = CreateNetEqTestAndRun(
        parsed_log, ssrc, replacement_file_name, file_sample_rate_hz);
    if (stats) {
      neteq_stats[ssrc] = std::move(stats);
    }
  }
  return neteq_stats;
}

// Given a NetEqStatsGetter and the SSRC that the NetEqStatsGetter was created
// for, this method generates a plot for the jitter buffer delay profile.
void CreateAudioJitterBufferGraph(const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  uint32_t ssrc,
                                  const test::NetEqStatsGetter* stats_getter,
                                  Plot* plot) {
  test::NetEqDelayAnalyzer::Delays arrival_delay_ms;
  test::NetEqDelayAnalyzer::Delays corrected_arrival_delay_ms;
  test::NetEqDelayAnalyzer::Delays playout_delay_ms;
  test::NetEqDelayAnalyzer::Delays target_delay_ms;

  stats_getter->delay_analyzer()->CreateGraphs(
      &arrival_delay_ms, &corrected_arrival_delay_ms, &playout_delay_ms,
      &target_delay_ms);

  TimeSeries time_series_packet_arrival("packet arrival delay",
                                        LineStyle::kLine);
  TimeSeries time_series_relative_packet_arrival(
      "Relative packet arrival delay", LineStyle::kLine);
  TimeSeries time_series_play_time("Playout delay", LineStyle::kLine);
  TimeSeries time_series_target_time("Target delay", LineStyle::kLine,
                                     PointStyle::kHighlight);

  for (const auto& data : arrival_delay_ms) {
    const float x = config.GetCallTimeSec(Timestamp::Millis(data.first));
    const float y = data.second;
    time_series_packet_arrival.points.emplace_back(TimeSeriesPoint(x, y));
  }
  for (const auto& data : corrected_arrival_delay_ms) {
    const float x = config.GetCallTimeSec(Timestamp::Millis(data.first));
    const float y = data.second;
    time_series_relative_packet_arrival.points.emplace_back(
        TimeSeriesPoint(x, y));
  }
  for (const auto& data : playout_delay_ms) {
    const float x = config.GetCallTimeSec(Timestamp::Millis(data.first));
    const float y = data.second;
    time_series_play_time.points.emplace_back(TimeSeriesPoint(x, y));
  }
  for (const auto& data : target_delay_ms) {
    const float x = config.GetCallTimeSec(Timestamp::Millis(data.first));
    const float y = data.second;
    time_series_target_time.points.emplace_back(TimeSeriesPoint(x, y));
  }

  plot->AppendTimeSeries(std::move(time_series_packet_arrival));
  plot->AppendTimeSeries(std::move(time_series_relative_packet_arrival));
  plot->AppendTimeSeries(std::move(time_series_play_time));
  plot->AppendTimeSeries(std::move(time_series_target_time));

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Relative delay (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("NetEq timing for " +
                 GetStreamName(parsed_log, kIncomingPacket, ssrc));
}

template <typename NetEqStatsType>
void CreateNetEqStatsGraphInternal(
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<const std::vector<std::pair<int64_t, NetEqStatsType>>*(
        const test::NetEqStatsGetter*)> data_extractor,
    rtc::FunctionView<float(const NetEqStatsType&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;

  for (const auto& st : neteq_stats) {
    const uint32_t ssrc = st.first;
    const std::vector<std::pair<int64_t, NetEqStatsType>>* data_vector =
        data_extractor(st.second.get());
    for (const auto& data : *data_vector) {
      const float time = config.GetCallTimeSec(Timestamp::Millis(data.first));
      const float value = stats_extractor(data.second);
      time_series[ssrc].points.emplace_back(TimeSeriesPoint(time, value));
    }
  }

  for (auto& series : time_series) {
    series.second.label =
        GetStreamName(parsed_log, kIncomingPacket, series.first);
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, plot_name, kBottomMargin, kTopMargin);
  plot->SetTitle(plot_name);
}

void CreateNetEqNetworkStatsGraph(
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<float(const NetEqNetworkStatistics&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) {
  CreateNetEqStatsGraphInternal<NetEqNetworkStatistics>(
      parsed_log, config, neteq_stats,
      [](const test::NetEqStatsGetter* stats_getter) {
        return stats_getter->stats();
      },
      stats_extractor, plot_name, plot);
}

void CreateNetEqLifetimeStatsGraph(
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    const NetEqStatsGetterMap& neteq_stats,
    rtc::FunctionView<float(const NetEqLifetimeStatistics&)> stats_extractor,
    const std::string& plot_name,
    Plot* plot) {
  CreateNetEqStatsGraphInternal<NetEqLifetimeStatistics>(
      parsed_log, config, neteq_stats,
      [](const test::NetEqStatsGetter* stats_getter) {
        return stats_getter->lifetime_stats();
      },
      stats_extractor, plot_name, plot);
}

}  // namespace webrtc
