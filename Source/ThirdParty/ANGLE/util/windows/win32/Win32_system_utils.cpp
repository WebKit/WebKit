//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Win32_system_utils.cpp: Implementation of OS-specific functions for Win32 (Windows)

#include "util/system_utils.h"

#include <windows.h>
#include <array>

namespace angle
{

void SetLowPriorityProcess()
{
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
}

bool StabilizeCPUForBenchmarking()
{
    if (SetThreadAffinityMask(GetCurrentThread(), 1) == 0)
    {
        return false;
    }
    if (SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) == FALSE)
    {
        return false;
    }
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) == FALSE)
    {
        return false;
    }

    return true;
}
}  // namespace angle
