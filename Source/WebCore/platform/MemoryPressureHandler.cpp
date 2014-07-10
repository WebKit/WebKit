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
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "GCController.h"
#include "JSDOMWindow.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageCache.h"
#include "ScrollingThread.h"
#include "StorageThread.h"
#include "WorkerThread.h"
#include <wtf/CurrentTime.h>
#include <wtf/FastMalloc.h>
#include <wtf/Functional.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

bool MemoryPressureHandler::ReliefLogger::s_loggingEnabled = false;

MemoryPressureHandler& memoryPressureHandler()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(MemoryPressureHandler, staticMemoryPressureHandler, ());
    return staticMemoryPressureHandler;
}

MemoryPressureHandler::MemoryPressureHandler() 
    : m_installed(false)
    , m_lastRespondTime(0)
    , m_lowMemoryHandler(releaseMemory)
    , m_underMemoryPressure(false)
#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    , m_memoryPressureReason(MemoryPressureReasonNone)
    , m_clearPressureOnMemoryRelease(true)
    , m_releaseMemoryBlock(0)
    , m_observer(0)
#endif
{
}

void MemoryPressureHandler::releaseNoncriticalMemory()
{
    {
        ReliefLogger log("Purge inactive FontData");
        fontCache().purgeInactiveFontData();
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
        ReliefLogger log("Clearing JS string cache");
        JSDOMWindow::commonVM().stringCache.clear();
    }
}

void MemoryPressureHandler::releaseCriticalMemory()
{
    {
        ReliefLogger log("Empty the PageCache");
        int savedPageCacheCapacity = pageCache()->capacity();
        pageCache()->setCapacity(0);
        pageCache()->setCapacity(savedPageCacheCapacity);
    }

    {
        ReliefLogger log("Prune MemoryCache");
        memoryCache()->pruneToPercentage(0);
    }

    {
        ReliefLogger log("Drain CSSValuePool");
        cssValuePool().drain();
    }

    {
        ReliefLogger log("Discard StyleResolvers");
        for (auto* document : Document::allDocuments())
            document->clearStyleResolver();
    }

    {
        ReliefLogger log("Discard all JIT-compiled code");
        gcController().discardAllCompiledCode();
    }
}

void MemoryPressureHandler::releaseMemory(bool critical)
{
    releaseNoncriticalMemory();

    if (critical)
        releaseCriticalMemory();

    platformReleaseMemory(critical);

    {
        ReliefLogger log("Release free FastMalloc memory");
        // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
        StorageThread::releaseFastMallocFreeMemoryInAllThreads();
        WorkerThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(ASYNC_SCROLLING) && !PLATFORM(IOS)
        ScrollingThread::dispatch(bind(WTF::releaseFastMallocFreeMemory));
#endif
        WTF::releaseFastMallocFreeMemory();
    }
}

#if !PLATFORM(COCOA)
void MemoryPressureHandler::install() { }
void MemoryPressureHandler::uninstall() { }
void MemoryPressureHandler::holdOff(unsigned) { }
void MemoryPressureHandler::respondToMemoryPressure(bool) { }
void MemoryPressureHandler::platformReleaseMemory(bool) { }
void MemoryPressureHandler::ReliefLogger::platformLog() { }
size_t MemoryPressureHandler::ReliefLogger::platformMemoryUsage() { return 0; }
#endif

} // namespace WebCore
