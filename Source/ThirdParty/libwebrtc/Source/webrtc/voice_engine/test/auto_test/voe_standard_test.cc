/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/test/auto_test/automated_mode.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_defines.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

DEFINE_bool(include_timing_dependent_tests, true,
            "If true, we will include tests / parts of tests that are known "
            "to break in slow execution environments (such as valgrind).");
DEFINE_bool(automated, false,
            "If true, we'll run the automated tests we have in noninteractive "
            "mode.");

using namespace webrtc;

namespace voetest {

int dummy = 0;  // Dummy used in different functions to avoid warnings

void SubAPIManager::DisplayStatus() const {
  TEST_LOG("Supported sub APIs:\n\n");
  if (_base)
    TEST_LOG("  Base\n");
  if (_codec)
    TEST_LOG("  Codec\n");
  if (_file)
    TEST_LOG("  File\n");
  if (_hardware)
    TEST_LOG("  Hardware\n");
  if (_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (_network)
    TEST_LOG("  Network\n");
  if (_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
  TEST_LOG("Excluded sub APIs:\n\n");
  if (!_base)
    TEST_LOG("  Base\n");
  if (!_codec)
    TEST_LOG("  Codec\n");
  if (!_file)
    TEST_LOG("  File\n");
  if (!_hardware)
    TEST_LOG("  Hardware\n");
  if (!_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (!_network)
    TEST_LOG("  Network\n");
  if (!_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (!_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (!_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
}
}  // namespace voetest

int RunInManualMode() {
  using namespace voetest;

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

  printf("----------------------------\n");
  printf("Select type of test\n\n");
  printf(" (0)  Quit\n");
  printf(" (1)  Standard test\n");
  printf("\n: ");

  int selection(0);
  dummy = scanf("%d", &selection);

  switch (selection) {
    case 0:
      return 0;
    case 1:
      TEST_LOG("\n\n+++ Running standard tests +++\n\n");
      // Currently, all googletest-rewritten tests are in the "automated" suite.
      return RunInAutomatedMode();
    default:
      TEST_LOG("Invalid selection!\n");
      return 0;
  }
}

// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

#if !defined(WEBRTC_IOS)
int main(int argc, char** argv) {
  // This function and RunInAutomatedMode is defined in automated_mode.cc
  // to avoid macro clashes with googletest (for instance ASSERT_TRUE).
  InitializeGoogleTest(&argc, argv);
  // AllowCommandLineParsing allows us to ignore flags passed on to us by
  // Chromium build bots without having to explicitly disable them.
  google::AllowCommandLineReparsing();
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_automated) {
    return RunInAutomatedMode();
  }

  return RunInManualMode();
}
#endif //#if !defined(WEBRTC_IOS)
