/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/rtp_headers.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_neteq_set_minimum_delay.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {
namespace test {
namespace {

class NetEqEventLogInput : public NetEqInput {
 public:
  NetEqEventLogInput(const std::vector<LoggedRtpPacketIncoming>& packet_stream,
                     const std::vector<LoggedAudioPlayoutEvent>& output_events,
                     const std::vector<LoggedNetEqSetMinimumDelayEvent>&
                         neteq_set_minimum_delay_events,
                     std::optional<int64_t> end_time_ms)
      : packet_stream_(packet_stream),
        packet_stream_it_(packet_stream_.begin()),
        output_events_(output_events),
        output_events_it_(output_events_.begin()),
        neteq_set_minimum_delay_events_(neteq_set_minimum_delay_events),
        neteq_set_minimum_delay_events_it_(
            neteq_set_minimum_delay_events_.begin()),
        end_time_ms_(end_time_ms) {
    next_packet_ = CreateNextPacket();

    // Ignore all output events before the first packet.
    while (output_events_it_ != output_events_.end() &&
           output_events_it_->log_time_ms() <
               packet_stream_it_->log_time_ms()) {
      ++output_events_it_;
    }
  }

  std::optional<int64_t> NextPacketTime() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return std::nullopt;
    }
    if (end_time_ms_ && packet_stream_it_->rtp.log_time_ms() > *end_time_ms_) {
      return std::nullopt;
    }
    return packet_stream_it_->rtp.log_time_ms();
  }

  std::optional<int64_t> NextOutputEventTime() const override {
    if (output_events_it_ == output_events_.end()) {
      return std::nullopt;
    }
    if (end_time_ms_ && output_events_it_->log_time_ms() > *end_time_ms_) {
      return std::nullopt;
    }
    return output_events_it_->log_time_ms();
  }

  std::optional<SetMinimumDelayInfo> NextSetMinimumDelayInfo() const override {
    if (neteq_set_minimum_delay_events_it_ ==
        neteq_set_minimum_delay_events_.end()) {
      return std::nullopt;
    }
    if (end_time_ms_ &&
        neteq_set_minimum_delay_events_it_->log_time_ms() > *end_time_ms_) {
      return std::nullopt;
    }
    return SetMinimumDelayInfo(
        neteq_set_minimum_delay_events_it_->log_time_ms(),
        neteq_set_minimum_delay_events_it_->minimum_delay_ms);
  }

  std::unique_ptr<RtpPacketReceived> PopPacket() override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return nullptr;
    }
    ++packet_stream_it_;
    return std::exchange(next_packet_, CreateNextPacket());
  }

  void AdvanceOutputEvent() override {
    if (output_events_it_ != output_events_.end()) {
      ++output_events_it_;
    }
  }

  void AdvanceSetMinimumDelay() override {
    if (neteq_set_minimum_delay_events_it_ !=
        neteq_set_minimum_delay_events_.end()) {
      ++neteq_set_minimum_delay_events_it_;
    }
  }

  bool ended() const override { return !NextEventTime(); }

  const RtpPacketReceived* NextPacket() const override {
    return next_packet_.get();
  }

 private:
  std::unique_ptr<RtpPacketReceived> CreateNextPacket() {
    if (packet_stream_it_ == packet_stream_.end()) {
      return nullptr;
    }
    const LoggedRtpPacket& logged = packet_stream_it_->rtp;
    auto packet_data = std::make_unique<RtpPacketReceived>();
    packet_data->SetPayloadType(logged.header.payloadType);
    packet_data->SetMarker(logged.header.markerBit);
    packet_data->SetSequenceNumber(logged.header.sequenceNumber);
    packet_data->SetTimestamp(logged.header.timestamp);
    packet_data->SetSsrc(logged.header.ssrc);
    packet_data->SetCsrcs(
        MakeArrayView(logged.header.arrOfCSRCs, logged.header.numCSRCs));
    packet_data->set_arrival_time(logged.log_time());

    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    size_t payload_size = logged.total_length - logged.header_length;
    std::fill_n(packet_data->AllocatePayload(payload_size), payload_size, 0);

    return packet_data;
  }

  const std::vector<LoggedRtpPacketIncoming> packet_stream_;
  std::vector<LoggedRtpPacketIncoming>::const_iterator packet_stream_it_;
  std::unique_ptr<RtpPacketReceived> next_packet_;
  const std::vector<LoggedAudioPlayoutEvent> output_events_;
  std::vector<LoggedAudioPlayoutEvent>::const_iterator output_events_it_;
  const std::vector<LoggedNetEqSetMinimumDelayEvent>
      neteq_set_minimum_delay_events_;
  std::vector<LoggedNetEqSetMinimumDelayEvent>::const_iterator
      neteq_set_minimum_delay_events_it_;
  const std::optional<int64_t> end_time_ms_;
};

}  // namespace

std::unique_ptr<NetEqInput> CreateNetEqEventLogInput(
    const ParsedRtcEventLog& parsed_log,
    std::optional<uint32_t> ssrc) {
  if (parsed_log.incoming_audio_ssrcs().empty()) {
    return nullptr;
  }
  // Pick the first SSRC if none was provided.
  ssrc = ssrc.value_or(*parsed_log.incoming_audio_ssrcs().begin());
  auto streams = parsed_log.incoming_rtp_packets_by_ssrc();
  auto stream =
      std::find_if(streams.begin(), streams.end(),
                   [ssrc](auto stream) { return stream.ssrc == ssrc; });
  if (stream == streams.end()) {
    return nullptr;
  }
  auto output_events_it = parsed_log.audio_playout_events().find(*ssrc);
  if (output_events_it == parsed_log.audio_playout_events().end()) {
    return nullptr;
  }
  std::vector<LoggedNetEqSetMinimumDelayEvent> neteq_set_minimum_delay_events;
  auto neteq_set_minimum_delay_events_it =
      parsed_log.neteq_set_minimum_delay_events().find(*ssrc);
  if (neteq_set_minimum_delay_events_it !=
      parsed_log.neteq_set_minimum_delay_events().end()) {
    neteq_set_minimum_delay_events = neteq_set_minimum_delay_events_it->second;
  }
  int64_t end_time_ms = parsed_log.first_log_segment().stop_time_ms();
  return std::make_unique<NetEqEventLogInput>(
      stream->incoming_packets, output_events_it->second,
      neteq_set_minimum_delay_events, end_time_ms);
}

}  // namespace test
}  // namespace webrtc
