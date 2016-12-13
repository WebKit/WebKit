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

#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "FontCache.h"
#include "GCController.h"
#include "HTMLMediaElement.h"
#include "InlineStyleSheetOwner.h"
#include "InspectorInstrumentation.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageCache.h"
#include "ScrollingThread.h"
#include "StyleScope.h"
#include "StyledElement.h"
#include "WorkerThread.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

static void releaseNoncriticalMemory()
{
    FontCache::singleton().purgeInactiveFontData();

    clearWidthCaches();

    for (auto* document : Document::allDocuments())
        document->clearSelectorQueryCache();

    MemoryCache::singleton().pruneDeadResourcesToSize(0);

    StyledElement::clearPresentationAttributeCache();

    InlineStyleSheetOwner::clearCache();
}

static void releaseCriticalMemory(Synchronous synchronous)
{
    // Right now, the only reason we call release critical memory while not under memory pressure is if the process is about to be suspended.
    PruningReason pruningReason = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? PruningReason::MemoryPressure : PruningReason::ProcessSuspended;
    PageCache::singleton().pruneToSizeNow(0, pruningReason);

    MemoryCache::singleton().pruneLiveResourcesToSize(0, /*shouldDestroyDecodedDataForAllLiveResources*/ true);

    CSSValuePool::singleton().drain();

    Vector<RefPtr<Document>> documents;
    copyToVector(Document::allDocuments(), documents);
    for (auto& document : documents)
        document->styleScope().clearResolver();

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
#if PLATFORM(IOS)
        GCController::singleton().garbageCollectNowIfNotDoneRecently();
#else
        GCController::singleton().garbageCollectSoon();
#endif
    }

    // We reduce tiling coverage while under memory pressure, so make sure to drop excess tiles ASAP.
    Page::forEachPage([](Page& page) {
        page.chrome().client().scheduleCompositingLayerFlush();
    });
}

void releaseMemory(Critical critical, Synchronous synchronous)
{
    if (critical == Critical::Yes)
        releaseCriticalMemory(synchronous);

    releaseNoncriticalMemory();

    platformReleaseMemory(critical);

    // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
    WorkerThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(ASYNC_SCROLLING) && !PLATFORM(IOS)
    ScrollingThread::dispatch([]() {
        WTF::releaseFastMallocFreeMemory();
    });
#endif
    WTF::releaseFastMallocFreeMemory();

#if ENABLE(RESOURCE_USAGE)
    Page::forEachPage([&](Page& page) {
        InspectorInstrumentation::didHandleMemoryPressure(page, critical);
    });
#endif
}

#if !PLATFORM(COCOA)
void platformReleaseMemory(Critical) { }
void jettisonExpensiveObjectsOnTopLevelNavigation() { }
void registerMemoryReleaseNotifyCallbacks() { }
#endif

} // namespace WebCore
