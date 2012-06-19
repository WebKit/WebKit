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

static const CFTimeInterval sweepTimeSlicePerBlock = 0.01;
static const CFTimeInterval sweepTimeMultiplier = 1.0 / sweepTimeSlicePerBlock;
 
void IncrementalSweeper::doWork()
{
    APIEntryShim shim(m_globalData);
    doSweep(WTF::monotonicallyIncreasingTime());
}
    
IncrementalSweeper::IncrementalSweeper(Heap* heap, CFRunLoopRef runLoop)
    : HeapTimer(heap->globalData(), runLoop)
    , m_currentBlockToSweepIndex(0)
    , m_lengthOfLastSweepIncrement(0.0)
{
}

PassOwnPtr<IncrementalSweeper> IncrementalSweeper::create(Heap* heap)
{
    return adoptPtr(new IncrementalSweeper(heap, CFRunLoopGetCurrent()));
}

void IncrementalSweeper::scheduleTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + (m_lengthOfLastSweepIncrement * sweepTimeMultiplier));
}

void IncrementalSweeper::cancelTimer()
{
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + s_decade);
}

void IncrementalSweeper::doSweep(double sweepBeginTime)
{
    for (; m_currentBlockToSweepIndex < m_blocksToSweep.size(); m_currentBlockToSweepIndex++) {
        MarkedBlock* nextBlock = m_blocksToSweep[m_currentBlockToSweepIndex];
        if (!nextBlock->needsSweeping())
            continue;

        nextBlock->sweep();
        m_blocksToSweep[m_currentBlockToSweepIndex++] = 0;
        m_lengthOfLastSweepIncrement = WTF::monotonicallyIncreasingTime() - sweepBeginTime;
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
