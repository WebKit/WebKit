/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RINGBUFFER_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RINGBUFFER_H_

#include <memory>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

// A RingBuffer works like a fixed size queue which starts discarding
// the oldest elements when it becomes full.
template <typename T>
class RingBuffer {
 public:
  // Creates a RingBuffer with space for |capacity| elements.
  explicit RingBuffer(size_t capacity)
      :  // We allocate space for one extra sentinel element.
        data_(new T[capacity + 1]) {
    RTC_DCHECK(capacity > 0);
    end_ = data_.get() + (capacity + 1);
    front_ = data_.get();
    back_ = data_.get();
  }

  ~RingBuffer() {
    // The unique_ptr will free the memory.
  }

  // Removes an element from the front of the queue.
  void pop_front() {
    RTC_DCHECK(!empty());
    ++front_;
    if (front_ == end_) {
      front_ = data_.get();
    }
  }

  // Appends an element to the back of the queue (and removes an
  // element from the front if there is no space at the back of the queue).
  void push_back(const T& elem) {
    *back_ = elem;
    ++back_;
    if (back_ == end_) {
      back_ = data_.get();
    }
    if (back_ == front_) {
      ++front_;
    }
    if (front_ == end_) {
      front_ = data_.get();
    }
  }

  // Appends an element to the back of the queue (and removes an
  // element from the front if there is no space at the back of the queue).
  void push_back(T&& elem) {
    *back_ = std::move(elem);
    ++back_;
    if (back_ == end_) {
      back_ = data_.get();
    }
    if (back_ == front_) {
      ++front_;
    }
    if (front_ == end_) {
      front_ = data_.get();
    }
  }

  T& front() { return *front_; }

  const T& front() const { return *front_; }

  bool empty() const { return (front_ == back_); }

 private:
  std::unique_ptr<T[]> data_;
  T* end_;
  T* front_;
  T* back_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RingBuffer);
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RINGBUFFER_H_
