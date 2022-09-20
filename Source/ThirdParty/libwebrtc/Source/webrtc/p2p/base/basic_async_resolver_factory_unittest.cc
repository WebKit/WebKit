/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_async_resolver_factory.h"

#include "api/test/mock_async_dns_resolver.h"
#include "p2p/base/mock_async_resolver.h"
#include "rtc_base/async_resolver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

namespace webrtc {

class BasicAsyncResolverFactoryTest : public ::testing::Test,
                                      public sigslot::has_slots<> {
 public:
  void TestCreate() {
    BasicAsyncResolverFactory factory;
    rtc::AsyncResolverInterface* resolver = factory.Create();
    ASSERT_TRUE(resolver);
    resolver->SignalDone.connect(
        this, &BasicAsyncResolverFactoryTest::SetAddressResolved);

    rtc::SocketAddress address("", 0);
    resolver->Start(address);
    ASSERT_TRUE_WAIT(address_resolved_, 10000 /*ms*/);
    resolver->Destroy(false);
  }

  void SetAddressResolved(rtc::AsyncResolverInterface* resolver) {
    address_resolved_ = true;
  }

 private:
  bool address_resolved_ = false;
};

// This test is primarily intended to let tools check that the created resolver
// doesn't leak.
TEST_F(BasicAsyncResolverFactoryTest, TestCreate) {
  rtc::AutoThread main_thread;
  TestCreate();
}

TEST(WrappingAsyncDnsResolverFactoryTest, TestCreateAndResolve) {
  rtc::AutoThread main_thread;
  WrappingAsyncDnsResolverFactory factory(
      std::make_unique<BasicAsyncResolverFactory>());

  std::unique_ptr<AsyncDnsResolverInterface> resolver(factory.Create());
  ASSERT_TRUE(resolver);

  bool address_resolved = false;
  rtc::SocketAddress address("", 0);
  resolver->Start(address, [&address_resolved]() { address_resolved = true; });
  ASSERT_TRUE_WAIT(address_resolved, 10000 /*ms*/);
  resolver.reset();
}

TEST(WrappingAsyncDnsResolverFactoryTest, WrapOtherResolver) {
  rtc::AutoThread main_thread;
  BasicAsyncResolverFactory non_owned_factory;
  WrappingAsyncDnsResolverFactory factory(&non_owned_factory);
  std::unique_ptr<AsyncDnsResolverInterface> resolver(factory.Create());
  ASSERT_TRUE(resolver);

  bool address_resolved = false;
  rtc::SocketAddress address("", 0);
  resolver->Start(address, [&address_resolved]() { address_resolved = true; });
  ASSERT_TRUE_WAIT(address_resolved, 10000 /*ms*/);
  resolver.reset();
}

#if GTEST_HAS_DEATH_TEST && defined(WEBRTC_LINUX)
// Tests that the prohibition against deleting the resolver from the callback
// is enforced. This is required by the use of sigslot in the wrapped resolver.
// Checking the error message fails on a number of platforms, so run this
// test only on the platforms where it works.
void CallResolver(WrappingAsyncDnsResolverFactory& factory) {
  rtc::SocketAddress address("", 0);
  std::unique_ptr<AsyncDnsResolverInterface> resolver(factory.Create());
  resolver->Start(address, [&resolver]() { resolver.reset(); });
  WAIT(!resolver.get(), 10000 /*ms*/);
}

TEST(WrappingAsyncDnsResolverFactoryDeathTest, DestroyResolverInCallback) {
  rtc::AutoThread main_thread;
  // TODO(bugs.webrtc.org/12652): Rewrite as death test in loop style when it
  // works.
  WrappingAsyncDnsResolverFactory factory(
      std::make_unique<BasicAsyncResolverFactory>());

  // Since EXPECT_DEATH is thread sensitive, and the resolver creates a thread,
  // we wrap the whole creation section in EXPECT_DEATH.
  RTC_EXPECT_DEATH(CallResolver(factory),
                   "Check failed: !within_resolve_result_");
}
#endif

}  // namespace webrtc
