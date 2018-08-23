/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basicasyncresolverfactory.h"
#include "rtc_base/gunit.h"

namespace webrtc {

class BasicAsyncResolverFactoryTest : public testing::Test,
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
  TestCreate();
}

}  // namespace webrtc
