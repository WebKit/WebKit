/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/packet.h"

namespace webrtc {
namespace testing {
namespace bwe {

Packet::Packet()
    : flow_id_(0),
      creation_time_us_(-1),
      send_time_us_(-1),
      sender_timestamp_us_(-1),
      payload_size_(0) {}

Packet::Packet(int flow_id, int64_t send_time_us, size_t payload_size)
    : flow_id_(flow_id),
      creation_time_us_(send_time_us),
      send_time_us_(send_time_us),
      sender_timestamp_us_(send_time_us),
      payload_size_(payload_size) {}

Packet::~Packet() {}

bool Packet::operator<(const Packet& rhs) const {
  return send_time_us_ < rhs.send_time_us_;
}

void Packet::set_send_time_us(int64_t send_time_us) {
  assert(send_time_us >= 0);
  send_time_us_ = send_time_us;
}

int Packet::flow_id() const {
  return flow_id_;
}

int64_t Packet::send_time_us() const {
  return send_time_us_;
}

int64_t Packet::sender_timestamp_us() const {
  return sender_timestamp_us_;
}

size_t Packet::payload_size() const {
  return payload_size_;
}

void Packet::set_sender_timestamp_us(int64_t sender_timestamp_us) {
  sender_timestamp_us_ = sender_timestamp_us;
}

int64_t Packet::creation_time_ms() const {
  return (creation_time_us_ + 500) / 1000;
}

int64_t Packet::sender_timestamp_ms() const {
  return (sender_timestamp_us_ + 500) / 1000;
}

int64_t Packet::send_time_ms() const {
  return (send_time_us_ + 500) / 1000;
}

Packet::Type MediaPacket::GetPacketType() const {
  return kMedia;
}

Packet::Type FeedbackPacket::GetPacketType() const {
  return kFeedback;
}

BbrBweFeedback::~BbrBweFeedback() = default;

SendSideBweFeedback::~SendSideBweFeedback() = default;

TcpFeedback::TcpFeedback(int flow_id,
                         int64_t send_time_us,
                         int64_t latest_send_time_ms,
                         const std::vector<uint16_t>& acked_packets)
    : FeedbackPacket(flow_id, send_time_us, latest_send_time_ms),
      acked_packets_(acked_packets) {}

TcpFeedback::~TcpFeedback() = default;

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
