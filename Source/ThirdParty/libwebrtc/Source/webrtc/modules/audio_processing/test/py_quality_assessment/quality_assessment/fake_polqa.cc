/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <fstream>
#include <iostream>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {

const char* const kErrorMessage = "-Out /path/to/output/file is mandatory";

// Writes fake output intended to be parsed by
// quality_assessment.eval_scores.PolqaScore.
void WriteOutputFile(const std::string& output_file_path) {
  RTC_CHECK_NE(output_file_path, "");
  std::ofstream out(output_file_path);
  RTC_CHECK(!out.bad());
  out << "* Fake Polqa output" << std::endl;
  out << "FakeField1\tPolqaScore\tFakeField2" << std::endl;
  out << "FakeValue1\t3.25\tFakeValue2" << std::endl;
  out.close();
}

}  // namespace

int main(int argc, char* argv[]) {
  // Find "-Out" and use its next argument as output file path.
  RTC_CHECK_GE(argc, 3) << kErrorMessage;
  const std::string kSoughtFlagName = "-Out";
  for (int i = 1; i < argc - 1; ++i) {
    if (kSoughtFlagName.compare(argv[i]) == 0) {
      WriteOutputFile(argv[i + 1]);
      return 0;
    }
  }
  FATAL() << kErrorMessage;
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
