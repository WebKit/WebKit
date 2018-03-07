/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace rtc {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

template <typename T>
void Call(ArrayView<T>) {}

}  // namespace

TEST(ArrayViewTest, TestConstructFromPtrAndArray) {
  char arr[] = "Arrr!";
  const char carr[] = "Carrr!";
  Call<const char>(arr);
  Call<const char>(carr);
  Call<char>(arr);
  // Call<char>(carr);  // Compile error, because can't drop const.
  // Call<int>(arr);  // Compile error, because incompatible types.
  ArrayView<int*> x;
  EXPECT_EQ(0u, x.size());
  EXPECT_EQ(nullptr, x.data());
  ArrayView<char> y = arr;
  EXPECT_EQ(6u, y.size());
  EXPECT_EQ(arr, y.data());
  ArrayView<char, 6> yf = arr;
  static_assert(yf.size() == 6, "");
  EXPECT_EQ(arr, yf.data());
  ArrayView<const char> z(arr + 1, 3);
  EXPECT_EQ(3u, z.size());
  EXPECT_EQ(arr + 1, z.data());
  ArrayView<const char, 3> zf(arr + 1, 3);
  static_assert(zf.size() == 3, "");
  EXPECT_EQ(arr + 1, zf.data());
  ArrayView<const char> w(arr, 2);
  EXPECT_EQ(2u, w.size());
  EXPECT_EQ(arr, w.data());
  ArrayView<const char, 2> wf(arr, 2);
  static_assert(wf.size() == 2, "");
  EXPECT_EQ(arr, wf.data());
  ArrayView<char> q(arr, 0);
  EXPECT_EQ(0u, q.size());
  EXPECT_EQ(nullptr, q.data());
  ArrayView<char, 0> qf(arr, 0);
  static_assert(qf.size() == 0, "");
  EXPECT_EQ(nullptr, qf.data());
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
  // DCHECK error (nullptr with nonzero size).
  EXPECT_DEATH(ArrayView<int>(static_cast<int*>(nullptr), 5), "");
#endif
  // These are compile errors, because incompatible types.
  // ArrayView<int> m = arr;
  // ArrayView<float> n(arr + 2, 2);
}

TEST(ArrayViewTest, TestCopyConstructorVariable) {
  char arr[] = "Arrr!";
  ArrayView<char> x = arr;
  EXPECT_EQ(6u, x.size());
  EXPECT_EQ(arr, x.data());
  ArrayView<char> y = x;  // Copy non-const -> non-const.
  EXPECT_EQ(6u, y.size());
  EXPECT_EQ(arr, y.data());
  ArrayView<const char> z = x;  // Copy non-const -> const.
  EXPECT_EQ(6u, z.size());
  EXPECT_EQ(arr, z.data());
  ArrayView<const char> w = z;  // Copy const -> const.
  EXPECT_EQ(6u, w.size());
  EXPECT_EQ(arr, w.data());
  // ArrayView<char> v = z;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestCopyConstructorFixed) {
  char arr[] = "Arrr!";
  ArrayView<char, 6> x = arr;
  static_assert(x.size() == 6, "");
  EXPECT_EQ(arr, x.data());

  // Copy fixed -> fixed.
  ArrayView<char, 6> y = x;  // Copy non-const -> non-const.
  static_assert(y.size() == 6, "");
  EXPECT_EQ(arr, y.data());
  ArrayView<const char, 6> z = x;  // Copy non-const -> const.
  static_assert(z.size() == 6, "");
  EXPECT_EQ(arr, z.data());
  ArrayView<const char, 6> w = z;  // Copy const -> const.
  static_assert(w.size() == 6, "");
  EXPECT_EQ(arr, w.data());
  // ArrayView<char, 6> v = z;  // Compile error, because can't drop const.

  // Copy fixed -> variable.
  ArrayView<char> yv = x;  // Copy non-const -> non-const.
  EXPECT_EQ(6u, yv.size());
  EXPECT_EQ(arr, yv.data());
  ArrayView<const char> zv = x;  // Copy non-const -> const.
  EXPECT_EQ(6u, zv.size());
  EXPECT_EQ(arr, zv.data());
  ArrayView<const char> wv = z;  // Copy const -> const.
  EXPECT_EQ(6u, wv.size());
  EXPECT_EQ(arr, wv.data());
  // ArrayView<char> vv = z;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestCopyAssignmentVariable) {
  char arr[] = "Arrr!";
  ArrayView<char> x(arr);
  EXPECT_EQ(6u, x.size());
  EXPECT_EQ(arr, x.data());
  ArrayView<char> y;
  y = x;  // Copy non-const -> non-const.
  EXPECT_EQ(6u, y.size());
  EXPECT_EQ(arr, y.data());
  ArrayView<const char> z;
  z = x;  // Copy non-const -> const.
  EXPECT_EQ(6u, z.size());
  EXPECT_EQ(arr, z.data());
  ArrayView<const char> w;
  w = z;  // Copy const -> const.
  EXPECT_EQ(6u, w.size());
  EXPECT_EQ(arr, w.data());
  // ArrayView<char> v;
  // v = z;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestCopyAssignmentFixed) {
  char arr[] = "Arrr!";
  char init[] = "Init!";
  ArrayView<char, 6> x(arr);
  EXPECT_EQ(arr, x.data());

  // Copy fixed -> fixed.
  ArrayView<char, 6> y(init);
  y = x;  // Copy non-const -> non-const.
  EXPECT_EQ(arr, y.data());
  ArrayView<const char, 6> z(init);
  z = x;  // Copy non-const -> const.
  EXPECT_EQ(arr, z.data());
  ArrayView<const char, 6> w(init);
  w = z;  // Copy const -> const.
  EXPECT_EQ(arr, w.data());
  // ArrayView<char, 6> v(init);
  // v = z;  // Compile error, because can't drop const.

  // Copy fixed -> variable.
  ArrayView<char> yv;
  yv = x;  // Copy non-const -> non-const.
  EXPECT_EQ(6u, yv.size());
  EXPECT_EQ(arr, yv.data());
  ArrayView<const char> zv;
  zv = x;  // Copy non-const -> const.
  EXPECT_EQ(6u, zv.size());
  EXPECT_EQ(arr, zv.data());
  ArrayView<const char> wv;
  wv = z;  // Copy const -> const.
  EXPECT_EQ(6u, wv.size());
  EXPECT_EQ(arr, wv.data());
  // ArrayView<char> v;
  // v = z;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestStdVector) {
  std::vector<int> v;
  v.push_back(3);
  v.push_back(11);
  Call<const int>(v);
  Call<int>(v);
  // Call<unsigned int>(v);  // Compile error, because incompatible types.
  ArrayView<int> x = v;
  EXPECT_EQ(2u, x.size());
  EXPECT_EQ(v.data(), x.data());
  ArrayView<const int> y;
  y = v;
  EXPECT_EQ(2u, y.size());
  EXPECT_EQ(v.data(), y.data());
  // ArrayView<double> d = v;  // Compile error, because incompatible types.
  const std::vector<int> cv;
  Call<const int>(cv);
  // Call<int>(cv);  // Compile error, because can't drop const.
  ArrayView<const int> z = cv;
  EXPECT_EQ(0u, z.size());
  EXPECT_EQ(nullptr, z.data());
  // ArrayView<int> w = cv;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestRtcBuffer) {
  rtc::Buffer b = "so buffer";
  Call<const uint8_t>(b);
  Call<uint8_t>(b);
  // Call<int8_t>(b);  // Compile error, because incompatible types.
  ArrayView<uint8_t> x = b;
  EXPECT_EQ(10u, x.size());
  EXPECT_EQ(b.data(), x.data());
  ArrayView<const uint8_t> y;
  y = b;
  EXPECT_EQ(10u, y.size());
  EXPECT_EQ(b.data(), y.data());
  // ArrayView<char> d = b;  // Compile error, because incompatible types.
  const rtc::Buffer cb = "very const";
  Call<const uint8_t>(cb);
  // Call<uint8_t>(cb);  // Compile error, because can't drop const.
  ArrayView<const uint8_t> z = cb;
  EXPECT_EQ(11u, z.size());
  EXPECT_EQ(cb.data(), z.data());
  // ArrayView<uint8_t> w = cb;  // Compile error, because can't drop const.
}

TEST(ArrayViewTest, TestSwapVariable) {
  const char arr[] = "Arrr!";
  const char aye[] = "Aye, Cap'n!";
  ArrayView<const char> x(arr);
  EXPECT_EQ(6u, x.size());
  EXPECT_EQ(arr, x.data());
  ArrayView<const char> y(aye);
  EXPECT_EQ(12u, y.size());
  EXPECT_EQ(aye, y.data());
  using std::swap;
  swap(x, y);
  EXPECT_EQ(12u, x.size());
  EXPECT_EQ(aye, x.data());
  EXPECT_EQ(6u, y.size());
  EXPECT_EQ(arr, y.data());
  // ArrayView<char> z;
  // swap(x, z);  // Compile error, because can't drop const.
}

TEST(FixArrayViewTest, TestSwapFixed) {
  const char arr[] = "Arr!";
  char aye[] = "Aye!";
  ArrayView<const char, 5> x(arr);
  EXPECT_EQ(arr, x.data());
  ArrayView<const char, 5> y(aye);
  EXPECT_EQ(aye, y.data());
  using std::swap;
  swap(x, y);
  EXPECT_EQ(aye, x.data());
  EXPECT_EQ(arr, y.data());
  // ArrayView<char, 5> z(aye);
  // swap(x, z);  // Compile error, because can't drop const.
  // ArrayView<const char, 4> w(aye, 4);
  // swap(x, w);  // Compile error, because different sizes.
}

TEST(ArrayViewTest, TestIndexing) {
  char arr[] = "abcdefg";
  ArrayView<char> x(arr);
  const ArrayView<char> y(arr);
  ArrayView<const char, 8> z(arr);
  EXPECT_EQ(8u, x.size());
  EXPECT_EQ(8u, y.size());
  EXPECT_EQ(8u, z.size());
  EXPECT_EQ('b', x[1]);
  EXPECT_EQ('c', y[2]);
  EXPECT_EQ('d', z[3]);
  x[3] = 'X';
  y[2] = 'Y';
  // z[1] = 'Z';  // Compile error, because z's element type is const char.
  EXPECT_EQ('b', x[1]);
  EXPECT_EQ('Y', y[2]);
  EXPECT_EQ('X', z[3]);
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
  EXPECT_DEATH(z[8], "");  // DCHECK error (index out of bounds).
#endif
}

TEST(ArrayViewTest, TestIterationEmpty) {
  // Variable-size.
  ArrayView<std::vector<std::vector<std::vector<std::string>>>> av;
  EXPECT_EQ(av.begin(), av.end());
  EXPECT_EQ(av.cbegin(), av.cend());
  for (auto& e : av) {
    EXPECT_TRUE(false);
    EXPECT_EQ(42u, e.size());  // Dummy use of e to prevent unused var warning.
  }

  // Fixed-size.
  ArrayView<std::vector<std::vector<std::vector<std::string>>>, 0> af;
  EXPECT_EQ(af.begin(), af.end());
  EXPECT_EQ(af.cbegin(), af.cend());
  for (auto& e : af) {
    EXPECT_TRUE(false);
    EXPECT_EQ(42u, e.size());  // Dummy use of e to prevent unused var warning.
  }
}

TEST(ArrayViewTest, TestIterationVariable) {
  char arr[] = "Arrr!";
  ArrayView<char> av(arr);
  EXPECT_EQ('A', *av.begin());
  EXPECT_EQ('A', *av.cbegin());
  EXPECT_EQ('\0', *(av.end() - 1));
  EXPECT_EQ('\0', *(av.cend() - 1));
  char i = 0;
  for (auto& e : av) {
    EXPECT_EQ(arr + i, &e);
    e = 's' + i;
    ++i;
  }
  i = 0;
  for (auto& e : ArrayView<const char>(av)) {
    EXPECT_EQ(arr + i, &e);
    // e = 'q' + i;  // Compile error, because e is a const char&.
    ++i;
  }
}

TEST(ArrayViewTest, TestIterationFixed) {
  char arr[] = "Arrr!";
  ArrayView<char, 6> av(arr);
  EXPECT_EQ('A', *av.begin());
  EXPECT_EQ('A', *av.cbegin());
  EXPECT_EQ('\0', *(av.end() - 1));
  EXPECT_EQ('\0', *(av.cend() - 1));
  char i = 0;
  for (auto& e : av) {
    EXPECT_EQ(arr + i, &e);
    e = 's' + i;
    ++i;
  }
  i = 0;
  for (auto& e : ArrayView<const char, 6>(av)) {
    EXPECT_EQ(arr + i, &e);
    // e = 'q' + i;  // Compile error, because e is a const char&.
    ++i;
  }
}

TEST(ArrayViewTest, TestEmpty) {
  EXPECT_TRUE(ArrayView<int>().empty());
  const int a[] = {1, 2, 3};
  EXPECT_FALSE(ArrayView<const int>(a).empty());

  static_assert(ArrayView<int, 0>::empty(), "");
  static_assert(!ArrayView<int, 3>::empty(), "");
}

TEST(ArrayViewTest, TestCompare) {
  int a[] = {1, 2, 3};
  int b[] = {1, 2, 3};

  EXPECT_EQ(ArrayView<int>(a), ArrayView<int>(a));
  EXPECT_EQ((ArrayView<int, 3>(a)), (ArrayView<int, 3>(a)));
  EXPECT_EQ(ArrayView<int>(a), (ArrayView<int, 3>(a)));
  EXPECT_EQ(ArrayView<int>(), ArrayView<int>());
  EXPECT_EQ(ArrayView<int>(), ArrayView<int>(a, 0));
  EXPECT_EQ(ArrayView<int>(a, 0), ArrayView<int>(b, 0));
  EXPECT_EQ((ArrayView<int, 0>(a, 0)), ArrayView<int>());

  EXPECT_NE(ArrayView<int>(a), ArrayView<int>(b));
  EXPECT_NE((ArrayView<int, 3>(a)), (ArrayView<int, 3>(b)));
  EXPECT_NE((ArrayView<int, 3>(a)), ArrayView<int>(b));
  EXPECT_NE(ArrayView<int>(a), ArrayView<int>());
  EXPECT_NE(ArrayView<int>(a), ArrayView<int>(a, 2));
  EXPECT_NE((ArrayView<int, 3>(a)), (ArrayView<int, 2>(a, 2)));
}

TEST(ArrayViewTest, TestSubViewVariable) {
  int a[] = {1, 2, 3};
  ArrayView<int> av(a);

  EXPECT_EQ(av.subview(0), av);

  EXPECT_THAT(av.subview(1), ElementsAre(2, 3));
  EXPECT_THAT(av.subview(2), ElementsAre(3));
  EXPECT_THAT(av.subview(3), IsEmpty());
  EXPECT_THAT(av.subview(4), IsEmpty());

  EXPECT_THAT(av.subview(1, 0), IsEmpty());
  EXPECT_THAT(av.subview(1, 1), ElementsAre(2));
  EXPECT_THAT(av.subview(1, 2), ElementsAre(2, 3));
  EXPECT_THAT(av.subview(1, 3), ElementsAre(2, 3));
}

TEST(ArrayViewTest, TestSubViewFixed) {
  int a[] = {1, 2, 3};
  ArrayView<int, 3> av(a);

  EXPECT_EQ(av.subview(0), av);

  EXPECT_THAT(av.subview(1), ElementsAre(2, 3));
  EXPECT_THAT(av.subview(2), ElementsAre(3));
  EXPECT_THAT(av.subview(3), IsEmpty());
  EXPECT_THAT(av.subview(4), IsEmpty());

  EXPECT_THAT(av.subview(1, 0), IsEmpty());
  EXPECT_THAT(av.subview(1, 1), ElementsAre(2));
  EXPECT_THAT(av.subview(1, 2), ElementsAre(2, 3));
  EXPECT_THAT(av.subview(1, 3), ElementsAre(2, 3));
}

}  // namespace rtc
