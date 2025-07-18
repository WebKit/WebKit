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

#include <memory>

#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::IsTrue;

const TimeDelta kDefaultTimeout = TimeDelta::Millis(1000);
const int kPortNumber = 3027;

TEST(AsyncDnsResolver, ConstructorWorks) {
  AsyncDnsResolver resolver;
}

TEST(AsyncDnsResolver, ResolvingLocalhostWorks) {
  test::RunLoop loop;  // Ensure that posting back to main thread works
  AsyncDnsResolver resolver;
  SocketAddress address("localhost",
                        kPortNumber);  // Port number does not matter
  SocketAddress resolved_address;
  bool done = false;
  resolver.Start(address, [&done] { done = true; });
  ASSERT_THAT(
      WaitUntil([&] { return done; }, IsTrue(), {.timeout = kDefaultTimeout}),
      IsRtcOk());
  EXPECT_EQ(resolver.result().GetError(), 0);
  if (resolver.result().GetResolvedAddress(AF_INET, &resolved_address)) {
    EXPECT_EQ(resolved_address, SocketAddress("127.0.0.1", kPortNumber));
  } else {
    RTC_LOG(LS_INFO) << "Resolution gave no address, skipping test";
  }
}

TEST(AsyncDnsResolver, ResolveAfterDeleteDoesNotReturn) {
  test::RunLoop loop;
  std::unique_ptr<AsyncDnsResolver> resolver =
      std::make_unique<AsyncDnsResolver>();
  SocketAddress address("localhost",
                        kPortNumber);  // Port number does not matter
  SocketAddress resolved_address;
  bool done = false;
  resolver->Start(address, [&done] { done = true; });
  resolver.reset();                    // Deletes resolver.
  loop.Flush();                        // Allows callback to execute
  EXPECT_FALSE(done);                  // Expect no result.
}

}  // namespace
}  // namespace webrtc
