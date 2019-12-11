/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_BASELINEFILE_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_BASELINEFILE_H_

#include <string>
#include "modules/include/module_common_types.h"

namespace webrtc {
namespace testing {
namespace bwe {

class BaseLineFileInterface {
 public:
  virtual ~BaseLineFileInterface() {}

  // Compare, or log, one estimate against the baseline file.
  virtual void Estimate(int64_t time_ms, uint32_t estimate_bps) = 0;

  // Verify whether there are any differences between the logged estimates and
  // those read from the baseline file. If updating the baseline file, write out
  // new file if there were differences. Return true if logged estimates are
  // identical, or if output file was updated successfully.
  virtual bool VerifyOrWrite() = 0;

  // Create an instance for either verifying estimates against a baseline file
  // with name |filename|, living in the resources/ directory or, if the flag
  // |write_updated_file| is set, write logged estimates to a file with the same
  // name, living in the out/ directory.
  static BaseLineFileInterface* Create(const std::string& filename,
                                       bool write_updated_file);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_BASELINEFILE_H_
