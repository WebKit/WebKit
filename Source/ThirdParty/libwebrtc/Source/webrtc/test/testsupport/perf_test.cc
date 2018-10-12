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
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"

#include <stdio.h>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace {

template <typename Container>
void OutputListToStream(std::ostream* ostream, const Container& values) {
  const char* sep = "";
  for (const auto& v : values) {
    (*ostream) << sep << v;
    sep = ",";
  }
}

class PerfResultsLogger {
 public:
  PerfResultsLogger() : crit_(), output_(stdout), graphs_() {}
  void ClearResults() {
    rtc::CritScope lock(&crit_);
    graphs_.clear();
  }
  void SetOutput(FILE* output) {
    rtc::CritScope lock(&crit_);
    output_ = output;
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important) {
    RTC_CHECK(std::isfinite(value));

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << value;
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"scalar",)";
    json_stream << R"("value":)" << value << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important) {
    RTC_CHECK(std::isfinite(mean));
    RTC_CHECK(std::isfinite(error));

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '{' << mean << ',' << error << '}';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":[)" << mean << "],";
    json_stream << R"("std":)" << error << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important) {
    for (double v : values) {
      RTC_CHECK(std::isfinite(v));
    }

    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '[';
    OutputListToStream(&value_stream, values);
    value_stream << ']';
    LogResultsImpl(graph_name, trace_name, value_stream.str(), units,
                   important);

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":)" << value_stream.str() << ',';
    json_stream << R"("units":")" << units << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }
  std::string ToJSON() const;

 private:
  void LogResultsImpl(const std::string& graph_name,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
    // <*>RESULT <graph_name>: <trace_name>= <value> <units>
    // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
    // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
    rtc::CritScope lock(&crit_);

    if (important) {
      fprintf(output_, "*");
    }
    fprintf(output_, "RESULT %s: %s= %s %s\n", graph_name.c_str(),
            trace.c_str(), values.c_str(), units.c_str());
  }

  rtc::CriticalSection crit_;
  FILE* output_ RTC_GUARDED_BY(&crit_);
  std::map<std::string, std::vector<std::string>> graphs_
      RTC_GUARDED_BY(&crit_);
};

std::string PerfResultsLogger::ToJSON() const {
  std::ostringstream json_stream;
  json_stream << R"({"format_version":"1.0",)";
  json_stream << R"("charts":{)";
  rtc::CritScope lock(&crit_);
  for (auto graphs_it = graphs_.begin(); graphs_it != graphs_.end();
       ++graphs_it) {
    if (graphs_it != graphs_.begin())
      json_stream << ',';
    json_stream << '"' << graphs_it->first << "\":";
    json_stream << '{';
    OutputListToStream(&json_stream, graphs_it->second);
    json_stream << '}';
  }
  json_stream << "}}";
  return json_stream.str();
}

PerfResultsLogger& GetPerfResultsLogger() {
  static PerfResultsLogger* const logger_ = new PerfResultsLogger();
  return *logger_;
}

}  // namespace

namespace webrtc {
namespace test {

void ClearPerfResults() {
  GetPerfResultsLogger().ClearResults();
}

void SetPerfResultsOutput(FILE* output) {
  GetPerfResultsLogger().SetOutput(output);
}

std::string GetPerfResultsJSON() {
  return GetPerfResultsLogger().ToJSON();
}

void WritePerfResults(const std::string& output_path) {
  std::string json_results = GetPerfResultsJSON();
  std::fstream json_file(output_path, std::fstream::out);
  json_file << json_results;
  json_file.close();
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important) {
  GetPerfResultsLogger().LogResult(measurement + modifier, trace, value, units,
                                   important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important) {
  GetPerfResultsLogger().LogResultMeanAndError(measurement + modifier, trace,
                                               mean, error, units, important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     bool important) {
  GetPerfResultsLogger().LogResultList(measurement + modifier, trace, values,
                                       units, important);
}

}  // namespace test
}  // namespace webrtc
