/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/sequence_number_util.h"

#include <cstdint>
#include <iterator>
#include <set>

#include "test/gtest.h"

namespace webrtc {
class TestSeqNumUtil : public ::testing::Test {
 protected:
  // Can't use std::numeric_limits<unsigned long>::max() since
  // MSVC doesn't support constexpr.
  static const unsigned long ulmax = ~0ul;  // NOLINT
};

TEST_F(TestSeqNumUtil, AheadOrAt) {
  uint8_t x = 0;
  uint8_t y = 0;
  ASSERT_TRUE(AheadOrAt(x, y));
  ++x;
  ASSERT_TRUE(AheadOrAt(x, y));
  ASSERT_FALSE(AheadOrAt(y, x));
  for (int i = 0; i < 256; ++i) {
    ASSERT_TRUE(AheadOrAt(x, y));
    ++x;
    ++y;
  }

  x = 128;
  y = 0;
  ASSERT_TRUE(AheadOrAt(x, y));
  ASSERT_FALSE(AheadOrAt(y, x));

  x = 129;
  ASSERT_FALSE(AheadOrAt(x, y));
  ASSERT_TRUE(AheadOrAt(y, x));
  ASSERT_TRUE(AheadOrAt<uint16_t>(x, y));
  ASSERT_FALSE(AheadOrAt<uint16_t>(y, x));
}

TEST_F(TestSeqNumUtil, AheadOrAtWithDivisor) {
  ASSERT_TRUE((AheadOrAt<uint8_t, 11>(5, 0)));
  ASSERT_FALSE((AheadOrAt<uint8_t, 11>(6, 0)));
  ASSERT_FALSE((AheadOrAt<uint8_t, 11>(0, 5)));
  ASSERT_TRUE((AheadOrAt<uint8_t, 11>(0, 6)));

  ASSERT_TRUE((AheadOrAt<uint8_t, 10>(5, 0)));
  ASSERT_FALSE((AheadOrAt<uint8_t, 10>(6, 0)));
  ASSERT_FALSE((AheadOrAt<uint8_t, 10>(0, 5)));
  ASSERT_TRUE((AheadOrAt<uint8_t, 10>(0, 6)));

  const uint8_t D = 211;
  uint8_t x = 0;
  for (int i = 0; i < D; ++i) {
    uint8_t next_x = Add<D>(x, 1);
    ASSERT_TRUE((AheadOrAt<uint8_t, D>(i, i)));
    ASSERT_TRUE((AheadOrAt<uint8_t, D>(next_x, i)));
    ASSERT_FALSE((AheadOrAt<uint8_t, D>(i, next_x)));
    x = next_x;
  }
}

TEST_F(TestSeqNumUtil, AheadOf) {
  uint8_t x = 0;
  uint8_t y = 0;
  ASSERT_FALSE(AheadOf(x, y));
  ++x;
  ASSERT_TRUE(AheadOf(x, y));
  ASSERT_FALSE(AheadOf(y, x));
  for (int i = 0; i < 256; ++i) {
    ASSERT_TRUE(AheadOf(x, y));
    ++x;
    ++y;
  }

  x = 128;
  y = 0;
  for (int i = 0; i < 128; ++i) {
    ASSERT_TRUE(AheadOf(x, y));
    ASSERT_FALSE(AheadOf(y, x));
    x++;
    y++;
  }

  for (int i = 0; i < 128; ++i) {
    ASSERT_FALSE(AheadOf(x, y));
    ASSERT_TRUE(AheadOf(y, x));
    x++;
    y++;
  }

  x = 129;
  y = 0;
  ASSERT_FALSE(AheadOf(x, y));
  ASSERT_TRUE(AheadOf(y, x));
  ASSERT_TRUE(AheadOf<uint16_t>(x, y));
  ASSERT_FALSE(AheadOf<uint16_t>(y, x));
}

TEST_F(TestSeqNumUtil, AheadOfWithDivisor) {
  ASSERT_TRUE((AheadOf<uint8_t, 11>(5, 0)));
  ASSERT_FALSE((AheadOf<uint8_t, 11>(6, 0)));
  ASSERT_FALSE((AheadOf<uint8_t, 11>(0, 5)));
  ASSERT_TRUE((AheadOf<uint8_t, 11>(0, 6)));

  ASSERT_TRUE((AheadOf<uint8_t, 10>(5, 0)));
  ASSERT_FALSE((AheadOf<uint8_t, 10>(6, 0)));
  ASSERT_FALSE((AheadOf<uint8_t, 10>(0, 5)));
  ASSERT_TRUE((AheadOf<uint8_t, 10>(0, 6)));

  const uint8_t D = 211;
  uint8_t x = 0;
  for (int i = 0; i < D; ++i) {
    uint8_t next_x = Add<D>(x, 1);
    ASSERT_FALSE((AheadOf<uint8_t, D>(i, i)));
    ASSERT_TRUE((AheadOf<uint8_t, D>(next_x, i)));
    ASSERT_FALSE((AheadOf<uint8_t, D>(i, next_x)));
    x = next_x;
  }
}

TEST_F(TestSeqNumUtil, ForwardDiffWithDivisor) {
  const uint8_t kDivisor = 211;

  for (uint8_t i = 0; i < kDivisor - 1; ++i) {
    ASSERT_EQ(0, (ForwardDiff<uint8_t, kDivisor>(i, i)));
    ASSERT_EQ(1, (ForwardDiff<uint8_t, kDivisor>(i, i + 1)));
    ASSERT_EQ(kDivisor - 1, (ForwardDiff<uint8_t, kDivisor>(i + 1, i)));
  }

  for (uint8_t i = 1; i < kDivisor; ++i) {
    ASSERT_EQ(i, (ForwardDiff<uint8_t, kDivisor>(0, i)));
    ASSERT_EQ(kDivisor - i, (ForwardDiff<uint8_t, kDivisor>(i, 0)));
  }
}

TEST_F(TestSeqNumUtil, ReverseDiffWithDivisor) {
  const uint8_t kDivisor = 241;

  for (uint8_t i = 0; i < kDivisor - 1; ++i) {
    ASSERT_EQ(0, (ReverseDiff<uint8_t, kDivisor>(i, i)));
    ASSERT_EQ(kDivisor - 1, (ReverseDiff<uint8_t, kDivisor>(i, i + 1)));
    ASSERT_EQ(1, (ReverseDiff<uint8_t, kDivisor>(i + 1, i)));
  }

  for (uint8_t i = 1; i < kDivisor; ++i) {
    ASSERT_EQ(kDivisor - i, (ReverseDiff<uint8_t, kDivisor>(0, i)));
    ASSERT_EQ(i, (ReverseDiff<uint8_t, kDivisor>(i, 0)));
  }
}

TEST_F(TestSeqNumUtil, SeqNumComparator) {
  std::set<uint8_t, AscendingSeqNumComp<uint8_t>> seq_nums_asc;
  std::set<uint8_t, DescendingSeqNumComp<uint8_t>> seq_nums_desc;

  uint8_t x = 0;
  for (int i = 0; i < 128; ++i) {
    seq_nums_asc.insert(x);
    seq_nums_desc.insert(x);
    ASSERT_EQ(x, *seq_nums_asc.begin());
    ASSERT_EQ(x, *seq_nums_desc.rbegin());
    ++x;
  }

  seq_nums_asc.clear();
  seq_nums_desc.clear();
  x = 199;
  for (int i = 0; i < 128; ++i) {
    seq_nums_asc.insert(x);
    seq_nums_desc.insert(x);
    ASSERT_EQ(x, *seq_nums_asc.begin());
    ASSERT_EQ(x, *seq_nums_desc.rbegin());
    ++x;
  }
}

TEST_F(TestSeqNumUtil, SeqNumComparatorWithDivisor) {
  const uint8_t D = 223;

  std::set<uint8_t, AscendingSeqNumComp<uint8_t, D>> seq_nums_asc;
  std::set<uint8_t, DescendingSeqNumComp<uint8_t, D>> seq_nums_desc;

  uint8_t x = 0;
  for (int i = 0; i < D / 2; ++i) {
    seq_nums_asc.insert(x);
    seq_nums_desc.insert(x);
    ASSERT_EQ(x, *seq_nums_asc.begin());
    ASSERT_EQ(x, *seq_nums_desc.rbegin());
    x = Add<D>(x, 1);
  }

  seq_nums_asc.clear();
  seq_nums_desc.clear();
  x = 200;
  for (int i = 0; i < D / 2; ++i) {
    seq_nums_asc.insert(x);
    seq_nums_desc.insert(x);
    ASSERT_EQ(x, *seq_nums_asc.begin());
    ASSERT_EQ(x, *seq_nums_desc.rbegin());
    x = Add<D>(x, 1);
  }
}

TEST(SeqNumUnwrapper, PreserveStartValue) {
  SeqNumUnwrapper<uint8_t> unwrapper;
  EXPECT_EQ(123, unwrapper.Unwrap(123));
}

TEST(SeqNumUnwrapper, ForwardWrap) {
  SeqNumUnwrapper<uint8_t> unwrapper;
  EXPECT_EQ(255, unwrapper.Unwrap(255));
  EXPECT_EQ(256, unwrapper.Unwrap(0));
}

TEST(SeqNumUnwrapper, ForwardWrapWithDivisor) {
  SeqNumUnwrapper<uint8_t, 33> unwrapper;
  EXPECT_EQ(30, unwrapper.Unwrap(30));
  EXPECT_EQ(36, unwrapper.Unwrap(3));
}

TEST(SeqNumUnwrapper, BackWardWrap) {
  SeqNumUnwrapper<uint8_t> unwrapper;
  EXPECT_EQ(0, unwrapper.Unwrap(0));
  EXPECT_EQ(-2, unwrapper.Unwrap(254));
}

TEST(SeqNumUnwrapper, BackWardWrapWithDivisor) {
  SeqNumUnwrapper<uint8_t, 33> unwrapper;
  EXPECT_EQ(0, unwrapper.Unwrap(0));
  EXPECT_EQ(-2, unwrapper.Unwrap(31));
}

TEST(SeqNumUnwrapper, Unwrap) {
  SeqNumUnwrapper<uint16_t> unwrapper;
  const uint16_t kMax = std::numeric_limits<uint16_t>::max();
  const uint16_t kMaxDist = kMax / 2 + 1;

  EXPECT_EQ(0, unwrapper.Unwrap(0));
  EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
  EXPECT_EQ(0, unwrapper.Unwrap(0));

  EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
  EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
  EXPECT_EQ(kMax + 1, unwrapper.Unwrap(0));
  EXPECT_EQ(kMax, unwrapper.Unwrap(kMax));
  EXPECT_EQ(kMaxDist, unwrapper.Unwrap(kMaxDist));
  EXPECT_EQ(0, unwrapper.Unwrap(0));
}

TEST(SeqNumUnwrapper, UnwrapOddDivisor) {
  SeqNumUnwrapper<uint8_t, 11> unwrapper;

  EXPECT_EQ(10, unwrapper.Unwrap(10));
  EXPECT_EQ(11, unwrapper.Unwrap(0));
  EXPECT_EQ(16, unwrapper.Unwrap(5));
  EXPECT_EQ(21, unwrapper.Unwrap(10));
  EXPECT_EQ(22, unwrapper.Unwrap(0));
  EXPECT_EQ(17, unwrapper.Unwrap(6));
  EXPECT_EQ(12, unwrapper.Unwrap(1));
  EXPECT_EQ(7, unwrapper.Unwrap(7));
  EXPECT_EQ(2, unwrapper.Unwrap(2));
  EXPECT_EQ(0, unwrapper.Unwrap(0));
}

TEST(SeqNumUnwrapper, ManyForwardWraps) {
  const int kLargeNumber = 4711;
  const int kMaxStep = kLargeNumber / 2;
  const int kNumWraps = 100;
  SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

  uint16_t next_unwrap = 0;
  int64_t expected = 0;
  for (int i = 0; i < kNumWraps * 2 + 1; ++i) {
    EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
    expected += kMaxStep;
    next_unwrap = (next_unwrap + kMaxStep) % kLargeNumber;
  }
}

TEST(SeqNumUnwrapper, ManyBackwardWraps) {
  const int kLargeNumber = 4711;
  const int kMaxStep = kLargeNumber / 2;
  const int kNumWraps = 100;
  SeqNumUnwrapper<uint16_t, kLargeNumber> unwrapper;

  uint16_t next_unwrap = 0;
  int64_t expected = 0;
  for (uint16_t i = 0; i < kNumWraps * 2 + 1; ++i) {
    EXPECT_EQ(expected, unwrapper.Unwrap(next_unwrap));
    expected -= kMaxStep;
    next_unwrap = (next_unwrap + kMaxStep + 1) % kLargeNumber;
  }
}

TEST(SeqNumUnwrapper, UnwrapForward) {
  SeqNumUnwrapper<uint8_t> unwrapper;
  EXPECT_EQ(255, unwrapper.Unwrap(255));
  EXPECT_EQ(256, unwrapper.UnwrapForward(0));
  EXPECT_EQ(511, unwrapper.UnwrapForward(255));
}

TEST(SeqNumUnwrapper, UnwrapForwardWithDivisor) {
  SeqNumUnwrapper<uint8_t, 33> unwrapper;
  EXPECT_EQ(30, unwrapper.UnwrapForward(30));
  EXPECT_EQ(36, unwrapper.UnwrapForward(3));
  EXPECT_EQ(63, unwrapper.UnwrapForward(30));
}

TEST(SeqNumUnwrapper, UnwrapBackwards) {
  SeqNumUnwrapper<uint8_t> unwrapper;
  EXPECT_EQ(0, unwrapper.UnwrapBackwards(0));
  EXPECT_EQ(-2, unwrapper.UnwrapBackwards(254));
  EXPECT_EQ(-256, unwrapper.UnwrapBackwards(0));
}

TEST(SeqNumUnwrapper, UnwrapBackwardsWithDivisor) {
  SeqNumUnwrapper<uint8_t, 33> unwrapper;
  EXPECT_EQ(0, unwrapper.Unwrap(0));
  EXPECT_EQ(-2, unwrapper.UnwrapBackwards(31));
  EXPECT_EQ(-33, unwrapper.UnwrapBackwards(0));
}

}  // namespace webrtc
