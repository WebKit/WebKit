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

static const CFTimeInterval decade = 60 * 60 * 24 * 365 * 10;
static const CFTimeInterval sweepTimeSlicePerBlock = 0.01;
static const CFTimeInterval sweepTimeMultiplier = 1.0 / sweepTimeSlicePerBlock;
 
void IncrementalSweeper::timerDidFire(CFRunLoopTimerRef, void* info)
{
    Heap* heap = static_cast<Heap*>(info);
    APIEntryShim shim(heap->globalData());
    heap->sweeper()->doSweep(WTF::monotonicallyIncreasingTime());
}
    
IncrementalSweeper::IncrementalSweeper(Heap* heap, CFRunLoopRef runLoop)
    : m_heap(heap)
    , m_currentBlockToSweepIndex(0)
    , m_lengthOfLastSweepIncrement(0.0)
{
    memset(&m_context, 0, sizeof(CFRunLoopTimerContext));
    m_context.info = m_heap;
    m_runLoop = runLoop;
    m_timer.adoptCF(CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent(), decade, 0, 0, &timerDidFire, &m_context));
    CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

IncrementalSweeper::~IncrementalSweeper()
{
    CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    CFRunLoopTimerInvalidate(m_timer.get());
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
    CFRunLoopTimerSetNextFireDate(m_timer.get(), CFAbsoluteTimeGetCurrent() + decade);
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

IncrementalSweeper::IncrementalSweeper()
{
}

IncrementalSweeper::~IncrementalSweeper()
{
}

PassOwnPtr<IncrementalSweeper> IncrementalSweeper::create(Heap*)
{
    return adoptPtr(new IncrementalSweeper());
}

void IncrementalSweeper::startSweeping(const HashSet<MarkedBlock*>&)
{
}
    
#endif

} // namespace JSC
