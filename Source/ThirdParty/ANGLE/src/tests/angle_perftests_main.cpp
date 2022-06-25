//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_perftests_main.cpp
//   Entry point for the gtest-based performance tests.
//

#include <gtest/gtest.h>

#include "test_utils/runner/TestSuite.h"

void ANGLEProcessPerfTestArgs(int *argc, char **argv);

#if defined(ANGLE_TRACE_PERF_TESTS)
void RegisterTraceTests();
#endif  // defined(ANGLE_TRACE_PERF_TESTS)

int main(int argc, char **argv)
{
    ANGLEProcessPerfTestArgs(&argc, argv);
#if defined(ANGLE_TRACE_PERF_TESTS)
    RegisterTraceTests();
#endif  // defined(ANGLE_TRACE_PERF_TESTS)
    angle::TestSuite testSuite(&argc, argv);
    return testSuite.run();
}
