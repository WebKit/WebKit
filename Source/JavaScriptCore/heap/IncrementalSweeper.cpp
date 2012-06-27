/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "APIShims.h"
#include "Heap.h"
#include "JSObject.h"
#include "JSString.h"
#include "MarkedBlock.h"
#include "ScopeChain.h"
#include <wtf/HashSet.h>
#include <wtf/WTFThreadData.h>

namespace JSC {

#if USE(CF)

static const CFTimeInterval sweepTimeSlice = .01; // seconds
static const CFTimeInterval sweepTimeTotal = .10;
static const CFTimeInterval sweepTimeMultiplier = 1.0 / sweepTimeTotal;

void IncrementalSweeper::doWork()
{
    doSweep(WTF::monotonicallyIncreasingTime());
}
    
IncrementalSweeper::IncrementalSweeper(Heap* heap, CFRunLoopRef runLoop)
    : HeapTimer(heap->globalData(), runLoop)
    , m_currentBlockToSweepIndex(0)
{
}

IncrementalSweeper* IncrementalSweeper::create(Heap* heap)
{
    return new IncrementalSweeper(heap, CFRunLoopGetCurrent());
}

void IncrementalSweeper::scheduleTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + (sweepTimeSlice * sweepTimeMultiplier));
}

void IncrementalSweeper::cancelTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade);
}

void IncrementalSweeper::doSweep(double sweepBeginTime)
{
    while (m_currentBlockToSweepIndex < m_blocksToSweep.size()) {
        MarkedBlock* block = m_blocksToSweep[m_currentBlockToSweepIndex++];
        if (!block->needsSweeping())
            continue;

        block->sweep();
        m_globalData->heap.objectSpace().freeOrShrinkBlock(block);

        CFTimeInterval elapsedTime = WTF::monotonicallyIncreasingTime() - sweepBeginTime;
        if (elapsedTime < sweepTimeSlice)
            continue;

        scheduleTimer();
        return;
    }

    m_blocksToSweep.clear();
    cancelTimer();
}

void IncrementalSweeper::startSweeping(const HashSet<MarkedBlock*>& blockSnapshot)
{
    WTF::copyToVector(blockSnapshot, m_blocksToSweep);
    m_currentBlockToSweepIndex = 0;
    scheduleTimer();
}

#else

IncrementalSweeper::IncrementalSweeper(JSGlobalData* globalData)
    : HeapTimer(globalData)
{
}

void IncrementalSweeper::doWork()
{
}

IncrementalSweeper* IncrementalSweeper::create(Heap* heap)
{
    return new IncrementalSweeper(heap->globalData());
}

void IncrementalSweeper::startSweeping(const HashSet<MarkedBlock*>&)
{
}
    
#endif

} // namespace JSC
