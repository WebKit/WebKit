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

#include "config.h"
#include "MemoryFootprintMonitor.h"

#if ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)

#include "Logging.h"
#include "ProcessThrottler.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/HashSet.h>
#include <wtf/MachSendRight.h>
#include <wtf/MathExtras.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RAMSize.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/Device.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MemoryFootprintMonitor);
static constexpr Seconds defaultPollInterval { 30_s };

MemoryFootprintMonitor::Configuration MemoryFootprintMonitor::defaultConfiguration()
{
    // Chosen to match per-process jetsam limits, except on iPadOS and visionOS (which run with no
    // limit other than the global Mach per-task limit).
#if PLATFORM(IOS_FAMILY) && !PLATFORM(VISION)
    size_t perProcessJetsamLimit = 2 * GB;

#if PLATFORM(WATCHOS)
    perProcessJetsamLimit = 120 * MB;
#elif PLATFORM(APPLETV)
    perProcessJetsamLimit = 840 * MB;
#endif

    if (!PAL::deviceClassIsDesktop())
        return { defaultPollInterval, perProcessJetsamLimit, perProcessJetsamLimit, perProcessJetsamLimit };
#endif

    // Chosen to match MemoryPressureMonitor defaults.
    auto ramSize = WTF::ramSize();
    return {
        defaultPollInterval,
        ramSize > 16 * GB ? 16 * GB : 8 * GB,
        std::min<size_t>(4 * GB, truncateDoubleToUint64(ramSize * 0.9)),
        std::min<size_t>(4 * GB, truncateDoubleToUint64(ramSize * 0.9)),
    };
}

MemoryFootprintMonitor& MemoryFootprintMonitor::singleton()
{
    ASSERT(isMainRunLoop());
    static MainRunLoopNeverDestroyed<MemoryFootprintMonitor> monitor(defaultConfiguration());
    return monitor.get();
}

WorkQueue& MemoryFootprintMonitor::measurementQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("MemoryFootprintMonitor Work Queue"_s);
    return workQueue.get();
}

MemoryFootprintMonitor::MemoryFootprintMonitor(Configuration&& configuration)
    : m_configuration(WTF::move(configuration))
    , m_measurementTimer(RunLoop::mainSingleton(), "MemoryFootprintMonitor::MeasurementTimer"_s, this, &MemoryFootprintMonitor::measurementTimerFired)
{
    ASSERT(isMainRunLoop());
}

MemoryFootprintMonitor::~MemoryFootprintMonitor() = default;

static bool shouldEnforceMemoryLimits()
{
#if ASAN_ENABLED || TSAN_ENABLED
    RELEASE_LOG(MemoryMeasurement, "MemoryFootprintMonitor: not enforcing memory limits because ASAN or TSAN is enabled");
    return false;
#else
    // Matches the previous enablement criteria for the WebProcess periodic memory monitor.
    bool fastMallocDisabled = getenv("__XPC_Malloc") || getenv("__XPC_MallocStackLogging");
    bool strongRefTrackerEnabled = getenv("__XPC_JSC_enableStrongRefTracker");
    bool dumpHeapOnLowMemoryEnabled = getenv("__XPC_JSC_dumpHeapOnLowMemory");

    bool shouldEnforce = !fastMallocDisabled || strongRefTrackerEnabled || dumpHeapOnLowMemoryEnabled;
    if (!shouldEnforce)
        RELEASE_LOG(MemoryMeasurement, "MemoryFootprintMonitor: not enforcing memory limits. fastMallocDisabled: %d strongRefTrackerEnabled: %d dumpHeapOnLowMemoryEnabled: %d", fastMallocDisabled, strongRefTrackerEnabled, dumpHeapOnLowMemoryEnabled);
    return shouldEnforce;
#endif
}

void MemoryFootprintMonitor::start()
{
    ASSERT(isMainRunLoop());
    if (m_state != State::NotStarted)
        return;
    if (!shouldEnforceMemoryLimits())
        return;

    m_state = State::WaitingToMeasure;
    m_measurementTimer.startOneShot(m_configuration.pollInterval);
}

void MemoryFootprintMonitor::setConfiguration(Configuration&& configuration)
{
    ASSERT(isMainRunLoop());
    m_configuration = WTF::move(configuration);

    if (m_state == State::WaitingToMeasure) {
        m_measurementTimer.stop();
        m_measurementTimer.startOneShot(m_configuration.pollInterval);
    }
}

static ProcessThrottleState processThrottleState(const WebProcessProxy& process)
{
#if !USE(RUNNINGBOARD)
    // FIXME: Consider making ProcessThrottler to return sensible values when RunningBoard is
    // disabled. Currently, when RB is disabled, ProcessThrottler fails to acquire activities and
    // always thinks it's suspended.
    UNUSED_PARAM(process);
    return ProcessThrottleState::Foreground;
#else
    return process.throttler().currentState();
#endif
}

void MemoryFootprintMonitor::measurementTimerFired()
{
    ASSERT(isMainRunLoop());
    ASSERT(m_state == State::WaitingToMeasure);

    Vector<std::pair<ProcessID, MachSendRight>> processesToMeasure;
    for (Ref process : WebProcessProxy::allProcesses()) {
        auto pid = process->processID();
        if (!pid || process->wasTerminated() || !process->taskNamePort() || processThrottleState(process) == ProcessThrottleState::Suspended)
            continue;

        processesToMeasure.append({ pid, MachSendRight { process->taskNamePort() } });
    }

    if (processesToMeasure.isEmpty()) {
        m_measurementTimer.startOneShot(m_configuration.pollInterval);
        return;
    }

    m_state = State::Measuring;

    measurementQueueSingleton().dispatch([processesToMeasure = WTF::move(processesToMeasure), weakThis = WeakPtr { *this }]() mutable {
        HashMap<ProcessID, size_t> footprints;
        footprints.reserveInitialCapacity(processesToMeasure.size());
        for (auto& [pid, taskNamePort] : processesToMeasure) {
            auto footprint = WTF::memoryFootprint(taskNamePort.sendRight());
            footprints.add(pid, footprint);
        }

        RunLoop::mainSingleton().dispatch([footprints = WTF::move(footprints), weakThis = WTF::move(weakThis)]() mutable {
            if (weakThis)
                weakThis->didCompleteMeasurement(WTF::move(footprints));
        });
    });
}

void MemoryFootprintMonitor::didCompleteMeasurement(HashMap<ProcessID, size_t>&& footprints)
{
    ASSERT(isMainRunLoop());

    ASSERT(m_state == State::Measuring);
    m_state = State::WaitingToMeasure;

    enum class MemoryLimitType : bool { Active, Inactive };
    struct ProcessMemoryInfo {
        size_t footprint { 0 };
        bool isPageAssociatedProcess { false };
        std::optional<MemoryLimitType> exceededLimit;
    };
    HashMap<Ref<WebProcessProxy>, ProcessMemoryInfo> processInfos;

    for (Ref process : WebProcessProxy::allProcesses()) {
        if (auto maybeFootprint = footprints.getOptional(process->processID()))
            processInfos.add(process, ProcessMemoryInfo { *maybeFootprint });
    }

    struct PageMemoryInfo {
        size_t footprint { 0 };
        unsigned processCount { 0 };
        bool isForeground { false };
    };
    HashMap<Ref<WebPageProxy>, PageMemoryInfo> pageInfos;

    for (auto& [process, processInfo] : processInfos) {
        // FIXME (323677): Remove this deduping after we figure out why WebProcessProxy::pages
        // sometimes returns duplicate values.
        HashSet<Ref<WebPageProxy>> pages { protect(process)->pages() };

        for (Ref page : pages) {
            auto state = processThrottleState(protect(page->legacyMainFrameProcess()));
            if (state == ProcessThrottleState::Suspended)
                continue;

            processInfo.isPageAssociatedProcess = true;

            auto initialValue = PageMemoryInfo { 0, 0, state == ProcessThrottleState::Foreground };
            auto& pageInfo = pageInfos.add(page, WTF::move(initialValue)).iterator->value;
            ++pageInfo.processCount;

            // A process might be associated with more than one page (e.g. window.open without Site
            // Isolation). In this case, attribute the footprint of the process evenly to each page.
            pageInfo.footprint += processInfo.footprint / pages.size();
        }
    }

    bool anyProcessExceededLimit = false;

    for (auto& [page, pageInfo] : pageInfos) {
        auto limit = pageInfo.isForeground ? m_configuration.foregroundPageMemoryLimit : m_configuration.backgroundPageMemoryLimit;
        Ref mainFrameProcess = page->legacyMainFrameProcess();

        if (pageInfo.footprint <= limit) {
            RELEASE_LOG_INFO(MemoryMeasurement, "WebPageProxy %p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, state=%" PUBLIC_LOG_STRING ", processCount=%u]: footprint %zu MB, below limit %zu MB", page.ptr(), page->identifier().toUInt64(), page->webPageIDInMainFrameProcess().toUInt64(), mainFrameProcess->processID(), pageInfo.isForeground ? "FG" : "BG", pageInfo.processCount, pageInfo.footprint / MB, limit / MB);
            continue;
        }

        // Only mark the main frame process as exceeding the memory limit. When Site Isolation is
        // enabled, we count on WebProcessProxy to forward the memory kills to remote frame
        // processes in a sensible manner.
        if (auto it = processInfos.find(mainFrameProcess); it != processInfos.end()) {
            it->value.exceededLimit = pageInfo.isForeground ? MemoryLimitType::Active : MemoryLimitType::Inactive;
            anyProcessExceededLimit = true;
        }

        RELEASE_LOG_ERROR(MemoryMeasurement, "WebPageProxy %p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, state=%" PUBLIC_LOG_STRING ", processCount=%u]: footprint %zu MB, exceeded limit %zu MB", page.ptr(), page->identifier().toUInt64(), page->webPageIDInMainFrameProcess().toUInt64(), mainFrameProcess->processID(), pageInfo.isForeground ? "FG" : "BG", pageInfo.processCount, pageInfo.footprint / MB, limit / MB);
    }

    for (auto& [process, info] : processInfos) {
        if (info.isPageAssociatedProcess)
            continue;

        if (info.footprint <= m_configuration.webProcessMemoryLimit) {
            RELEASE_LOG_INFO(MemoryMeasurement, "WebProcessProxy %p - [PID=%i]: pageless process with footprint %zu MB, below limit %zu MB", process.ptr(), process->processID(), info.footprint / MB, m_configuration.webProcessMemoryLimit / MB);
            continue;
        }

        info.exceededLimit = MemoryLimitType::Inactive;
        anyProcessExceededLimit = true;

        RELEASE_LOG_ERROR(MemoryMeasurement, "WebProcessProxy %p - [PID=%i]: pageless process with footprint %zu MB, exceeded limit %zu MB", process.ptr(), process->processID(), info.footprint / MB, m_configuration.webProcessMemoryLimit / MB);
    }

    if (anyProcessExceededLimit) {
        for (auto& [process, info] : processInfos) {
            if (!info.exceededLimit)
                continue;

            RunLoop::mainSingleton().dispatch([weakProcess = WeakPtr { process.get() }, exceededLimit = *info.exceededLimit] {
                if (RefPtr process = weakProcess.get()) {
                    if (exceededLimit == MemoryLimitType::Active)
                        process->didExceedActiveMemoryLimit();
                    else
                        process->didExceedInactiveMemoryLimit();
                }
            });
        }
    }

    m_measurementTimer.startOneShot(m_configuration.pollInterval);
}

} // namespace WebKit

#endif // ENABLE(UIPROCESS_PERIODIC_MEMORY_MONITOR)
