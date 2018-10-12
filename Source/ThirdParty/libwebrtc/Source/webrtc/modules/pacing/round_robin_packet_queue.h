/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
#define MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_

#include <list>
#include <map>
#include <queue>
#include <set>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RoundRobinPacketQueue {
 public:
  explicit RoundRobinPacketQueue(const Clock* clock);
  ~RoundRobinPacketQueue();

  struct Packet {
    Packet(RtpPacketSender::Priority priority,
           uint32_t ssrc,
           uint16_t seq_number,
           int64_t capture_time_ms,
           int64_t enqueue_time_ms,
           size_t length_in_bytes,
           bool retransmission,
           uint64_t enqueue_order);
    Packet(const Packet& other);
    virtual ~Packet();
    bool operator<(const Packet& other) const;

    RtpPacketSender::Priority priority;
    uint32_t ssrc;
    uint16_t sequence_number;
    int64_t capture_time_ms;  // Absolute time of frame capture.
    int64_t enqueue_time_ms;  // Absolute time of pacer queue entry.
    int64_t sum_paused_ms;
    size_t bytes;
    bool retransmission;
    uint64_t enqueue_order;
    std::list<Packet>::iterator this_it;
    std::multiset<int64_t>::iterator enqueue_time_it;
  };

  void Push(const Packet& packet);
  const Packet& BeginPop();
  void CancelPop(const Packet& packet);
  void FinalizePop(const Packet& packet);

  bool Empty() const;
  size_t SizeInPackets() const;
  uint64_t SizeInBytes() const;

  int64_t OldestEnqueueTimeMs() const;
  int64_t AverageQueueTimeMs() const;
  void UpdateQueueTime(int64_t timestamp_ms);
  void SetPauseState(bool paused, int64_t timestamp_ms);

 private:
  struct StreamPrioKey {
    StreamPrioKey(RtpPacketSender::Priority priority, int64_t bytes)
        : priority(priority), bytes(bytes) {}

    bool operator<(const StreamPrioKey& other) const {
      if (priority != other.priority)
        return priority < other.priority;
      return bytes < other.bytes;
    }

    const RtpPacketSender::Priority priority;
    const size_t bytes;
  };

  struct Stream {
    Stream();
    Stream(const Stream&);

    virtual ~Stream();

    size_t bytes;
    uint32_t ssrc;
    std::priority_queue<Packet> packet_queue;

    // Whenever a packet is inserted for this stream we check if |priority_it|
    // points to an element in |stream_priorities_|, and if it does it means
    // this stream has already been scheduled, and if the scheduled priority is
    // lower than the priority of the incoming packet we reschedule this stream
    // with the higher priority.
    std::multimap<StreamPrioKey, uint32_t>::iterator priority_it;
  };

  static constexpr size_t kMaxLeadingBytes = 1400;

  Stream* GetHighestPriorityStream();

  // Just used to verify correctness.
  bool IsSsrcScheduled(uint32_t ssrc) const;

  int64_t time_last_updated_;
  absl::optional<Packet> pop_packet_;
  absl::optional<Stream*> pop_stream_;

  bool paused_ = false;
  size_t size_packets_ = 0;
  size_t size_bytes_ = 0;
  size_t max_bytes_ = kMaxLeadingBytes;
  int64_t queue_time_sum_ms_ = 0;
  int64_t pause_time_sum_ms_ = 0;

  // A map of streams used to prioritize from which stream to send next. We use
  // a multimap instead of a priority_queue since the priority of a stream can
  // change as a new packet is inserted, and a multimap allows us to remove and
  // then reinsert a StreamPrioKey if the priority has increased.
  std::multimap<StreamPrioKey, uint32_t> stream_priorities_;

  // A map of SSRCs to Streams.
  std::map<uint32_t, Stream> streams_;

  // The enqueue time of every packet currently in the queue. Used to figure out
  // the age of the oldest packet in the queue.
  std::multiset<int64_t> enqueue_times_;
};
}  // namespace webrtc

#endif  // MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
