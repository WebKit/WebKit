/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2014 Raspberry Pi Foundation. All Rights Reserved.
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

#include <atomic>
#include <ctime>
#include <functional>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>

#if PLATFORM(IOS)
#include <wtf/Lock.h>
#include <wtf/ThreadingPrimitives.h>
#elif OS(LINUX)
#include "Timer.h"
#endif

namespace WebCore {

#if PLATFORM(IOS)
enum MemoryPressureReason {
    MemoryPressureReasonNone = 0 << 0,
    MemoryPressureReasonVMPressure = 1 << 0,
    MemoryPressureReasonVMStatus = 1 << 1,
};
#endif

enum class Critical { No, Yes };
enum class Synchronous { No, Yes };

typedef std::function<void(Critical, Synchronous)> LowMemoryHandler;

class MemoryPressureHandler {
    WTF_MAKE_FAST_ALLOCATED;
    friend class WTF::NeverDestroyed<MemoryPressureHandler>;
public:
    WEBCORE_EXPORT static MemoryPressureHandler& singleton();

    WEBCORE_EXPORT void install();

    void setLowMemoryHandler(LowMemoryHandler handler)
    {
        ASSERT(!m_installed);
        m_lowMemoryHandler = handler;
    }

    bool isUnderMemoryPressure() const { return m_underMemoryPressure; }
    void setUnderMemoryPressure(bool b) { m_underMemoryPressure = b; }

#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    WEBCORE_EXPORT void installMemoryReleaseBlock(void (^releaseMemoryBlock)(), bool clearPressureOnMemoryRelease = true);
    WEBCORE_EXPORT void setReceivedMemoryPressure(MemoryPressureReason);
    WEBCORE_EXPORT void clearMemoryPressure();
    WEBCORE_EXPORT bool shouldWaitForMemoryClearMessage();
    void respondToMemoryPressureIfNeeded();
#elif OS(LINUX)
    static void waitForMemoryPressureEvent(void*);
#endif

    class ReliefLogger {
    public:
        explicit ReliefLogger(const char *log)
            : m_logString(log)
#if !LOG_ALWAYS_DISABLED
            , m_initialMemory(platformMemoryUsage())
#else
            , m_initialMemory(s_loggingEnabled ? platformMemoryUsage() : 0)
#endif
        {
        }

        ~ReliefLogger()
        {
#if !LOG_ALWAYS_DISABLED
            logMemoryUsageChange();
#else
            if (s_loggingEnabled)
                logMemoryUsageChange();
#endif
        }

        const char* logString() const { return m_logString; }
        static void setLoggingEnabled(bool enabled) { s_loggingEnabled = enabled; }
        static bool loggingEnabled() { return s_loggingEnabled; }

    private:
        size_t platformMemoryUsage();
        void logMemoryUsageChange();

        const char* m_logString;
        size_t m_initialMemory;

        WEBCORE_EXPORT static bool s_loggingEnabled;
    };

    WEBCORE_EXPORT void releaseMemory(Critical, Synchronous = Synchronous::No);

private:
    void releaseNoncriticalMemory();
    void releaseCriticalMemory(Synchronous);

    void uninstall();

    void holdOff(unsigned);

    MemoryPressureHandler();
    ~MemoryPressureHandler() = delete;

    void respondToMemoryPressure(Critical, Synchronous = Synchronous::No);
    void platformReleaseMemory(Critical);

    bool m_installed;
    time_t m_lastRespondTime;
    LowMemoryHandler m_lowMemoryHandler;

    std::atomic<bool> m_underMemoryPressure;

#if PLATFORM(IOS)
    uint32_t m_memoryPressureReason;
    bool m_clearPressureOnMemoryRelease;
    void (^m_releaseMemoryBlock)();
    CFRunLoopObserverRef m_observer;
    Lock m_observerMutex;
#elif OS(LINUX)
    int m_eventFD;
    int m_pressureLevelFD;
    WTF::ThreadIdentifier m_threadID;
    Timer m_holdOffTimer;
    void holdOffTimerFired();
    void logErrorAndCloseFDs(const char* error);
#endif
};

} // namespace WebCore

#endif // MemoryPressureHandler_h
