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

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/pacing/pacing_controller.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

class RoundRobinPacketQueue : public PacingController::PacketQueue {
 public:
  explicit RoundRobinPacketQueue(Timestamp start_time);
  ~RoundRobinPacketQueue();

  void Push(Timestamp enqueue_time,
            std::unique_ptr<RtpPacketToSend> packet) override;
  std::unique_ptr<RtpPacketToSend> Pop() override;

  int SizeInPackets() const override;
  DataSize SizeInPayloadBytes() const override;
  const std::array<int, kNumMediaTypes>& SizeInPacketsPerRtpPacketMediaType()
      const override;
  Timestamp LeadingAudioPacketEnqueueTime() const override;
  Timestamp OldestEnqueueTime() const override;
  TimeDelta AverageQueueTime() const override;
  void UpdateAverageQueueTime(Timestamp now) override;
  void SetPauseState(bool paused, Timestamp now) override;

 private:
  struct QueuedPacket {
   public:
    QueuedPacket(int priority,
                 Timestamp enqueue_time,
                 int64_t enqueue_order,
                 std::multiset<Timestamp>::iterator enqueue_time_it,
                 std::unique_ptr<RtpPacketToSend> packet);
    QueuedPacket(const QueuedPacket& rhs);
    ~QueuedPacket();

    bool operator<(const QueuedPacket& other) const;

    int Priority() const;
    RtpPacketMediaType Type() const;
    uint32_t Ssrc() const;
    Timestamp EnqueueTime() const;
    bool IsRetransmission() const;
    int64_t EnqueueOrder() const;
    RtpPacketToSend* RtpPacket() const;

    std::multiset<Timestamp>::iterator EnqueueTimeIterator() const;
    void UpdateEnqueueTimeIterator(std::multiset<Timestamp>::iterator it);
    void SubtractPauseTime(TimeDelta pause_time_sum);

   private:
    int priority_;
    Timestamp enqueue_time_;  // Absolute time of pacer queue entry.
    int64_t enqueue_order_;
    bool is_retransmission_;  // Cached for performance.
    std::multiset<Timestamp>::iterator enqueue_time_it_;
    // Raw pointer since priority_queue doesn't allow for moving
    // out of the container.
    RtpPacketToSend* owned_packet_;
  };

  class PriorityPacketQueue : public std::priority_queue<QueuedPacket> {
   public:
    using const_iterator = container_type::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
  };

  struct StreamPrioKey {
    StreamPrioKey(int priority, DataSize size)
        : priority(priority), size(size) {}

    bool operator<(const StreamPrioKey& other) const {
      if (priority != other.priority)
        return priority < other.priority;
      return size < other.size;
    }

    const int priority;
    const DataSize size;
  };

  struct Stream {
    Stream();
    Stream(const Stream&);

    virtual ~Stream();

    DataSize size;
    uint32_t ssrc;

    PriorityPacketQueue packet_queue;

    // Whenever a packet is inserted for this stream we check if `priority_it`
    // points to an element in `stream_priorities_`, and if it does it means
    // this stream has already been scheduled, and if the scheduled priority is
    // lower than the priority of the incoming packet we reschedule this stream
    // with the higher priority.
    std::multimap<StreamPrioKey, uint32_t>::iterator priority_it;
  };

  void Push(QueuedPacket packet);

  DataSize PacketSize(const QueuedPacket& packet) const;
  void MaybePromoteSinglePacketToNormalQueue();

  Stream* GetHighestPriorityStream();

  // Just used to verify correctness.
  bool IsSsrcScheduled(uint32_t ssrc) const;

  DataSize transport_overhead_per_packet_;

  Timestamp time_last_updated_;

  int64_t enqueue_count_;

  bool paused_;
  int size_packets_;
  std::array<int, kNumMediaTypes> size_packets_per_media_type_;
  DataSize size_;
  DataSize max_size_;
  TimeDelta queue_time_sum_;
  TimeDelta pause_time_sum_;

  // A map of streams used to prioritize from which stream to send next. We use
  // a multimap instead of a priority_queue since the priority of a stream can
  // change as a new packet is inserted, and a multimap allows us to remove and
  // then reinsert a StreamPrioKey if the priority has increased.
  std::multimap<StreamPrioKey, uint32_t> stream_priorities_;

  // A map of SSRCs to Streams.
  std::unordered_map<uint32_t, Stream> streams_;

  // The enqueue time of every packet currently in the queue. Used to figure out
  // the age of the oldest packet in the queue.
  std::multiset<Timestamp> enqueue_times_;

  absl::optional<QueuedPacket> single_packet_queue_;

  bool include_overhead_;
};
}  // namespace webrtc

#endif  // MODULES_PACING_ROUND_ROBIN_PACKET_QUEUE_H_
