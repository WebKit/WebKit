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
bool gCalibration          = false;
int gStepsToRunOverride    = -1;
bool gEnableTrace          = false;
const char *gTraceFile     = "ANGLETrace.json";
const char *gScreenShotDir = nullptr;
}  // namespace angle

using namespace angle;

void ANGLEProcessPerfTestArgs(int *argc, char **argv)
{
    int argcOutCount = 0;

    for (int argIndex = 0; argIndex < *argc; argIndex++)
    {
        if (strcmp("--one-frame-only", argv[argIndex]) == 0)
        {
            gStepsToRunOverride = 1;
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
        }
        else if (strcmp("--steps", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            unsigned int stepsToRun = 0;
            std::stringstream strstr;
            strstr << argv[argIndex + 1];
            strstr >> stepsToRun;
            gStepsToRunOverride = stepsToRun;
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--screenshot-dir", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gScreenShotDir = argv[argIndex + 1];
            argIndex++;
        }
        else
        {
            argv[argcOutCount++] = argv[argIndex];
        }
    }

    *argc = argcOutCount;
}
