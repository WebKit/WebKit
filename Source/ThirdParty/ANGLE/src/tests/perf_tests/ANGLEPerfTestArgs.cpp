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

#include "util/test_utils.h"

namespace angle
{
bool gCalibration                  = false;
int gStepsPerTrial                 = 0;
int gMaxStepsPerformed             = 0;
bool gEnableTrace                  = false;
const char *gTraceFile             = "ANGLETrace.json";
const char *gScreenshotDir         = nullptr;
const char *gRenderTestOutputDir   = nullptr;
bool gSaveScreenshots              = false;
int gScreenshotFrame               = 1;
bool gVerboseLogging               = false;
int gCalibrationTimeSeconds        = 1;
int gTrialTimeSeconds              = 0;
int gTestTrials                    = 3;
bool gNoFinish                     = false;
bool gRetraceMode                  = false;
bool gMinimizeGPUWork              = false;
bool gTraceTestValidation          = false;
const char *gPerfCounters          = nullptr;
const char *gUseANGLE              = nullptr;
const char *gUseGL                 = nullptr;
bool gOffscreen                    = false;
bool gVsync                        = false;
bool gOneFrameOnly                 = false;
bool gNoWarmup                     = false;
int gFixedTestTime                 = 0;
bool gTraceInterpreter             = false;
const char *gPrintExtensionsToFile = nullptr;
const char *gRequestedExtensions   = nullptr;

// Default to three warmup trials. There's no science to this. More than two was experimentally
// helpful on a Windows NVIDIA setup when testing with Vulkan and native trace tests.
int gWarmupTrials = 3;
int gWarmupSteps  = std::numeric_limits<int>::max();

namespace
{
bool PerfTestArg(int *argc, char **argv, int argIndex)
{
    return ParseFlag("--one-frame-only", argc, argv, argIndex, &gOneFrameOnly) ||
           ParseFlag("--enable-trace", argc, argv, argIndex, &gEnableTrace) ||
           ParseFlag("--calibration", argc, argv, argIndex, &gCalibration) ||
           ParseFlag("-v", argc, argv, argIndex, &gVerboseLogging) ||
           ParseFlag("--verbose", argc, argv, argIndex, &gVerboseLogging) ||
           ParseFlag("--verbose-logging", argc, argv, argIndex, &gVerboseLogging) ||
           ParseFlag("--no-warmup", argc, argv, argIndex, &gNoWarmup) ||
           ParseFlag("--no-finish", argc, argv, argIndex, &gNoFinish) ||
           ParseCStringArg("--trace-file", argc, argv, argIndex, &gTraceFile) ||
           ParseCStringArg("--perf-counters", argc, argv, argIndex, &gPerfCounters) ||
           ParseIntArg("--steps-per-trial", argc, argv, argIndex, &gStepsPerTrial) ||
           ParseIntArg("--max-steps-performed", argc, argv, argIndex, &gMaxStepsPerformed) ||
           ParseIntArg("--fixed-test-time", argc, argv, argIndex, &gFixedTestTime) ||
           ParseIntArg("--warmup-trials", argc, argv, argIndex, &gWarmupTrials) ||
           ParseIntArg("--warmup-steps", argc, argv, argIndex, &gWarmupSteps) ||
           ParseIntArg("--calibration-time", argc, argv, argIndex, &gCalibrationTimeSeconds) ||
           ParseIntArg("--trial-time", argc, argv, argIndex, &gTrialTimeSeconds) ||
           ParseIntArg("--max-trial-time", argc, argv, argIndex, &gTrialTimeSeconds) ||
           ParseIntArg("--trials", argc, argv, argIndex, &gTestTrials);
}

bool TraceTestArg(int *argc, char **argv, int argIndex)
{
    return ParseFlag("--retrace-mode", argc, argv, argIndex, &gRetraceMode) ||
           ParseFlag("--validation", argc, argv, argIndex, &gTraceTestValidation) ||
           ParseFlag("--save-screenshots", argc, argv, argIndex, &gSaveScreenshots) ||
           ParseFlag("--offscreen", argc, argv, argIndex, &gOffscreen) ||
           ParseFlag("--vsync", argc, argv, argIndex, &gVsync) ||
           ParseFlag("--minimize-gpu-work", argc, argv, argIndex, &gMinimizeGPUWork) ||
           ParseFlag("--trace-interpreter", argc, argv, argIndex, &gTraceInterpreter) ||
           ParseFlag("--interpreter", argc, argv, argIndex, &gTraceInterpreter) ||
           ParseIntArg("--screenshot-frame", argc, argv, argIndex, &gScreenshotFrame) ||
           ParseCStringArg("--render-test-output-dir", argc, argv, argIndex,
                           &gRenderTestOutputDir) ||
           ParseCStringArg("--screenshot-dir", argc, argv, argIndex, &gScreenshotDir) ||
           ParseCStringArg("--use-angle", argc, argv, argIndex, &gUseANGLE) ||
           ParseCStringArg("--use-gl", argc, argv, argIndex, &gUseGL) ||
           ParseCStringArg("--print-extensions-to-file", argc, argv, argIndex,
                           &gPrintExtensionsToFile) ||
           ParseCStringArg("--request-extensions", argc, argv, argIndex, &gRequestedExtensions);
}
}  // namespace
}  // namespace angle

using namespace angle;

void ANGLEProcessPerfTestArgs(int *argc, char **argv)
{
    for (int argIndex = 1; argIndex < *argc;)
    {
        if (!PerfTestArg(argc, argv, argIndex))
        {
            argIndex++;
        }
    }

    if (gOneFrameOnly)
    {
        gStepsPerTrial = 1;
        gWarmupTrials  = 0;
    }

    if (gCalibration)
    {
        gTestTrials = 0;
    }

    if (gMaxStepsPerformed > 0)
    {
        gWarmupTrials     = 0;
        gTestTrials       = 1;
        gTrialTimeSeconds = 36000;
    }

    if (gFixedTestTime != 0)
    {
        gTrialTimeSeconds = gFixedTestTime;
        gStepsPerTrial    = std::numeric_limits<int>::max();
        gTestTrials       = 1;
        gWarmupTrials     = 0;
    }

    if (gNoWarmup)
    {
        gWarmupTrials = 0;
    }

    if (gTrialTimeSeconds == 0)
    {
        gTrialTimeSeconds = 10;
    }
    else
    {
        gCalibrationTimeSeconds = gTrialTimeSeconds;
    }
}

void ANGLEProcessTraceTestArgs(int *argc, char **argv)
{
    ANGLEProcessPerfTestArgs(argc, argv);

    for (int argIndex = 1; argIndex < *argc;)
    {
        if (!TraceTestArg(argc, argv, argIndex))
        {
            argIndex++;
        }
    }

    if (gScreenshotDir)
    {
        // implicitly set here but not when using kRenderTestOutputDir
        gSaveScreenshots = true;
    }

    if (gRenderTestOutputDir)
    {
        gScreenshotDir = gRenderTestOutputDir;
    }

    if (gTraceTestValidation)
    {
        gWarmupTrials     = 0;
        gTestTrials       = 1;
        gTrialTimeSeconds = 600;
    }
}
