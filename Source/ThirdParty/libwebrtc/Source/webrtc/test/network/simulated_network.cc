/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/simulated_network.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace {

// Calculate the time that it takes to send N `bits` on a
// network with link capacity equal to `capacity_kbps` starting at time
// `start_time`.
Timestamp CalculateArrivalTime(Timestamp start_time,
                               int64_t bits,
                               DataRate capacity) {
  if (capacity.IsInfinite()) {
    return start_time;
  }
  if (capacity.IsZero()) {
    return Timestamp::PlusInfinity();
  }

  // Adding `capacity - 1` to the numerator rounds the extra delay caused by
  // capacity constraints up to an integral microsecond. Sending 0 bits takes 0
  // extra time, while sending 1 bit gets rounded up to 1 (the multiplication by
  // 1000 is because capacity is in kbps).
  // The factor 1000 comes from 10^6 / 10^3, where 10^6 is due to the time unit
  // being us and 10^3 is due to the rate unit being kbps.
  return start_time + TimeDelta::Micros((1000 * bits + capacity.kbps() - 1) /
                                        capacity.kbps());
}

}  // namespace

SimulatedNetwork::SimulatedNetwork(Config config,
                                   uint64_t random_seed,
                                   std::unique_ptr<NetworkQueue> queue)
    : queue_(std::move(queue)), random_(random_seed) {
  SetConfig(config);
}

SimulatedNetwork::SimulatedNetwork(Config config, uint64_t random_seed)
    : SimulatedNetwork(config,
                       random_seed,
                       std::make_unique<LeakyBucketNetworkQueue>()) {}

SimulatedNetwork::~SimulatedNetwork() = default;

void SimulatedNetwork::SetConfig(const Config& config) {
  MutexLock lock(&lock_);
  SetConfigLocked(config);
}

void SimulatedNetwork::SetConfigLocked(const Config& config) {
  queue_->SetMaxPacketCapacity(
      /*max_length=*/config.queue_length_packets > 0
          ? config.queue_length_packets - 1  // -1 to account for the
                                             // packet in the capacity link.
          : NetworkQueue::kMaxPacketCapacity);

  config_state_.config = config;  // Shallow copy of the struct.
  double prob_loss = config.loss_percent / 100.0;
  if (config_state_.config.avg_burst_loss_length == -1) {
    // Uniform loss
    config_state_.prob_loss_bursting = prob_loss;
    config_state_.prob_start_bursting = prob_loss;
  } else {
    // Lose packets according to a gilbert-elliot model.
    int avg_burst_loss_length = config.avg_burst_loss_length;
    int min_avg_burst_loss_length = std::ceil(prob_loss / (1 - prob_loss));

    RTC_CHECK_GT(avg_burst_loss_length, min_avg_burst_loss_length)
        << "For a total packet loss of " << config.loss_percent
        << "%% then"
           " avg_burst_loss_length must be "
        << min_avg_burst_loss_length + 1 << " or higher.";

    config_state_.prob_loss_bursting = (1.0 - 1.0 / avg_burst_loss_length);
    config_state_.prob_start_bursting =
        prob_loss / (1 - prob_loss) / avg_burst_loss_length;
  }
}

void SimulatedNetwork::SetConfig(const BuiltInNetworkBehaviorConfig& new_config,
                                 Timestamp config_update_time) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);

  {
    MutexLock lock(&lock_);
    if (capacity_link_.has_value()) {
      // Calculate and update how large portion of the packet first in the
      // capacity link is left to to send at time `config_update_time`.
      const BuiltInNetworkBehaviorConfig& current_config = config_state_.config;
      TimeDelta duration_with_current_config =
          config_update_time - capacity_link_->last_update_time;
      RTC_DCHECK_GE(duration_with_current_config, TimeDelta::Zero());
      capacity_link_->bits_left_to_send -=
          std::min(duration_with_current_config.ms() *
                       current_config.link_capacity.kbps(),
                   capacity_link_->bits_left_to_send);
      capacity_link_->last_update_time = config_update_time;
    }
    SetConfigLocked(new_config);
    UpdateCapacityLink(config_state_, config_update_time);
  };

  if (UpdateNextProcessTime() && next_process_time_changed_callback_) {
    next_process_time_changed_callback_();
  }
}

void SimulatedNetwork::UpdateConfig(
    std::function<void(BuiltInNetworkBehaviorConfig*)> config_modifier) {
  MutexLock lock(&lock_);
  config_modifier(&config_state_.config);
}

void SimulatedNetwork::PauseTransmissionUntil(int64_t until_us) {
  MutexLock lock(&lock_);
  config_state_.pause_transmission_until_us = until_us;
}

bool SimulatedNetwork::EnqueuePacket(PacketInFlightInfo packet) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  // Check that old packets don't get enqueued, the SimulatedNetwork expect that
  // the packets' send time is monotonically increasing. The tolerance for
  // non-monotonic enqueue events is 0.5 ms because on multi core systems
  // clock_gettime(CLOCK_MONOTONIC) can show non-monotonic behaviour between
  // theads running on different cores.
  // TODO(bugs.webrtc.org/14525): Open a bug on this with the goal to re-enable
  // the DCHECK.
  // At the moment, we see more than 130ms between non-monotonic events, which
  // is more than expected.
  // RTC_DCHECK_GE(packet.send_time_us - last_enqueue_time_us_, -2000);
  last_enqueue_time_us_ = packet.send_time_us;

  MutexLock lock(&lock_);

  // If the network config requires packet overhead, let's apply it as early as
  // possible.
  packet.size += config_state_.config.packet_overhead;

  Timestamp enqueue_time = packet.send_time();
  bool packet_enqueued = queue_->EnqueuePacket(packet);
  // A packet can not enter the narrow section before the last packet has exit.
  if (capacity_link_.has_value()) {
    // A packet is already in the capacity link. Wait until it exits.
    return packet_enqueued;
  }
  PacketInFlightInfo next_packet = packet;
  if (!queue_->empty()) {
    next_packet = *queue_->DequeuePacket(enqueue_time);
  }
  Timestamp arrival_time = CalculateArrivalTime(
      std::max(next_packet.send_time(), last_capacity_link_exit_time_),
      packet.size * 8, config_state_.config.link_capacity);

  capacity_link_ = {
      .packet = next_packet,
      .last_update_time = enqueue_time,
      .bits_left_to_send = 8 * static_cast<int64_t>(next_packet.size),
      .arrival_time = arrival_time};

  // Only update `next_process_time_` if not already set. Otherwise,
  // next_process_time_ is calculated when a packet is dequeued. Note that this
  // means that the newly enqueued packet risk having an arrival time before
  // `next_process_time_` if packet reordering is allowed and
  // config.delay_standard_deviation_ms is set.
  // TODO(bugs.webrtc.org/14525): Consider preventing this.
  if (next_process_time_.IsInfinite() && arrival_time.IsFinite()) {
    next_process_time_ = arrival_time;
  }
  return true;
}

std::optional<int64_t> SimulatedNetwork::NextDeliveryTimeUs() const {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  if (next_process_time_.IsFinite()) {
    return next_process_time_.us();
  }
  return std::nullopt;
}

void SimulatedNetwork::UpdateCapacityLink(ConfigState state,
                                          Timestamp time_now) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  if (capacity_link_.has_value()) {
    // Recalculate the arrival time of the packet currently in the capacity link
    // since it may have changed if the capacity has changed.
    capacity_link_->last_update_time = std::max(
        capacity_link_->last_update_time, last_capacity_link_exit_time_);
    capacity_link_->arrival_time = CalculateArrivalTime(
        capacity_link_->last_update_time, capacity_link_->bits_left_to_send,
        state.config.link_capacity);
  }

  if (!capacity_link_.has_value() || time_now < capacity_link_->arrival_time) {
    return;
  }
  bool reorder_packets = false;

  do {
    // Time to get this packet (the original or just updated arrival_time is
    // smaller or equal to time_now_us).
    PacketInfo packet = *capacity_link_;
    RTC_DCHECK(packet.arrival_time.IsFinite());
    capacity_link_ = std::nullopt;

    // If the network is paused, the pause will be implemented as an extra delay
    // to be spent in the `delay_link_` queue.
    if (state.pause_transmission_until_us > packet.arrival_time.us()) {
      packet.arrival_time =
          Timestamp::Micros(state.pause_transmission_until_us);
    }

    // Store the original arrival time, before applying packet loss or extra
    // delay. This is needed to know when it is possible for the next packet
    // in the queue to start transmitting.
    last_capacity_link_exit_time_ = packet.arrival_time;

    // Drop packets at an average rate of `state.config.loss_percent` with
    // and average loss burst length of `state.config.avg_burst_loss_length`.
    if ((bursting_ && random_.Rand<double>() < state.prob_loss_bursting) ||
        (!bursting_ && random_.Rand<double>() < state.prob_start_bursting)) {
      bursting_ = true;
      packet.arrival_time = Timestamp::MinusInfinity();
    } else {
      // If packets are not dropped, apply extra delay as configured.
      bursting_ = false;
      TimeDelta arrival_time_jitter = TimeDelta::Micros(std::max(
          random_.Gaussian(state.config.queue_delay_ms * 1000,
                           state.config.delay_standard_deviation_ms * 1000),
          0.0));

      // If reordering is not allowed then adjust arrival_time_jitter
      // to make sure all packets are sent in order.
      Timestamp last_arrival_time = delay_link_.empty()
                                        ? Timestamp::MinusInfinity()
                                        : delay_link_.back().arrival_time;
      if (!state.config.allow_reordering && !delay_link_.empty() &&
          packet.arrival_time + arrival_time_jitter < last_arrival_time) {
        arrival_time_jitter = last_arrival_time - packet.arrival_time;
      }
      packet.arrival_time += arrival_time_jitter;

      // Optimization: Schedule a reorder only when a packet will exit before
      // the one in front.
      if (last_arrival_time > packet.arrival_time) {
        reorder_packets = true;
      }
    }
    delay_link_.emplace_back(packet);

    // If there are no packets in the queue, there is nothing else to do.
    std::optional<PacketInFlightInfo> peek_packet = queue_->PeekNextPacket();
    if (!peek_packet) {
      break;
    }
    // It is possible that the next packet in the queue has a send time (at
    // least in tests) after the previous packet left the capacity link.
    Timestamp next_start =
        std::max(last_capacity_link_exit_time_, peek_packet->send_time());
    std::optional<PacketInFlightInfo> next_packet =
        queue_->DequeuePacket(next_start);
    capacity_link_ = {
        .packet = *next_packet,
        .last_update_time = next_start,
        .bits_left_to_send = 8 * static_cast<int64_t>(next_packet->size),
        .arrival_time = CalculateArrivalTime(next_start, next_packet->size * 8,
                                             state.config.link_capacity)};
    // And if the next packet in the queue needs to exit, let's dequeue it.
  } while (capacity_link_->arrival_time <= time_now);

  if (state.config.allow_reordering && reorder_packets) {
    // Packets arrived out of order and since the network config allows
    // reordering, let's sort them per arrival_time to make so they will also
    // be delivered out of order.
    std::stable_sort(delay_link_.begin(), delay_link_.end(),
                     [](const PacketInfo& p1, const PacketInfo& p2) {
                       return p1.arrival_time < p2.arrival_time;
                     });
  }
}

std::vector<PacketDeliveryInfo> SimulatedNetwork::DequeueDeliverablePackets(
    int64_t receive_time_us) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  Timestamp receive_time = Timestamp::Micros(receive_time_us);

  MutexLock lock(&lock_);
  UpdateCapacityLink(config_state_, receive_time);
  std::vector<PacketDeliveryInfo> packets_to_deliver;

  for (const PacketInFlightInfo& packet : queue_->DequeueDroppedPackets()) {
    packets_to_deliver.emplace_back(packet, PacketDeliveryInfo::kNotReceived);
  }

  // Check the extra delay queue.
  while (!delay_link_.empty() &&
         receive_time >= delay_link_.front().arrival_time) {
    PacketInfo packet_info = delay_link_.front();
    packets_to_deliver.emplace_back(PacketDeliveryInfo(
        packet_info.packet, packet_info.arrival_time.IsFinite()
                                ? packet_info.arrival_time.us()
                                : PacketDeliveryInfo::kNotReceived));
    delay_link_.pop_front();
  }
  // There is no need to invoke `next_process_time_changed_callback_` here since
  // it is expected that the user of NetworkBehaviorInterface calls
  // NextDeliveryTimeUs after DequeueDeliverablePackets. See
  // NetworkBehaviorInterface.
  UpdateNextProcessTime();

  if (!config_state_.config.forward_ecn) {
    for (PacketDeliveryInfo& packet : packets_to_deliver) {
      packet.ecn = EcnMarking::kNotEct;
    }
  }
  return packets_to_deliver;
}

bool SimulatedNetwork::UpdateNextProcessTime() {
  Timestamp next_process_time = next_process_time_;

  next_process_time_ = Timestamp::PlusInfinity();
  for (const PacketInfo& packet : delay_link_) {
    if (packet.arrival_time.IsFinite()) {
      next_process_time_ = packet.arrival_time;
      break;
    }
  }
  if (next_process_time_.IsInfinite() && capacity_link_.has_value()) {
    next_process_time_ = capacity_link_->arrival_time;
  }
  return next_process_time != next_process_time_;
}

void SimulatedNetwork::RegisterDeliveryTimeChangedCallback(
    absl::AnyInvocable<void()> callback) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  next_process_time_changed_callback_ = std::move(callback);
}

}  // namespace webrtc
