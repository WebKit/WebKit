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
#include "rtc_base/callback_list.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(CallbackList, NoRecieverSingleMessageTest) {
  CallbackList<std::string> c;

  c.Send("message");
}

TEST(CallbackList, MultipleParameterMessageTest) {
  CallbackList<const std::string&, std::string, std::string&&, int, int*,
             std::string&>
      c;
  std::string str = "messege";
  int i = 10;

  c.Send(str, "message1", "message0", 123, &i, str);
}

TEST(CallbackList, NoParameterMessageTest) {
  CallbackList<> c;

  c.Send();
}

TEST(CallbackList, ReferenceTest) {
  CallbackList<int&> c;
  int index = 1;

  c.AddReceiver([](int& index) { index++; });
  c.Send(index);

  EXPECT_EQ(index, 2);
}

enum State {
  kNew,
  kChecking,
};

TEST(CallbackList, SingleEnumValueTest) {
  CallbackList<State> c;
  State s1 = kNew;
  int index = 0;

  c.AddReceiver([&index](State s) { index++; });
  c.Send(s1);

  EXPECT_EQ(index, 1);
}

TEST(CallbackList, SingleEnumReferenceTest) {
  CallbackList<State&> c;
  State s = kNew;

  c.AddReceiver([](State& s) { s = kChecking; });
  c.Send(s);

  EXPECT_EQ(s, kChecking);
}

TEST(CallbackList, ConstReferenceTest) {
  CallbackList<int&> c;
  int i = 0;
  int index = 1;

  c.AddReceiver([&i](const int& index) { i = index; });
  c.Send(index);

  EXPECT_EQ(i, 1);
}

TEST(CallbackList, PointerTest) {
  CallbackList<int*> c;
  int index = 1;

  c.AddReceiver([](int* index) { (*index)++; });
  c.Send(&index);

  EXPECT_EQ(index, 2);
}

TEST(CallbackList, CallByValue) {
  CallbackList<int> c;
  int x = 17;

  c.AddReceiver([&x](int n) { x += n; });
  int y = 89;
  c.Send(y);

  EXPECT_EQ(x, 106);
}

void PlusOne(int& a) {
  a++;
}

TEST(CallbackList, FunctionPtrTest) {
  CallbackList<int&> c;
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

  void operator()(int& b) { b = 1; }
};

TEST(CallbackList, LargeNonTrivialTest) {
  CallbackList<int&> c;
  int i = 0;
  static_assert(sizeof(LargeNonTrivial) > UntypedFunction::kInlineStorageSize,
                "");
  c.AddReceiver(LargeNonTrivial());
  c.Send(i);

  EXPECT_EQ(i, 1);
}

struct LargeTrivial {
  int a[17];
  void operator()(int& x) { x = 1; }
};

TEST(CallbackList, LargeTrivial) {
  CallbackList<int&> c;
  LargeTrivial lt;
  int i = 0;

  static_assert(sizeof(lt) > UntypedFunction::kInlineStorageSize, "");
  c.AddReceiver(lt);
  c.Send(i);

  EXPECT_EQ(i, 1);
}

struct OnlyNonTriviallyConstructible {
  OnlyNonTriviallyConstructible() = default;
  OnlyNonTriviallyConstructible(OnlyNonTriviallyConstructible&& m) {}

  void operator()(int& a) { a = 1; }
};

TEST(CallbackList, OnlyNonTriviallyMoveConstructible) {
  CallbackList<int&> c;
  int i = 0;

  c.AddReceiver(OnlyNonTriviallyConstructible());
  c.Send(i);

  EXPECT_EQ(i, 1);
}

TEST(CallbackList, MultipleReceiverSendTest) {
  CallbackList<int&> c;
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

TEST(CallbackList, MemberFunctionTest) {
  CallbackList<int&> c;
  A a;
  int index = 1;

  c.AddReceiver([&a](int& i) { a.increment(i); });
  c.Send(index);

  EXPECT_EQ(index, 2);
}

// todo(glahiru): Add a test case to catch some error for Karl's first fix

TEST(CallbackList, RemoveOneReceiver) {
  int removal_tag[2];
  CallbackList<> c;
  int accumulator = 0;
  c.AddReceiver([&accumulator] { accumulator += 1; });
  c.AddReceiver(&removal_tag[0], [&accumulator] { accumulator += 10; });
  c.AddReceiver(&removal_tag[1], [&accumulator] { accumulator += 100; });
  c.Send();
  EXPECT_EQ(accumulator, 111);
  c.RemoveReceivers(&removal_tag[0]);
  c.Send();
  EXPECT_EQ(accumulator, 212);
}

TEST(CallbackList, RemoveZeroReceivers) {
  int removal_tag[3];
  CallbackList<> c;
  int accumulator = 0;
  c.AddReceiver([&accumulator] { accumulator += 1; });
  c.AddReceiver(&removal_tag[0], [&accumulator] { accumulator += 10; });
  c.AddReceiver(&removal_tag[1], [&accumulator] { accumulator += 100; });
  c.Send();
  EXPECT_EQ(accumulator, 111);
  c.RemoveReceivers(&removal_tag[2]);
  c.Send();
  EXPECT_EQ(accumulator, 222);
}

TEST(CallbackList, RemoveManyReceivers) {
  int removal_tag;
  CallbackList<> c;
  int accumulator = 0;
  c.AddReceiver([&accumulator] { accumulator += 1; });
  c.AddReceiver(&removal_tag, [&accumulator] { accumulator += 10; });
  c.AddReceiver([&accumulator] { accumulator += 100; });
  c.AddReceiver(&removal_tag, [&accumulator] { accumulator += 1000; });
  c.Send();
  EXPECT_EQ(accumulator, 1111);
  c.RemoveReceivers(&removal_tag);
  c.Send();
  EXPECT_EQ(accumulator, 1212);
}

}  // namespace
}  // namespace webrtc
