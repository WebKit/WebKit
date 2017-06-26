/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include "GCController.h"

#include "CommonVM.h"
#include <runtime/VM.h>
#include <runtime/JSLock.h>
#include <heap/Heap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/FastMalloc.h>
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {

static void collect()
{
    JSLockHolder lock(commonVM());
    commonVM().heap.collectNow(Async, CollectionScope::Full);
}

GCController& GCController::singleton()
{
    static NeverDestroyed<GCController> controller;
    return controller;
}

GCController::GCController()
    : m_GCTimer(*this, &GCController::gcTimerFired)
{
}

void GCController::garbageCollectSoon()
{
    // We only use reportAbandonedObjectGraph for systems for which there's an implementation
    // of the garbage collector timers in JavaScriptCore. We wouldn't need this if JavaScriptCore
    // used a timer implementation from WTF like RunLoop::Timer.
#if USE(CF) || USE(GLIB)
    JSLockHolder lock(commonVM());
    commonVM().heap.reportAbandonedObjectGraph();
#else
    garbageCollectOnNextRunLoop();
#endif
}

void GCController::garbageCollectOnNextRunLoop()
{
    if (!m_GCTimer.isActive())
        m_GCTimer.startOneShot(0_s);
}

void GCController::gcTimerFired()
{
    collect();
}

void GCController::garbageCollectNow()
{
    JSLockHolder lock(commonVM());
    if (!commonVM().heap.isCurrentThreadBusy()) {
        commonVM().heap.collectNow(Sync, CollectionScope::Full);
        WTF::releaseFastMallocFreeMemory();
    }
}

void GCController::garbageCollectNowIfNotDoneRecently()
{
#if USE(CF) || USE(GLIB)
    JSLockHolder lock(commonVM());
    if (!commonVM().heap.isCurrentThreadBusy())
        commonVM().heap.collectNowFullIfNotDoneRecently(Async);
#else
    garbageCollectSoon();
#endif
}

void GCController::garbageCollectOnAlternateThreadForDebugging(bool waitUntilDone)
{
    RefPtr<Thread> thread = Thread::create("WebCore: GCController", &collect);

    if (waitUntilDone) {
        thread->waitForCompletion();
        return;
    }

    thread->detach();
}

void GCController::setJavaScriptGarbageCollectorTimerEnabled(bool enable)
{
    commonVM().heap.setGarbageCollectionTimerEnabled(enable);
}

void GCController::deleteAllCode(DeleteAllCodeEffort effort)
{
    JSLockHolder lock(commonVM());
    commonVM().deleteAllCode(effort);
}

void GCController::deleteAllLinkedCode(DeleteAllCodeEffort effort)
{
    JSLockHolder lock(commonVM());
    commonVM().deleteAllLinkedCode(effort);
}

} // namespace WebCore
