/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/rtc_event_log_source.h"

#include <string.h>
#include <iostream>
#include <limits>
#include <utility>

#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

namespace {
bool ShouldSkipStream(ParsedRtcEventLogNew::MediaType media_type,
                      uint32_t ssrc,
                      absl::optional<uint32_t> ssrc_filter) {
  if (media_type != ParsedRtcEventLogNew::MediaType::AUDIO)
    return true;
  if (ssrc_filter.has_value() && ssrc != *ssrc_filter)
    return true;
  return false;
}
}  // namespace

RtcEventLogSource* RtcEventLogSource::Create(
    const std::string& file_name,
    absl::optional<uint32_t> ssrc_filter) {
  RtcEventLogSource* source = new RtcEventLogSource();
  RTC_CHECK(source->OpenFile(file_name, ssrc_filter));
  return source;
}

RtcEventLogSource::~RtcEventLogSource() {}

std::unique_ptr<Packet> RtcEventLogSource::NextPacket() {
  if (rtp_packet_index_ >= rtp_packets_.size())
    return nullptr;

  std::unique_ptr<Packet> packet = std::move(rtp_packets_[rtp_packet_index_++]);
  return packet;
}

int64_t RtcEventLogSource::NextAudioOutputEventMs() {
  if (audio_output_index_ >= audio_outputs_.size())
    return std::numeric_limits<int64_t>::max();

  int64_t output_time_ms = audio_outputs_[audio_output_index_++];
  return output_time_ms;
}

RtcEventLogSource::RtcEventLogSource() : PacketSource() {}

bool RtcEventLogSource::OpenFile(const std::string& file_name,
                                 absl::optional<uint32_t> ssrc_filter) {
  ParsedRtcEventLogNew parsed_log;
  if (!parsed_log.ParseFile(file_name))
    return false;

  const auto first_log_end_time_us =
      parsed_log.stop_log_events().empty()
          ? std::numeric_limits<int64_t>::max()
          : parsed_log.stop_log_events().front().log_time_us();

  auto handle_rtp_packet =
      [this,
       first_log_end_time_us](const webrtc::LoggedRtpPacketIncoming& incoming) {
        if (!filter_.test(incoming.rtp.header.payloadType) &&
            incoming.log_time_us() < first_log_end_time_us) {
          rtp_packets_.emplace_back(absl::make_unique<Packet>(
              incoming.rtp.header, incoming.rtp.total_length,
              incoming.rtp.total_length - incoming.rtp.header_length,
              static_cast<double>(incoming.log_time_ms())));
        }
      };

  auto handle_audio_playout =
      [this, first_log_end_time_us](
          const webrtc::LoggedAudioPlayoutEvent& audio_playout) {
        if (audio_playout.log_time_us() < first_log_end_time_us) {
          audio_outputs_.emplace_back(audio_playout.log_time_ms());
        }
      };

  // This wouldn't be needed if we knew that there was at most one audio stream.
  webrtc::RtcEventProcessor event_processor;
  for (const auto& rtp_packets : parsed_log.incoming_rtp_packets_by_ssrc()) {
    ParsedRtcEventLogNew::MediaType media_type =
        parsed_log.GetMediaType(rtp_packets.ssrc, webrtc::kIncomingPacket);
    if (ShouldSkipStream(media_type, rtp_packets.ssrc, ssrc_filter)) {
      continue;
    }
    auto rtp_view = absl::make_unique<
        webrtc::ProcessableEventList<webrtc::LoggedRtpPacketIncoming>>(
        rtp_packets.incoming_packets.begin(),
        rtp_packets.incoming_packets.end(), handle_rtp_packet);
    event_processor.AddEvents(std::move(rtp_view));
  }

  for (const auto& audio_playouts : parsed_log.audio_playout_events()) {
    if (ssrc_filter.has_value() && audio_playouts.first != *ssrc_filter)
      continue;
    auto audio_view = absl::make_unique<
        webrtc::ProcessableEventList<webrtc::LoggedAudioPlayoutEvent>>(
        audio_playouts.second.begin(), audio_playouts.second.end(),
        handle_audio_playout);
    event_processor.AddEvents(std::move(audio_view));
  }

  // Fills in rtp_packets_ and audio_outputs_.
  event_processor.ProcessEventsInOrder();

  return true;
}

}  // namespace test
}  // namespace webrtc
