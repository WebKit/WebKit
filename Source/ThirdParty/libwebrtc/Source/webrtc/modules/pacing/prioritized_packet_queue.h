/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PRIORITIZED_PACKET_QUEUE_H_
#define MODULES_PACING_PRIORITIZED_PACKET_QUEUE_H_

#include <stddef.h>

#include <deque>
#include <list>
#include <memory>
#include <unordered_map>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/pacing/pacing_controller.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

class PrioritizedPacketQueue : public PacingController::PacketQueue {
 public:
  explicit PrioritizedPacketQueue(Timestamp creation_time);
  PrioritizedPacketQueue(const PrioritizedPacketQueue&) = delete;
  PrioritizedPacketQueue& operator=(const PrioritizedPacketQueue&) = delete;

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
  static constexpr int kNumPriorityLevels = 4;

  class QueuedPacket {
   public:
    DataSize PacketSize() const;

    std::unique_ptr<RtpPacketToSend> packet;
    Timestamp enqueue_time;
    std::list<Timestamp>::iterator enqueue_time_iterator;
  };

  // Class containing packets for an RTP stream.
  // For each priority level, packets are simply stored in a fifo queue.
  class StreamQueue {
   public:
    explicit StreamQueue(Timestamp creation_time);
    StreamQueue(StreamQueue&&) = default;
    StreamQueue& operator=(StreamQueue&&) = default;

    StreamQueue(const StreamQueue&) = delete;
    StreamQueue& operator=(const StreamQueue&) = delete;

    // Enqueue packet at the given priority level. Returns true if the packet
    // count for that priority level went from zero to non-zero.
    bool EnqueuePacket(QueuedPacket packet, int priority_level);

    QueuedPacket DequePacket(int priority_level);

    bool HasPacketsAtPrio(int priority_level) const;
    bool IsEmpty() const;
    Timestamp LeadingAudioPacketEnqueueTime() const;
    Timestamp LastEnqueueTime() const;

   private:
    std::deque<QueuedPacket> packets_[kNumPriorityLevels];
    Timestamp last_enqueue_time_;
  };

  // Cumulative sum, over all packets, of time spent in the queue.
  TimeDelta queue_time_sum_;
  // Cumulative sum of time the queue has spent in a paused state.
  TimeDelta pause_time_sum_;
  // Total number of packets stored in this queue.
  int size_packets_;
  // Total number of packets stored in this queue per RtpPacketMediaType.
  std::array<int, kNumMediaTypes> size_packets_per_media_type_;
  // Sum of payload sizes for all packts stored in this queue.
  DataSize size_payload_;
  // The last time queue/pause time sums were updated.
  Timestamp last_update_time_;
  bool paused_;

  // Last time `streams_` was culled for inactive streams.
  Timestamp last_culling_time_;

  // Map from SSRC to packet queues for the associated RTP stream.
  std::unordered_map<uint32_t, std::unique_ptr<StreamQueue>> streams_;

  // For each priority level, a queue of StreamQueues which have at least one
  // packet pending for that prio level.
  std::deque<StreamQueue*> streams_by_prio_[kNumPriorityLevels];

  // The first index into `stream_by_prio_` that is non-empty.
  int top_active_prio_level_;

  // Ordered list of enqueue times. Additions are always increasing and added to
  // the end. QueuedPacket instances have a iterators into this list for fast
  // removal.
  std::list<Timestamp> enqueue_times_;
};

}  // namespace webrtc

#endif  // MODULES_PACING_PRIORITIZED_PACKET_QUEUE_H_
