/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TEST_SUITE_H_
#define WEBRTC_TEST_TEST_SUITE_H_

// Derived from Chromium's src/base/test/test_suite.h.

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include <memory>

#include "webrtc/base/constructormagic.h"

namespace webrtc {
namespace test {

class TraceToStderr;

class TestSuite {
 public:
  TestSuite(int argc, char** argv);
  virtual ~TestSuite();

  int Run();

 protected:
  // Override these for custom initialization and shutdown handling.  Use these
  // instead of putting complex code in your constructor/destructor.
  virtual void Initialize();
  virtual void Shutdown();

  RTC_DISALLOW_COPY_AND_ASSIGN(TestSuite);

 private:
  std::unique_ptr<TraceToStderr> trace_to_stderr_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TEST_SUITE_H_
