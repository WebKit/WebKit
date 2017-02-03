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
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/RunLoop.h>

#if PLATFORM(IOS)
#include <wtf/Lock.h>
#include <wtf/ThreadingPrimitives.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

#if PLATFORM(IOS)
enum MemoryPressureReason {
    MemoryPressureReasonNone = 0 << 0,
    MemoryPressureReasonVMPressure = 1 << 0,
    MemoryPressureReasonVMStatus = 1 << 1,
};
#endif

enum class MemoryUsagePolicy {
    Unrestricted, // Allocate as much as you want
    Conservative, // Maybe you don't cache every single thing
    Strict, // Time to start pinching pennies for real
    Panic, // OH GOD WE'RE SINKING, THROW EVERYTHING OVERBOARD
};

enum class Critical { No, Yes };
enum class Synchronous { No, Yes };

typedef std::function<void(Critical, Synchronous)> LowMemoryHandler;

class MemoryPressureHandler {
    WTF_MAKE_FAST_ALLOCATED;
    friend class WTF::NeverDestroyed<MemoryPressureHandler>;
public:
    WEBCORE_EXPORT static MemoryPressureHandler& singleton();

    WEBCORE_EXPORT void install();

    WEBCORE_EXPORT void setShouldUsePeriodicMemoryMonitor(bool);

    void setMemoryKillCallback(WTF::Function<void()> function) { m_memoryKillCallback = WTFMove(function); }
    void setProcessIsEligibleForMemoryKillCallback(WTF::Function<bool()> function) { m_processIsEligibleForMemoryKillCallback = WTFMove(function); }

    void setLowMemoryHandler(LowMemoryHandler&& handler)
    {
        m_lowMemoryHandler = WTFMove(handler);
    }

    bool isUnderMemoryPressure() const
    {
        return m_underMemoryPressure
#if PLATFORM(MAC)
            || m_memoryUsagePolicy >= MemoryUsagePolicy::Strict
#endif
            || m_isSimulatingMemoryPressure;
    }
    void setUnderMemoryPressure(bool b) { m_underMemoryPressure = b; }

#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    WEBCORE_EXPORT void installMemoryReleaseBlock(void (^releaseMemoryBlock)(), bool clearPressureOnMemoryRelease = true);
    WEBCORE_EXPORT void setReceivedMemoryPressure(MemoryPressureReason);
    WEBCORE_EXPORT void clearMemoryPressure();
    WEBCORE_EXPORT bool shouldWaitForMemoryClearMessage();
    void respondToMemoryPressureIfNeeded();
#elif OS(LINUX)
    void setMemoryPressureMonitorHandle(int fd);
#endif

    class ReliefLogger {
    public:
        explicit ReliefLogger(const char *log)
            : m_logString(log)
            , m_initialMemory(loggingEnabled() ? platformMemoryUsage() : MemoryUsage { })
        {
        }

        ~ReliefLogger()
        {
            if (loggingEnabled())
                logMemoryUsageChange();
        }


        const char* logString() const { return m_logString; }
        static void setLoggingEnabled(bool enabled) { s_loggingEnabled = enabled; }
        static bool loggingEnabled()
        {
#if RELEASE_LOG_DISABLED
            return s_loggingEnabled;
#else
            return true;
#endif
        }

    private:
        struct MemoryUsage {
            MemoryUsage() = default;
            MemoryUsage(size_t resident, size_t physical)
                : resident(resident)
                , physical(physical)
            {
            }
            size_t resident { 0 };
            size_t physical { 0 };
        };
        std::optional<MemoryUsage> platformMemoryUsage();
        void logMemoryUsageChange();

        const char* m_logString;
        std::optional<MemoryUsage> m_initialMemory;

        WEBCORE_EXPORT static bool s_loggingEnabled;
    };

    WEBCORE_EXPORT void releaseMemory(Critical, Synchronous = Synchronous::No);

    WEBCORE_EXPORT void beginSimulatedMemoryPressure();
    WEBCORE_EXPORT void endSimulatedMemoryPressure();

private:
    void uninstall();

    void holdOff(unsigned);

    MemoryPressureHandler();
    ~MemoryPressureHandler() = delete;

    void respondToMemoryPressure(Critical, Synchronous = Synchronous::No);
    void platformReleaseMemory(Critical);

    NO_RETURN_DUE_TO_CRASH void didExceedMemoryLimitAndFailedToRecover();
    void measurementTimerFired();

#if OS(LINUX)
    class EventFDPoller {
        WTF_MAKE_NONCOPYABLE(EventFDPoller); WTF_MAKE_FAST_ALLOCATED;
    public:
        EventFDPoller(int fd, std::function<void ()>&& notifyHandler);
        ~EventFDPoller();

    private:
        void readAndNotify() const;

        std::optional<int> m_fd;
        std::function<void ()> m_notifyHandler;
#if USE(GLIB)
        GRefPtr<GSource> m_source;
#else
        ThreadIdentifier m_threadID;
#endif
    };
#endif

    bool m_installed { false };
    LowMemoryHandler m_lowMemoryHandler;

    std::atomic<bool> m_underMemoryPressure;
    bool m_isSimulatingMemoryPressure { false };

    std::unique_ptr<RunLoop::Timer<MemoryPressureHandler>> m_measurementTimer;
    MemoryUsagePolicy m_memoryUsagePolicy { MemoryUsagePolicy::Unrestricted };
    WTF::Function<void()> m_memoryKillCallback;
    WTF::Function<bool()> m_processIsEligibleForMemoryKillCallback;

#if PLATFORM(IOS)
    // FIXME: Can we share more of this with OpenSource?
    uint32_t m_memoryPressureReason { MemoryPressureReasonNone };
    bool m_clearPressureOnMemoryRelease { true };
    void (^m_releaseMemoryBlock)() { nullptr };
    CFRunLoopObserverRef m_observer { nullptr };
    Lock m_observerMutex;
#elif OS(LINUX)
    std::optional<int> m_eventFD;
    std::optional<int> m_pressureLevelFD;
    std::unique_ptr<EventFDPoller> m_eventFDPoller;
    RunLoop::Timer<MemoryPressureHandler> m_holdOffTimer;
    void holdOffTimerFired();
    void logErrorAndCloseFDs(const char* error);
    bool tryEnsureEventFD();
#endif
};

} // namespace WebCore

#endif // MemoryPressureHandler_h
