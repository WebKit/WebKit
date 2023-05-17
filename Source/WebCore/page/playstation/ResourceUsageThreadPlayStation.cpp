/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include <showmap.h>

namespace WebCore {

static float cpuUsage()
{
    // FIXME: Need a way to calculate cpu usage
    return 0;
}

void ResourceUsageThread::platformSaveStateBeforeStarting()
{
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
    auto& categories = data.categories;

    auto currentGCHeapCapacity = vm->heap.blockBytesAllocated();
    auto currentGCOwnedExtra = vm->heap.extraMemorySize();
    auto currentGCOwnedExternal = vm->heap.externalMemorySize();
    RELEASE_ASSERT(currentGCOwnedExternal <= currentGCOwnedExtra);

    categories[MemoryCategory::GCHeap].dirtySize = currentGCHeapCapacity;
    categories[MemoryCategory::GCOwned].dirtySize = currentGCOwnedExtra - currentGCOwnedExternal;
    categories[MemoryCategory::GCOwned].externalSize = currentGCOwnedExternal;

    auto currentGCDirtySize = currentGCHeapCapacity + currentGCOwnedExtra - currentGCOwnedExternal;

    // TODO: collect dirty size of "MemoryCategory::Images" and "MemoryCategory::Layers"

    showmap::Result<256> result;
    result.collect();
    data.totalDirtySize = result.effectiveRss();

    if (auto* entry = result.entry("SceNKFastMalloc")) {
        auto rss = entry->effectiveRss();
        RELEASE_ASSERT(data.totalDirtySize > rss);
        categories[MemoryCategory::Other].dirtySize = data.totalDirtySize - rss;
        categories[MemoryCategory::bmalloc].dirtySize = rss - std::min(rss, currentGCDirtySize);
    } else
        categories[MemoryCategory::Other] = data.totalDirtySize - std::min(data.totalDirtySize, currentGCDirtySize);

    data.totalExternalSize = currentGCOwnedExternal;

    data.timeOfNextEdenCollection = data.timestamp + vm->heap.edenActivityCallback()->timeUntilFire().value_or(Seconds(std::numeric_limits<double>::infinity()));
    data.timeOfNextFullCollection = data.timestamp + vm->heap.fullActivityCallback()->timeUntilFire().value_or(Seconds(std::numeric_limits<double>::infinity()));
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
