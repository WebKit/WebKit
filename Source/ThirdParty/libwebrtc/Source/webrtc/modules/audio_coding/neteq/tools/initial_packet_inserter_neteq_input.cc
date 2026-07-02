/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/initial_packet_inserter_neteq_input.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

InitialPacketInserterNetEqInput::InitialPacketInserterNetEqInput(
    std::unique_ptr<NetEqInput> source,
    int number_of_initial_packets,
    int sample_rate_hz)
    : source_(std::move(source)),
      packets_to_insert_(number_of_initial_packets),
      sample_rate_hz_(sample_rate_hz) {}

std::optional<int64_t> InitialPacketInserterNetEqInput::NextPacketTime() const {
  return source_->NextPacketTime();
}

std::optional<int64_t> InitialPacketInserterNetEqInput::NextOutputEventTime()
    const {
  return source_->NextOutputEventTime();
}

std::optional<NetEqInput::SetMinimumDelayInfo>
InitialPacketInserterNetEqInput::NextSetMinimumDelayInfo() const {
  return source_->NextSetMinimumDelayInfo();
}

std::unique_ptr<RtpPacketReceived>
InitialPacketInserterNetEqInput::PopPacket() {
  if (!first_packet_) {
    first_packet_ = source_->PopPacket();
    if (!first_packet_) {
      // The source has no packets, so we should not insert any dummy packets.
      packets_to_insert_ = 0;
    }
  }
  if (packets_to_insert_ > 0) {
    RTC_CHECK(first_packet_);
    auto dummy_packet = std::make_unique<RtpPacketReceived>(*first_packet_);
    dummy_packet->SetSequenceNumber(first_packet_->SequenceNumber() -
                                    packets_to_insert_);
    // This assumes 20ms per packet.
    dummy_packet->SetTimestamp(first_packet_->Timestamp() -
                               20 * sample_rate_hz_ * packets_to_insert_ /
                                   1000);
    packets_to_insert_--;
    return dummy_packet;
  }
  return source_->PopPacket();
}

void InitialPacketInserterNetEqInput::AdvanceSetMinimumDelay() {
  source_->AdvanceSetMinimumDelay();
}

void InitialPacketInserterNetEqInput::AdvanceOutputEvent() {
  source_->AdvanceOutputEvent();
}

bool InitialPacketInserterNetEqInput::ended() const {
  return source_->ended();
}

const RtpPacketReceived* InitialPacketInserterNetEqInput::NextPacket() const {
  return source_->NextPacket();
}

}  // namespace test
}  // namespace webrtc
