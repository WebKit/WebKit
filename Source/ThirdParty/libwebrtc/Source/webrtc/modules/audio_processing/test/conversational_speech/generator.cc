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

#include "gflags/gflags.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/config.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/timing.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_factory.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/multiend_call.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/simulator.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

// Adapting DirExists/FileExists interfaces to DEFINE_validator.
auto dir_exists = [](const char* c, const std::string& dirpath) {
  return DirExists(dirpath);
};
auto file_exists = [](const char* c, const std::string& filepath) {
  return FileExists(filepath);
};

const char kUsageDescription[] =
    "Usage: conversational_speech_generator\n"
    "          -i <path/to/source/audiotracks>\n"
    "          -t <path/to/timing_file.txt>\n"
    "          -o <output/path>\n"
    "\n\n"
    "Command-line tool to generate multiple-end audio tracks to simulate "
    "conversational speech with two or more participants.";

DEFINE_string(i, "", "Directory containing the speech turn wav files");
DEFINE_validator(i, dir_exists);
DEFINE_string(t, "", "Path to the timing text file");
DEFINE_validator(t, file_exists);
DEFINE_string(o, "", "Output wav files destination path");
DEFINE_validator(o, dir_exists);

}  // namespace

int main(int argc, char* argv[]) {
  google::SetUsageMessage(kUsageDescription);
  google::ParseCommandLineFlags(&argc, &argv, true);
  conversational_speech::Config config(FLAGS_i, FLAGS_t, FLAGS_o);

  // Load timing.
  std::vector<conversational_speech::Turn> timing =
      conversational_speech::LoadTiming(config.timing_filepath());

  // Parse timing and audio tracks.
  auto wavreader_factory = rtc::MakeUnique<
      conversational_speech::WavReaderFactory>();
  conversational_speech::MultiEndCall multiend_call(
      timing, config.audiotracks_path(), std::move(wavreader_factory));

  // Generate output audio tracks.
  auto generated_audiotrack_pairs = conversational_speech::Simulate(
      multiend_call, config.output_path());

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
