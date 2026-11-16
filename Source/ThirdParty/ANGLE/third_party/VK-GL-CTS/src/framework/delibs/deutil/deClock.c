/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief System clock.
 *//*--------------------------------------------------------------------*/

#include "deClock.h"

#include <time.h>

#if (DE_OS == DE_OS_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_SYMBIAN) || (DE_OS == DE_OS_QNX)
#include <time.h>
#elif (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
#include <mach/mach_time.h>
#endif

uint64_t deGetMicroseconds(void)
{
#if (DE_OS == DE_OS_WIN32)
    LARGE_INTEGER freq;
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    DE_ASSERT(freq.LowPart != 0 || freq.HighPart != 0);
    /* \todo [2010-03-26 kalle] consider adding a 32bit-friendly implementation */

    if (count.QuadPart < MAXLONGLONG / 1000000)
    {
        DE_ASSERT(freq.QuadPart != 0);
        return count.QuadPart * 1000000 / freq.QuadPart;
    }
    else
    {
        DE_ASSERT(freq.QuadPart >= 1000000);
        return count.QuadPart / (freq.QuadPart / 1000000);
    }

#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_QNX) || (DE_OS == DE_OS_FUCHSIA)
    struct timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);
    return (uint64_t)currTime.tv_sec * 1000000 + ((uint64_t)currTime.tv_nsec / 1000);

#elif (DE_OS == DE_OS_SYMBIAN)
    struct timespec currTime;
    /* Symbian supports only realtime clock for clock_gettime. */
    /* \todo [2011-08-22 kalle] Proper Symbian-based implementation that is guaranteed to be monotonic. */
    clock_gettime(CLOCK_REALTIME, &currTime);
    return (uint64_t)currTime.tv_sec * 1000000 + ((uint64_t)currTime.tv_nsec / 1000);

#elif (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
    mach_timebase_info_data_t mach_timebase;
    mach_timebase_info(&mach_timebase);
    return mach_absolute_time() * mach_timebase.numer / mach_timebase.denom / 1000;

#else
#error "Not implemented for target OS"
#endif
}

uint64_t deGetTime(void)
{
    return (uint64_t)time(NULL);
}
