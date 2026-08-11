/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/congestion_control_feedback_generator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/rtp/congestion_controller_feedback_stats.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

constexpr DataRate kMaxFeedbackRate = DataRate::KilobitsPerSec(500);

CongestionControlFeedbackGenerator::CongestionControlFeedbackGenerator(
    const Environment& env,
    RtcpSender rtcp_sender)
    : env_(env),
      rtcp_sender_(std::move(rtcp_sender)),
      min_time_between_feedback_("min_send_delta", TimeDelta::Millis(25)),
      max_time_to_wait_for_packet_with_marker_("max_wait_for_marker",
                                               TimeDelta::Millis(25)),
      max_time_between_feedback_("max_send_delta", TimeDelta::Millis(500)) {
  ParseFieldTrial(
      {&min_time_between_feedback_, &max_time_to_wait_for_packet_with_marker_,
       &max_time_between_feedback_},
      env.field_trials().Lookup("WebRTC-RFC8888CongestionControlFeedback"));
}

void CongestionControlFeedbackGenerator::OnReceivedPacket(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  marker_bit_seen_ |= packet.Marker();
  Timestamp now = env_.clock().CurrentTime();
  if (!first_arrival_time_since_feedback_) {
    first_arrival_time_since_feedback_ = now;
  }
  auto it = feedback_trackers_.try_emplace(packet.Ssrc(), packet.Ssrc()).first;
  it->second.ReceivedPacket(packet);
  if (NextFeedbackTime() < now) {
    SendFeedback(now);
  }
}

Timestamp CongestionControlFeedbackGenerator::NextFeedbackTime() const {
  if (!first_arrival_time_since_feedback_) {
    return std::max(env_.clock().CurrentTime() + min_time_between_feedback_,
                    next_possible_feedback_send_time_);
  }

  if (!marker_bit_seen_) {
    return std::max(next_possible_feedback_send_time_,
                    *first_arrival_time_since_feedback_ +
                        max_time_to_wait_for_packet_with_marker_.Get());
  }
  return next_possible_feedback_send_time_;
}

TimeDelta CongestionControlFeedbackGenerator::Process(Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (NextFeedbackTime() <= now) {
    SendFeedback(now);
  }
  return NextFeedbackTime() - now;
}

void CongestionControlFeedbackGenerator::SendFeedback(Timestamp now) {
  RTC_DCHECK_GE(now, next_possible_feedback_send_time_);
  uint32_t compact_ntp =
      CompactNtp(env_.clock().ConvertTimestampToNtpTime(now));
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> rtcp_packet_info;
  for (auto& [unused, tracker] : feedback_trackers_) {
    tracker.AddPacketsToFeedback(now, rtcp_packet_info);
  }
  marker_bit_seen_ = false;
  first_arrival_time_since_feedback_ = std::nullopt;

  auto feedback = std::make_unique<rtcp::CongestionControlFeedback>(
      std::move(rtcp_packet_info), compact_ntp);
  CalculateNextPossibleSendTime(DataSize::Bytes(feedback->BlockLength()), now);

  std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets;
  rtcp_packets.push_back(std::move(feedback));
  rtcp_sender_(std::move(rtcp_packets));
}

void CongestionControlFeedbackGenerator::CalculateNextPossibleSendTime(
    DataSize feedback_size,
    Timestamp now) {
  TimeDelta time_since_last_sent = last_feedback_sent_time_.IsFinite()
                                       ? now - last_feedback_sent_time_
                                       : TimeDelta::Zero();
  DataSize debt_payed = time_since_last_sent * kMaxFeedbackRate;
  send_rate_debt_ = debt_payed > send_rate_debt_ ? DataSize::Zero()
                                                 : send_rate_debt_ - debt_payed;
  send_rate_debt_ += feedback_size;
  last_feedback_sent_time_ = now;
  next_possible_feedback_send_time_ =
      now + std::clamp(send_rate_debt_ / kMaxFeedbackRate,
                       min_time_between_feedback_.Get(),
                       max_time_between_feedback_.Get());
}

flat_map<uint32_t, SentCongestionControllerFeedbackStats>
CongestionControlFeedbackGenerator::GetStatsPerSsrc() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  flat_map<uint32_t, SentCongestionControllerFeedbackStats> result;
  result.reserve(feedback_trackers_.size());
  for (const auto& [ssrc, tracker] : feedback_trackers_) {
    // feedback_trackers_ are sorted by the SSRC, so when adding to the
    // flat_map, expect it uses the same sorting and thus new elements would
    // always be at the end.
    result.insert_or_assign(
        /*hint=*/result.end(), /*key=*/ssrc, tracker.GetStats());
  }
  return result;
}

}  // namespace webrtc
