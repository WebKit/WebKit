/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/isolated_output.h"

#include <string.h>

#include "gflags/gflags.h"
#include "webrtc/base/file.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/test/testsupport/fileutils.h"

DEFINE_string(isolated_out_dir, webrtc::test::OutputPath(),
    "The isolated output folder provided by swarming test framework.");

namespace webrtc {
namespace test {

bool WriteIsolatedOutput(const char* filename,
                         const uint8_t* buffer,
                         size_t length) {
  if (FLAGS_isolated_out_dir.empty()) {
    LOG(LS_WARNING) << "No isolated_out_dir defined.";
    return false;
  }

  if (filename == nullptr || strlen(filename) == 0) {
    LOG(LS_WARNING) << "filename must be provided.";
    return false;
  }

  rtc::File output =
      rtc::File::Create(rtc::Pathname(FLAGS_isolated_out_dir, filename));

  return output.IsOpen() && output.Write(buffer, length) == length;
}

bool WriteIsolatedOutput(const char* filename, const std::string& content) {
  return WriteIsolatedOutput(filename,
                             reinterpret_cast<const uint8_t*>(content.c_str()),
                             content.length());
}

}  // namespace test
}  // namespace webrtc
