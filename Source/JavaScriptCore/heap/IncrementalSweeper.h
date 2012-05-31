#ifndef IncrementalSweeper_h
#define IncrementalSweeper_h

#include "MarkedBlock.h"
#include <wtf/HashSet.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class Heap;
    
class IncrementalSweeper {
public:
    ~IncrementalSweeper();

    static PassOwnPtr<IncrementalSweeper> create(Heap*);
    void startSweeping(const HashSet<MarkedBlock*>& blockSnapshot);

private:
#if USE(CF)
    IncrementalSweeper(Heap*, CFRunLoopRef);
    
    static void timerDidFire(CFRunLoopTimerRef, void*);
    void doSweep(double startTime);
    void scheduleTimer();
    void cancelTimer();
    
    Heap* m_heap;
    unsigned m_currentBlockToSweepIndex;
    RetainPtr<CFRunLoopTimerRef> m_timer;
    RetainPtr<CFRunLoopRef> m_runLoop;
    CFRunLoopTimerContext m_context;
   
    double m_lengthOfLastSweepIncrement; 
    Vector<MarkedBlock*> m_blocksToSweep;
#else
    
    IncrementalSweeper();
    
#endif
};

} // namespace JSC

#endif
