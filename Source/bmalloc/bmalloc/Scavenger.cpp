/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Scavenger.h"

#include "AvailableMemory.h"
#include "Heap.h"
#include <thread>

namespace bmalloc {

Scavenger::Scavenger(std::lock_guard<StaticMutex>&)
{
#if BOS(DARWIN)
    auto queue = dispatch_queue_create("WebKit Malloc Memory Pressure Handler", DISPATCH_QUEUE_SERIAL);
    m_pressureHandlerDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0, DISPATCH_MEMORYPRESSURE_CRITICAL, queue);
    dispatch_source_set_event_handler(m_pressureHandlerDispatchSource, ^{
        scavenge();
    });
    dispatch_resume(m_pressureHandlerDispatchSource);
    dispatch_release(queue);
#endif
    
    m_thread = std::thread(&threadEntryPoint, this);
}

void Scavenger::run()
{
    std::lock_guard<Mutex> lock(m_mutex);
    runHoldingLock();
}

void Scavenger::runHoldingLock()
{
    m_state = State::Run;
    m_condition.notify_all();
}

void Scavenger::runSoon()
{
    std::lock_guard<Mutex> lock(m_mutex);
    runSoonHoldingLock();
}

void Scavenger::runSoonHoldingLock()
{
    if (willRunSoon())
        return;
    m_state = State::RunSoon;
    m_condition.notify_all();
}

void Scavenger::didStartGrowing()
{
    // We don't really need to lock here, since this is just a heuristic.
    m_isProbablyGrowing = true;
}

void Scavenger::scheduleIfUnderMemoryPressure(size_t bytes)
{
    std::lock_guard<Mutex> lock(m_mutex);
    scheduleIfUnderMemoryPressureHoldingLock(bytes);
}

void Scavenger::scheduleIfUnderMemoryPressureHoldingLock(size_t bytes)
{
    m_scavengerBytes += bytes;
    if (m_scavengerBytes < scavengerBytesPerMemoryPressureCheck)
        return;

    m_scavengerBytes = 0;

    if (willRun())
        return;

    if (!isUnderMemoryPressure())
        return;

    m_isProbablyGrowing = false;
    runHoldingLock();
}

void Scavenger::schedule(size_t bytes)
{
    std::lock_guard<Mutex> lock(m_mutex);
    scheduleIfUnderMemoryPressureHoldingLock(bytes);
    
    if (willRunSoon())
        return;
    
    m_isProbablyGrowing = false;
    runSoonHoldingLock();
}

void Scavenger::scavenge()
{
    std::lock_guard<StaticMutex> lock(Heap::mutex());
    for (unsigned i = numHeaps; i--;)
        PerProcess<PerHeapKind<Heap>>::get()->at(i).scavenge(lock);
}

void Scavenger::threadEntryPoint(Scavenger* scavenger)
{
    scavenger->threadRunLoop();
}

void Scavenger::threadRunLoop()
{
    setSelfQOSClass();
    
    // This loop ratchets downward from most active to least active state. While
    // we ratchet downward, any other thread may reset our state.
    
    // We require any state change while we are sleeping to signal to our
    // condition variable and wake us up.
    
    auto truth = [] { return true; };
    
    while (truth()) {
        if (m_state == State::Sleep) {
            std::unique_lock<Mutex> lock(m_mutex);
            m_condition.wait(lock, [&]() { return m_state != State::Sleep; });
        }
        
        if (m_state == State::RunSoon) {
            std::unique_lock<Mutex> lock(m_mutex);
            m_condition.wait_for(lock, asyncTaskSleepDuration, [&]() { return m_state != State::RunSoon; });
        }
        
        m_state = State::Sleep;
        
        setSelfQOSClass();
        
        {
            std::unique_lock<Mutex> lock(m_mutex);
            if (m_isProbablyGrowing && !isUnderMemoryPressure()) {
                m_isProbablyGrowing = false;
                runSoonHoldingLock();
                continue;
            }
        }
        
        scavenge();
    }
}

void Scavenger::setSelfQOSClass()
{
#if BOS(DARWIN)
    pthread_set_qos_class_self_np(requestedScavengerThreadQOSClass(), 0);
#endif
}

} // namespace bmalloc

