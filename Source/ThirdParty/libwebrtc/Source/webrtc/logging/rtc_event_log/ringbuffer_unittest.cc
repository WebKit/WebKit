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

#include <list>

#include "webrtc/base/random.h"
#include "webrtc/logging/rtc_event_log/ringbuffer.h"
#include "webrtc/test/gtest.h"

namespace {
template <typename T>
class MovableType {
 public:
  MovableType() : value_(), moved_from_(false), moved_to_(false) {}
  explicit MovableType(T value)
      : value_(value), moved_from_(false), moved_to_(false) {}
  MovableType(const MovableType<T>& other)
      : value_(other.value_), moved_from_(false), moved_to_(false) {}
  MovableType(MovableType<T>&& other)
      : value_(other.value_), moved_from_(false), moved_to_(true) {
    other.moved_from_ = true;
  }

  MovableType& operator=(const MovableType<T>& other) {
    value_ = other.value_;
    moved_from_ = false;
    moved_to_ = false;
    return *this;
  }

  MovableType& operator=(MovableType<T>&& other) {
    value_ = other.value_;
    moved_from_ = false;
    moved_to_ = true;
    other.moved_from_ = true;
    return *this;
  }

  T Value() { return value_; }
  bool IsMovedFrom() { return moved_from_; }
  bool IsMovedTo() { return moved_to_; }

 private:
  T value_;
  bool moved_from_;
  bool moved_to_;
};

}  // namespace

namespace webrtc {

// Verify that the ring buffer works as a simple queue.
TEST(RingBufferTest, SimpleQueue) {
  size_t capacity = 100;
  RingBuffer<size_t> q(capacity);
  EXPECT_TRUE(q.empty());
  for (size_t i = 0; i < capacity; i++) {
    q.push_back(i);
    EXPECT_FALSE(q.empty());
  }

  for (size_t i = 0; i < capacity; i++) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(i, q.front());
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
}

// Do a "random" sequence of queue operations and verify that the
// result is consistent with the same operation performed on a std::list.
TEST(RingBufferTest, ConsistentWithStdList) {
  Random prng(987654321ull);
  size_t capacity = 10;
  RingBuffer<int> q(capacity);
  std::list<int> l;
  EXPECT_TRUE(q.empty());
  for (size_t i = 0; i < 100 * capacity; i++) {
    bool insert = prng.Rand<bool>();
    if ((insert && l.size() < capacity) || l.size() == 0) {
      int x = prng.Rand<int>();
      l.push_back(x);
      q.push_back(x);
      EXPECT_FALSE(q.empty());
    } else {
      EXPECT_FALSE(q.empty());
      EXPECT_EQ(l.front(), q.front());
      l.pop_front();
      q.pop_front();
    }
  }
  while (!l.empty()) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(l.front(), q.front());
    l.pop_front();
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
}

// Test that the ringbuffer starts reusing elements from the front
// when the queue becomes full.
TEST(RingBufferTest, OverwriteOldElements) {
  size_t capacity = 100;
  size_t insertions = 3 * capacity + 25;
  RingBuffer<size_t> q(capacity);
  EXPECT_TRUE(q.empty());
  for (size_t i = 0; i < insertions; i++) {
    q.push_back(i);
    EXPECT_FALSE(q.empty());
  }

  for (size_t i = insertions - capacity; i < insertions; i++) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(i, q.front());
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
}

// Test that the ringbuffer uses std::move when pushing an rvalue reference.
TEST(RingBufferTest, MoveSemanticsForPushBack) {
  size_t capacity = 100;
  size_t insertions = 3 * capacity + 25;
  RingBuffer<MovableType<size_t>> q(capacity);
  EXPECT_TRUE(q.empty());
  for (size_t i = 0; i < insertions; i++) {
    MovableType<size_t> tmp(i);
    EXPECT_FALSE(tmp.IsMovedFrom());
    EXPECT_FALSE(tmp.IsMovedTo());
    q.push_back(std::move(tmp));
    EXPECT_TRUE(tmp.IsMovedFrom());
    EXPECT_FALSE(tmp.IsMovedTo());
    EXPECT_FALSE(q.empty());
  }

  for (size_t i = insertions - capacity; i < insertions; i++) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(i, q.front().Value());
    EXPECT_FALSE(q.front().IsMovedFrom());
    EXPECT_TRUE(q.front().IsMovedTo());
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
}

TEST(RingBufferTest, SmallCapacity) {
  size_t capacity = 1;
  RingBuffer<int> q(capacity);
  EXPECT_TRUE(q.empty());
  q.push_back(4711);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(4711, q.front());
  q.push_back(1024);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(1024, q.front());
  q.pop_front();
  EXPECT_TRUE(q.empty());
}

}  // namespace webrtc
