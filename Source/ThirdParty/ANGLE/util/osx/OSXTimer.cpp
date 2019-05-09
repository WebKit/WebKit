//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// OSXTimer.cpp: Implementation of a high precision timer class on OSX

#include "util/osx/OSXTimer.h"

#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

OSXTimer::OSXTimer() : mRunning(false), mSecondCoeff(0) {}

double OSXTimer::getSecondCoeff()
{
    // If this is the first time we've run, get the timebase.
    if (mSecondCoeff == 0.0)
    {
        mach_timebase_info_data_t timebaseInfo;
        mach_timebase_info(&timebaseInfo);

        mSecondCoeff = timebaseInfo.numer * (1.0 / 1000000000) / timebaseInfo.denom;
    }

    return mSecondCoeff;
}

void OSXTimer::start()
{
    mStartTime = mach_absolute_time();
    // Cache secondCoeff
    getSecondCoeff();
    mRunning = true;
}

void OSXTimer::stop()
{
    mStopTime = mach_absolute_time();
    mRunning  = false;
}

double OSXTimer::getElapsedTime() const
{
    if (mRunning)
    {
        return mSecondCoeff * (mach_absolute_time() - mStartTime);
    }
    else
    {
        return mSecondCoeff * (mStopTime - mStartTime);
    }
}

double OSXTimer::getAbsoluteTime()
{
    return getSecondCoeff() * mach_absolute_time();
}

Timer *CreateTimer()
{
    return new OSXTimer();
}
