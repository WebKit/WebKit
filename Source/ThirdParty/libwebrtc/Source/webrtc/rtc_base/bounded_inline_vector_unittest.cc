/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bounded_inline_vector.h"

#include <memory>
#include <string>
#include <utility>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using SmallTrivial = BoundedInlineVector<int, 2>;
using LargeTrivial = BoundedInlineVector<int, 200>;
using NonTrivial = BoundedInlineVector<std::string, 2>;
static_assert(std::is_trivially_copyable<SmallTrivial>::value, "");
static_assert(!std::is_trivially_copyable<LargeTrivial>::value, "");
static_assert(std::is_trivially_destructible<LargeTrivial>::value, "");
static_assert(!std::is_trivially_copyable<NonTrivial>::value, "");
static_assert(!std::is_trivially_destructible<NonTrivial>::value, "");

template <typename T>
class BoundedInlineVectorTestAllTypes : public ::testing::Test {};

using AllTypes =
    ::testing::Types<int,                    // Scalar type.
                     std::pair<int, float>,  // Trivial nonprimitive type.
                     std::unique_ptr<int>,   // Move-only type.
                     std::string>;           // Nontrivial copyable type.
TYPED_TEST_SUITE(BoundedInlineVectorTestAllTypes, AllTypes);

template <typename T>
class BoundedInlineVectorTestCopyableTypes : public ::testing::Test {};

using CopyableTypes = ::testing::Types<int, std::pair<int, float>, std::string>;
TYPED_TEST_SUITE(BoundedInlineVectorTestCopyableTypes, CopyableTypes);

TYPED_TEST(BoundedInlineVectorTestAllTypes, ConstructEmpty) {
  BoundedInlineVector<TypeParam, 3> x;
  EXPECT_EQ(x.size(), 0);
  EXPECT_EQ(x.begin(), x.end());
  static_assert(x.capacity() == 3, "");
}

TYPED_TEST(BoundedInlineVectorTestAllTypes, ConstructNonempty) {
  BoundedInlineVector<TypeParam, 3> x = {TypeParam(), TypeParam()};
  EXPECT_EQ(x.size(), 2);
  static_assert(x.capacity() == 3, "");
}

TYPED_TEST(BoundedInlineVectorTestCopyableTypes, CopyConstruct) {
  BoundedInlineVector<TypeParam, 3> x = {TypeParam(), TypeParam()};
  BoundedInlineVector<TypeParam, 2> y = x;
  EXPECT_EQ(y.size(), 2);
  static_assert(x.capacity() == 3, "");
  static_assert(y.capacity() == 2, "");
}

TYPED_TEST(BoundedInlineVectorTestCopyableTypes, CopyAssign) {
  BoundedInlineVector<TypeParam, 3> x = {TypeParam(), TypeParam()};
  BoundedInlineVector<TypeParam, 2> y;
  EXPECT_EQ(y.size(), 0);
  y = x;
  EXPECT_EQ(y.size(), 2);
}

TYPED_TEST(BoundedInlineVectorTestAllTypes, MoveConstruct) {
  BoundedInlineVector<TypeParam, 3> x = {TypeParam(), TypeParam()};
  BoundedInlineVector<TypeParam, 2> y = std::move(x);
  EXPECT_EQ(y.size(), 2);
  static_assert(x.capacity() == 3, "");
  static_assert(y.capacity() == 2, "");
}

TYPED_TEST(BoundedInlineVectorTestAllTypes, MoveAssign) {
  BoundedInlineVector<TypeParam, 3> x = {TypeParam(), TypeParam()};
  BoundedInlineVector<TypeParam, 2> y;
  EXPECT_EQ(y.size(), 0);
  y = std::move(x);
  EXPECT_EQ(y.size(), 2);
}

TEST(BoundedInlineVectorTestOneType, Iteration) {
  BoundedInlineVector<std::string, 4> sv{"one", "two", "three", "four"};
  std::string cat;
  for (const auto& s : sv) {
    cat += s;
  }
  EXPECT_EQ(cat, "onetwothreefour");
}

TEST(BoundedInlineVectorTestOneType, Indexing) {
  BoundedInlineVector<double, 1> x = {3.14};
  EXPECT_EQ(x[0], 3.14);
}

template <typename T, int capacity, typename... Ts>
BoundedInlineVector<T, capacity> Returns(Ts... values) {
  return {std::forward<Ts>(values)...};
}

TYPED_TEST(BoundedInlineVectorTestAllTypes, Return) {
  EXPECT_EQ((Returns<TypeParam, 3>().size()), 0);
  EXPECT_EQ((Returns<TypeParam, 3>(TypeParam(), TypeParam()).size()), 2);
}

TYPED_TEST(BoundedInlineVectorTestAllTypes, Resize) {
  BoundedInlineVector<TypeParam, 17> x;
  EXPECT_EQ(x.size(), 0);
  x.resize(17);
  EXPECT_EQ(x.size(), 17);
  // Test one arbitrary element, mostly to give MSan a chance to scream. But if
  // the type has a trivial default constructor we can't, because the element
  // won't be initialized.
  if (!std::is_trivially_default_constructible<TypeParam>::value) {
    EXPECT_EQ(x[4], TypeParam());
  }
  x.resize(2);
  EXPECT_EQ(x.size(), 2);
}

}  // namespace
}  // namespace webrtc
