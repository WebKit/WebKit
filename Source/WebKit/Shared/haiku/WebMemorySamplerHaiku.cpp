/*
 * Copyright (C) 2014,2019 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebMemorySampler.h"

#if ENABLE(MEMORY_SAMPLER)

#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <WebCore/CommonVM.h>
#include <WebCore/JSDOMWindow.h>
#include "NotImplemented.h"
#include <string.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>  

#include <OS.h>

using namespace WebCore;
using namespace JSC;
using namespace WTF;

namespace WebKit {

struct ApplicationMemoryStats {
    size_t totalProgramSize;
    size_t residentSetSize;
    size_t sharedSize;
    size_t textSize;
    size_t librarySize;
    size_t dataStackSize;
    size_t dirtyPageSize;
};

static inline void appendKeyValuePair(WebMemoryStatistics& stats, const String& key, size_t value)
{
    stats.keys.append(key);
    stats.values.append(value);
}

static ApplicationMemoryStats sampleMemoryAllocatedForApplication()
{
    ApplicationMemoryStats applicationStats = {0, 0, 0, 0, 0, 0, 0};

    ssize_t cookie;
    area_info area;

    while (get_next_area_info(B_CURRENT_TEAM, &cookie, &area) == B_OK)
    {
        applicationStats.totalProgramSize += area.size;
        // TODO fill the other infos...
    }

    return applicationStats;
}

String WebMemorySampler::processName() const
{
    team_info info;
    if (get_team_info(B_CURRENT_TEAM, &info) == B_OK)
        return String(info.args);
    return String();
}

WebMemoryStatistics WebMemorySampler::sampleWebKit() const
{
    WebMemoryStatistics webKitMemoryStats;

    WallTime now = WallTime::now();

    appendKeyValuePair(webKitMemoryStats, "Timestamp"_s, now.secondsSinceEpoch().seconds());

    ApplicationMemoryStats applicationStats = sampleMemoryAllocatedForApplication();

    appendKeyValuePair(webKitMemoryStats, "Total Program Size"_s, applicationStats.totalProgramSize);
    appendKeyValuePair(webKitMemoryStats, "RSS"_s, applicationStats.residentSetSize);
    appendKeyValuePair(webKitMemoryStats, "Shared"_s, applicationStats.sharedSize);
    appendKeyValuePair(webKitMemoryStats, "Text"_s, applicationStats.textSize);
    appendKeyValuePair(webKitMemoryStats, "Library"_s, applicationStats.librarySize);
    appendKeyValuePair(webKitMemoryStats, "Data/Stack"_s, applicationStats.dataStackSize);
    appendKeyValuePair(webKitMemoryStats, "Dirty"_s, applicationStats.dirtyPageSize);

    size_t totalBytesInUse = 0;
    size_t totalBytesCommitted = 0;

#if ENABLE(GLOBAL_FASTMALLOC_NEW)
    FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();
    size_t fastMallocBytesInUse = fastMallocStatistics.committedVMBytes - fastMallocStatistics.freeListBytes;
    size_t fastMallocBytesCommitted = fastMallocStatistics.committedVMBytes;
    totalBytesInUse += fastMallocBytesInUse;
    totalBytesCommitted += fastMallocBytesCommitted;

    appendKeyValuePair(webKitMemoryStats, "Fast Malloc In Use"_s, fastMallocBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "Fast Malloc Committed Memory"_s, fastMallocBytesCommitted);
#endif

    size_t jscHeapBytesInUse = commonVM().heap.size();
    size_t jscHeapBytesCommitted = commonVM().heap.capacity();
    totalBytesInUse += jscHeapBytesInUse;
    totalBytesCommitted += jscHeapBytesCommitted;

    GlobalMemoryStatistics globalMemoryStats = globalMemoryStatistics();
    totalBytesInUse += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;
    totalBytesCommitted += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;

    appendKeyValuePair(webKitMemoryStats, "JavaScript Heap In Use"_s, jscHeapBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "JavaScript Heap Commited Memory"_s, jscHeapBytesCommitted);
    
    appendKeyValuePair(webKitMemoryStats, "JavaScript Stack Bytes"_s, globalMemoryStats.stackBytes);
    appendKeyValuePair(webKitMemoryStats, "JavaScript JIT Bytes"_s, globalMemoryStats.JITBytes);

    appendKeyValuePair(webKitMemoryStats, "Total Memory In Use"_s, totalBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "Total Committed Memory"_s, totalBytesCommitted);

    system_info systemInfo;
    if (!get_system_info(&systemInfo)) {
        appendKeyValuePair(webKitMemoryStats, "System Total Bytes"_s, systemInfo.max_pages * B_PAGE_SIZE);
        appendKeyValuePair(webKitMemoryStats, "Available Bytes"_s, systemInfo.free_memory);
        //appendKeyValuePair(webKitMemoryStats, "Shared Bytes"_s, systemInfo.sharedram);
        appendKeyValuePair(webKitMemoryStats, "Buffer Bytes"_s, systemInfo.block_cache_pages * B_PAGE_SIZE);
        appendKeyValuePair(webKitMemoryStats, "Total Swap Bytes"_s, systemInfo.max_swap_pages * B_PAGE_SIZE);
        appendKeyValuePair(webKitMemoryStats, "Available Swap Bytes"_s, systemInfo.free_swap_pages * B_PAGE_SIZE);
    }   

    return webKitMemoryStats;
}

void WebMemorySampler::sendMemoryPressureEvent()
{
    notImplemented();
}

}
#endif
