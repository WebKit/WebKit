/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/direct_transport.h"

#include "absl/memory/memory.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "rtc_base/time_utils.h"
#include "test/rtp_header_parser.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {
namespace test {

Demuxer::Demuxer(const std::map<uint8_t, MediaType>& payload_type_map)
    : payload_type_map_(payload_type_map) {}

MediaType Demuxer::GetMediaType(const uint8_t* packet_data,
                                const size_t packet_length) const {
  if (!RtpHeaderParser::IsRtcp(packet_data, packet_length)) {
    RTC_CHECK_GE(packet_length, 2);
    const uint8_t payload_type = packet_data[1] & 0x7f;
    std::map<uint8_t, MediaType>::const_iterator it =
        payload_type_map_.find(payload_type);
    RTC_CHECK(it != payload_type_map_.end())
        << "payload type " << static_cast<int>(payload_type) << " unknown.";
    return it->second;
  }
  return MediaType::ANY;
}

DirectTransport::DirectTransport(
    DEPRECATED_SingleThreadedTaskQueueForTesting* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : send_call_(send_call),
      task_queue_(task_queue),
      demuxer_(payload_type_map),
      fake_network_(std::move(pipe)) {
  Start();
}

DirectTransport::~DirectTransport() {
  if (next_process_task_)
    task_queue_->CancelTask(*next_process_task_);
}

void DirectTransport::StopSending() {
  rtc::CritScope cs(&process_lock_);
  if (next_process_task_)
    task_queue_->CancelTask(*next_process_task_);
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  rtc::CritScope cs(&process_lock_);
  fake_network_->SetReceiver(receiver);
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id, rtc::TimeMillis());
    sent_packet.info.included_in_feedback = options.included_in_feedback;
    sent_packet.info.included_in_allocation = options.included_in_allocation;
    sent_packet.info.packet_size_bytes = length;
    sent_packet.info.packet_type = rtc::PacketType::kData;
    send_call_->OnSentPacket(sent_packet);
  }
  SendPacket(data, length);
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  SendPacket(data, length);
  return true;
}

void DirectTransport::SendPacket(const uint8_t* data, size_t length) {
  MediaType media_type = demuxer_.GetMediaType(data, length);
  int64_t send_time_us = rtc::TimeMicros();
  fake_network_->DeliverPacket(media_type, rtc::CopyOnWriteBuffer(data, length),
                               send_time_us);
  rtc::CritScope cs(&process_lock_);
  if (!next_process_task_)
    ProcessPackets();
}

int DirectTransport::GetAverageDelayMs() {
  return fake_network_->AverageDelay();
}

void DirectTransport::Start() {
  RTC_DCHECK(task_queue_);
  if (send_call_) {
    send_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    send_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }
}

void DirectTransport::ProcessPackets() {
  next_process_task_.reset();
  auto delay_ms = fake_network_->TimeUntilNextProcess();
  if (delay_ms) {
    next_process_task_ = task_queue_->PostDelayedTask(
        [this]() {
          fake_network_->Process();
          rtc::CritScope cs(&process_lock_);
          ProcessPackets();
        },
        *delay_ms);
  }
}
}  // namespace test
}  // namespace webrtc
