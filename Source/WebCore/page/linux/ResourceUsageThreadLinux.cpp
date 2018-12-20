/*
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceUsageThread.h"

#if ENABLE(RESOURCE_USAGE) && OS(LINUX)

#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/VM.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/linux/CurrentProcessMemoryStatus.h>

namespace WebCore {

static float cpuPeriod()
{
    FILE* file = fopen("/proc/stat", "r");
    if (!file)
        return 0;

    static const unsigned statMaxLineLength = 512;
    char buffer[statMaxLineLength + 1];
    char* line = fgets(buffer, statMaxLineLength, file);
    if (!line) {
        fclose(file);
        return 0;
    }

    unsigned long long userTime, niceTime, systemTime, idleTime;
    unsigned long long ioWait, irq, softIrq, steal, guest, guestnice;
    ioWait = irq = softIrq = steal = guest = guestnice = 0;
    int retVal = sscanf(buffer, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",
        &userTime, &niceTime, &systemTime, &idleTime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
    // We expect 10 values to be matched by sscanf
    if (retVal != 10) {
        fclose(file);
        return 0;
    }


    // Keep parsing if we still don't know cpuCount.
    static unsigned cpuCount = 0;
    if (!cpuCount) {
        while ((line = fgets(buffer, statMaxLineLength, file))) {
            if (strlen(line) > 4 && line[0] == 'c' && line[1] == 'p' && line[2] == 'u')
                cpuCount++;
            else
                break;
        }
    }
    fclose(file);

    if (!cpuCount)
        return 0;

    static unsigned long long previousTotalTime = 0;
    unsigned long long totalTime = userTime + niceTime + systemTime + irq + softIrq + idleTime + ioWait + steal;
    unsigned long long period = totalTime > previousTotalTime ? totalTime - previousTotalTime : 0;
    previousTotalTime = totalTime;
    return static_cast<float>(period) / cpuCount;
}

static float cpuUsage()
{
    float period = cpuPeriod();
    if (!period)
        return -1;

    int fd = open("/proc/self/stat", O_RDONLY);
    if (fd < 0)
        return -1;

    static const ssize_t maxBufferLength = BUFSIZ - 1;
    char buffer[BUFSIZ];
    buffer[0] = '\0';

    ssize_t totalBytesRead = 0;
    while (totalBytesRead < maxBufferLength) {
        ssize_t bytesRead = read(fd, buffer + totalBytesRead, maxBufferLength - totalBytesRead);
        if (bytesRead < 0) {
            if (errno != EINTR) {
                close(fd);
                return -1;
            }
            continue;
        }

        if (!bytesRead)
            break;

        totalBytesRead += bytesRead;
    }
    close(fd);
    buffer[totalBytesRead] = '\0';

    // Skip pid and process name.
    char* position = strrchr(buffer, ')');
    if (!position)
        return -1;

    // Move after state.
    position += 4;

    // Skip ppid, pgrp, sid, tty_nr, tty_pgrp, flags, min_flt, cmin_flt, maj_flt, cmaj_flt.
    unsigned tokensToSkip = 10;
    while (tokensToSkip--) {
        while (!isASCIISpace(position[0]))
            position++;
        position++;
    }

    static unsigned long long previousUtime = 0;
    static unsigned long long previousStime = 0;
    unsigned long long utime = strtoull(position, &position, 10);
    unsigned long long stime = strtoull(position, &position, 10);
    float usage = (utime + stime - (previousUtime + previousStime)) / period * 100.0;
    previousUtime = utime;
    previousStime = stime;

    return clampTo<float>(usage, 0, 100);
}

void ResourceUsageThread::platformThreadBody(JSC::VM* vm, ResourceUsageData& data)
{
    data.cpu = cpuUsage();

    ProcessMemoryStatus memoryStatus;
    currentProcessMemoryStatus(memoryStatus);
    data.totalDirtySize = memoryStatus.resident - memoryStatus.shared;

    size_t currentGCHeapCapacity = vm->heap.blockBytesAllocated();
    size_t currentGCOwnedExtra = vm->heap.extraMemorySize();
    size_t currentGCOwnedExternal = vm->heap.externalMemorySize();
    RELEASE_ASSERT(currentGCOwnedExternal <= currentGCOwnedExtra);

    data.categories[MemoryCategory::GCHeap].dirtySize = currentGCHeapCapacity;
    data.categories[MemoryCategory::GCOwned].dirtySize = currentGCOwnedExtra - currentGCOwnedExternal;
    data.categories[MemoryCategory::GCOwned].externalSize = currentGCOwnedExternal;

    data.totalExternalSize = currentGCOwnedExternal;

    auto now = MonotonicTime::now();
    data.timeOfNextEdenCollection = now + vm->heap.edenActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
    data.timeOfNextFullCollection = now + vm->heap.fullActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE) && OS(LINUX)
