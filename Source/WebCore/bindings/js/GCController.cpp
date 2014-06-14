/*
 * Copyright (C) 2007, 2014 Apple Inc. All rights reserved.
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

#include "JSDOMWindow.h"
#include <runtime/VM.h>
#include <runtime/JSLock.h>
#include <heap/Heap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/FastMalloc.h>

using namespace JSC;

namespace WebCore {

static void collect(void*)
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    JSDOMWindow::commonVM().heap.collectAllGarbage();
}

GCController& gcController()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(GCController, staticGCController, ());
    return staticGCController;
}

GCController::GCController()
#if !USE(CF)
    : m_GCTimer(this, &GCController::gcTimerFired)
#endif
{
}

void GCController::garbageCollectSoon()
{
    // We only use reportAbandonedObjectGraph on systems with CoreFoundation 
    // since it uses a run-loop-based timer that is currently only available on
    // systems with CoreFoundation. If and when the notion of a run loop is pushed 
    // down into WTF so that more platforms can take advantage of it, we will be 
    // able to use reportAbandonedObjectGraph on more platforms.
#if USE(CF)
    JSLockHolder lock(JSDOMWindow::commonVM());
    JSDOMWindow::commonVM().heap.reportAbandonedObjectGraph();
#else
    if (!m_GCTimer.isActive())
        m_GCTimer.startOneShot(0);
#endif
}

#if !USE(CF)
void GCController::gcTimerFired(Timer<GCController>*)
{
    collect(nullptr);
}
#endif

void GCController::garbageCollectNow()
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    if (!JSDOMWindow::commonVM().heap.isBusy()) {
        JSDOMWindow::commonVM().heap.collectAllGarbage();
        WTF::releaseFastMallocFreeMemory();
    }
}

void GCController::garbageCollectOnAlternateThreadForDebugging(bool waitUntilDone)
{
    ThreadIdentifier threadID = createThread(collect, 0, "WebCore: GCController");

    if (waitUntilDone) {
        waitForThreadCompletion(threadID);
        return;
    }

    detachThread(threadID);
}

void GCController::releaseExecutableMemory()
{
    JSLockHolder lock(JSDOMWindow::commonVM());

    // We shouldn't have any JavaScript running on our stack when this function is called.
    // The following line asserts that, but to be safe we check this in release builds anyway.
    ASSERT(!JSDOMWindow::commonVM().entryScope);
    if (JSDOMWindow::commonVM().entryScope)
        return;

    JSDOMWindow::commonVM().releaseExecutableMemory();
}

void GCController::setJavaScriptGarbageCollectorTimerEnabled(bool enable)
{
    JSDOMWindow::commonVM().heap.setGarbageCollectionTimerEnabled(enable);
}

void GCController::discardAllCompiledCode()
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    JSDOMWindow::commonVM().discardAllCode();
}

} // namespace WebCore
