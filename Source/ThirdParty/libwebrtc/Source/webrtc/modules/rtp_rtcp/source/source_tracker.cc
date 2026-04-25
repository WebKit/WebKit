/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/source_tracker.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

SourceTracker::SourceTracker(Clock* clock)
    : SourceTracker(clock, absl::AnyInvocable<void(bool, bool)>()) {}

SourceTracker::SourceTracker(
    Clock* clock,
    absl::AnyInvocable<void(bool, bool)> on_source_changed)
    : clock_(clock), on_source_changed_(std::move(on_source_changed)) {
  RTC_DCHECK(clock_);
}

void SourceTracker::OnFrameDelivered(const RtpPacketInfos& packet_infos,
                                     Timestamp delivery_time) {
  TRACE_EVENT0("webrtc", "SourceTracker::OnFrameDelivered");
  if (packet_infos.empty()) {
    return;
  }
  if (delivery_time.IsInfinite()) {
    delivery_time = clock_->CurrentTime();
  }

  std::optional<uint32_t> prev_ssrc = last_received_ssrc_;
  std::vector<uint32_t> prev_csrcs = last_received_csrcs_;
  last_received_csrcs_.clear();
  for (const RtpPacketInfo& packet_info : packet_infos) {
    for (uint32_t csrc : packet_info.csrcs()) {
      SourceKey key(RtpSourceType::CSRC, csrc);
      SourceEntry& entry = UpdateEntry(key);
      last_received_csrcs_.push_back(csrc);

      entry.timestamp = delivery_time;
      entry.audio_level = packet_info.audio_level();
      entry.absolute_capture_time = packet_info.absolute_capture_time();
      entry.local_capture_clock_offset =
          packet_info.local_capture_clock_offset();
      entry.rtp_timestamp = packet_info.rtp_timestamp();
    }

    SourceKey key(RtpSourceType::SSRC, packet_info.ssrc());
    SourceEntry& entry = UpdateEntry(key);
    last_received_ssrc_ = packet_info.ssrc();

    entry.timestamp = delivery_time;
    entry.audio_level = packet_info.audio_level();
    entry.absolute_capture_time = packet_info.absolute_capture_time();
    entry.local_capture_clock_offset = packet_info.local_capture_clock_offset();
    entry.rtp_timestamp = packet_info.rtp_timestamp();
  }

  PruneEntries(delivery_time);

  bool fire_ssrc_change = last_received_ssrc_ != prev_ssrc;
  bool fire_csrc_change = last_received_csrcs_ != prev_csrcs;
  if ((fire_ssrc_change || fire_csrc_change) && on_source_changed_) {
    ShouldFireOnSoourceChangedCallback(fire_ssrc_change, fire_csrc_change);
  }
}

void SourceTracker::SetOnSourceChangedCallback(
    absl::AnyInvocable<void(bool, bool)> on_source_changed) {
  on_source_changed_ = std::move(on_source_changed);
  // Fire on set if a frame was received before the caller had a chance to add
  // its callback.
  if (last_received_ssrc_ || !last_received_csrcs_.empty()) {
    ShouldFireOnSoourceChangedCallback(last_received_ssrc_.has_value(),
                                       !last_received_csrcs_.empty());
  }
}

void SourceTracker::ShouldFireOnSoourceChangedCallback(bool ssrc_changed,
                                                       bool csrc_changed) {
  TaskQueueBase::Current()->PostTask(
      SafeTask(safety_.flag(), [this, ssrc_changed, csrc_changed] {
        if (on_source_changed_) {
          on_source_changed_(ssrc_changed, csrc_changed);
        }
      }));
}

std::vector<RtpSource> SourceTracker::GetSources() const {
  PruneEntries(clock_->CurrentTime());

  std::vector<RtpSource> sources;
  for (const auto& pair : list_) {
    const SourceKey& key = pair.first;
    const SourceEntry& entry = pair.second;

    sources.emplace_back(
        entry.timestamp, key.source, key.source_type, entry.rtp_timestamp,
        RtpSource::Extensions{
            .audio_level = entry.audio_level,
            .absolute_capture_time = entry.absolute_capture_time,
            .local_capture_clock_offset = entry.local_capture_clock_offset});
  }

  return sources;
}

SourceTracker::SourceEntry& SourceTracker::UpdateEntry(const SourceKey& key) {
  // We intentionally do |find() + emplace()|, instead of checking the return
  // value of `emplace()`, for performance reasons. It's much more likely for
  // the key to already exist than for it not to.
  auto map_it = map_.find(key);
  if (map_it == map_.end()) {
    // Insert a new entry at the front of the list.
    list_.emplace_front(key, SourceEntry());
    map_.emplace(key, list_.begin());
  } else if (map_it->second != list_.begin()) {
    // Move the old entry to the front of the list.
    list_.splice(list_.begin(), list_, map_it->second);
  }

  return list_.front().second;
}

void SourceTracker::PruneEntries(Timestamp now) const {
  if (now < Timestamp::Zero() + kTimeout) {
    return;
  }
  Timestamp prune = now - kTimeout;
  while (!list_.empty() && list_.back().second.timestamp < prune) {
    map_.erase(list_.back().first);
    list_.pop_back();
  }
}

}  // namespace webrtc
