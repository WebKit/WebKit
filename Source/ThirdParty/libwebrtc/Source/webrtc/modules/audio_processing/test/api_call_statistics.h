/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_API_CALL_STATISTICS_H_
#define MODULES_AUDIO_PROCESSING_TEST_API_CALL_STATISTICS_H_

#include <string>
#include <vector>

namespace webrtc {
namespace test {

// Collects statistics about the API call durations.
class ApiCallStatistics {
 public:
  enum class CallType { kRender, kCapture };

  // Adds a new datapoint.
  void Add(int64_t duration_nanos, CallType call_type);

  // Prints out a report of the statistics.
  void PrintReport() const;

  // Writes the call information to a file.
  void WriteReportToFile(const std::string& filename) const;

 private:
  struct CallData {
    CallData(int64_t duration_nanos, CallType call_type);
    int64_t duration_nanos;
    CallType call_type;
  };
  std::vector<CallData> calls_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_API_CALL_STATISTICS_H_
