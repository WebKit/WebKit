/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_monotonic_time.h"
#if PAS_OS(DARWIN)
#include <mach/mach_time.h>
#endif

#if PAS_OS(DARWIN)
static mach_timebase_info_data_t timebase_info;
static mach_timebase_info_data_t* timebase_info_ptr;

static PAS_NEVER_INLINE mach_timebase_info_data_t* get_timebase_info_slow(void)
{
    kern_return_t kern_return;
    kern_return = mach_timebase_info(&timebase_info);
    PAS_ASSERT(kern_return == KERN_SUCCESS);
    pas_fence();
    timebase_info_ptr = &timebase_info;
    return &timebase_info;
}

static mach_timebase_info_data_t* get_timebase_info(void)
{
    mach_timebase_info_data_t* result;

    result = timebase_info_ptr;
    if (PAS_LIKELY(result))
        return result;

    return get_timebase_info_slow();
}

uint64_t pas_get_current_monotonic_time_nanoseconds(void)
{
    uint64_t result;
    mach_timebase_info_data_t* info;

    info = get_timebase_info();

    result = mach_approximate_time();
    result *= info->numer;
    result /= info->denom;

    return result;
}

#elif PAS_OS(LINUX)

uint64_t pas_get_current_monotonic_time_nanoseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    return ts.tv_sec * 1.0e9 + ts.tv_nsec;
}

#elif PAS_PLATFORM(PLAYSTATION)

uint64_t pas_get_current_monotonic_time_nanoseconds(void)
{
    struct timespec ts;
    clock_gettime_np(CLOCK_MONOTONIC_FAST, &ts);
    return ts.tv_sec * 1000u * 1000u * 1000u + ts.tv_nsec;
}

#endif

#endif /* LIBPAS_ENABLED */
