/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics
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
#include <WebCore/NotImplemented.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <wtf/WallTime.h>
#include <wtf/linux/CurrentProcessMemoryStatus.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

using namespace JSC;
using namespace WebCore;

static const unsigned int maxBuffer = 128;
static const unsigned int maxProcessPath = 35;

static inline String nextToken(FILE* file)
{
    ASSERT(file);
    if (!file)
        return String();

    char buffer[maxBuffer] = {0, };
    unsigned int index = 0;
    while (index < maxBuffer) {
        int ch = fgetc(file);
        if (ch == EOF || (isASCIISpace(ch) && index)) // Break on non-initial ASCII space.
            break;
        if (!isASCIISpace(ch)) {
            buffer[index] = ch;
            index++;
        }
    }

    return String(buffer);
}

static inline void appendKeyValuePair(WebMemoryStatistics& stats, const String& key, size_t value)
{
    stats.keys.append(key);
    stats.values.append(value);
}

String WebMemorySampler::processName() const
{
    char processPath[maxProcessPath];
    snprintf(processPath, maxProcessPath, "/proc/self/status");
    FILE* statusFileDescriptor = fopen(processPath, "r");
    if (!statusFileDescriptor)
        return String();
        
    nextToken(statusFileDescriptor);
    String processName = nextToken(statusFileDescriptor);

    fclose(statusFileDescriptor);

    return processName;
}

WebMemoryStatistics WebMemorySampler::sampleWebKit() const
{
    WebMemoryStatistics webKitMemoryStats;

    WallTime now = WallTime::now();

    appendKeyValuePair(webKitMemoryStats, "Timestamp"_s, now.secondsSinceEpoch().seconds());

    ProcessMemoryStatus processMemoryStatus;
    currentProcessMemoryStatus(processMemoryStatus);

    appendKeyValuePair(webKitMemoryStats, "Total Program Bytes"_s, processMemoryStatus.size);
    appendKeyValuePair(webKitMemoryStats, "Resident Set Bytes"_s, processMemoryStatus.resident);
    appendKeyValuePair(webKitMemoryStats, "Resident Shared Bytes"_s, processMemoryStatus.shared);
    appendKeyValuePair(webKitMemoryStats, "Text Bytes"_s, processMemoryStatus.text);
    appendKeyValuePair(webKitMemoryStats, "Library Bytes"_s, processMemoryStatus.lib);
    appendKeyValuePair(webKitMemoryStats, "Data + Stack Bytes"_s, processMemoryStatus.data);
    appendKeyValuePair(webKitMemoryStats, "Dirty Bytes"_s, processMemoryStatus.dt);

    size_t totalBytesInUse = 0;
    size_t totalBytesCommitted = 0;

    auto fastMallocStatistics = WTF::fastMallocStatistics();
    size_t fastMallocBytesInUse = fastMallocStatistics.committedVMBytes - fastMallocStatistics.freeListBytes;
    size_t fastMallocBytesCommitted = fastMallocStatistics.committedVMBytes;
    totalBytesInUse += fastMallocBytesInUse;
    totalBytesCommitted += fastMallocBytesCommitted;

    appendKeyValuePair(webKitMemoryStats, "Fast Malloc In Use"_s, fastMallocBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "Fast Malloc Committed Memory"_s, fastMallocBytesCommitted);

    size_t jscHeapBytesInUse = commonVM().heap.size();
    size_t jscHeapBytesCommitted = commonVM().heap.capacity();
    totalBytesInUse += jscHeapBytesInUse;
    totalBytesCommitted += jscHeapBytesCommitted;

    GlobalMemoryStatistics globalMemoryStats = globalMemoryStatistics();
    totalBytesInUse += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;
    totalBytesCommitted += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;

    appendKeyValuePair(webKitMemoryStats, "JavaScript Heap In Use"_s, jscHeapBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "JavaScript Heap Committed Memory"_s, jscHeapBytesCommitted);
    
    appendKeyValuePair(webKitMemoryStats, "JavaScript Stack Bytes"_s, globalMemoryStats.stackBytes);
    appendKeyValuePair(webKitMemoryStats, "JavaScript JIT Bytes"_s, globalMemoryStats.JITBytes);

    appendKeyValuePair(webKitMemoryStats, "Total Memory In Use"_s, totalBytesInUse);
    appendKeyValuePair(webKitMemoryStats, "Total Committed Memory"_s, totalBytesCommitted);

    struct sysinfo systemInfo;
    if (!sysinfo(&systemInfo)) {
        appendKeyValuePair(webKitMemoryStats, "System Total Bytes"_s, systemInfo.totalram);
        appendKeyValuePair(webKitMemoryStats, "Available Bytes"_s, systemInfo.freeram);
        appendKeyValuePair(webKitMemoryStats, "Shared Bytes"_s, systemInfo.sharedram);
        appendKeyValuePair(webKitMemoryStats, "Buffer Bytes"_s, systemInfo.bufferram);
        appendKeyValuePair(webKitMemoryStats, "Total Swap Bytes"_s, systemInfo.totalswap);
        appendKeyValuePair(webKitMemoryStats, "Available Swap Bytes"_s, systemInfo.freeswap);
    }   

    return webKitMemoryStats;
}

void WebMemorySampler::sendMemoryPressureEvent()
{
    notImplemented();
}

}
#endif
