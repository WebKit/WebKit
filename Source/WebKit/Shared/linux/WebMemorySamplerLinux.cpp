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
#include <wtf/SystemTracing.h>
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

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Linux port
    char buffer[maxBuffer] = {0, };
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    unsigned int index = 0;
    while (index < maxBuffer) {
        int ch = fgetc(file);
        if (ch == EOF || (isUnicodeCompatibleASCIIWhitespace(ch) && index)) // Break on non-initial ASCII space.
            break;
        if (!isUnicodeCompatibleASCIIWhitespace(ch)) {
            buffer[index] = ch;
            index++;
        }
    }

    return String::fromLatin1(buffer);
}

static inline void appendKeyValuePair(WebMemoryStatistics& stats, const String& key, size_t value)
{
    stats.keys.append(key);
    stats.values.append(value);
}

#define INSTRUMENT_KEY_VALUE_COUNTER(stats, id, key, value) \
do { \
    WTFSetCounter(id, value); \
    appendKeyValuePair(stats, key, value); \
} while (0);

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
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
IGNORE_CLANG_WARNINGS_END

WebMemoryStatistics WebMemorySampler::sampleWebKit() const
{
    WebMemoryStatistics webKitMemoryStats;

    WallTime now = WallTime::now();

    appendKeyValuePair(webKitMemoryStats, "Timestamp"_s, now.secondsSinceEpoch().seconds());

    ProcessMemoryStatus processMemoryStatus;
    currentProcessMemoryStatus(processMemoryStatus);

    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, TotalProgramBytes, "Total Program Bytes"_s, processMemoryStatus.size);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, ResidentSetBytes, "Resident Set Bytes"_s, processMemoryStatus.resident);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, ResidentSharedBytes, "Resident Shared Bytes"_s, processMemoryStatus.shared);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, TextBytes, "Text Bytes"_s, processMemoryStatus.text);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, LibraryBytes, "Library Bytes"_s, processMemoryStatus.lib);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, DataStackBytes, "Data + Stack Bytes"_s, processMemoryStatus.data);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, DirtyBytes, "Dirty Bytes"_s, processMemoryStatus.dt);

    size_t totalBytesInUse = 0;
    size_t totalBytesCommitted = 0;

    auto fastMallocStatistics = WTF::fastMallocStatistics();
    size_t fastMallocBytesInUse = fastMallocStatistics.committedVMBytes - fastMallocStatistics.freeListBytes;
    size_t fastMallocBytesCommitted = fastMallocStatistics.committedVMBytes;
    totalBytesInUse += fastMallocBytesInUse;
    totalBytesCommitted += fastMallocBytesCommitted;

    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, FastMallocInUse, "Fast Malloc In Use"_s, fastMallocBytesInUse);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, FastMallocCommittedMemory, "Fast Malloc Committed Memory"_s, fastMallocBytesCommitted);

    size_t jscHeapBytesInUse = commonVM().heap.size();
    size_t jscHeapBytesCommitted = commonVM().heap.capacity();
    totalBytesInUse += jscHeapBytesInUse;
    totalBytesCommitted += jscHeapBytesCommitted;

    GlobalMemoryStatistics globalMemoryStats = globalMemoryStatistics();
    totalBytesInUse += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;
    totalBytesCommitted += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;

    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, JSHeapInUse, "JavaScript Heap In Use"_s, jscHeapBytesInUse);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, JSHeapCommittedMemory, "JavaScript Heap Committed Memory"_s, jscHeapBytesCommitted);

    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, JSStackBytes, "JavaScript Stack Bytes"_s, globalMemoryStats.stackBytes);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, JSJITBytes, "JavaScript JIT Bytes"_s, globalMemoryStats.JITBytes);

    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, TotalMemory, "Total Memory In Use"_s, totalBytesInUse);
    INSTRUMENT_KEY_VALUE_COUNTER(webKitMemoryStats, TotalCommittedMemory, "Total Committed Memory"_s, totalBytesCommitted);

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
