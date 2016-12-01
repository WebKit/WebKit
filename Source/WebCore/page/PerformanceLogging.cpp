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

#include "DOMWindow.h"
#include "Document.h"
#include "FrameLoaderClient.h"
#include "JSDOMWindow.h"
#include "Logging.h"
#include "MainFrame.h"
#include "PageCache.h"

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

static void getMemoryUsageStatistics(HashMap<const char*, size_t>& stats)
{
    auto& vm = JSDOMWindow::commonVM();
    stats.set("javascript_gc_heap_capacity", vm.heap.capacity());

    auto& pageCache = PageCache::singleton();
    stats.set("pagecache_page_count", pageCache.pageCount());

    stats.set("document_count", Document::allDocuments().size());
}
#endif

PerformanceLogging::PerformanceLogging(MainFrame& mainFrame)
    : m_mainFrame(mainFrame)
{
}

void PerformanceLogging::didReachPointOfInterest(PointOfInterest poi)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(poi);
#else
    // Ignore synthetic main frames used internally by SVG and web inspector.
    if (m_mainFrame.loader().client().isEmptyFrameLoaderClient())
        return;

    HashMap<const char*, size_t> stats;
    getMemoryUsageStatistics(stats);
    getPlatformMemoryUsageStatistics(stats);

    RELEASE_LOG(PerformanceLogging, "Memory usage info dump at %s:", toString(poi));
    for (auto& it : stats)
        RELEASE_LOG(PerformanceLogging, "  %s: %zu", it.key, it.value);
#endif
}

#if !PLATFORM(COCOA)
void PerformanceLogging::getPlatformMemoryUsageStatistics(HashMap<const char*, size_t>&) { }
#endif

}
