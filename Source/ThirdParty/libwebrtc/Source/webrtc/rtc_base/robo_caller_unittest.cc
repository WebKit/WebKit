/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string>
#include <type_traits>

#include "api/function_view.h"
#include "rtc_base/bind.h"
#include "rtc_base/robo_caller.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(RoboCaller, NoRecieverSingleMessageTest) {
  RoboCaller<std::string> c;

  c.Send("message");
}

TEST(RoboCaller, MultipleParameterMessageTest) {
  RoboCaller<const std::string&, std::string, std::string&&, int, int*,
             std::string&>
      c;
  std::string str = "messege";
  int i = 10;

  c.Send(str, "message1", "message0", 123, &i, str);
}

TEST(RoboCaller, NoParameterMessageTest) {
  RoboCaller<> c;

  c.Send();
}

TEST(RoboCaller, ReferenceTest) {
  RoboCaller<int&> c;
  int index = 1;

  c.AddReceiver([](int& index) { index++; });
  c.Send(index);

  EXPECT_EQ(index, 2);
}

enum State {
  kNew,
  kChecking,
};

TEST(RoboCaller, SingleEnumValueTest) {
  RoboCaller<State> c;
  State s1 = kNew;
  int index = 0;

  c.AddReceiver([&index](State s) { index++; });
  c.Send(s1);

  EXPECT_EQ(index, 1);
}

TEST(RoboCaller, SingleEnumReferenceTest) {
  RoboCaller<State&> c;
  State s = kNew;

  c.AddReceiver([](State& s) { s = kChecking; });
  c.Send(s);

  EXPECT_EQ(s, kChecking);
}

TEST(RoboCaller, ConstReferenceTest) {
  RoboCaller<int&> c;
  int i = 0;
  int index = 1;

  c.AddReceiver([&i](const int& index) { i = index; });
  c.Send(index);

  EXPECT_EQ(i, 1);
}

TEST(RoboCaller, PointerTest) {
  RoboCaller<int*> c;
  int index = 1;

  c.AddReceiver([](int* index) { (*index)++; });
  c.Send(&index);

  EXPECT_EQ(index, 2);
}

TEST(RoboCaller, CallByValue) {
  RoboCaller<int> c;
  int x = 17;

  c.AddReceiver([&x](int n) { x += n; });
  int y = 89;
  c.Send(y);

  EXPECT_EQ(x, 106);
}

void PlusOne(int& a) {
  a++;
}

TEST(RoboCaller, FunctionPtrTest) {
  RoboCaller<int&> c;
  int index = 1;

  c.AddReceiver(PlusOne);
  c.Send(index);

  EXPECT_EQ(index, 2);
}

struct LargeNonTrivial {
  int a[17];

  LargeNonTrivial() = default;
  LargeNonTrivial(LargeNonTrivial&& m) {}
  ~LargeNonTrivial() = default;

  void operator()(int& a) { a = 1; }
};

TEST(RoboCaller, LargeNonTrivialTest) {
  RoboCaller<int&> c;
  int i = 0;
  static_assert(sizeof(LargeNonTrivial) > 16, "");
  c.AddReceiver(LargeNonTrivial());
  c.Send(i);

  EXPECT_EQ(i, 1);
}

/* sizeof(LargeTrivial) = 20bytes which is greater than
 * the size check (16bytes) of the CSC library */
struct LargeTrivial {
  int a[17];
  void operator()(int& x) { x = 1; }
};

TEST(RoboCaller, LargeTrivial) {
  RoboCaller<int&> c;
  LargeTrivial lt;
  int i = 0;

  static_assert(sizeof(lt) > 16, "");
  c.AddReceiver(lt);
  c.Send(i);

  EXPECT_EQ(i, 1);
}

struct OnlyNonTriviallyConstructible {
  OnlyNonTriviallyConstructible() = default;
  OnlyNonTriviallyConstructible(OnlyNonTriviallyConstructible&& m) {}

  void operator()(int& a) { a = 1; }
};

TEST(RoboCaller, OnlyNonTriviallyMoveConstructible) {
  RoboCaller<int&> c;
  int i = 0;

  c.AddReceiver(OnlyNonTriviallyConstructible());
  c.Send(i);

  EXPECT_EQ(i, 1);
}

TEST(RoboCaller, MultipleReceiverSendTest) {
  RoboCaller<int&> c;
  std::function<void(int&)> plus = PlusOne;
  int index = 1;

  c.AddReceiver(plus);
  c.AddReceiver([](int& i) { i--; });
  c.AddReceiver(plus);
  c.AddReceiver(plus);
  c.Send(index);
  c.Send(index);

  EXPECT_EQ(index, 5);
}

class A {
 public:
  void increment(int& i) const { i++; }
};

TEST(RoboCaller, MemberFunctionTest) {
  RoboCaller<int&> c;
  A a;
  int index = 1;

  c.AddReceiver([&a](int& i) { a.increment(i); });
  c.Send(index);

  EXPECT_EQ(index, 2);
}
// todo(glahiru): Add a test case to catch some error for Karl's first fix
// todo(glahiru): Add a test for rtc::Bind
// which used the following code in the Send
}  // namespace
}  // namespace webrtc
