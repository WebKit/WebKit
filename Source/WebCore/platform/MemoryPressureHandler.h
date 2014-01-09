/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#ifndef MemoryPressureHandler_h
#define MemoryPressureHandler_h

#include <time.h>
#include <wtf/FastMalloc.h>

#if PLATFORM(IOS)
#include <wtf/ThreadingPrimitives.h>
#endif

namespace WebCore {

#if PLATFORM(IOS)
enum MemoryPressureReason {
    MemoryPressureReasonNone = 0 << 0,
    MemoryPressureReasonVMPressure = 1 << 0,
    MemoryPressureReasonVMStatus = 1 << 1,
};
#endif

typedef void (*LowMemoryHandler)(bool critical);

class MemoryPressureHandler {
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend MemoryPressureHandler& memoryPressureHandler();

    void install();

    void setLowMemoryHandler(LowMemoryHandler handler)
    {
        ASSERT(!m_installed);
        m_lowMemoryHandler = handler;
    }

#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    void installMemoryReleaseBlock(void (^releaseMemoryBlock)(), bool clearPressureOnMemoryRelease = true);
    void setReceivedMemoryPressure(MemoryPressureReason);
    bool hasReceivedMemoryPressure();
    void clearMemoryPressure();
    bool shouldWaitForMemoryClearMessage();
    void respondToMemoryPressureIfNeeded();
#endif

private:
    void uninstall();

    void holdOff(unsigned);

    MemoryPressureHandler();
    ~MemoryPressureHandler();

    void respondToMemoryPressure();
    static void releaseMemory(bool critical);

    bool m_installed;
    time_t m_lastRespondTime;
    LowMemoryHandler m_lowMemoryHandler;

#if PLATFORM(IOS)
    uint32_t m_receivedMemoryPressure;
    uint32_t m_memoryPressureReason;
    bool m_clearPressureOnMemoryRelease;
    void (^m_releaseMemoryBlock)();
    CFRunLoopObserverRef m_observer;
    Mutex m_observerMutex;
#endif
};
 
// Function to obtain the global memory pressure object.
MemoryPressureHandler& memoryPressureHandler();

} // namespace WebCore

#endif // MemoryPressureHandler_h
