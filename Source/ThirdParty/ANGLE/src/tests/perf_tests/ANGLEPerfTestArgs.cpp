//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTestArgs.cpp:
//   Parse command line arguments for angle_perftests.
//

#include "ANGLEPerfTestArgs.h"
#include <string.h>
#include <sstream>

namespace angle
{
bool gCalibration              = false;
int gStepsPerTrial             = 0;
int gMaxStepsPerformed         = 0;
bool gEnableTrace              = false;
const char *gTraceFile         = "ANGLETrace.json";
const char *gScreenShotDir     = nullptr;
int gScreenShotFrame           = 1;
bool gVerboseLogging           = false;
double gCalibrationTimeSeconds = 1.0;
double gMaxTrialTimeSeconds    = 10.0;
int gTestTrials                = 3;
bool gNoFinish                 = false;
bool gEnableAllTraceTests      = false;
bool gRetraceMode              = false;
bool gMinimizeGPUWork          = false;
bool gTraceTestValidation      = false;
const char *gPerfCounters      = nullptr;

// Default to three warmup loops. There's no science to this. More than two loops was experimentally
// helpful on a Windows NVIDIA setup when testing with Vulkan and native trace tests.
int gWarmupLoops = 3;
int gWarmupSteps = std::numeric_limits<int>::max();
}  // namespace angle

namespace
{
int ReadIntArgument(const char *arg)
{
    std::stringstream strstr;
    strstr << arg;

    int value;
    strstr >> value;
    return value;
}

// The same as --screenshot-dir, but used by Chrome tests.
constexpr char kRenderTestDirArg[] = "--render-test-output-dir=";
}  // namespace

using namespace angle;

void ANGLEProcessPerfTestArgs(int *argc, char **argv)
{
    for (int argIndex = 0; argIndex < *argc; argIndex++)
    {
        if (strcmp("--one-frame-only", argv[argIndex]) == 0)
        {
            gStepsPerTrial = 1;
            gWarmupLoops   = 0;
        }
        else if (strcmp("--enable-trace", argv[argIndex]) == 0)
        {
            gEnableTrace = true;
        }
        else if (strcmp("--trace-file", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gTraceFile = argv[argIndex + 1];
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--calibration", argv[argIndex]) == 0)
        {
            gCalibration = true;
            gTestTrials  = 0;
        }
        else if (strcmp("--steps-per-trial", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gStepsPerTrial = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--max-steps-performed", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gMaxStepsPerformed   = ReadIntArgument(argv[argIndex + 1]);
            gWarmupLoops         = 0;
            gTestTrials          = 1;
            gMaxTrialTimeSeconds = 36000;
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--fixed-test-time", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gMaxTrialTimeSeconds = ReadIntArgument(argv[argIndex + 1]);
            gStepsPerTrial       = std::numeric_limits<int>::max();
            gTestTrials          = 1;
            gWarmupLoops         = 0;
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--screenshot-dir", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gScreenShotDir = argv[argIndex + 1];
            argIndex++;
        }
        else if (strcmp("--screenshot-frame", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gScreenShotFrame = ReadIntArgument(argv[argIndex + 1]);
            argIndex++;
        }
        else if (strcmp("--verbose-logging", argv[argIndex]) == 0 ||
                 strcmp("--verbose", argv[argIndex]) == 0 || strcmp("-v", argv[argIndex]) == 0)
        {
            gVerboseLogging = true;
        }
        else if (strcmp("--warmup-loops", argv[argIndex]) == 0)
        {
            gWarmupLoops = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--warmup-steps", argv[argIndex]) == 0)
        {
            gWarmupSteps = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--no-warmup", argv[argIndex]) == 0)
        {
            gWarmupLoops = 0;
        }
        else if (strncmp(kRenderTestDirArg, argv[argIndex], strlen(kRenderTestDirArg)) == 0)
        {
            gScreenShotDir = argv[argIndex] + strlen(kRenderTestDirArg);
        }
        else if (strcmp("--calibration-time", argv[argIndex]) == 0)
        {
            gCalibrationTimeSeconds = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--max-trial-time", argv[argIndex]) == 0)
        {
            gMaxTrialTimeSeconds = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--trials", argv[argIndex]) == 0)
        {
            gTestTrials = ReadIntArgument(argv[argIndex + 1]);
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--no-finish", argv[argIndex]) == 0)
        {
            gNoFinish = true;
        }
        else if (strcmp("--enable-all-trace-tests", argv[argIndex]) == 0)
        {
            gEnableAllTraceTests = true;
        }
        else if (strcmp("--retrace-mode", argv[argIndex]) == 0)
        {
            gRetraceMode = true;
        }
        else if (strcmp("--minimize-gpu-work", argv[argIndex]) == 0)
        {
            gMinimizeGPUWork = true;
        }
        else if (strcmp("--validation", argv[argIndex]) == 0)
        {
            gTraceTestValidation = true;
            gWarmupLoops         = 0;
            gTestTrials          = 1;
            gMaxTrialTimeSeconds = 600.0;
        }
        else if (strcmp("--perf-counters", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gPerfCounters = argv[argIndex + 1];
            argIndex++;
        }
    }
}
