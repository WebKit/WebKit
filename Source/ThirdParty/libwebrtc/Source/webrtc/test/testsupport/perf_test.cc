/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test.h"

#include <stdio.h>

#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/perf_test_histogram_writer.h"

namespace webrtc {
namespace test {

namespace {

std::string UnitWithDirection(
    absl::string_view units,
    webrtc::test::ImproveDirection improve_direction) {
  switch (improve_direction) {
    case webrtc::test::ImproveDirection::kNone:
      return std::string(units);
    case webrtc::test::ImproveDirection::kSmallerIsBetter:
      return std::string(units) + "_smallerIsBetter";
    case webrtc::test::ImproveDirection::kBiggerIsBetter:
      return std::string(units) + "_biggerIsBetter";
  }
}

std::vector<SamplesStatsCounter::StatsSample> GetSortedSamples(
    const SamplesStatsCounter& counter) {
  rtc::ArrayView<const SamplesStatsCounter::StatsSample> view =
      counter.GetTimedSamples();
  std::vector<SamplesStatsCounter::StatsSample> out(view.begin(), view.end());
  std::sort(out.begin(), out.end(),
            [](const SamplesStatsCounter::StatsSample& a,
               const SamplesStatsCounter::StatsSample& b) {
              return a.time < b.time;
            });
  return out;
}

template <typename Container>
void OutputListToStream(std::ostream* ostream, const Container& values) {
  const char* sep = "";
  for (const auto& v : values) {
    (*ostream) << sep << v;
    sep = ",";
  }
}

struct PlottableCounter {
  std::string graph_name;
  std::string trace_name;
  webrtc::SamplesStatsCounter counter;
  std::string units;
};

class PlottableCounterPrinter {
 public:
  PlottableCounterPrinter() : output_(stdout) {}

  void SetOutput(FILE* output) {
    MutexLock lock(&mutex_);
    output_ = output;
  }

  void AddCounter(absl::string_view graph_name,
                  absl::string_view trace_name,
                  const webrtc::SamplesStatsCounter& counter,
                  absl::string_view units) {
    MutexLock lock(&mutex_);
    plottable_counters_.push_back({std::string(graph_name),
                                   std::string(trace_name), counter,
                                   std::string(units)});
  }

  void Print(const std::vector<std::string>& desired_graphs_raw) const {
    std::set<std::string> desired_graphs(desired_graphs_raw.begin(),
                                         desired_graphs_raw.end());
    MutexLock lock(&mutex_);
    for (auto& counter : plottable_counters_) {
      if (!desired_graphs.empty()) {
        auto it = desired_graphs.find(counter.graph_name);
        if (it == desired_graphs.end()) {
          continue;
        }
      }

      std::ostringstream value_stream;
      value_stream.precision(8);
      value_stream << R"({"graph_name":")" << counter.graph_name << R"(",)";
      value_stream << R"("trace_name":")" << counter.trace_name << R"(",)";
      value_stream << R"("units":")" << counter.units << R"(",)";
      if (!counter.counter.IsEmpty()) {
        value_stream << R"("mean":)" << counter.counter.GetAverage() << ',';
        value_stream << R"("std":)" << counter.counter.GetStandardDeviation()
                     << ',';
      }
      value_stream << R"("samples":[)";
      const char* sep = "";
      for (const auto& sample : counter.counter.GetTimedSamples()) {
        value_stream << sep << R"({"time":)" << sample.time.us() << ','
                     << R"("value":)" << sample.value << '}';
        sep = ",";
      }
      value_stream << "]}";

      fprintf(output_, "PLOTTABLE_DATA: %s\n", value_stream.str().c_str());
    }
  }

 private:
  mutable Mutex mutex_;
  std::vector<PlottableCounter> plottable_counters_ RTC_GUARDED_BY(&mutex_);
  FILE* output_ RTC_GUARDED_BY(&mutex_);
};

PlottableCounterPrinter& GetPlottableCounterPrinter() {
  static PlottableCounterPrinter* printer_ = new PlottableCounterPrinter();
  return *printer_;
}

class ResultsLinePrinter {
 public:
  ResultsLinePrinter() : output_(stdout) {}

  void SetOutput(FILE* output) {
    MutexLock lock(&mutex_);
    output_ = output;
  }

  void PrintResult(absl::string_view graph_name,
                   absl::string_view trace_name,
                   const double value,
                   absl::string_view units,
                   bool important,
                   ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << value;

    PrintResultImpl(graph_name, trace_name, value_stream.str(), std::string(),
                    std::string(), UnitWithDirection(units, improve_direction),
                    important);
  }

  void PrintResultMeanAndError(absl::string_view graph_name,
                               absl::string_view trace_name,
                               const double mean,
                               const double error,
                               absl::string_view units,
                               bool important,
                               ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << mean << ',' << error;
    PrintResultImpl(graph_name, trace_name, value_stream.str(), "{", "}",
                    UnitWithDirection(units, improve_direction), important);
  }

  void PrintResultList(absl::string_view graph_name,
                       absl::string_view trace_name,
                       const rtc::ArrayView<const double> values,
                       absl::string_view units,
                       const bool important,
                       webrtc::test::ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    OutputListToStream(&value_stream, values);
    PrintResultImpl(graph_name, trace_name, value_stream.str(), "[", "]", units,
                    important);
  }

 private:
  void PrintResultImpl(absl::string_view graph_name,
                       absl::string_view trace_name,
                       absl::string_view values,
                       absl::string_view prefix,
                       absl::string_view suffix,
                       absl::string_view units,
                       bool important) {
    MutexLock lock(&mutex_);
    rtc::StringBuilder message;
    message << (important ? "*" : "") << "RESULT " << graph_name << ": "
            << trace_name << "= " << prefix << values << suffix << " " << units;
    // <*>RESULT <graph_name>: <trace_name>= <value> <units>
    // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
    // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
    fprintf(output_, "%s\n", message.str().c_str());
  }

  Mutex mutex_;
  FILE* output_ RTC_GUARDED_BY(&mutex_);
};

ResultsLinePrinter& GetResultsLinePrinter() {
  static ResultsLinePrinter* const printer_ = new ResultsLinePrinter();
  return *printer_;
}

PerfTestResultWriter& GetPerfWriter() {
  static PerfTestResultWriter* writer = CreateHistogramWriter();
  return *writer;
}

}  // namespace

void ClearPerfResults() {
  GetPerfWriter().ClearResults();
}

void SetPerfResultsOutput(FILE* output) {
  GetPlottableCounterPrinter().SetOutput(output);
  GetResultsLinePrinter().SetOutput(output);
}

std::string GetPerfResults() {
  return GetPerfWriter().Serialize();
}

void PrintPlottableResults(const std::vector<std::string>& desired_graphs) {
  GetPlottableCounterPrinter().Print(desired_graphs);
}

bool WritePerfResults(const std::string& output_path) {
  std::string results = GetPerfResults();
  CreateDir(DirName(output_path));
  FILE* output = fopen(output_path.c_str(), "wb");
  if (output == NULL) {
    printf("Failed to write to %s.\n", output_path.c_str());
    return false;
  }
  size_t written =
      fwrite(results.c_str(), sizeof(char), results.size(), output);
  fclose(output);

  if (written != results.size()) {
    long expected = results.size();
    printf("Wrote %zu, tried to write %lu\n", written, expected);
    return false;
  }

  return true;
}

void PrintResult(absl::string_view measurement,
                 absl::string_view modifier,
                 absl::string_view trace,
                 const double value,
                 absl::string_view units,
                 bool important,
                 ImproveDirection improve_direction) {
  rtc::StringBuilder graph_name;
  graph_name << measurement << modifier;
  RTC_CHECK(std::isfinite(value))
      << "Expected finite value for graph " << graph_name.str()
      << ", trace name " << trace << ", units " << units << ", got " << value;
  GetPerfWriter().LogResult(graph_name.str(), trace, value, units, important,
                            improve_direction);
  GetResultsLinePrinter().PrintResult(graph_name.str(), trace, value, units,
                                      important, improve_direction);
}

void PrintResult(absl::string_view measurement,
                 absl::string_view modifier,
                 absl::string_view trace,
                 const SamplesStatsCounter& counter,
                 absl::string_view units,
                 const bool important,
                 ImproveDirection improve_direction) {
  rtc::StringBuilder graph_name;
  graph_name << measurement << modifier;
  GetPlottableCounterPrinter().AddCounter(graph_name.str(), trace, counter,
                                          units);

  double mean = counter.IsEmpty() ? 0 : counter.GetAverage();
  double error = counter.IsEmpty() ? 0 : counter.GetStandardDeviation();

  std::vector<SamplesStatsCounter::StatsSample> timed_samples =
      GetSortedSamples(counter);
  std::vector<double> samples(timed_samples.size());
  for (size_t i = 0; i < timed_samples.size(); ++i) {
    samples[i] = timed_samples[i].value;
  }
  // If we have an empty counter, default it to 0.
  if (samples.empty()) {
    samples.push_back(0);
  }

  GetPerfWriter().LogResultList(graph_name.str(), trace, samples, units,
                                important, improve_direction);
  GetResultsLinePrinter().PrintResultMeanAndError(graph_name.str(), trace, mean,
                                                  error, units, important,
                                                  improve_direction);
}

void PrintResultMeanAndError(absl::string_view measurement,
                             absl::string_view modifier,
                             absl::string_view trace,
                             const double mean,
                             const double error,
                             absl::string_view units,
                             bool important,
                             ImproveDirection improve_direction) {
  RTC_CHECK(std::isfinite(mean));
  RTC_CHECK(std::isfinite(error));

  rtc::StringBuilder graph_name;
  graph_name << measurement << modifier;
  GetPerfWriter().LogResultMeanAndError(graph_name.str(), trace, mean, error,
                                        units, important, improve_direction);
  GetResultsLinePrinter().PrintResultMeanAndError(graph_name.str(), trace, mean,
                                                  error, units, important,
                                                  improve_direction);
}

void PrintResultList(absl::string_view measurement,
                     absl::string_view modifier,
                     absl::string_view trace,
                     const rtc::ArrayView<const double> values,
                     absl::string_view units,
                     bool important,
                     ImproveDirection improve_direction) {
  for (double v : values) {
    RTC_CHECK(std::isfinite(v));
  }

  rtc::StringBuilder graph_name;
  graph_name << measurement << modifier;
  GetPerfWriter().LogResultList(graph_name.str(), trace, values, units,
                                important, improve_direction);
  GetResultsLinePrinter().PrintResultList(graph_name.str(), trace, values,
                                          units, important, improve_direction);
}

}  // namespace test
}  // namespace webrtc
