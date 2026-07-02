/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyze_rtp_rtcp.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "api/function_view.h"
#include "api/rtp_headers.h"
#include "api/transport/ecn_marking.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/target_bitrate.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"
#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

namespace webrtc {

namespace {

template <typename T>
TimeSeries CreateRtcpTypeTimeSeries(const std::vector<T>& rtcp_list,
                                    AnalyzerConfig config,
                                    std::string rtcp_name,
                                    int category_id) {
  TimeSeries time_series(rtcp_name, LineStyle::kNone, PointStyle::kHighlight);
  for (const auto& rtcp : rtcp_list) {
    float x = config.GetCallTimeSec(rtcp.timestamp);
    float y = category_id;
    time_series.points.emplace_back(x, y);
  }
  return time_series;
}

struct PacketLossSummary {
  size_t num_packets = 0;
  size_t num_lost_packets = 0;
  Timestamp base_time = Timestamp::MinusInfinity();
};

}  // namespace

float GetHighestSeqNumber(const rtcp::ReportBlock& block) {
  return block.extended_high_seq_num();
}

float GetFractionLost(const rtcp::ReportBlock& block) {
  return static_cast<double>(block.fraction_lost()) / 256 * 100;
}

float GetCumulativeLost(const rtcp::ReportBlock& block) {
  return block.cumulative_lost();
}

float DelaySinceLastSr(const rtcp::ReportBlock& block) {
  return static_cast<double>(block.delay_since_last_sr()) / 65536;
}

void CreatePacketGraph(PacketDirection direction,
                       const ParsedRtcEventLog& parsed_log,
                       const AnalyzerConfig& config,
                       Plot* plot) {
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(parsed_log, direction, stream.ssrc),
                           LineStyle::kBar);
    auto GetPacketSize = [](const LoggedRtpPacket& packet) {
      return std::optional<float>(packet.total_length);
    };
    auto ToCallTime = [&config](const LoggedRtpPacket& packet) {
      return config.GetCallTimeSec(packet.timestamp);
    };
    ProcessPoints<LoggedRtpPacket>(ToCallTime, GetPacketSize,
                                   stream.packet_view, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet size (bytes)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " RTP packets");
}

void CreateRtcpTypeGraph(PacketDirection direction,
                         const ParsedRtcEventLog& parsed_log,
                         const AnalyzerConfig& config,
                         Plot* plot) {
  plot->AppendTimeSeries(CreateRtcpTypeTimeSeries(
      parsed_log.transport_feedbacks(direction), config, "TWCC", 1));
  plot->AppendTimeSeries(CreateRtcpTypeTimeSeries(
      parsed_log.congestion_feedback(direction), config, "CCFB", 2));
  plot->AppendTimeSeries(CreateRtcpTypeTimeSeries(
      parsed_log.receiver_reports(direction), config, "RR", 3));
  plot->AppendTimeSeries(CreateRtcpTypeTimeSeries(
      parsed_log.sender_reports(direction), config, "SR", 4));
  plot->AppendTimeSeries(CreateRtcpTypeTimeSeries(
      parsed_log.extended_reports(direction), config, "XR", 5));
  plot->AppendTimeSeries(
      CreateRtcpTypeTimeSeries(parsed_log.nacks(direction), config, "NACK", 6));
  plot->AppendTimeSeries(
      CreateRtcpTypeTimeSeries(parsed_log.rembs(direction), config, "REMB", 7));
  plot->AppendTimeSeries(
      CreateRtcpTypeTimeSeries(parsed_log.firs(direction), config, "FIR", 8));
  plot->AppendTimeSeries(
      CreateRtcpTypeTimeSeries(parsed_log.plis(direction), config, "PLI", 9));
  plot->AppendTimeSeries(
      CreateRtcpTypeTimeSeries(parsed_log.byes(direction), config, "BYE", 10));
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "RTCP type", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " RTCP packets");
  plot->SetYAxisTickLabels({{1, "TWCC"},
                            {2, "CCFB"},
                            {3, "RR"},
                            {4, "SR"},
                            {5, "XR"},
                            {6, "NACK"},
                            {7, "REMB"},
                            {8, "FIR"},
                            {9, "PLI"},
                            {10, "BYE"}});
}

template <typename IterableType>
void CreateAccumulatedPacketsTimeSeries(Plot* plot,
                                        const AnalyzerConfig& config,
                                        const IterableType& packets,
                                        const std::string& label) {
  TimeSeries time_series(label, LineStyle::kStep);
  for (size_t i = 0; i < packets.size(); i++) {
    float x = config.GetCallTimeSec(packets[i].log_time());
    time_series.points.emplace_back(x, i + 1);
  }
  plot->AppendTimeSeries(std::move(time_series));
}

void CreateAccumulatedPacketsGraph(PacketDirection direction,
                                   const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot) {
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_))
      continue;
    std::string label =
        std::string("RTP ") + GetStreamName(parsed_log, direction, stream.ssrc);
    CreateAccumulatedPacketsTimeSeries(plot, config, stream.packet_view, label);
  }
  std::string label =
      std::string("RTCP ") + "(" + GetDirectionAsShortString(direction) + ")";
  if (direction == kIncomingPacket) {
    CreateAccumulatedPacketsTimeSeries(
        plot, config, parsed_log.incoming_rtcp_packets(), label);
  } else {
    CreateAccumulatedPacketsTimeSeries(
        plot, config, parsed_log.outgoing_rtcp_packets(), label);
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Received Packets", kBottomMargin, kTopMargin);
  plot->SetTitle(std::string("Accumulated ") + GetDirectionAsString(direction) +
                 " RTP/RTCP packets");
}

void CreatePacketRateGraph(PacketDirection direction,
                           const ParsedRtcEventLog& parsed_log,
                           const AnalyzerConfig& config,
                           Plot* plot) {
  auto CountPackets = [](auto packet) { return 1.0; };
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_)) {
      continue;
    }
    TimeSeries time_series(
        std::string("RTP ") + GetStreamName(parsed_log, direction, stream.ssrc),
        LineStyle::kLine);
    MovingAverage<LoggedRtpPacket, double>(CountPackets, stream.packet_view,
                                           config, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }
  TimeSeries time_series(
      std::string("RTCP ") + "(" + GetDirectionAsShortString(direction) + ")",
      LineStyle::kLine);
  if (direction == kIncomingPacket) {
    MovingAverage<LoggedRtcpPacketIncoming, double>(
        CountPackets, parsed_log.incoming_rtcp_packets(), config, &time_series);
  } else {
    MovingAverage<LoggedRtcpPacketOutgoing, double>(
        CountPackets, parsed_log.outgoing_rtcp_packets(), config, &time_series);
  }
  plot->AppendTimeSeries(std::move(time_series));

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet Rate (packets/s)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Rate of " + GetDirectionAsString(direction) +
                 " RTP/RTCP packets");
}

void CreateTotalPacketRateGraph(PacketDirection direction,
                                const ParsedRtcEventLog& parsed_log,
                                const AnalyzerConfig& config,
                                Plot* plot) {
  // Contains a log timestamp to enable counting logged events of different
  // types using MovingAverage().
  class LogTime {
   public:
    explicit LogTime(Timestamp log_time) : log_time_(log_time) {}
    Timestamp log_time() const { return log_time_; }

   private:
    Timestamp log_time_;
  };
  std::vector<LogTime> packet_times;
  auto handle_rtp = [&packet_times](const LoggedRtpPacket& packet) {
    packet_times.emplace_back(packet.log_time());
  };
  RtcEventProcessor process;
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    process.AddEvents(stream.packet_view, handle_rtp, direction);
  }
  if (direction == kIncomingPacket) {
    auto handle_incoming_rtcp =
        [&packet_times](const LoggedRtcpPacketIncoming& packet) {
          packet_times.emplace_back(packet.log_time());
        };
    process.AddEvents(parsed_log.incoming_rtcp_packets(), handle_incoming_rtcp);
  } else {
    auto handle_outgoing_rtcp =
        [&packet_times](const LoggedRtcpPacketOutgoing& packet) {
          packet_times.emplace_back(packet.log_time());
        };
    process.AddEvents(parsed_log.outgoing_rtcp_packets(), handle_outgoing_rtcp);
  }
  process.ProcessEventsInOrder();
  TimeSeries time_series(std::string("Total ") + "(" +
                             GetDirectionAsShortString(direction) + ") packets",
                         LineStyle::kLine);
  MovingAverage<LogTime, uint64_t>([](auto packet) { return 1; }, packet_times,
                                   config, &time_series);
  plot->AppendTimeSeries(std::move(time_series));

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet Rate (packets/s)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Rate of all " + GetDirectionAsString(direction) +
                 " RTP/RTCP packets");
}

// For each SSRC, plot the sequence number difference between consecutive
// incoming packets.
void CreateSequenceNumberGraph(const ParsedRtcEventLog& parsed_log,
                               const AnalyzerConfig& config,
                               Plot* plot) {
  for (const auto& stream : parsed_log.incoming_rtp_packets_by_ssrc()) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(
        GetStreamName(parsed_log, kIncomingPacket, stream.ssrc),
        LineStyle::kBar);
    auto GetSequenceNumberDiff = [](const LoggedRtpPacketIncoming& old_packet,
                                    const LoggedRtpPacketIncoming& new_packet) {
      int64_t diff =
          WrappingDifference(new_packet.rtp.header.sequenceNumber,
                             old_packet.rtp.header.sequenceNumber, 1ul << 16);
      return diff;
    };
    auto ToCallTime = [&config](const LoggedRtpPacketIncoming& packet) {
      return config.GetCallTimeSec(packet.log_time());
    };
    ProcessPairs<LoggedRtpPacketIncoming, float>(
        ToCallTime, GetSequenceNumberDiff, stream.incoming_packets,
        &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Difference since last packet", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Incoming sequence number delta");
}

void CreateIncomingPacketLossGraph(const ParsedRtcEventLog& parsed_log,
                                   const AnalyzerConfig& config,
                                   Plot* plot) {
  for (const auto& stream : parsed_log.incoming_rtp_packets_by_ssrc()) {
    const std::vector<LoggedRtpPacketIncoming>& packets =
        stream.incoming_packets;
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_) || packets.empty()) {
      continue;
    }

    TimeSeries time_series(
        GetStreamName(parsed_log, kIncomingPacket, stream.ssrc),
        LineStyle::kLine, PointStyle::kHighlight);
    // TODO(terelius): Should the window and step size be read from the class
    // instead?
    const TimeDelta kWindow = TimeDelta::Millis(1000);
    const TimeDelta kStep = TimeDelta::Millis(1000);
    SeqNumUnwrapper<uint16_t> unwrapper_;
    SeqNumUnwrapper<uint16_t> prior_unwrapper_;
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    uint64_t highest_seq_number =
        unwrapper_.Unwrap(packets[0].rtp.header.sequenceNumber) - 1;
    uint64_t highest_prior_seq_number =
        prior_unwrapper_.Unwrap(packets[0].rtp.header.sequenceNumber) - 1;

    for (Timestamp t = config.begin_time_; t < config.end_time_ + kStep;
         t += kStep) {
      while (window_index_end < packets.size() &&
             packets[window_index_end].rtp.log_time() < t) {
        uint64_t sequence_number = unwrapper_.Unwrap(
            packets[window_index_end].rtp.header.sequenceNumber);
        highest_seq_number = std::max(highest_seq_number, sequence_number);
        ++window_index_end;
      }
      while (window_index_begin < packets.size() &&
             packets[window_index_begin].rtp.log_time() < t - kWindow) {
        uint64_t sequence_number = prior_unwrapper_.Unwrap(
            packets[window_index_begin].rtp.header.sequenceNumber);
        highest_prior_seq_number =
            std::max(highest_prior_seq_number, sequence_number);
        ++window_index_begin;
      }
      float x = config.GetCallTimeSec(t);
      uint64_t expected_packets = highest_seq_number - highest_prior_seq_number;
      if (expected_packets > 0) {
        int64_t received_packets = window_index_end - window_index_begin;
        int64_t lost_packets = expected_packets - received_packets;
        float y = static_cast<float>(lost_packets) / expected_packets * 100;
        time_series.points.emplace_back(x, y);
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Loss rate (in %)", kBottomMargin, kTopMargin);
  plot->SetTitle("Incoming packet loss (derived from incoming packets)");
}

// For each SSRC, plot the bandwidth used by that stream.
void CreateStreamBitrateGraph(PacketDirection direction,
                              const ParsedRtcEventLog& parsed_log,
                              const AnalyzerConfig& config,
                              Plot* plot) {
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    // Filter on SSRC.
    if (!MatchingSsrc(stream.ssrc, config.desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName(parsed_log, direction, stream.ssrc),
                           LineStyle::kLine);
    auto GetPacketSizeKilobits = [](const LoggedRtpPacket& packet) {
      return packet.total_length * 8.0 / 1000.0;
    };
    MovingAverage<LoggedRtpPacket, double>(
        GetPacketSizeKilobits, stream.packet_view, config, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " bitrate per stream");
}

// Plot the bitrate allocation for each temporal and spatial layer.
// Computed from RTCP XR target bitrate block, so the graph is only populated if
// those are sent.
void CreateBitrateAllocationGraph(PacketDirection direction,
                                  const ParsedRtcEventLog& parsed_log,
                                  const AnalyzerConfig& config,
                                  Plot* plot) {
  std::map<LayerDescription, TimeSeries> time_series;
  const auto& xr_list = parsed_log.extended_reports(direction);
  for (const auto& rtcp : xr_list) {
    const std::optional<rtcp::TargetBitrate>& target_bitrate =
        rtcp.xr.target_bitrate();
    if (!target_bitrate.has_value())
      continue;
    for (const auto& bitrate_item : target_bitrate->GetTargetBitrates()) {
      LayerDescription layer(rtcp.xr.sender_ssrc(), bitrate_item.spatial_layer,
                             bitrate_item.temporal_layer);
      auto time_series_it = time_series.find(layer);
      if (time_series_it == time_series.end()) {
        std::string layer_name = GetLayerName(layer);
        bool inserted;
        std::tie(time_series_it, inserted) = time_series.insert(
            std::make_pair(layer, TimeSeries(layer_name, LineStyle::kStep)));
        RTC_DCHECK(inserted);
      }
      float x = config.GetCallTimeSec(rtcp.log_time());
      float y = bitrate_item.target_bitrate_kbps;
      time_series_it->second.points.emplace_back(x, y);
    }
  }
  for (auto& layer : time_series) {
    plot->AppendTimeSeries(std::move(layer.second));
  }
  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  if (direction == kIncomingPacket)
    plot->SetTitle("Target bitrate per incoming layer");
  else
    plot->SetTitle("Target bitrate per outgoing layer");
}

void CreateEcnFeedbackGraph(Plot* plot,
                            PacketDirection direction,
                            const ParsedRtcEventLog& parsed_log,
                            const AnalyzerConfig& config) {
  TimeSeries not_ect("Not ECN capable", LineStyle::kBar,
                     PointStyle::kHighlight);
  TimeSeries ect_1("ECN capable", LineStyle::kBar, PointStyle::kHighlight);
  TimeSeries ce("Congestion experienced", LineStyle::kBar,
                PointStyle::kHighlight);
  TimeSeries reported_lost("Reported lost", LineStyle::kBar,
                           PointStyle::kHighlight);

  for (const LoggedRtcpCongestionControlFeedback& feedback :
       parsed_log.congestion_feedback(direction)) {
    int ect_1_count = 0;
    int not_ect_count = 0;
    int ce_count = 0;
    int reported_lost_count = 0;

    for (const rtcp::CongestionControlFeedback::PacketInfo& info :
         feedback.congestion_feedback.packets()) {
      if (!info.received()) {
        ++reported_lost_count;
        continue;
      }
      switch (info.ecn) {
        case EcnMarking::kNotEct:
          ++not_ect_count;
          break;
        case EcnMarking::kEct1:
          ++ect_1_count;
          break;
        case EcnMarking::kEct0:
          RTC_LOG(LS_ERROR) << "unexpected ect(0)";
          break;
        case EcnMarking::kCe:
          ++ce_count;
          break;
      }
    }
    ect_1.points.emplace_back(config.GetCallTimeSec(feedback.timestamp),
                              ect_1_count);
    not_ect.points.emplace_back(config.GetCallTimeSec(feedback.timestamp),
                                not_ect_count);
    ce.points.emplace_back(config.GetCallTimeSec(feedback.timestamp), ce_count);
    reported_lost.points.emplace_back(config.GetCallTimeSec(feedback.timestamp),
                                      reported_lost_count);
  }

  plot->AppendTimeSeriesIfNotEmpty(std::move(ect_1));
  plot->AppendTimeSeriesIfNotEmpty(std::move(not_ect));
  plot->AppendTimeSeriesIfNotEmpty(std::move(ce));
  plot->AppendTimeSeriesIfNotEmpty(std::move(reported_lost));

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Count per feedback", kBottomMargin,
                          kTopMargin);
}

void CreateOutgoingEcnFeedbackGraph(const ParsedRtcEventLog& parsed_log,
                                    const AnalyzerConfig& config,
                                    Plot* plot) {
  CreateEcnFeedbackGraph(plot, kOutgoingPacket, parsed_log, config);
  plot->SetTitle("Outgoing ECN count per feedback");
}

void CreateIncomingEcnFeedbackGraph(const ParsedRtcEventLog& parsed_log,
                                    const AnalyzerConfig& config,
                                    Plot* plot) {
  CreateEcnFeedbackGraph(plot, kIncomingPacket, parsed_log, config);
  plot->SetTitle("Incoming ECN count per feedback");
}

void CreateOutgoingLossRateGraph(const ParsedRtcEventLog& parsed_log,
                                 const AnalyzerConfig& config,
                                 Plot* plot) {
  struct PacketLossPerFeedback {
    Timestamp timestamp;              // Time when this feedback was received.
    int num_packets_in_feedback = 0;  // Includes lost packets.
    int num_lost_packets = 0;         // In this specific feedback.
    int num_reordered_packets = 0;    // Packets received in this feedback, but
                                      // was previously reported as lost.
    int num_missing_feedback =
        0;  // Packets missing feedback between this report and the previous.
  };

  class LossFeedbackBuilder {
   public:
    void AddPacket(uint16_t sequence_number, TimeDelta arrival_time_delta) {
      last_unwrapped_sequence_number_ =
          sequence_number_unwrapper_.Unwrap(sequence_number);
      if (!first_sequence_number_.has_value()) {
        first_sequence_number_ = last_unwrapped_sequence_number_;
      }
      ++num_packets_;
      if (arrival_time_delta.IsInfinite()) {
        lost_sequence_numbers_.insert(last_unwrapped_sequence_number_);
      } else {
        num_reordered_packets_ += previous_lost_sequence_numbers_.count(
            last_unwrapped_sequence_number_);
      }
    }

    void Update(PacketLossPerFeedback& feedback) {
      feedback.num_packets_in_feedback += num_packets_;
      feedback.num_lost_packets += lost_sequence_numbers_.size();
      feedback.num_reordered_packets += num_reordered_packets_;
      if (first_sequence_number_.has_value() &&
          previous_feedback_highest_seq_number_.has_value()) {
        feedback.num_missing_feedback +=
            *first_sequence_number_ - *previous_feedback_highest_seq_number_ -
            1;
      }

      // Prepare for next feedback.
      first_sequence_number_ = std::nullopt;
      previous_lost_sequence_numbers_.insert(lost_sequence_numbers_.begin(),
                                             lost_sequence_numbers_.end());
      previous_feedback_highest_seq_number_ = last_unwrapped_sequence_number_;
      lost_sequence_numbers_.clear();
      num_reordered_packets_ = 0;
      num_packets_ = 0;
    }

   private:
    int64_t last_unwrapped_sequence_number_ = 0;
    int num_reordered_packets_ = 0;
    int num_packets_ = 0;
    std::optional<int64_t> first_sequence_number_;

    std::unordered_set<int64_t> lost_sequence_numbers_;
    std::unordered_set<int64_t> previous_lost_sequence_numbers_;
    std::optional<int64_t> previous_feedback_highest_seq_number_;

    RtpSequenceNumberUnwrapper sequence_number_unwrapper_;
  };

  TimeSeries loss_rate_series("Loss rate (from packet feedback)",
                              LineStyle::kLine, PointStyle::kHighlight);
  TimeSeries reordered_packets_between_feedback(
      "Ratio of reordered packets from last feedback", LineStyle::kLine,
      PointStyle::kHighlight);
  TimeSeries average_loss_rate_series("Average loss rate last 5s",
                                      LineStyle::kLine, PointStyle::kHighlight);
  TimeSeries missing_feedback_series("Missing feedback", LineStyle::kNone,
                                     PointStyle::kHighlight);

  std::vector<PacketLossPerFeedback> loss_per_feedback;

  if (!parsed_log.congestion_feedback(kIncomingPacket).empty()) {
    plot->SetTitle("Outgoing loss rate (from CCFB)");

    std::map</*ssrc*/ uint32_t, LossFeedbackBuilder> per_ssrc_builder;
    for (const LoggedRtcpCongestionControlFeedback& feedback :
         parsed_log.congestion_feedback(kIncomingPacket)) {
      const rtcp::CongestionControlFeedback& transport_feedback =
          feedback.congestion_feedback;

      PacketLossPerFeedback packet_loss_per_feedback = {
          .timestamp = feedback.log_time()};
      for (const rtcp::CongestionControlFeedback::PacketInfo& packet :
           transport_feedback.packets()) {
        per_ssrc_builder[packet.ssrc].AddPacket(packet.sequence_number,
                                                packet.arrival_time_offset);
      }
      for (auto& [ssrc, builder] : per_ssrc_builder) {
        builder.Update(packet_loss_per_feedback);
      }
      loss_per_feedback.push_back(packet_loss_per_feedback);
    }
  } else if (!parsed_log.transport_feedbacks(kIncomingPacket).empty()) {
    plot->SetTitle("Outgoing loss rate (from TWCC)");

    LossFeedbackBuilder builder;
    for (const LoggedRtcpPacketTransportFeedback& feedback :
         parsed_log.transport_feedbacks(kIncomingPacket)) {
      feedback.transport_feedback.ForAllPackets(
          [&](uint16_t sequence_number, TimeDelta receive_time_delta) {
            builder.AddPacket(sequence_number, receive_time_delta);
          });
      PacketLossPerFeedback packet_loss_per_feedback = {
          .timestamp = feedback.log_time()};
      builder.Update(packet_loss_per_feedback);
      loss_per_feedback.push_back(packet_loss_per_feedback);
    }
  }

  PacketLossSummary window_summary;
  Timestamp last_observation_receive_time = Timestamp::Zero();

  // Use loss based bwe 2 observation duration and observation window size.
  constexpr TimeDelta kObservationDuration = TimeDelta::Millis(250);
  constexpr uint32_t kObservationWindowSize = 20;
  std::deque<PacketLossSummary> observations;
  int previous_feedback_size = 0;
  for (const PacketLossPerFeedback& feedback : loss_per_feedback) {
    for (int64_t num = 0; num < feedback.num_missing_feedback; ++num) {
      missing_feedback_series.points.emplace_back(
          config.GetCallTimeSec(feedback.timestamp), 100 + num);
    }

    // Compute loss rate from the transport feedback.
    float loss_rate = static_cast<float>(feedback.num_lost_packets * 100.0 /
                                         feedback.num_packets_in_feedback);

    loss_rate_series.points.emplace_back(
        config.GetCallTimeSec(feedback.timestamp), loss_rate);
    float reordered_rate =
        previous_feedback_size == 0
            ? 0
            : static_cast<float>(feedback.num_reordered_packets * 100.0 /
                                 previous_feedback_size);
    previous_feedback_size = feedback.num_packets_in_feedback;
    reordered_packets_between_feedback.points.emplace_back(
        config.GetCallTimeSec(feedback.timestamp), reordered_rate);

    // Compute loss rate in a window of kObservationWindowSize.
    if (window_summary.num_packets == 0) {
      window_summary.base_time = feedback.timestamp;
    }
    window_summary.num_packets += feedback.num_packets_in_feedback;
    window_summary.num_lost_packets +=
        feedback.num_lost_packets - feedback.num_reordered_packets;

    const Timestamp last_received_time = feedback.timestamp;
    const TimeDelta observation_duration =
        window_summary.base_time == Timestamp::Zero()
            ? TimeDelta::Zero()
            : last_received_time - window_summary.base_time;
    if (observation_duration > kObservationDuration) {
      last_observation_receive_time = last_received_time;
      observations.push_back(window_summary);
      if (observations.size() > kObservationWindowSize) {
        observations.pop_front();
      }

      // Compute average loss rate in a number of windows.
      int total_packets = 0;
      int total_loss = 0;
      for (const auto& observation : observations) {
        total_loss += observation.num_lost_packets;
        total_packets += observation.num_packets;
      }
      if (total_packets > 0) {
        float average_loss_rate = total_loss * 100.0 / total_packets;
        average_loss_rate_series.points.emplace_back(
            config.GetCallTimeSec(feedback.timestamp), average_loss_rate);
      } else {
        average_loss_rate_series.points.emplace_back(
            config.GetCallTimeSec(feedback.timestamp), 0);
      }
      window_summary = PacketLossSummary();
    }
  }
  // Add the data set to the plot.
  plot->AppendTimeSeriesIfNotEmpty(std::move(loss_rate_series));
  plot->AppendTimeSeriesIfNotEmpty(
      std::move(reordered_packets_between_feedback));
  plot->AppendTimeSeriesIfNotEmpty(std::move(average_loss_rate_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(missing_feedback_series));

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 100, "Loss rate (percent)", kBottomMargin,
                          kTopMargin);
}

void CreateTimestampGraph(PacketDirection direction,
                          const ParsedRtcEventLog& parsed_log,
                          const AnalyzerConfig& config,
                          Plot* plot) {
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    TimeSeries rtp_timestamps(
        GetStreamName(parsed_log, direction, stream.ssrc) + " capture-time",
        LineStyle::kLine, PointStyle::kHighlight);
    for (const auto& packet : stream.packet_view) {
      float x = config.GetCallTimeSec(packet.log_time());
      float y = packet.header.timestamp;
      rtp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(rtp_timestamps));

    TimeSeries rtcp_timestamps(
        GetStreamName(parsed_log, direction, stream.ssrc) +
            " rtcp capture-time",
        LineStyle::kLine, PointStyle::kHighlight);
    // TODO(terelius): Why only sender reports?
    const auto& sender_reports = parsed_log.sender_reports(direction);
    for (const auto& rtcp : sender_reports) {
      if (rtcp.sr.sender_ssrc() != stream.ssrc)
        continue;
      float x = config.GetCallTimeSec(rtcp.log_time());
      float y = rtcp.sr.rtp_timestamp();
      rtcp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeriesIfNotEmpty(std::move(rtcp_timestamps));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "RTP timestamp", kBottomMargin, kTopMargin);
  plot->SetTitle(GetDirectionAsString(direction) + " timestamps");
}

void CreateSenderAndReceiverReportPlot(
    PacketDirection direction,
    FunctionView<float(const rtcp::ReportBlock&)> fy,
    std::string title,
    std::string yaxis_label,
    const ParsedRtcEventLog& parsed_log,
    const AnalyzerConfig& config,
    Plot* plot) {
  std::map<uint32_t, TimeSeries> sr_reports_by_ssrc;
  const auto& sender_reports = parsed_log.sender_reports(direction);
  for (const auto& rtcp : sender_reports) {
    float x = config.GetCallTimeSec(rtcp.log_time());
    uint32_t ssrc = rtcp.sr.sender_ssrc();
    for (const auto& block : rtcp.sr.report_blocks()) {
      float y = fy(block);
      auto sr_report_it = sr_reports_by_ssrc.find(ssrc);
      bool inserted;
      if (sr_report_it == sr_reports_by_ssrc.end()) {
        std::tie(sr_report_it, inserted) = sr_reports_by_ssrc.emplace(
            ssrc, TimeSeries(GetStreamName(parsed_log, direction, ssrc) +
                                 " Sender Reports",
                             LineStyle::kLine, PointStyle::kHighlight));
      }
      sr_report_it->second.points.emplace_back(x, y);
    }
  }
  for (auto& kv : sr_reports_by_ssrc) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  std::map<uint32_t, TimeSeries> rr_reports_by_ssrc;
  const auto& receiver_reports = parsed_log.receiver_reports(direction);
  for (const auto& rtcp : receiver_reports) {
    float x = config.GetCallTimeSec(rtcp.log_time());
    uint32_t ssrc = rtcp.rr.sender_ssrc();
    for (const auto& block : rtcp.rr.report_blocks()) {
      float y = fy(block);
      auto rr_report_it = rr_reports_by_ssrc.find(ssrc);
      bool inserted;
      if (rr_report_it == rr_reports_by_ssrc.end()) {
        std::tie(rr_report_it, inserted) = rr_reports_by_ssrc.emplace(
            ssrc, TimeSeries(GetStreamName(parsed_log, direction, ssrc) +
                                 " Receiver Reports",
                             LineStyle::kLine, PointStyle::kHighlight));
      }
      rr_report_it->second.points.emplace_back(x, y);
    }
  }
  for (auto& kv : rr_reports_by_ssrc) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(config.CallBeginTimeSec(), config.CallEndTimeSec(), "Time (s)",
                 kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, yaxis_label, kBottomMargin, kTopMargin);
  plot->SetTitle(title);
}

}  // namespace webrtc
