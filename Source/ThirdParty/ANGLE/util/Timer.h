//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_TIMER_H
#define SAMPLE_UTIL_TIMER_H

#include "util/util_export.h"

class ANGLE_UTIL_EXPORT Timer
{
  public:
    virtual ~Timer() {}

    // Timer functionality: Use start() and stop() to record the duration and use getElapsedTime()
    // to query that duration.  If getElapsedTime() is called in between, it will report the elapsed
    // time since start().
    virtual void start()                  = 0;
    virtual void stop()                   = 0;
    virtual double getElapsedTime() const = 0;

    // Timestamp functionality: Use getAbsoluteTime() to get an absolute time with an unknown
    // origin. This time moves forward regardless of start()/stop().
    virtual double getAbsoluteTime() = 0;
};

ANGLE_UTIL_EXPORT Timer *CreateTimer();

#endif  // SAMPLE_UTIL_TIMER_H
