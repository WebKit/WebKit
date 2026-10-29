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

#if defined(ANGLE_PLATFORM_ANDROID)
#    include "util/android/AndroidWindow.h"
#endif

namespace angle
{

constexpr int kDefaultStepsPerTrial    = std::numeric_limits<int>::max();
constexpr int kDefaultTrialTimeSeconds = 0;
constexpr int kDefaultTestTrials       = 3;

int gStepsPerTrial                 = kDefaultStepsPerTrial;
int gMaxStepsPerformed             = kDefaultMaxStepsPerformed;
bool gEnableTrace                  = false;
const char *gTraceFile             = "ANGLETrace.json";
const char *gScreenshotDir         = nullptr;
const char *gRenderTestOutputDir   = nullptr;
bool gSaveScreenshots              = false;
int gScreenshotFrame               = kDefaultScreenshotFrame;
bool gVerboseLogging               = false;
bool gWarmup                       = false;
int gTrialTimeSeconds              = kDefaultTrialTimeSeconds;
int gTestTrials                    = kDefaultTestTrials;
int gSleepBetweenTrialMs           = 0;
bool gNoFinish                     = false;
bool gRetraceMode                  = false;
bool gMinimizeGPUWork              = false;
bool gSkipBlitInOffscreen          = false;
bool gTraceTestValidation          = false;
const char *gPerfCounters          = nullptr;
const char *gUseANGLE              = nullptr;
const char *gUseGL                 = nullptr;
bool gOffscreen                    = false;
bool gVsync                        = false;
int gFpsLimit                      = 0;
bool gFpsLimitUsesBusyWait         = false;
bool gRunToKeyFrame                = false;
int gFixedTestTime                 = 0;
int gFixedTestTimeWithWarmup       = 0;
const char *gTraceInterpreter      = nullptr;
const char *gPrintExtensionsToFile = nullptr;
const char *gRequestedExtensions   = nullptr;
bool gIncludeInactiveResources     = false;
bool gTrackGPUTime                 = false;
bool gAddSwapIntoGPUTime           = false;
bool gTrackFrameWallTime           = false;
bool gAddSwapIntoFrameWallTime     = false;
int gTrackVulkanApiWallTime        = 0;
bool gCapturedFrameCountOnly       = false;

namespace
{
// Perf and trace test args stay in argv so that, in bot mode, TestSuite forwards them to the
// child processes that run the tests.
bool PerfTestArg(int *argc, char **argv, int argIndex)
{
    return ParseAndPreserveFlag("--run-to-key-frame", argc, argv, argIndex, &gRunToKeyFrame) ||
           ParseAndPreserveFlag("--enable-trace", argc, argv, argIndex, &gEnableTrace) ||
           ParseAndPreserveFlag("-v", argc, argv, argIndex, &gVerboseLogging) ||
           ParseAndPreserveFlag("--verbose", argc, argv, argIndex, &gVerboseLogging) ||
           ParseAndPreserveFlag("--verbose-logging", argc, argv, argIndex, &gVerboseLogging) ||
           ParseAndPreserveFlag("--no-finish", argc, argv, argIndex, &gNoFinish) ||
           ParseAndPreserveFlag("--warmup", argc, argv, argIndex, &gWarmup) ||
           ParseAndPreserveCStringArg("--trace-file", argc, argv, argIndex, &gTraceFile) ||
           ParseAndPreserveCStringArg("--perf-counters", argc, argv, argIndex, &gPerfCounters) ||
           ParseAndPreserveIntArg("--steps-per-trial", argc, argv, argIndex, &gStepsPerTrial) ||
           ParseAndPreserveIntArg("--max-steps-performed", argc, argv, argIndex,
                                  &gMaxStepsPerformed) ||
           ParseAndPreserveIntArg("--fixed-test-time", argc, argv, argIndex, &gFixedTestTime) ||
           ParseAndPreserveIntArg("--fixed-test-time-with-warmup", argc, argv, argIndex,
                                  &gFixedTestTimeWithWarmup) ||
           ParseAndPreserveIntArg("--trial-time", argc, argv, argIndex, &gTrialTimeSeconds) ||
           ParseAndPreserveIntArg("--max-trial-time", argc, argv, argIndex, &gTrialTimeSeconds) ||
           ParseAndPreserveIntArg("--trials", argc, argv, argIndex, &gTestTrials) ||
           ParseAndPreserveIntArg("--sleep-between-trials", argc, argv, argIndex,
                                  &gSleepBetweenTrialMs);
}

bool TraceTestArg(int *argc, char **argv, int argIndex)
{
    return ParseAndPreserveFlag("--retrace-mode", argc, argv, argIndex, &gRetraceMode) ||
           ParseAndPreserveFlag("--validation", argc, argv, argIndex, &gTraceTestValidation) ||
           ParseAndPreserveFlag("--save-screenshots", argc, argv, argIndex, &gSaveScreenshots) ||
           ParseAndPreserveFlag("--offscreen", argc, argv, argIndex, &gOffscreen) ||
           ParseAndPreserveFlag("--vsync", argc, argv, argIndex, &gVsync) ||
           ParseAndPreserveFlag("--minimize-gpu-work", argc, argv, argIndex, &gMinimizeGPUWork) ||
           ParseAndPreserveFlag("--skip-blit-in-offscreen", argc, argv, argIndex,
                                &gSkipBlitInOffscreen) ||
           ParseAndPreserveCStringArg("--trace-interpreter", argc, argv, argIndex,
                                      &gTraceInterpreter) ||
           ParseAndPreserveIntArg("--screenshot-frame", argc, argv, argIndex, &gScreenshotFrame) ||
           ParseAndPreserveIntArg("--fps-limit", argc, argv, argIndex, &gFpsLimit) ||
           ParseAndPreserveFlag("--fps-limit-uses-busy-wait", argc, argv, argIndex,
                                &gFpsLimitUsesBusyWait) ||
           ParseAndPreserveCStringArg("--render-test-output-dir", argc, argv, argIndex,
                                      &gRenderTestOutputDir) ||
           ParseAndPreserveCStringArg("--screenshot-dir", argc, argv, argIndex, &gScreenshotDir) ||
           ParseAndPreserveCStringArg("--use-angle", argc, argv, argIndex, &gUseANGLE) ||
           ParseAndPreserveCStringArg("--use-gl", argc, argv, argIndex, &gUseGL) ||
           ParseAndPreserveCStringArg("--print-extensions-to-file", argc, argv, argIndex,
                                      &gPrintExtensionsToFile) ||
           ParseAndPreserveCStringArg("--request-extensions", argc, argv, argIndex,
                                      &gRequestedExtensions) ||
           ParseAndPreserveFlag("--include-inactive-resources", argc, argv, argIndex,
                                &gIncludeInactiveResources) ||
           ParseAndPreserveFlag("--track-gpu-time", argc, argv, argIndex, &gTrackGPUTime) ||
           ParseAndPreserveFlag("--add-swap-into-gpu-time", argc, argv, argIndex,
                                &gAddSwapIntoGPUTime) ||
           ParseAndPreserveFlag("--track-frame-wall-time", argc, argv, argIndex,
                                &gTrackFrameWallTime) ||
           ParseAndPreserveFlag("--add-swap-into-frame-wall-time", argc, argv, argIndex,
                                &gAddSwapIntoFrameWallTime) ||
           ParseAndPreserveIntArg("--track-vulkan-api-wall-time", argc, argv, argIndex,
                                  &gTrackVulkanApiWallTime) ||
           ParseAndPreserveFlag("--captured-framecount-only", argc, argv, argIndex,
                                &gCapturedFrameCountOnly);
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

    if (gRunToKeyFrame || gMaxStepsPerformed > 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gTestTrials == kDefaultTestTrials && gTrialTimeSeconds == kDefaultTrialTimeSeconds);

        gTestTrials       = 1;
        gTrialTimeSeconds = 36000;
    }

    if (gFixedTestTime != 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gTrialTimeSeconds == kDefaultTrialTimeSeconds &&
               gStepsPerTrial == kDefaultStepsPerTrial && gTestTrials == kDefaultTestTrials);

        gTrialTimeSeconds = gFixedTestTime;
        gStepsPerTrial    = std::numeric_limits<int>::max();
        gTestTrials       = 1;
    }

    if (gFixedTestTimeWithWarmup != 0)
    {
        // Ensure defaults were provided for params we're about to set
        ASSERT(gTrialTimeSeconds == kDefaultTrialTimeSeconds &&
               gStepsPerTrial == kDefaultStepsPerTrial && gTestTrials == kDefaultTestTrials);

        // This option is primarily useful for trace replays when you want to iterate once through
        // the trace to warm caches, then run for a fixed amount of time. It is equivalent to:
        // --trial-time X --steps-per-trial INF --trials 1 --warmup
        gTrialTimeSeconds = gFixedTestTimeWithWarmup;
        gStepsPerTrial    = std::numeric_limits<int>::max();
        gTestTrials       = 1;
        gWarmup           = true;
    }

    if (gTrialTimeSeconds == 0)
    {
        gTrialTimeSeconds = 10;
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

    if (gAddSwapIntoFrameWallTime || gTrackVulkanApiWallTime > 0)
    {
        ASSERT(gTrackFrameWallTime);
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
        gTestTrials       = 1;
        gTrialTimeSeconds = 600;
    }

    if (kStandaloneBenchmark)
    {
        gVerboseLogging = true;
#if defined(ANGLE_PLATFORM_ANDROID)
        gScreenshotDir = strdup((AndroidWindow::GetApplicationDirectory() + "/files").c_str());
#else
        gScreenshotDir = ".";
#endif
        gSaveScreenshots = true;
        gUseANGLE        = "vulkan";
    }
}
