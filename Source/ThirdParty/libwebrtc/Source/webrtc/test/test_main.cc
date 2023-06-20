/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "test/gmock.h"
#include "test/test_main_lib.h"

namespace {

std::vector<std::string> ReplaceDashesWithUnderscores(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  for (std::string& arg : args) {
    // Only replace arguments that starts with a dash.
    if (!arg.empty() && arg[0] == '-') {
      // Don't replace the 2 first characters.
      auto begin = arg.begin() + 2;
      // Replace dashes on the left of '=' or on all the arg if no '=' is found.
      auto end = std::find(arg.begin(), arg.end(), '=');
      std::replace(begin, end, '-', '_');
    }
  }
  return args;
}

std::vector<char*> VectorOfStringsToVectorOfPointers(
    std::vector<std::string>& input) {
  std::vector<char*> output(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    output[i] = &(input[i][0]);
  }
  return output;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  absl::InitializeSymbolizer(argv[0]);
  testing::InitGoogleMock(&argc, argv);
  // Before parsing the arguments with the absl flag library, any internal '-'
  // characters will be converted to '_' characters to make sure the string is a
  // valid attribute name.
  std::vector<std::string> new_argv = ReplaceDashesWithUnderscores(argc, argv);
  std::vector<char*> raw_new_argv = VectorOfStringsToVectorOfPointers(new_argv);
  absl::ParseCommandLine(argc, &raw_new_argv[0]);

// This absl handler use unsupported features/instructions on Fuchsia
#if !defined(WEBRTC_FUCHSIA)
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
#endif

  std::unique_ptr<webrtc::TestMain> main = webrtc::TestMain::Create();
  int err_code = main->Init();
  if (err_code != 0) {
    return err_code;
  }
  return main->Run(argc, argv);
}
