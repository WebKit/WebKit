/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PERF_RESULT_REPORTER_H_
#define TEST_TESTSUPPORT_PERF_RESULT_REPORTER_H_

#include <string>
#include <unordered_map>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace test {

// These match the units in histogram.proto (in third_party/catapult).
enum class Unit {
  kMs,
  kMsBestFitFormat,
  kMsTs,
  kNPercent,
  kSizeInBytes,
  kBytesPerSecond,
  kHertz,
  kUnitless,
  kCount,
  kSigma,
};

struct MetricInfo {
  Unit unit;
  ImproveDirection improve_direction;
};

// A helper class for using the perf test printing functions safely, as
// otherwise it's easy to accidentally mix up arguments to produce usable but
// malformed perf data. See https://crbug.com/923564.
//
// Sample usage:
// auto reporter = PerfResultReporter("ramp_up_time", "bwe_15s");
// reporter.RegisterImportantMetric(
//     "_turn_over_tcp", Unit::kMs, ImproveDirection::kBiggerIsBetter);
// reporter.RegisterImportantMetric("_cpu_time", Unit::kMs);
// ...
// reporter.AddResult("turn_over_tcp", GetTurnOverTcpTime());
// reporter.AddResult("turn_over_udp", GetTurnOverUdpTime());
//
// This will show in the dashboard as
// (test binary name) > (bot) > ramp_up_time_turn_over_tcp > bwe_15s.
// (test binary name) > (bot) > ramp_up_time_turn_over_udp > bwe_15s.
//
// If you add more reporters that cover other user stories, they will show up
// as separate subtests (e.g. next to bwe_15s).
class PerfResultReporter {
 public:
  PerfResultReporter(absl::string_view metric_basename,
                     absl::string_view story_name);
  ~PerfResultReporter();

  void RegisterMetric(absl::string_view metric_suffix, Unit unit);
  void RegisterMetric(absl::string_view metric_suffix,
                      Unit unit,
                      ImproveDirection improve_direction);
  void AddResult(absl::string_view metric_suffix, size_t value) const;
  void AddResult(absl::string_view metric_suffix, double value) const;

  void AddResultList(absl::string_view metric_suffix,
                     rtc::ArrayView<const double> values) const;

  // Users should prefer AddResultList if possible, as otherwise the min/max
  // values reported on the perf dashboard aren't useful.
  // |mean_and_error| should be a comma-separated string of mean then
  // error/stddev, e.g. "2.4,0.5".
  void AddResultMeanAndError(absl::string_view metric_suffix,
                             double mean,
                             double error);

  // Returns the metric info if it has been registered.
  absl::optional<MetricInfo> GetMetricInfo(
      absl::string_view metric_suffix) const;

 private:
  MetricInfo GetMetricInfoOrFail(absl::string_view metric_suffix) const;

  std::string metric_basename_;
  std::string story_name_;
  std::unordered_map<std::string, MetricInfo> metric_map_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_PERF_RESULT_REPORTER_H_
