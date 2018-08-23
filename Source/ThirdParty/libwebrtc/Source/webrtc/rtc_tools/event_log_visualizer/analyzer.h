/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "modules/audio_coding/neteq/tools/neteq_stats_getter.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"
#include "rtc_tools/event_log_visualizer/triage_notifications.h"

namespace webrtc {

class EventLogAnalyzer {
 public:
  // The EventLogAnalyzer keeps a reference to the ParsedRtcEventLogNew for the
  // duration of its lifetime. The ParsedRtcEventLogNew must not be destroyed or
  // modified while the EventLogAnalyzer is being used.
  EventLogAnalyzer(const ParsedRtcEventLogNew& log, bool normalize_time);

  void CreatePacketGraph(PacketDirection direction, Plot* plot);

  void CreateAccumulatedPacketsGraph(PacketDirection direction, Plot* plot);

  void CreatePlayoutGraph(Plot* plot);

  void CreateAudioLevelGraph(PacketDirection direction, Plot* plot);

  void CreateSequenceNumberGraph(Plot* plot);

  void CreateIncomingPacketLossGraph(Plot* plot);

  void CreateIncomingDelayDeltaGraph(Plot* plot);
  void CreateIncomingDelayGraph(Plot* plot);

  void CreateFractionLossGraph(Plot* plot);

  void CreateTotalIncomingBitrateGraph(Plot* plot);
  void CreateTotalOutgoingBitrateGraph(Plot* plot,
                                       bool show_detector_state = false,
                                       bool show_alr_state = false);

  void CreateStreamBitrateGraph(PacketDirection direction, Plot* plot);

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

  void CreateAudioEncoderTargetBitrateGraph(Plot* plot);
  void CreateAudioEncoderFrameLengthGraph(Plot* plot);
  void CreateAudioEncoderPacketLossGraph(Plot* plot);
  void CreateAudioEncoderEnableFecGraph(Plot* plot);
  void CreateAudioEncoderEnableDtxGraph(Plot* plot);
  void CreateAudioEncoderNumChannelsGraph(Plot* plot);

  using NetEqStatsGetterMap =
      std::map<uint32_t, std::unique_ptr<test::NetEqStatsGetter>>;
  NetEqStatsGetterMap SimulateNetEq(const std::string& replacement_file_name,
                                    int file_sample_rate_hz) const;

  void CreateAudioJitterBufferGraph(uint32_t ssrc,
                                    const test::NetEqStatsGetter* stats_getter,
                                    Plot* plot) const;
  void CreateNetEqNetworkStatsGraph(
      const NetEqStatsGetterMap& neteq_stats_getters,
      rtc::FunctionView<float(const NetEqNetworkStatistics&)> stats_extractor,
      const std::string& plot_name,
      Plot* plot) const;
  void CreateNetEqLifetimeStatsGraph(
      const NetEqStatsGetterMap& neteq_stats_getters,
      rtc::FunctionView<float(const NetEqLifetimeStatistics&)> stats_extractor,
      const std::string& plot_name,
      Plot* plot) const;

  void CreateIceCandidatePairConfigGraph(Plot* plot);
  void CreateIceConnectivityCheckGraph(Plot* plot);

  void CreateTriageNotifications();
  void PrintNotifications(FILE* file);

 private:
  bool IsRtxSsrc(PacketDirection direction, uint32_t ssrc) const {
    if (direction == kIncomingPacket) {
      return parsed_log_.incoming_rtx_ssrcs().find(ssrc) !=
             parsed_log_.incoming_rtx_ssrcs().end();
    } else {
      return parsed_log_.outgoing_rtx_ssrcs().find(ssrc) !=
             parsed_log_.outgoing_rtx_ssrcs().end();
    }
  }

  bool IsVideoSsrc(PacketDirection direction, uint32_t ssrc) const {
    if (direction == kIncomingPacket) {
      return parsed_log_.incoming_video_ssrcs().find(ssrc) !=
             parsed_log_.incoming_video_ssrcs().end();
    } else {
      return parsed_log_.outgoing_video_ssrcs().find(ssrc) !=
             parsed_log_.outgoing_video_ssrcs().end();
    }
  }

  bool IsAudioSsrc(PacketDirection direction, uint32_t ssrc) const {
    if (direction == kIncomingPacket) {
      return parsed_log_.incoming_audio_ssrcs().find(ssrc) !=
             parsed_log_.incoming_audio_ssrcs().end();
    } else {
      return parsed_log_.outgoing_audio_ssrcs().find(ssrc) !=
             parsed_log_.outgoing_audio_ssrcs().end();
    }
  }

  template <typename NetEqStatsType>
  void CreateNetEqStatsGraphInternal(
      const NetEqStatsGetterMap& neteq_stats,
      rtc::FunctionView<const std::vector<std::pair<int64_t, NetEqStatsType>>*(
          const test::NetEqStatsGetter*)> data_extractor,
      rtc::FunctionView<float(const NetEqStatsType&)> stats_extractor,
      const std::string& plot_name,
      Plot* plot) const;

  template <typename IterableType>
  void CreateAccumulatedPacketsTimeSeries(Plot* plot,
                                          const IterableType& packets,
                                          const std::string& label);

  void CreateStreamGapAlerts(PacketDirection direction);
  void CreateTransmissionGapAlerts(PacketDirection direction);

  std::string GetStreamName(PacketDirection direction, uint32_t ssrc) const {
    char buffer[200];
    rtc::SimpleStringBuilder name(buffer);
    if (IsAudioSsrc(direction, ssrc)) {
      name << "Audio ";
    } else if (IsVideoSsrc(direction, ssrc)) {
      name << "Video ";
    } else {
      name << "Unknown ";
    }
    if (IsRtxSsrc(direction, ssrc)) {
      name << "RTX ";
    }
    if (direction == kIncomingPacket)
      name << "(In) ";
    else
      name << "(Out) ";
    name << "SSRC " << ssrc;
    return name.str();
  }

  int64_t ToCallTimeUs(int64_t timestamp) const;
  float ToCallTimeSec(int64_t timestamp) const;

  void Alert_RtpLogTimeGap(PacketDirection direction,
                           float time_seconds,
                           int64_t duration) {
    if (direction == kIncomingPacket) {
      incoming_rtp_recv_time_gaps_.emplace_back(time_seconds, duration);
    } else {
      outgoing_rtp_send_time_gaps_.emplace_back(time_seconds, duration);
    }
  }

  void Alert_RtcpLogTimeGap(PacketDirection direction,
                            float time_seconds,
                            int64_t duration) {
    if (direction == kIncomingPacket) {
      incoming_rtcp_recv_time_gaps_.emplace_back(time_seconds, duration);
    } else {
      outgoing_rtcp_send_time_gaps_.emplace_back(time_seconds, duration);
    }
  }

  void Alert_SeqNumJump(PacketDirection direction,
                        float time_seconds,
                        uint32_t ssrc) {
    if (direction == kIncomingPacket) {
      incoming_seq_num_jumps_.emplace_back(time_seconds, ssrc);
    } else {
      outgoing_seq_num_jumps_.emplace_back(time_seconds, ssrc);
    }
  }

  void Alert_CaptureTimeJump(PacketDirection direction,
                             float time_seconds,
                             uint32_t ssrc) {
    if (direction == kIncomingPacket) {
      incoming_capture_time_jumps_.emplace_back(time_seconds, ssrc);
    } else {
      outgoing_capture_time_jumps_.emplace_back(time_seconds, ssrc);
    }
  }

  void Alert_OutgoingHighLoss(double avg_loss_fraction) {
    outgoing_high_loss_alerts_.emplace_back(avg_loss_fraction);
  }

  std::string GetCandidatePairLogDescriptionFromId(uint32_t candidate_pair_id);

  const ParsedRtcEventLogNew& parsed_log_;

  // A list of SSRCs we are interested in analysing.
  // If left empty, all SSRCs will be considered relevant.
  std::vector<uint32_t> desired_ssrc_;

  // Stores the timestamps for all log segments, in the form of associated start
  // and end events.
  std::vector<std::pair<int64_t, int64_t>> log_segments_;

  std::vector<IncomingRtpReceiveTimeGap> incoming_rtp_recv_time_gaps_;
  std::vector<IncomingRtcpReceiveTimeGap> incoming_rtcp_recv_time_gaps_;
  std::vector<OutgoingRtpSendTimeGap> outgoing_rtp_send_time_gaps_;
  std::vector<OutgoingRtcpSendTimeGap> outgoing_rtcp_send_time_gaps_;
  std::vector<IncomingSeqNumJump> incoming_seq_num_jumps_;
  std::vector<IncomingCaptureTimeJump> incoming_capture_time_jumps_;
  std::vector<OutgoingSeqNoJump> outgoing_seq_num_jumps_;
  std::vector<OutgoingCaptureTimeJump> outgoing_capture_time_jumps_;
  std::vector<OutgoingHighLoss> outgoing_high_loss_alerts_;

  std::map<uint32_t, std::string> candidate_pair_desc_by_id_;

  // Window and step size used for calculating moving averages, e.g. bitrate.
  // The generated data points will be |step_| microseconds apart.
  // Only events occuring at most |window_duration_| microseconds before the
  // current data point will be part of the average.
  int64_t window_duration_;
  int64_t step_;

  // First and last events of the log.
  int64_t begin_time_;
  int64_t end_time_;
  const bool normalize_time_;

  // Duration (in seconds) of log file.
  float call_duration_s_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
