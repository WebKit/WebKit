/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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
#include "GCThreadSharedData.h"

#include "JSGlobalData.h"
#include "MarkStack.h"
#include "SlotVisitor.h"
#include <wtf/MainThread.h>

namespace JSC {

#if ENABLE(PARALLEL_GC)
void GCThreadSharedData::resetChildren()
{
    for (unsigned i = 0; i < m_markingThreadsMarkStack.size(); ++i)
        m_markingThreadsMarkStack[i]->reset();
}

size_t GCThreadSharedData::childVisitCount()
{       
    unsigned long result = 0;
    for (unsigned i = 0; i < m_markingThreadsMarkStack.size(); ++i)
        result += m_markingThreadsMarkStack[i]->visitCount();
    return result;
}

void GCThreadSharedData::markingThreadMain(SlotVisitor* slotVisitor)
{
    WTF::registerGCThread();
    {
        ParallelModeEnabler enabler(*slotVisitor);
        slotVisitor->drainFromShared(SlotVisitor::SlaveDrain);
    }
    delete slotVisitor;
}

void GCThreadSharedData::markingThreadStartFunc(void* myVisitor)
{               
    SlotVisitor* slotVisitor = static_cast<SlotVisitor*>(myVisitor);

    slotVisitor->sharedData().markingThreadMain(slotVisitor);
}
#endif

GCThreadSharedData::GCThreadSharedData(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_copiedSpace(&globalData->heap.m_storageSpace)
    , m_shouldHashConst(false)
    , m_sharedMarkStack(m_segmentAllocator)
    , m_numberOfActiveParallelMarkers(0)
    , m_parallelMarkersShouldExit(false)
{
#if ENABLE(PARALLEL_GC)
    for (unsigned i = 1; i < Options::numberOfGCMarkers(); ++i) {
        SlotVisitor* slotVisitor = new SlotVisitor(*this);
        m_markingThreadsMarkStack.append(slotVisitor);
        m_markingThreads.append(createThread(markingThreadStartFunc, slotVisitor, "JavaScriptCore::Marking"));
        ASSERT(m_markingThreads.last());
    }
#endif
}

GCThreadSharedData::~GCThreadSharedData()
{
#if ENABLE(PARALLEL_GC)    
    // Destroy our marking threads.
    {
        MutexLocker locker(m_markingLock);
        m_parallelMarkersShouldExit = true;
        m_markingCondition.broadcast();
    }
    for (unsigned i = 0; i < m_markingThreads.size(); ++i)
        waitForThreadCompletion(m_markingThreads[i]);
#endif
}
    
void GCThreadSharedData::reset()
{
    ASSERT(!m_numberOfActiveParallelMarkers);
    ASSERT(!m_parallelMarkersShouldExit);
    ASSERT(m_sharedMarkStack.isEmpty());
    
#if ENABLE(PARALLEL_GC)
    m_segmentAllocator.shrinkReserve();
    m_opaqueRoots.clear();
#else
    ASSERT(m_opaqueRoots.isEmpty());
#endif
    m_weakReferenceHarvesters.removeAll();

    if (m_shouldHashConst) {
        m_globalData->resetNewStringsSinceLastHashConst();
        m_shouldHashConst = false;
    }
}

} // namespace JSC
