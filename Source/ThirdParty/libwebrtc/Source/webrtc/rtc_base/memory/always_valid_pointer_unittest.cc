/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/always_valid_pointer.h"

#include <string>

#include "test/gtest.h"

namespace webrtc {

TEST(AlwaysValidPointerTest, DefaultToEmptyValue) {
  AlwaysValidPointer<std::string> ptr(nullptr);
  EXPECT_EQ(*ptr, "");
}
TEST(AlwaysValidPointerTest, DefaultWithForwardedArgument) {
  AlwaysValidPointer<std::string> ptr(nullptr, "test");
  EXPECT_EQ(*ptr, "test");
}
TEST(AlwaysValidPointerTest, DefaultToSubclass) {
  struct A {
    virtual ~A() {}
    virtual int f() = 0;
  };
  struct B : public A {
    int b = 0;
    explicit B(int val) : b(val) {}
    virtual ~B() {}
    int f() override { return b; }
  };
  AlwaysValidPointer<A, B> ptr(nullptr, 3);
  EXPECT_EQ(ptr->f(), 3);
  EXPECT_EQ((*ptr).f(), 3);
  EXPECT_EQ(ptr.get()->f(), 3);
}
TEST(AlwaysValidPointerTest, NonDefaultValue) {
  std::string str("keso");
  AlwaysValidPointer<std::string> ptr(&str, "test");
  EXPECT_EQ(*ptr, "keso");
}

TEST(AlwaysValidPointerTest, TakeOverOwnershipOfInstance) {
  std::string str("keso");
  std::unique_ptr<std::string> str2 = std::make_unique<std::string>("kent");
  AlwaysValidPointer<std::string> ptr(std::move(str2), &str);
  EXPECT_EQ(*ptr, "kent");
  EXPECT_EQ(str2, nullptr);
}

TEST(AlwaysValidPointerTest, TakeOverOwnershipFallbackOnPointer) {
  std::string str("keso");
  std::unique_ptr<std::string> str2;
  AlwaysValidPointer<std::string> ptr(std::move(str2), &str);
  EXPECT_EQ(*ptr, "keso");
}

TEST(AlwaysValidPointerTest, TakeOverOwnershipFallbackOnDefault) {
  std::unique_ptr<std::string> str;
  std::string* str_ptr = nullptr;
  AlwaysValidPointer<std::string> ptr(std::move(str), str_ptr);
  EXPECT_EQ(*ptr, "");
}

TEST(AlwaysValidPointerTest,
     TakeOverOwnershipFallbackOnDefaultWithForwardedArgument) {
  std::unique_ptr<std::string> str2;
  AlwaysValidPointer<std::string> ptr(std::move(str2), nullptr, "keso");
  EXPECT_EQ(*ptr, "keso");
}

TEST(AlwaysValidPointerTest, TakeOverOwnershipDoesNotForwardDefaultArguments) {
  std::unique_ptr<std::string> str = std::make_unique<std::string>("kalle");
  std::unique_ptr<std::string> str2 = std::make_unique<std::string>("anka");
  AlwaysValidPointer<std::string> ptr(std::move(str), nullptr, *str2);
  EXPECT_EQ(*ptr, "kalle");
  EXPECT_TRUE(!str);
  EXPECT_EQ(*str2, "anka");
}

TEST(AlwaysValidPointerTest, DefaultToLambda) {
  AlwaysValidPointer<std::string> ptr(
      nullptr, []() { return std::make_unique<std::string>("onkel skrue"); });
  EXPECT_EQ(*ptr, "onkel skrue");
}

TEST(AlwaysValidPointerTest, NoDefaultObjectPassValidPointer) {
  std::string str("foo");
  AlwaysValidPointerNoDefault<std::string> ptr(&str);
  EXPECT_EQ(*ptr, "foo");
  EXPECT_EQ(ptr, &str);
}

TEST(AlwaysValidPointerTest, NoDefaultObjectWithTakeOverOwnership) {
  std::unique_ptr<std::string> str = std::make_unique<std::string>("yum");
  AlwaysValidPointerNoDefault<std::string> ptr(std::move(str));
  EXPECT_EQ(*ptr, "yum");
  std::unique_ptr<std::string> str2 = std::make_unique<std::string>("fun");
  AlwaysValidPointerNoDefault<std::string> ptr2(std::move(str), str2.get());
  EXPECT_EQ(*ptr2, "fun");
  EXPECT_EQ(ptr2, str2.get());
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(AlwaysValidPointerTest, NoDefaultObjectPassNullPointer) {
  auto pass_null = []() {
    AlwaysValidPointerNoDefault<std::string> ptr(nullptr);
  };
  EXPECT_DEATH(pass_null(), "");
}

TEST(AlwaysValidPointerTest, NoDefaultObjectPassNullUniquePointer) {
  auto pass_null = []() {
    std::unique_ptr<std::string> str;
    AlwaysValidPointerNoDefault<std::string> ptr(std::move(str));
  };
  EXPECT_DEATH(pass_null(), "");
}

#endif

}  // namespace webrtc
