/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/contributing_sources.h"

namespace webrtc {

namespace {

// Allow some stale records to accumulate before cleaning.
constexpr int64_t kPruningIntervalMs = 15 * rtc::kNumMillisecsPerSec;

}  // namespace

constexpr int64_t ContributingSources::kHistoryMs;

ContributingSources::ContributingSources() = default;
ContributingSources::~ContributingSources() = default;

void ContributingSources::Update(int64_t now_ms,
                                 rtc::ArrayView<const uint32_t> csrcs,
                                 absl::optional<uint8_t> audio_level) {
  Entry entry = { now_ms, audio_level };
  for (uint32_t csrc : csrcs) {
    active_csrcs_[csrc] = entry;
  }
  if (!next_pruning_ms_) {
    next_pruning_ms_ = now_ms + kPruningIntervalMs;
  } else if (now_ms > next_pruning_ms_) {
    // To prevent unlimited growth, prune it every 15 seconds.
    DeleteOldEntries(now_ms);
  }
}

// Return contributing sources seen the last 10 s.
// TODO(nisse): It would be more efficient to delete any stale entries while
// iterating over the mapping, but then we'd have to make the method
// non-const.
std::vector<RtpSource> ContributingSources::GetSources(int64_t now_ms) const {
  std::vector<RtpSource> sources;
  for (auto& record : active_csrcs_) {
    if (record.second.last_seen_ms >= now_ms - kHistoryMs) {
      if (record.second.audio_level.has_value()) {
        sources.emplace_back(record.second.last_seen_ms, record.first,
                             RtpSourceType::CSRC,
                             *record.second.audio_level);
      } else {
        sources.emplace_back(record.second.last_seen_ms, record.first,
                             RtpSourceType::CSRC);
      }
    }
  }

  return sources;
}

// Delete stale entries.
void ContributingSources::DeleteOldEntries(int64_t now_ms) {
  for (auto it = active_csrcs_.begin(); it != active_csrcs_.end();) {
    if (it->second.last_seen_ms >= now_ms - kHistoryMs) {
      // Still relevant.
      ++it;
    } else {
      it = active_csrcs_.erase(it);
    }
  }
  next_pruning_ms_ = now_ms + kPruningIntervalMs;
}

ContributingSources::Entry::Entry() = default;
ContributingSources::Entry::Entry(int64_t timestamp_ms,
                                  absl::optional<uint8_t> audio_level_arg)
    : last_seen_ms(timestamp_ms), audio_level(audio_level_arg) {}

}  // namespace webrtc
