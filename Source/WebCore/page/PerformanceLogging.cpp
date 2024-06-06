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
#include "Document.h"
#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "Page.h"

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static ASCIILiteral toString(PerformanceLogging::PointOfInterest poi)
{
    switch (poi) {
    case PerformanceLogging::MainFrameLoadStarted:
        return "MainFrameLoadStarted"_s;
    case PerformanceLogging::MainFrameLoadCompleted:
        return "MainFrameLoadCompleted"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return ""_s;
}
#endif

Vector<std::pair<ASCIILiteral, size_t>> PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations includeExpensive)
{
    Vector<std::pair<ASCIILiteral, size_t>> stats;
    stats.reserveInitialCapacity(32);

    stats.append(std::pair { "page_count"_s, Page::nonUtilityPageCount() });
    stats.append(std::pair { "backforward_cache_page_count"_s, BackForwardCache::singleton().pageCount() });
    stats.append(std::pair { "document_count"_s, Document::allDocuments().size() });

    Ref vm = commonVM();
    JSC::JSLockHolder locker(vm);
    stats.append(std::pair { "javascript_gc_heap_capacity_mb"_s, vm->heap.capacity() >> 20 });
    stats.append(std::pair { "javascript_gc_heap_extra_memory_size_mb"_s, vm->heap.extraMemorySize() >> 20 });

    if (includeExpensive == ShouldIncludeExpensiveComputations::Yes) {
        stats.append(std::pair { "javascript_gc_heap_size_mb"_s, vm->heap.size() >> 20 });
        stats.append(std::pair { "javascript_gc_object_count"_s, vm->heap.objectCount() });
        stats.append(std::pair { "javascript_gc_protected_object_count"_s, vm->heap.protectedObjectCount() });
        stats.append(std::pair { "javascript_gc_protected_global_object_count"_s, vm->heap.protectedGlobalObjectCount() });
    }

    getPlatformMemoryUsageStatistics(stats);

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
    UNUSED_VARIABLE(m_page);
#else
    // Ignore synthetic main frames used internally by SVG and web inspector.
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page.mainFrame())) {
        if (localMainFrame->loader().client().isEmptyFrameLoaderClient())
            return;
    }

    RELEASE_LOG(PerformanceLogging, "Memory usage info dump at %s:", toString(poi).characters());
    for (auto& [key, value] : memoryUsageStatistics(ShouldIncludeExpensiveComputations::No))
        RELEASE_LOG(PerformanceLogging, "  %s: %zu", key.characters(), value);
#endif
}

#if !PLATFORM(COCOA)
void PerformanceLogging::getPlatformMemoryUsageStatistics(Vector<std::pair<ASCIILiteral, size_t>>&) { }
std::optional<uint64_t> PerformanceLogging::physicalFootprint() { return std::nullopt; }
#endif

}
