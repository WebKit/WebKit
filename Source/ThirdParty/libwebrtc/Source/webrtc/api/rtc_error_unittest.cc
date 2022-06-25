/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_error.h"

#include <utility>

#include "test/gtest.h"

namespace {

const int kDefaultMoveOnlyIntValue = 0xbadf00d;

// Class that has no copy constructor, ensuring that RTCErrorOr can
struct MoveOnlyInt {
  MoveOnlyInt() {}
  explicit MoveOnlyInt(int value) : value(value) {}
  MoveOnlyInt(const MoveOnlyInt& other) = delete;
  MoveOnlyInt& operator=(const MoveOnlyInt& other) = delete;
  MoveOnlyInt(MoveOnlyInt&& other) : value(other.value) {}
  MoveOnlyInt& operator=(MoveOnlyInt&& other) {
    value = other.value;
    return *this;
  }

  int value = kDefaultMoveOnlyIntValue;
};

// Same as above. Used to test conversion from RTCErrorOr<A> to RTCErrorOr<B>
// when A can be converted to B.
struct MoveOnlyInt2 {
  MoveOnlyInt2() {}
  explicit MoveOnlyInt2(int value) : value(value) {}
  MoveOnlyInt2(const MoveOnlyInt2& other) = delete;
  MoveOnlyInt2& operator=(const MoveOnlyInt2& other) = delete;
  MoveOnlyInt2(MoveOnlyInt2&& other) : value(other.value) {}
  MoveOnlyInt2& operator=(MoveOnlyInt2&& other) {
    value = other.value;
    return *this;
  }

  explicit MoveOnlyInt2(MoveOnlyInt&& other) : value(other.value) {}
  MoveOnlyInt2& operator=(MoveOnlyInt&& other) {
    value = other.value;
    return *this;
  }

  int value = kDefaultMoveOnlyIntValue;
};

}  // namespace

namespace webrtc {

// Test that the default constructor creates a "no error" error.
TEST(RTCErrorTest, DefaultConstructor) {
  RTCError e;
  EXPECT_EQ(RTCErrorType::NONE, e.type());
  EXPECT_EQ(std::string(), e.message());
  EXPECT_TRUE(e.ok());
}

TEST(RTCErrorTest, NormalConstructors) {
  RTCError a(RTCErrorType::INVALID_PARAMETER);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, a.type());
  EXPECT_EQ(std::string(), a.message());

  // Constructor that takes const char* message.
  RTCError b(RTCErrorType::UNSUPPORTED_PARAMETER, "foobar");
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER, b.type());
  EXPECT_EQ(std::string("foobar"), b.message());

  // Constructor that takes std::string message.
  RTCError c(RTCErrorType::INVALID_RANGE, std::string("new"));
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, c.type());
  EXPECT_EQ(std::string("new"), c.message());
}

TEST(RTCErrorTest, MoveConstructor) {
  // Static string.
  RTCError a(RTCErrorType::INVALID_PARAMETER, "foo");
  RTCError b(std::move(a));
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, b.type());
  EXPECT_EQ(std::string("foo"), b.message());

  // Non-static string.
  RTCError c(RTCErrorType::UNSUPPORTED_PARAMETER, std::string("bar"));
  RTCError d(std::move(c));
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER, d.type());
  EXPECT_EQ(std::string("bar"), d.message());
}

TEST(RTCErrorTest, MoveAssignment) {
  // Try all combinations of "is static string"/"is non-static string" moves.
  RTCError e(RTCErrorType::INVALID_PARAMETER, "foo");

  e = RTCError(RTCErrorType::UNSUPPORTED_PARAMETER, "bar");
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER, e.type());
  EXPECT_EQ(std::string("bar"), e.message());

  e = RTCError(RTCErrorType::SYNTAX_ERROR, std::string("baz"));
  EXPECT_EQ(std::string("baz"), e.message());

  e = RTCError(RTCErrorType::SYNTAX_ERROR, std::string("another"));
  EXPECT_EQ(std::string("another"), e.message());

  e = RTCError(RTCErrorType::SYNTAX_ERROR, "last");
  EXPECT_EQ(std::string("last"), e.message());
}

// Test that the error returned by RTCError::OK() is a "no error" error.
TEST(RTCErrorTest, OKConstant) {
  RTCError ok = RTCError::OK();
  EXPECT_EQ(RTCErrorType::NONE, ok.type());
  EXPECT_EQ(std::string(), ok.message());
  EXPECT_TRUE(ok.ok());
}

// Test that "error.ok()" behaves as expected.
TEST(RTCErrorTest, OkMethod) {
  RTCError success;
  RTCError failure(RTCErrorType::INTERNAL_ERROR);
  EXPECT_TRUE(success.ok());
  EXPECT_FALSE(failure.ok());
}

// Test that a message can be set using either static const strings or
// std::strings.
TEST(RTCErrorTest, SetMessage) {
  RTCError e;
  // Try all combinations of "is static string"/"is non-static string" calls.
  e.set_message("foo");
  EXPECT_EQ(std::string("foo"), e.message());

  e.set_message("bar");
  EXPECT_EQ(std::string("bar"), e.message());

  e.set_message(std::string("string"));
  EXPECT_EQ(std::string("string"), e.message());

  e.set_message(std::string("more"));
  EXPECT_EQ(std::string("more"), e.message());

  e.set_message("love to test");
  EXPECT_EQ(std::string("love to test"), e.message());
}

// Test that the default constructor creates an "INTERNAL_ERROR".
TEST(RTCErrorOrTest, DefaultConstructor) {
  RTCErrorOr<MoveOnlyInt> e;
  EXPECT_EQ(RTCErrorType::INTERNAL_ERROR, e.error().type());
}

// Test that an RTCErrorOr can be implicitly constructed from a value.
TEST(RTCErrorOrTest, ImplicitValueConstructor) {
  RTCErrorOr<MoveOnlyInt> e = [] { return MoveOnlyInt(100); }();
  EXPECT_EQ(100, e.value().value);
}

// Test that an RTCErrorOr can be implicitly constructed from an RTCError.
TEST(RTCErrorOrTest, ImplicitErrorConstructor) {
  RTCErrorOr<MoveOnlyInt> e = [] {
    return RTCError(RTCErrorType::SYNTAX_ERROR);
  }();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, e.error().type());
}

TEST(RTCErrorOrTest, MoveConstructor) {
  RTCErrorOr<MoveOnlyInt> a(MoveOnlyInt(5));
  RTCErrorOr<MoveOnlyInt> b(std::move(a));
  EXPECT_EQ(5, b.value().value);
}

TEST(RTCErrorOrTest, MoveAssignment) {
  RTCErrorOr<MoveOnlyInt> a(MoveOnlyInt(5));
  RTCErrorOr<MoveOnlyInt> b(MoveOnlyInt(10));
  a = std::move(b);
  EXPECT_EQ(10, a.value().value);
}

TEST(RTCErrorOrTest, ConversionConstructor) {
  RTCErrorOr<MoveOnlyInt> a(MoveOnlyInt(1));
  RTCErrorOr<MoveOnlyInt2> b(std::move(a));
}

TEST(RTCErrorOrTest, ConversionAssignment) {
  RTCErrorOr<MoveOnlyInt> a(MoveOnlyInt(5));
  RTCErrorOr<MoveOnlyInt2> b(MoveOnlyInt2(10));
  b = std::move(a);
  EXPECT_EQ(5, b.value().value);
}

TEST(RTCErrorOrTest, OkMethod) {
  RTCErrorOr<int> success(1337);
  RTCErrorOr<int> error = RTCError(RTCErrorType::INTERNAL_ERROR);
  EXPECT_TRUE(success.ok());
  EXPECT_FALSE(error.ok());
}

TEST(RTCErrorOrTest, MoveError) {
  RTCErrorOr<int> e({RTCErrorType::SYNTAX_ERROR, "message"});
  RTCError err = e.MoveError();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, err.type());
  EXPECT_EQ(std::string("message"), err.message());
}

TEST(RTCErrorOrTest, MoveValue) {
  RTCErrorOr<MoveOnlyInt> e(MoveOnlyInt(88));
  MoveOnlyInt value = e.MoveValue();
  EXPECT_EQ(88, value.value);
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(RTCErrorOrDeathTest, ConstructWithOkError) {
  RTCErrorOr<int> err;
  EXPECT_DEATH(err = RTCError::OK(), "");
}

TEST(RTCErrorOrDeathTest, DereferenceErrorValue) {
  RTCErrorOr<int> error = RTCError(RTCErrorType::INTERNAL_ERROR);
  EXPECT_DEATH(error.value(), "");
}

TEST(RTCErrorOrDeathTest, MoveErrorValue) {
  RTCErrorOr<int> error = RTCError(RTCErrorType::INTERNAL_ERROR);
  EXPECT_DEATH(error.MoveValue(), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
