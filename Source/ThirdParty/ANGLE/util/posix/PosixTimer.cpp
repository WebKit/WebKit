//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PosixTimer.cpp: Implementation of a high precision timer class on POSIX

#include "util/posix/PosixTimer.h"
#include <iostream>

PosixTimer::PosixTimer() : mRunning(false) {}

namespace
{
uint64_t getCurrentTimeNs()
{
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    return currentTime.tv_sec * 1'000'000'000llu + currentTime.tv_nsec;
}
}  // anonymous namespace

void PosixTimer::start()
{
    mStartTimeNs = getCurrentTimeNs();
    mRunning     = true;
}

void PosixTimer::stop()
{
    mStopTimeNs = getCurrentTimeNs();
    mRunning    = false;
}

double PosixTimer::getElapsedTime() const
{
    uint64_t endTimeNs;
    if (mRunning)
    {
        endTimeNs = getCurrentTimeNs();
    }
    else
    {
        endTimeNs = mStopTimeNs;
    }

    return (endTimeNs - mStartTimeNs) * 1e-9;
}

double PosixTimer::getAbsoluteTime()
{
    return getCurrentTimeNs() * 1e-9;
}

Timer *CreateTimer()
{
    return new PosixTimer();
}
