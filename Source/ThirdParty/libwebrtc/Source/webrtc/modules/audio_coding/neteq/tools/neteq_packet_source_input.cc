/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_packet_source_input.h"

#include <algorithm>
#include <limits>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

NetEqPacketSourceInput::NetEqPacketSourceInput() : next_output_event_ms_(0) {}

absl::optional<int64_t> NetEqPacketSourceInput::NextPacketTime() const {
  return packet_
             ? absl::optional<int64_t>(static_cast<int64_t>(packet_->time_ms()))
             : absl::nullopt;
}

absl::optional<RTPHeader> NetEqPacketSourceInput::NextHeader() const {
  return packet_ ? absl::optional<RTPHeader>(packet_->header()) : absl::nullopt;
}

void NetEqPacketSourceInput::LoadNextPacket() {
  packet_ = source()->NextPacket();
}

std::unique_ptr<NetEqInput::PacketData> NetEqPacketSourceInput::PopPacket() {
  if (!packet_) {
    return std::unique_ptr<PacketData>();
  }
  std::unique_ptr<PacketData> packet_data(new PacketData);
  packet_data->header = packet_->header();
  if (packet_->payload_length_bytes() == 0 &&
      packet_->virtual_payload_length_bytes() > 0) {
    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_->virtual_payload_length_bytes());
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);
  } else {
    packet_data->payload.SetData(packet_->payload(),
                                 packet_->payload_length_bytes());
  }
  packet_data->time_ms = packet_->time_ms();

  LoadNextPacket();

  return packet_data;
}

NetEqRtpDumpInput::NetEqRtpDumpInput(absl::string_view file_name,
                                     const RtpHeaderExtensionMap& hdr_ext_map,
                                     absl::optional<uint32_t> ssrc_filter)
    : source_(RtpFileSource::Create(file_name, ssrc_filter)) {
  for (const auto& ext_pair : hdr_ext_map) {
    source_->RegisterRtpHeaderExtension(ext_pair.second, ext_pair.first);
  }
  LoadNextPacket();
}

absl::optional<int64_t> NetEqRtpDumpInput::NextOutputEventTime() const {
  return next_output_event_ms_;
}

void NetEqRtpDumpInput::AdvanceOutputEvent() {
  if (next_output_event_ms_) {
    *next_output_event_ms_ += kOutputPeriodMs;
  }
  if (!NextPacketTime()) {
    next_output_event_ms_ = absl::nullopt;
  }
}

PacketSource* NetEqRtpDumpInput::source() {
  return source_.get();
}

}  // namespace test
}  // namespace webrtc
