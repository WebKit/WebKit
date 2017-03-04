/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/direct_transport.h"

#include "webrtc/call/call.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

DirectTransport::DirectTransport(Call* send_call)
    : DirectTransport(FakeNetworkPipe::Config(), send_call) {}

DirectTransport::DirectTransport(const FakeNetworkPipe::Config& config,
                                 Call* send_call)
    : send_call_(send_call),
      packet_event_(false, false),
      thread_(NetworkProcess, this, "NetworkProcess"),
      clock_(Clock::GetRealTimeClock()),
      shutting_down_(false),
      fake_network_(clock_, config) {
  thread_.Start();
  if (send_call_) {
    send_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    send_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }
}

DirectTransport::~DirectTransport() { StopSending(); }

void DirectTransport::SetConfig(const FakeNetworkPipe::Config& config) {
  fake_network_.SetConfig(config);
}

void DirectTransport::StopSending() {
  {
    rtc::CritScope crit(&lock_);
    shutting_down_ = true;
  }

  packet_event_.Set();
  thread_.Stop();
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  fake_network_.SetReceiver(receiver);
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id,
                                clock_->TimeInMilliseconds());
    send_call_->OnSentPacket(sent_packet);
  }
  fake_network_.SendPacket(data, length);
  packet_event_.Set();
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  fake_network_.SendPacket(data, length);
  packet_event_.Set();
  return true;
}

int DirectTransport::GetAverageDelayMs() {
  return fake_network_.AverageDelay();
}

bool DirectTransport::NetworkProcess(void* transport) {
  return static_cast<DirectTransport*>(transport)->SendPackets();
}

bool DirectTransport::SendPackets() {
  fake_network_.Process();
  int64_t wait_time_ms = fake_network_.TimeUntilNextProcess();
  if (wait_time_ms > 0) {
    packet_event_.Wait(static_cast<int>(wait_time_ms));
  }
  rtc::CritScope crit(&lock_);
  return shutting_down_ ? false : true;
}
}  // namespace test
}  // namespace webrtc
