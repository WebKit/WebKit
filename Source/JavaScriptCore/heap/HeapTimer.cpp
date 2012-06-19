#include "config.h"
#include "HeapTimer.h"

#include <wtf/Threading.h>

namespace JSC {

#if USE(CF)
    
const CFTimeInterval HeapTimer::s_decade = 60 * 60 * 24 * 365 * 10;

HeapTimer::HeapTimer(JSGlobalData* globalData, CFRunLoopRef runLoop)
    : m_globalData(globalData)
    , m_runLoop(runLoop)
{
    memset(&m_context, 0, sizeof(CFRunLoopTimerContext));
    m_context.info = this;
    m_timer.adoptCF(CFRunLoopTimerCreate(0, s_decade, s_decade, 0, 0, HeapTimer::timerDidFire, &m_context));
    CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

HeapTimer::~HeapTimer()
{
    invalidate();
}

void HeapTimer::synchronize()
{
    if (CFRunLoopGetCurrent() == m_runLoop.get())
        return;
    CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    m_runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
}

void HeapTimer::invalidate()
{
    CFRunLoopRemoveTimer(m_runLoop.get(), m_timer.get(), kCFRunLoopCommonModes);
    CFRunLoopTimerInvalidate(m_timer.get());
}

void HeapTimer::timerDidFire(CFRunLoopTimerRef, void* info)
{
    HeapTimer* agent = static_cast<HeapTimer*>(info);
    agent->doWork();
}

#else
    
HeapTimer::HeapTimer(JSGlobalData* globalData)
    : m_globalData(globalData)
{
}

HeapTimer::~HeapTimer()
{
}

void HeapTimer::synchronize()
{
}

void HeapTimer::invalidate()
{
}


#endif
    

} // namespace JSC
