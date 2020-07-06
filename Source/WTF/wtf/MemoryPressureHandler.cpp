/*
 * Copyright (C) 2011-2017 Apple Inc. All Rights Reserved.
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

#include <wtf/MemoryFootprint.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RAMSize.h>

#define LOG_CHANNEL_PREFIX Log

namespace WTF {

#if RELEASE_LOG_DISABLED
WTFLogChannel LogMemoryPressure = { WTFLogChannelState::On, "MemoryPressure", WTFLogLevel::Error };
#endif
#if USE(OS_LOG) && !RELEASE_LOG_DISABLED
WTFLogChannel LogMemoryPressure = { WTFLogChannelState::On, "MemoryPressure", WTFLogLevel::Error, LOG_CHANNEL_WEBKIT_SUBSYSTEM, OS_LOG_DEFAULT };
#endif
#if USE(JOURNALD) && !RELEASE_LOG_DISABLED
WTFLogChannel LogMemoryPressure = { WTFLogChannelState::On, "MemoryPressure", WTFLogLevel::Error, LOG_CHANNEL_WEBKIT_SUBSYSTEM };
#endif

WTF_EXPORT_PRIVATE bool MemoryPressureHandler::ReliefLogger::s_loggingEnabled = false;

MemoryPressureHandler& MemoryPressureHandler::singleton()
{
    static NeverDestroyed<MemoryPressureHandler> memoryPressureHandler;
    return memoryPressureHandler;
}

MemoryPressureHandler::MemoryPressureHandler()
#if OS(LINUX) || OS(FREEBSD)
    : m_holdOffTimer(RunLoop::main(), this, &MemoryPressureHandler::holdOffTimerFired)
#elif OS(WINDOWS)
    : m_windowsMeasurementTimer(RunLoop::main(), this, &MemoryPressureHandler::windowsMeasurementTimerFired)
#endif
{
#if PLATFORM(COCOA)
    setDispatchQueue(dispatch_get_main_queue());
#endif
}

void MemoryPressureHandler::setShouldUsePeriodicMemoryMonitor(bool use)
{
    if (!isFastMallocEnabled()) {
        // If we're running with FastMalloc disabled, some kind of testing or debugging is probably happening.
        // Let's be nice and not enable the memory kill mechanism.
        return;
    }

    if (use) {
        m_measurementTimer = makeUnique<RunLoop::Timer<MemoryPressureHandler>>(RunLoop::main(), this, &MemoryPressureHandler::measurementTimerFired);
        m_measurementTimer->startRepeating(30_s);
    } else
        m_measurementTimer = nullptr;
}

#if !RELEASE_LOG_DISABLED
static const char* toString(MemoryUsagePolicy policy)
{
    switch (policy) {
    case MemoryUsagePolicy::Unrestricted: return "Unrestricted";
    case MemoryUsagePolicy::Conservative: return "Conservative";
    case MemoryUsagePolicy::Strict: return "Strict";
    }
    ASSERT_NOT_REACHED();
    return "";
}
#endif

static size_t thresholdForMemoryKillWithProcessState(WebsamProcessState processState, unsigned tabCount)
{
    size_t baseThreshold = 2 * GB;
#if CPU(X86_64) || CPU(ARM64)
    if (processState == WebsamProcessState::Active)
        baseThreshold = 4 * GB;
    if (tabCount > 1)
        baseThreshold += std::min(tabCount - 1, 4u) * 1 * GB;
#else
    if ((tabCount > 1) || (processState == WebsamProcessState::Active))
        baseThreshold = 3 * GB;
#endif
    return std::min(baseThreshold, static_cast<size_t>(ramSize() * 0.9));
}

void MemoryPressureHandler::setPageCount(unsigned pageCount)
{
    if (singleton().m_pageCount == pageCount)
        return;
    singleton().m_pageCount = pageCount;
}

size_t MemoryPressureHandler::thresholdForMemoryKill()
{
    return thresholdForMemoryKillWithProcessState(m_processState, m_pageCount);
}

static size_t thresholdForPolicy(MemoryUsagePolicy policy)
{
    const size_t baseThresholdForPolicy = std::min(3 * GB, ramSize());

#if PLATFORM(IOS_FAMILY)
    const double conservativeThresholdFraction = 0.5;
    const double strictThresholdFraction = 0.65;
#else
    const double conservativeThresholdFraction = 0.33;
    const double strictThresholdFraction = 0.5;
#endif

    switch (policy) {
    case MemoryUsagePolicy::Unrestricted:
        return 0;
    case MemoryUsagePolicy::Conservative:
        return baseThresholdForPolicy * conservativeThresholdFraction;
    case MemoryUsagePolicy::Strict:
        return baseThresholdForPolicy * strictThresholdFraction;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

static MemoryUsagePolicy policyForFootprint(size_t footprint)
{
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Strict))
        return MemoryUsagePolicy::Strict;
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Conservative))
        return MemoryUsagePolicy::Conservative;
    return MemoryUsagePolicy::Unrestricted;
}

MemoryUsagePolicy MemoryPressureHandler::currentMemoryUsagePolicy()
{
    return policyForFootprint(memoryFootprint());
}

void MemoryPressureHandler::shrinkOrDie()
{
    RELEASE_LOG(MemoryPressure, "Process is above the memory kill threshold. Trying to shrink down.");
    releaseMemory(Critical::Yes, Synchronous::Yes);

    size_t footprint = memoryFootprint();
    RELEASE_LOG(MemoryPressure, "New memory footprint: %zu MB", footprint / MB);

    if (footprint < thresholdForMemoryKill()) {
        RELEASE_LOG(MemoryPressure, "Shrank below memory kill threshold. Process gets to live.");
        setMemoryUsagePolicyBasedOnFootprint(footprint);
        return;
    }

    WTFLogAlways("Unable to shrink memory footprint of process (%zu MB) below the kill thresold (%zu MB). Killed\n", footprint / MB, thresholdForMemoryKill() / MB);
    RELEASE_ASSERT(m_memoryKillCallback);
    m_memoryKillCallback();
}

void MemoryPressureHandler::setMemoryUsagePolicyBasedOnFootprint(size_t footprint)
{
    auto newPolicy = policyForFootprint(footprint);
    if (newPolicy == m_memoryUsagePolicy)
        return;

    RELEASE_LOG(MemoryPressure, "Memory usage policy changed: %s -> %s", toString(m_memoryUsagePolicy), toString(newPolicy));
    m_memoryUsagePolicy = newPolicy;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::measurementTimerFired()
{
    size_t footprint = memoryFootprint();
#if PLATFORM(COCOA)
    RELEASE_LOG(MemoryPressure, "Current memory footprint: %zu MB", footprint / MB);
#endif
    if (footprint >= thresholdForMemoryKill()) {
        shrinkOrDie();
        return;
    }

    setMemoryUsagePolicyBasedOnFootprint(footprint);

    switch (m_memoryUsagePolicy) {
    case MemoryUsagePolicy::Unrestricted:
        break;
    case MemoryUsagePolicy::Conservative:
        releaseMemory(Critical::No, Synchronous::No);
        break;
    case MemoryUsagePolicy::Strict:
        releaseMemory(Critical::Yes, Synchronous::No);
        break;
    }

    if (processState() == WebsamProcessState::Active && footprint > thresholdForMemoryKillWithProcessState(WebsamProcessState::Inactive, m_pageCount))
        doesExceedInactiveLimitWhileActive();
    else
        doesNotExceedInactiveLimitWhileActive();
}

void MemoryPressureHandler::doesExceedInactiveLimitWhileActive()
{
    if (m_hasInvokedDidExceedInactiveLimitWhileActiveCallback)
        return;
    if (m_didExceedInactiveLimitWhileActiveCallback)
        m_didExceedInactiveLimitWhileActiveCallback();
    m_hasInvokedDidExceedInactiveLimitWhileActiveCallback = true;
}

void MemoryPressureHandler::doesNotExceedInactiveLimitWhileActive()
{
    m_hasInvokedDidExceedInactiveLimitWhileActiveCallback = false;
}

void MemoryPressureHandler::setProcessState(WebsamProcessState state)
{
    if (m_processState == state)
        return;
    m_processState = state;
}

void MemoryPressureHandler::beginSimulatedMemoryPressure()
{
    if (m_isSimulatingMemoryPressure)
        return;
    m_isSimulatingMemoryPressure = true;
    memoryPressureStatusChanged();
    respondToMemoryPressure(Critical::Yes, Synchronous::Yes);
}

void MemoryPressureHandler::endSimulatedMemoryPressure()
{
    if (!m_isSimulatingMemoryPressure)
        return;
    m_isSimulatingMemoryPressure = false;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::releaseMemory(Critical critical, Synchronous synchronous)
{
    if (!m_lowMemoryHandler)
        return;

    ReliefLogger log("Total");
    m_lowMemoryHandler(critical, synchronous);
    platformReleaseMemory(critical);
}

void MemoryPressureHandler::setUnderMemoryPressure(bool underMemoryPressure)
{
    if (m_underMemoryPressure == underMemoryPressure)
        return;
    m_underMemoryPressure = underMemoryPressure;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::memoryPressureStatusChanged()
{
    if (m_memoryPressureStatusChangedCallback)
        m_memoryPressureStatusChangedCallback(isUnderMemoryPressure());
}

void MemoryPressureHandler::ReliefLogger::logMemoryUsageChange()
{
#if !RELEASE_LOG_DISABLED
#define MEMORYPRESSURE_LOG(...) RELEASE_LOG(MemoryPressure, __VA_ARGS__)
#else
#define MEMORYPRESSURE_LOG(...) WTFLogAlways(__VA_ARGS__)
#endif

    auto currentMemory = platformMemoryUsage();
    if (!currentMemory || !m_initialMemory) {
        MEMORYPRESSURE_LOG("Memory pressure relief: %" PUBLIC_LOG_STRING ": (Unable to get dirty memory information for process)", m_logString);
        return;
    }

    long residentDiff = currentMemory->resident - m_initialMemory->resident;
    long physicalDiff = currentMemory->physical - m_initialMemory->physical;

    MEMORYPRESSURE_LOG("Memory pressure relief: %" PUBLIC_LOG_STRING ": res = %zu/%zu/%ld, res+swap = %zu/%zu/%ld",
        m_logString,
        m_initialMemory->resident, currentMemory->resident, residentDiff,
        m_initialMemory->physical, currentMemory->physical, physicalDiff);
}

#if !OS(WINDOWS)
void MemoryPressureHandler::platformInitialize() { }
#endif

#if PLATFORM(COCOA)
void MemoryPressureHandler::setDispatchQueue(dispatch_queue_t queue)
{
    RELEASE_ASSERT(!m_installed);
    dispatch_retain(queue);
    if (m_dispatchQueue)
        dispatch_release(m_dispatchQueue);
    m_dispatchQueue = queue;
}
#endif

} // namespace WebCore
