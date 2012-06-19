#ifndef IncrementalSweeper_h
#define IncrementalSweeper_h

#include "HeapTimer.h"
#include "MarkedBlock.h"
#include <wtf/HashSet.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace JSC {

class Heap;
    
class IncrementalSweeper : public HeapTimer {
public:
    static PassOwnPtr<IncrementalSweeper> create(Heap*);
    void startSweeping(const HashSet<MarkedBlock*>& blockSnapshot);
    virtual void doWork();

private:
#if USE(CF)
    IncrementalSweeper(Heap*, CFRunLoopRef);
    
    void doSweep(double startTime);
    void scheduleTimer();
    void cancelTimer();
    
    unsigned m_currentBlockToSweepIndex;
    double m_lengthOfLastSweepIncrement; 
    Vector<MarkedBlock*> m_blocksToSweep;
#else
    
    IncrementalSweeper(JSGlobalData*);
    
#endif
};

} // namespace JSC

#endif
