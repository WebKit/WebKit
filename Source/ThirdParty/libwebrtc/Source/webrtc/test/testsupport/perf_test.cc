/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A stripped-down version of Chromium's chrome/test/perf/perf_test.cc.
// ResultsToString(), PrintResult(size_t value) and AppendResult(size_t value)
// have been modified. The remainder are identical to the Chromium version.

#include "webrtc/test/testsupport/perf_test.h"

#include <sstream>
#include <stdio.h>

namespace {

std::string ResultsToString(const std::string& measurement,
                            const std::string& modifier,
                            const std::string& trace,
                            const std::string& values,
                            const std::string& prefix,
                            const std::string& suffix,
                            const std::string& units,
                            bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>

  // TODO(ajm): Use of a stream here may violate the style guide (depending on
  // one's definition of "logging"). Consider adding StringPrintf-like
  // functionality as in the original Chromium implementation.
  std::ostringstream stream;
  if (important) {
    stream << "*";
  }
  stream << "RESULT " << measurement << modifier << ": " << trace << "= "
         << prefix << values << suffix << " " << units << std::endl;
  return stream.str();
}

void PrintResultsImpl(const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& prefix,
                      const std::string& suffix,
                      const std::string& units,
                      bool important) {
  printf("%s", ResultsToString(measurement, modifier, trace, values,
                               prefix, suffix, units, important).c_str());
}

}  // namespace

namespace webrtc {
namespace test {

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 size_t value,
                 const std::string& units,
                 bool important) {
  std::ostringstream value_stream;
  value_stream << value;
  PrintResultsImpl(measurement, modifier, trace, value_stream.str(), "", "",
                   units, important);
}

void AppendResult(std::string& output,
                  const std::string& measurement,
                  const std::string& modifier,
                  const std::string& trace,
                  size_t value,
                  const std::string& units,
                  bool important) {
  std::ostringstream value_stream;
  value_stream << value;
  output += ResultsToString(measurement, modifier, trace,
                            value_stream.str(),
                            "", "", units, important);
}

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const std::string& value,
                 const std::string& units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, value, "", "", units,
                   important);
}

void AppendResult(std::string& output,
                  const std::string& measurement,
                  const std::string& modifier,
                  const std::string& trace,
                  const std::string& value,
                  const std::string& units,
                  bool important) {
  output += ResultsToString(measurement, modifier, trace, value, "", "", units,
                            important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const std::string& mean_and_error,
                             const std::string& units,
                             bool important) {
  PrintResultsImpl(measurement, modifier, trace, mean_and_error,
                   "{", "}", units, important);
}

void AppendResultMeanAndError(std::string& output,
                              const std::string& measurement,
                              const std::string& modifier,
                              const std::string& trace,
                              const std::string& mean_and_error,
                              const std::string& units,
                              bool important) {
  output += ResultsToString(measurement, modifier, trace, mean_and_error,
                            "{", "}", units, important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::string& values,
                     const std::string& units,
                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
                   "[", "]", units, important);
}

void AppendResultList(std::string& output,
                      const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
  output += ResultsToString(measurement, modifier, trace, values,
                            "[", "]", units, important);
}

void PrintSystemCommitCharge(const std::string& test_name,
                             size_t charge,
                             bool important) {
  PrintSystemCommitCharge(stdout, test_name, charge, important);
}

void PrintSystemCommitCharge(FILE* target,
                             const std::string& test_name,
                             size_t charge,
                             bool important) {
  fprintf(target, "%s", SystemCommitChargeToString(test_name, charge,
                                                   important).c_str());
}

std::string SystemCommitChargeToString(const std::string& test_name,
                                       size_t charge,
                                       bool important) {
  std::string trace_name(test_name);
  std::string output;
  AppendResult(output, "commit_charge", "", "cc" + trace_name, charge, "kb",
               important);
  return output;
}

}  // namespace test
}  // namespace webrtc
