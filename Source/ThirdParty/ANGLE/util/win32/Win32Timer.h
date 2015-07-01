//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_WIN32_TIMER_H
#define SAMPLE_UTIL_WIN32_TIMER_H

#include "Timer.h"
#include <windows.h>

class Win32Timer : public Timer
{
  public:
    Win32Timer();

    void start();
    void stop();
    double getElapsedTime() const ;

  private:
    bool mRunning;
    LONGLONG mStartTime;
    LONGLONG mStopTime;

    LONGLONG mFrequency;
};

#endif // SAMPLE_UTIL_WIN32_TIMER_H
