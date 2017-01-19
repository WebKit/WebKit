/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_
#define WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_

#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace test {

// Upon constructing an instance of this class, all traces will be redirected
// to stderr. At destruction, redirection is halted.
class TraceToStderr : public TraceCallback {
 public:
  TraceToStderr();
  // Set |override_time| to true to control the time printed with each trace
  // through SetTimeSeconds(). Otherwise, the trace's usual wallclock time is
  // used.
  //
  // This is useful for offline test tools, where the file time is much more
  // informative than the real time.
  explicit TraceToStderr(bool override_time);
  ~TraceToStderr() override;

  // Every subsequent trace printout will use |time|. Has no effect if
  // |override_time| in the constructor was set to false.
  //
  // No attempt is made to ensure thread-safety between the trace writing and
  // time updating. In tests, since traces will normally be triggered by the
  // main thread doing the time updating, this should be of no concern.
  virtual void SetTimeSeconds(float time);

  // Implements TraceCallback.
  void Print(TraceLevel level, const char* msg_array, int length) override;

 private:
  bool override_time_;
  float time_seconds_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_
