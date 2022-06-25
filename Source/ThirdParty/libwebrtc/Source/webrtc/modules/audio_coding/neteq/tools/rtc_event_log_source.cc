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
#include <memory>
#include <set>
#include <utility>

#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

namespace {
bool ShouldSkipStream(ParsedRtcEventLog::MediaType media_type,
                      uint32_t ssrc,
                      absl::optional<uint32_t> ssrc_filter) {
  if (media_type != ParsedRtcEventLog::MediaType::AUDIO)
    return true;
  if (ssrc_filter.has_value() && ssrc != *ssrc_filter)
    return true;
  return false;
}
}  // namespace

std::unique_ptr<RtcEventLogSource> RtcEventLogSource::CreateFromFile(
    const std::string& file_name,
    absl::optional<uint32_t> ssrc_filter) {
  auto source = std::unique_ptr<RtcEventLogSource>(new RtcEventLogSource());
  ParsedRtcEventLog parsed_log;
  auto status = parsed_log.ParseFile(file_name);
  if (!status.ok()) {
    std::cerr << "Failed to parse event log: " << status.message() << std::endl;
    std::cerr << "Skipping log." << std::endl;
    return nullptr;
  }
  if (!source->Initialize(parsed_log, ssrc_filter)) {
    std::cerr << "Failed to initialize source from event log, skipping."
              << std::endl;
    return nullptr;
  }
  return source;
}

std::unique_ptr<RtcEventLogSource> RtcEventLogSource::CreateFromString(
    const std::string& file_contents,
    absl::optional<uint32_t> ssrc_filter) {
  auto source = std::unique_ptr<RtcEventLogSource>(new RtcEventLogSource());
  ParsedRtcEventLog parsed_log;
  auto status = parsed_log.ParseString(file_contents);
  if (!status.ok()) {
    std::cerr << "Failed to parse event log: " << status.message() << std::endl;
    std::cerr << "Skipping log." << std::endl;
    return nullptr;
  }
  if (!source->Initialize(parsed_log, ssrc_filter)) {
    std::cerr << "Failed to initialize source from event log, skipping."
              << std::endl;
    return nullptr;
  }
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

bool RtcEventLogSource::Initialize(const ParsedRtcEventLog& parsed_log,
                                   absl::optional<uint32_t> ssrc_filter) {
  const auto first_log_end_time_us =
      parsed_log.stop_log_events().empty()
          ? std::numeric_limits<int64_t>::max()
          : parsed_log.stop_log_events().front().log_time_us();

  std::set<uint32_t> packet_ssrcs;
  auto handle_rtp_packet =
      [this, first_log_end_time_us,
       &packet_ssrcs](const webrtc::LoggedRtpPacketIncoming& incoming) {
        if (!filter_.test(incoming.rtp.header.payloadType) &&
            incoming.log_time_us() < first_log_end_time_us) {
          rtp_packets_.emplace_back(std::make_unique<Packet>(
              incoming.rtp.header, incoming.rtp.total_length,
              incoming.rtp.total_length - incoming.rtp.header_length,
              static_cast<double>(incoming.log_time_ms())));
          packet_ssrcs.insert(rtp_packets_.back()->header().ssrc);
        }
      };

  std::set<uint32_t> ignored_ssrcs;
  auto handle_audio_playout =
      [this, first_log_end_time_us, &packet_ssrcs,
       &ignored_ssrcs](const webrtc::LoggedAudioPlayoutEvent& audio_playout) {
        if (audio_playout.log_time_us() < first_log_end_time_us) {
          if (packet_ssrcs.count(audio_playout.ssrc) > 0) {
            audio_outputs_.emplace_back(audio_playout.log_time_ms());
          } else {
            ignored_ssrcs.insert(audio_playout.ssrc);
          }
        }
      };

  // This wouldn't be needed if we knew that there was at most one audio stream.
  webrtc::RtcEventProcessor event_processor;
  for (const auto& rtp_packets : parsed_log.incoming_rtp_packets_by_ssrc()) {
    ParsedRtcEventLog::MediaType media_type =
        parsed_log.GetMediaType(rtp_packets.ssrc, webrtc::kIncomingPacket);
    if (ShouldSkipStream(media_type, rtp_packets.ssrc, ssrc_filter)) {
      continue;
    }
    event_processor.AddEvents(rtp_packets.incoming_packets, handle_rtp_packet);
    // If no SSRC filter has been set, use the first SSRC only. The simulator
    // does not work properly with interleaved packets from multiple SSRCs.
    if (!ssrc_filter.has_value()) {
      ssrc_filter = rtp_packets.ssrc;
    }
  }

  for (const auto& audio_playouts : parsed_log.audio_playout_events()) {
    if (ssrc_filter.has_value() && audio_playouts.first != *ssrc_filter)
      continue;
    event_processor.AddEvents(audio_playouts.second, handle_audio_playout);
  }

  // Fills in rtp_packets_ and audio_outputs_.
  event_processor.ProcessEventsInOrder();

  for (const auto& ssrc : ignored_ssrcs) {
    std::cout << "Ignoring GetAudio events from SSRC 0x" << std::hex << ssrc
              << " because no packets were found with a matching SSRC."
              << std::endl;
  }

  return true;
}

}  // namespace test
}  // namespace webrtc
