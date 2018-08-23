/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACKET_QUEUE_H_
#define MODULES_PACING_PACKET_QUEUE_H_

#include <list>
#include <queue>
#include <set>
#include <vector>

#include "modules/pacing/packet_queue_interface.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class PacketQueue : public PacketQueueInterface {
 public:
  explicit PacketQueue(const Clock* clock);
  ~PacketQueue() override;

  using Packet = PacketQueueInterface::Packet;

  void Push(const Packet& packet) override;
  const Packet& BeginPop() override;
  void CancelPop(const Packet& packet) override;
  void FinalizePop(const Packet& packet) override;
  bool Empty() const override;
  size_t SizeInPackets() const override;
  uint64_t SizeInBytes() const override;
  int64_t OldestEnqueueTimeMs() const override;
  void UpdateQueueTime(int64_t timestamp_ms) override;
  void SetPauseState(bool paused, int64_t timestamp_ms) override;
  int64_t AverageQueueTimeMs() const override;

 private:
  // Try to add a packet to the set of ssrc/seqno identifiers currently in the
  // queue. Return true if inserted, false if this is a duplicate.
  bool AddToDupeSet(const Packet& packet);

  void RemoveFromDupeSet(const Packet& packet);

  // Used by priority queue to sort packets.
  struct Comparator {
    bool operator()(const Packet* first, const Packet* second) {
      // Highest prio = 0.
      if (first->priority != second->priority)
        return first->priority > second->priority;

      // Retransmissions go first.
      if (second->retransmission != first->retransmission)
        return second->retransmission;

      // Older frames have higher prio.
      if (first->capture_time_ms != second->capture_time_ms)
        return first->capture_time_ms > second->capture_time_ms;

      return first->enqueue_order > second->enqueue_order;
    }
  };

  // List of packets, in the order the were enqueued. Since dequeueing may
  // occur out of order, use list instead of vector.
  std::list<Packet> packet_list_;
  // Priority queue of the packets, sorted according to Comparator.
  // Use pointers into list, to avodi moving whole struct within heap.
  std::priority_queue<Packet*, std::vector<Packet*>, Comparator> prio_queue_;
  // Total number of bytes in the queue.
  uint64_t bytes_;
  const Clock* const clock_;
  int64_t queue_time_sum_;
  int64_t time_last_updated_;
  bool paused_;
};
}  // namespace webrtc

#endif  // MODULES_PACING_PACKET_QUEUE_H_
