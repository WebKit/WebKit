// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_PERF_PERF_RESULT_REPORTER_H_
#define TESTING_PERF_PERF_RESULT_REPORTER_H_

#include <string>
#include <unordered_map>

namespace perf_test
{

struct MetricInfo
{
    std::string units;
    bool important;
};

// A helper class for using the perf test printing functions safely, as
// otherwise it's easy to accidentally mix up arguments to produce usable but
// malformed perf data. See https://crbug.com/923564.

// Sample usage:
// auto reporter = PerfResultReporter("TextRendering", "100_chars");
// reporter.RegisterImportantMetric(".wall_time", "ms");
// reporter.RegisterImportantMetric(".cpu_time", "ms");
// ...
// reporter.AddResult(".wall_time", GetWallTime());
// reporter.AddResult(".cpu_time", GetCpuTime());

// This would end up reporting "TextRendering.wall_time" and
// "TextRendering.cpu_time" metrics on the dashboard, made up of results from
// a single "100_chars" story. If an additional story run is added, e.g.
// "200_chars", then the metrics will be averaged over both runs with the
// ability to drill down into results for specific stories.
class PerfResultReporter
{
  public:
    PerfResultReporter(const std::string &metric_basename, const std::string &story_name);
    ~PerfResultReporter();

    void RegisterFyiMetric(const std::string &metric_suffix, const std::string &units);
    void RegisterImportantMetric(const std::string &metric_suffix, const std::string &units);
    void AddResult(const std::string &metric_suffix, size_t value);
    void AddResult(const std::string &metric_suffix, double value);
    void AddResult(const std::string &metric_suffix, const std::string &value);

    // Returns true and fills the pointer if the metric is registered, otherwise
    // returns false.
    bool GetMetricInfo(const std::string &metric_suffix, MetricInfo *out);

  private:
    void RegisterMetric(const std::string &metric_suffix, const std::string &units, bool important);

    std::string metric_basename_;
    std::string story_name_;
    std::unordered_map<std::string, MetricInfo> metric_map_;
};

}  // namespace perf_test

#endif  // TESTING_PERF_PERF_RESULT_REPORTER_H_
