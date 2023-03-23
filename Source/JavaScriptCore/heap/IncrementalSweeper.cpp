/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "MarkedBlock.h"
#include "VM.h"
#include "BlockDirectoryInlines.h"

namespace JSC {

static constexpr Seconds sweepTimeSlice = 10_ms;
static constexpr double sweepTimeTotal = .10;
static constexpr double sweepTimeMultiplier = 1.0 / sweepTimeTotal;

void IncrementalSweeper::scheduleTimer()
{
    setTimeUntilFire(sweepTimeSlice * sweepTimeMultiplier);
}

IncrementalSweeper::IncrementalSweeper(Heap* heap)
    : Base(heap->vm())
    , m_currentDirectory(nullptr)
    , m_currentAllocator(nullptr)
    , doneSweeping(false)
{
}

void IncrementalSweeper::doWork(VM& vm)
{
    doSweep(vm, MonotonicTime::now());
}

void IncrementalSweeper::doSweep(VM& vm, MonotonicTime sweepBeginTime)
{
    if (!doneSweeping) {
        while (sweepNextBlock(vm)) {
            Seconds elapsedTime = MonotonicTime::now() - sweepBeginTime;
            if (elapsedTime < sweepTimeSlice)
                continue;

            scheduleTimer();
            return;
        }

        if (m_shouldFreeFastMallocMemoryAfterSweeping) {
            WTF::releaseFastMallocFreeMemory();
            m_shouldFreeFastMallocMemoryAfterSweeping = false;
        }

        doneSweeping = true;
        m_currentDirectory = vm.heap.objectSpace().firstDirectory();
    }

    while (constructNextSpeculativeFreeList(vm)) {
        Seconds elapsedTime = MonotonicTime::now() - sweepBeginTime;
        if (elapsedTime < sweepTimeSlice)
            continue;

        scheduleTimer();
        return;
    }
}

bool IncrementalSweeper::constructNextSpeculativeFreeList(VM& vm)
{
    vm.heap.stopIfNecessary();

    // MarkedBlock::Handle* block = nullptr;

    int iterations = 0;
    DeferGCForAWhile deferGC(vm);
    
    for (; m_currentDirectory; m_currentDirectory = m_currentDirectory->nextDirectory()) {
        if (iterations)
            break;
        // auto allocator = m_currentAllocator ? *m_currentAllocator : m_currentDirectory->m_localAllocators.begin();

        // for (; allocator != m_currentDirectory->m_localAllocators.end(); ++allocator) {
        //     block = m_currentDirectory->findBlockForAllocation(*allocator, true);
        //     if (block)
        //         break;
            
        //     ++iterations;
        //     if (iterations > 1000) {
        //         m_currentAllocator = { allocator };
        //         return true;
        //     }
        // }

        // m_currentAllocator = { allocator };
        // if (block)
        //     break;

        // m_currentAllocator = { };
        m_currentDirectory->forEachBlock([&] (MarkedBlock::Handle* block) {
            ++iterations;
            block->sweepSpeculativelyToFreeList();
        });
    }
    
    if (!iterations) {
        // dataLogLn("no block to sweep");
        return false;
    }
    
    return true;
}

bool IncrementalSweeper::sweepNextBlock(VM& vm)
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
        vm.heap.objectSpace().freeOrShrinkBlock(block);
        return true;
    }

    return vm.heap.sweepNextLogicallyEmptyWeakBlock();
}

void IncrementalSweeper::startSweeping(Heap& heap)
{
    scheduleTimer();
    doneSweeping = false;
    m_currentDirectory = heap.objectSpace().firstDirectory();
    m_currentAllocator = { };
}

void IncrementalSweeper::stopSweeping()
{
    m_currentDirectory = nullptr;
    m_currentAllocator = { };
    cancelTimer();
}

} // namespace JSC
