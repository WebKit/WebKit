/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
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

#if OS(LINUX)

#include <sys/resource.h>
#include <wtf/linux/CurrentProcessMemoryStatus.h>

struct ProcessMemoryFootprint {
    uint64_t current;
    uint64_t peak;

    static ProcessMemoryFootprint now()
    {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);

        ProcessMemoryStatus ps;
        currentProcessMemoryStatus(ps);

        return { ps.resident, static_cast<uint64_t>(ru.ru_maxrss) * 1024 };
    }

    static void resetPeak()
    {
        // To reset the peak size, we need to write 5 to /proc/self/clear_refs
        // as described in `man -s5 proc`, in the clear_refs section.
        // Only available since 4.0.
        FILE* f = fopen("/proc/self/clear_refs", "w");
        if (!f)
            return;
        fwrite("5", 1, 1, f);
        fclose(f);
    }
};

#endif
