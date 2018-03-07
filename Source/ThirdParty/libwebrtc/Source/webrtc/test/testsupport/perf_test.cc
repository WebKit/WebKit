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

#include "test/testsupport/perf_test.h"

#include <sstream>
#include <stdio.h>
#include <vector>

namespace {

void PrintResultsImpl(const std::string& graph_name,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& units,
                      bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>

  if (important) {
    printf("*");
  }
  printf("RESULT %s: %s= %s %s\n", graph_name.c_str(), trace.c_str(),
         values.c_str(), units.c_str());
}

}  // namespace

namespace webrtc {
namespace test {

void PrintResult(const std::string& measurement,
                 const std::string& modifier,
                 const std::string& trace,
                 const double value,
                 const std::string& units,
                 bool important) {
  std::ostringstream value_stream;
  value_stream << value;
  PrintResultsImpl(measurement + modifier, trace, value_stream.str(), units,
                   important);
}

void PrintResultMeanAndError(const std::string& measurement,
                             const std::string& modifier,
                             const std::string& trace,
                             const double mean,
                             const double error,
                             const std::string& units,
                             bool important) {
  std::ostringstream value_stream;
  value_stream << '{' << mean << ',' << error << '}';
  PrintResultsImpl(measurement + modifier, trace, value_stream.str(), units,
                   important);
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::vector<double>& values,
                     const std::string& units,
                     bool important) {
  std::ostringstream value_stream;
  value_stream << '[';
  if (!values.empty()) {
    auto it = values.begin();
    while (true) {
      value_stream << *it;
      if (++it == values.end())
        break;
      value_stream << ',';
    }
  }
  value_stream << ']';
  PrintResultsImpl(measurement + modifier, trace, value_stream.str(), units,
                   important);
}

}  // namespace test
}  // namespace webrtc
