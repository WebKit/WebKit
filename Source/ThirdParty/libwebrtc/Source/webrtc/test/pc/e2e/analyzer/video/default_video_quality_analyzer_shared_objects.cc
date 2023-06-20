/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <string>

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

constexpr int kMicrosPerSecond = 1000000;

}  // namespace

std::string StreamCodecInfo::ToString() const {
  rtc::StringBuilder out;
  out << "{codec_name=" << codec_name << "; first_frame_id=" << first_frame_id
      << "; last_frame_id=" << last_frame_id
      << "; switched_on_at=" << webrtc::ToString(switched_on_at)
      << "; switched_from_at=" << webrtc::ToString(switched_from_at) << " }";
  return out.str();
}

std::ostream& operator<<(std::ostream& os, const StreamCodecInfo& state) {
  return os << state.ToString();
}

rtc::StringBuilder& operator<<(rtc::StringBuilder& sb,
                               const StreamCodecInfo& state) {
  return sb << state.ToString();
}

bool operator==(const StreamCodecInfo& a, const StreamCodecInfo& b) {
  return a.codec_name == b.codec_name && a.first_frame_id == b.first_frame_id &&
         a.last_frame_id == b.last_frame_id &&
         a.switched_on_at == b.switched_on_at &&
         a.switched_from_at == b.switched_from_at;
}

std::string ToString(FrameDropPhase phase) {
  switch (phase) {
    case FrameDropPhase::kBeforeEncoder:
      return "kBeforeEncoder";
    case FrameDropPhase::kByEncoder:
      return "kByEncoder";
    case FrameDropPhase::kTransport:
      return "kTransport";
    case FrameDropPhase::kByDecoder:
      return "kByDecoder";
    case FrameDropPhase::kAfterDecoder:
      return "kAfterDecoder";
    case FrameDropPhase::kLastValue:
      return "kLastValue";
  }
}

std::ostream& operator<<(std::ostream& os, FrameDropPhase phase) {
  return os << ToString(phase);
}
rtc::StringBuilder& operator<<(rtc::StringBuilder& sb, FrameDropPhase phase) {
  return sb << ToString(phase);
}

void SamplesRateCounter::AddEvent(Timestamp event_time) {
  if (event_first_time_.IsMinusInfinity()) {
    event_first_time_ = event_time;
  }
  event_last_time_ = event_time;
  events_count_++;
}

double SamplesRateCounter::GetEventsPerSecond() const {
  RTC_DCHECK(!IsEmpty());
  // Divide on us and multiply on kMicrosPerSecond to correctly process cases
  // where there were too small amount of events, so difference is less then 1
  // sec. We can use us here, because Timestamp has us resolution.
  return static_cast<double>(events_count_) /
         (event_last_time_ - event_first_time_).us() * kMicrosPerSecond;
}

StreamStats::StreamStats(Timestamp stream_started_time)
    : stream_started_time(stream_started_time) {
  for (int i = static_cast<int>(FrameDropPhase::kBeforeEncoder);
       i < static_cast<int>(FrameDropPhase::kLastValue); ++i) {
    dropped_by_phase.emplace(static_cast<FrameDropPhase>(i), 0);
  }
}

std::string StatsKey::ToString() const {
  rtc::StringBuilder out;
  out << stream_label << "_" << receiver;
  return out.str();
}

bool operator<(const StatsKey& a, const StatsKey& b) {
  if (a.stream_label != b.stream_label) {
    return a.stream_label < b.stream_label;
  }
  return a.receiver < b.receiver;
}

bool operator==(const StatsKey& a, const StatsKey& b) {
  return a.stream_label == b.stream_label && a.receiver == b.receiver;
}

VideoStreamsInfo::VideoStreamsInfo(
    std::map<std::string, std::string> stream_to_sender,
    std::map<std::string, std::set<std::string>> sender_to_streams,
    std::map<std::string, std::set<std::string>> stream_to_receivers)
    : stream_to_sender_(std::move(stream_to_sender)),
      sender_to_streams_(std::move(sender_to_streams)),
      stream_to_receivers_(std::move(stream_to_receivers)) {}

std::set<StatsKey> VideoStreamsInfo::GetStatsKeys() const {
  std::set<StatsKey> out;
  for (const std::string& stream_label : GetStreams()) {
    for (const std::string& receiver : GetReceivers(stream_label)) {
      out.insert(StatsKey(stream_label, receiver));
    }
  }
  return out;
}

std::set<std::string> VideoStreamsInfo::GetStreams() const {
  std::set<std::string> out;
  std::transform(stream_to_sender_.begin(), stream_to_sender_.end(),
                 std::inserter(out, out.end()),
                 [](auto map_entry) { return map_entry.first; });
  return out;
}

std::set<std::string> VideoStreamsInfo::GetStreams(
    absl::string_view sender_name) const {
  auto it = sender_to_streams_.find(std::string(sender_name));
  if (it == sender_to_streams_.end()) {
    return {};
  }
  return it->second;
}

absl::optional<std::string> VideoStreamsInfo::GetSender(
    absl::string_view stream_label) const {
  auto it = stream_to_sender_.find(std::string(stream_label));
  if (it == stream_to_sender_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

std::set<std::string> VideoStreamsInfo::GetReceivers(
    absl::string_view stream_label) const {
  auto it = stream_to_receivers_.find(std::string(stream_label));
  if (it == stream_to_receivers_.end()) {
    return {};
  }
  return it->second;
}

}  // namespace webrtc
