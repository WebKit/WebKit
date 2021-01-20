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

#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/perf_test_histogram_writer.h"

namespace webrtc {
namespace test {

namespace {

std::string UnitWithDirection(
    const std::string& units,
    webrtc::test::ImproveDirection improve_direction) {
  switch (improve_direction) {
    case webrtc::test::ImproveDirection::kNone:
      return units;
    case webrtc::test::ImproveDirection::kSmallerIsBetter:
      return units + "_smallerIsBetter";
    case webrtc::test::ImproveDirection::kBiggerIsBetter:
      return units + "_biggerIsBetter";
  }
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

  void AddCounter(const std::string& graph_name,
                  const std::string& trace_name,
                  const webrtc::SamplesStatsCounter& counter,
                  const std::string& units) {
    MutexLock lock(&mutex_);
    plottable_counters_.push_back({graph_name, trace_name, counter, units});
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

  void PrintResult(const std::string& graph_name,
                   const std::string& trace_name,
                   const double value,
                   const std::string& units,
                   bool important,
                   ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << value;

    PrintResultImpl(graph_name, trace_name, value_stream.str(), std::string(),
                    std::string(), UnitWithDirection(units, improve_direction),
                    important);
  }

  void PrintResultMeanAndError(const std::string& graph_name,
                               const std::string& trace_name,
                               const double mean,
                               const double error,
                               const std::string& units,
                               bool important,
                               ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << mean << ',' << error;
    PrintResultImpl(graph_name, trace_name, value_stream.str(), "{", "}",
                    UnitWithDirection(units, improve_direction), important);
  }

  void PrintResultList(const std::string& graph_name,
                       const std::string& trace_name,
                       const rtc::ArrayView<const double> values,
                       const std::string& units,
                       const bool important,
                       webrtc::test::ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    OutputListToStream(&value_stream, values);
    PrintResultImpl(graph_name, trace_name, value_stream.str(), "[", "]", units,
                    important);
  }

 private:
  void PrintResultImpl(const std::string& graph_name,
                       const std::string& trace_name,
                       const std::string& values,
                       const std::string& prefix,
                       const std::string& suffix,
                       const std::string& units,
                       bool important) {
    MutexLock lock(&mutex_);
    // <*>RESULT <graph_name>: <trace_name>= <value> <units>
    // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
    // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
    fprintf(output_, "%sRESULT %s: %s= %s%s%s %s\n", important ? "*" : "",
            graph_name.c_str(), trace_name.c_str(), prefix.c_str(),
            values.c_str(), suffix.c_str(), units.c_str());
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

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important,
                 ImproveDirection improve_direction) {
  std::string graph_name = measurement + modifier;
  RTC_CHECK(std::isfinite(value))
      << "Expected finite value for graph " << graph_name << ", trace name "
      << trace << ", units " << units << ", got " << value;
  GetPerfWriter().LogResult(graph_name, trace, value, units, important,
                            improve_direction);
  GetResultsLinePrinter().PrintResult(graph_name, trace, value, units,
                                      important, improve_direction);
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const SamplesStatsCounter& counter,
                 const std::string& units,
                 const bool important,
                 ImproveDirection improve_direction) {
  std::string graph_name = measurement + modifier;
  GetPlottableCounterPrinter().AddCounter(graph_name, trace, counter, units);

  double mean = counter.IsEmpty() ? 0 : counter.GetAverage();
  double error = counter.IsEmpty() ? 0 : counter.GetStandardDeviation();
  PrintResultMeanAndError(measurement, modifier, trace, mean, error, units,
                          important, improve_direction);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important,
                             ImproveDirection improve_direction) {
  RTC_CHECK(std::isfinite(mean));
  RTC_CHECK(std::isfinite(error));

  std::string graph_name = measurement + modifier;
  GetPerfWriter().LogResultMeanAndError(graph_name, trace, mean, error, units,
                                        important, improve_direction);
  GetResultsLinePrinter().PrintResultMeanAndError(
      graph_name, trace, mean, error, units, important, improve_direction);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     bool important,
                     ImproveDirection improve_direction) {
  for (double v : values) {
    RTC_CHECK(std::isfinite(v));
  }

  std::string graph_name = measurement + modifier;
  GetPerfWriter().LogResultList(graph_name, trace, values, units, important,
                                improve_direction);
  GetResultsLinePrinter().PrintResultList(graph_name, trace, values, units,
                                          important, improve_direction);
}

}  // namespace test
}  // namespace webrtc
