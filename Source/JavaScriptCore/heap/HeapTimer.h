#ifndef HeapTimer_h
#define HeapTimer_h

#include <wtf/RetainPtr.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class JSGlobalData;
    
class HeapTimer {
public:
#if USE(CF)
    HeapTimer(JSGlobalData*, CFRunLoopRef);
    static void timerDidFire(CFRunLoopTimerRef, void*);
#else
    HeapTimer(JSGlobalData*);
#endif
    
    virtual ~HeapTimer();
    
    virtual void synchronize();
    virtual void doWork() = 0;
    
protected:
    JSGlobalData* m_globalData;

#if USE(CF)
    static const CFTimeInterval s_decade;

    RetainPtr<CFRunLoopTimerRef> m_timer;
    RetainPtr<CFRunLoopRef> m_runLoop;
    CFRunLoopTimerContext m_context;
#endif
    
private:
    void invalidate();
};

} // namespace JSC

#endif
