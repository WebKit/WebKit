/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PERF_TEST_RESULT_WRITER_H_
#define TEST_TESTSUPPORT_PERF_TEST_RESULT_WRITER_H_

#include <stdio.h>
#include <string>

#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace test {

// Interface for classes that write perf results to some kind of JSON format.
class PerfTestResultWriter {
 public:
  virtual ~PerfTestResultWriter() = default;

  virtual void ClearResults() = 0;
  virtual void LogResult(const std::string& graph_name,
                         const std::string& trace_name,
                         const double value,
                         const std::string& units,
                         const bool important,
                         webrtc::test::ImproveDirection improve_direction) = 0;
  virtual void LogResultMeanAndError(
      const std::string& graph_name,
      const std::string& trace_name,
      const double mean,
      const double error,
      const std::string& units,
      const bool important,
      webrtc::test::ImproveDirection improve_direction) = 0;
  virtual void LogResultList(
      const std::string& graph_name,
      const std::string& trace_name,
      const rtc::ArrayView<const double> values,
      const std::string& units,
      const bool important,
      webrtc::test::ImproveDirection improve_direction) = 0;

  virtual std::string Serialize() const = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_PERF_TEST_RESULT_WRITER_H_
