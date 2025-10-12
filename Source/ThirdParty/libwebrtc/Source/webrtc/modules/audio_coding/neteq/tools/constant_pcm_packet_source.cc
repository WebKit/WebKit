/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/constant_pcm_packet_source.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "api/units/timestamp.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

ConstantPcmPacketSource::ConstantPcmPacketSource(size_t payload_len_samples,
                                                 int16_t sample_value,
                                                 int sample_rate_hz,
                                                 int payload_type)
    : payload_len_samples_(payload_len_samples),
      packet_len_bytes_(2 * payload_len_samples_ + kHeaderLenBytes),
      samples_per_ms_(sample_rate_hz / 1000),
      next_arrival_time_ms_(0.0),
      payload_type_(payload_type),
      seq_number_(0),
      timestamp_(0),
      payload_ssrc_(0xABCD1234) {
  size_t encoded_len = WebRtcPcm16b_Encode(&sample_value, 1, encoded_sample_);
  RTC_CHECK_EQ(2U, encoded_len);
}

std::unique_ptr<RtpPacketReceived> ConstantPcmPacketSource::NextPacket() {
  RTC_CHECK_GT(packet_len_bytes_, kHeaderLenBytes);
  auto rtp_packet = std::make_unique<RtpPacketReceived>();
  rtp_packet->SetPayloadType(payload_type_);
  rtp_packet->SetSequenceNumber(seq_number_);
  rtp_packet->SetTimestamp(timestamp_);
  rtp_packet->SetSsrc(payload_ssrc_);
  ++seq_number_;
  timestamp_ += static_cast<uint32_t>(payload_len_samples_);

  uint8_t* packet_memory =
      rtp_packet->AllocatePayload(2 * payload_len_samples_);
  // Fill the payload part of the packet memory with the pre-encoded value.
  for (size_t i = 0; i < 2 * payload_len_samples_; ++i) {
    packet_memory[i] = encoded_sample_[i % 2];
  }

  rtp_packet->set_arrival_time(Timestamp::Millis(next_arrival_time_ms_));
  next_arrival_time_ms_ += payload_len_samples_ / samples_per_ms_;

  return rtp_packet;
}

}  // namespace test
}  // namespace webrtc
