/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <atomic>
#include <functional>
#include <ranges>
#include <wtf/Logging.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RAMSize.h>

#if PLATFORM(COCOA)
#include <wtf/darwin/DispatchExtras.h>
#endif

namespace WTF {

WTF_EXPORT_PRIVATE bool MemoryPressureHandler::ReliefLogger::s_loggingEnabled = false;

#if PLATFORM(IOS_FAMILY)
static const double s_conservativeThresholdFraction = 0.5;
static const double s_strictThresholdFraction = 0.65;
#else
static const double s_conservativeThresholdFraction = 0.33;
static const double s_strictThresholdFraction = 0.5;
#endif
static const std::optional<double> s_killThresholdFraction;
static const Seconds s_pollInterval = 30_s;

static std::atomic<bool> s_hasCreatedMemoryPressureHandler { true };

MemoryPressureHandler& MemoryPressureHandler::singleton()
{
    static NeverDestroyed<MemoryPressureHandler> memoryPressureHandler;
    return memoryPressureHandler;
}

static MemoryPressureHandler* memoryPressureHandlerIfExists()
{
    return s_hasCreatedMemoryPressureHandler.load() ? &MemoryPressureHandler::singleton() : nullptr;
}

MemoryPressureHandler::MemoryPressureHandler()
#if OS(LINUX) || OS(FREEBSD) || OS(HAIKU) || OS(QNX)
    : m_holdOffTimer(RunLoop::mainSingleton(), "MemoryPressureHandler::HoldOffTimer"_s, this, &MemoryPressureHandler::holdOffTimerFired)
#elif OS(WINDOWS)
    : m_windowsMeasurementTimer(RunLoop::mainSingleton(), "MemoryPressureHandler::WindowsMeasurementTimer"_s, this, &MemoryPressureHandler::windowsMeasurementTimerFired)
#endif
{
#if PLATFORM(COCOA)
    setDispatchQueue(mainDispatchQueueSingleton());
#endif
}

void MemoryPressureHandler::setMemoryFootprintPollIntervalForTesting(Seconds pollInterval)
{
    m_configuration.pollInterval = pollInterval;
}

void MemoryPressureHandler::setShouldUsePeriodicMemoryMonitor(bool use)
{
    if (use) {
        m_measurementTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "MemoryPressureHandler::MeasurementTimer"_s, this, &MemoryPressureHandler::measurementTimerFired);
        m_measurementTimer->startRepeating(m_configuration.pollInterval);
    } else
        m_measurementTimer = nullptr;
}

#if !RELEASE_LOG_DISABLED
static ASCIILiteral toString(MemoryUsagePolicy policy)
{
    switch (policy) {
    case MemoryUsagePolicy::Unrestricted: return "Unrestricted"_s;
    case MemoryUsagePolicy::Conservative: return "Conservative"_s;
    case MemoryUsagePolicy::Strict: return "Strict"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}
#endif

static size_t thresholdForMemoryKillOfActiveProcess(unsigned tabCount)
{
#if CPU(ADDRESS64)
    size_t baseThreshold = ramSize() > 16 * GB ? 15 * GB : 7 * GB;
    return baseThreshold + tabCount * GB;
#else
    UNUSED_PARAM(tabCount);
    return std::min(3 * GB, static_cast<size_t>(ramSize() * 0.9));
#endif
}

static size_t thresholdForMemoryKillOfInactiveProcess(unsigned tabCount)
{
#if CPU(ADDRESS64)
    size_t baseThreshold = 3 * GB + tabCount * GB;
#else
    size_t baseThreshold = tabCount > 1 ? 3 * GB : 2 * GB;
#endif
    return std::min(baseThreshold, static_cast<size_t>(ramSize() * 0.9));
}

void MemoryPressureHandler::setPageCount(unsigned pageCount)
{
    if (singleton().m_pageCount == pageCount)
        return;
    singleton().m_pageCount = pageCount;
}

std::optional<size_t> MemoryPressureHandler::thresholdForMemoryKill()
{
    if (m_configuration.killThresholdFraction)
        return m_configuration.baseThreshold * (*m_configuration.killThresholdFraction);

    switch (m_processState) {
    case WebsamProcessState::Inactive:
        return thresholdForMemoryKillOfInactiveProcess(m_pageCount);
    case WebsamProcessState::Active:
        return thresholdForMemoryKillOfActiveProcess(m_pageCount);
    }
    return std::nullopt;
}

size_t MemoryPressureHandler::thresholdForPolicy(MemoryUsagePolicy policy)
{
    switch (policy) {
    case MemoryUsagePolicy::Unrestricted:
        return 0;
    case MemoryUsagePolicy::Conservative:
        return m_configuration.baseThreshold * m_configuration.conservativeThresholdFraction;
    case MemoryUsagePolicy::Strict:
        return m_configuration.baseThreshold * m_configuration.strictThresholdFraction;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

MemoryUsagePolicy MemoryPressureHandler::policyForFootprint(size_t footprint)
{
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Strict))
        return MemoryUsagePolicy::Strict;
    if (footprint >= thresholdForPolicy(MemoryUsagePolicy::Conservative))
        return MemoryUsagePolicy::Conservative;
    return MemoryUsagePolicy::Unrestricted;
}

MemoryUsagePolicy MemoryPressureHandler::currentMemoryUsagePolicy()
{
    if (m_isSimulatingMemoryWarning)
        return MemoryUsagePolicy::Conservative;
    if (m_isSimulatingMemoryPressure)
        return MemoryUsagePolicy::Strict;
    return policyForFootprint(memoryFootprint());
}

void MemoryPressureHandler::shrinkOrDie(size_t killThreshold)
{
    RELEASE_LOG(MemoryPressure, "Process is above the memory kill threshold. Trying to shrink down.");
    releaseMemory(Critical::Yes, Synchronous::Yes);

    size_t footprint = memoryFootprint();
    RELEASE_LOG(MemoryPressure, "New memory footprint: %zu MB", footprint / MB);

    if (footprint < killThreshold) {
        RELEASE_LOG(MemoryPressure, "Shrank below memory kill threshold. Process gets to live.");
        setMemoryUsagePolicyBasedOnFootprint(footprint);
        return;
    }

    WTFLogAlways("Unable to shrink memory footprint of process (%zu MB) below the kill thresold (%zu MB). Killed\n", footprint / MB, killThreshold / MB);
    RELEASE_ASSERT(m_memoryKillCallback);
    m_memoryKillCallback();
}

void MemoryPressureHandler::setMemoryUsagePolicyBasedOnFootprint(size_t footprint)
{
    auto newPolicy = policyForFootprint(footprint);
    if (newPolicy == m_memoryUsagePolicy)
        return;

    RELEASE_LOG(MemoryPressure, "Memory usage policy changed: %s -> %s", toString(m_memoryUsagePolicy).characters(), toString(newPolicy).characters());
    m_memoryUsagePolicy = newPolicy;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::setMemoryFootprintNotificationThresholds(Vector<uint64_t>&& thresholds, WTF::Function<void(uint64_t)>&& handler)
{
    if (thresholds.isEmpty() || !handler)
        return;

    std::ranges::sort(thresholds, std::greater<>());
    m_memoryFootprintNotificationThresholds = WTFMove(thresholds);
    m_memoryFootprintNotificationHandler = WTFMove(handler);
}


void MemoryPressureHandler::measurementTimerFired()
{
    size_t footprint = memoryFootprint();
#if PLATFORM(COCOA)
    RELEASE_LOG(MemoryPressure, "Current memory footprint: %zu MB", footprint / MB);
#endif

    while (m_memoryFootprintNotificationThresholds.size() && footprint > m_memoryFootprintNotificationThresholds.last()) {
        auto notificationThreshold = m_memoryFootprintNotificationThresholds.takeLast();
        m_memoryFootprintNotificationHandler(notificationThreshold);
    }

    auto killThreshold = thresholdForMemoryKill();
    if (killThreshold && footprint >= *killThreshold) {
        shrinkOrDie(*killThreshold);
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
}

void MemoryPressureHandler::setProcessState(WebsamProcessState state)
{
    if (m_processState == state)
        return;
    m_processState = state;
}

ASCIILiteral MemoryPressureHandler::processStateDescription()
{
    if (RefPtr handler = memoryPressureHandlerIfExists()) {
        switch (handler->processState()) {
        case WebsamProcessState::Active:
            return "active"_s;
        case WebsamProcessState::Inactive:
            return "inactive"_s;
        }
    }
    return "unknown"_s;
}

void MemoryPressureHandler::beginSimulatedMemoryWarning()
{
    if (m_isSimulatingMemoryWarning)
        return;
    m_isSimulatingMemoryWarning = true;
    memoryPressureStatusChanged();
    respondToMemoryPressure(Critical::No, Synchronous::Yes);
}

void MemoryPressureHandler::endSimulatedMemoryWarning()
{
    if (!m_isSimulatingMemoryWarning)
        return;
    m_isSimulatingMemoryWarning = false;
    memoryPressureStatusChanged();
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

void MemoryPressureHandler::setMemoryPressureStatus(SystemMemoryPressureStatus status)
{
    if (m_memoryPressureStatus == status)
        return;

    m_memoryPressureStatus = status;
    memoryPressureStatusChanged();
}

void MemoryPressureHandler::memoryPressureStatusChanged()
{
    if (m_memoryPressureStatusChangedCallback)
        m_memoryPressureStatusChangedCallback();
}

void MemoryPressureHandler::didExceedProcessMemoryLimit(ProcessMemoryLimit limit)
{
    if (m_didExceedProcessMemoryLimitCallback)
        m_didExceedProcessMemoryLimitCallback(limit);
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

MemoryPressureHandlerConfiguration::MemoryPressureHandlerConfiguration()
    : baseThreshold(std::min(3 * GB, ramSize()))
    , conservativeThresholdFraction(s_conservativeThresholdFraction)
    , strictThresholdFraction(s_strictThresholdFraction)
    , killThresholdFraction(s_killThresholdFraction)
    , pollInterval(s_pollInterval)
{
}

MemoryPressureHandlerConfiguration::MemoryPressureHandlerConfiguration(uint64_t base, double conservative, double strict, std::optional<double> kill, Seconds interval)
    : baseThreshold(base)
    , conservativeThresholdFraction(conservative)
    , strictThresholdFraction(strict)
    , killThresholdFraction(kill)
    , pollInterval(interval)
{
}

} // namespace WebCore
