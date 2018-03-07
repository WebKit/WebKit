/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "rtc_base/gunit.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"

namespace rtc {

namespace {

class A {
 public:
  A() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(A);
};

class RefClass : public RefCountInterface {
 public:
  RefClass() {}

 protected:
  ~RefClass() override {}
};

class RefClassWithRvalue : public RefCountInterface {
 public:
  explicit RefClassWithRvalue(std::unique_ptr<A> a) : a_(std::move(a)) {}

 protected:
  ~RefClassWithRvalue() override {}

 public:
  std::unique_ptr<A> a_;
};

class RefClassWithMixedValues : public RefCountInterface {
 public:
  RefClassWithMixedValues(std::unique_ptr<A> a, int b, const std::string& c)
      : a_(std::move(a)), b_(b), c_(c) {}

 protected:
  ~RefClassWithMixedValues() override {}

 public:
  std::unique_ptr<A> a_;
  int b_;
  std::string c_;
};

}  // namespace

TEST(RefCountedObject, HasOneRef) {
  scoped_refptr<RefCountedObject<RefClass>> aref(
      new RefCountedObject<RefClass>());
  EXPECT_TRUE(aref->HasOneRef());
  aref->AddRef();
  EXPECT_FALSE(aref->HasOneRef());
  EXPECT_EQ(aref->Release(), RefCountReleaseStatus::kOtherRefsRemained);
  EXPECT_TRUE(aref->HasOneRef());
}

TEST(RefCountedObject, SupportRValuesInCtor) {
  std::unique_ptr<A> a(new A());
  scoped_refptr<RefClassWithRvalue> ref(
      new RefCountedObject<RefClassWithRvalue>(std::move(a)));
  EXPECT_TRUE(ref->a_.get() != nullptr);
  EXPECT_TRUE(a.get() == nullptr);
}

TEST(RefCountedObject, SupportMixedTypesInCtor) {
  std::unique_ptr<A> a(new A());
  int b = 9;
  std::string c = "hello";
  scoped_refptr<RefClassWithMixedValues> ref(
      new RefCountedObject<RefClassWithMixedValues>(std::move(a), b, c));
  EXPECT_TRUE(ref->a_.get() != nullptr);
  EXPECT_TRUE(a.get() == nullptr);
  EXPECT_EQ(b, ref->b_);
  EXPECT_EQ(c, ref->c_);
}

}  // namespace rtc
