/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_dns_resolver.h"

#include "rtc_base/logging.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/socket_address.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {
namespace {
constexpr int kSomePortNumber = 3027;

TEST(AsyncDnsResolver, ResolvingLocalhostWorks) {
  test::RunLoop loop;  // Ensure that posting back to main thread works
  AsyncDnsResolver resolver;
  bool done = false;
  resolver.Start(SocketAddress("localhost", kSomePortNumber), [&]() {
    done = true;
    loop.Quit();
  });
  EXPECT_FALSE(done);  // The target TQ hasn't gotten a chance to run yet.
  loop.Run();          // Wait for the callback to arrive.
  EXPECT_TRUE(done);   // Now `done` must be true.
  EXPECT_EQ(resolver.result().GetError(), 0);
  SocketAddress resolved_address;
  if (resolver.result().GetResolvedAddress(AF_INET, &resolved_address)) {
    EXPECT_EQ(resolved_address, SocketAddress("127.0.0.1", kSomePortNumber));
  } else {
    RTC_LOG(LS_INFO) << "Resolution gave no address, skipping test";
  }
}

TEST(AsyncDnsResolver, ResolvingBogusFails) {
  test::RunLoop loop;  // Ensure that posting back to main thread works
  AsyncDnsResolver resolver;
  bool done = false;
  resolver.Start(SocketAddress("*!#*", kSomePortNumber), [&]() {
    done = true;
    loop.Quit();
  });
  EXPECT_FALSE(done);  // The target TQ hasn't gotten a chance to run yet.
  loop.Run();          // Wait for the callback to arrive.
  EXPECT_TRUE(done);   // Now `done` must be true.
  EXPECT_NE(resolver.result().GetError(), 0);
  SocketAddress resolved_address;
  EXPECT_FALSE(
      resolver.result().GetResolvedAddress(AF_INET, &resolved_address));
}

TEST(AsyncDnsResolver, ResolveAfterDeleteDoesNotReturn) {
  bool done = false;
  test::RunLoop loop;
  {
    AsyncDnsResolver resolver;
    SocketAddress address("some-very-slow-dns-entry.local", kSomePortNumber);
    resolver.Start(address, [&] { done = true; });
  }
  EXPECT_FALSE(done);  // Expect no result.
}

}  // namespace
}  // namespace webrtc
