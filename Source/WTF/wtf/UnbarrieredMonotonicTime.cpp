/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/UnbarrieredMonotonicTime.h>

#include <wtf/ContinuousTime.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PrintStream.h>
#include <wtf/WallTime.h>

#if OS(DARWIN)
#include <mach/mach_time.h>
#endif

namespace WTF {

#if USE(HARDWARE_UNBARRIERED_MONOTONIC_TIME)
UnbarrieredMonotonicTime::Calibration UnbarrieredMonotonicTime::s_calibration;

void UnbarrieredMonotonicTime::calibrate()
{
    constexpr double nanosecondsPerSecond = 1000'000'000;
    static Lock lock;

    Locker locker { lock };
    if (s_calibration.nanosecondsPerTick)
        return; // Already calibrated.

    auto readCounterFrequency = [] -> uint64_t {
        uint64_t val;
        __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(val) : : "memory");
        return val;
    };

    // We're going straight to mach_absolute_time() instead of MonotonicTime::now()
    // because we want to miminize the time difference between sampling it and sampling
    // CNTVCT_EL0. MonotonicTime::now() can be deterministically computed from the
    // value of mach_absolute_time().
    //
    // Note that the value of mach_absolute_time() relies on when the kernel samples
    // CNTVCT_EL0 and update it. Effectively, mach_absolute_time may be of a lower
    // resolution than CNTVCT_EL0 depending on when the kernel updates it. So, for our
    // calibration, to minimize the difference between the 2 values, we'll sample
    // them until we see mach_absolute_time()'s value change 2 times. This allows
    // us to catch the CNTVCT_EL0 value right as mach_absolute_time() changes.
    //
    // We look for 2 transitions of mach_absolute_time() because the first transition
    // may race against us and occur just as we enter the sampling loop. Waiting for
    // the 2nd transition reduces the impact of such a race.

    uint64_t counter = readCounter();
    uint64_t startAbsTime = mach_absolute_time();
    uint64_t absTime = startAbsTime;
    for (int i = 0; i < 2; ++i) {
        do {
            counter = readCounter();
            absTime = mach_absolute_time();
        } while (absTime == startAbsTime);
    }

    // Now that we have the closely paired samples of mach_absolute_time() and CNTVCT_EL0,
    // determinstically compute the calibration values.
    mach_timebase_info_data_t info;
    kern_return_t kr = mach_timebase_info(&info);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    s_calibration.startCounter = counter;
    s_calibration.startNanoseconds = absTime * info.numer / (double)info.denom;

    uint64_t ticksPerSecond = readCounterFrequency();
    s_calibration.nanosecondsPerTick = nanosecondsPerSecond / ticksPerSecond;
}
#endif

void UnbarrieredMonotonicTime::dump(PrintStream& out) const
{
    out.print("UnbarrieredMonotonic(", m_value, " sec)");
}

} // namespace WTF
