/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurrentTime.h"

#if PLATFORM(MAC)
#include <mach/mach_time.h>
#include <sys/time.h>
#elif OS(WINDOWS)

// Windows is first since we want to use hires timers, despite USE(CF)
// being defined.
// If defined, WIN32_LEAN_AND_MEAN disables timeBeginPeriod/timeEndPeriod.
#undef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#if USE(QUERY_PERFORMANCE_COUNTER)
#if OS(WINCE)
extern "C" time_t mktime(struct tm *t);
#else
#include <sys/timeb.h>
#include <sys/types.h>
#endif
#endif

#elif PLATFORM(WX)
#include <wx/datetime.h>
#elif PLATFORM(EFL)
#include <Ecore.h>
#else
#include <sys/time.h>
#endif

#if PLATFORM(GTK)
#include <glib.h>
#endif

#if PLATFORM(QT)
#include <QElapsedTimer>
#endif

#if PLATFORM(CHROMIUM)
#error Chromium uses a different timer implementation
#endif

namespace WTF {

const double msPerSecond = 1000.0;

#if OS(WINDOWS)

#if USE(QUERY_PERFORMANCE_COUNTER)

static LARGE_INTEGER qpcFrequency;
static bool syncedTime;

static double highResUpTime()
{
    // We use QPC, but only after sanity checking its result, due to bugs:
    // http://support.microsoft.com/kb/274323
    // http://support.microsoft.com/kb/895980
    // http://msdn.microsoft.com/en-us/library/ms644904.aspx ("...you can get different results on different processors due to bugs in the basic input/output system (BIOS) or the hardware abstraction layer (HAL)."

    static LARGE_INTEGER qpcLast;
    static DWORD tickCountLast;
    static bool inited;

    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    DWORD tickCount = GetTickCount();

    if (inited) {
        __int64 qpcElapsed = ((qpc.QuadPart - qpcLast.QuadPart) * 1000) / qpcFrequency.QuadPart;
        __int64 tickCountElapsed;
        if (tickCount >= tickCountLast)
            tickCountElapsed = (tickCount - tickCountLast);
        else {
#if COMPILER(MINGW)
            __int64 tickCountLarge = tickCount + 0x100000000ULL;
#else
            __int64 tickCountLarge = tickCount + 0x100000000I64;
#endif
            tickCountElapsed = tickCountLarge - tickCountLast;
        }

        // force a re-sync if QueryPerformanceCounter differs from GetTickCount by more than 500ms.
        // (500ms value is from http://support.microsoft.com/kb/274323)
        __int64 diff = tickCountElapsed - qpcElapsed;
        if (diff > 500 || diff < -500)
            syncedTime = false;
    } else
        inited = true;

    qpcLast = qpc;
    tickCountLast = tickCount;

    return (1000.0 * qpc.QuadPart) / static_cast<double>(qpcFrequency.QuadPart);
}

static double lowResUTCTime()
{
#if OS(WINCE)
    SYSTEMTIME systemTime;
    GetSystemTime(&systemTime);
    struct tm tmtime;
    tmtime.tm_year = systemTime.wYear - 1900;
    tmtime.tm_mon = systemTime.wMonth - 1;
    tmtime.tm_mday = systemTime.wDay;
    tmtime.tm_wday = systemTime.wDayOfWeek;
    tmtime.tm_hour = systemTime.wHour;
    tmtime.tm_min = systemTime.wMinute;
    tmtime.tm_sec = systemTime.wSecond;
    time_t timet = mktime(&tmtime);
    return timet * msPerSecond + systemTime.wMilliseconds;
#else
    struct _timeb timebuffer;
    _ftime(&timebuffer);
    return timebuffer.time * msPerSecond + timebuffer.millitm;
#endif
}

static bool qpcAvailable()
{
    static bool available;
    static bool checked;

    if (checked)
        return available;

    available = QueryPerformanceFrequency(&qpcFrequency);
    checked = true;
    return available;
}

double currentTime()
{
    // Use a combination of ftime and QueryPerformanceCounter.
    // ftime returns the information we want, but doesn't have sufficient resolution.
    // QueryPerformanceCounter has high resolution, but is only usable to measure time intervals.
    // To combine them, we call ftime and QueryPerformanceCounter initially. Later calls will use QueryPerformanceCounter
    // by itself, adding the delta to the saved ftime.  We periodically re-sync to correct for drift.
    static double syncLowResUTCTime;
    static double syncHighResUpTime;
    static double lastUTCTime;

    double lowResTime = lowResUTCTime();

    if (!qpcAvailable())
        return lowResTime / 1000.0;

    double highResTime = highResUpTime();

    if (!syncedTime) {
        timeBeginPeriod(1); // increase time resolution around low-res time getter
        syncLowResUTCTime = lowResTime = lowResUTCTime();
        timeEndPeriod(1); // restore time resolution
        syncHighResUpTime = highResTime;
        syncedTime = true;
    }

    double highResElapsed = highResTime - syncHighResUpTime;
    double utc = syncLowResUTCTime + highResElapsed;

    // force a clock re-sync if we've drifted
    double lowResElapsed = lowResTime - syncLowResUTCTime;
    const double maximumAllowedDriftMsec = 15.625 * 2.0; // 2x the typical low-res accuracy
    if (fabs(highResElapsed - lowResElapsed) > maximumAllowedDriftMsec)
        syncedTime = false;

    // make sure time doesn't run backwards (only correct if difference is < 2 seconds, since DST or clock changes could occur)
    const double backwardTimeLimit = 2000.0;
    if (utc < lastUTCTime && (lastUTCTime - utc) < backwardTimeLimit)
        return lastUTCTime / 1000.0;
    lastUTCTime = utc;
    return utc / 1000.0;
}

#else

static double currentSystemTime()
{
    FILETIME ft;
    GetCurrentFT(&ft);

    // As per Windows documentation for FILETIME, copy the resulting FILETIME structure to a
    // ULARGE_INTEGER structure using memcpy (using memcpy instead of direct assignment can
    // prevent alignment faults on 64-bit Windows).

    ULARGE_INTEGER t;
    memcpy(&t, &ft, sizeof(t));

    // Windows file times are in 100s of nanoseconds.
    // To convert to seconds, we have to divide by 10,000,000, which is more quickly
    // done by multiplying by 0.0000001.

    // Between January 1, 1601 and January 1, 1970, there were 369 complete years,
    // of which 89 were leap years (1700, 1800, and 1900 were not leap years).
    // That is a total of 134774 days, which is 11644473600 seconds.

    return t.QuadPart * 0.0000001 - 11644473600.0;
}

double currentTime()
{
    static bool init = false;
    static double lastTime;
    static DWORD lastTickCount;
    if (!init) {
        lastTime = currentSystemTime();
        lastTickCount = GetTickCount();
        init = true;
        return lastTime;
    }

    DWORD tickCountNow = GetTickCount();
    DWORD elapsed = tickCountNow - lastTickCount;
    double timeNow = lastTime + (double)elapsed / 1000.;
    if (elapsed >= 0x7FFFFFFF) {
        lastTime = timeNow;
        lastTickCount = tickCountNow;
    }
    return timeNow;
}

#endif // USE(QUERY_PERFORMANCE_COUNTER)

#elif PLATFORM(GTK)

// Note: GTK on Windows will pick up the PLATFORM(WIN) implementation above which provides
// better accuracy compared with Windows implementation of g_get_current_time:
// (http://www.google.com/codesearch/p?hl=en#HHnNRjks1t0/glib-2.5.2/glib/gmain.c&q=g_get_current_time).
// Non-Windows GTK builds could use gettimeofday() directly but for the sake of consistency lets use GTK function.
double currentTime()
{
    GTimeVal now;
    g_get_current_time(&now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_usec / 1000000.0);
}

#elif PLATFORM(WX)

double currentTime()
{
    wxDateTime now = wxDateTime::UNow();
    return (double)now.GetTicks() + (double)(now.GetMillisecond() / 1000.0);
}

#elif PLATFORM(EFL)

double currentTime()
{
    return ecore_time_unix_get();
}

#else

double currentTime()
{
    struct timeval now;
    gettimeofday(&now, 0);
    return now.tv_sec + now.tv_usec / 1000000.0;
}

#endif

#if PLATFORM(MAC)

double monotonicallyIncreasingTime()
{
    // Based on listing #2 from Apple QA 1398.
    static mach_timebase_info_data_t timebaseInfo;
    if (!timebaseInfo.denom) {
        kern_return_t kr = mach_timebase_info(&timebaseInfo);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }
    return (mach_absolute_time() * timebaseInfo.numer) / (1.0e9 * timebaseInfo.denom);
}

#elif PLATFORM(EFL)

double monotonicallyIncreasingTime()
{
    return ecore_time_get();
}

#elif PLATFORM(GTK)

double monotonicallyIncreasingTime()
{
    return static_cast<double>(g_get_monotonic_time() / 1000000.0);
}

#elif PLATFORM(QT)

double monotonicallyIncreasingTime()
{
    ASSERT(QElapsedTimer::isMonotonic());
    static QElapsedTimer timer;
    return timer.nsecsElapsed() / 1.0e9;
}

#else

double monotonicallyIncreasingTime()
{
    static double lastTime = 0;
    double currentTimeNow = currentTime();
    if (currentTimeNow < lastTime)
        return lastTime;
    lastTime = currentTimeNow;
    return currentTimeNow;
}

#endif

} // namespace WTF
