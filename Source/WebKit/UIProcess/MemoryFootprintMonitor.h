/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)

#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/ProcessID.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class MemoryFootprintMonitor : public CanMakeWeakPtr<MemoryFootprintMonitor>, public CanMakeCheckedPtr<MemoryFootprintMonitor> {
    WTF_MAKE_TZONE_ALLOCATED(MemoryFootprintMonitor);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MemoryFootprintMonitor);
    WTF_MAKE_NONCOPYABLE(MemoryFootprintMonitor);
public:
    struct Configuration {
        Seconds pollInterval;
        size_t foregroundPageMemoryLimit;
        size_t backgroundPageMemoryLimit;
        size_t webProcessMemoryLimit;
    };
    static Configuration defaultConfiguration();

    static MemoryFootprintMonitor& singleton();

    explicit MemoryFootprintMonitor(Configuration&&);
    ~MemoryFootprintMonitor();

    void start();

    void setConfiguration(Configuration&&);
    const Configuration& configuration() const { return m_configuration; }

private:
    enum class State : uint8_t {
        NotStarted,
        WaitingToMeasure,
        Measuring,
    };

    static WorkQueue& measurementQueueSingleton();

    void measurementTimerFired();
    void didCompleteMeasurement(HashMap<ProcessID, size_t>&&);

    Configuration m_configuration;
    RunLoop::Timer m_measurementTimer;
    State m_state { State::NotStarted };
};

} // namespace WebKit

#endif // ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)
