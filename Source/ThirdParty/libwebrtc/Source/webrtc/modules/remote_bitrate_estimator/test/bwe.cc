/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/bwe.h"

#include <algorithm>
#include <limits>

#include "modules/remote_bitrate_estimator/test/estimators/bbr.h"
#include "modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "modules/remote_bitrate_estimator/test/estimators/remb.h"
#include "modules/remote_bitrate_estimator/test/estimators/send_side.h"
#include "modules/remote_bitrate_estimator/test/estimators/tcp.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/system/fallthrough.h"

namespace webrtc {
namespace testing {
namespace bwe {

const int kSetCapacity = 1000;

BweReceiver::BweReceiver(int flow_id)
    : flow_id_(flow_id),
      received_packets_(kSetCapacity),
      rate_counter_(),
      loss_account_() {}

BweReceiver::BweReceiver(int flow_id, int64_t window_size_ms)
    : flow_id_(flow_id),
      received_packets_(kSetCapacity),
      rate_counter_(window_size_ms),
      loss_account_() {}

void BweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                const MediaPacket& media_packet) {
  if (received_packets_.size() == kSetCapacity) {
    RelieveSetAndUpdateLoss();
  }

  received_packets_.Insert(media_packet.sequence_number(),
                           media_packet.send_time_ms(), arrival_time_ms,
                           media_packet.payload_size());

  rate_counter_.UpdateRates(media_packet.send_time_ms() * 1000,
                            static_cast<uint32_t>(media_packet.payload_size()));
}

FeedbackPacket* BweReceiver::GetFeedback(int64_t now_ms) {
  return nullptr;
}

class NullBweSender : public BweSender {
 public:
  NullBweSender() {}
  ~NullBweSender() override {}

  int GetFeedbackIntervalMs() const override { return 1000; }
  void GiveFeedback(const FeedbackPacket& feedback) override {}
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override {
    return std::numeric_limits<int64_t>::max();
  }
  void Process() override {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(NullBweSender);
};

int64_t GetAbsSendTimeInMs(uint32_t abs_send_time) {
  const int kInterArrivalShift = 26;
  const int kAbsSendTimeInterArrivalUpshift = 8;
  const double kTimestampToMs =
      1000.0 / static_cast<double>(1 << kInterArrivalShift);
  uint32_t timestamp = abs_send_time << kAbsSendTimeInterArrivalUpshift;
  return static_cast<int64_t>(timestamp) * kTimestampToMs;
}

BweSender* CreateBweSender(BandwidthEstimatorType estimator,
                           int kbps,
                           BitrateObserver* observer,
                           Clock* clock) {
  switch (estimator) {
    case kRembEstimator:
      return new RembBweSender(kbps, observer, clock);
    case kSendSideEstimator:
      return new SendSideBweSender(kbps, observer, clock);
    case kNadaEstimator:
      return new NadaBweSender(kbps, observer, clock);
    case kBbrEstimator:
      return new BbrBweSender(observer, clock);
    case kTcpEstimator:
      RTC_FALLTHROUGH();
    case kNullEstimator:
      return new NullBweSender();
  }
  assert(false);
  return NULL;
}

BweReceiver* CreateBweReceiver(BandwidthEstimatorType type,
                               int flow_id,
                               bool plot) {
  switch (type) {
    case kRembEstimator:
      return new RembReceiver(flow_id, plot);
    case kSendSideEstimator:
      return new SendSideBweReceiver(flow_id);
    case kNadaEstimator:
      return new NadaBweReceiver(flow_id);
    case kBbrEstimator:
      return new BbrBweReceiver(flow_id);
    case kTcpEstimator:
      return new TcpBweReceiver(flow_id);
    case kNullEstimator:
      return new BweReceiver(flow_id);
  }
  assert(false);
  return NULL;
}

// Take into account all LinkedSet content.
void BweReceiver::UpdateLoss() {
  loss_account_.Add(LinkedSetPacketLossRatio());
}

// Preserve 10% latest packets and update packet loss based on the oldest
// 90%, that will be removed.
void BweReceiver::RelieveSetAndUpdateLoss() {
  // Compute Loss for the whole LinkedSet and updates loss_account_.
  UpdateLoss();

  size_t num_preserved_elements = received_packets_.size() / 10;
  PacketNodeIt it = received_packets_.begin();
  std::advance(it, num_preserved_elements);

  while (it != received_packets_.end()) {
    received_packets_.Erase(it++);
  }

  // Compute Loss for the preserved elements
  loss_account_.Subtract(LinkedSetPacketLossRatio());
}

float BweReceiver::GlobalReceiverPacketLossRatio() {
  UpdateLoss();
  return loss_account_.LossRatio();
}

// This function considers at most kSetCapacity = 1000 packets.
LossAccount BweReceiver::LinkedSetPacketLossRatio() {
  if (received_packets_.empty()) {
    return LossAccount();
  }

  size_t set_total_packets = received_packets_.Range();
  size_t set_received_packets = received_packets_.size();
  size_t set_lost_packets = set_total_packets - set_received_packets;

  return LossAccount(set_total_packets, set_lost_packets);
}

uint32_t BweReceiver::RecentKbps() const {
  return (rate_counter_.bits_per_second() + 500) / 1000;
}

// Go through a fixed time window of most recent packets received and
// counts packets missing to obtain the packet loss ratio. If an unordered
// packet falls out of the timewindow it will be counted as missing.
// E.g.: for a timewindow covering 5 packets of the following arrival sequence
// {10 7 9 5 6} 8 3 2 4 1, the output will be 1/6 (#8 is considered as missing).
float BweReceiver::RecentPacketLossRatio() {
  if (received_packets_.empty()) {
    return 0.0f;
  }
  int number_packets_received = 0;

  PacketNodeIt node_it = received_packets_.begin();  // Latest.

  // Lowest timestamp limit, oldest one that should be checked.
  int64_t time_limit_ms = node_it->arrival_time_ms - kPacketLossTimeWindowMs;
  // Oldest and newest values found within the given time window.
  int64_t oldest_seq_num = node_it->unwrapped_sequence_number;
  int64_t newest_seq_num = oldest_seq_num;

  while (node_it != received_packets_.end()) {
    if (node_it->arrival_time_ms < time_limit_ms) {
      break;
    }
    int64_t seq_num = node_it->unwrapped_sequence_number;
    newest_seq_num = std::max(newest_seq_num, seq_num);
    oldest_seq_num = std::min(oldest_seq_num, seq_num);
    ++node_it;
    ++number_packets_received;
  }
  // Interval width between oldest and newest sequence number.
  int gap = rtc::dchecked_cast<int>(newest_seq_num - oldest_seq_num + 1);

  return static_cast<float>(gap - number_packets_received) / gap;
}

LinkedSet::LinkedSet(int capacity) : capacity_(capacity) {}

LinkedSet::~LinkedSet() {
}

void LinkedSet::Insert(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size) {
  auto unwrapped_sequence_number = unwrapper_.Unwrap(sequence_number);
  auto it = map_.find(unwrapped_sequence_number);
  // Handle duplicate unwrapped sequence number.
  if (it != map_.end()) {
    PacketNodeIt node_it = it->second;
    PacketIdentifierNode& node = *node_it;
    node.arrival_time_ms = arrival_time_ms;
    // Move to front, without invalidating iterators.
    list_.splice(list_.begin(), list_, node_it);
  } else {
    if (size() == capacity_) {
      RemoveTail();
    }
    // Push new element to the front of the list and insert it in the map.
    PacketIdentifierNode new_head(unwrapped_sequence_number, send_time_ms,
                                  arrival_time_ms, payload_size);
    list_.push_front(new_head);
    map_[unwrapped_sequence_number] = list_.begin();
  }
}

void LinkedSet::RemoveTail() {
  Erase(--list_.end());
}

void LinkedSet::Erase(PacketNodeIt node_it) {
  map_.erase(node_it->unwrapped_sequence_number);
  list_.erase(node_it);
}

void LossAccount::Add(LossAccount rhs) {
  num_total += rhs.num_total;
  num_lost += rhs.num_lost;
}
void LossAccount::Subtract(LossAccount rhs) {
  num_total -= rhs.num_total;
  num_lost -= rhs.num_lost;
}

float LossAccount::LossRatio() {
  if (num_total == 0)
    return 0.0f;
  return static_cast<float>(num_lost) / num_total;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
