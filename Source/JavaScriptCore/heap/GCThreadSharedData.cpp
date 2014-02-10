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
#include "CopyVisitorInlines.h"
#include "GCThread.h"
#include "MarkStack.h"
#include "Operations.h"
#include "SlotVisitor.h"
#include "SlotVisitorInlines.h"
#include "VM.h"

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

size_t GCThreadSharedData::childBytesVisited()
{       
    size_t result = 0;
    for (unsigned i = 0; i < m_gcThreads.size(); ++i)
        result += m_gcThreads[i]->slotVisitor()->bytesVisited();
    return result;
}

size_t GCThreadSharedData::childBytesCopied()
{       
    size_t result = 0;
    for (unsigned i = 0; i < m_gcThreads.size(); ++i)
        result += m_gcThreads[i]->slotVisitor()->bytesCopied();
    return result;
}
#endif

GCThreadSharedData::GCThreadSharedData(VM* vm)
    : m_vm(vm)
    , m_copiedSpace(&vm->heap.m_storageSpace)
    , m_shouldHashCons(false)
    , m_sharedMarkStack(vm->heap.blockAllocator())
    , m_numberOfActiveParallelMarkers(0)
    , m_parallelMarkersShouldExit(false)
    , m_copyIndex(0)
    , m_numberOfActiveGCThreads(0)
    , m_gcThreadsShouldWait(false)
    , m_currentPhase(NoPhase)
{
    m_copyLock.Init();
#if ENABLE(PARALLEL_GC)
    // Grab the lock so the new GC threads can be properly initialized before they start running.
    std::unique_lock<std::mutex> lock(m_phaseMutex);
    for (unsigned i = 1; i < Options::numberOfGCMarkers(); ++i) {
        m_numberOfActiveGCThreads++;
        SlotVisitor* slotVisitor = new SlotVisitor(*this);
        CopyVisitor* copyVisitor = new CopyVisitor(*this);
        GCThread* newThread = new GCThread(*this, slotVisitor, copyVisitor);
        ThreadIdentifier threadID = createThread(GCThread::gcThreadStartFunc, newThread, "JavaScriptCore::Marking");
        newThread->initializeThreadID(threadID);
        m_gcThreads.append(newThread);
    }

    // Wait for all the GCThreads to get to the right place.
    m_activityConditionVariable.wait(lock, [this] { return !m_numberOfActiveGCThreads; });
#endif
}

GCThreadSharedData::~GCThreadSharedData()
{
#if ENABLE(PARALLEL_GC)    
    // Destroy our marking threads.
    {
        std::lock_guard<std::mutex> markingLock(m_markingMutex);
        std::lock_guard<std::mutex> phaseLock(m_phaseMutex);
        ASSERT(m_currentPhase == NoPhase);
        m_parallelMarkersShouldExit = true;
        m_gcThreadsShouldWait = false;
        m_currentPhase = Exit;
        m_phaseConditionVariable.notify_all();
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
    
    m_weakReferenceHarvesters.removeAll();

    if (m_shouldHashCons) {
        m_vm->resetNewStringsSinceLastHashCons();
        m_shouldHashCons = false;
    }
}

void GCThreadSharedData::startNextPhase(GCPhase phase)
{
    std::lock_guard<std::mutex> lock(m_phaseMutex);
    ASSERT(!m_gcThreadsShouldWait);
    ASSERT(m_currentPhase == NoPhase);
    m_gcThreadsShouldWait = true;
    m_currentPhase = phase;
    m_phaseConditionVariable.notify_all();
}

void GCThreadSharedData::endCurrentPhase()
{
    ASSERT(m_gcThreadsShouldWait);
    std::unique_lock<std::mutex> lock(m_phaseMutex);
    m_currentPhase = NoPhase;
    m_gcThreadsShouldWait = false;
    m_phaseConditionVariable.notify_all();
    m_activityConditionVariable.wait(lock, [this] { return !m_numberOfActiveGCThreads; });
}

void GCThreadSharedData::didStartMarking()
{
    if (m_vm->heap.operationInProgress() == FullCollection) {
#if ENABLE(PARALLEL_GC)
        m_opaqueRoots.clear();
#else
        ASSERT(m_opaqueRoots.isEmpty());
#endif
}
    std::lock_guard<std::mutex> lock(m_markingMutex);
    m_parallelMarkersShouldExit = false;
    startNextPhase(Mark);
}

void GCThreadSharedData::didFinishMarking()
{
    {
        std::lock_guard<std::mutex> lock(m_markingMutex);
        m_parallelMarkersShouldExit = true;
        m_markingConditionVariable.notify_all();
    }

    ASSERT(m_currentPhase == Mark);
    endCurrentPhase();
}

void GCThreadSharedData::didStartCopying()
{
    {
        SpinLockHolder locker(&m_copyLock);
        if (m_vm->heap.operationInProgress() == EdenCollection) {
            // Reset the vector to be empty, but don't throw away the backing store.
            m_blocksToCopy.shrink(0);
            for (CopiedBlock* block = m_copiedSpace->m_newGen.fromSpace->head(); block; block = block->next())
                m_blocksToCopy.append(block);
        } else {
            ASSERT(m_vm->heap.operationInProgress() == FullCollection);
            WTF::copyToVector(m_copiedSpace->m_blockSet, m_blocksToCopy);
        }
        m_copyIndex = 0;
    }

    // We do this here so that we avoid a race condition where the main thread can 
    // blow through all of the copying work before the GCThreads fully wake up. 
    // The GCThreads then request a block from the CopiedSpace when the copying phase 
    // has completed, which isn't allowed.
    for (size_t i = 0; i < m_gcThreads.size(); i++)
        m_gcThreads[i]->copyVisitor()->startCopying();

    startNextPhase(Copy);
}

void GCThreadSharedData::didFinishCopying()
{
    ASSERT(m_currentPhase == Copy);
    endCurrentPhase();
}

} // namespace JSC
