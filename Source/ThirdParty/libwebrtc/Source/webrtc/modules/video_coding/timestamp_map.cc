/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timestamp_map.h"

#include <stdlib.h>

#include "modules/include/module_common_types_public.h"

namespace webrtc {

VCMTimestampMap::VCMTimestampMap(size_t capacity)
    : ring_buffer_(new TimestampDataTuple[capacity]),
      capacity_(capacity),
      next_add_idx_(0),
      next_pop_idx_(0) {}

VCMTimestampMap::~VCMTimestampMap() {}

void VCMTimestampMap::Add(uint32_t timestamp, const VCMFrameInformation& data) {
  ring_buffer_[next_add_idx_].timestamp = timestamp;
  ring_buffer_[next_add_idx_].data = data;
  next_add_idx_ = (next_add_idx_ + 1) % capacity_;

  if (next_add_idx_ == next_pop_idx_) {
    // Circular list full; forget oldest entry.
    next_pop_idx_ = (next_pop_idx_ + 1) % capacity_;
  }
}

absl::optional<VCMFrameInformation> VCMTimestampMap::Pop(uint32_t timestamp) {
  while (!IsEmpty()) {
    if (ring_buffer_[next_pop_idx_].timestamp == timestamp) {
      // Found start time for this timestamp.
      const VCMFrameInformation& data = ring_buffer_[next_pop_idx_].data;
      ring_buffer_[next_pop_idx_].timestamp = 0;
      next_pop_idx_ = (next_pop_idx_ + 1) % capacity_;
      return data;
    } else if (IsNewerTimestamp(ring_buffer_[next_pop_idx_].timestamp,
                                timestamp)) {
      // The timestamp we are looking for is not in the list.
      return absl::nullopt;
    }

    // Not in this position, check next (and forget this position).
    next_pop_idx_ = (next_pop_idx_ + 1) % capacity_;
  }

  // Could not find matching timestamp in list.
  return absl::nullopt;
}

bool VCMTimestampMap::IsEmpty() const {
  return (next_add_idx_ == next_pop_idx_);
}

size_t VCMTimestampMap::Size() const {
  // The maximum number of elements in the list is |capacity_| - 1. The list is
  // empty if the add and pop indices are equal.
  return next_add_idx_ >= next_pop_idx_
             ? next_add_idx_ - next_pop_idx_
             : next_add_idx_ + capacity_ - next_pop_idx_;
}

void VCMTimestampMap::Clear() {
  while (!IsEmpty()) {
    ring_buffer_[next_pop_idx_].timestamp = 0;
    next_pop_idx_ = (next_pop_idx_ + 1) % capacity_;
  }
}

}  // namespace webrtc
