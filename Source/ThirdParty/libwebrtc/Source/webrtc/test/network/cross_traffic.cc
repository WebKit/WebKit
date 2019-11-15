/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/cross_traffic.h"

#include <math.h>

#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {

RandomWalkCrossTraffic::RandomWalkCrossTraffic(RandomWalkConfig config,
                                               TrafficRoute* traffic_route)
    : config_(config),
      traffic_route_(traffic_route),
      random_(config_.random_seed) {
  sequence_checker_.Detach();
}
RandomWalkCrossTraffic::~RandomWalkCrossTraffic() = default;

void RandomWalkCrossTraffic::Process(Timestamp at_time) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = at_time;
  }
  TimeDelta delta = at_time - last_process_time_;
  last_process_time_ = at_time;

  if (at_time - last_update_time_ >= config_.update_interval) {
    intensity_ += random_.Gaussian(config_.bias, config_.variance) *
                  sqrt((at_time - last_update_time_).seconds<double>());
    intensity_ = rtc::SafeClamp(intensity_, 0.0, 1.0);
    last_update_time_ = at_time;
  }
  pending_size_ += TrafficRate() * delta;

  if (pending_size_ >= config_.min_packet_size &&
      at_time >= last_send_time_ + config_.min_packet_interval) {
    traffic_route_->SendPacket(pending_size_.bytes());
    pending_size_ = DataSize::Zero();
    last_send_time_ = at_time;
  }
}

DataRate RandomWalkCrossTraffic::TrafficRate() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return config_.peak_rate * intensity_;
}

ColumnPrinter RandomWalkCrossTraffic::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "random_walk_cross_traffic_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.0lf", TrafficRate().bps() / 8.0);
      },
      32);
}

PulsedPeaksCrossTraffic::PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                                                 TrafficRoute* traffic_route)
    : config_(config), traffic_route_(traffic_route) {
  sequence_checker_.Detach();
}
PulsedPeaksCrossTraffic::~PulsedPeaksCrossTraffic() = default;

void PulsedPeaksCrossTraffic::Process(Timestamp at_time) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  TimeDelta time_since_toggle = at_time - last_update_time_;
  if (time_since_toggle.IsInfinite() ||
      (sending_ && time_since_toggle >= config_.send_duration)) {
    sending_ = false;
    last_update_time_ = at_time;
  } else if (!sending_ && time_since_toggle >= config_.hold_duration) {
    sending_ = true;
    last_update_time_ = at_time;
    // Start sending period.
    last_send_time_ = at_time;
  }

  if (sending_) {
    DataSize pending_size = config_.peak_rate * (at_time - last_send_time_);

    if (pending_size >= config_.min_packet_size &&
        at_time >= last_send_time_ + config_.min_packet_interval) {
      traffic_route_->SendPacket(pending_size.bytes());
      last_send_time_ = at_time;
    }
  }
}

DataRate PulsedPeaksCrossTraffic::TrafficRate() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return sending_ ? config_.peak_rate : DataRate::Zero();
}

ColumnPrinter PulsedPeaksCrossTraffic::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "pulsed_peaks_cross_traffic_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.0lf", TrafficRate().bps() / 8.0);
      },
      32);
}

FakeTcpCrossTraffic::FakeTcpCrossTraffic(FakeTcpConfig config,
                                         EmulatedRoute* send_route,
                                         EmulatedRoute* ret_route)
    : conf_(config), route_(this, send_route, ret_route) {}

void FakeTcpCrossTraffic::Process(Timestamp at_time) {
  SendPackets(at_time);
}

void FakeTcpCrossTraffic::OnRequest(int sequence_number, Timestamp at_time) {
  const size_t kAckPacketSize = 20;
  route_.SendResponse(kAckPacketSize, sequence_number);
}

void FakeTcpCrossTraffic::OnResponse(int sequence_number, Timestamp at_time) {
  ack_received_ = true;
  auto it = in_flight_.find(sequence_number);
  if (it != in_flight_.end()) {
    last_rtt_ = at_time - in_flight_.at(sequence_number);
    in_flight_.erase(sequence_number);
  }
  if (sequence_number - last_acked_seq_num_ > 1) {
    HandleLoss(at_time);
  } else if (cwnd_ <= ssthresh_) {
    cwnd_ += 1;
  } else {
    cwnd_ += 1.0f / cwnd_;
  }
  last_acked_seq_num_ = std::max(sequence_number, last_acked_seq_num_);
  SendPackets(at_time);
}

void FakeTcpCrossTraffic::HandleLoss(Timestamp at_time) {
  if (at_time - last_reduction_time_ < last_rtt_)
    return;
  last_reduction_time_ = at_time;
  ssthresh_ = std::max(static_cast<int>(in_flight_.size() / 2), 2);
  cwnd_ = ssthresh_;
}

void FakeTcpCrossTraffic::SendPackets(Timestamp at_time) {
  int cwnd = std::ceil(cwnd_);
  int packets_to_send = std::max(cwnd - static_cast<int>(in_flight_.size()), 0);
  bool timeouts = false;
  for (auto it = in_flight_.begin(); it != in_flight_.end();) {
    if (it->second < at_time - conf_.packet_timeout) {
      it = in_flight_.erase(it);
      timeouts = true;
    } else {
      ++it;
    }
  }
  if (timeouts)
    HandleLoss(at_time);
  for (int i = 0; i < packets_to_send; ++i) {
    if ((total_sent_ + conf_.packet_size) > conf_.send_limit) {
      break;
    }
    in_flight_.insert({next_sequence_number_, at_time});
    route_.SendRequest(conf_.packet_size.bytes<size_t>(),
                       next_sequence_number_++);
    total_sent_ += conf_.packet_size;
  }
}

}  // namespace test
}  // namespace webrtc
