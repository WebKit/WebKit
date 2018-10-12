/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Based on the Quic implementation in Chromium.

#include <algorithm>

#include "modules/congestion_controller/bbr/bandwidth_sampler.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace bbr {
namespace {
constexpr int64_t kMaxTrackedPackets = 10000;
}

BandwidthSampler::BandwidthSampler()
    : total_data_sent_(DataSize::Zero()),
      total_data_acked_(DataSize::Zero()),
      total_data_sent_at_last_acked_packet_(DataSize::Zero()),
      last_acked_packet_sent_time_(),
      last_acked_packet_ack_time_(),
      last_sent_packet_(0),
      is_app_limited_(false),
      end_of_app_limited_phase_(0),
      connection_state_map_() {}

BandwidthSampler::~BandwidthSampler() {}

void BandwidthSampler::OnPacketSent(Timestamp sent_time,
                                    int64_t packet_number,
                                    DataSize data_size,
                                    DataSize data_in_flight) {
  last_sent_packet_ = packet_number;

  total_data_sent_ += data_size;

  // If there are no packets in flight, the time at which the new transmission
  // opens can be treated as the A_0 point for the purpose of bandwidth
  // sampling. This underestimates bandwidth to some extent, and produces some
  // artificially low samples for most packets in flight, but it provides with
  // samples at important points where we would not have them otherwise, most
  // importantly at the beginning of the connection.
  if (data_in_flight.IsZero()) {
    last_acked_packet_ack_time_ = sent_time;
    total_data_sent_at_last_acked_packet_ = total_data_sent_;

    // In this situation ack compression is not a concern, set send rate to
    // effectively infinite.
    last_acked_packet_sent_time_ = sent_time;
  }

  if (!connection_state_map_.IsEmpty() &&
      packet_number >
          connection_state_map_.last_packet() + kMaxTrackedPackets) {
    RTC_LOG(LS_WARNING)
        << "BandwidthSampler in-flight packet map has exceeded maximum "
           "number "
           "of tracked packets.";
  }

  bool success =
      connection_state_map_.Emplace(packet_number, sent_time, data_size, *this);
  if (!success)
    RTC_LOG(LS_WARNING) << "BandwidthSampler failed to insert the packet "
                           "into the map, most likely because it's already "
                           "in it.";
}

BandwidthSample BandwidthSampler::OnPacketAcknowledged(Timestamp ack_time,
                                                       int64_t packet_number) {
  ConnectionStateOnSentPacket* sent_packet_pointer =
      connection_state_map_.GetEntry(packet_number);
  if (sent_packet_pointer == nullptr) {
    return BandwidthSample();
  }
  BandwidthSample sample =
      OnPacketAcknowledgedInner(ack_time, packet_number, *sent_packet_pointer);
  connection_state_map_.Remove(packet_number);
  return sample;
}

BandwidthSample BandwidthSampler::OnPacketAcknowledgedInner(
    Timestamp ack_time,
    int64_t packet_number,
    const ConnectionStateOnSentPacket& sent_packet) {
  total_data_acked_ += sent_packet.size;
  total_data_sent_at_last_acked_packet_ = sent_packet.total_data_sent;
  last_acked_packet_sent_time_ = sent_packet.sent_time;
  last_acked_packet_ack_time_ = ack_time;

  // Exit app-limited phase once a packet that was sent while the connection is
  // not app-limited is acknowledged.
  if (is_app_limited_ && packet_number > end_of_app_limited_phase_) {
    is_app_limited_ = false;
  }

  // There might have been no packets acknowledged at the moment when the
  // current packet was sent. In that case, there is no bandwidth sample to
  // make.
  if (!sent_packet.last_acked_packet_sent_time ||
      !sent_packet.last_acked_packet_ack_time) {
    return BandwidthSample();
  }

  // Infinite rate indicates that the sampler is supposed to discard the
  // current send rate sample and use only the ack rate.
  DataRate send_rate = DataRate::Infinity();
  if (sent_packet.sent_time > *sent_packet.last_acked_packet_sent_time) {
    DataSize sent_delta = sent_packet.total_data_sent -
                          sent_packet.total_data_sent_at_last_acked_packet;
    TimeDelta time_delta =
        sent_packet.sent_time - *sent_packet.last_acked_packet_sent_time;
    send_rate = sent_delta / time_delta;
  }

  // During the slope calculation, ensure that ack time of the current packet is
  // always larger than the time of the previous packet, otherwise division by
  // zero or integer underflow can occur.
  if (ack_time <= *sent_packet.last_acked_packet_ack_time) {
    RTC_LOG(LS_WARNING)
        << "Time of the previously acked packet is larger than the time "
           "of the current packet.";
    return BandwidthSample();
  }
  DataSize ack_delta =
      total_data_acked_ - sent_packet.total_data_acked_at_the_last_acked_packet;
  TimeDelta time_delta = ack_time - *sent_packet.last_acked_packet_ack_time;
  DataRate ack_rate = ack_delta / time_delta;

  BandwidthSample sample;
  sample.bandwidth = std::min(send_rate, ack_rate);
  // Note: this sample does not account for delayed acknowledgement time.  This
  // means that the RTT measurements here can be artificially high, especially
  // on low bandwidth connections.
  sample.rtt = ack_time - sent_packet.sent_time;
  // A sample is app-limited if the packet was sent during the app-limited
  // phase.
  sample.is_app_limited = sent_packet.is_app_limited;
  return sample;
}

void BandwidthSampler::OnPacketLost(int64_t packet_number) {
  connection_state_map_.Remove(packet_number);
}

void BandwidthSampler::OnAppLimited() {
  is_app_limited_ = true;
  end_of_app_limited_phase_ = last_sent_packet_;
}

void BandwidthSampler::RemoveObsoletePackets(int64_t least_unacked) {
  while (!connection_state_map_.IsEmpty() &&
         connection_state_map_.first_packet() < least_unacked) {
    connection_state_map_.Remove(connection_state_map_.first_packet());
  }
}

DataSize BandwidthSampler::total_data_acked() const {
  return total_data_acked_;
}

bool BandwidthSampler::is_app_limited() const {
  return is_app_limited_;
}

int64_t BandwidthSampler::end_of_app_limited_phase() const {
  return end_of_app_limited_phase_;
}

BandwidthSampler::ConnectionStateOnSentPacket::ConnectionStateOnSentPacket(
    Timestamp sent_time,
    DataSize size,
    const BandwidthSampler& sampler)
    : sent_time(sent_time),
      size(size),
      total_data_sent(sampler.total_data_sent_),
      total_data_sent_at_last_acked_packet(
          sampler.total_data_sent_at_last_acked_packet_),
      last_acked_packet_sent_time(sampler.last_acked_packet_sent_time_),
      last_acked_packet_ack_time(sampler.last_acked_packet_ack_time_),
      total_data_acked_at_the_last_acked_packet(sampler.total_data_acked_),
      is_app_limited(sampler.is_app_limited_) {}

BandwidthSampler::ConnectionStateOnSentPacket::ConnectionStateOnSentPacket()
    : sent_time(Timestamp::MinusInfinity()),
      size(DataSize::Zero()),
      total_data_sent(DataSize::Zero()),
      total_data_sent_at_last_acked_packet(DataSize::Zero()),
      last_acked_packet_sent_time(),
      last_acked_packet_ack_time(),
      total_data_acked_at_the_last_acked_packet(DataSize::Zero()),
      is_app_limited(false) {}

BandwidthSampler::ConnectionStateOnSentPacket::~ConnectionStateOnSentPacket() {}

}  // namespace bbr
}  // namespace webrtc
