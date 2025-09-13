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

void IncrementalSweeper::doWorkUntil(VM& vm, MonotonicTime deadline)
{
    m_hasWork = true; // assume true since we might reset to first directory and we'll clear this anyway if we're wrong
    if (!m_currentDirectory)
        m_currentDirectory = vm.heap.objectSpace().firstDirectory();

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
    doSweep(vm, MonotonicTime::now() + sweepTimeSlice, SweepTrigger::Timer);
}

void IncrementalSweeper::doSweep(VM& vm, MonotonicTime deadline, SweepTrigger trigger)
{
    std::optional<TraceScope> traceScope;
    if (Options::useTracePoints()) [[unlikely]]
        traceScope.emplace(IncrementalSweepStart, IncrementalSweepEnd, vm.heap.size(), vm.heap.capacity());

    while (sweepNextBlock(vm, trigger)) {
        if (MonotonicTime::now() < deadline)
            continue;

        if (trigger == SweepTrigger::Timer)
            scheduleTimer();
        else
            m_lastOpportunisticTaskDidFinishSweeping = false;
        return;
    }
    if (trigger == SweepTrigger::OpportunisticTask)
        m_lastOpportunisticTaskDidFinishSweeping = true;

    cancelTimer();
    m_hasWork = false;
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
    m_hasWork = false;
}

bool IncrementalSweeper::hasWork()
{
    return m_hasWork;
}

} // namespace JSC
