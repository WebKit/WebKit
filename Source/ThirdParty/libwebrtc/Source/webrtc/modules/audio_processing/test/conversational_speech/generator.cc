/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <memory>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "modules/audio_processing/test/conversational_speech/config.h"
#include "modules/audio_processing/test/conversational_speech/multiend_call.h"
#include "modules/audio_processing/test/conversational_speech/simulator.h"
#include "modules/audio_processing/test/conversational_speech/timing.h"
#include "modules/audio_processing/test/conversational_speech/wavreader_factory.h"
#include "test/testsupport/file_utils.h"

ABSL_FLAG(std::string, i, "", "Directory containing the speech turn wav files");
ABSL_FLAG(std::string, t, "", "Path to the timing text file");
ABSL_FLAG(std::string, o, "", "Output wav files destination path");

namespace webrtc {
namespace test {
namespace {

const char kUsageDescription[] =
    "Usage: conversational_speech_generator\n"
    "          -i <path/to/source/audiotracks>\n"
    "          -t <path/to/timing_file.txt>\n"
    "          -o <output/path>\n"
    "\n\n"
    "Command-line tool to generate multiple-end audio tracks to simulate "
    "conversational speech with two or more participants.\n";

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  if (args.size() != 1) {
    printf("%s", kUsageDescription);
    return 1;
  }
  RTC_CHECK(DirExists(absl::GetFlag(FLAGS_i)));
  RTC_CHECK(FileExists(absl::GetFlag(FLAGS_t)));
  RTC_CHECK(DirExists(absl::GetFlag(FLAGS_o)));

  conversational_speech::Config config(
      absl::GetFlag(FLAGS_i), absl::GetFlag(FLAGS_t), absl::GetFlag(FLAGS_o));

  // Load timing.
  std::vector<conversational_speech::Turn> timing =
      conversational_speech::LoadTiming(config.timing_filepath());

  // Parse timing and audio tracks.
  auto wavreader_factory =
      std::make_unique<conversational_speech::WavReaderFactory>();
  conversational_speech::MultiEndCall multiend_call(
      timing, config.audiotracks_path(), std::move(wavreader_factory));

  // Generate output audio tracks.
  auto generated_audiotrack_pairs =
      conversational_speech::Simulate(multiend_call, config.output_path());

  // Show paths to created audio tracks.
  std::cout << "Output files:" << std::endl;
  for (const auto& output_paths_entry : *generated_audiotrack_pairs) {
    std::cout << "  speaker: " << output_paths_entry.first << std::endl;
    std::cout << "    near end: " << output_paths_entry.second.near_end
              << std::endl;
    std::cout << "    far end: " << output_paths_entry.second.far_end
              << std::endl;
  }

  return 0;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
