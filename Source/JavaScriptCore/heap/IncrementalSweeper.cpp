/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "IncrementalSweeper.h"

#include "DeferGCInlines.h"
#include "HeapInlines.h"
#include "MarkedBlockInlines.h"
#include "Watchdog.h" // mlam TEST
#include <wtf/SystemTracing.h>

namespace JSC {

static constexpr Seconds sweepTimeSlice = 10_ms;
static constexpr double sweepTimeTotal = .10;
static constexpr double sweepTimeMultiplier = 1.0 / sweepTimeTotal;

void IncrementalSweeper::scheduleTimer()
{
    setTimeUntilFire(sweepTimeSlice * sweepTimeMultiplier);
}

IncrementalSweeper::IncrementalSweeper(JSC::Heap* heap)
    : Base(heap->vm())
    , m_currentDirectory(nullptr)
{
}

void IncrementalSweeper::startSweepingForOpportunisticTask(VM& vm)
{
    if (!m_currentDirectory)
        m_currentDirectory = vm.heap.objectSpace().firstDirectory();
}

void IncrementalSweeper::doWorkForOpportunisticTask(VM& vm, UnbarrieredMonotonicTime deadline)
{
    if (m_currentDirectory)
        doSweep(vm, deadline, SweepTrigger::OpportunisticTask);
}

void IncrementalSweeper::doWork(VM& vm)
{
    if (m_lastOpportunisticTaskDidFinishSweeping) {
        m_lastOpportunisticTaskDidFinishSweeping = false;
        scheduleTimer();
        return;
    }
    doSweep(vm, UnbarrieredMonotonicTime::now() + sweepTimeSlice, SweepTrigger::Timer);
}

#if MLAM_VERBOSE
static constexpr bool mlamTraceSweep = false;
#endif

void IncrementalSweeper::doSweep(VM& vm, UnbarrieredMonotonicTime deadline, SweepTrigger trigger)
{
    std::optional<TraceScope> traceScope;
    if (Options::useTracePoints()) [[unlikely]]
        traceScope.emplace(IncrementalSweepStart, IncrementalSweepEnd, vm.heap.size(), vm.heap.capacity());

#if MLAM_VERBOSE
    UnbarrieredMonotonicTime startTime; // mlam
    UnbarrieredMonotonicTime endTime; // mlam
    unsigned count = 0;

    if constexpr (mlamTraceSweep)
        startTime = UnbarrieredMonotonicTime::now(); // mlam
#endif
    while (sweepNextBlock(vm, trigger)) {
#if MLAM_VERBOSE
        count++;
        if constexpr (mlamTraceSweep)
            endTime = UnbarrieredMonotonicTime::now(); // mlam
#endif
        if (UnbarrieredMonotonicTime::now() < deadline) {
#if MLAM_VERBOSE
            if constexpr (mlamTraceSweep) {
                dataLogLn("    mlam IncrementalSweeper::doSweep[", count, "]: sweepNextBlock time ", (endTime - startTime).nanosecondsAs<long>(), " ns | remaining ", (deadline - endTime).nanosecondsAs<long>(), " ns");
                startTime = UnbarrieredMonotonicTime::now(); // mlam
            }
#endif
            continue;
        }

        if (trigger == SweepTrigger::Timer)
            scheduleTimer();
        else {
            m_lastOpportunisticTaskDidFinishSweeping = false;
#if MLAM_VERBOSE
            if constexpr (mlamTraceSweep) {
                dataLogLn("    mlam IncrementalSweeper::doSweep[", count, "]: OUT OF TIME time ", (endTime - startTime).nanosecondsAs<long>(), " ns | remaining ", (deadline - endTime).nanosecondsAs<long>(), " ns");
            }
#endif
        }
        return;
    }
#if MLAM_VERBOSE
    if constexpr (mlamTraceSweep) {
        endTime = UnbarrieredMonotonicTime::now(); // mlam
        dataLogLn("    mlam IncrementalSweeper::doSweep[", count, "]: FINISHED time ", (endTime - startTime).nanosecondsAs<long>(), " ns | remaining ", (deadline - endTime).nanosecondsAs<long>(), " ns");
    }
#endif
    if (trigger == SweepTrigger::OpportunisticTask)
        m_lastOpportunisticTaskDidFinishSweeping = true;

    cancelTimer();
}

bool IncrementalSweeper::sweepNextBlock(VM& vm, SweepTrigger trigger)
{
    vm.heap.stopIfNecessary();

    MarkedBlock::Handle* block = nullptr;
    
    for (; m_currentDirectory; m_currentDirectory = m_currentDirectory->nextDirectory()) {
        block = m_currentDirectory->findBlockToSweep();
        if (block)
            break;
    }
    
    if (block) {
        DeferGCForAWhile deferGC(vm);
        block->sweep(nullptr);

        bool blockIsFreed = false;
        if (trigger == SweepTrigger::Timer) {
            if (!block->isEmpty())
                block->shrink();
            else {
                vm.heap.objectSpace().freeBlock(block);
                blockIsFreed = true;
            }
        }

        if (!blockIsFreed)
            m_currentDirectory->didFinishUsingBlock(block);
        return true;
    }

    return vm.heap.sweepNextLogicallyEmptyWeakBlock();
}

void IncrementalSweeper::startSweeping(JSC::Heap& heap)
{
    scheduleTimer();
    m_currentDirectory = heap.objectSpace().firstDirectory();
}

void IncrementalSweeper::stopSweeping()
{
    m_currentDirectory = nullptr;
    cancelTimer();
}

} // namespace JSC
