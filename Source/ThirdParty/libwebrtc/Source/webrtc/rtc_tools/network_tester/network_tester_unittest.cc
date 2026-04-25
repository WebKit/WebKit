/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_NETWORK_TESTER_TEST_ENABLED

#include <string>

#include "api/environment/environment.h"
#include "api/test/rtc_error_matchers.h"
#include "rtc_base/random.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_tools/network_tester/test_controller.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/wait_until.h"

namespace webrtc {

TEST(NetworkTesterTest, ServerClient) {
  // Use a unique port rather than a hard-coded one to avoid collision when
  // running the test in parallel in stress runs. Skipping all reserved ports.
  const int MIN_PORT = 49152;
  const int MAX_PORT = 65535;
  int port = Random(TimeMicros()).Rand(MIN_PORT, MAX_PORT);

  AutoThread main_thread;
  Environment env = CreateTestEnvironment();

  TestController client(
      env, 0, 0, test::ResourcePath("network_tester/client_config", "dat"),
      test::OutputPath() + "client_packet_log.dat");
  TestController server(
      env, port, port,
      test::ResourcePath("network_tester/server_config", "dat"),
      test::OutputPath() + "server_packet_log.dat");
  client.SendConnectTo("127.0.0.1", port);
  EXPECT_THAT(
      WaitUntil([&] { return server.IsTestDone() && client.IsTestDone(); },
                ::testing::IsTrue()),
      IsRtcOk());
}

}  // namespace webrtc

#endif  // WEBRTC_NETWORK_TESTER_TEST_ENABLED
