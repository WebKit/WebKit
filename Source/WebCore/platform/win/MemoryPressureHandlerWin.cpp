/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "MemoryPressureHandler.h"

#include <WebCore/Timer.h>
#include <WebCore/Win32Handle.h>
#include <psapi.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// We create a timer for checking the memory usage at regular intervals.

class CheckMemoryTimer : public TimerBase {
public:
    CheckMemoryTimer(MemoryPressureHandler&);

private:
    virtual void fired();

    void handleMemoryLow();

    MemoryPressureHandler& m_pressureHandler;
    Win32Handle m_lowMemoryHandle;
};

CheckMemoryTimer::CheckMemoryTimer(MemoryPressureHandler& pressureHandler)
    : m_pressureHandler(pressureHandler)
{
    m_lowMemoryHandle = CreateMemoryResourceNotification(LowMemoryResourceNotification);
}

void CheckMemoryTimer::fired()
{
    m_pressureHandler.setUnderMemoryPressure(false);

    BOOL memoryLow;

    if (QueryMemoryResourceNotification(m_lowMemoryHandle.get(), &memoryLow) && memoryLow) {
        handleMemoryLow();
        return;
    }

#if CPU(X86)
    PROCESS_MEMORY_COUNTERS_EX counters;

    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return;

    // On Windows, 32-bit processes have 2GB of memory available, where some is used by the system.
    // Debugging has shown that allocations might fail and cause crashes when memory usage is > ~1GB.
    const int maxMemoryUsageBytes = 1024 * 1024 * 1024;

    if (counters.PrivateUsage > maxMemoryUsageBytes)
        handleMemoryLow();
#endif
}

void CheckMemoryTimer::handleMemoryLow()
{
    m_pressureHandler.setUnderMemoryPressure(true);
    m_pressureHandler.releaseMemory(Critical::Yes);
}

void MemoryPressureHandler::platformReleaseMemory(Critical)
{
}

static std::unique_ptr<CheckMemoryTimer>& memCheckTimer()
{
    static NeverDestroyed<std::unique_ptr<CheckMemoryTimer>> memCheckTimer;
    return memCheckTimer;
}

void MemoryPressureHandler::install()
{
    m_installed = true;
    memCheckTimer() = std::make_unique<CheckMemoryTimer>(*this);
    memCheckTimer()->startRepeating(60.0);
}

void MemoryPressureHandler::uninstall()
{
    if (!m_installed)
        return;

    memCheckTimer() = nullptr;
    m_installed = false;
}

void MemoryPressureHandler::holdOff(unsigned seconds)
{
}

void MemoryPressureHandler::respondToMemoryPressure(Critical critical, Synchronous synchronous)
{
    uninstall();

    m_lowMemoryHandler(critical, synchronous);
}

size_t MemoryPressureHandler::ReliefLogger::platformMemoryUsage()
{
    return 0;
}

} // namespace WebCore
