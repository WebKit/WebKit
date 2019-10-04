/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#pragma once

#if OS(DARWIN)

#if !PLATFORM(IOS_FAMILY_SIMULATOR) && __has_include(<libproc.h>)
#    include <libproc.h>
#    if RUSAGE_INFO_CURRENT >= 4
#        define HAS_MAX_FOOTPRINT
#        if defined(RLIMIT_FOOTPRINT_INTERVAL) && __has_include(<libproc_internal.h>) \
             && (PLATFORM(IOS_FAMILY) \
                 || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400))
#            define HAS_RESET_FOOTPRINT_INTERVAL
#            define MAX_FOOTPRINT_FIELD ri_interval_max_phys_footprint
#            include <libproc_internal.h>
#        else
#            define MAX_FOOTPRINT_FIELD ri_lifetime_max_phys_footprint
#        endif
#    else
#        define HAS_ONLY_PHYS_FOOTPRINT
#    endif
#endif

struct ProcessMemoryFootprint {
public:
    uint64_t current;
    uint64_t peak;

    static ProcessMemoryFootprint now()
    {
#ifdef HAS_MAX_FOOTPRINT
        rusage_info_v4 rusage;
        if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&rusage))
            return { 0L, 0L };

        return { rusage.ri_phys_footprint, rusage.MAX_FOOTPRINT_FIELD };
#elif defined(HAS_ONLY_PHYS_FOOTPRINT)
        rusage_info_v0 rusage;
        if (proc_pid_rusage(getpid(), RUSAGE_INFO_V0, (rusage_info_t *)&rusage))
            return { 0L, 0L };

        return { rusage.ri_phys_footprint, 0L };
#else
        return { 0L, 0L };
#endif
    }

    static void resetPeak()
    {
#ifdef HAS_RESET_FOOTPRINT_INTERVAL
        proc_reset_footprint_interval(getpid());
#endif
    }
};

#endif
