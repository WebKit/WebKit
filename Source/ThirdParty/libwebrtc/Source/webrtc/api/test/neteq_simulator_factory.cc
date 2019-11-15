/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/neteq_simulator_factory.h"

#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "modules/audio_coding/neteq/tools/neteq_test_factory.h"
#include "rtc_base/checks.h"

ABSL_FLAG(std::string,
          replacement_audio_file,
          "",
          "A PCM file that will be used to populate dummy"
          " RTP packets");
ABSL_FLAG(int,
          max_nr_packets_in_buffer,
          50,
          "Maximum allowed number of packets in the buffer");

namespace webrtc {
namespace test {

NetEqSimulatorFactory::NetEqSimulatorFactory()
    : factory_(absl::make_unique<NetEqTestFactory>()) {}

NetEqSimulatorFactory::~NetEqSimulatorFactory() = default;

std::unique_ptr<NetEqSimulator> NetEqSimulatorFactory::CreateSimulator(
    int argc,
    char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  RTC_CHECK_EQ(args.size(), 3)
      << "Wrong number of input arguments. Expected 3, got " << args.size();
  // TODO(ivoc) Stop (ab)using command-line flags in this function.
  const std::string output_audio_filename(args[2]);
  NetEqTestFactory::Config config;
  config.replacement_audio_file = absl::GetFlag(FLAGS_replacement_audio_file);
  config.max_nr_packets_in_buffer =
      absl::GetFlag(FLAGS_max_nr_packets_in_buffer);
  config.output_audio_filename = output_audio_filename;
  return factory_->InitializeTestFromFile(args[1], config);
}

std::unique_ptr<NetEqSimulator> NetEqSimulatorFactory::CreateSimulatorFromFile(
    absl::string_view event_log_filename,
    absl::string_view replacement_audio_filename,
    Config simulation_config) {
  NetEqTestFactory::Config config;
  config.replacement_audio_file = std::string(replacement_audio_filename);
  config.max_nr_packets_in_buffer = simulation_config.max_nr_packets_in_buffer;
  return factory_->InitializeTestFromFile(std::string(event_log_filename),
                                          config);
}

std::unique_ptr<NetEqSimulator>
NetEqSimulatorFactory::CreateSimulatorFromString(
    absl::string_view event_log_file_contents,
    absl::string_view replacement_audio_filename,
    Config simulation_config) {
  NetEqTestFactory::Config config;
  config.replacement_audio_file = std::string(replacement_audio_filename);
  config.max_nr_packets_in_buffer = simulation_config.max_nr_packets_in_buffer;
  return factory_->InitializeTestFromString(
      std::string(event_log_file_contents), config);
}

}  // namespace test
}  // namespace webrtc
