//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowsTimer.cpp: Implementation of a high precision timer class on Windows

#include "util/windows/WindowsTimer.h"

WindowsTimer::WindowsTimer() : mRunning(false), mStartTime(0), mStopTime(0), mFrequency(0) {}

LONGLONG WindowsTimer::getFrequency()
{
    if (mFrequency == 0)
    {
        LARGE_INTEGER frequency = {};
        QueryPerformanceFrequency(&frequency);

        mFrequency = frequency.QuadPart;
    }

    return mFrequency;
}

void WindowsTimer::start()
{
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);
    mStartTime = curTime.QuadPart;

    // Cache the frequency
    getFrequency();

    mRunning = true;
}

void WindowsTimer::stop()
{
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);
    mStopTime = curTime.QuadPart;

    mRunning = false;
}

double WindowsTimer::getElapsedTime() const
{
    LONGLONG endTime;
    if (mRunning)
    {
        LARGE_INTEGER curTime;
        QueryPerformanceCounter(&curTime);
        endTime = curTime.QuadPart;
    }
    else
    {
        endTime = mStopTime;
    }

    return static_cast<double>(endTime - mStartTime) / mFrequency;
}

double WindowsTimer::getAbsoluteTime()
{
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);

    return static_cast<double>(curTime.QuadPart) / getFrequency();
}

Timer *CreateTimer()
{
    return new WindowsTimer();
}
