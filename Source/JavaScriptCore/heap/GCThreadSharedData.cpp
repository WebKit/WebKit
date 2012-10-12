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

#include "CopyVisitor.h"
#include "CopyVisitorInlineMethods.h"
#include "GCThread.h"
#include "JSGlobalData.h"
#include "MarkStack.h"
#include "SlotVisitor.h"
#include "SlotVisitorInlineMethods.h"

namespace JSC {

#if ENABLE(PARALLEL_GC)
void GCThreadSharedData::resetChildren()
{
    for (size_t i = 0; i < m_gcThreads.size(); ++i)
        m_gcThreads[i]->slotVisitor()->reset();
}

size_t GCThreadSharedData::childVisitCount()
{       
    unsigned long result = 0;
    for (unsigned i = 0; i < m_gcThreads.size(); ++i)
        result += m_gcThreads[i]->slotVisitor()->visitCount();
    return result;
}
#endif

GCThreadSharedData::GCThreadSharedData(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_copiedSpace(&globalData->heap.m_storageSpace)
    , m_shouldHashConst(false)
    , m_sharedMarkStack(m_segmentAllocator)
    , m_numberOfActiveParallelMarkers(0)
    , m_parallelMarkersShouldExit(false)
    , m_blocksToCopy(globalData->heap.m_blockSnapshot)
    , m_copyIndex(0)
    , m_currentPhase(NoPhase)
{
    m_copyLock.Init();
#if ENABLE(PARALLEL_GC)
    // Grab the lock so the new GC threads can be properly initialized before they start running.
    MutexLocker locker(m_markingLock);
    for (unsigned i = 1; i < Options::numberOfGCMarkers(); ++i) {
        SlotVisitor* slotVisitor = new SlotVisitor(*this);
        CopyVisitor* copyVisitor = new CopyVisitor(*this);
        GCThread* newThread = new GCThread(*this, slotVisitor, copyVisitor);
        ThreadIdentifier threadID = createThread(GCThread::gcThreadStartFunc, newThread, "JavaScriptCore::Marking");
        newThread->initializeThreadID(threadID);
        m_gcThreads.append(newThread);
    }
#endif
}

GCThreadSharedData::~GCThreadSharedData()
{
#if ENABLE(PARALLEL_GC)    
    // Destroy our marking threads.
    {
        MutexLocker markingLocker(m_markingLock);
        MutexLocker phaseLocker(m_phaseLock);
        ASSERT(m_currentPhase == NoPhase);
        m_parallelMarkersShouldExit = true;
        m_currentPhase = Exit;
        m_phaseCondition.broadcast();
    }
    for (unsigned i = 0; i < m_gcThreads.size(); ++i) {
        waitForThreadCompletion(m_gcThreads[i]->threadID());
        delete m_gcThreads[i];
    }
#endif
}
    
void GCThreadSharedData::reset()
{
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

void GCThreadSharedData::didStartMarking()
{
    MutexLocker markingLocker(m_markingLock);
    MutexLocker phaseLocker(m_phaseLock);
    ASSERT(m_currentPhase == NoPhase);
    m_currentPhase = Mark;
    m_parallelMarkersShouldExit = false;
    m_phaseCondition.broadcast();
}

void GCThreadSharedData::didFinishMarking()
{
    MutexLocker markingLocker(m_markingLock);
    MutexLocker phaseLocker(m_phaseLock);
    ASSERT(m_currentPhase == Mark);
    m_currentPhase = NoPhase;
    m_parallelMarkersShouldExit = true;
    m_markingCondition.broadcast();
}

void GCThreadSharedData::didStartCopying()
{
    {
        SpinLockHolder locker(&m_copyLock);
        m_blocksToCopy = m_globalData->heap.m_blockSnapshot;
        m_copyIndex = 0;
    }

    // We do this here so that we avoid a race condition where the main thread can 
    // blow through all of the copying work before the GCThreads fully wake up. 
    // The GCThreads then request a block from the CopiedSpace when the copying phase 
    // has completed, which isn't allowed.
    for (size_t i = 0; i < m_gcThreads.size(); i++)
        m_gcThreads[i]->copyVisitor()->startCopying();

    MutexLocker locker(m_phaseLock);
    ASSERT(m_currentPhase == NoPhase);
    m_currentPhase = Copy;
    m_phaseCondition.broadcast(); 
}

void GCThreadSharedData::didFinishCopying()
{
    MutexLocker locker(m_phaseLock);
    ASSERT(m_currentPhase == Copy);
    m_currentPhase = NoPhase;
    m_phaseCondition.broadcast();
}

} // namespace JSC
