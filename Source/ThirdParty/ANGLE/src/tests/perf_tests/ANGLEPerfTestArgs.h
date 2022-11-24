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

#include <string>
#include <vector>
#include "common/Optional.h"

namespace angle
{
extern bool gCalibration;
extern int gStepsPerTrial;
extern int gMaxStepsPerformed;
extern bool gEnableTrace;
extern const char *gTraceFile;
extern const char *gScreenshotDir;
extern bool gSaveScreenshots;
extern int gScreenshotFrame;
extern bool gVerboseLogging;
extern int gWarmupTrials;
extern int gWarmupSteps;
extern int gCalibrationTimeSeconds;
extern int gTrialTimeSeconds;
extern int gTestTrials;
extern bool gNoFinish;
extern bool gRetraceMode;
extern bool gMinimizeGPUWork;
extern bool gTraceTestValidation;
extern bool gTraceInterpreter;
extern const char *gPerfCounters;
extern const char *gUseANGLE;
extern const char *gUseGL;
extern bool gOffscreen;
extern bool gVsync;
extern const char *gPrintExtensionsToFile;
extern const char *gRequestedExtensions;

inline bool OneFrame()
{
    return gStepsPerTrial == 1 || gMaxStepsPerformed == 1;
}
}  // namespace angle

#endif  // TESTS_PERF_TESTS_ANGLE_PERF_TEST_ARGS_H_
