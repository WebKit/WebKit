/*
 * Copyright (C) 2011, 2012 Apple Inc. All Rights Reserved.
 * Copyright (C) 2014 Raspberry Pi Foundation. All Rights Reserved.
 * Copyright (C) 2018 Igalia S.L.
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
#include <wtf/MemoryPressureHandler.h>

#include <malloc.h>
#include <unistd.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/linux/CurrentProcessMemoryStatus.h>
#include <wtf/text/WTFString.h>

#define LOG_CHANNEL_PREFIX Log

namespace WTF {

// Disable memory event reception for a minimum of s_minimumHoldOffTime
// seconds after receiving an event. Don't let events fire any sooner than
// s_holdOffMultiplier times the last cleanup processing time. Effectively
// this is 1 / s_holdOffMultiplier percent of the time.
// If after releasing the memory we don't free at least s_minimumBytesFreedToUseMinimumHoldOffTime,
// we wait longer to try again (s_maximumHoldOffTime).
// These value seems reasonable and testing verifies that it throttles frequent
// low memory events, greatly reducing CPU usage.
static const Seconds s_minimumHoldOffTime { 5_s };
static const Seconds s_maximumHoldOffTime { 30_s };
static const size_t s_minimumBytesFreedToUseMinimumHoldOffTime = 1 * MB;
static const unsigned s_holdOffMultiplier = 20;

void MemoryPressureHandler::triggerMemoryPressureEvent(bool isCritical)
{
    if (!m_installed)
        return;

    if (ReliefLogger::loggingEnabled())
        LOG(MemoryPressure, "Got memory pressure notification (%s)", isCritical ? "critical" : "non-critical");

    setUnderMemoryPressure(true);

    if (isMainThread())
        respondToMemoryPressure(isCritical ? Critical::Yes : Critical::No);
    else
        RunLoop::main().dispatch([this, isCritical] {
            respondToMemoryPressure(isCritical ? Critical::Yes : Critical::No);
        });

    if (ReliefLogger::loggingEnabled() && isUnderMemoryPressure())
        LOG(MemoryPressure, "System is no longer under memory pressure.");

    setUnderMemoryPressure(false);
}

void MemoryPressureHandler::install()
{
    if (m_installed || m_holdOffTimer.isActive())
        return;

    m_installed = true;
}

void MemoryPressureHandler::uninstall()
{
    if (!m_installed)
        return;

    m_holdOffTimer.stop();

    m_installed = false;
}

void MemoryPressureHandler::holdOffTimerFired()
{
    install();
}

void MemoryPressureHandler::holdOff(Seconds seconds)
{
    m_holdOffTimer.startOneShot(seconds);
}

static size_t processMemoryUsage()
{
    ProcessMemoryStatus memoryStatus;
    currentProcessMemoryStatus(memoryStatus);
    return (memoryStatus.resident - memoryStatus.shared);
}

void MemoryPressureHandler::respondToMemoryPressure(Critical critical, Synchronous synchronous)
{
    uninstall();

    MonotonicTime startTime = MonotonicTime::now();
    int64_t processMemory = processMemoryUsage();
    releaseMemory(critical, synchronous);
    int64_t bytesFreed = processMemory - processMemoryUsage();
    Seconds holdOffTime = s_maximumHoldOffTime;
    if (bytesFreed > 0 && static_cast<size_t>(bytesFreed) >= s_minimumBytesFreedToUseMinimumHoldOffTime)
        holdOffTime = (MonotonicTime::now() - startTime) * s_holdOffMultiplier;
    holdOff(std::max(holdOffTime, s_minimumHoldOffTime));
}

void MemoryPressureHandler::platformReleaseMemory(Critical)
{
#if HAVE(MALLOC_TRIM)
    malloc_trim(0);
#endif
}

Optional<MemoryPressureHandler::ReliefLogger::MemoryUsage> MemoryPressureHandler::ReliefLogger::platformMemoryUsage()
{
    return MemoryUsage {processMemoryUsage(), memoryFootprint()};
}

} // namespace WTF
