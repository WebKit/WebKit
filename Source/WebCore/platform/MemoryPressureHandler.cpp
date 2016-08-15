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
#include "MemoryPressureHandler.h"

#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "FontCache.h"
#include "GCController.h"
#include "HTMLMediaElement.h"
#include "InspectorInstrumentation.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageCache.h"
#include "ScrollingThread.h"
#include "StyledElement.h"
#include "WorkerThread.h"
#include <JavaScriptCore/IncrementalSweeper.h>
#include <chrono>
#include <wtf/CurrentTime.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WEBCORE_EXPORT bool MemoryPressureHandler::ReliefLogger::s_loggingEnabled = false;

MemoryPressureHandler& MemoryPressureHandler::singleton()
{
    static NeverDestroyed<MemoryPressureHandler> memoryPressureHandler;
    return memoryPressureHandler;
}

MemoryPressureHandler::MemoryPressureHandler() 
    : m_installed(false)
    , m_lastRespondTime(0)
    , m_lowMemoryHandler([this] (Critical critical, Synchronous synchronous) { releaseMemory(critical, synchronous); })
    , m_underMemoryPressure(false)
#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    , m_memoryPressureReason(MemoryPressureReasonNone)
    , m_clearPressureOnMemoryRelease(true)
    , m_releaseMemoryBlock(0)
    , m_observer(0)
#elif OS(LINUX)
    , m_holdOffTimer(RunLoop::main(), this, &MemoryPressureHandler::holdOffTimerFired)
#endif
{
    platformInitialize();
}

void MemoryPressureHandler::releaseNoncriticalMemory()
{
    {
        ReliefLogger log("Purge inactive FontData");
        FontCache::singleton().purgeInactiveFontData();
    }

    {
        ReliefLogger log("Clear WidthCaches");
        clearWidthCaches();
    }

    {
        ReliefLogger log("Discard Selector Query Cache");
        for (auto* document : Document::allDocuments())
            document->clearSelectorQueryCache();
    }

    {
        ReliefLogger log("Prune MemoryCache dead resources");
        MemoryCache::singleton().pruneDeadResourcesToSize(0);
    }

    {
        ReliefLogger log("Prune presentation attribute cache");
        StyledElement::clearPresentationAttributeCache();
    }
}

void MemoryPressureHandler::releaseCriticalMemory(Synchronous synchronous)
{
    {
        ReliefLogger log("Empty the PageCache");
        // Right now, the only reason we call release critical memory while not under memory pressure is if the process is about to be suspended.
        PruningReason pruningReason = isUnderMemoryPressure() ? PruningReason::MemoryPressure : PruningReason::ProcessSuspended;
        PageCache::singleton().pruneToSizeNow(0, pruningReason);
    }

    {
        ReliefLogger log("Prune MemoryCache live resources");
        MemoryCache::singleton().pruneLiveResourcesToSize(0, /*shouldDestroyDecodedDataForAllLiveResources*/ true);
    }

    {
        ReliefLogger log("Drain CSSValuePool");
        CSSValuePool::singleton().drain();
    }

    {
        ReliefLogger log("Discard StyleResolvers");
        Vector<RefPtr<Document>> documents;
        copyToVector(Document::allDocuments(), documents);
        for (auto& document : documents)
            document->clearStyleResolver();
    }

    {
        ReliefLogger log("Discard all JIT-compiled code");
        GCController::singleton().deleteAllCode();
    }

#if ENABLE(VIDEO)
    {
        ReliefLogger log("Dropping buffered data from paused media elements");
        for (auto* mediaElement: HTMLMediaElement::allMediaElements()) {
            if (mediaElement->paused())
                mediaElement->purgeBufferedDataIfPossible();
        }
    }
#endif

    if (synchronous == Synchronous::Yes) {
        ReliefLogger log("Collecting JavaScript garbage");
        GCController::singleton().garbageCollectNow();
    } else
        GCController::singleton().garbageCollectNowIfNotDoneRecently();

    // We reduce tiling coverage while under memory pressure, so make sure to drop excess tiles ASAP.
    Page::forEachPage([](Page& page) {
        page.chrome().client().scheduleCompositingLayerFlush();
    });
}

void MemoryPressureHandler::jettisonExpensiveObjectsOnTopLevelNavigation()
{
#if PLATFORM(IOS)
    // Protect against doing excessive jettisoning during repeated navigations.
    const auto minimumTimeSinceNavigation = 2s;

    static auto timeOfLastNavigation = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    bool shouldJettison = now - timeOfLastNavigation >= minimumTimeSinceNavigation;
    timeOfLastNavigation = now;

    if (!shouldJettison)
        return;

    // Throw away linked JS code. Linked code is tied to a global object and is not reusable.
    // The immediate memory savings outweigh the cost of recompilation in case we go back again.
    GCController::singleton().deleteAllLinkedCode();
#endif
}

void MemoryPressureHandler::beginSimulatedMemoryPressure()
{
    m_isSimulatingMemoryPressure = true;
    MemoryPressureHandler::singleton().respondToMemoryPressure(Critical::Yes, Synchronous::Yes);
}

void MemoryPressureHandler::endSimulatedMemoryPressure()
{
    m_isSimulatingMemoryPressure = false;
}

void MemoryPressureHandler::releaseMemory(Critical critical, Synchronous synchronous)
{
    if (critical == Critical::Yes)
        releaseCriticalMemory(synchronous);

    releaseNoncriticalMemory();

    platformReleaseMemory(critical);

    {
        ReliefLogger log("Release free FastMalloc memory");
        // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
        WorkerThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(ASYNC_SCROLLING) && !PLATFORM(IOS)
        ScrollingThread::dispatch([]() {
            WTF::releaseFastMallocFreeMemory();
        });
#endif
        WTF::releaseFastMallocFreeMemory();
    }

#if ENABLE(RESOURCE_USAGE)
    Page::forEachPage([&](Page& page) {
        InspectorInstrumentation::didHandleMemoryPressure(page, critical);
    });
#endif
}

void MemoryPressureHandler::ReliefLogger::logMemoryUsageChange()
{
#if !RELEASE_LOG_DISABLED
#define STRING_SPECIFICATION "%{public}s"
#define MEMORYPRESSURE_LOG(...) RELEASE_LOG(__VA_ARGS__)
#else
#define STRING_SPECIFICATION "%s"
#define MEMORYPRESSURE_LOG(...) WTFLogAlways(__VA_ARGS__)
#endif

    size_t currentMemory = platformMemoryUsage();
    if (currentMemory == static_cast<size_t>(-1) || m_initialMemory == static_cast<size_t>(-1)) {
        MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": (Unable to get dirty memory information for process)", m_logString);
        return;
    }

    long memoryDiff = currentMemory - m_initialMemory;
    if (memoryDiff < 0)
        MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": -dirty %ld bytes (from %zu to %zu)", m_logString, (memoryDiff * -1), m_initialMemory, currentMemory);
    else if (memoryDiff > 0)
        MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": +dirty %ld bytes (from %zu to %zu)", m_logString, memoryDiff, m_initialMemory, currentMemory);
    else
        MEMORYPRESSURE_LOG("Memory pressure relief: " STRING_SPECIFICATION ": =dirty (at %zu bytes)", m_logString, currentMemory);
}

#if !PLATFORM(COCOA)
void MemoryPressureHandler::platformInitialize() { }
#endif

#if !PLATFORM(COCOA) && !OS(LINUX) && !PLATFORM(WIN)
void MemoryPressureHandler::install() { }
void MemoryPressureHandler::uninstall() { }
void MemoryPressureHandler::holdOff(unsigned) { }
void MemoryPressureHandler::respondToMemoryPressure(Critical, Synchronous) { }
void MemoryPressureHandler::platformReleaseMemory(Critical) { }
size_t MemoryPressureHandler::ReliefLogger::platformMemoryUsage() { return 0; }
#endif

} // namespace WebCore
