/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/alerts.h"

#include <stdio.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"

namespace webrtc {

void TriageHelper::AnalyzeStreamGaps(const ParsedRtcEventLog& parsed_log,
                                     PacketDirection direction) {
  // With 100 packets/s (~800kbps), false positives would require 10 s without
  // data.
  constexpr int64_t kMaxSeqNumJump = 1000;
  // With a 90 kHz clock, false positives would require 10 s without data.
  constexpr int64_t kTicksPerMillisec = 90;
  constexpr int64_t kCaptureTimeGraceMs = 10000;

  std::string seq_num_explanation =
      direction == kIncomingPacket
          ? "Incoming RTP sequence number jumps more than 1000. Counter may "
            "have been reset or rewritten incorrectly in a group call."
          : "Outgoing RTP sequence number jumps more than 1000. Counter may "
            "have been reset.";
  std::string capture_time_explanation =
      direction == kIncomingPacket ? "Incoming capture time jumps more than "
                                     "10s. Clock might have been reset."
                                   : "Outgoing capture time jumps more than "
                                     "10s. Clock might have been reset.";
  TriageAlertType seq_num_alert = direction == kIncomingPacket
                                      ? TriageAlertType::kIncomingSeqNumJump
                                      : TriageAlertType::kOutgoingSeqNumJump;
  TriageAlertType capture_time_alert =
      direction == kIncomingPacket ? TriageAlertType::kIncomingCaptureTimeJump
                                   : TriageAlertType::kOutgoingCaptureTimeJump;

  const int64_t segment_end_us = parsed_log.first_log_segment().stop_time_us();

  // Check for gaps in sequence numbers and capture timestamps.
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    if (IsRtxSsrc(parsed_log, direction, stream.ssrc)) {
      continue;
    }
    auto packets = stream.packet_view;
    if (packets.empty()) {
      continue;
    }
    SeqNumUnwrapper<uint16_t> seq_num_unwrapper;
    int64_t last_seq_num =
        seq_num_unwrapper.Unwrap(packets[0].header.sequenceNumber);
    SeqNumUnwrapper<uint32_t> capture_time_unwrapper;
    int64_t last_capture_time =
        capture_time_unwrapper.Unwrap(packets[0].header.timestamp);
    int64_t last_log_time_ms = packets[0].log_time_ms();
    for (const auto& packet : packets) {
      if (packet.log_time_us() > segment_end_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }

      int64_t seq_num = seq_num_unwrapper.Unwrap(packet.header.sequenceNumber);
      if (std::abs(seq_num - last_seq_num) > kMaxSeqNumJump) {
        Alert(seq_num_alert, config_.GetCallTimeSec(packet.log_time()),
              seq_num_explanation);
      }
      last_seq_num = seq_num;

      int64_t capture_time =
          capture_time_unwrapper.Unwrap(packet.header.timestamp);
      if (std::abs(capture_time - last_capture_time) >
          kTicksPerMillisec *
              (kCaptureTimeGraceMs + packet.log_time_ms() - last_log_time_ms)) {
        Alert(capture_time_alert, config_.GetCallTimeSec(packet.log_time()),
              capture_time_explanation);
      }
      last_capture_time = capture_time;
    }
  }
}

void TriageHelper::AnalyzeTransmissionGaps(const ParsedRtcEventLog& parsed_log,
                                           PacketDirection direction) {
  constexpr int64_t kMaxRtpTransmissionGap = 500000;
  constexpr int64_t kMaxRtcpTransmissionGap = 3000000;
  std::string rtp_explanation =
      direction == kIncomingPacket
          ? "No RTP packets received for more than 500ms. This indicates a "
            "network problem. Temporary video freezes and choppy or robotic "
            "audio is unavoidable. Unnecessary BWE drops is a known issue."
          : "No RTP packets sent for more than 500 ms. This might be an issue "
            "with the pacer.";
  std::string rtcp_explanation =
      direction == kIncomingPacket
          ? "No RTCP packets received for more than 3 s. Could be a longer "
            "connection outage"
          : "No RTCP packets sent for more than 3 s. This is most likely a "
            "bug.";
  TriageAlertType rtp_alert = direction == kIncomingPacket
                                  ? TriageAlertType::kIncomingRtpGap
                                  : TriageAlertType::kOutgoingRtpGap;
  TriageAlertType rtcp_alert = direction == kIncomingPacket
                                   ? TriageAlertType::kIncomingRtcpGap
                                   : TriageAlertType::kOutgoingRtcpGap;

  const int64_t segment_end_us = parsed_log.first_log_segment().stop_time_us();

  // TODO(terelius): The parser could provide a list of all packets, ordered
  // by time, for each direction.
  std::multimap<int64_t, const LoggedRtpPacket*> rtp_in_direction;
  for (const auto& stream : parsed_log.rtp_packets_by_ssrc(direction)) {
    for (const LoggedRtpPacket& rtp_packet : stream.packet_view)
      rtp_in_direction.emplace(rtp_packet.log_time_us(), &rtp_packet);
  }
  absl::optional<int64_t> last_rtp_time;
  for (const auto& kv : rtp_in_direction) {
    int64_t timestamp = kv.first;
    if (timestamp > segment_end_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t duration = timestamp - last_rtp_time.value_or(0);
    if (last_rtp_time.has_value() && duration > kMaxRtpTransmissionGap) {
      // No packet sent/received for more than 500 ms.
      Alert(rtp_alert, config_.GetCallTimeSec(Timestamp::Micros(timestamp)),
            rtp_explanation);
    }
    last_rtp_time.emplace(timestamp);
  }

  absl::optional<int64_t> last_rtcp_time;
  if (direction == kIncomingPacket) {
    for (const auto& rtcp : parsed_log.incoming_rtcp_packets()) {
      if (rtcp.log_time_us() > segment_end_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }
      int64_t duration = rtcp.log_time_us() - last_rtcp_time.value_or(0);
      if (last_rtcp_time.has_value() && duration > kMaxRtcpTransmissionGap) {
        // No feedback sent/received for more than 2000 ms.
        Alert(rtcp_alert, config_.GetCallTimeSec(rtcp.log_time()),
              rtcp_explanation);
      }
      last_rtcp_time.emplace(rtcp.log_time_us());
    }
  } else {
    for (const auto& rtcp : parsed_log.outgoing_rtcp_packets()) {
      if (rtcp.log_time_us() > segment_end_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }
      int64_t duration = rtcp.log_time_us() - last_rtcp_time.value_or(0);
      if (last_rtcp_time.has_value() && duration > kMaxRtcpTransmissionGap) {
        // No feedback sent/received for more than 2000 ms.
        Alert(rtcp_alert, config_.GetCallTimeSec(rtcp.log_time()),
              rtcp_explanation);
      }
      last_rtcp_time.emplace(rtcp.log_time_us());
    }
  }
}

// TODO(terelius): Notifications could possibly be generated by the same code
// that produces the graphs. There is some code duplication that could be
// avoided, but that might be solved anyway when we move functionality from the
// analyzer to the parser.
void TriageHelper::AnalyzeLog(const ParsedRtcEventLog& parsed_log) {
  AnalyzeStreamGaps(parsed_log, kIncomingPacket);
  AnalyzeStreamGaps(parsed_log, kOutgoingPacket);
  AnalyzeTransmissionGaps(parsed_log, kIncomingPacket);
  AnalyzeTransmissionGaps(parsed_log, kOutgoingPacket);

  const int64_t segment_end_us = parsed_log.first_log_segment().stop_time_us();

  Timestamp first_occurrence = parsed_log.last_timestamp();
  constexpr double kMaxLossFraction = 0.05;
  // Loss feedback
  int64_t total_lost_packets = 0;
  int64_t total_expected_packets = 0;
  for (auto& bwe_update : parsed_log.bwe_loss_updates()) {
    if (bwe_update.log_time_us() > segment_end_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t lost_packets = static_cast<double>(bwe_update.fraction_lost) / 255 *
                           bwe_update.expected_packets;
    total_lost_packets += lost_packets;
    total_expected_packets += bwe_update.expected_packets;
    if (bwe_update.fraction_lost >= 255 * kMaxLossFraction) {
      first_occurrence = std::min(first_occurrence, bwe_update.log_time());
    }
  }
  double avg_outgoing_loss =
      static_cast<double>(total_lost_packets) / total_expected_packets;
  if (avg_outgoing_loss > kMaxLossFraction) {
    Alert(TriageAlertType::kOutgoingHighLoss,
          config_.GetCallTimeSec(first_occurrence),
          "More than 5% of outgoing packets lost.");
  }
}

void TriageHelper::Print(FILE* file) {
  fprintf(file, "========== TRIAGE NOTIFICATIONS ==========\n");
  for (const auto& alert : triage_alerts_) {
    fprintf(file, "%d %s. First occurrence at %3.3lf\n", alert.second.count,
            alert.second.explanation.c_str(), alert.second.first_occurrence);
  }
  fprintf(file, "========== END TRIAGE NOTIFICATIONS ==========\n");
}

void TriageHelper::ProcessAlerts(
    std::function<void(int, float, std::string)> f) {
  for (const auto& alert : triage_alerts_) {
    f(alert.second.count, alert.second.first_occurrence,
      alert.second.explanation);
  }
}

}  // namespace webrtc
