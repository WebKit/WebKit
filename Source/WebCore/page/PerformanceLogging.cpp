/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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
#include "PerformanceLogging.h"

#include "BackForwardCache.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "JSDOMWindow.h"
#include "Logging.h"
#include "Page.h"

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static const char* toString(PerformanceLogging::PointOfInterest poi)
{
    switch (poi) {
    case PerformanceLogging::MainFrameLoadStarted:
        return "MainFrameLoadStarted";
    case PerformanceLogging::MainFrameLoadCompleted:
        return "MainFrameLoadCompleted";
    }
}
#endif

HashMap<const char*, size_t> PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations includeExpensive)
{
    HashMap<const char*, size_t> stats;

    auto& vm = commonVM();
    stats.add("javascript_gc_heap_capacity", vm.heap.capacity());
    stats.add("javascript_gc_heap_extra_memory_size", vm.heap.extraMemorySize());

    auto& backForwardCache = BackForwardCache::singleton();
    stats.add("backforward_cache_page_count", backForwardCache.pageCount());

    stats.add("document_count", Document::allDocuments().size());

    if (includeExpensive == ShouldIncludeExpensiveComputations::Yes) {
        stats.add("javascript_gc_heap_size", vm.heap.size());
        stats.add("javascript_gc_object_count", vm.heap.objectCount());
        stats.add("javascript_gc_protected_object_count", vm.heap.protectedObjectCount());
        stats.add("javascript_gc_global_object_count", vm.heap.globalObjectCount());
        stats.add("javascript_gc_protected_global_object_count", vm.heap.protectedGlobalObjectCount());
    }

    return stats;
}

HashCountedSet<const char*> PerformanceLogging::javaScriptObjectCounts()
{
    return WTFMove(*commonVM().heap.objectTypeCounts());
}

PerformanceLogging::PerformanceLogging(Page& page)
    : m_page(page)
{
}

void PerformanceLogging::didReachPointOfInterest(PointOfInterest poi)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(poi);
#else
    // Ignore synthetic main frames used internally by SVG and web inspector.
    if (m_page.mainFrame().loader().client().isEmptyFrameLoaderClient())
        return;

    auto stats = memoryUsageStatistics(ShouldIncludeExpensiveComputations::No);
    getPlatformMemoryUsageStatistics(stats);

    RELEASE_LOG(PerformanceLogging, "Memory usage info dump at %s:", toString(poi));
    for (auto& it : stats)
        RELEASE_LOG(PerformanceLogging, "  %s: %zu", it.key, it.value);
#endif
}

#if !PLATFORM(COCOA)
void PerformanceLogging::getPlatformMemoryUsageStatistics(HashMap<const char*, size_t>&) { }
Optional<uint64_t> PerformanceLogging::physicalFootprint() { return WTF::nullopt; }
#endif

}
