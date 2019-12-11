/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/proxy.h"

#include <memory>
#include <string>

#include "rtc_base/gunit.h"
#include "rtc_base/ref_count.h"
#include "test/gmock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace webrtc {

// Interface used for testing here.
class FakeInterface : public rtc::RefCountInterface {
 public:
  virtual void VoidMethod0() = 0;
  virtual std::string Method0() = 0;
  virtual std::string ConstMethod0() const = 0;
  virtual std::string Method1(std::string s) = 0;
  virtual std::string ConstMethod1(std::string s) const = 0;
  virtual std::string Method2(std::string s1, std::string s2) = 0;

 protected:
  virtual ~FakeInterface() {}
};

// Implementation of the test interface.
class Fake : public FakeInterface {
 public:
  static rtc::scoped_refptr<Fake> Create() {
    return new rtc::RefCountedObject<Fake>();
  }
  // Used to verify destructor is called on the correct thread.
  MOCK_METHOD0(Destroy, void());

  MOCK_METHOD0(VoidMethod0, void());
  MOCK_METHOD0(Method0, std::string());
  MOCK_CONST_METHOD0(ConstMethod0, std::string());

  MOCK_METHOD1(Method1, std::string(std::string));
  MOCK_CONST_METHOD1(ConstMethod1, std::string(std::string));

  MOCK_METHOD2(Method2, std::string(std::string, std::string));

 protected:
  Fake() {}
  ~Fake() { Destroy(); }
};

// Proxies for the test interface.
BEGIN_PROXY_MAP(Fake)
PROXY_WORKER_THREAD_DESTRUCTOR()
PROXY_METHOD0(void, VoidMethod0)
PROXY_METHOD0(std::string, Method0)
PROXY_CONSTMETHOD0(std::string, ConstMethod0)
PROXY_WORKER_METHOD1(std::string, Method1, std::string)
PROXY_CONSTMETHOD1(std::string, ConstMethod1, std::string)
PROXY_WORKER_METHOD2(std::string, Method2, std::string, std::string)
END_PROXY_MAP()

// Preprocessor hack to get a proxy class a name different than FakeProxy.
#define FakeProxy FakeSignalingProxy
#define FakeProxyWithInternal FakeSignalingProxyWithInternal
BEGIN_SIGNALING_PROXY_MAP(Fake)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_METHOD0(void, VoidMethod0)
PROXY_METHOD0(std::string, Method0)
PROXY_CONSTMETHOD0(std::string, ConstMethod0)
PROXY_METHOD1(std::string, Method1, std::string)
PROXY_CONSTMETHOD1(std::string, ConstMethod1, std::string)
PROXY_METHOD2(std::string, Method2, std::string, std::string)
END_PROXY_MAP()
#undef FakeProxy

class SignalingProxyTest : public ::testing::Test {
 public:
  // Checks that the functions are called on the right thread.
  void CheckSignalingThread() { EXPECT_TRUE(signaling_thread_->IsCurrent()); }

 protected:
  void SetUp() override {
    signaling_thread_ = rtc::Thread::Create();
    ASSERT_TRUE(signaling_thread_->Start());
    fake_ = Fake::Create();
    fake_signaling_proxy_ =
        FakeSignalingProxy::Create(signaling_thread_.get(), fake_.get());
  }

 protected:
  std::unique_ptr<rtc::Thread> signaling_thread_;
  rtc::scoped_refptr<FakeInterface> fake_signaling_proxy_;
  rtc::scoped_refptr<Fake> fake_;
};

TEST_F(SignalingProxyTest, SignalingThreadDestructor) {
  EXPECT_CALL(*fake_, Destroy())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread));
  fake_ = nullptr;
  fake_signaling_proxy_ = nullptr;
}

TEST_F(SignalingProxyTest, VoidMethod0) {
  EXPECT_CALL(*fake_, VoidMethod0())
      .Times(Exactly(1))
      .WillOnce(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread));
  fake_signaling_proxy_->VoidMethod0();
}

TEST_F(SignalingProxyTest, Method0) {
  EXPECT_CALL(*fake_, Method0())
      .Times(Exactly(1))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread),
          Return("Method0")));
  EXPECT_EQ("Method0", fake_signaling_proxy_->Method0());
}

TEST_F(SignalingProxyTest, ConstMethod0) {
  EXPECT_CALL(*fake_, ConstMethod0())
      .Times(Exactly(1))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread),
          Return("ConstMethod0")));
  EXPECT_EQ("ConstMethod0", fake_signaling_proxy_->ConstMethod0());
}

TEST_F(SignalingProxyTest, Method1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, Method1(arg1))
      .Times(Exactly(1))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread),
          Return("Method1")));
  EXPECT_EQ("Method1", fake_signaling_proxy_->Method1(arg1));
}

TEST_F(SignalingProxyTest, ConstMethod1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, ConstMethod1(arg1))
      .Times(Exactly(1))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread),
          Return("ConstMethod1")));
  EXPECT_EQ("ConstMethod1", fake_signaling_proxy_->ConstMethod1(arg1));
}

TEST_F(SignalingProxyTest, Method2) {
  const std::string arg1 = "arg1";
  const std::string arg2 = "arg2";
  EXPECT_CALL(*fake_, Method2(arg1, arg2))
      .Times(Exactly(1))
      .WillOnce(DoAll(
          InvokeWithoutArgs(this, &SignalingProxyTest::CheckSignalingThread),
          Return("Method2")));
  EXPECT_EQ("Method2", fake_signaling_proxy_->Method2(arg1, arg2));
}

class ProxyTest : public ::testing::Test {
 public:
  // Checks that the functions are called on the right thread.
  void CheckSignalingThread() { EXPECT_TRUE(signaling_thread_->IsCurrent()); }
  void CheckWorkerThread() { EXPECT_TRUE(worker_thread_->IsCurrent()); }

 protected:
  void SetUp() override {
    signaling_thread_ = rtc::Thread::Create();
    worker_thread_ = rtc::Thread::Create();
    ASSERT_TRUE(signaling_thread_->Start());
    ASSERT_TRUE(worker_thread_->Start());
    fake_ = Fake::Create();
    fake_proxy_ = FakeProxy::Create(signaling_thread_.get(),
                                    worker_thread_.get(), fake_.get());
  }

 protected:
  std::unique_ptr<rtc::Thread> signaling_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_refptr<FakeInterface> fake_proxy_;
  rtc::scoped_refptr<Fake> fake_;
};

TEST_F(ProxyTest, WorkerThreadDestructor) {
  EXPECT_CALL(*fake_, Destroy())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &ProxyTest::CheckWorkerThread));
  fake_ = nullptr;
  fake_proxy_ = nullptr;
}

TEST_F(ProxyTest, VoidMethod0) {
  EXPECT_CALL(*fake_, VoidMethod0())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &ProxyTest::CheckSignalingThread));
  fake_proxy_->VoidMethod0();
}

TEST_F(ProxyTest, Method0) {
  EXPECT_CALL(*fake_, Method0())
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckSignalingThread),
                      Return("Method0")));
  EXPECT_EQ("Method0", fake_proxy_->Method0());
}

TEST_F(ProxyTest, ConstMethod0) {
  EXPECT_CALL(*fake_, ConstMethod0())
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckSignalingThread),
                      Return("ConstMethod0")));
  EXPECT_EQ("ConstMethod0", fake_proxy_->ConstMethod0());
}

TEST_F(ProxyTest, WorkerMethod1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, Method1(arg1))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckWorkerThread),
                      Return("Method1")));
  EXPECT_EQ("Method1", fake_proxy_->Method1(arg1));
}

TEST_F(ProxyTest, ConstMethod1) {
  const std::string arg1 = "arg1";
  EXPECT_CALL(*fake_, ConstMethod1(arg1))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckSignalingThread),
                      Return("ConstMethod1")));
  EXPECT_EQ("ConstMethod1", fake_proxy_->ConstMethod1(arg1));
}

TEST_F(ProxyTest, WorkerMethod2) {
  const std::string arg1 = "arg1";
  const std::string arg2 = "arg2";
  EXPECT_CALL(*fake_, Method2(arg1, arg2))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ProxyTest::CheckWorkerThread),
                      Return("Method2")));
  EXPECT_EQ("Method2", fake_proxy_->Method2(arg1, arg2));
}

// Interface for testing OWNED_PROXY_MAP.
class FooInterface {
 public:
  virtual ~FooInterface() {}
  virtual void Bar() = 0;
};

class Foo : public FooInterface {
 public:
  Foo() {}
  MOCK_METHOD0(Bar, void());
};

BEGIN_OWNED_PROXY_MAP(Foo)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_METHOD0(void, Bar)
END_PROXY_MAP()

class OwnedProxyTest : public ::testing::Test {
 public:
  OwnedProxyTest()
      : signaling_thread_(rtc::Thread::Create()),
        worker_thread_(rtc::Thread::Create()),
        foo_(new Foo()),
        foo_proxy_(FooProxy::Create(signaling_thread_.get(),
                                    worker_thread_.get(),
                                    std::unique_ptr<FooInterface>(foo_))) {
    signaling_thread_->Start();
    worker_thread_->Start();
  }

  void CheckSignalingThread() { EXPECT_TRUE(signaling_thread_->IsCurrent()); }
  void CheckWorkerThread() { EXPECT_TRUE(worker_thread_->IsCurrent()); }

 protected:
  std::unique_ptr<rtc::Thread> signaling_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  Foo* foo_;  // Owned by foo_proxy_, not this class.
  std::unique_ptr<FooInterface> foo_proxy_;
};

// Just tests that a method can be invoked using an "owned proxy" (as opposed
// to normal ref-counted version).
TEST_F(OwnedProxyTest, BasicTest) {
  EXPECT_CALL(*foo_, Bar())
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &OwnedProxyTest::CheckSignalingThread));
  foo_proxy_->Bar();
}

}  // namespace webrtc
