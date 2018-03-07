/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/packet_queue.h"

#include <algorithm>
#include <list>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/pacing/alr_detector.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

PacketQueue::Packet::Packet(RtpPacketSender::Priority priority,
                            uint32_t ssrc,
                            uint16_t seq_number,
                            int64_t capture_time_ms,
                            int64_t enqueue_time_ms,
                            size_t length_in_bytes,
                            bool retransmission,
                            uint64_t enqueue_order)
    : priority(priority),
      ssrc(ssrc),
      sequence_number(seq_number),
      capture_time_ms(capture_time_ms),
      enqueue_time_ms(enqueue_time_ms),
      sum_paused_ms(0),
      bytes(length_in_bytes),
      retransmission(retransmission),
      enqueue_order(enqueue_order) {}

PacketQueue::Packet::Packet(const Packet& other) = default;

PacketQueue::Packet::~Packet() {}

PacketQueue::PacketQueue(const Clock* clock)
    : bytes_(0),
      clock_(clock),
      queue_time_sum_(0),
      time_last_updated_(clock_->TimeInMilliseconds()),
      paused_(false) {}

PacketQueue::~PacketQueue() {}

void PacketQueue::Push(const Packet& packet) {
  UpdateQueueTime(packet.enqueue_time_ms);

  // Store packet in list, use pointers in priority queue for cheaper moves.
  // Packets have a handle to its own iterator in the list, for easy removal
  // when popping from queue.
  packet_list_.push_front(packet);
  std::list<Packet>::iterator it = packet_list_.begin();
  it->this_it = it;          // Handle for direct removal from list.
  prio_queue_.push(&(*it));  // Pointer into list.
  bytes_ += packet.bytes;
}

const PacketQueue::Packet& PacketQueue::BeginPop() {
  const PacketQueue::Packet& packet = *prio_queue_.top();
  prio_queue_.pop();
  return packet;
}

void PacketQueue::CancelPop(const PacketQueue::Packet& packet) {
  prio_queue_.push(&(*packet.this_it));
}

void PacketQueue::FinalizePop(const PacketQueue::Packet& packet) {
  bytes_ -= packet.bytes;
  int64_t packet_queue_time_ms = time_last_updated_ - packet.enqueue_time_ms;
  RTC_DCHECK_LE(packet.sum_paused_ms, packet_queue_time_ms);
  packet_queue_time_ms -= packet.sum_paused_ms;
  RTC_DCHECK_LE(packet_queue_time_ms, queue_time_sum_);
  queue_time_sum_ -= packet_queue_time_ms;
  packet_list_.erase(packet.this_it);
  RTC_DCHECK_EQ(packet_list_.size(), prio_queue_.size());
  if (packet_list_.empty())
    RTC_DCHECK_EQ(0, queue_time_sum_);
}

bool PacketQueue::Empty() const {
  return prio_queue_.empty();
}

size_t PacketQueue::SizeInPackets() const {
  return prio_queue_.size();
}

uint64_t PacketQueue::SizeInBytes() const {
  return bytes_;
}

int64_t PacketQueue::OldestEnqueueTimeMs() const {
  auto it = packet_list_.rbegin();
  if (it == packet_list_.rend())
    return 0;
  return it->enqueue_time_ms;
}

void PacketQueue::UpdateQueueTime(int64_t timestamp_ms) {
  RTC_DCHECK_GE(timestamp_ms, time_last_updated_);
  if (timestamp_ms == time_last_updated_)
    return;

  int64_t delta_ms = timestamp_ms - time_last_updated_;

  if (paused_) {
    // Increase per-packet accumulators of time spent in queue while paused,
    // so that we can disregard that when subtracting main accumulator when
    // popping packet from the queue.
    for (auto& it : packet_list_) {
      it.sum_paused_ms += delta_ms;
    }
  } else {
    // Use packet packet_list_.size() not prio_queue_.size() here, as there
    // might be an outstanding element popped from prio_queue_ currently in
    // the SendPacket() call, while packet_list_ will always be correct.
    queue_time_sum_ += delta_ms * packet_list_.size();
  }
  time_last_updated_ = timestamp_ms;
}

void PacketQueue::SetPauseState(bool paused, int64_t timestamp_ms) {
  if (paused_ == paused)
    return;
  UpdateQueueTime(timestamp_ms);
  paused_ = paused;
}

int64_t PacketQueue::AverageQueueTimeMs() const {
  if (prio_queue_.empty())
    return 0;
  return queue_time_sum_ / packet_list_.size();
}

}  // namespace webrtc
