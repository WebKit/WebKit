/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/trace_to_stderr.h"

#include <assert.h>
#include <stdio.h>

#include <string>

namespace webrtc {
namespace test {

static const int kLevelFilter = kTraceError | kTraceWarning | kTraceTerseInfo;

TraceToStderr::TraceToStderr()
    : override_time_(false),
      time_seconds_(0) {
  Trace::set_level_filter(kLevelFilter);
  Trace::CreateTrace();
  Trace::SetTraceCallback(this);
}

TraceToStderr::TraceToStderr(bool override_time)
    : override_time_(override_time),
      time_seconds_(0) {
  Trace::set_level_filter(kLevelFilter);
  Trace::CreateTrace();
  Trace::SetTraceCallback(this);
}

TraceToStderr::~TraceToStderr() {
  Trace::SetTraceCallback(NULL);
  Trace::ReturnTrace();
}

void TraceToStderr::SetTimeSeconds(float time) { time_seconds_ = time; }

void TraceToStderr::Print(TraceLevel level, const char* msg_array, int length) {
  if (level & kLevelFilter) {
    assert(length > Trace::kBoilerplateLength);
    std::string msg = msg_array;
    std::string msg_log = msg.substr(Trace::kBoilerplateLength);
    if (override_time_) {
      fprintf(stderr, "%.2fs %s\n", time_seconds_, msg_log.c_str());
    } else {
      std::string msg_time = msg.substr(Trace::kTimestampPosition,
                                        Trace::kTimestampLength);
      fprintf(stderr, "%s %s\n", msg_time.c_str(), msg_log.c_str());
    }
    fflush(stderr);
  }
}

}  // namespace test
}  // namespace webrtc
