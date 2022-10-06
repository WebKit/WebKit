/*
 * Copyright (C) 2011, 2014 Apple Inc. All Rights Reserved.
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
#include "MemoryRelease.h"

#include "BackForwardCache.h"
#include "CSSFontSelector.h"
#include "CSSValuePool.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "CookieJar.h"
#include "Document.h"
#include "FontCache.h"
#include "Frame.h"
#include "GCController.h"
#include "HTMLMediaElement.h"
#include "HTMLNameCache.h"
#include "InlineStyleSheetOwner.h"
#include "InspectorInstrumentation.h"
#include "LayoutIntegrationLineLayout.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PerformanceLogging.h"
#include "RenderTheme.h"
#include "ScrollingThread.h"
#include "StyleScope.h"
#include "StyledElement.h"
#include "TextPainter.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/VM.h>
#include <wtf/FastMalloc.h>
#include <wtf/ResourceUsage.h>
#include <wtf/SystemTracing.h>

#if PLATFORM(COCOA)
#include "ResourceUsageThread.h"
#include <wtf/spi/darwin/OSVariantSPI.h>
#endif

namespace WebCore {

static void releaseNoncriticalMemory(MaintainMemoryCache maintainMemoryCache)
{
    RenderTheme::singleton().purgeCaches();

    FontCache::releaseNoncriticalMemoryInAllFontCaches();

    GlyphDisplayListCache::singleton().clear();

    for (auto* document : Document::allDocuments()) {
        document->clearSelectorQueryCache();

        if (auto* renderView = document->renderView())
            LayoutIntegration::LineLayout::releaseCaches(*renderView);
    }

    if (maintainMemoryCache == MaintainMemoryCache::No)
        MemoryCache::singleton().pruneDeadResourcesToSize(0);

    InlineStyleSheetOwner::clearCache();
    HTMLNameCache::clear();
}

static void releaseCriticalMemory(Synchronous synchronous, MaintainBackForwardCache maintainBackForwardCache, MaintainMemoryCache maintainMemoryCache)
{
    // Right now, the only reason we call release critical memory while not under memory pressure is if the process is about to be suspended.
    if (maintainBackForwardCache == MaintainBackForwardCache::No) {
        PruningReason pruningReason = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? PruningReason::MemoryPressure : PruningReason::ProcessSuspended;
        BackForwardCache::singleton().pruneToSizeNow(0, pruningReason);
    }

    if (maintainMemoryCache == MaintainMemoryCache::No) {
        auto shouldDestroyDecodedDataForAllLiveResources = true;
        MemoryCache::singleton().pruneLiveResourcesToSize(0, shouldDestroyDecodedDataForAllLiveResources);
    }

    CSSValuePool::singleton().drain();

    Page::forEachPage([](auto& page) {
        page.cookieJar().clearCache();
    });

    for (auto& document : copyToVectorOf<RefPtr<Document>>(Document::allDocuments())) {
        document->styleScope().releaseMemory();
        document->fontSelector().emptyCaches();
        document->cachedResourceLoader().garbageCollectDocumentResources();
    }

    GCController::singleton().deleteAllCode(JSC::DeleteAllCodeIfNotCollecting);

#if ENABLE(VIDEO)
    for (auto* mediaElement : HTMLMediaElement::allMediaElements()) {
        if (mediaElement->paused())
            mediaElement->purgeBufferedDataIfPossible();
    }
#endif

    if (synchronous == Synchronous::Yes) {
        GCController::singleton().garbageCollectNow();
    } else {
#if PLATFORM(IOS_FAMILY)
        GCController::singleton().garbageCollectNowIfNotDoneRecently();
#else
        GCController::singleton().garbageCollectSoon();
#endif
    }

    WorkerGlobalScope::releaseMemoryInWorkers(synchronous);
}

void releaseMemory(Critical critical, Synchronous synchronous, MaintainBackForwardCache maintainBackForwardCache, MaintainMemoryCache maintainMemoryCache)
{
    TraceScope scope(MemoryPressureHandlerStart, MemoryPressureHandlerEnd, static_cast<uint64_t>(critical), static_cast<uint64_t>(synchronous));

    if (critical == Critical::Yes) {
        // Return unused pages back to the OS now as this will likely give us a little memory to work with.
        WTF::releaseFastMallocFreeMemory();
        releaseCriticalMemory(synchronous, maintainBackForwardCache, maintainMemoryCache);
    }

    releaseNoncriticalMemory(maintainMemoryCache);

    platformReleaseMemory(critical);

    if (synchronous == Synchronous::Yes) {
        // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
        WorkerOrWorkletThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(SCROLLING_THREAD)
        ScrollingThread::dispatch(WTF::releaseFastMallocFreeMemory);
#endif
        WTF::releaseFastMallocFreeMemory();
    }

#if ENABLE(RESOURCE_USAGE)
    Page::forEachPage([&](Page& page) {
        InspectorInstrumentation::didHandleMemoryPressure(page, critical);
    });
#endif
}

void releaseGraphicsMemory(Critical critical, Synchronous synchronous)
{
    TraceScope scope(MemoryPressureHandlerStart, MemoryPressureHandlerEnd, static_cast<uint64_t>(critical), static_cast<uint64_t>(synchronous));

    platformReleaseGraphicsMemory(critical);

    WTF::releaseFastMallocFreeMemory();
}

#if RELEASE_LOG_DISABLED
void logMemoryStatistics(LogMemoryStatisticsReason) { }
#else
static const char* logMemoryStatisticsReasonDescription(LogMemoryStatisticsReason reason)
{
    switch (reason) {
    case LogMemoryStatisticsReason::DebugNotification:
        return "debug notification";
    case LogMemoryStatisticsReason::WarningMemoryPressureNotification:
        return "warning memory pressure notification";
    case LogMemoryStatisticsReason::CriticalMemoryPressureNotification:
        return "critical memory pressure notification";
    case LogMemoryStatisticsReason::OutOfMemoryDeath:
        return "out of memory death";
    };
    RELEASE_ASSERT_NOT_REACHED();
}

void logMemoryStatistics(LogMemoryStatisticsReason reason)
{
    const char* description = logMemoryStatisticsReasonDescription(reason);

    RELEASE_LOG(MemoryPressure, "WebKit memory usage statistics at time of %" PUBLIC_LOG_STRING ":", description);
    RELEASE_LOG(MemoryPressure, "Websam state: %" PUBLIC_LOG_STRING, MemoryPressureHandler::processStateDescription().characters());
    auto stats = PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations::Yes);
    for (auto& [key, val] : stats)
        RELEASE_LOG(MemoryPressure, "%" PUBLIC_LOG_STRING ": %zu", key, val);

#if PLATFORM(COCOA)
    auto pageSize = vmPageSize();
    auto pages = pagesPerVMTag();

    RELEASE_LOG(MemoryPressure, "Dirty memory per VM tag at time of %" PUBLIC_LOG_STRING ":", description);
    for (unsigned i = 0; i < 256; ++i) {
        size_t dirty = pages[i].dirty * pageSize;
        if (!dirty)
            continue;
        String tagName = displayNameForVMTag(i);
        if (!tagName)
            tagName = makeString("Tag ", i);
        RELEASE_LOG(MemoryPressure, "  %" PUBLIC_LOG_STRING ": %lu MB in %zu regions", tagName.latin1().data(), dirty / MB, pages[i].regionCount);
    }

    bool shouldLogJavaScriptObjectCounts = os_variant_allows_internal_security_policies("com.apple.WebKit");
    if (!shouldLogJavaScriptObjectCounts)
        return;
#endif

    auto& vm = commonVM();
    JSC::JSLockHolder locker(vm);
    RELEASE_LOG(MemoryPressure, "Live JavaScript objects at time of %" PUBLIC_LOG_STRING ":", description);
    auto typeCounts = vm.heap.objectTypeCounts();
    for (auto& it : *typeCounts)
        RELEASE_LOG(MemoryPressure, "  %" PUBLIC_LOG_STRING ": %d", it.key, it.value);
}
#endif

#if !PLATFORM(COCOA)
void platformReleaseMemory(Critical) { }
void platformReleaseGraphicsMemory(Critical) { }
void jettisonExpensiveObjectsOnTopLevelNavigation() { }
void registerMemoryReleaseNotifyCallbacks() { }
#endif

} // namespace WebCore
