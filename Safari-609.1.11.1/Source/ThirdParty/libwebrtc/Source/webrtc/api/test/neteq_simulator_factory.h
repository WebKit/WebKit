/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_FACTORY_H_
#define API_TEST_NETEQ_SIMULATOR_FACTORY_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "api/test/neteq_simulator.h"

namespace webrtc {
namespace test {

class NetEqTestFactory;

class NetEqSimulatorFactory {
 public:
  NetEqSimulatorFactory();
  ~NetEqSimulatorFactory();
  struct Config {
    int max_nr_packets_in_buffer = 0;
  };
  // This function takes the same arguments as the neteq_rtpplay utility.
  std::unique_ptr<NetEqSimulator> CreateSimulator(int argc, char* argv[]);
  std::unique_ptr<NetEqSimulator> CreateSimulatorFromFile(
      absl::string_view event_log_filename,
      absl::string_view replacement_audio_filename,
      Config simulation_config);
  // The same as above, but pass the file contents as a string.
  std::unique_ptr<NetEqSimulator> CreateSimulatorFromString(
      absl::string_view event_log_file_contents,
      absl::string_view replacement_audio_file,
      Config simulation_config);

 private:
  std::unique_ptr<NetEqTestFactory> factory_;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETEQ_SIMULATOR_FACTORY_H_
