/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <string>

#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"
#include "rtc_base/flags.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

DEFINE_bool(codec_map,
            false,
            "Prints the mapping between RTP payload type and "
            "codec");
DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");
DEFINE_bool(help, false, "Prints this message");

int main(int argc, char* argv[]) {
  webrtc::test::NetEqTestFactory factory;
  std::string program_name = argv[0];
  std::string usage =
      "Tool for decoding an RTP dump file using NetEq.\n"
      "Run " +
      program_name +
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.rtp output.{pcm, wav}\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    exit(1);
  }
  if (FLAG_help) {
    std::cout << usage;
    rtc::FlagList::Print(nullptr, false);
    exit(0);
  }
  if (FLAG_codec_map) {
    factory.PrintCodecMap();
  }
  if (argc != 3) {
    if (FLAG_codec_map) {
      // We have already printed the codec map. Just end the program.
      exit(0);
    }
    // Print usage information.
    std::cout << usage;
    exit(0);
  }
  webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(FLAG_force_fieldtrials);
  std::unique_ptr<webrtc::test::NetEqTest> test =
      factory.InitializeTest(argv[1], argv[2]);
  test->Run();
  return 0;
}
