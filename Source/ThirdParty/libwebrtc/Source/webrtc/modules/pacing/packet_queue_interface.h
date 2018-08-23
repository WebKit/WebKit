/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACKET_QUEUE_INTERFACE_H_
#define MODULES_PACING_PACKET_QUEUE_INTERFACE_H_

#include <stdint.h>

#include <list>
#include <queue>
#include <set>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class PacketQueueInterface {
 public:
  PacketQueueInterface() = default;
  virtual ~PacketQueueInterface() = default;

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

  virtual void Push(const Packet& packet) = 0;
  virtual const Packet& BeginPop() = 0;
  virtual void CancelPop(const Packet& packet) = 0;
  virtual void FinalizePop(const Packet& packet) = 0;
  virtual bool Empty() const = 0;
  virtual size_t SizeInPackets() const = 0;
  virtual uint64_t SizeInBytes() const = 0;
  virtual int64_t OldestEnqueueTimeMs() const = 0;
  virtual void UpdateQueueTime(int64_t timestamp_ms) = 0;
  virtual void SetPauseState(bool paused, int64_t timestamp_ms) = 0;
  virtual int64_t AverageQueueTimeMs() const = 0;
};
}  // namespace webrtc

#endif  // MODULES_PACING_PACKET_QUEUE_INTERFACE_H_
