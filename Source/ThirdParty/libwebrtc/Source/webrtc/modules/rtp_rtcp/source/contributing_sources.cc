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
                                 rtc::ArrayView<const uint32_t> csrcs) {
  for (uint32_t csrc : csrcs) {
    last_seen_ms_[csrc] = now_ms;
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
  for (auto& record : last_seen_ms_) {
    if (record.second >= now_ms - kHistoryMs) {
      sources.emplace_back(record.second, record.first, RtpSourceType::CSRC);
    }
  }

  return sources;
}

// Delete stale entries.
void ContributingSources::DeleteOldEntries(int64_t now_ms) {
  for (auto it = last_seen_ms_.begin(); it != last_seen_ms_.end();) {
    if (it->second >= now_ms - kHistoryMs) {
      // Still relevant.
      ++it;
    } else {
      it = last_seen_ms_.erase(it);
    }
  }
  next_pruning_ms_ = now_ms + kPruningIntervalMs;
}

}  // namespace webrtc
