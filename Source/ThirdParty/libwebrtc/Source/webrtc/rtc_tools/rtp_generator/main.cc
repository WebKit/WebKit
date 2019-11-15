/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "rtc_tools/rtp_generator/rtp_generator.h"

ABSL_FLAG(std::string, input_config, "", "JSON file with config");
ABSL_FLAG(std::string, output_rtpdump, "", "Where to store the rtpdump");

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "Generates custom configured rtpdumps for the purpose of testing.\n"
      "Example Usage:\n"
      "./rtp_generator --input_config=sender_config.json\n"
      "                --output_rtpdump=my.rtpdump\n");
  absl::ParseCommandLine(argc, argv);

  const std::string config_path = absl::GetFlag(FLAGS_input_config);
  const std::string rtp_dump_path = absl::GetFlag(FLAGS_output_rtpdump);

  if (rtp_dump_path.empty() || config_path.empty()) {
    return EXIT_FAILURE;
  }

  absl::optional<webrtc::RtpGeneratorOptions> options =
      webrtc::ParseRtpGeneratorOptionsFromFile(config_path);
  if (!options.has_value()) {
    return EXIT_FAILURE;
  }

  webrtc::RtpGenerator rtp_generator(*options);
  rtp_generator.GenerateRtpDump(rtp_dump_path);

  return EXIT_SUCCESS;
}
