/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/packet_queue2.h"

#include <algorithm>

#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

PacketQueue2::Stream::Stream() : bytes(0) {}
PacketQueue2::Stream::~Stream() {}

PacketQueue2::PacketQueue2(const Clock* clock)
    : PacketQueue(clock),
      clock_(clock),
      time_last_updated_(clock_->TimeInMilliseconds()) {}

PacketQueue2::~PacketQueue2() {}

void PacketQueue2::Push(const Packet& packet_to_insert) {
  Packet packet(packet_to_insert);

  auto stream_info_it = streams_.find(packet.ssrc);
  if (stream_info_it == streams_.end()) {
    stream_info_it = streams_.emplace(packet.ssrc, Stream()).first;
    stream_info_it->second.priority_it = stream_priorities_.end();
    stream_info_it->second.ssrc = packet.ssrc;
  }

  Stream* streams_ = &stream_info_it->second;

  if (streams_->priority_it == stream_priorities_.end()) {
    // If the SSRC is not currently scheduled, add it to |stream_priorities_|.
    RTC_CHECK(!IsSsrcScheduled(streams_->ssrc));
    streams_->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.priority, streams_->bytes), packet.ssrc);
  } else if (packet.priority < streams_->priority_it->first.priority) {
    // If the priority of this SSRC increased, remove the outdated StreamPrioKey
    // and insert a new one with the new priority. Note that
    // RtpPacketSender::Priority uses lower ordinal for higher priority.
    stream_priorities_.erase(streams_->priority_it);
    streams_->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.priority, streams_->bytes), packet.ssrc);
  }
  RTC_CHECK(streams_->priority_it != stream_priorities_.end());

  packet.enqueue_time_it = enqueue_times_.insert(packet.enqueue_time_ms);

  // In order to figure out how much time a packet has spent in the queue while
  // not in a paused state, we subtract the total amount of time the queue has
  // been paused so far, and when the packet is poped we subtract the total
  // amount of time the queue has been paused at that moment. This way we
  // subtract the total amount of time the packet has spent in the queue while
  // in a paused state.
  UpdateQueueTime(packet.enqueue_time_ms);
  packet.enqueue_time_ms -= pause_time_sum_ms_;
  streams_->packet_queue.push(packet);

  size_packets_ += 1;
  size_bytes_ += packet.bytes;
}

const PacketQueue2::Packet& PacketQueue2::BeginPop() {
  RTC_CHECK(!pop_packet_ && !pop_stream_);

  Stream* stream = GetHighestPriorityStream();
  pop_stream_.emplace(stream);
  pop_packet_.emplace(stream->packet_queue.top());
  stream->packet_queue.pop();

  return *pop_packet_;
}

void PacketQueue2::CancelPop(const Packet& packet) {
  RTC_CHECK(pop_packet_ && pop_stream_);
  (*pop_stream_)->packet_queue.push(*pop_packet_);
  pop_packet_.reset();
  pop_stream_.reset();
}

void PacketQueue2::FinalizePop(const Packet& packet) {
  RTC_CHECK(!paused_);
  if (!Empty()) {
    RTC_CHECK(pop_packet_ && pop_stream_);
    Stream* stream = *pop_stream_;
    stream_priorities_.erase(stream->priority_it);
    const Packet& packet = *pop_packet_;

    // Calculate the total amount of time spent by this packet in the queue
    // while in a non-paused state. Note that the |pause_time_sum_ms_| was
    // subtracted from |packet.enqueue_time_ms| when the packet was pushed, and
    // by subtracting it now we effectively remove the time spent in in the
    // queue while in a paused state.
    int64_t time_in_non_paused_state_ms =
        time_last_updated_ - packet.enqueue_time_ms - pause_time_sum_ms_;
    queue_time_sum_ms_ -= time_in_non_paused_state_ms;

    RTC_CHECK(packet.enqueue_time_it != enqueue_times_.end());
    enqueue_times_.erase(packet.enqueue_time_it);

    // Update |bytes| of this stream. The general idea is that the stream that
    // has sent the least amount of bytes should have the highest priority.
    // The problem with that is if streams send with different rates, in which
    // case a "budget" will be built up for the stream sending at the lower
    // rate. To avoid building a too large budget we limit |bytes| to be within
    // kMaxLeading bytes of the stream that has sent the most amount of bytes.
    stream->bytes =
        std::max(stream->bytes + packet.bytes, max_bytes_ - kMaxLeadingBytes);
    max_bytes_ = std::max(max_bytes_, stream->bytes);

    size_bytes_ -= packet.bytes;
    size_packets_ -= 1;
    RTC_CHECK(size_packets_ > 0 || queue_time_sum_ms_ == 0);

    // If there are packets left to be sent, schedule the stream again.
    RTC_CHECK(!IsSsrcScheduled(stream->ssrc));
    if (stream->packet_queue.empty()) {
      stream->priority_it = stream_priorities_.end();
    } else {
      RtpPacketSender::Priority priority = stream->packet_queue.top().priority;
      stream->priority_it = stream_priorities_.emplace(
          StreamPrioKey(priority, stream->bytes), stream->ssrc);
    }

    pop_packet_.reset();
    pop_stream_.reset();
  }
}

bool PacketQueue2::Empty() const {
  RTC_CHECK((!stream_priorities_.empty() && size_packets_ > 0) ||
            (stream_priorities_.empty() && size_packets_ == 0));
  return stream_priorities_.empty();
}

size_t PacketQueue2::SizeInPackets() const {
  return size_packets_;
}

uint64_t PacketQueue2::SizeInBytes() const {
  return size_bytes_;
}

int64_t PacketQueue2::OldestEnqueueTimeMs() const {
  if (Empty())
    return 0;
  RTC_CHECK(!enqueue_times_.empty());
  return *enqueue_times_.begin();
}

void PacketQueue2::UpdateQueueTime(int64_t timestamp_ms) {
  RTC_CHECK_GE(timestamp_ms, time_last_updated_);
  if (timestamp_ms == time_last_updated_)
    return;

  int64_t delta_ms = timestamp_ms - time_last_updated_;

  if (paused_) {
    pause_time_sum_ms_ += delta_ms;
  } else {
    queue_time_sum_ms_ += delta_ms * size_packets_;
  }

  time_last_updated_ = timestamp_ms;
}

void PacketQueue2::SetPauseState(bool paused, int64_t timestamp_ms) {
  if (paused_ == paused)
    return;
  UpdateQueueTime(timestamp_ms);
  paused_ = paused;
}

int64_t PacketQueue2::AverageQueueTimeMs() const {
  if (Empty())
    return 0;
  return queue_time_sum_ms_ / size_packets_;
}

PacketQueue2::Stream* PacketQueue2::GetHighestPriorityStream() {
  RTC_CHECK(!stream_priorities_.empty());
  uint32_t ssrc = stream_priorities_.begin()->second;

  auto stream_info_it = streams_.find(ssrc);
  RTC_CHECK(stream_info_it != streams_.end());
  RTC_CHECK(stream_info_it->second.priority_it == stream_priorities_.begin());
  RTC_CHECK(!stream_info_it->second.packet_queue.empty());
  return &stream_info_it->second;
}

bool PacketQueue2::IsSsrcScheduled(uint32_t ssrc) const {
  for (const auto& scheduled_stream : stream_priorities_) {
    if (scheduled_stream.second == ssrc)
      return true;
  }
  return false;
}

}  // namespace webrtc
