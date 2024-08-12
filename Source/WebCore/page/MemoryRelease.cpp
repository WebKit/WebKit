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
#include "DocumentInlines.h"
#include "FontCache.h"
#include "GCController.h"
#include "HRTFElevation.h"
#include "HTMLMediaElement.h"
#include "HTMLNameCache.h"
#include "ImmutableStyleProperties.h"
#include "InlineStyleSheetOwner.h"
#include "InspectorInstrumentation.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PerformanceLogging.h"
#include "PluginDocument.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGPathElement.h"
#include "ScrollingThread.h"
#include "SelectorQuery.h"
#include "StyleScope.h"
#include "StyleSheetContentsCache.h"
#include "StyledElement.h"
#include "TextBreakingPositionCache.h"
#include "TextPainter.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/VM.h>
#include <wtf/FastMalloc.h>
#include <wtf/ResourceUsage.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/MakeString.h>

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
    SelectorQueryCache::singleton().clear();

    for (auto& document : Document::allDocuments()) {
        if (CheckedPtr renderView = document->renderView()) {
            LayoutIntegration::LineLayout::releaseCaches(*renderView);
            Layout::TextBreakingPositionCache::singleton().clear();
        }
    }

    if (maintainMemoryCache == MaintainMemoryCache::No)
        MemoryCache::singleton().pruneDeadResourcesToSize(0);

    Style::StyleSheetContentsCache::singleton().clear();
    HTMLNameCache::clear();
    ImmutableStyleProperties::clearDeduplicationMap();
    SVGPathElement::clearCache();
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
#if ENABLE(WEB_AUDIO)
    HRTFElevation::clearCache();
#endif

    Page::forEachPage([](auto& page) {
        page.cookieJar().clearCache();
    });

    auto allDocuments = Document::allDocuments();
    auto protectedDocuments = WTF::map(allDocuments, [](auto& document) -> Ref<Document> {
        return document.get();
    });
    for (auto& document : protectedDocuments) {
        document->clearQuerySelectorAllResults();
        document->styleScope().releaseMemory();
        if (RefPtr fontSelector = document->fontSelectorIfExists())
            fontSelector->emptyCaches();
        document->protectedCachedResourceLoader()->garbageCollectDocumentResources();

        if (RefPtr pluginDocument = dynamicDowncast<PluginDocument>(document))
            pluginDocument->releaseMemory();
    }

    if (synchronous == Synchronous::Yes)
        GCController::singleton().deleteAllCode(JSC::PreventCollectionAndDeleteAllCode);
    else
        GCController::singleton().deleteAllCode(JSC::DeleteAllCodeIfNotCollecting);

#if ENABLE(VIDEO)
    for (auto& mediaElement : HTMLMediaElement::allMediaElements())
        Ref { mediaElement.get() }->purgeBufferedDataIfPossible();
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

#if PLATFORM(IOS_FAMILY)
    if (critical == Critical::No)
        GCController::singleton().garbageCollectNowIfNotDoneRecently();
#endif

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
static ASCIILiteral logMemoryStatisticsReasonDescription(LogMemoryStatisticsReason reason)
{
    switch (reason) {
    case LogMemoryStatisticsReason::DebugNotification:
        return "debug notification"_s;
    case LogMemoryStatisticsReason::WarningMemoryPressureNotification:
        return "warning memory pressure notification"_s;
    case LogMemoryStatisticsReason::CriticalMemoryPressureNotification:
        return "critical memory pressure notification"_s;
    case LogMemoryStatisticsReason::OutOfMemoryDeath:
        return "out of memory death"_s;
    };
    RELEASE_ASSERT_NOT_REACHED();
}

void logMemoryStatistics(LogMemoryStatisticsReason reason)
{
    const auto description = logMemoryStatisticsReasonDescription(reason);

    RELEASE_LOG(MemoryPressure, "WebKit memory usage statistics at time of %" PUBLIC_LOG_STRING ":", description.characters());
    RELEASE_LOG(MemoryPressure, "Websam state: %" PUBLIC_LOG_STRING, MemoryPressureHandler::processStateDescription().characters());
    auto stats = PerformanceLogging::memoryUsageStatistics(ShouldIncludeExpensiveComputations::Yes);
    for (auto& [key, val] : stats)
        RELEASE_LOG(MemoryPressure, "%" PUBLIC_LOG_STRING ": %zu", key.characters(), val);

#if PLATFORM(COCOA)
    auto pageSize = vmPageSize();
    auto pages = pagesPerVMTag();

    RELEASE_LOG(MemoryPressure, "Dirty memory per VM tag at time of %" PUBLIC_LOG_STRING ":", description.characters());
    for (unsigned i = 0; i < 256; ++i) {
        size_t dirty = pages[i].dirty * pageSize;
        if (!dirty)
            continue;
        String tagName = displayNameForVMTag(i);
        if (!tagName)
            tagName = makeString("Tag "_s, i);
        RELEASE_LOG(MemoryPressure, "  %" PUBLIC_LOG_STRING ": %lu MB in %zu regions", tagName.latin1().data(), dirty / MB, pages[i].regionCount);
    }

    bool shouldLogJavaScriptObjectCounts = os_variant_allows_internal_security_policies("com.apple.WebKit");
    if (!shouldLogJavaScriptObjectCounts)
        return;
#endif

    auto& vm = commonVM();
    JSC::JSLockHolder locker(vm);
    RELEASE_LOG(MemoryPressure, "Live JavaScript objects at time of %" PUBLIC_LOG_STRING ":", description.characters());
    auto typeCounts = vm.heap.objectTypeCounts();
    for (auto& it : *typeCounts)
        RELEASE_LOG(MemoryPressure, "  %" PUBLIC_LOG_STRING ": %d", it.key, it.value);
}
#endif

#if !PLATFORM(COCOA)
#if !USE(SKIA)
void platformReleaseMemory(Critical) { }
#endif
void platformReleaseGraphicsMemory(Critical) { }
void jettisonExpensiveObjectsOnTopLevelNavigation() { }
void registerMemoryReleaseNotifyCallbacks() { }
#endif

} // namespace WebCore
