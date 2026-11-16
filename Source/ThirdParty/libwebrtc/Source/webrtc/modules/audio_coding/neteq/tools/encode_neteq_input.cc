/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "api/audio_codecs/audio_encoder.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace test {

EncodeNetEqInput::EncodeNetEqInput(std::unique_ptr<Generator> generator,
                                   std::unique_ptr<AudioEncoder> encoder,
                                   int64_t input_duration_ms)
    : generator_(std::move(generator)),
      encoder_(std::move(encoder)),
      input_duration_ms_(input_duration_ms) {
  CreatePacket();
}

EncodeNetEqInput::~EncodeNetEqInput() = default;

std::optional<int64_t> EncodeNetEqInput::NextPacketTime() const {
  RTC_DCHECK(packet_data_);
  return packet_data_->arrival_time().ms();
}

std::optional<int64_t> EncodeNetEqInput::NextOutputEventTime() const {
  return next_output_event_ms_;
}

std::unique_ptr<RtpPacketReceived> EncodeNetEqInput::PopPacket() {
  RTC_DCHECK(packet_data_);
  // Grab the packet to return...
  std::unique_ptr<RtpPacketReceived> packet_to_return = std::move(packet_data_);
  // ... and line up the next packet for future use.
  CreatePacket();

  return packet_to_return;
}

void EncodeNetEqInput::AdvanceOutputEvent() {
  next_output_event_ms_ += kOutputPeriodMs;
}

bool EncodeNetEqInput::ended() const {
  return next_output_event_ms_ > input_duration_ms_;
}

const RtpPacketReceived* EncodeNetEqInput::NextPacket() const {
  RTC_DCHECK(packet_data_);
  return packet_data_.get();
}

void EncodeNetEqInput::CreatePacket() {
  // Create a new PacketData object.
  RTC_DCHECK(!packet_data_);
  packet_data_ = std::make_unique<RtpPacketReceived>();

  // Loop until we get a packet.
  AudioEncoder::EncodedInfo info;
  RTC_DCHECK(!info.send_even_if_empty);
  int num_blocks = 0;
  Buffer payload;
  while (payload.empty() && !info.send_even_if_empty) {
    const size_t num_samples = CheckedDivExact(
        static_cast<int>(encoder_->SampleRateHz() * kOutputPeriodMs), 1000);

    info = encoder_->Encode(rtp_timestamp_, generator_->Generate(num_samples),
                            &payload);

    rtp_timestamp_ +=
        dchecked_cast<uint32_t>(num_samples * encoder_->RtpTimestampRateHz() /
                                encoder_->SampleRateHz());
    ++num_blocks;
  }
  packet_data_->SetPayload(payload);
  packet_data_->SetTimestamp(info.encoded_timestamp);
  packet_data_->SetPayloadType(info.payload_type);
  packet_data_->SetSequenceNumber(sequence_number_++);
  packet_data_->set_arrival_time(Timestamp::Millis(next_packet_time_ms_));
  next_packet_time_ms_ += num_blocks * kOutputPeriodMs;
}

}  // namespace test
}  // namespace webrtc
