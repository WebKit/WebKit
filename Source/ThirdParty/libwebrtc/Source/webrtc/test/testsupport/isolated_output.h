/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TESTSUPPORT_ISOLATED_OUTPUT_H_
#define WEBRTC_TEST_TESTSUPPORT_ISOLATED_OUTPUT_H_

#include <stdlib.h>

#include <string>

namespace webrtc {
namespace test {

// Writes a |length| bytes array |buffer| to |filename| in isolated output
// directory defined by swarming. If the file is existing, content will be
// appended. Otherwise a new file will be created. This function returns false
// if isolated output directory has not been defined, or |filename| indicates an
// invalid or non-writable file, or underlying file system errors.
bool WriteIsolatedOutput(const char* filename,
                         const uint8_t* buffer,
                         size_t length);

bool WriteIsolatedOutput(const char* filename, const std::string& content);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TESTSUPPORT_ISOLATED_OUTPUT_H_
