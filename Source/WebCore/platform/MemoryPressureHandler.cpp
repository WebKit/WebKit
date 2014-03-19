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

MemoryPressureHandler& memoryPressureHandler()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(MemoryPressureHandler, staticMemoryPressureHandler, ());
    return staticMemoryPressureHandler;
}

MemoryPressureHandler::MemoryPressureHandler() 
    : m_installed(false)
    , m_lastRespondTime(0)
    , m_lowMemoryHandler(releaseMemory)
#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    , m_receivedMemoryPressure(0)
    , m_memoryPressureReason(MemoryPressureReasonNone)
    , m_clearPressureOnMemoryRelease(true)
    , m_releaseMemoryBlock(0)
    , m_observer(0)
#endif
{
}

void MemoryPressureHandler::releaseMemory(bool critical)
{
    int savedPageCacheCapacity = pageCache()->capacity();
    pageCache()->setCapacity(0);
    pageCache()->setCapacity(savedPageCacheCapacity);

    fontCache()->purgeInactiveFontData();

    memoryCache()->pruneToPercentage(0);

    cssValuePool().drain();

    clearWidthCaches();

    for (auto* document : Document::allDocuments())
        document->clearStyleResolver();

    gcController().discardAllCompiledCode();

    platformReleaseMemory(critical);

    // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
    StorageThread::releaseFastMallocFreeMemoryInAllThreads();
    WorkerThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(ASYNC_SCROLLING)
    ScrollingThread::dispatch(bind(WTF::releaseFastMallocFreeMemory));
#endif
    WTF::releaseFastMallocFreeMemory();
}

#if !PLATFORM(MAC)

void MemoryPressureHandler::install() { }
void MemoryPressureHandler::uninstall() { }
void MemoryPressureHandler::holdOff(unsigned) { }
void MemoryPressureHandler::respondToMemoryPressure() { }
void MemoryPressureHandler::platformReleaseMemory(bool) { }

#endif

} // namespace WebCore
