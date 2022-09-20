/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"

namespace webrtc {
namespace test {

PerfTestResultWriter* CreateHistogramWriter() {
  RTC_DCHECK_NOTREACHED()
      << "Cannot run perf tests with rtc_enable_protobuf = false. "
         "Perf write results as protobufs.";
  return nullptr;
}

}  // namespace test
}  // namespace webrtc
