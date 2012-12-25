/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *           (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "MemoryUsageSupport.h"

#include "JSDOMWindow.h"
#include <runtime/MemoryStatistics.h>
#include <wtf/FastMalloc.h>

#if OS(LINUX)
#include <malloc.h>
#endif

namespace WebCore {

#if OS(LINUX)
static size_t mallocMemoryUsage(bool inuse)
{
    // Return how much memory (in bytes) has been allocated on the system heap.
    struct mallinfo minfo = ::mallinfo();

    // If we want the internal memory usage, we subtract the memory used by
    // free blocks. That is memory allocated from the system by malloc but
    // which malloc considers free.
    return minfo.arena - (inuse ? minfo.fordblks : 0);
}
#else
static size_t mallocMemoryUsage(bool)
{
    // FIXME: Implement for other supported OS's.
    return 0;
}
#endif

// This is how much system-memory we use.
static int memoryUsageKB()
{
    size_t mallocUsage = mallocMemoryUsage(false);
    WTF::FastMallocStatistics fmStats = WTF::fastMallocStatistics();

    // Extract memory statistics from JavaScriptCore:
    JSC::GlobalMemoryStatistics jscStats = JSC::globalMemoryStatistics();
    size_t jscHeapUsage = JSDOMWindow::commonJSGlobalData()->heap.capacity();
    return (mallocUsage + fmStats.committedVMBytes + jscStats.stackBytes + jscStats.JITBytes + jscHeapUsage) >> 10;
}

// This is how much memory we use internally, not including memory only reserved from the system.
static int actualMemoryUsageKB()
{
    size_t mallocUsage = mallocMemoryUsage(true);
    WTF::FastMallocStatistics fmStats = WTF::fastMallocStatistics();

    // Extract memory statistics from JavaScriptCore:
    JSC::GlobalMemoryStatistics jscStats = JSC::globalMemoryStatistics();
    size_t jscHeapUsage = JSDOMWindow::commonJSGlobalData()->heap.size();
    return (mallocUsage + fmStats.committedVMBytes - fmStats.freeListBytes + jscStats.stackBytes + jscStats.JITBytes + jscHeapUsage) >> 10;
}

int MemoryUsageSupport::memoryUsageMB()
{
    return memoryUsageKB() >> 10;
}

int MemoryUsageSupport::actualMemoryUsageMB()
{
    return actualMemoryUsageKB() >> 10;
}

// FIXME: These values should be determined based on hardware or set by the application.

static const unsigned normalMemoryWatermark = 128; // Chromium default: 256
static const unsigned highMemoryWatermark = 256; // Chromium default: 1024
static const unsigned highMemoryDelta = 64; // Chromium default: 128

int MemoryUsageSupport::lowMemoryUsageMB()
{
    return normalMemoryWatermark;
}

int MemoryUsageSupport::highMemoryUsageMB()
{
    return highMemoryWatermark;
}

int MemoryUsageSupport::highUsageDeltaMB()
{
    return highMemoryDelta;
}

bool MemoryUsageSupport::processMemorySizesInBytes(size_t*, size_t*)
{
    // FIXME: Not implemented.
    return false;
}

void MemoryUsageSupport::requestProcessMemorySizes(PassOwnPtr<WebCore::MemoryUsageSupport::ProcessMemorySizesCallback> requestCallback)
{
    // FIXME: Not implemented.
}

void MemoryUsageSupport::memoryUsageByComponents(Vector<ComponentInfo>&)
{
}

} // namespace WebCore
