/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_stream_state.h"

#include <optional>
#include <set>
#include <unordered_map>

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/analyzer/video/dvqa/pausable_state.h"

namespace webrtc {
namespace {

template <typename T>
std::optional<T> MaybeGetValue(const std::unordered_map<size_t, T>& map,
                               size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace

StreamState::StreamState(size_t sender,
                         std::set<size_t> receivers,
                         Timestamp stream_started_time,
                         Clock* clock)
    : sender_(sender),
      stream_started_time_(stream_started_time),
      clock_(clock),
      receivers_(receivers),
      frame_ids_(std::move(receivers)) {
  frame_ids_.AddReader(kAliveFramesQueueIndex);
  RTC_CHECK_NE(sender_, kAliveFramesQueueIndex);
  for (size_t receiver : receivers_) {
    RTC_CHECK_NE(receiver, kAliveFramesQueueIndex);
    pausable_state_.emplace(receiver, PausableState(clock_));
  }
}

uint16_t StreamState::PopFront(size_t peer) {
  RTC_CHECK_NE(peer, kAliveFramesQueueIndex);
  std::optional<uint16_t> frame_id = frame_ids_.PopFront(peer);
  RTC_DCHECK(frame_id.has_value());

  // If alive's frame queue is longer than all others, than also pop frame from
  // it, because that frame is received by all receivers.
  size_t alive_size = frame_ids_.size(kAliveFramesQueueIndex);
  size_t other_size = GetLongestReceiverQueue();
  // Pops frame from alive queue if alive's queue is the longest one.
  if (alive_size > other_size) {
    std::optional<uint16_t> alive_frame_id =
        frame_ids_.PopFront(kAliveFramesQueueIndex);
    RTC_DCHECK(alive_frame_id.has_value());
    RTC_DCHECK_EQ(frame_id.value(), alive_frame_id.value());
  }

  return frame_id.value();
}

void StreamState::AddPeer(size_t peer) {
  RTC_CHECK_NE(peer, kAliveFramesQueueIndex);
  frame_ids_.AddReader(peer, kAliveFramesQueueIndex);
  receivers_.insert(peer);
  pausable_state_.emplace(peer, PausableState(clock_));
}

void StreamState::RemovePeer(size_t peer) {
  RTC_CHECK_NE(peer, kAliveFramesQueueIndex);
  frame_ids_.RemoveReader(peer);
  receivers_.erase(peer);
  pausable_state_.erase(peer);

  // If we removed the last receiver for the alive frames, we need to pop them
  // from the queue, because now they received by all receivers.
  size_t alive_size = frame_ids_.size(kAliveFramesQueueIndex);
  size_t other_size = GetLongestReceiverQueue();
  while (alive_size > other_size) {
    frame_ids_.PopFront(kAliveFramesQueueIndex);
    alive_size--;
  }
}

PausableState* StreamState::GetPausableState(size_t peer) {
  auto it = pausable_state_.find(peer);
  RTC_CHECK(it != pausable_state_.end())
      << "No pausable state for receiver " << peer;
  return &it->second;
}

void StreamState::SetLastRenderedFrameTime(size_t peer, Timestamp time) {
  auto it = last_rendered_frame_time_.find(peer);
  if (it == last_rendered_frame_time_.end()) {
    last_rendered_frame_time_.insert({peer, time});
  } else {
    it->second = time;
  }
}

std::optional<Timestamp> StreamState::last_rendered_frame_time(
    size_t peer) const {
  return MaybeGetValue(last_rendered_frame_time_, peer);
}

size_t StreamState::GetLongestReceiverQueue() const {
  size_t max = 0;
  for (size_t receiver : receivers_) {
    size_t cur_size = frame_ids_.size(receiver);
    if (cur_size > max) {
      max = cur_size;
    }
  }
  return max;
}

}  // namespace webrtc
