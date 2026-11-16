/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_MAC_AUDIO_RING_BUFFER_MAC_H_
#define MODULES_AUDIO_DEVICE_MAC_AUDIO_RING_BUFFER_MAC_H_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

#include "rtc_base/checks.h"

namespace webrtc {

// A lightweight, lock-free, single-producer single-consumer (SPSC) ring buffer
// for streaming audio samples.
template <typename T>
class AudioRingBufferMac {
 public:
  explicit AudioRingBufferMac(size_t capacity)
      : capacity_(capacity),
        buffer_(capacity + 1),
        read_index_(0),
        write_index_(0) {
    RTC_DCHECK_GT(capacity, 0);
  }

  AudioRingBufferMac(const AudioRingBufferMac&) = delete;
  AudioRingBufferMac& operator=(const AudioRingBufferMac&) = delete;

  // Resets the buffer to empty. Should only be called when the buffer
  // is idle or externally synchronized.
  void Clear() {
    write_index_.store(0, std::memory_order_relaxed);
    read_index_.store(0, std::memory_order_relaxed);
  }

  size_t capacity() const { return capacity_; }

  // Returns the number of readable elements currently in the buffer.
  size_t AvailableToRead() const {
    const size_t w = write_index_.load(std::memory_order_acquire);
    const size_t r = read_index_.load(std::memory_order_relaxed);
    return (w >= r) ? (w - r) : (buffer_.size() - (r - w));
  }

  // Returns the number of writable element slots available in the buffer.
  size_t AvailableToWrite() const {
    const size_t w = write_index_.load(std::memory_order_relaxed);
    const size_t r = read_index_.load(std::memory_order_acquire);
    const size_t used = (w >= r) ? (w - r) : (buffer_.size() - (r - w));
    return capacity_ - used;
  }

  // Writes up to `data.size()` elements from `data` into the buffer.
  // If there are fewer writeable element slots than `data.size()` in the
  // buffer, the available slots are filled.
  // Returns the number of elements written.
  size_t Write(std::span<const T> data) {
    const size_t to_write = std::min(data.size(), AvailableToWrite());
    if (to_write == 0) {
      return 0;
    }

    const size_t w = write_index_.load(std::memory_order_relaxed);
    const size_t first = std::min(to_write, buffer_.size() - w);

    std::copy_n(data.data(), first, buffer_.data() + w);
    std::copy_n(data.data() + first, to_write - first, buffer_.data());

    const size_t new_w = w + to_write;
    write_index_.store(new_w >= buffer_.size() ? new_w - buffer_.size() : new_w,
                       std::memory_order_release);
    return to_write;
  }

  // Reads up to `data.size()` elements from the buffer into `data`.
  // If there are fewer readable elements than `data.size()` available in the
  // buffer, those available are read.
  // Returns the number of elements read.
  size_t Read(std::span<T> data) {
    const size_t to_read = std::min(data.size(), AvailableToRead());
    if (to_read == 0) {
      return 0;
    }

    const size_t r = read_index_.load(std::memory_order_relaxed);
    const size_t first = std::min(to_read, buffer_.size() - r);

    std::copy_n(buffer_.data() + r, first, data.data());
    std::copy_n(buffer_.data(), to_read - first, data.data() + first);

    const size_t new_r = r + to_read;
    read_index_.store(new_r >= buffer_.size() ? new_r - buffer_.size() : new_r,
                      std::memory_order_release);
    return to_read;
  }

 private:
  const size_t capacity_;
  std::vector<T> buffer_;
  alignas(64) std::atomic<size_t> read_index_;
  alignas(64) std::atomic<size_t> write_index_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_MAC_AUDIO_RING_BUFFER_MAC_H_
