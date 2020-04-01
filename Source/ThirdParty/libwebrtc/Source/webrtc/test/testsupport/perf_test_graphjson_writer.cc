/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_graphjson_writer.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"

namespace webrtc {
namespace test {

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

namespace {

class PerfTestGraphJsonWriter : public PerfTestResultWriter {
 public:
  PerfTestGraphJsonWriter() : crit_(), graphs_() {}
  void ClearResults() {
    rtc::CritScope lock(&crit_);
    graphs_.clear();
  }

  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important,
                 webrtc::test::ImproveDirection improve_direction) {
    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"scalar",)";
    json_stream << R"("value":)" << value << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }

  void LogResultMeanAndError(const std::string& graph_name,
                             const std::string& trace_name,
                             const double mean,
                             const double error,
                             const std::string& units,
                             const bool important,
                             webrtc::test::ImproveDirection improve_direction) {
    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":[)" << mean << "],";
    json_stream << R"("std":)" << error << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }

  void LogResultList(const std::string& graph_name,
                     const std::string& trace_name,
                     const rtc::ArrayView<const double> values,
                     const std::string& units,
                     const bool important,
                     webrtc::test::ImproveDirection improve_direction) {
    std::ostringstream value_stream;
    value_stream.precision(8);
    value_stream << '[';
    OutputListToStream(&value_stream, values);
    value_stream << ']';

    std::ostringstream json_stream;
    json_stream << '"' << trace_name << R"(":{)";
    json_stream << R"("type":"list_of_scalar_values",)";
    json_stream << R"("values":)" << value_stream.str() << ',';
    json_stream << R"("units":")" << UnitWithDirection(units, improve_direction)
                << R"("})";
    rtc::CritScope lock(&crit_);
    graphs_[graph_name].push_back(json_stream.str());
  }

  std::string Serialize() const {
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

 private:
  rtc::CriticalSection crit_;
  std::map<std::string, std::vector<std::string>> graphs_
      RTC_GUARDED_BY(&crit_);
};

}  // namespace

PerfTestResultWriter* CreateGraphJsonWriter() {
  return new PerfTestGraphJsonWriter();
}

}  // namespace test
}  // namespace webrtc
