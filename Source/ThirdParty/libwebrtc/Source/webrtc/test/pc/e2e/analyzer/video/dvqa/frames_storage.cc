/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/dvqa/frames_storage.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "api/units/timestamp.h"
#include "api/video/video_frame.h"

namespace webrtc {

void FramesStorage::Add(const VideoFrame& frame, Timestamp captured_time) {
  heap_.push_back(HeapNode{.frame = frame, .captured_time = captured_time});
  frame_id_index_[frame.id()] = heap_.size() - 1;
  Heapify(heap_.size() - 1);
  RemoveTooOldFrames();
}

std::optional<VideoFrame> FramesStorage::Get(uint16_t frame_id) {
  auto it = frame_id_index_.find(frame_id);
  if (it == frame_id_index_.end()) {
    return std::nullopt;
  }

  return heap_[it->second].frame;
}

void FramesStorage::Remove(uint16_t frame_id) {
  RemoveInternal(frame_id);
  RemoveTooOldFrames();
}

void FramesStorage::RemoveInternal(uint16_t frame_id) {
  auto it = frame_id_index_.find(frame_id);
  if (it == frame_id_index_.end()) {
    return;
  }

  size_t index = it->second;
  frame_id_index_.erase(it);

  // If it's not the last element in the heap, swap the last element in the heap
  // with element to remove.
  if (index != heap_.size() - 1) {
    heap_[index] = std::move(heap_[heap_.size() - 1]);
    frame_id_index_[heap_[index].frame.id()] = index;
  }

  // Remove the last element.
  heap_.pop_back();

  if (index < heap_.size()) {
    Heapify(index);
  }
}

void FramesStorage::Heapify(size_t index) {
  HeapifyUp(index);
  HeapifyDown(index);
}

void FramesStorage::HeapifyUp(size_t index) {
  if (index == 0) {
    return;
  }
  RTC_CHECK_LT(index, heap_.size());
  size_t parent = index / 2;
  if (heap_[parent].captured_time <= heap_[index].captured_time) {
    return;
  }
  HeapNode tmp = std::move(heap_[index]);
  heap_[index] = std::move(heap_[parent]);
  heap_[parent] = std::move(tmp);

  frame_id_index_[heap_[index].frame.id()] = index;
  frame_id_index_[heap_[parent].frame.id()] = parent;

  HeapifyUp(parent);
}

void FramesStorage::HeapifyDown(size_t index) {
  RTC_CHECK_GE(index, 0);
  RTC_CHECK_LT(index, heap_.size());

  size_t left_child = 2 * index;
  size_t right_child = 2 * index + 1;

  if (left_child >= heap_.size()) {
    return;
  }
  size_t smallest_child = left_child;
  if (right_child < heap_.size() &&
      heap_[right_child].captured_time < heap_[left_child].captured_time) {
    smallest_child = right_child;
  }

  if (heap_[index].captured_time <= heap_[smallest_child].captured_time) {
    return;
  }

  HeapNode tmp = std::move(heap_[index]);
  heap_[index] = std::move(heap_[smallest_child]);
  heap_[smallest_child] = std::move(tmp);

  frame_id_index_[heap_[index].frame.id()] = index;
  frame_id_index_[heap_[smallest_child].frame.id()] = smallest_child;

  HeapifyDown(smallest_child);
}

void FramesStorage::RemoveTooOldFrames() {
  Timestamp now = clock_->CurrentTime();
  while (!heap_.empty() &&
         (heap_[0].captured_time + max_storage_duration_) < now) {
    RemoveInternal(heap_[0].frame.id());
  }
}

}  // namespace webrtc
