/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/dvqa/pausable_state.h"

#include <cstdint>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {

void PausableState::Pause() {
  if (!IsPaused()) {
    events_.push_back(Event{.time = clock_->CurrentTime(), .is_paused = true});
  }
}

void PausableState::Resume() {
  if (IsPaused()) {
    events_.push_back(Event{.time = clock_->CurrentTime(), .is_paused = false});
  }
}

bool PausableState::IsPaused() const {
  return !events_.empty() && events_.back().is_paused;
}

bool PausableState::WasPausedAt(Timestamp time) const {
  if (events_.empty()) {
    return false;
  }

  int64_t pos = GetPos(time);
  return pos != -1 && events_[pos].is_paused;
}

bool PausableState::WasResumedAfter(Timestamp time) const {
  if (events_.empty()) {
    return false;
  }

  int64_t pos = GetPos(time);
  return (pos + 1 < static_cast<int64_t>(events_.size())) &&
         !events_[pos + 1].is_paused;
}

Timestamp PausableState::GetLastEventTime() const {
  if (events_.empty()) {
    return Timestamp::PlusInfinity();
  }

  return events_.back().time;
}

TimeDelta PausableState::GetActiveDurationFrom(Timestamp time) const {
  if (events_.empty()) {
    return clock_->CurrentTime() - time;
  }

  int64_t pos = GetPos(time);
  TimeDelta duration = TimeDelta::Zero();
  for (int64_t i = pos; i < static_cast<int64_t>(events_.size()); ++i) {
    if (i == -1 || !events_[i].is_paused) {
      Timestamp start_time = (i == pos) ? time : events_[i].time;
      Timestamp end_time = (i + 1 == static_cast<int64_t>(events_.size()))
                               ? clock_->CurrentTime()
                               : events_[i + 1].time;

      duration += end_time - start_time;
    }
  }
  return duration;
}

int64_t PausableState::GetPos(Timestamp time) const {
  int64_t l = 0, r = events_.size() - 1;
  while (l < r) {
    int64_t pos = (l + r) / 2;
    if (time < events_[pos].time) {
      r = pos;
    } else if (time >= events_[pos].time) {
      l = pos + 1;
    }
  }
  if (time < events_[l].time) {
    return l - 1;
  } else {
    return l;
  }
}

}  // namespace webrtc
