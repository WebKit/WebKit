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

#include "common/debug.h"
#include "util/test_utils.h"

namespace angle
{

constexpr int kDefaultStepsPerTrial    = 0;
constexpr int kDefaultTrialTimeSeconds = 0;
constexpr int kDefaultTestTrials       = 3;

bool gCalibration                  = false;
int gStepsPerTrial                 = kDefaultStepsPerTrial;
int gMaxStepsPerformed             = 0;
bool gEnableTrace                  = false;
const char *gTraceFile             = "ANGLETrace.json";
const char *gScreenshotDir         = nullptr;
const char *gRenderTestOutputDir   = nullptr;
bool gSaveScreenshots              = false;
int gScreenshotFrame               = 1;
bool gVerboseLogging               = false;
int gCalibrationTimeSeconds        = 1;
int gTrialTimeSeconds              = kDefaultTrialTimeSeconds;
int gTestTrials                    = kDefaultTestTrials;
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
int gFixedTestTimeWithWarmup       = 0;
bool gTraceInterpreter             = false;
const char *gPrintExtensionsToFile = nullptr;
const char *gRequestedExtensions   = nullptr;

// Default to three warmup trials. There's no science to this. More than two was experimentally
// helpful on a Windows NVIDIA setup when testing with Vulkan and native trace tests.
constexpr int kDefaultWarmupTrials = 3;
constexpr int kDefaultWarmupSteps  = 0;

int gWarmupTrials = kDefaultWarmupTrials;
int gWarmupSteps  = kDefaultWarmupSteps;

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
           ParseIntArg("--fixed-test-time-with-warmup", argc, argv, argIndex,
                       &gFixedTestTimeWithWarmup) ||
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
           ParseCStringArgWithHandling("--render-test-output-dir", argc, argv, argIndex,
                                       &gRenderTestOutputDir, ArgHandling::Preserve) ||
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
        // Ensure defaults were provided for params we're about to set
        ASSERT(gStepsPerTrial == kDefaultStepsPerTrial && gWarmupTrials == kDefaultWarmupTrials);

        gStepsPerTrial = 1;
        gWarmupTrials  = 0;
    }

    if (gCalibration)
    {
        gTestTrials = 0;
    }

    if (gMaxStepsPerformed > 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gWarmupTrials == kDefaultWarmupTrials && gTestTrials == kDefaultTestTrials &&
               gTrialTimeSeconds == kDefaultTrialTimeSeconds);

        gWarmupTrials     = 0;
        gTestTrials       = 1;
        gTrialTimeSeconds = 36000;
    }

    if (gFixedTestTime != 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gTrialTimeSeconds == kDefaultTrialTimeSeconds &&
               gStepsPerTrial == kDefaultStepsPerTrial && gTestTrials == kDefaultTestTrials &&
               gWarmupTrials == kDefaultWarmupTrials);

        gTrialTimeSeconds = gFixedTestTime;
        gStepsPerTrial    = std::numeric_limits<int>::max();
        gTestTrials       = 1;
        gWarmupTrials     = 0;
    }

    if (gFixedTestTimeWithWarmup != 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gTrialTimeSeconds == kDefaultTrialTimeSeconds &&
               gStepsPerTrial == kDefaultStepsPerTrial && gTestTrials == kDefaultTestTrials &&
               gWarmupTrials == kDefaultWarmupTrials && gWarmupSteps == kDefaultWarmupSteps);

        // This option is primarily useful for trace replays when you want to iterate once through
        // the trace to warm caches, then run for a fixed amount of time. It is equivalent to:
        // --trial-time X --steps-per-trial INF --trials 1 --warmup-trials 1 --warmup-steps <frames>
        gTrialTimeSeconds = gFixedTestTimeWithWarmup;
        gStepsPerTrial    = std::numeric_limits<int>::max();
        gTestTrials       = 1;
        gWarmupTrials     = 1;
        gWarmupSteps      = kAllFrames;
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
