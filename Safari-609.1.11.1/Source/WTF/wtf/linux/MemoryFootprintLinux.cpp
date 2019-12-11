/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include <wtf/MemoryFootprint.h>

#include <stdio.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WTF {

static const Seconds s_memoryFootprintUpdateInterval = 1_s;

template<typename Functor>
static void forEachLine(FILE* file, Functor functor)
{
    char* buffer = nullptr;
    size_t size = 0;
    while (getline(&buffer, &size, file) != -1) {
        functor(buffer);
    }
    free(buffer);
}

static size_t computeMemoryFootprint()
{
    FILE* file = fopen("/proc/self/smaps", "r");
    if (!file)
        return 0;

    unsigned long totalPrivateDirtyInKB = 0;
    bool isAnonymous = false;
    forEachLine(file, [&] (char* buffer) {
        {
            unsigned long start;
            unsigned long end;
            unsigned long offset;
            unsigned long inode;
            char dev[32];
            char perms[5];
            char path[7];
            int scannedCount = sscanf(buffer, "%lx-%lx %4s %lx %31s %lu %6s", &start, &end, perms, &offset, dev, &inode, path);
            if (scannedCount == 6) {
                isAnonymous = true;
                return;
            }
            if (scannedCount == 7) {
                StringView pathString(path);
                isAnonymous = pathString == "[heap]" || pathString.startsWith("[stack");
                return;
            }
        }

        if (!isAnonymous)
            return;

        unsigned long privateDirtyInKB;
        if (sscanf(buffer, "Private_Dirty: %lu", &privateDirtyInKB) == 1)
            totalPrivateDirtyInKB += privateDirtyInKB;
    });
    fclose(file);
    return totalPrivateDirtyInKB * KB;
}

size_t memoryFootprint()
{
    static size_t footprint = 0;
    static MonotonicTime previousUpdateTime = { };
    Seconds elapsed = MonotonicTime::now() - previousUpdateTime;
    if (elapsed >= s_memoryFootprintUpdateInterval) {
        footprint = computeMemoryFootprint();
        previousUpdateTime = MonotonicTime::now();
    }

    return footprint;
}

} // namespace WTF
