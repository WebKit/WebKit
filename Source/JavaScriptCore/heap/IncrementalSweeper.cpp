/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(EFL)
#include <Ecore.h>
#include <wtf/CurrentTime.h>
#elif USE(GLIB)
#include <glib.h>
#endif

namespace JSC {

#if USE(CF) || PLATFORM(EFL) || USE(GLIB)

static const double sweepTimeSlice = .01; // seconds
static const double sweepTimeTotal = .10;
static const double sweepTimeMultiplier = 1.0 / sweepTimeTotal;

#if USE(CF)
IncrementalSweeper::IncrementalSweeper(Heap* heap, CFRunLoopRef runLoop)
    : HeapTimer(heap->vm(), runLoop)
    , m_blocksToSweep(heap->m_blockSnapshot)
{
}

void IncrementalSweeper::scheduleTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + (sweepTimeSlice * sweepTimeMultiplier));
}

void IncrementalSweeper::cancelTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade);
}
#elif PLATFORM(EFL)
IncrementalSweeper::IncrementalSweeper(Heap* heap)
    : HeapTimer(heap->vm())
    , m_blocksToSweep(heap->m_blockSnapshot)
{
}

void IncrementalSweeper::scheduleTimer()
{
    if (ecore_timer_freeze_get(m_timer))
        ecore_timer_thaw(m_timer);

    double targetTime = currentTime() + (sweepTimeSlice * sweepTimeMultiplier);
    ecore_timer_interval_set(m_timer, targetTime);
}

void IncrementalSweeper::cancelTimer()
{
    ecore_timer_freeze(m_timer);
}
#elif USE(GLIB)
IncrementalSweeper::IncrementalSweeper(Heap* heap)
    : HeapTimer(heap->vm())
    , m_blocksToSweep(heap->m_blockSnapshot)
{
}

void IncrementalSweeper::scheduleTimer()
{
    auto delayDuration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(sweepTimeSlice * sweepTimeMultiplier));
    gint64 currentTime = g_get_monotonic_time();
    gint64 targetTime = currentTime + std::min<gint64>(G_MAXINT64 - currentTime, delayDuration.count());
    ASSERT(targetTime >= currentTime);
    g_source_set_ready_time(m_timer.get(), targetTime);
}

void IncrementalSweeper::cancelTimer()
{
    g_source_set_ready_time(m_timer.get(), -1);
}
#endif

void IncrementalSweeper::doWork()
{
    doSweep(WTF::monotonicallyIncreasingTime());
}

void IncrementalSweeper::doSweep(double sweepBeginTime)
{
    while (sweepNextBlock()) {
        double elapsedTime = WTF::monotonicallyIncreasingTime() - sweepBeginTime;
        if (elapsedTime < sweepTimeSlice)
            continue;

        scheduleTimer();
        return;
    }

    m_blocksToSweep.clear();
    cancelTimer();
}

bool IncrementalSweeper::sweepNextBlock()
{
    while (!m_blocksToSweep.isEmpty()) {
        MarkedBlock::Handle* block = m_blocksToSweep.takeLast();
        block->setIsOnBlocksToSweep(false);

        if (!block->needsSweeping())
            continue;

        DeferGCForAWhile deferGC(m_vm->heap);
        block->sweep();
        m_vm->heap.objectSpace().freeOrShrinkBlock(block);
        return true;
    }

    return m_vm->heap.sweepNextLogicallyEmptyWeakBlock();
}

void IncrementalSweeper::startSweeping()
{
    scheduleTimer();
}

void IncrementalSweeper::willFinishSweeping()
{
    m_blocksToSweep.clear();
    if (m_vm)
        cancelTimer();
}

#else

IncrementalSweeper::IncrementalSweeper(Heap* heap)
    : HeapTimer(heap->vm())
{
}

void IncrementalSweeper::doWork()
{
}

void IncrementalSweeper::startSweeping()
{
}

void IncrementalSweeper::willFinishSweeping()
{
}

bool IncrementalSweeper::sweepNextBlock()
{
    return false;
}

#endif

} // namespace JSC
