/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/ptr_util.h"

#include <stddef.h>
#include <string>

#include "webrtc/base/gunit.h"

namespace rtc {

namespace {

class DeleteCounter {
 public:
  DeleteCounter() { ++count_; }
  ~DeleteCounter() { --count_; }

  static size_t count() { return count_; }

 private:
  static size_t count_;
};

size_t DeleteCounter::count_ = 0;

}  // namespace

TEST(PtrUtilTest, WrapUnique) {
  EXPECT_EQ(0u, DeleteCounter::count());
  DeleteCounter* counter = new DeleteCounter;
  EXPECT_EQ(1u, DeleteCounter::count());
  std::unique_ptr<DeleteCounter> owned_counter = WrapUnique(counter);
  EXPECT_EQ(1u, DeleteCounter::count());
  owned_counter.reset();
  EXPECT_EQ(0u, DeleteCounter::count());
}

TEST(PtrUtilTest, MakeUniqueScalar) {
  auto s = MakeUnique<std::string>();
  EXPECT_EQ("", *s);

  auto s2 = MakeUnique<std::string>("test");
  EXPECT_EQ("test", *s2);
}

TEST(PtrUtilTest, MakeUniqueScalarWithMoveOnlyType) {
  using MoveOnly = std::unique_ptr<std::string>;
  auto p = MakeUnique<MoveOnly>(MakeUnique<std::string>("test"));
  EXPECT_EQ("test", **p);
}

TEST(PtrUtilTest, MakeUniqueArray) {
  EXPECT_EQ(0u, DeleteCounter::count());
  auto a = MakeUnique<DeleteCounter[]>(5);
  EXPECT_EQ(5u, DeleteCounter::count());
  a.reset();
  EXPECT_EQ(0u, DeleteCounter::count());
}

}  // namespace rtc
