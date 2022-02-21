/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/rtc_event_log_visualizer/log_simulation.h"

#include <algorithm>
#include <utility>

#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

LogBasedNetworkControllerSimulation::LogBasedNetworkControllerSimulation(
    std::unique_ptr<NetworkControllerFactoryInterface> factory,
    std::function<void(const NetworkControlUpdate&, Timestamp)> update_handler)
    : update_handler_(update_handler), factory_(std::move(factory)) {}

LogBasedNetworkControllerSimulation::~LogBasedNetworkControllerSimulation() {}

void LogBasedNetworkControllerSimulation::HandleStateUpdate(
    const NetworkControlUpdate& update) {
  update_handler_(update, current_time_);
}

void LogBasedNetworkControllerSimulation::ProcessUntil(Timestamp to_time) {
  if (last_process_.IsInfinite()) {
    NetworkControllerConfig config;
    config.constraints.at_time = to_time;
    config.constraints.min_data_rate = DataRate::KilobitsPerSec(30);
    config.constraints.starting_rate = DataRate::KilobitsPerSec(300);
    config.event_log = &null_event_log_;
    controller_ = factory_->Create(config);
  }
  if (last_process_.IsInfinite() ||
      to_time - last_process_ > TimeDelta::Seconds(1)) {
    last_process_ = to_time;
    current_time_ = to_time;
    ProcessInterval msg;
    msg.at_time = to_time;
    HandleStateUpdate(controller_->OnProcessInterval(msg));
  } else {
    while (last_process_ + factory_->GetProcessInterval() <= to_time) {
      last_process_ += factory_->GetProcessInterval();
      current_time_ = last_process_;
      ProcessInterval msg;
      msg.at_time = current_time_;
      HandleStateUpdate(controller_->OnProcessInterval(msg));
    }
    current_time_ = to_time;
  }
}

void LogBasedNetworkControllerSimulation::OnProbeCreated(
    const LoggedBweProbeClusterCreatedEvent& probe_cluster) {
  pending_probes_.push_back({probe_cluster, 0, 0});
}

void LogBasedNetworkControllerSimulation::OnPacketSent(
    const LoggedPacketInfo& packet) {
  ProcessUntil(packet.log_packet_time);
  if (packet.has_transport_seq_no) {
    PacedPacketInfo probe_info;
    if (!pending_probes_.empty() &&
        packet.media_type == LoggedMediaType::kVideo) {
      auto& probe = pending_probes_.front();
      probe_info.probe_cluster_id = probe.event.id;
      probe_info.send_bitrate_bps = probe.event.bitrate_bps;
      probe_info.probe_cluster_min_bytes = probe.event.min_bytes;
      probe_info.probe_cluster_min_probes = probe.event.min_packets;
      probe.packets_sent++;
      probe.bytes_sent += packet.size + packet.overhead;
      if (probe.bytes_sent >= probe.event.min_bytes &&
          probe.packets_sent >= probe.event.min_packets) {
        pending_probes_.pop_front();
      }
    }

    RtpPacketSendInfo packet_info;
    packet_info.media_ssrc = packet.ssrc;
    packet_info.transport_sequence_number = packet.transport_seq_no;
    packet_info.rtp_sequence_number = packet.stream_seq_no;
    packet_info.length = packet.size;
    packet_info.pacing_info = probe_info;
    transport_feedback_.AddPacket(packet_info, packet.overhead,
                                  packet.log_packet_time);
  }
  rtc::SentPacket sent_packet;
  sent_packet.send_time_ms = packet.log_packet_time.ms();
  sent_packet.info.included_in_allocation = true;
  sent_packet.info.packet_size_bytes = packet.size + packet.overhead;
  if (packet.has_transport_seq_no) {
    sent_packet.packet_id = packet.transport_seq_no;
    sent_packet.info.included_in_feedback = true;
  }
  auto msg = transport_feedback_.ProcessSentPacket(sent_packet);
  if (msg)
    HandleStateUpdate(controller_->OnSentPacket(*msg));
}

void LogBasedNetworkControllerSimulation::OnFeedback(
    const LoggedRtcpPacketTransportFeedback& feedback) {
  auto feedback_time = Timestamp::Millis(feedback.log_time_ms());
  ProcessUntil(feedback_time);
  auto msg = transport_feedback_.ProcessTransportFeedback(
      feedback.transport_feedback, feedback_time);
  if (msg)
    HandleStateUpdate(controller_->OnTransportPacketsFeedback(*msg));
}

void LogBasedNetworkControllerSimulation::OnReceiverReport(
    const LoggedRtcpPacketReceiverReport& report) {
  if (report.rr.report_blocks().empty())
    return;
  auto report_time = Timestamp::Millis(report.log_time_ms());
  ProcessUntil(report_time);
  int packets_delta = 0;
  int lost_delta = 0;
  for (auto& block : report.rr.report_blocks()) {
    auto it = last_report_blocks_.find(block.source_ssrc());
    if (it != last_report_blocks_.end()) {
      packets_delta +=
          block.extended_high_seq_num() - it->second.extended_high_seq_num();
      lost_delta += block.cumulative_lost() - it->second.cumulative_lost();
    }
    last_report_blocks_[block.source_ssrc()] = block;
  }
  if (packets_delta > lost_delta) {
    TransportLossReport msg;
    msg.packets_lost_delta = lost_delta;
    msg.packets_received_delta = packets_delta - lost_delta;
    msg.receive_time = report_time;
    msg.start_time = last_report_block_time_;
    msg.end_time = report_time;
    last_report_block_time_ = report_time;
    HandleStateUpdate(controller_->OnTransportLossReport(msg));
  }

  Clock* clock = Clock::GetRealTimeClock();
  TimeDelta rtt = TimeDelta::PlusInfinity();
  for (auto& rb : report.rr.report_blocks()) {
    if (rb.last_sr()) {
      Timestamp report_log_time = Timestamp::Micros(report.log_time_us());
      uint32_t receive_time_ntp =
          CompactNtp(clock->ConvertTimestampToNtpTime(report_log_time));
      uint32_t rtt_ntp =
          receive_time_ntp - rb.delay_since_last_sr() - rb.last_sr();
      rtt = std::min(rtt, TimeDelta::Millis(CompactNtpRttToMs(rtt_ntp)));
    }
  }
  if (rtt.IsFinite()) {
    RoundTripTimeUpdate msg;
    msg.receive_time = report_time;
    msg.round_trip_time = rtt;
    HandleStateUpdate(controller_->OnRoundTripTimeUpdate(msg));
  }
}

void LogBasedNetworkControllerSimulation::OnIceConfig(
    const LoggedIceCandidatePairConfig& candidate) {
  if (candidate.type == IceCandidatePairConfigType::kSelected) {
    auto log_time = Timestamp::Micros(candidate.log_time_us());
    ProcessUntil(log_time);
    NetworkRouteChange msg;
    msg.at_time = log_time;
    msg.constraints.min_data_rate = DataRate::KilobitsPerSec(30);
    msg.constraints.starting_rate = DataRate::KilobitsPerSec(300);
    msg.constraints.at_time = log_time;
    HandleStateUpdate(controller_->OnNetworkRouteChange(msg));
  }
}

void LogBasedNetworkControllerSimulation::ProcessEventsInLog(
    const ParsedRtcEventLog& parsed_log_) {
  auto packet_infos = parsed_log_.GetOutgoingPacketInfos();
  RtcEventProcessor processor;
  processor.AddEvents(
      parsed_log_.bwe_probe_cluster_created_events(),
      [this](const LoggedBweProbeClusterCreatedEvent& probe_cluster) {
        OnProbeCreated(probe_cluster);
      });
  processor.AddEvents(packet_infos, [this](const LoggedPacketInfo& packet) {
    OnPacketSent(packet);
  });
  processor.AddEvents(
      parsed_log_.transport_feedbacks(PacketDirection::kIncomingPacket),
      [this](const LoggedRtcpPacketTransportFeedback& feedback) {
        OnFeedback(feedback);
      });
  processor.AddEvents(
      parsed_log_.receiver_reports(PacketDirection::kIncomingPacket),
      [this](const LoggedRtcpPacketReceiverReport& report) {
        OnReceiverReport(report);
      });
  processor.AddEvents(parsed_log_.ice_candidate_pair_configs(),
                      [this](const LoggedIceCandidatePairConfig& candidate) {
                        OnIceConfig(candidate);
                      });
  processor.ProcessEventsInOrder();
}

}  // namespace webrtc
