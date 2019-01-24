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
#include "JSHTMLDocument.h"
#include "Location.h"
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/HeapSnapshotBuilder.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <pal/Logging.h>
#include <wtf/FastMalloc.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
using namespace JSC;

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
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.dumpGCHeap", [] {
            GCController::singleton().dumpHeap();
        });
    });
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
    auto thread = Thread::create("WebCore: GCController", &collect);

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

void GCController::dumpHeap()
{
    FileSystem::PlatformFileHandle fileHandle;
    String tempFilePath = FileSystem::openTemporaryFile("GCHeap"_s, fileHandle);
    if (!FileSystem::isHandleValid(fileHandle)) {
        WTFLogAlways("Dumping GC heap failed to open temporary file");
        return;
    }

    VM& vm = commonVM();
    JSLockHolder lock(vm);

    sanitizeStackForVM(&vm);

    String jsonData;
    {
        DeferGCForAWhile deferGC(vm.heap); // Prevent concurrent GC from interfering with the full GC that the snapshot does.

        HeapSnapshotBuilder snapshotBuilder(vm.ensureHeapProfiler(), HeapSnapshotBuilder::SnapshotType::GCDebuggingSnapshot);
        snapshotBuilder.buildSnapshot();

        jsonData = snapshotBuilder.json();
    }

    CString utf8String = jsonData.utf8();

    FileSystem::writeToFile(fileHandle, utf8String.data(), utf8String.length());
    FileSystem::closeFile(fileHandle);
    
    WTFLogAlways("Dumped GC heap to %s", tempFilePath.utf8().data());
}

} // namespace WebCore
