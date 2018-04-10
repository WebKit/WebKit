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

#include "AllIsoHeapsInlines.h"
#include "AvailableMemory.h"
#include "Environment.h"
#include "Heap.h"
#if BOS(DARWIN)
#import <dispatch/dispatch.h>
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#endif
#include <thread>

namespace bmalloc {

static constexpr bool verbose = false;

Scavenger::Scavenger(std::lock_guard<Mutex>&)
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

inline void dumpStats()
{
    auto dump = [] (auto* string, auto size) {
        fprintf(stderr, "%s %zuMB\n", string, static_cast<size_t>(size) / 1024 / 1024);
    };

#if BOS(DARWIN)
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t vmSize = TASK_VM_INFO_COUNT;
    if (KERN_SUCCESS == task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)(&vmInfo), &vmSize)) {
        dump("phys_footrpint", vmInfo.phys_footprint);
        dump("internal+compressed", vmInfo.internal + vmInfo.compressed);
    }
#endif

    dump("bmalloc-freeable", PerProcess<Scavenger>::get()->freeableMemory());
    dump("bmalloc-footprint", PerProcess<Scavenger>::get()->footprint());
}

void Scavenger::scavenge()
{
    if (verbose) {
        fprintf(stderr, "--------------------------------\n");
        fprintf(stderr, "--before scavenging--\n");
        dumpStats();
    }

    {
        std::lock_guard<Mutex> lock(Heap::mutex());
        for (unsigned i = numHeaps; i--;) {
            if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                continue;
            PerProcess<PerHeapKind<Heap>>::get()->at(i).scavenge(lock);
        }
    }
    
    {
        std::lock_guard<Mutex> locker(m_isoScavengeLock);
        RELEASE_BASSERT(!m_deferredDecommits.size());
        PerProcess<AllIsoHeaps>::get()->forEach(
            [&] (IsoHeapImplBase& heap) {
                heap.scavenge(m_deferredDecommits);
            });
        IsoHeapImplBase::finishScavenging(m_deferredDecommits);
        m_deferredDecommits.shrink(0);
    }

    if (verbose) {
        fprintf(stderr, "--after scavenging--\n");
        dumpStats();
        fprintf(stderr, "--------------------------------\n");
    }
}

size_t Scavenger::freeableMemory()
{
    size_t result = 0;
    {
        std::lock_guard<Mutex> lock(Heap::mutex());
        for (unsigned i = numHeaps; i--;) {
            if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                continue;
            result += PerProcess<PerHeapKind<Heap>>::get()->at(i).freeableMemory(lock);
        }
    }

    std::lock_guard<Mutex> locker(m_isoScavengeLock);
    PerProcess<AllIsoHeaps>::get()->forEach(
        [&] (IsoHeapImplBase& heap) {
            result += heap.freeableMemory();
        });

    return result;
}

size_t Scavenger::footprint()
{
    RELEASE_BASSERT(!PerProcess<Environment>::get()->isDebugHeapEnabled());

    size_t result = 0;
    for (unsigned i = numHeaps; i--;) {
        if (!isActiveHeapKind(static_cast<HeapKind>(i)))
            continue;
        result += PerProcess<PerHeapKind<Heap>>::get()->at(i).footprint();
    }

    PerProcess<AllIsoHeaps>::get()->forEach(
        [&] (IsoHeapImplBase& heap) {
            result += heap.footprint();
        });

    return result;
}

void Scavenger::threadEntryPoint(Scavenger* scavenger)
{
    scavenger->threadRunLoop();
}

void Scavenger::threadRunLoop()
{
    setSelfQOSClass();
    setThreadName("bmalloc scavenger");
    
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
            if (verbose) {
                fprintf(stderr, "--------------------------------\n");
                fprintf(stderr, "considering running scavenger\n");
                dumpStats();
                fprintf(stderr, "--------------------------------\n");
            }

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

void Scavenger::setThreadName(const char* name)
{
    BUNUSED(name);
#if BOS(DARWIN)
    pthread_setname_np(name);
#elif BOS(LINUX)
    // Truncate the given name since Linux limits the size of the thread name 16 including null terminator.
    std::array<char, 16> buf;
    strncpy(buf.data(), name, buf.size() - 1);
    buf[buf.size() - 1] = '\0';
    pthread_setname_np(pthread_self(), buf.data());
#endif
}

void Scavenger::setSelfQOSClass()
{
#if BOS(DARWIN)
    pthread_set_qos_class_self_np(requestedScavengerThreadQOSClass(), 0);
#endif
}

} // namespace bmalloc

