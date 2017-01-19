/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FILEUTILS_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FILEUTILS_H_

#include <stdio.h>

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
namespace testing {
namespace bwe {

class ResourceFileReader {
 public:
  ~ResourceFileReader();

  bool IsAtEnd();
  bool Read(uint32_t* out);

  static ResourceFileReader* Create(const std::string& filename,
                                    const std::string& extension);

 private:
  explicit ResourceFileReader(FILE* file) : file_(file) {}
  FILE* file_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceFileReader);
};

class OutputFileWriter {
 public:
  ~OutputFileWriter();

  bool Write(uint32_t value);

  static OutputFileWriter* Create(const std::string& filename,
                                  const std::string& extension);

 private:
  explicit OutputFileWriter(FILE* file) : file_(file) {}
  FILE* file_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(OutputFileWriter);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FILEUTILS_H_
