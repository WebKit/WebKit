/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "BulkDecommit.h"
#include "Environment.h"
#include "Heap.h"
#include "IsoHeapImplInlines.h"
#if BOS(DARWIN)
#import <dispatch/dispatch.h>
#import <mach/host_info.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#endif
#include <stdio.h>
#include <thread>

namespace bmalloc {

static constexpr bool verbose = false;

struct PrintTime {
    PrintTime(const char* str) 
        : string(str)
    { }

    ~PrintTime()
    {
        if (!printed)
            print();
    }
    void print()
    {
        if (verbose) {
            fprintf(stderr, "%s %lfms\n", string, static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()) / 1000);
            printed = true;
        }
    }
    const char* string;
    std::chrono::steady_clock::time_point start { std::chrono::steady_clock::now() };
    bool printed { false };
};

DEFINE_STATIC_PER_PROCESS_STORAGE(Scavenger);

Scavenger::Scavenger(const LockHolder&)
{
    BASSERT(!Environment::get()->isDebugHeapEnabled());

#if BOS(DARWIN)
    auto queue = dispatch_queue_create("WebKit Malloc Memory Pressure Handler", DISPATCH_QUEUE_SERIAL);
    m_pressureHandlerDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0, DISPATCH_MEMORYPRESSURE_CRITICAL, queue);
    dispatch_source_set_event_handler(m_pressureHandlerDispatchSource, ^{
        scavenge();
    });
    dispatch_resume(m_pressureHandlerDispatchSource);
    dispatch_release(queue);
#endif
#if BUSE(PARTIAL_SCAVENGE)
    m_waitTime = std::chrono::milliseconds(m_isInMiniMode ? 200 : 2000);
#else
    m_waitTime = std::chrono::milliseconds(10);
#endif

    m_thread = std::thread(&threadEntryPoint, this);
}

void Scavenger::run()
{
    LockHolder lock(mutex());
    run(lock);
}

void Scavenger::run(const LockHolder&)
{
    m_state = State::Run;
    m_condition.notify_all();
}

void Scavenger::runSoon()
{
    LockHolder lock(mutex());
    runSoon(lock);
}

void Scavenger::runSoon(const LockHolder&)
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
    LockHolder lock(mutex());
    scheduleIfUnderMemoryPressure(lock, bytes);
}

void Scavenger::scheduleIfUnderMemoryPressure(const LockHolder& lock, size_t bytes)
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
    run(lock);
}

void Scavenger::schedule(size_t bytes)
{
    LockHolder lock(mutex());
    scheduleIfUnderMemoryPressure(lock, bytes);
    
    if (willRunSoon())
        return;
    
    m_isProbablyGrowing = false;
    runSoon(lock);
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
        dump("phys_footprint", vmInfo.phys_footprint);
        dump("internal+compressed", vmInfo.internal + vmInfo.compressed);
    }
#endif

    dump("bmalloc-freeable", Scavenger::get()->freeableMemory());
    dump("bmalloc-footprint", Scavenger::get()->footprint());
}

std::chrono::milliseconds Scavenger::timeSinceLastFullScavenge()
{
    UniqueLockHolder lock(mutex());
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastFullScavengeTime);
}

#if BUSE(PARTIAL_SCAVENGE)
std::chrono::milliseconds Scavenger::timeSinceLastPartialScavenge()
{
    UniqueLockHolder lock(mutex());
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastPartialScavengeTime);
}
#endif

void Scavenger::enableMiniMode()
{
    m_isInMiniMode = true; // We just store to this racily. The scavenger thread will eventually pick up the right value.
    if (m_state == State::RunSoon)
        run();
}

void Scavenger::scavenge()
{
    UniqueLockHolder lock(m_scavengingMutex);

    if (verbose) {
        fprintf(stderr, "--------------------------------\n");
        fprintf(stderr, "--before scavenging--\n");
        dumpStats();
    }

    {
        BulkDecommit decommitter;

        {
            PrintTime printTime("\nfull scavenge under lock time");
#if !BUSE(PARTIAL_SCAVENGE)
            size_t deferredDecommits = 0;
#endif
            LockHolder lock(Heap::mutex());
            for (unsigned i = numHeaps; i--;) {
                if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                    continue;
#if BUSE(PARTIAL_SCAVENGE)
                PerProcess<PerHeapKind<Heap>>::get()->at(i).scavenge(lock, decommitter);
#else
                PerProcess<PerHeapKind<Heap>>::get()->at(i).scavenge(lock, decommitter, deferredDecommits);
#endif
            }
            decommitter.processEager();

#if !BUSE(PARTIAL_SCAVENGE)
            if (deferredDecommits)
                m_state = State::RunSoon;
#endif
        }

        {
            PrintTime printTime("full scavenge lazy decommit time");
            decommitter.processLazy();
        }

        {
            PrintTime printTime("full scavenge mark all as eligible time");
            LockHolder lock(Heap::mutex());
            for (unsigned i = numHeaps; i--;) {
                if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                    continue;
                PerProcess<PerHeapKind<Heap>>::get()->at(i).markAllLargeAsEligibile(lock);
            }
        }
    }

    {
        RELEASE_BASSERT(!m_deferredDecommits.size());
        AllIsoHeaps::get()->forEach(
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

    {
        UniqueLockHolder lock(mutex());
        m_lastFullScavengeTime = std::chrono::steady_clock::now();
    }
}

#if BUSE(PARTIAL_SCAVENGE)
void Scavenger::partialScavenge()
{
    UniqueLockHolder lock(m_scavengingMutex);

    if (verbose) {
        fprintf(stderr, "--------------------------------\n");
        fprintf(stderr, "--before partial scavenging--\n");
        dumpStats();
    }

    {
        BulkDecommit decommitter;
        {
            PrintTime printTime("\npartialScavenge under lock time");
            LockHolder lock(Heap::mutex());
            for (unsigned i = numHeaps; i--;) {
                if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                    continue;
                Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(i);
                size_t freeableMemory = heap.freeableMemory(lock);
                if (freeableMemory < 4 * MB)
                    continue;
                heap.scavengeToHighWatermark(lock, decommitter);
            }

            decommitter.processEager();
        }

        {
            PrintTime printTime("partialScavenge lazy decommit time");
            decommitter.processLazy();
        }

        {
            PrintTime printTime("partialScavenge mark all as eligible time");
            LockHolder lock(Heap::mutex());
            for (unsigned i = numHeaps; i--;) {
                if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                    continue;
                Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(i);
                heap.markAllLargeAsEligibile(lock);
            }
        }
    }

    {
        RELEASE_BASSERT(!m_deferredDecommits.size());
        AllIsoHeaps::get()->forEach(
            [&] (IsoHeapImplBase& heap) {
                heap.scavengeToHighWatermark(m_deferredDecommits);
            });
        IsoHeapImplBase::finishScavenging(m_deferredDecommits);
        m_deferredDecommits.shrink(0);
    }

    if (verbose) {
        fprintf(stderr, "--after partial scavenging--\n");
        dumpStats();
        fprintf(stderr, "--------------------------------\n");
    }

    {
        UniqueLockHolder lock(mutex());
        m_lastPartialScavengeTime = std::chrono::steady_clock::now();
    }
}
#endif

size_t Scavenger::freeableMemory()
{
    size_t result = 0;
    {
        LockHolder lock(Heap::mutex());
        for (unsigned i = numHeaps; i--;) {
            if (!isActiveHeapKind(static_cast<HeapKind>(i)))
                continue;
            result += PerProcess<PerHeapKind<Heap>>::get()->at(i).freeableMemory(lock);
        }
    }

    AllIsoHeaps::get()->forEach(
        [&] (IsoHeapImplBase& heap) {
            result += heap.freeableMemory();
        });

    return result;
}

size_t Scavenger::footprint()
{
    RELEASE_BASSERT(!Environment::get()->isDebugHeapEnabled());

    size_t result = 0;
    for (unsigned i = numHeaps; i--;) {
        if (!isActiveHeapKind(static_cast<HeapKind>(i)))
            continue;
        result += PerProcess<PerHeapKind<Heap>>::get()->at(i).footprint();
    }

    AllIsoHeaps::get()->forEach(
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
#if BOS(DARWIN)
    setThreadName("JavaScriptCore bmalloc scavenger");
#else
    setThreadName("BMScavenger");
#endif
    
    // This loop ratchets downward from most active to least active state. While
    // we ratchet downward, any other thread may reset our state.
    
    // We require any state change while we are sleeping to signal to our
    // condition variable and wake us up.
    
    while (true) {
        if (m_state == State::Sleep) {
            UniqueLockHolder lock(mutex());
            m_condition.wait(lock, [&]() { return m_state != State::Sleep; });
        }
        
        if (m_state == State::RunSoon) {
            UniqueLockHolder lock(mutex());
            m_condition.wait_for(lock, m_waitTime, [&]() { return m_state != State::RunSoon; });
        }
        
        m_state = State::Sleep;
        
        setSelfQOSClass();
        
        if (verbose) {
            fprintf(stderr, "--------------------------------\n");
            fprintf(stderr, "considering running scavenger\n");
            dumpStats();
            fprintf(stderr, "--------------------------------\n");
        }

#if BUSE(PARTIAL_SCAVENGE)
        enum class ScavengeMode {
            None,
            Partial,
            Full
        };

        size_t freeableMemory = this->freeableMemory();

        ScavengeMode scavengeMode = [&] {
            auto timeSinceLastFullScavenge = this->timeSinceLastFullScavenge();
            auto timeSinceLastPartialScavenge = this->timeSinceLastPartialScavenge();
            auto timeSinceLastScavenge = std::min(timeSinceLastPartialScavenge, timeSinceLastFullScavenge);

            if (isUnderMemoryPressure() && freeableMemory > 1 * MB && timeSinceLastScavenge > std::chrono::milliseconds(5))
                return ScavengeMode::Full;

            if (!m_isProbablyGrowing) {
                if (timeSinceLastFullScavenge < std::chrono::milliseconds(1000) && !m_isInMiniMode)
                    return ScavengeMode::Partial;
                return ScavengeMode::Full;
            }

            if (m_isInMiniMode) {
                if (timeSinceLastFullScavenge < std::chrono::milliseconds(200))
                    return ScavengeMode::Partial;
                return ScavengeMode::Full;
            }

#if BCPU(X86_64)
            auto partialScavengeInterval = std::chrono::milliseconds(12000);
#else
            auto partialScavengeInterval = std::chrono::milliseconds(8000);
#endif
            if (timeSinceLastScavenge < partialScavengeInterval) {
                // Rate limit partial scavenges.
                return ScavengeMode::None;
            }
            if (freeableMemory < 25 * MB)
                return ScavengeMode::None;
            if (5 * freeableMemory < footprint())
                return ScavengeMode::None;
            return ScavengeMode::Partial;
        }();

        m_isProbablyGrowing = false;

        switch (scavengeMode) {
        case ScavengeMode::None: {
            runSoon();
            break;
        }
        case ScavengeMode::Partial: {
            partialScavenge();
            runSoon();
            break;
        }
        case ScavengeMode::Full: {
            scavenge();
            break;
        }
        }
#else
        std::chrono::steady_clock::time_point start { std::chrono::steady_clock::now() };
        
        scavenge();

        auto timeSpentScavenging = std::chrono::steady_clock::now() - start;

        if (verbose) {
            fprintf(stderr, "time spent scavenging %lfms\n",
                static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(timeSpentScavenging).count()) / 1000);
        }

        // FIXME: We need to investigate mini-mode's adjustment.
        // https://bugs.webkit.org/show_bug.cgi?id=203987
        if (!m_isInMiniMode) {
            timeSpentScavenging *= 150;
            std::chrono::milliseconds newWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(timeSpentScavenging);
            m_waitTime = std::min(std::max(newWaitTime, std::chrono::milliseconds(100)), std::chrono::milliseconds(10000));
        }

        if (verbose)
            fprintf(stderr, "new wait time %lldms\n", static_cast<long long int>(m_waitTime.count()));
#endif
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

