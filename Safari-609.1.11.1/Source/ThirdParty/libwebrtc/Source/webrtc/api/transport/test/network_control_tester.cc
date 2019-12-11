/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/test/network_control_tester.h"

#include <algorithm>

#include "api/transport/network_control.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {
void Update(NetworkControlUpdate* target, const NetworkControlUpdate& update) {
  if (update.congestion_window) {
    RTC_LOG(LS_INFO) << "Received window="
                     << ToString(*update.congestion_window);
    target->congestion_window = update.congestion_window;
  }
  if (update.pacer_config) {
    RTC_LOG(LS_INFO) << "Received pacing at:"
                     << ToString(update.pacer_config->at_time)
                     << ": rate=" << ToString(update.pacer_config->data_rate())
                     << ", pad=" << ToString(update.pacer_config->pad_rate());
    target->pacer_config = update.pacer_config;
  }
  if (update.target_rate) {
    RTC_LOG(LS_INFO)
        << "Received target at:" << ToString(update.target_rate->at_time)
        << ": rate=" << ToString(update.target_rate->target_rate) << ", rtt="
        << ToString(update.target_rate->network_estimate.round_trip_time);
    target->target_rate = update.target_rate;
  }
  for (const auto& probe : update.probe_cluster_configs) {
    target->probe_cluster_configs.push_back(probe);
    RTC_LOG(LS_INFO) << "Received probe at:" << ToString(probe.at_time)
                     << ": target=" << ToString(probe.target_data_rate);
  }
}
}  // namespace

SentPacket SimpleTargetRateProducer::ProduceNext(
    const NetworkControlUpdate& cache,
    Timestamp current_time,
    TimeDelta time_delta) {
  DataRate actual_send_rate =
      std::max(cache.target_rate->target_rate, cache.pacer_config->pad_rate());
  SentPacket packet;
  packet.send_time = current_time;
  packet.size = time_delta * actual_send_rate;
  return packet;
}

NetworkControllerTester::NetworkControllerTester(
    NetworkControllerFactoryInterface* factory,
    NetworkControllerConfig initial_config)
    : current_time_(Timestamp::seconds(100000)),
      packet_sequence_number_(1),
      accumulated_buffer_(TimeDelta::Zero()) {
  initial_config.constraints.at_time = current_time_;
  controller_ = factory->Create(initial_config);
  process_interval_ = factory->GetProcessInterval();
  ProcessInterval interval_msg;
  interval_msg.at_time = current_time_;
  Update(&state_, controller_->OnProcessInterval(interval_msg));
}

NetworkControllerTester::~NetworkControllerTester() = default;

void NetworkControllerTester::RunSimulation(TimeDelta duration,
                                            TimeDelta packet_interval,
                                            DataRate actual_bandwidth,
                                            TimeDelta propagation_delay,
                                            PacketProducer next_packet) {
  RTC_CHECK(actual_bandwidth.bps() > 0);
  Timestamp start_time = current_time_;
  Timestamp last_process_time = current_time_;
  while (current_time_ - start_time < duration) {
    bool send_packet = true;
    if (state_.congestion_window && state_.congestion_window->IsFinite()) {
      DataSize data_in_flight = DataSize::Zero();
      for (PacketResult& packet : outstanding_packets_)
        data_in_flight += packet.sent_packet.size;
      if (data_in_flight > *state_.congestion_window)
        send_packet = false;
    }

    if (send_packet) {
      SentPacket sent_packet;
      sent_packet = next_packet(state_, current_time_, packet_interval);
      sent_packet.sequence_number = packet_sequence_number_++;
      sent_packet.data_in_flight = sent_packet.size;
      for (PacketResult& packet : outstanding_packets_)
        sent_packet.data_in_flight += packet.sent_packet.size;
      Update(&state_, controller_->OnSentPacket(sent_packet));

      TimeDelta time_in_flight = sent_packet.size / actual_bandwidth;
      accumulated_buffer_ += time_in_flight;
      TimeDelta total_delay = propagation_delay + accumulated_buffer_;
      PacketResult result;
      result.sent_packet = sent_packet;
      result.receive_time = sent_packet.send_time + total_delay;

      outstanding_packets_.push_back(result);
    }

    TimeDelta buffer_consumed = std::min(accumulated_buffer_, packet_interval);
    accumulated_buffer_ -= buffer_consumed;

    if (outstanding_packets_.size() >= 2 &&
        current_time_ >=
            outstanding_packets_[1].receive_time + propagation_delay) {
      TransportPacketsFeedback feedback;
      feedback.prior_in_flight = DataSize::Zero();
      for (PacketResult& packet : outstanding_packets_)
        feedback.prior_in_flight += packet.sent_packet.size;
      while (!outstanding_packets_.empty() &&
             current_time_ >= outstanding_packets_.front().receive_time +
                                  propagation_delay) {
        feedback.packet_feedbacks.push_back(outstanding_packets_.front());
        outstanding_packets_.pop_front();
      }
      feedback.feedback_time =
          feedback.packet_feedbacks.back().receive_time + propagation_delay;
      feedback.data_in_flight = DataSize::Zero();
      for (PacketResult& packet : outstanding_packets_)
        feedback.data_in_flight += packet.sent_packet.size;
      Update(&state_, controller_->OnTransportPacketsFeedback(feedback));
    }
    current_time_ += packet_interval;
    if (current_time_ - last_process_time > process_interval_) {
      ProcessInterval interval_msg;
      interval_msg.at_time = current_time_;
      Update(&state_, controller_->OnProcessInterval(interval_msg));
    }
  }
}

}  // namespace test
}  // namespace webrtc
