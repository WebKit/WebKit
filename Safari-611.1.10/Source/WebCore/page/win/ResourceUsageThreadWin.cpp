/*
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#if ENABLE(RESOURCE_USAGE)

#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/VM.h>
#include <psapi.h>

namespace WebCore {

void ResourceUsageThread::platformSaveStateBeforeStarting()
{
}

static uint64_t fileTimeToUint64(FILETIME ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;

    return u.QuadPart;
}

static bool getCurrentCpuTime(uint64_t& nowTime, uint64_t& userTime, uint64_t& kernelTime)
{
    FILETIME creationFileTime, exitFileTime, kernelFileTime, userFileTime;
    if (!GetProcessTimes(GetCurrentProcess(), &creationFileTime, &exitFileTime, &kernelFileTime, &userFileTime))
        return false;

    FILETIME nowFileTime;
    GetSystemTimeAsFileTime(&nowFileTime);

    nowTime = fileTimeToUint64(nowFileTime);
    userTime = fileTimeToUint64(userFileTime);
    kernelTime  = fileTimeToUint64(kernelFileTime);

    return true;
}

static float cpuUsage()
{
    static int numberOfProcessors = 0;
    static uint64_t lastTime = 0;
    static uint64_t lastKernelTime = 0;
    static uint64_t lastUserTime = 0;

    if (!lastTime) {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        numberOfProcessors = systemInfo.dwNumberOfProcessors;

        getCurrentCpuTime(lastTime, lastKernelTime, lastUserTime);
        return 0;
    }

    uint64_t nowTime, kernelTime, userTime;
    if (!getCurrentCpuTime(nowTime, kernelTime, userTime))
        return 0;

    uint64_t elapsed = nowTime - lastTime;
    uint64_t totalCPUTime = (kernelTime - lastKernelTime);
    totalCPUTime += (userTime - lastUserTime);

    lastTime = nowTime;
    lastKernelTime = kernelTime;
    lastUserTime = userTime;

    float usage =  (100.0 * totalCPUTime) / elapsed / numberOfProcessors;
    return clampTo<float>(usage, 0, 100);
}

static size_t memoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        return pmc.PrivateUsage;

    return 0;
}

void ResourceUsageThread::platformCollectCPUData(JSC::VM*, ResourceUsageData& data)
{
    data.cpu = cpuUsage();

    // FIXME: Exclude the ResourceUsage thread.
    // FIXME: Exclude the SamplingProfiler thread.
    // FIXME: Classify usage per thread.
    data.cpuExcludingDebuggerThreads = data.cpu;
}

void ResourceUsageThread::platformCollectMemoryData(JSC::VM* vm, ResourceUsageData& data)
{
    data.totalDirtySize = memoryUsage();

    size_t currentGCHeapCapacity = vm->heap.blockBytesAllocated();
    size_t currentGCOwnedExtra = vm->heap.extraMemorySize();
    size_t currentGCOwnedExternal = vm->heap.externalMemorySize();
    RELEASE_ASSERT(currentGCOwnedExternal <= currentGCOwnedExtra);

    data.categories[MemoryCategory::GCHeap].dirtySize = currentGCHeapCapacity;
    data.categories[MemoryCategory::GCOwned].dirtySize = currentGCOwnedExtra - currentGCOwnedExternal;
    data.categories[MemoryCategory::GCOwned].externalSize = currentGCOwnedExternal;

    data.totalExternalSize = currentGCOwnedExternal;

    data.timeOfNextEdenCollection = data.timestamp + vm->heap.edenActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
    data.timeOfNextFullCollection = data.timestamp + vm->heap.fullActivityCallback()->timeUntilFire().valueOr(Seconds(std::numeric_limits<double>::infinity()));
}

}

#endif
