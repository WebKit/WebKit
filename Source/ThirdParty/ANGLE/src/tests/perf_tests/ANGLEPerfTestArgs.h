//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTestArgs.h:
//   Command line arguments for angle_perftests.
//

#ifndef TESTS_PERF_TESTS_ANGLE_PERF_TEST_ARGS_H_
#define TESTS_PERF_TESTS_ANGLE_PERF_TEST_ARGS_H_

#include "common/Optional.h"

namespace angle
{
extern bool gCalibration;
extern Optional<unsigned int> gStepsToRunOverride;
extern bool gEnableTrace;
extern const char *gTraceFile;

inline bool OneFrame()
{
    return gStepsToRunOverride.valid() && gStepsToRunOverride.value() == 1;
}
}  // namespace angle

#endif  // TESTS_PERF_TESTS_ANGLE_PERF_TEST_ARGS_H_
