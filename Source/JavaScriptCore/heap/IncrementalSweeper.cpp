/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "Heap.h"
#include "JSObject.h"
#include "JSString.h"
#include "MarkedBlock.h"
#include "JSCInlines.h"

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
{
}

void IncrementalSweeper::doWork(VM& vm)
{
    doSweep(vm, MonotonicTime::now());
}

void IncrementalSweeper::doSweep(VM& vm, MonotonicTime sweepBeginTime)
{
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
    cancelTimer();
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
        DeferGCForAWhile deferGC(vm.heap);
        block->sweep(nullptr);
        vm.heap.objectSpace().freeOrShrinkBlock(block);
        return true;
    }

    return vm.heap.sweepNextLogicallyEmptyWeakBlock();
}

void IncrementalSweeper::startSweeping(Heap& heap)
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
