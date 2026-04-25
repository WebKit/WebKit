/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_input.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {

std::string NetEqInput::ToString(const RtpPacketReceived& packet) {
  StringBuilder ss;
  ss << "{"
        "time_ms: "
     << packet.arrival_time().ms()
     << ", "
        "header: {"
        "pt: "
     << static_cast<int>(packet.PayloadType())
     << ", "
        "sn: "
     << packet.SequenceNumber()
     << ", "
        "ts: "
     << packet.Timestamp()
     << ", "
        "ssrc: "
     << packet.Ssrc()
     << "}, "
        "payload bytes: "
     << packet.payload_size() << "}";
  return ss.Release();
}

TimeLimitedNetEqInput::TimeLimitedNetEqInput(std::unique_ptr<NetEqInput> input,
                                             int64_t duration_ms)
    : input_(std::move(input)),
      start_time_ms_(input_->NextEventTime()),
      duration_ms_(duration_ms) {}

TimeLimitedNetEqInput::~TimeLimitedNetEqInput() = default;

std::optional<int64_t> TimeLimitedNetEqInput::NextPacketTime() const {
  return ended_ ? std::nullopt : input_->NextPacketTime();
}

std::optional<int64_t> TimeLimitedNetEqInput::NextOutputEventTime() const {
  return ended_ ? std::nullopt : input_->NextOutputEventTime();
}

std::optional<NetEqInput::SetMinimumDelayInfo>
TimeLimitedNetEqInput::NextSetMinimumDelayInfo() const {
  return ended_ ? std::nullopt : input_->NextSetMinimumDelayInfo();
}

std::unique_ptr<RtpPacketReceived> TimeLimitedNetEqInput::PopPacket() {
  if (ended_) {
    return nullptr;
  }
  auto packet = input_->PopPacket();
  MaybeSetEnded();
  return packet;
}

void TimeLimitedNetEqInput::AdvanceOutputEvent() {
  if (!ended_) {
    input_->AdvanceOutputEvent();
    MaybeSetEnded();
  }
}

void TimeLimitedNetEqInput::AdvanceSetMinimumDelay() {
  if (!ended_) {
    input_->AdvanceSetMinimumDelay();
    MaybeSetEnded();
  }
}

bool TimeLimitedNetEqInput::ended() const {
  return ended_ || input_->ended();
}

const RtpPacketReceived* TimeLimitedNetEqInput::NextPacket() const {
  return ended_ ? nullptr : input_->NextPacket();
}

void TimeLimitedNetEqInput::MaybeSetEnded() {
  if (NextEventTime() && start_time_ms_ &&
      *NextEventTime() - *start_time_ms_ > duration_ms_) {
    ended_ = true;
  }
}

}  // namespace test
}  // namespace webrtc
