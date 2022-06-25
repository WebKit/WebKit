/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/untyped_function.h"

#include <memory>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Pointee;

TEST(UntypedFunction, Empty1) {
  UntypedFunction uf;
  EXPECT_FALSE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
}

TEST(UntypedFunction, Empty2) {
  UntypedFunction uf = nullptr;
  EXPECT_FALSE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
}

TEST(UntypedFunction, Empty3) {
  UntypedFunction uf = UntypedFunction::Create<int(int)>(nullptr);
  EXPECT_FALSE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
}

TEST(UntypedFunction, CallTrivialWithInt) {
  auto uf = UntypedFunction::Create<int(int)>([](int x) { return x + 5; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int)>(17), 22);
}

TEST(UntypedFunction, CallTrivialWithPointer) {
  auto uf = UntypedFunction::Create<int(int*)>([](int* x) { return *x; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  int x = 12;
  EXPECT_EQ(uf.Call<int(int*)>(&x), 12);
}

TEST(UntypedFunction, CallTrivialWithReference) {
  auto uf = UntypedFunction::Create<void(int&)>([](int& x) { x = 3; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  int x = 12;
  uf.Call<void(int&)>(x);
  EXPECT_EQ(x, 3);
}

TEST(UntypedFunction, CallTrivialWithRvalueReference) {
  auto uf = UntypedFunction::Create<int(int&&)>([](int&& x) { return x - 2; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int&&)>(34), 32);
}

TEST(UntypedFunction, CallNontrivialWithInt) {
  std::vector<int> list;
  auto uf = UntypedFunction::Create<int(int)>([list](int x) mutable {
    list.push_back(x);
    return list.size();
  });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int)>(17), 1);
  EXPECT_EQ(uf.Call<int(int)>(17), 2);
}

TEST(UntypedFunction, CallNontrivialWithPointer) {
  std::vector<int> list;
  auto uf = UntypedFunction::Create<int*(int*)>([list](int* x) mutable {
    list.push_back(*x);
    return list.data();
  });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  int x = 12;
  EXPECT_THAT(uf.Call<int*(int*)>(&x), Pointee(12));
}

TEST(UntypedFunction, CallNontrivialWithReference) {
  std::vector<int> list = {34, 35, 36};
  auto uf =
      UntypedFunction::Create<void(int&)>([list](int& x) { x = list[1]; });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  int x = 12;
  uf.Call<void(int&)>(x);
  EXPECT_EQ(x, 35);
}

TEST(UntypedFunction, CallNontrivialWithRvalueReference) {
  std::vector<int> list;
  auto uf = UntypedFunction::Create<int(int&&)>([list](int&& x) mutable {
    list.push_back(x);
    return list.size();
  });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int&&)>(34), 1);
  EXPECT_EQ(uf.Call<int(int&&)>(34), 2);
}

int AddFive(int x) {
  return x + 5;
}
int DereferencePointer(int* x) {
  return *x;
}
void AssignThree(int& x) {
  x = 3;
}
int SubtractTwo(int&& x) {
  return x - 2;
}

TEST(UntypedFunction, CallFunctionPointerWithInt) {
  auto uf = UntypedFunction::Create<int(int)>(AddFive);
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int)>(17), 22);
}

TEST(UntypedFunction, CallFunctionPointerWithPointer) {
  auto uf = UntypedFunction::Create<int(int*)>(DereferencePointer);
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  int x = 12;
  EXPECT_EQ(uf.Call<int(int*)>(&x), 12);
}

TEST(UntypedFunction, CallFunctionPointerWithReference) {
  auto uf = UntypedFunction::Create<void(int&)>(AssignThree);
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  int x = 12;
  uf.Call<void(int&)>(x);
  EXPECT_EQ(x, 3);
}

TEST(UntypedFunction, CallFunctionPointerWithRvalueReference) {
  auto uf = UntypedFunction::Create<int(int&&)>(SubtractTwo);
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int(int&&)>(34), 32);
}

TEST(UntypedFunction, CallTrivialWithNoArgs) {
  int arr[] = {1, 2, 3};
  static_assert(sizeof(arr) <= UntypedFunction::kInlineStorageSize, "");
  auto uf = UntypedFunction::Create<int()>([arr] { return arr[1]; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int()>(), 2);
}

TEST(UntypedFunction, CallLargeTrivialWithNoArgs) {
  int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
  static_assert(sizeof(arr) > UntypedFunction::kInlineStorageSize, "");
  auto uf = UntypedFunction::Create<int()>([arr] { return arr[4]; });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int()>(), 5);
}

TEST(UntypedFunction, MoveonlyReturnValue) {
  auto uf = UntypedFunction::Create<std::unique_ptr<int>()>(
      [] { return std::make_unique<int>(567); });
  EXPECT_THAT(uf.Call<std::unique_ptr<int>()>(), Pointee(567));
}

TEST(UntypedFunction, MoveonlyArgument) {
  auto uf = UntypedFunction::Create<int(std::unique_ptr<int>)>(
      [](std::unique_ptr<int> x) { return *x + 19; });
  EXPECT_EQ(uf.Call<int(std::unique_ptr<int>)>(std::make_unique<int>(40)), 59);
}

TEST(UntypedFunction, MoveOnlyCallable) {
  auto uf = UntypedFunction::Create<int()>(
      [x = std::make_unique<int>(17)] { return ++*x; });
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int()>(), 18);
  EXPECT_EQ(uf.Call<int()>(), 19);
  UntypedFunction uf2 = std::move(uf);
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_FALSE(uf2.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int()>(), 20);
  EXPECT_EQ(uf.Call<int()>(), 21);
}

class Destroyer {
 public:
  explicit Destroyer(int& destroy_count) : destroy_count_(&destroy_count) {}
  ~Destroyer() { ++*destroy_count_; }
  int operator()() { return 72; }
  int* destroy_count_;
};

TEST(UntypedFunction, CallableIsDestroyed) {
  int destroy_count = 0;
  {
    auto uf = UntypedFunction::Create<int()>(Destroyer(destroy_count));
    // Destruction count is 1 here, because the temporary we created above was
    // destroyed.
    EXPECT_EQ(destroy_count, 1);
    {
      auto uf2 = std::move(uf);
      EXPECT_EQ(destroy_count, 1);
    }
    // `uf2` was destroyed.
    EXPECT_EQ(destroy_count, 2);
  }
  // `uf` was destroyed, but it didn't contain a Destroyer since we moved it to
  // `uf2` above.
  EXPECT_EQ(destroy_count, 2);
}

TEST(UntypedFunction, MoveAssign) {
  int destroy_count = 0;
  auto uf = UntypedFunction::Create<int()>(Destroyer(destroy_count));
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  // Destruction count is 1 here, because the temporary we created above was
  // destroyed.
  EXPECT_EQ(destroy_count, 1);
  UntypedFunction uf2 = nullptr;
  EXPECT_FALSE(uf2);
  EXPECT_TRUE(uf2.IsTriviallyDestructible());

  uf2 = std::move(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_TRUE(uf2);
  EXPECT_FALSE(uf2.IsTriviallyDestructible());
  EXPECT_EQ(destroy_count, 1);  // The callable was not destroyed.
  EXPECT_EQ(uf2.Call<int()>(), 72);

  UntypedFunction uf3 = nullptr;
  uf2 = std::move(uf3);
  EXPECT_FALSE(uf2);
  EXPECT_TRUE(uf2.IsTriviallyDestructible());
  EXPECT_EQ(destroy_count, 2);  // The callable was destroyed by the assignment.
}

TEST(UntypedFunction, NullptrAssign) {
  int destroy_count = 0;
  auto uf = UntypedFunction::Create<int()>(Destroyer(destroy_count));
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  // Destruction count is 1 here, because the temporary we created above was
  // destroyed.
  EXPECT_EQ(destroy_count, 1);

  uf = nullptr;
  EXPECT_FALSE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_EQ(destroy_count, 2);  // The callable was destroyed by the assignment.
}

TEST(UntypedFunction, Swap) {
  int x = 13;
  auto uf = UntypedFunction::Create<int()>([x]() mutable { return ++x; });
  EXPECT_TRUE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  auto y = std::make_unique<int>(113);
  auto uf2 =
      UntypedFunction::Create<int()>([y = std::move(y)] { return ++*y; });
  EXPECT_TRUE(uf2);
  EXPECT_FALSE(uf2.IsTriviallyDestructible());
  UntypedFunction uf3 = nullptr;
  EXPECT_FALSE(uf3);
  EXPECT_TRUE(uf3.IsTriviallyDestructible());

  EXPECT_EQ(uf.Call<int()>(), 14);
  swap(uf, uf2);
  EXPECT_TRUE(uf);
  EXPECT_FALSE(uf.IsTriviallyDestructible());
  EXPECT_TRUE(uf2);
  EXPECT_TRUE(uf2.IsTriviallyDestructible());
  EXPECT_EQ(uf.Call<int()>(), 114);
  EXPECT_EQ(uf2.Call<int()>(), 15);

  swap(uf, uf3);
  EXPECT_FALSE(uf);
  EXPECT_TRUE(uf.IsTriviallyDestructible());
  EXPECT_TRUE(uf3);
  EXPECT_FALSE(uf3.IsTriviallyDestructible());
  EXPECT_EQ(uf3.Call<int()>(), 115);
}

}  // namespace
}  // namespace webrtc
