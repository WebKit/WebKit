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

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/media_types.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc {
namespace test {

Demuxer::Demuxer(const std::map<uint8_t, MediaType>& payload_type_map)
    : payload_type_map_(payload_type_map) {}

MediaType Demuxer::GetMediaType(const uint8_t* packet_data,
                                const size_t packet_length) const {
  if (IsRtpPacket(MakeArrayView(packet_data, packet_length))) {
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
    TaskQueueBase* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map,
    ArrayView<const RtpExtension> audio_extensions,
    ArrayView<const RtpExtension> video_extensions)
    : clock_(*Clock::GetRealTimeClock()),
      send_call_(send_call),
      task_queue_(task_queue),
      demuxer_(payload_type_map),
      fake_network_(std::move(pipe)),
      audio_extensions_(audio_extensions),
      video_extensions_(video_extensions) {
  Start();
}

DirectTransport::DirectTransport(
    const Environment& env,
    TaskQueueBase* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map,
    ArrayView<const RtpExtension> audio_extensions,
    ArrayView<const RtpExtension> video_extensions)
    : env_(env),
      clock_(env.clock()),
      send_call_(send_call),
      task_queue_(task_queue),
      demuxer_(payload_type_map),
      fake_network_(std::move(pipe)),
      audio_extensions_(audio_extensions),
      video_extensions_(video_extensions) {
  Start();
}

DirectTransport::~DirectTransport() {
  next_process_task_.Stop();
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  fake_network_->SetReceiver(receiver);
}

bool DirectTransport::SendRtp(ArrayView<const uint8_t> data,
                              const PacketOptions& options) {
  if (send_call_) {
    SentPacketInfo sent_packet(options.packet_id, clock_.TimeInMilliseconds());
    sent_packet.info.included_in_feedback = options.included_in_feedback;
    sent_packet.info.included_in_allocation = options.included_in_allocation;
    sent_packet.info.packet_size_bytes = data.size();
    sent_packet.info.packet_type = PacketType::kData;
    send_call_->OnSentPacket(sent_packet);
  }

  const RtpHeaderExtensionMap* extensions = nullptr;
  MediaType media_type = demuxer_.GetMediaType(data.data(), data.size());
  switch (demuxer_.GetMediaType(data.data(), data.size())) {
    case MediaType::AUDIO:
      extensions = &audio_extensions_;
      break;
    case MediaType::VIDEO:
      extensions = &video_extensions_;
      break;
    default:
      RTC_CHECK_NOTREACHED();
  }
  RtpPacketReceived packet(extensions, clock_.CurrentTime());
  if (media_type == MediaType::VIDEO) {
    packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);
  }
  RTC_CHECK(packet.Parse(CopyOnWriteBuffer(data)));
  fake_network_->DeliverRtpPacket(
      media_type, std::move(packet),
      [](const RtpPacketReceived& packet) { return false; });

  MutexLock lock(&process_lock_);
  if (!next_process_task_.Running())
    ProcessPackets();
  return true;
}

bool DirectTransport::SendRtcp(ArrayView<const uint8_t> data,
                               const PacketOptions& /* options */) {
  fake_network_->DeliverRtcpPacket(CopyOnWriteBuffer(data));
  MutexLock lock(&process_lock_);
  if (!next_process_task_.Running())
    ProcessPackets();
  return true;
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
  std::optional<int64_t> initial_delay_ms =
      fake_network_->TimeUntilNextProcess();
  if (initial_delay_ms == std::nullopt)
    return;

  next_process_task_ = RepeatingTaskHandle::DelayedStart(
      task_queue_, TimeDelta::Millis(*initial_delay_ms), [this] {
        fake_network_->Process();
        if (auto delay_ms = fake_network_->TimeUntilNextProcess())
          return TimeDelta::Millis(*delay_ms);
        // Otherwise stop the task.
        MutexLock lock(&process_lock_);
        next_process_task_.Stop();
        // Since this task is stopped, return value doesn't matter.
        return TimeDelta::Zero();
      });
}
}  // namespace test
}  // namespace webrtc
