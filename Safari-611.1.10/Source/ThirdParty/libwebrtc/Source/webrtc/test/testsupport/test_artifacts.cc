/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/test_artifacts.h"

#include <string.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/testsupport/file_utils.h"

namespace {
const std::string& DefaultArtifactPath() {
  static const std::string path = webrtc::test::OutputPath();
  return path;
}
}  // namespace

ABSL_FLAG(std::string,
          test_artifacts_dir,
          DefaultArtifactPath().c_str(),
          "The output folder where test output should be saved.");

namespace webrtc {
namespace test {

bool GetTestArtifactsDir(std::string* out_dir) {
  if (absl::GetFlag(FLAGS_test_artifacts_dir).empty()) {
    RTC_LOG(LS_WARNING) << "No test_out_dir defined.";
    return false;
  }
  *out_dir = absl::GetFlag(FLAGS_test_artifacts_dir);
  return true;
}

bool WriteToTestArtifactsDir(const char* filename,
                             const uint8_t* buffer,
                             size_t length) {
  if (absl::GetFlag(FLAGS_test_artifacts_dir).empty()) {
    RTC_LOG(LS_WARNING) << "No test_out_dir defined.";
    return false;
  }

  if (filename == nullptr || strlen(filename) == 0) {
    RTC_LOG(LS_WARNING) << "filename must be provided.";
    return false;
  }

  FileWrapper output = FileWrapper::OpenWriteOnly(
      JoinFilename(absl::GetFlag(FLAGS_test_artifacts_dir), filename));

  return output.is_open() && output.Write(buffer, length);
}

bool WriteToTestArtifactsDir(const char* filename, const std::string& content) {
  return WriteToTestArtifactsDir(
      filename, reinterpret_cast<const uint8_t*>(content.c_str()),
      content.length());
}

}  // namespace test
}  // namespace webrtc
