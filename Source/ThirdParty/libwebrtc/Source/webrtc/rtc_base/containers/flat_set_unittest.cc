/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This implementation is borrowed from Chromium.

#include "rtc_base/containers/flat_set.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/containers/move_only_int.h"
#include "test/gmock.h"
#include "test/gtest.h"

// A flat_set is basically a interface to flat_tree. So several basic
// operations are tested to make sure things are set up properly, but the bulk
// of the tests are in flat_tree_unittests.cc.

using ::testing::ElementsAre;

namespace webrtc {
namespace {

TEST(FlatSet, IncompleteType) {
  struct A {
    using Set = flat_set<A>;
    int data;
    Set set_with_incomplete_type;
    Set::iterator it;
    Set::const_iterator cit;

    // We do not declare operator< because clang complains that it's unused.
  };

  A a;
}

TEST(FlatSet, RangeConstructor) {
  flat_set<int>::value_type input_vals[] = {1, 1, 1, 2, 2, 2, 3, 3, 3};

  flat_set<int> cont(std::begin(input_vals), std::end(input_vals));
  EXPECT_THAT(cont, ElementsAre(1, 2, 3));
}

TEST(FlatSet, MoveConstructor) {
  int input_range[] = {1, 2, 3, 4};

  flat_set<MoveOnlyInt> original(std::begin(input_range),
                                 std::end(input_range));
  flat_set<MoveOnlyInt> moved(std::move(original));

  EXPECT_EQ(1U, moved.count(MoveOnlyInt(1)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(2)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(3)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(4)));
}

TEST(FlatSet, InitializerListConstructor) {
  flat_set<int> cont({1, 2, 3, 4, 5, 6, 10, 8});
  EXPECT_THAT(cont, ElementsAre(1, 2, 3, 4, 5, 6, 8, 10));
}

TEST(FlatSet, InsertFindSize) {
  flat_set<int> s;
  s.insert(1);
  s.insert(1);
  s.insert(2);

  EXPECT_EQ(2u, s.size());
  EXPECT_EQ(1, *s.find(1));
  EXPECT_EQ(2, *s.find(2));
  EXPECT_EQ(s.end(), s.find(7));
}

TEST(FlatSet, CopySwap) {
  flat_set<int> original;
  original.insert(1);
  original.insert(2);
  EXPECT_THAT(original, ElementsAre(1, 2));

  flat_set<int> copy(original);
  EXPECT_THAT(copy, ElementsAre(1, 2));

  copy.erase(copy.begin());
  copy.insert(10);
  EXPECT_THAT(copy, ElementsAre(2, 10));

  original.swap(copy);
  EXPECT_THAT(original, ElementsAre(2, 10));
  EXPECT_THAT(copy, ElementsAre(1, 2));
}

TEST(FlatSet, UsingTransparentCompare) {
  using ExplicitInt = webrtc::MoveOnlyInt;
  flat_set<ExplicitInt> s;
  const auto& s1 = s;
  int x = 0;

  // Check if we can use lookup functions without converting to key_type.
  // Correctness is checked in flat_tree tests.
  s.count(x);
  s1.count(x);
  s.find(x);
  s1.find(x);
  s.equal_range(x);
  s1.equal_range(x);
  s.lower_bound(x);
  s1.lower_bound(x);
  s.upper_bound(x);
  s1.upper_bound(x);
  s.erase(x);

  // Check if we broke overload resolution.
  s.emplace(0);
  s.emplace(1);
  s.erase(s.begin());
  s.erase(s.cbegin());
}

TEST(FlatSet, SupportsEraseIf) {
  flat_set<MoveOnlyInt> s;
  s.emplace(MoveOnlyInt(1));
  s.emplace(MoveOnlyInt(2));
  s.emplace(MoveOnlyInt(3));
  s.emplace(MoveOnlyInt(4));
  s.emplace(MoveOnlyInt(5));

  EraseIf(s, [to_be_removed = MoveOnlyInt(2)](const MoveOnlyInt& elem) {
    return elem == to_be_removed;
  });

  EXPECT_EQ(s.size(), 4u);
  ASSERT_TRUE(s.find(MoveOnlyInt(1)) != s.end());
  ASSERT_FALSE(s.find(MoveOnlyInt(2)) != s.end());
  ASSERT_TRUE(s.find(MoveOnlyInt(3)) != s.end());
  ASSERT_TRUE(s.find(MoveOnlyInt(4)) != s.end());
  ASSERT_TRUE(s.find(MoveOnlyInt(5)) != s.end());
}
}  // namespace
}  // namespace webrtc
