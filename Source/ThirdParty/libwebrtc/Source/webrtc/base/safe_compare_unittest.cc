/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>

#include "webrtc/base/safe_compare.h"
#include "webrtc/test/gtest.h"

namespace rtc {

namespace {

constexpr std::uintmax_t umax = std::numeric_limits<std::uintmax_t>::max();
constexpr std::intmax_t imin = std::numeric_limits<std::intmax_t>::min();
constexpr std::intmax_t m1 = -1;

// m1 and umax have the same representation because we use 2's complement
// arithmetic, so naive casting will confuse them.
static_assert(static_cast<std::uintmax_t>(m1) == umax, "");
static_assert(m1 == static_cast<std::intmax_t>(umax), "");

std::pair<int, int> p1(1, 1);
std::pair<int, int> p2(1, 2);

}  // namespace

// clang-format off

// These functions aren't used in the tests, but it's useful to look at the
// compiler output for them, and verify that (1) the same-signedness *Safe
// functions result in exactly the same code as their *Ref counterparts, and
// that (2) the mixed-signedness *Safe functions have just a few extra
// arithmetic and logic instructions (but no extra control flow instructions).
bool TestLessThanRef(      int a,      int b) { return a < b; }
bool TestLessThanRef( unsigned a, unsigned b) { return a < b; }
bool TestLessThanSafe(     int a,      int b) { return safe_cmp::Lt(a, b); }
bool TestLessThanSafe(unsigned a, unsigned b) { return safe_cmp::Lt(a, b); }
bool TestLessThanSafe(unsigned a,      int b) { return safe_cmp::Lt(a, b); }
bool TestLessThanSafe(     int a, unsigned b) { return safe_cmp::Lt(a, b); }

// For these, we expect the *Ref and *Safe functions to result in identical
// code, except for the ones that compare a signed variable with an unsigned
// constant; in that case, the *Ref function does an unsigned comparison (fast
// but incorrect) and the *Safe function spends a few extra instructions on
// doing it right.
bool TestLessThan17Ref(       int a) { return a < 17; }
bool TestLessThan17Ref(  unsigned a) { return a < 17; }
bool TestLessThan17uRef(      int a) { return static_cast<unsigned>(a) < 17u; }
bool TestLessThan17uRef( unsigned a) { return a < 17u; }
bool TestLessThan17Safe(      int a) { return safe_cmp::Lt(a, 17); }
bool TestLessThan17Safe( unsigned a) { return safe_cmp::Lt(a, 17); }
bool TestLessThan17uSafe(     int a) { return safe_cmp::Lt(a, 17u); }
bool TestLessThan17uSafe(unsigned a) { return safe_cmp::Lt(a, 17u); }

// Cases where we can't convert to a larger signed type.
bool TestLessThanMax( intmax_t a, uintmax_t b) { return safe_cmp::Lt(a, b); }
bool TestLessThanMax(uintmax_t a,  intmax_t b) { return safe_cmp::Lt(a, b); }
bool TestLessThanMax17u( intmax_t a) { return safe_cmp::Lt(a, uintmax_t{17}); }
bool TestLessThanMax17( uintmax_t a) { return safe_cmp::Lt(a,  intmax_t{17}); }

// Cases where the compiler should be able to compute the result at compile
// time.
bool TestLessThanConst1() { return safe_cmp::Lt(  -1,    1); }
bool TestLessThanConst2() { return safe_cmp::Lt(  m1, umax); }
bool TestLessThanConst3() { return safe_cmp::Lt(umax, imin); }
bool TestLessThanConst4(unsigned a) { return safe_cmp::Lt( a, -1); }
bool TestLessThanConst5(unsigned a) { return safe_cmp::Lt(-1,  a); }
bool TestLessThanConst6(unsigned a) { return safe_cmp::Lt( a,  a); }

// clang-format on

TEST(SafeCmpTest, Eq) {
  EXPECT_FALSE(safe_cmp::Eq(-1, 2));
  EXPECT_FALSE(safe_cmp::Eq(-1, 2u));
  EXPECT_FALSE(safe_cmp::Eq(2, -1));
  EXPECT_FALSE(safe_cmp::Eq(2u, -1));

  EXPECT_FALSE(safe_cmp::Eq(1, 2));
  EXPECT_FALSE(safe_cmp::Eq(1, 2u));
  EXPECT_FALSE(safe_cmp::Eq(1u, 2));
  EXPECT_FALSE(safe_cmp::Eq(1u, 2u));
  EXPECT_FALSE(safe_cmp::Eq(2, 1));
  EXPECT_FALSE(safe_cmp::Eq(2, 1u));
  EXPECT_FALSE(safe_cmp::Eq(2u, 1));
  EXPECT_FALSE(safe_cmp::Eq(2u, 1u));

  EXPECT_TRUE(safe_cmp::Eq(2, 2));
  EXPECT_TRUE(safe_cmp::Eq(2, 2u));
  EXPECT_TRUE(safe_cmp::Eq(2u, 2));
  EXPECT_TRUE(safe_cmp::Eq(2u, 2u));

  EXPECT_TRUE(safe_cmp::Eq(imin, imin));
  EXPECT_FALSE(safe_cmp::Eq(imin, umax));
  EXPECT_FALSE(safe_cmp::Eq(umax, imin));
  EXPECT_TRUE(safe_cmp::Eq(umax, umax));

  EXPECT_TRUE(safe_cmp::Eq(m1, m1));
  EXPECT_FALSE(safe_cmp::Eq(m1, umax));
  EXPECT_FALSE(safe_cmp::Eq(umax, m1));
  EXPECT_TRUE(safe_cmp::Eq(umax, umax));

  EXPECT_FALSE(safe_cmp::Eq(1, 2));
  EXPECT_FALSE(safe_cmp::Eq(1, 2.0));
  EXPECT_FALSE(safe_cmp::Eq(1.0, 2));
  EXPECT_FALSE(safe_cmp::Eq(1.0, 2.0));
  EXPECT_FALSE(safe_cmp::Eq(2, 1));
  EXPECT_FALSE(safe_cmp::Eq(2, 1.0));
  EXPECT_FALSE(safe_cmp::Eq(2.0, 1));
  EXPECT_FALSE(safe_cmp::Eq(2.0, 1.0));

  EXPECT_TRUE(safe_cmp::Eq(2, 2));
  EXPECT_TRUE(safe_cmp::Eq(2, 2.0));
  EXPECT_TRUE(safe_cmp::Eq(2.0, 2));
  EXPECT_TRUE(safe_cmp::Eq(2.0, 2.0));

  EXPECT_TRUE(safe_cmp::Eq(p1, p1));
  EXPECT_FALSE(safe_cmp::Eq(p1, p2));
  EXPECT_FALSE(safe_cmp::Eq(p2, p1));
  EXPECT_TRUE(safe_cmp::Eq(p2, p2));
}

TEST(SafeCmpTest, Ne) {
  EXPECT_TRUE(safe_cmp::Ne(-1, 2));
  EXPECT_TRUE(safe_cmp::Ne(-1, 2u));
  EXPECT_TRUE(safe_cmp::Ne(2, -1));
  EXPECT_TRUE(safe_cmp::Ne(2u, -1));

  EXPECT_TRUE(safe_cmp::Ne(1, 2));
  EXPECT_TRUE(safe_cmp::Ne(1, 2u));
  EXPECT_TRUE(safe_cmp::Ne(1u, 2));
  EXPECT_TRUE(safe_cmp::Ne(1u, 2u));
  EXPECT_TRUE(safe_cmp::Ne(2, 1));
  EXPECT_TRUE(safe_cmp::Ne(2, 1u));
  EXPECT_TRUE(safe_cmp::Ne(2u, 1));
  EXPECT_TRUE(safe_cmp::Ne(2u, 1u));

  EXPECT_FALSE(safe_cmp::Ne(2, 2));
  EXPECT_FALSE(safe_cmp::Ne(2, 2u));
  EXPECT_FALSE(safe_cmp::Ne(2u, 2));
  EXPECT_FALSE(safe_cmp::Ne(2u, 2u));

  EXPECT_FALSE(safe_cmp::Ne(imin, imin));
  EXPECT_TRUE(safe_cmp::Ne(imin, umax));
  EXPECT_TRUE(safe_cmp::Ne(umax, imin));
  EXPECT_FALSE(safe_cmp::Ne(umax, umax));

  EXPECT_FALSE(safe_cmp::Ne(m1, m1));
  EXPECT_TRUE(safe_cmp::Ne(m1, umax));
  EXPECT_TRUE(safe_cmp::Ne(umax, m1));
  EXPECT_FALSE(safe_cmp::Ne(umax, umax));

  EXPECT_TRUE(safe_cmp::Ne(1, 2));
  EXPECT_TRUE(safe_cmp::Ne(1, 2.0));
  EXPECT_TRUE(safe_cmp::Ne(1.0, 2));
  EXPECT_TRUE(safe_cmp::Ne(1.0, 2.0));
  EXPECT_TRUE(safe_cmp::Ne(2, 1));
  EXPECT_TRUE(safe_cmp::Ne(2, 1.0));
  EXPECT_TRUE(safe_cmp::Ne(2.0, 1));
  EXPECT_TRUE(safe_cmp::Ne(2.0, 1.0));

  EXPECT_FALSE(safe_cmp::Ne(2, 2));
  EXPECT_FALSE(safe_cmp::Ne(2, 2.0));
  EXPECT_FALSE(safe_cmp::Ne(2.0, 2));
  EXPECT_FALSE(safe_cmp::Ne(2.0, 2.0));

  EXPECT_FALSE(safe_cmp::Ne(p1, p1));
  EXPECT_TRUE(safe_cmp::Ne(p1, p2));
  EXPECT_TRUE(safe_cmp::Ne(p2, p1));
  EXPECT_FALSE(safe_cmp::Ne(p2, p2));
}

TEST(SafeCmpTest, Lt) {
  EXPECT_TRUE(safe_cmp::Lt(-1, 2));
  EXPECT_TRUE(safe_cmp::Lt(-1, 2u));
  EXPECT_FALSE(safe_cmp::Lt(2, -1));
  EXPECT_FALSE(safe_cmp::Lt(2u, -1));

  EXPECT_TRUE(safe_cmp::Lt(1, 2));
  EXPECT_TRUE(safe_cmp::Lt(1, 2u));
  EXPECT_TRUE(safe_cmp::Lt(1u, 2));
  EXPECT_TRUE(safe_cmp::Lt(1u, 2u));
  EXPECT_FALSE(safe_cmp::Lt(2, 1));
  EXPECT_FALSE(safe_cmp::Lt(2, 1u));
  EXPECT_FALSE(safe_cmp::Lt(2u, 1));
  EXPECT_FALSE(safe_cmp::Lt(2u, 1u));

  EXPECT_FALSE(safe_cmp::Lt(2, 2));
  EXPECT_FALSE(safe_cmp::Lt(2, 2u));
  EXPECT_FALSE(safe_cmp::Lt(2u, 2));
  EXPECT_FALSE(safe_cmp::Lt(2u, 2u));

  EXPECT_FALSE(safe_cmp::Lt(imin, imin));
  EXPECT_TRUE(safe_cmp::Lt(imin, umax));
  EXPECT_FALSE(safe_cmp::Lt(umax, imin));
  EXPECT_FALSE(safe_cmp::Lt(umax, umax));

  EXPECT_FALSE(safe_cmp::Lt(m1, m1));
  EXPECT_TRUE(safe_cmp::Lt(m1, umax));
  EXPECT_FALSE(safe_cmp::Lt(umax, m1));
  EXPECT_FALSE(safe_cmp::Lt(umax, umax));

  EXPECT_TRUE(safe_cmp::Lt(1, 2));
  EXPECT_TRUE(safe_cmp::Lt(1, 2.0));
  EXPECT_TRUE(safe_cmp::Lt(1.0, 2));
  EXPECT_TRUE(safe_cmp::Lt(1.0, 2.0));
  EXPECT_FALSE(safe_cmp::Lt(2, 1));
  EXPECT_FALSE(safe_cmp::Lt(2, 1.0));
  EXPECT_FALSE(safe_cmp::Lt(2.0, 1));
  EXPECT_FALSE(safe_cmp::Lt(2.0, 1.0));

  EXPECT_FALSE(safe_cmp::Lt(2, 2));
  EXPECT_FALSE(safe_cmp::Lt(2, 2.0));
  EXPECT_FALSE(safe_cmp::Lt(2.0, 2));
  EXPECT_FALSE(safe_cmp::Lt(2.0, 2.0));

  EXPECT_FALSE(safe_cmp::Lt(p1, p1));
  EXPECT_TRUE(safe_cmp::Lt(p1, p2));
  EXPECT_FALSE(safe_cmp::Lt(p2, p1));
  EXPECT_FALSE(safe_cmp::Lt(p2, p2));
}

TEST(SafeCmpTest, Le) {
  EXPECT_TRUE(safe_cmp::Le(-1, 2));
  EXPECT_TRUE(safe_cmp::Le(-1, 2u));
  EXPECT_FALSE(safe_cmp::Le(2, -1));
  EXPECT_FALSE(safe_cmp::Le(2u, -1));

  EXPECT_TRUE(safe_cmp::Le(1, 2));
  EXPECT_TRUE(safe_cmp::Le(1, 2u));
  EXPECT_TRUE(safe_cmp::Le(1u, 2));
  EXPECT_TRUE(safe_cmp::Le(1u, 2u));
  EXPECT_FALSE(safe_cmp::Le(2, 1));
  EXPECT_FALSE(safe_cmp::Le(2, 1u));
  EXPECT_FALSE(safe_cmp::Le(2u, 1));
  EXPECT_FALSE(safe_cmp::Le(2u, 1u));

  EXPECT_TRUE(safe_cmp::Le(2, 2));
  EXPECT_TRUE(safe_cmp::Le(2, 2u));
  EXPECT_TRUE(safe_cmp::Le(2u, 2));
  EXPECT_TRUE(safe_cmp::Le(2u, 2u));

  EXPECT_TRUE(safe_cmp::Le(imin, imin));
  EXPECT_TRUE(safe_cmp::Le(imin, umax));
  EXPECT_FALSE(safe_cmp::Le(umax, imin));
  EXPECT_TRUE(safe_cmp::Le(umax, umax));

  EXPECT_TRUE(safe_cmp::Le(m1, m1));
  EXPECT_TRUE(safe_cmp::Le(m1, umax));
  EXPECT_FALSE(safe_cmp::Le(umax, m1));
  EXPECT_TRUE(safe_cmp::Le(umax, umax));

  EXPECT_TRUE(safe_cmp::Le(1, 2));
  EXPECT_TRUE(safe_cmp::Le(1, 2.0));
  EXPECT_TRUE(safe_cmp::Le(1.0, 2));
  EXPECT_TRUE(safe_cmp::Le(1.0, 2.0));
  EXPECT_FALSE(safe_cmp::Le(2, 1));
  EXPECT_FALSE(safe_cmp::Le(2, 1.0));
  EXPECT_FALSE(safe_cmp::Le(2.0, 1));
  EXPECT_FALSE(safe_cmp::Le(2.0, 1.0));

  EXPECT_TRUE(safe_cmp::Le(2, 2));
  EXPECT_TRUE(safe_cmp::Le(2, 2.0));
  EXPECT_TRUE(safe_cmp::Le(2.0, 2));
  EXPECT_TRUE(safe_cmp::Le(2.0, 2.0));

  EXPECT_TRUE(safe_cmp::Le(p1, p1));
  EXPECT_TRUE(safe_cmp::Le(p1, p2));
  EXPECT_FALSE(safe_cmp::Le(p2, p1));
  EXPECT_TRUE(safe_cmp::Le(p2, p2));
}

TEST(SafeCmpTest, Gt) {
  EXPECT_FALSE(safe_cmp::Gt(-1, 2));
  EXPECT_FALSE(safe_cmp::Gt(-1, 2u));
  EXPECT_TRUE(safe_cmp::Gt(2, -1));
  EXPECT_TRUE(safe_cmp::Gt(2u, -1));

  EXPECT_FALSE(safe_cmp::Gt(1, 2));
  EXPECT_FALSE(safe_cmp::Gt(1, 2u));
  EXPECT_FALSE(safe_cmp::Gt(1u, 2));
  EXPECT_FALSE(safe_cmp::Gt(1u, 2u));
  EXPECT_TRUE(safe_cmp::Gt(2, 1));
  EXPECT_TRUE(safe_cmp::Gt(2, 1u));
  EXPECT_TRUE(safe_cmp::Gt(2u, 1));
  EXPECT_TRUE(safe_cmp::Gt(2u, 1u));

  EXPECT_FALSE(safe_cmp::Gt(2, 2));
  EXPECT_FALSE(safe_cmp::Gt(2, 2u));
  EXPECT_FALSE(safe_cmp::Gt(2u, 2));
  EXPECT_FALSE(safe_cmp::Gt(2u, 2u));

  EXPECT_FALSE(safe_cmp::Gt(imin, imin));
  EXPECT_FALSE(safe_cmp::Gt(imin, umax));
  EXPECT_TRUE(safe_cmp::Gt(umax, imin));
  EXPECT_FALSE(safe_cmp::Gt(umax, umax));

  EXPECT_FALSE(safe_cmp::Gt(m1, m1));
  EXPECT_FALSE(safe_cmp::Gt(m1, umax));
  EXPECT_TRUE(safe_cmp::Gt(umax, m1));
  EXPECT_FALSE(safe_cmp::Gt(umax, umax));

  EXPECT_FALSE(safe_cmp::Gt(1, 2));
  EXPECT_FALSE(safe_cmp::Gt(1, 2.0));
  EXPECT_FALSE(safe_cmp::Gt(1.0, 2));
  EXPECT_FALSE(safe_cmp::Gt(1.0, 2.0));
  EXPECT_TRUE(safe_cmp::Gt(2, 1));
  EXPECT_TRUE(safe_cmp::Gt(2, 1.0));
  EXPECT_TRUE(safe_cmp::Gt(2.0, 1));
  EXPECT_TRUE(safe_cmp::Gt(2.0, 1.0));

  EXPECT_FALSE(safe_cmp::Gt(2, 2));
  EXPECT_FALSE(safe_cmp::Gt(2, 2.0));
  EXPECT_FALSE(safe_cmp::Gt(2.0, 2));
  EXPECT_FALSE(safe_cmp::Gt(2.0, 2.0));

  EXPECT_FALSE(safe_cmp::Gt(p1, p1));
  EXPECT_FALSE(safe_cmp::Gt(p1, p2));
  EXPECT_TRUE(safe_cmp::Gt(p2, p1));
  EXPECT_FALSE(safe_cmp::Gt(p2, p2));
}

TEST(SafeCmpTest, Ge) {
  EXPECT_FALSE(safe_cmp::Ge(-1, 2));
  EXPECT_FALSE(safe_cmp::Ge(-1, 2u));
  EXPECT_TRUE(safe_cmp::Ge(2, -1));
  EXPECT_TRUE(safe_cmp::Ge(2u, -1));

  EXPECT_FALSE(safe_cmp::Ge(1, 2));
  EXPECT_FALSE(safe_cmp::Ge(1, 2u));
  EXPECT_FALSE(safe_cmp::Ge(1u, 2));
  EXPECT_FALSE(safe_cmp::Ge(1u, 2u));
  EXPECT_TRUE(safe_cmp::Ge(2, 1));
  EXPECT_TRUE(safe_cmp::Ge(2, 1u));
  EXPECT_TRUE(safe_cmp::Ge(2u, 1));
  EXPECT_TRUE(safe_cmp::Ge(2u, 1u));

  EXPECT_TRUE(safe_cmp::Ge(2, 2));
  EXPECT_TRUE(safe_cmp::Ge(2, 2u));
  EXPECT_TRUE(safe_cmp::Ge(2u, 2));
  EXPECT_TRUE(safe_cmp::Ge(2u, 2u));

  EXPECT_TRUE(safe_cmp::Ge(imin, imin));
  EXPECT_FALSE(safe_cmp::Ge(imin, umax));
  EXPECT_TRUE(safe_cmp::Ge(umax, imin));
  EXPECT_TRUE(safe_cmp::Ge(umax, umax));

  EXPECT_TRUE(safe_cmp::Ge(m1, m1));
  EXPECT_FALSE(safe_cmp::Ge(m1, umax));
  EXPECT_TRUE(safe_cmp::Ge(umax, m1));
  EXPECT_TRUE(safe_cmp::Ge(umax, umax));

  EXPECT_FALSE(safe_cmp::Ge(1, 2));
  EXPECT_FALSE(safe_cmp::Ge(1, 2.0));
  EXPECT_FALSE(safe_cmp::Ge(1.0, 2));
  EXPECT_FALSE(safe_cmp::Ge(1.0, 2.0));
  EXPECT_TRUE(safe_cmp::Ge(2, 1));
  EXPECT_TRUE(safe_cmp::Ge(2, 1.0));
  EXPECT_TRUE(safe_cmp::Ge(2.0, 1));
  EXPECT_TRUE(safe_cmp::Ge(2.0, 1.0));

  EXPECT_TRUE(safe_cmp::Ge(2, 2));
  EXPECT_TRUE(safe_cmp::Ge(2, 2.0));
  EXPECT_TRUE(safe_cmp::Ge(2.0, 2));
  EXPECT_TRUE(safe_cmp::Ge(2.0, 2.0));

  EXPECT_TRUE(safe_cmp::Ge(p1, p1));
  EXPECT_FALSE(safe_cmp::Ge(p1, p2));
  EXPECT_TRUE(safe_cmp::Ge(p2, p1));
  EXPECT_TRUE(safe_cmp::Ge(p2, p2));
}

TEST(SafeCmpTest, Enum) {
  enum E1 { e1 = 13 };
  enum { e2 = 13 };
  enum E3 : unsigned { e3 = 13 };
  enum : unsigned { e4 = 13 };
  EXPECT_TRUE(safe_cmp::Eq(13, e1));
  EXPECT_TRUE(safe_cmp::Eq(13u, e1));
  EXPECT_TRUE(safe_cmp::Eq(13, e2));
  EXPECT_TRUE(safe_cmp::Eq(13u, e2));
  EXPECT_TRUE(safe_cmp::Eq(13, e3));
  EXPECT_TRUE(safe_cmp::Eq(13u, e3));
  EXPECT_TRUE(safe_cmp::Eq(13, e4));
  EXPECT_TRUE(safe_cmp::Eq(13u, e4));
}

}  // namespace rtc
