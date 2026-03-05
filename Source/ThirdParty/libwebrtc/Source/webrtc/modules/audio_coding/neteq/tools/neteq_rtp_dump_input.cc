/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_rtp_dump_input.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {
namespace test {
namespace {

// An adapter class to dress up a PacketSource object as a NetEqInput.
class NetEqRtpDumpInput : public NetEqInput {
 public:
  NetEqRtpDumpInput(absl::string_view file_name,
                    const std::map<int, RTPExtensionType>& hdr_ext_map,
                    std::optional<uint32_t> ssrc_filter)
      : source_(RtpFileSource::Create(file_name, ssrc_filter)) {
    for (const auto& ext_pair : hdr_ext_map) {
      source_->RegisterRtpHeaderExtension(ext_pair.second, ext_pair.first);
    }
    packet_ = source_->NextPacket();
  }

  std::optional<int64_t> NextOutputEventTime() const override {
    return next_output_event_ms_;
  }

  std::optional<SetMinimumDelayInfo> NextSetMinimumDelayInfo() const override {
    return std::nullopt;
  }

  void AdvanceOutputEvent() override {
    if (next_output_event_ms_) {
      *next_output_event_ms_ += kOutputPeriodMs;
    }
    if (!NextPacketTime()) {
      next_output_event_ms_ = std::nullopt;
    }
  }

  void AdvanceSetMinimumDelay() override {}

  std::optional<int64_t> NextPacketTime() const override {
    return packet_ ? std::optional(packet_->arrival_time().ms()) : std::nullopt;
  }

  std::unique_ptr<RtpPacketReceived> PopPacket() override {
    if (!packet_) {
      return nullptr;
    }
    return std::exchange(packet_, source_->NextPacket());
  }

  const RtpPacketReceived* NextPacket() const override { return packet_.get(); }

  bool ended() const override { return !next_output_event_ms_; }

 private:
  std::optional<int64_t> next_output_event_ms_ = 0;
  static constexpr int64_t kOutputPeriodMs = 10;

  std::unique_ptr<RtpFileSource> source_;
  std::unique_ptr<RtpPacketReceived> packet_;
};

}  // namespace

std::unique_ptr<NetEqInput> CreateNetEqRtpDumpInput(
    absl::string_view file_name,
    const std::map<int, RTPExtensionType>& hdr_ext_map,
    std::optional<uint32_t> ssrc_filter) {
  return std::make_unique<NetEqRtpDumpInput>(file_name, hdr_ext_map,
                                             ssrc_filter);
}

}  // namespace test
}  // namespace webrtc
