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

#include "call/call.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {
namespace test {

DirectTransport::DirectTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(task_queue,
                      FakeNetworkPipe::Config(),
                      send_call,
                      payload_type_map) {
}

DirectTransport::DirectTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    const FakeNetworkPipe::Config& config,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(
          task_queue,
          config,
          send_call,
          std::unique_ptr<Demuxer>(new DemuxerImpl(payload_type_map))) {
}

DirectTransport::DirectTransport(SingleThreadedTaskQueueForTesting* task_queue,
                                 const FakeNetworkPipe::Config& config,
                                 Call* send_call,
                                 std::unique_ptr<Demuxer> demuxer)
    : send_call_(send_call),
      clock_(Clock::GetRealTimeClock()),
      task_queue_(task_queue),
      fake_network_(rtc::MakeUnique<FakeNetworkPipe>(clock_, config,
                                                      std::move(demuxer))) {
  Start();
}

DirectTransport::DirectTransport(SingleThreadedTaskQueueForTesting* task_queue,
                                 std::unique_ptr<FakeNetworkPipe> pipe,
                                 Call* send_call)
    : send_call_(send_call),
      clock_(Clock::GetRealTimeClock()),
      task_queue_(task_queue),
      fake_network_(std::move(pipe)) {
  Start();
}

DirectTransport::~DirectTransport() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  // Constructor updates |next_scheduled_task_|, so it's guaranteed to
  // be initialized.
  task_queue_->CancelTask(next_scheduled_task_);
}

void DirectTransport::SetConfig(const FakeNetworkPipe::Config& config) {
  fake_network_->SetConfig(config);
}

void DirectTransport::StopSending() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  task_queue_->CancelTask(next_scheduled_task_);
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  fake_network_->SetReceiver(receiver);
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id,
                                clock_->TimeInMilliseconds());
    send_call_->OnSentPacket(sent_packet);
  }
  fake_network_->SendPacket(data, length);
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  fake_network_->SendPacket(data, length);
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
  SendPackets();
}

void DirectTransport::SendPackets() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  fake_network_->Process();

  int64_t delay_ms = fake_network_->TimeUntilNextProcess();
  next_scheduled_task_ = task_queue_->PostDelayedTask([this]() {
    SendPackets();
  }, delay_ms);
}
}  // namespace test
}  // namespace webrtc
