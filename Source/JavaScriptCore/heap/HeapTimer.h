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

#ifndef HeapTimer_h
#define HeapTimer_h

#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#elif PLATFORM(EFL)
#if USE(EO)
typedef struct _Eo_Opaque Ecore_Timer;
#else
typedef struct _Ecore_Timer Ecore_Timer;
#endif
#endif

namespace JSC {

class VM;

class HeapTimer {
public:
#if USE(CF)
    HeapTimer(VM*, CFRunLoopRef);
    static void timerDidFire(CFRunLoopTimerRef, void*);
#else
    HeapTimer(VM*);
#endif
    
    JS_EXPORT_PRIVATE virtual ~HeapTimer();
    virtual void doWork() = 0;
    
protected:
    VM* m_vm;

#if USE(CF)
    static const CFTimeInterval s_decade;

    RetainPtr<CFRunLoopTimerRef> m_timer;
    RetainPtr<CFRunLoopRef> m_runLoop;
    CFRunLoopTimerContext m_context;

    Mutex m_shutdownMutex;
#elif PLATFORM(EFL)
    static bool timerEvent(void*);
    Ecore_Timer* add(double delay, void* agent);
    void stop();
    Ecore_Timer* m_timer;
#endif
    
private:
    void invalidate();
};

} // namespace JSC

#endif
