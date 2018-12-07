/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {
constexpr int64_t kDefaultProcessIntervalMs = 5;
constexpr int64_t kLogIntervalMs = 5000;
}  // namespace

NetworkPacket::NetworkPacket(rtc::CopyOnWriteBuffer packet,
                             int64_t send_time,
                             int64_t arrival_time,
                             absl::optional<PacketOptions> packet_options,
                             bool is_rtcp,
                             MediaType media_type,
                             absl::optional<int64_t> packet_time_us)
    : packet_(std::move(packet)),
      send_time_(send_time),
      arrival_time_(arrival_time),
      packet_options_(packet_options),
      is_rtcp_(is_rtcp),
      media_type_(media_type),
      packet_time_us_(packet_time_us) {}

NetworkPacket::NetworkPacket(NetworkPacket&& o)
    : packet_(std::move(o.packet_)),
      send_time_(o.send_time_),
      arrival_time_(o.arrival_time_),
      packet_options_(o.packet_options_),
      is_rtcp_(o.is_rtcp_),
      media_type_(o.media_type_),
      packet_time_us_(o.packet_time_us_) {}

NetworkPacket::~NetworkPacket() = default;

NetworkPacket& NetworkPacket::operator=(NetworkPacket&& o) {
  packet_ = std::move(o.packet_);
  send_time_ = o.send_time_;
  arrival_time_ = o.arrival_time_;
  packet_options_ = o.packet_options_;
  is_rtcp_ = o.is_rtcp_;
  media_type_ = o.media_type_;
  packet_time_us_ = o.packet_time_us_;

  return *this;
}

FakeNetworkPipe::FakeNetworkPipe(
    Clock* clock,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior)
    : FakeNetworkPipe(clock, std::move(network_behavior), nullptr, 1) {}

FakeNetworkPipe::FakeNetworkPipe(
    Clock* clock,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    PacketReceiver* receiver)
    : FakeNetworkPipe(clock, std::move(network_behavior), receiver, 1) {}

FakeNetworkPipe::FakeNetworkPipe(
    Clock* clock,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    PacketReceiver* receiver,
    uint64_t seed)
    : clock_(clock),
      network_behavior_(std::move(network_behavior)),
      receiver_(receiver),
      transport_(nullptr),
      clock_offset_ms_(0),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_us_(0),
      last_log_time_us_(clock_->TimeInMicroseconds()) {}

FakeNetworkPipe::FakeNetworkPipe(
    Clock* clock,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior,
    Transport* transport)
    : clock_(clock),
      network_behavior_(std::move(network_behavior)),
      receiver_(nullptr),
      transport_(transport),
      clock_offset_ms_(0),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_us_(0),
      last_log_time_us_(clock_->TimeInMicroseconds()) {}

FakeNetworkPipe::~FakeNetworkPipe() = default;

void FakeNetworkPipe::SetReceiver(PacketReceiver* receiver) {
  rtc::CritScope crit(&config_lock_);
  receiver_ = receiver;
}

bool FakeNetworkPipe::SendRtp(const uint8_t* packet,
                              size_t length,
                              const PacketOptions& options) {
  RTC_DCHECK(HasTransport());
  EnqueuePacket(rtc::CopyOnWriteBuffer(packet, length), options, false,
                MediaType::ANY);
  return true;
}

bool FakeNetworkPipe::SendRtcp(const uint8_t* packet, size_t length) {
  RTC_DCHECK(HasTransport());
  EnqueuePacket(rtc::CopyOnWriteBuffer(packet, length), absl::nullopt, true,
                MediaType::ANY);
  return true;
}

PacketReceiver::DeliveryStatus FakeNetworkPipe::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    int64_t packet_time_us) {
  return EnqueuePacket(std::move(packet), absl::nullopt, false, media_type,
                       packet_time_us)
             ? PacketReceiver::DELIVERY_OK
             : PacketReceiver::DELIVERY_PACKET_ERROR;
}

void FakeNetworkPipe::SetClockOffset(int64_t offset_ms) {
  rtc::CritScope crit(&config_lock_);
  clock_offset_ms_ = offset_ms;
}

FakeNetworkPipe::StoredPacket::StoredPacket(NetworkPacket&& packet)
    : packet(std::move(packet)) {}

bool FakeNetworkPipe::EnqueuePacket(rtc::CopyOnWriteBuffer packet,
                                    absl::optional<PacketOptions> options,
                                    bool is_rtcp,
                                    MediaType media_type,
                                    absl::optional<int64_t> packet_time_us) {
  rtc::CritScope crit(&process_lock_);
  int64_t time_now_us = clock_->TimeInMicroseconds();
  size_t packet_size = packet.size();
  NetworkPacket net_packet(std::move(packet), time_now_us, time_now_us, options,
                           is_rtcp, media_type, packet_time_us);

  packets_in_flight_.emplace_back(StoredPacket(std::move(net_packet)));
  int64_t packet_id = reinterpret_cast<uint64_t>(&packets_in_flight_.back());
  bool sent = network_behavior_->EnqueuePacket(
      PacketInFlightInfo(packet_size, time_now_us, packet_id));

  if (!sent) {
    packets_in_flight_.pop_back();
    ++dropped_packets_;
  }
  if (network_behavior_->NextDeliveryTimeUs()) {
    rtc::CritScope crit(&process_thread_lock_);
    if (process_thread_)
      process_thread_->WakeUp(nullptr);
  }

  return sent;
}

float FakeNetworkPipe::PercentageLoss() {
  rtc::CritScope crit(&process_lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<float>(dropped_packets_) /
         (sent_packets_ + dropped_packets_);
}

int FakeNetworkPipe::AverageDelay() {
  rtc::CritScope crit(&process_lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<int>(total_packet_delay_us_ /
                          (1000 * static_cast<int64_t>(sent_packets_)));
}

size_t FakeNetworkPipe::DroppedPackets() {
  rtc::CritScope crit(&process_lock_);
  return dropped_packets_;
}

size_t FakeNetworkPipe::SentPackets() {
  rtc::CritScope crit(&process_lock_);
  return sent_packets_;
}

void FakeNetworkPipe::Process() {
  int64_t time_now_us;
  std::queue<NetworkPacket> packets_to_deliver;
  {
    rtc::CritScope crit(&process_lock_);
    time_now_us = clock_->TimeInMicroseconds();
    if (time_now_us - last_log_time_us_ > kLogIntervalMs * 1000) {
      int64_t queueing_delay_us = 0;
      if (!packets_in_flight_.empty())
        queueing_delay_us =
            time_now_us - packets_in_flight_.front().packet.send_time();

      RTC_LOG(LS_INFO) << "Network queue: " << queueing_delay_us / 1000
                       << " ms.";
      last_log_time_us_ = time_now_us;
    }

    std::vector<PacketDeliveryInfo> delivery_infos =
        network_behavior_->DequeueDeliverablePackets(time_now_us);
    for (auto& delivery_info : delivery_infos) {
      // In the common case where no reordering happens, find will return early
      // as the first packet will be a match.
      auto packet_it =
          std::find_if(packets_in_flight_.begin(), packets_in_flight_.end(),
                       [&delivery_info](StoredPacket& packet_ref) {
                         return reinterpret_cast<uint64_t>(&packet_ref) ==
                                delivery_info.packet_id;
                       });
      // Check that the packet is in the deque of packets in flight.
      RTC_CHECK(packet_it != packets_in_flight_.end());
      // Check that the packet is not already removed.
      RTC_DCHECK(!packet_it->removed);

      NetworkPacket packet = std::move(packet_it->packet);
      packet_it->removed = true;

      // Cleanup of removed packets at the beginning of the deque.
      while (!packets_in_flight_.empty() &&
             packets_in_flight_.front().removed) {
        packets_in_flight_.pop_front();
      }

      if (delivery_info.receive_time_us != PacketDeliveryInfo::kNotReceived) {
        int64_t added_delay_us =
            delivery_info.receive_time_us - packet.send_time();
        packet.IncrementArrivalTime(added_delay_us);
        packets_to_deliver.emplace(std::move(packet));
        // |time_now_us| might be later than when the packet should have
        // arrived, due to NetworkProcess being called too late. For stats, use
        // the time it should have been on the link.
        total_packet_delay_us_ += added_delay_us;
        ++sent_packets_;
      } else {
        ++dropped_packets_;
      }
    }
  }

  rtc::CritScope crit(&config_lock_);
  while (!packets_to_deliver.empty()) {
    NetworkPacket packet = std::move(packets_to_deliver.front());
    packets_to_deliver.pop();
    DeliverNetworkPacket(&packet);
  }
}

void FakeNetworkPipe::DeliverNetworkPacket(NetworkPacket* packet) {
  if (transport_) {
    RTC_DCHECK(!receiver_);
    if (packet->is_rtcp()) {
      transport_->SendRtcp(packet->data(), packet->data_length());
    } else {
      transport_->SendRtp(packet->data(), packet->data_length(),
                          packet->packet_options());
    }
  } else if (receiver_) {
    int64_t packet_time_us = packet->packet_time_us().value_or(-1);
    if (packet_time_us != -1) {
      int64_t queue_time_us = packet->arrival_time() - packet->send_time();
      RTC_CHECK(queue_time_us >= 0);
      packet_time_us += queue_time_us;
      packet_time_us += (clock_offset_ms_ * 1000);
    }
    receiver_->DeliverPacket(packet->media_type(),
                             std::move(*packet->raw_packet()), packet_time_us);
  }
}

int64_t FakeNetworkPipe::TimeUntilNextProcess() {
  rtc::CritScope crit(&process_lock_);
  absl::optional<int64_t> delivery_us = network_behavior_->NextDeliveryTimeUs();
  if (delivery_us) {
    int64_t delay_us = *delivery_us - clock_->TimeInMicroseconds();
    return std::max<int64_t>((delay_us + 500) / 1000, 0);
  }
  return kDefaultProcessIntervalMs;
}

void FakeNetworkPipe::ProcessThreadAttached(ProcessThread* process_thread) {
  rtc::CritScope cs(&process_thread_lock_);
  process_thread_ = process_thread;
}

bool FakeNetworkPipe::HasTransport() const {
  rtc::CritScope crit(&config_lock_);
  return transport_ != nullptr;
}
bool FakeNetworkPipe::HasReceiver() const {
  rtc::CritScope crit(&config_lock_);
  return receiver_ != nullptr;
}

void FakeNetworkPipe::DeliverPacketWithLock(NetworkPacket* packet) {
  rtc::CritScope crit(&config_lock_);
  DeliverNetworkPacket(packet);
}

void FakeNetworkPipe::ResetStats() {
  rtc::CritScope crit(&process_lock_);
  dropped_packets_ = 0;
  sent_packets_ = 0;
  total_packet_delay_us_ = 0;
}

int64_t FakeNetworkPipe::GetTimeInMicroseconds() const {
  return clock_->TimeInMicroseconds();
}

}  // namespace webrtc
