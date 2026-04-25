/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/rtc_event_log_visualizer/log_scream_simulation.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/environment/environment.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/sent_packet.h"

namespace webrtc {

LogScreamSimulation::LogScreamSimulation(const Config& config,
                                         const Environment& env)
    : env_(env), send_rate_tracker_(config.rate_window) {
  // Scream is recreated if candidates change.
  scream_.emplace(env_);
  scream_->SetTargetBitrateConstraints(
      /*min=*/DataRate::Zero(), /*max=*/DataRate::PlusInfinity(),
      /*start=*/DataRate::KilobitsPerSec(300));
}

void LogScreamSimulation::ProcessUntil(Timestamp to_time) {
  current_time_ = to_time;
}

void LogScreamSimulation::OnPacketSent(const LoggedPacketInfo& packet) {
  ProcessUntil(packet.log_packet_time);

  RtpPacketToSend send_packet(/*extensions=*/nullptr);

  int64_t packet_id = packet.has_transport_seq_no ? packet.transport_seq_no
                                                  : next_ccfb_packet_id_++;
  send_packet.set_transport_sequence_number(packet_id);
  send_packet.SetSsrc(packet.ssrc);
  send_packet.SetSequenceNumber(packet.stream_seq_no);
  send_packet.SetPayloadSize(packet.size - send_packet.headers_size());
  RTC_DCHECK_EQ(send_packet.size(), packet.size);
  transport_feedback_.AddPacket(send_packet, PacedPacketInfo(), packet.overhead,
                                packet.log_packet_time);

  SentPacketInfo sent_packet;
  sent_packet.packet_id = packet_id;
  sent_packet.send_time_ms = packet.log_packet_time.ms();
  sent_packet.info.included_in_allocation = true;
  sent_packet.info.packet_size_bytes = packet.size + packet.overhead;
  sent_packet.info.included_in_feedback = true;
  send_rate_tracker_.Update(DataSize::Bytes(sent_packet.info.packet_size_bytes),
                            packet.log_packet_time);

  std::optional<SentPacket> packet_info =
      transport_feedback_.ProcessSentPacket(sent_packet);
  if (packet_info.has_value()) {
    send_window_usage_ = State::kBelowRefWindow;
    if (packet_info->data_in_flight >= scream_->ref_window()) {
      send_window_usage_ = State::kAboveRefWindow;
    }
    if (packet_info->data_in_flight > scream_->max_data_in_flight()) {
      send_window_usage_ = State::kAboveScreamMax;
    }
    scream_->OnPacketSent(packet_info->data_in_flight);
    data_in_flight_ = packet_info->data_in_flight;
  }
}

void LogScreamSimulation::OnTransportFeedback(
    const LoggedRtcpPacketTransportFeedback& feedback) {
  auto feedback_time = Timestamp::Millis(feedback.log_time_ms());
  ProcessUntil(feedback_time);
  std::optional<TransportPacketsFeedback> msg =
      transport_feedback_.ProcessTransportFeedback(feedback.transport_feedback,
                                                   feedback_time);
  if (msg.has_value()) {
    scream_->OnTransportPacketsFeedback(*msg);
    LogState(*msg);
  }
}

void LogScreamSimulation::OnCongestionControlFeedback(
    const LoggedRtcpCongestionControlFeedback& feedback) {
  auto feedback_time = Timestamp::Millis(feedback.log_time_ms());
  ProcessUntil(feedback_time);
  std::optional<TransportPacketsFeedback> msg =
      transport_feedback_.ProcessCongestionControlFeedback(
          feedback.congestion_feedback, feedback_time);
  if (msg.has_value()) {
    scream_->OnTransportPacketsFeedback(*msg);
    LogState(*msg);
  }
}

void LogScreamSimulation::OnIceConfig(
    const LoggedIceCandidatePairConfig& candidate) {
  if (candidate.type == IceCandidatePairConfigType::kSelected) {
    auto log_time = Timestamp::Micros(candidate.log_time_us());
    ProcessUntil(log_time);

    // This may be a simplification. See
    // RtpTransportControllerSend::IsRelevantRouteChange
    if (local_candidate_type_ != candidate.local_candidate_type ||
        remote_candidate_type_ != candidate.remote_candidate_type) {
      // Recreate Scream. This is inline with behaviour in
      // ScreamNetworkController::OnNetworkRouteChange.
      scream_.emplace(env_);
      scream_->SetTargetBitrateConstraints(
          /*min=*/DataRate::Zero() /*max=*/, DataRate::PlusInfinity(),
          /*start=*/DataRate::KilobitsPerSec(300));
      local_candidate_type_ = candidate.local_candidate_type;
      remote_candidate_type_ = candidate.remote_candidate_type;
    }
  }
}

void LogScreamSimulation::ProcessEventsInLog(
    const ParsedRtcEventLog& parsed_log_) {
  std::vector<LoggedPacketInfo> packet_infos =
      parsed_log_.GetOutgoingPacketInfos();
  RtcEventProcessor processor;
  processor.AddEvents(
      packet_infos,
      [this](const LoggedPacketInfo& packet) { OnPacketSent(packet); },
      PacketDirection::kOutgoingPacket);
  processor.AddEvents(
      parsed_log_.transport_feedbacks(PacketDirection::kIncomingPacket),
      [this](const LoggedRtcpPacketTransportFeedback& feedback) {
        OnTransportFeedback(feedback);
      },
      PacketDirection::kIncomingPacket);
  processor.AddEvents(
      parsed_log_.congestion_feedback(PacketDirection::kIncomingPacket),
      [this](const LoggedRtcpCongestionControlFeedback& report) {
        OnCongestionControlFeedback(report);
      },
      PacketDirection::kIncomingPacket);
  processor.AddEvents(parsed_log_.ice_candidate_pair_configs(),
                      [this](const LoggedIceCandidatePairConfig& candidate) {
                        OnIceConfig(candidate);
                      });
  processor.ProcessEventsInOrder();
}

void LogScreamSimulation::LogState(const TransportPacketsFeedback& msg) {
  state_.emplace_back(State{
      .time = msg.feedback_time,
      .target_rate = scream_->target_rate(),
      .pacing_rate = scream_->pacing_rate(),
      .send_rate =
          send_rate_tracker_.Rate(msg.feedback_time).value_or(DataRate::Zero()),
      .ref_window = scream_->ref_window(),
      .ref_window_i = scream_->ref_window_i(),
      .max_data_in_flight = scream_->max_data_in_flight(),
      .data_in_flight = data_in_flight_,
      .send_window_usage = send_window_usage_,
      .queue_delay_dev_norm =
          scream_->delay_based_congestion_control().queue_delay_dev_norm(),
      .ref_window_scale_factor_due_to_increased_delay =
          scream_->delay_based_congestion_control().IsQueueDelayDetected()
              ? 0.0
              : scream_->delay_based_congestion_control()
                    .ref_window_scale_factor_due_to_increased_delay(),
      .ref_window_scale_factor_due_to_delay_variation =
          scream_->delay_based_congestion_control()
              .ref_window_scale_factor_due_to_delay_variation(
                  scream_->ref_window_mss_ratio()),
      .l4s_alpha = scream_->l4s_alpha(),
      .l4s_alpha_v = scream_->delay_based_congestion_control().l4s_alpha_v(),
  });
}

}  // namespace webrtc
