/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/simulated_network.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include "api/units/data_rate.h"

namespace webrtc {

SimulatedNetwork::SimulatedNetwork(SimulatedNetwork::Config config,
                                   uint64_t random_seed)
    : random_(random_seed), bursting_(false) {
  SetConfig(config);
}

SimulatedNetwork::~SimulatedNetwork() = default;

void SimulatedNetwork::SetConfig(const SimulatedNetwork::Config& config) {
  rtc::CritScope crit(&config_lock_);
  if (config_.link_capacity_kbps != config.link_capacity_kbps) {
    reset_capacity_delay_error_ = true;
  }
  config_ = config;  // Shallow copy of the struct.
  double prob_loss = config.loss_percent / 100.0;
  if (config_.avg_burst_loss_length == -1) {
    // Uniform loss
    prob_loss_bursting_ = prob_loss;
    prob_start_bursting_ = prob_loss;
  } else {
    // Lose packets according to a gilbert-elliot model.
    int avg_burst_loss_length = config.avg_burst_loss_length;
    int min_avg_burst_loss_length = std::ceil(prob_loss / (1 - prob_loss));

    RTC_CHECK_GT(avg_burst_loss_length, min_avg_burst_loss_length)
        << "For a total packet loss of " << config.loss_percent << "%% then"
        << " avg_burst_loss_length must be " << min_avg_burst_loss_length + 1
        << " or higher.";

    prob_loss_bursting_ = (1.0 - 1.0 / avg_burst_loss_length);
    prob_start_bursting_ = prob_loss / (1 - prob_loss) / avg_burst_loss_length;
  }
}

void SimulatedNetwork::PauseTransmissionUntil(int64_t until_us) {
  rtc::CritScope crit(&config_lock_);
  pause_transmission_until_us_ = until_us;
}

bool SimulatedNetwork::EnqueuePacket(PacketInFlightInfo packet) {
  Config config;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
  }
  rtc::CritScope crit(&process_lock_);
  if (config.queue_length_packets > 0 &&
      capacity_link_.size() >= config.queue_length_packets) {
    // Too many packet on the link, drop this one.
    return false;
  }
  int64_t network_start_time_us = packet.send_time_us;
  {
    rtc::CritScope crit(&config_lock_);
    if (reset_capacity_delay_error_) {
      capacity_delay_error_bytes_ = 0;
      reset_capacity_delay_error_ = false;
    }
    if (pause_transmission_until_us_) {
      network_start_time_us =
          std::max(network_start_time_us, *pause_transmission_until_us_);
      pause_transmission_until_us_.reset();
    }
  }

  // Delay introduced by the link capacity.
  TimeDelta capacity_delay = TimeDelta::Zero();
  if (config.link_capacity_kbps > 0) {
    const DataRate link_capacity = DataRate::kbps(config.link_capacity_kbps);
    int64_t compensated_size =
        static_cast<int64_t>(packet.size) + capacity_delay_error_bytes_;
    capacity_delay = DataSize::bytes(compensated_size) / link_capacity;

    capacity_delay_error_bytes_ +=
        packet.size - (capacity_delay * link_capacity).bytes();
  }

  // Check if there already are packets on the link and change network start
  // time forward if there is.
  if (!capacity_link_.empty() &&
      network_start_time_us < capacity_link_.back().arrival_time_us)
    network_start_time_us = capacity_link_.back().arrival_time_us;

  int64_t arrival_time_us = network_start_time_us + capacity_delay.us();
  capacity_link_.push({packet, arrival_time_us});
  return true;
}

absl::optional<int64_t> SimulatedNetwork::NextDeliveryTimeUs() const {
  rtc::CritScope crit(&process_lock_);
  if (!delay_link_.empty())
    return delay_link_.begin()->arrival_time_us;
  return absl::nullopt;
}
std::vector<PacketDeliveryInfo> SimulatedNetwork::DequeueDeliverablePackets(
    int64_t receive_time_us) {
  int64_t time_now_us = receive_time_us;
  Config config;
  double prob_loss_bursting;
  double prob_start_bursting;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
    prob_loss_bursting = prob_loss_bursting_;
    prob_start_bursting = prob_start_bursting_;
  }
  {
    rtc::CritScope crit(&process_lock_);
    // Check the capacity link first.
    if (!capacity_link_.empty()) {
      int64_t last_arrival_time_us =
          delay_link_.empty() ? -1 : delay_link_.back().arrival_time_us;
      bool needs_sort = false;
      while (!capacity_link_.empty() &&
             time_now_us >= capacity_link_.front().arrival_time_us) {
        // Time to get this packet.
        PacketInfo packet = std::move(capacity_link_.front());
        capacity_link_.pop();

        // Drop packets at an average rate of |config_.loss_percent| with
        // and average loss burst length of |config_.avg_burst_loss_length|.
        if ((bursting_ && random_.Rand<double>() < prob_loss_bursting) ||
            (!bursting_ && random_.Rand<double>() < prob_start_bursting)) {
          bursting_ = true;
          continue;
        } else {
          bursting_ = false;
        }

        int64_t arrival_time_jitter_us = std::max(
            random_.Gaussian(config.queue_delay_ms * 1000,
                             config.delay_standard_deviation_ms * 1000),
            0.0);

        // If reordering is not allowed then adjust arrival_time_jitter
        // to make sure all packets are sent in order.
        if (!config.allow_reordering && !delay_link_.empty() &&
            packet.arrival_time_us + arrival_time_jitter_us <
                last_arrival_time_us) {
          arrival_time_jitter_us =
              last_arrival_time_us - packet.arrival_time_us;
        }
        packet.arrival_time_us += arrival_time_jitter_us;
        if (packet.arrival_time_us >= last_arrival_time_us) {
          last_arrival_time_us = packet.arrival_time_us;
        } else {
          needs_sort = true;
        }
        delay_link_.emplace_back(std::move(packet));
      }

      if (needs_sort) {
        // Packet(s) arrived out of order, make sure list is sorted.
        std::sort(delay_link_.begin(), delay_link_.end(),
                  [](const PacketInfo& p1, const PacketInfo& p2) {
                    return p1.arrival_time_us < p2.arrival_time_us;
                  });
      }
    }

    std::vector<PacketDeliveryInfo> packets_to_deliver;
    // Check the extra delay queue.
    while (!delay_link_.empty() &&
           time_now_us >= delay_link_.front().arrival_time_us) {
      PacketInfo packet_info = delay_link_.front();
      packets_to_deliver.emplace_back(
          PacketDeliveryInfo(packet_info.packet, packet_info.arrival_time_us));
      delay_link_.pop_front();
    }
    return packets_to_deliver;
  }
}

}  // namespace webrtc
