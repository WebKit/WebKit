/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_network_pipe.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include <algorithm>
#include <cmath>

#include "webrtc/base/logging.h"
#include "webrtc/call/call.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

namespace {
constexpr int64_t kDefaultProcessIntervalMs = 5;
}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config)
    : FakeNetworkPipe(clock, config, 1) {}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config,
                                 uint64_t seed)
    : clock_(clock),
      packet_receiver_(NULL),
      random_(seed),
      config_(),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_(0),
      bursting_(false),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_log_time_(clock_->TimeInMilliseconds()) {
  SetConfig(config);
}

FakeNetworkPipe::~FakeNetworkPipe() {
  while (!capacity_link_.empty()) {
    delete capacity_link_.front();
    capacity_link_.pop();
  }
  while (!delay_link_.empty()) {
    delete *delay_link_.begin();
    delay_link_.erase(delay_link_.begin());
  }
}

void FakeNetworkPipe::SetReceiver(PacketReceiver* receiver) {
  packet_receiver_ = receiver;
}

void FakeNetworkPipe::SetConfig(const FakeNetworkPipe::Config& config) {
  rtc::CritScope crit(&lock_);
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

void FakeNetworkPipe::SendPacket(const uint8_t* data, size_t data_length) {
  // A NULL packet_receiver_ means that this pipe will terminate the flow of
  // packets.
  if (packet_receiver_ == NULL)
    return;
  rtc::CritScope crit(&lock_);
  if (config_.queue_length_packets > 0 &&
      capacity_link_.size() >= config_.queue_length_packets) {
    // Too many packet on the link, drop this one.
    ++dropped_packets_;
    return;
  }

  int64_t time_now = clock_->TimeInMilliseconds();

  // Delay introduced by the link capacity.
  int64_t capacity_delay_ms = 0;
  if (config_.link_capacity_kbps > 0)
    capacity_delay_ms = data_length / (config_.link_capacity_kbps / 8);
  int64_t network_start_time = time_now;

  // Check if there already are packets on the link and change network start
  // time forward if there is.
  if (!capacity_link_.empty() &&
      network_start_time < capacity_link_.back()->arrival_time())
    network_start_time = capacity_link_.back()->arrival_time();

  int64_t arrival_time = network_start_time + capacity_delay_ms;
  NetworkPacket* packet = new NetworkPacket(data, data_length, time_now,
                                            arrival_time);
  capacity_link_.push(packet);
}

float FakeNetworkPipe::PercentageLoss() {
  rtc::CritScope crit(&lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<float>(dropped_packets_) /
      (sent_packets_ + dropped_packets_);
}

int FakeNetworkPipe::AverageDelay() {
  rtc::CritScope crit(&lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<int>(total_packet_delay_ /
                          static_cast<int64_t>(sent_packets_));
}

void FakeNetworkPipe::Process() {
  int64_t time_now = clock_->TimeInMilliseconds();
  std::queue<NetworkPacket*> packets_to_deliver;
  {
    rtc::CritScope crit(&lock_);
    if (time_now - last_log_time_ > 5000) {
      int64_t queueing_delay_ms = 0;
      if (!capacity_link_.empty()) {
        queueing_delay_ms = time_now - capacity_link_.front()->send_time();
      }
      LOG(LS_INFO) << "Network queue: " << queueing_delay_ms << " ms.";
      last_log_time_ = time_now;
    }
    // Check the capacity link first.
    while (!capacity_link_.empty() &&
           time_now >= capacity_link_.front()->arrival_time()) {
      // Time to get this packet.
      NetworkPacket* packet = capacity_link_.front();
      capacity_link_.pop();

      // Drop packets at an average rate of |config_.loss_percent| with
      // and average loss burst length of |config_.avg_burst_loss_length|.
      if ((bursting_ && random_.Rand<double>() < prob_loss_bursting_) ||
          (!bursting_ && random_.Rand<double>() < prob_start_bursting_)) {
        bursting_ = true;
        delete packet;
        continue;
      } else {
        bursting_ = false;
      }

      int arrival_time_jitter = random_.Gaussian(
          config_.queue_delay_ms, config_.delay_standard_deviation_ms);

      // If reordering is not allowed then adjust arrival_time_jitter
      // to make sure all packets are sent in order.
      if (!config_.allow_reordering && !delay_link_.empty() &&
          packet->arrival_time() + arrival_time_jitter <
              (*delay_link_.rbegin())->arrival_time()) {
        arrival_time_jitter =
            (*delay_link_.rbegin())->arrival_time() - packet->arrival_time();
      }
      packet->IncrementArrivalTime(arrival_time_jitter);
      delay_link_.insert(packet);
    }

    // Check the extra delay queue.
    while (!delay_link_.empty() &&
           time_now >= (*delay_link_.begin())->arrival_time()) {
      // Deliver this packet.
      NetworkPacket* packet = *delay_link_.begin();
      packets_to_deliver.push(packet);
      delay_link_.erase(delay_link_.begin());
      // |time_now| might be later than when the packet should have arrived, due
      // to NetworkProcess being called too late. For stats, use the time it
      // should have been on the link.
      total_packet_delay_ += packet->arrival_time() - packet->send_time();
    }
    sent_packets_ += packets_to_deliver.size();
  }
  while (!packets_to_deliver.empty()) {
    NetworkPacket* packet = packets_to_deliver.front();
    packets_to_deliver.pop();
    packet_receiver_->DeliverPacket(MediaType::ANY, packet->data(),
                                    packet->data_length(), PacketTime());
    delete packet;
  }

  next_process_time_ = !delay_link_.empty()
                           ? (*delay_link_.begin())->arrival_time()
                           : time_now + kDefaultProcessIntervalMs;
}

int64_t FakeNetworkPipe::TimeUntilNextProcess() const {
  rtc::CritScope crit(&lock_);
  return std::max<int64_t>(next_process_time_ - clock_->TimeInMilliseconds(),
                           0);
}

}  // namespace webrtc
