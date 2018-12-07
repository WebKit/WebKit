/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <iostream>

#include "modules/audio_coding/neteq/tools/neteq_performance_test.h"
#include "rtc_base/flags.h"

// Define command line flags.
WEBRTC_DEFINE_int(runtime_ms, 10000, "Simulated runtime in ms.");
WEBRTC_DEFINE_int(lossrate, 10, "Packet lossrate; drop every N packets.");
WEBRTC_DEFINE_float(drift, 0.1f, "Clockdrift factor.");
WEBRTC_DEFINE_bool(help, false, "Print this message.");

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for measuring the speed of NetEq.\n"
      "Usage: " +
      program_name +
      " [options]\n\n"
      "  --runtime_ms=N         runtime in ms; default is 10000 ms\n"
      "  --lossrate=N           drop every N packets; default is 10\n"
      "  --drift=F              clockdrift factor between 0.0 and 1.0; "
      "default is 0.1\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc != 1) {
    printf("%s", usage.c_str());
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }
  RTC_CHECK_GT(FLAG_runtime_ms, 0);
  RTC_CHECK_GE(FLAG_lossrate, 0);
  RTC_CHECK(FLAG_drift >= 0.0 && FLAG_drift < 1.0);

  int64_t result = webrtc::test::NetEqPerformanceTest::Run(
      FLAG_runtime_ms, FLAG_lossrate, FLAG_drift);
  if (result <= 0) {
    std::cout << "There was an error" << std::endl;
    return -1;
  }

  std::cout << "Simulation done" << std::endl;
  std::cout << "Runtime = " << result << " ms" << std::endl;
  return 0;
}
