/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_result_reporter.h"

#include <vector>

namespace {

// These characters mess with either the stdout parsing or the dashboard itself.
const std::vector<std::string>& InvalidCharacters() {
  static const std::vector<std::string> kInvalidCharacters({"/", ":", "="});

  return kInvalidCharacters;
}

void CheckForInvalidCharacters(const std::string& str) {
  for (const auto& invalid : InvalidCharacters()) {
    RTC_CHECK(str.find(invalid) == std::string::npos)
        << "Given invalid character for perf names '" << invalid << "'";
  }
}

}  // namespace

namespace webrtc {
namespace test {

namespace {

// For now, mark all tests as "not important". This distinction mostly goes away
// in histograms anyway.
const bool kNotImportant = false;

std::string UnitToString(Unit unit) {
  // Down the line, we should convert directly from Unit to the histogram.proto
  // enum values. We need to convert to strings until all uses of perf_test.h
  // have been eliminated. We're not using the proto enum directly in the .h of
  // this file because we don't want to limit the exposure of the proto.
  //
  // Keep this list up to date with kJsonToProtoUnit in histogram.cc in the
  // Catapult repo.
  switch (unit) {
    case Unit::kMs:
      return "ms";
    case Unit::kMsBestFitFormat:
      return "msBestFitFormat";
    case Unit::kMsTs:
      return "tsMs";
    case Unit::kNPercent:
      return "n%";
    case Unit::kSizeInBytes:
      return "sizeInBytes";
    case Unit::kBytesPerSecond:
      return "bytesPerSecond";
    case Unit::kHertz:
      return "Hz";
    case Unit::kUnitless:
      return "unitless";
    case Unit::kCount:
      return "count";
    case Unit::kSigma:
      return "sigma";
    default:
      RTC_NOTREACHED() << "Unknown unit " << unit;
      return "unitless";
  }
}

}  // namespace

PerfResultReporter::PerfResultReporter(const std::string& metric_basename,
                                       const std::string& story_name)
    : metric_basename_(metric_basename), story_name_(story_name) {
  CheckForInvalidCharacters(metric_basename_);
  CheckForInvalidCharacters(story_name_);
}

PerfResultReporter::~PerfResultReporter() = default;

void PerfResultReporter::RegisterMetric(const std::string& metric_suffix,
                                        Unit unit) {
  RegisterMetric(metric_suffix, unit, ImproveDirection::kNone);
}
void PerfResultReporter::RegisterMetric(const std::string& metric_suffix,
                                        Unit unit,
                                        ImproveDirection improve_direction) {
  CheckForInvalidCharacters(metric_suffix);
  RTC_CHECK(metric_map_.count(metric_suffix) == 0);
  metric_map_.insert({metric_suffix, {unit, improve_direction}});
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   size_t value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResult(metric_basename_, metric_suffix, story_name_, value,
              UnitToString(info.unit), kNotImportant, info.improve_direction);
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   double value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResult(metric_basename_, metric_suffix, story_name_, value,
              UnitToString(info.unit), kNotImportant, info.improve_direction);
}

void PerfResultReporter::AddResultList(
    const std::string& metric_suffix,
    rtc::ArrayView<const double> values) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResultList(metric_basename_, metric_suffix, story_name_, values,
                  UnitToString(info.unit), kNotImportant,
                  info.improve_direction);
}

void PerfResultReporter::AddResultMeanAndError(const std::string& metric_suffix,
                                               const double mean,
                                               const double error) {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResultMeanAndError(metric_basename_, metric_suffix, story_name_, mean,
                          error, UnitToString(info.unit), kNotImportant,
                          info.improve_direction);
}

absl::optional<MetricInfo> PerfResultReporter::GetMetricInfo(
    const std::string& metric_suffix) const {
  auto iter = metric_map_.find(metric_suffix);
  if (iter == metric_map_.end()) {
    return absl::optional<MetricInfo>();
  }

  return absl::optional<MetricInfo>(iter->second);
}

MetricInfo PerfResultReporter::GetMetricInfoOrFail(
    const std::string& metric_suffix) const {
  absl::optional<MetricInfo> info = GetMetricInfo(metric_suffix);
  RTC_CHECK(info.has_value())
      << "Attempted to use unregistered metric " << metric_suffix;
  return *info;
}

}  // namespace test
}  // namespace webrtc
