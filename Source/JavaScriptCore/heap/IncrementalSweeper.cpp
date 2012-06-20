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
    APIEntryShim shim(m_globalData);
    doSweep(WTF::monotonicallyIncreasingTime());
}
    
IncrementalSweeper::IncrementalSweeper(Heap* heap, CFRunLoopRef runLoop)
    : HeapTimer(heap->globalData(), runLoop)
    , m_currentBlockToSweepIndex(0)
{
}

PassOwnPtr<IncrementalSweeper> IncrementalSweeper::create(Heap* heap)
{
    return adoptPtr(new IncrementalSweeper(heap, CFRunLoopGetCurrent()));
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

PassOwnPtr<IncrementalSweeper> IncrementalSweeper::create(Heap* heap)
{
    return adoptPtr(new IncrementalSweeper(heap->globalData()));
}

void IncrementalSweeper::startSweeping(const HashSet<MarkedBlock*>&)
{
}
    
#endif

} // namespace JSC
