/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "PerformanceMonitor.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Logging.h"
#include "Page.h"
#include "PerformanceLogging.h"
#include "Settings.h"

namespace WebCore {

#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(m_page.isAlwaysOnLoggingAllowed(), channel, "%p - PerformanceMonitor::" fmt, this, ##__VA_ARGS__)

static const std::chrono::seconds cpuUsageMeasurementDelay { 5 };
static const std::chrono::seconds postLoadCPUUsageMeasurementDuration { 10 };
static const std::chrono::minutes backgroundCPUUsageMeasurementDuration { 5 };
static const std::chrono::minutes cpuUsageSamplingInterval { 10 };

static const std::chrono::seconds memoryUsageMeasurementDelay { 10 };

static inline ActivityStateForCPUSampling activityStateForCPUSampling(ActivityState::Flags state)
{
    if (!(state & ActivityState::IsVisible))
        return ActivityStateForCPUSampling::NonVisible;
    if (state & ActivityState::WindowIsActive)
        return ActivityStateForCPUSampling::VisibleAndActive;
    return ActivityStateForCPUSampling::VisibleNonActive;
}

PerformanceMonitor::PerformanceMonitor(Page& page)
    : m_page(page)
    , m_postPageLoadCPUUsageTimer(*this, &PerformanceMonitor::measurePostLoadCPUUsage)
    , m_postBackgroundingCPUUsageTimer(*this, &PerformanceMonitor::measurePostBackgroundingCPUUsage)
    , m_perActivityStateCPUUsageTimer(*this, &PerformanceMonitor::measurePerActivityStateCPUUsage)
    , m_postPageLoadMemoryUsageTimer(*this, &PerformanceMonitor::measurePostLoadMemoryUsage)
    , m_postBackgroundingMemoryUsageTimer(*this, &PerformanceMonitor::measurePostBackgroundingMemoryUsage)
{
    ASSERT(!page.isUtilityPage());

    if (Settings::isPerActivityStateCPUUsageMeasurementEnabled()) {
        m_perActivityStateCPUTime = getCPUTime();
        m_perActivityStateCPUUsageTimer.startRepeating(cpuUsageSamplingInterval);
    }
}

void PerformanceMonitor::didStartProvisionalLoad()
{
    m_postLoadCPUTime = std::nullopt;
    m_postPageLoadCPUUsageTimer.stop();
    m_postPageLoadMemoryUsageTimer.stop();
}

void PerformanceMonitor::didFinishLoad()
{
    // Only do post-load CPU usage measurement if there is a single Page in the process in order to reduce noise.
    if (Settings::isPostLoadCPUUsageMeasurementEnabled() && m_page.isOnlyNonUtilityPage()) {
        m_postLoadCPUTime = std::nullopt;
        m_postPageLoadCPUUsageTimer.startOneShot(cpuUsageMeasurementDelay);
    }

    // Likewise for post-load memory usage measurement.
    if (Settings::isPostLoadMemoryUsageMeasurementEnabled() && m_page.isOnlyNonUtilityPage())
        m_postPageLoadMemoryUsageTimer.startOneShot(memoryUsageMeasurementDelay);
}

void PerformanceMonitor::activityStateChanged(ActivityState::Flags oldState, ActivityState::Flags newState)
{
    ActivityState::Flags changed = oldState ^ newState;
    bool visibilityChanged = changed & ActivityState::IsVisible;

    // Measure CPU usage of pages when they are no longer visible.
    if (Settings::isPostBackgroundingCPUUsageMeasurementEnabled() && visibilityChanged) {
        m_postBackgroundingCPUTime = std::nullopt;
        if (newState & ActivityState::IsVisible)
            m_postBackgroundingCPUUsageTimer.stop();
        else if (m_page.isOnlyNonUtilityPage())
            m_postBackgroundingCPUUsageTimer.startOneShot(cpuUsageMeasurementDelay);
    }

    if (Settings::isPerActivityStateCPUUsageMeasurementEnabled()) {
        // If visibility changed then report CPU usage right away because CPU usage is connected to visibility state.
        auto oldActivityStateForCPUSampling = activityStateForCPUSampling(oldState);
        if (oldActivityStateForCPUSampling != activityStateForCPUSampling(newState)) {
            measureCPUUsageInActivityState(oldActivityStateForCPUSampling);
            m_perActivityStateCPUUsageTimer.startRepeating(cpuUsageSamplingInterval);
        }
    }

    if (Settings::isPostBackgroundingMemoryUsageMeasurementEnabled() && visibilityChanged) {
        if (newState & ActivityState::IsVisible)
            m_postBackgroundingMemoryUsageTimer.stop();
        else if (m_page.isOnlyNonUtilityPage())
            m_postBackgroundingMemoryUsageTimer.startOneShot(memoryUsageMeasurementDelay);
    }
}

void PerformanceMonitor::measurePostLoadCPUUsage()
{
    if (!m_page.isOnlyNonUtilityPage()) {
        m_postLoadCPUTime = std::nullopt;
        return;
    }

    if (!m_postLoadCPUTime) {
        m_postLoadCPUTime = getCPUTime();
        if (m_postLoadCPUTime)
            m_postPageLoadCPUUsageTimer.startOneShot(postLoadCPUUsageMeasurementDuration);
        return;
    }
    std::optional<CPUTime> cpuTime = getCPUTime();
    if (!cpuTime)
        return;

    double cpuUsage = cpuTime.value().percentageCPUUsageSince(*m_postLoadCPUTime);
    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "measurePostLoadCPUUsage: Process was using %.1f%% CPU after the page load.", cpuUsage);
    m_page.diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::postPageLoadCPUUsageKey(), DiagnosticLoggingKeys::foregroundCPUUsageToDiagnosticLoggingKey(cpuUsage), ShouldSample::No);
}

void PerformanceMonitor::measurePostLoadMemoryUsage()
{
    if (!m_page.isOnlyNonUtilityPage())
        return;

    std::optional<uint64_t> memoryUsage = PerformanceLogging::physicalFootprint();
    if (!memoryUsage)
        return;

    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "measurePostLoadMemoryUsage: Process was using %llu bytes of memory after the page load.", memoryUsage.value());
    m_page.diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::postPageLoadMemoryUsageKey(), DiagnosticLoggingKeys::memoryUsageToDiagnosticLoggingKey(memoryUsage.value()), ShouldSample::No);
}

void PerformanceMonitor::measurePostBackgroundingMemoryUsage()
{
    if (!m_page.isOnlyNonUtilityPage())
        return;

    std::optional<uint64_t> memoryUsage = PerformanceLogging::physicalFootprint();
    if (!memoryUsage)
        return;

    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "measurePostBackgroundingMemoryUsage: Process was using %llu bytes of memory after becoming non visible.", memoryUsage.value());
    m_page.diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::postPageBackgroundingMemoryUsageKey(), DiagnosticLoggingKeys::memoryUsageToDiagnosticLoggingKey(memoryUsage.value()), ShouldSample::No);
}

void PerformanceMonitor::measurePostBackgroundingCPUUsage()
{
    if (!m_page.isOnlyNonUtilityPage()) {
        m_postBackgroundingCPUTime = std::nullopt;
        return;
    }

    if (!m_postBackgroundingCPUTime) {
        m_postBackgroundingCPUTime = getCPUTime();
        if (m_postBackgroundingCPUTime)
            m_postBackgroundingCPUUsageTimer.startOneShot(backgroundCPUUsageMeasurementDuration);
        return;
    }
    std::optional<CPUTime> cpuTime = getCPUTime();
    if (!cpuTime)
        return;

    double cpuUsage = cpuTime.value().percentageCPUUsageSince(*m_postBackgroundingCPUTime);
    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "measurePostBackgroundingCPUUsage: Process was using %.1f%% CPU after becoming non visible.", cpuUsage);
    m_page.diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::postPageBackgroundingCPUUsageKey(), DiagnosticLoggingKeys::backgroundCPUUsageToDiagnosticLoggingKey(cpuUsage), ShouldSample::No);
}

void PerformanceMonitor::measurePerActivityStateCPUUsage()
{
    measureCPUUsageInActivityState(activityStateForCPUSampling(m_page.activityState()));
}

#if !RELEASE_LOG_DISABLED

static inline const char* stringForCPUSamplingActivityState(ActivityStateForCPUSampling activityState)
{
    switch (activityState) {
    case ActivityStateForCPUSampling::NonVisible:
        return "NonVisible";
    case ActivityStateForCPUSampling::VisibleNonActive:
        return "VisibleNonActive";
    case ActivityStateForCPUSampling::VisibleAndActive:
        return "VisibleAndActive";
    }
}

#endif

void PerformanceMonitor::measureCPUUsageInActivityState(ActivityStateForCPUSampling activityState)
{
    if (!m_page.isOnlyNonUtilityPage()) {
        m_perActivityStateCPUTime = std::nullopt;
        return;
    }

    if (!m_perActivityStateCPUTime) {
        m_perActivityStateCPUTime = getCPUTime();
        return;
    }

    std::optional<CPUTime> cpuTime = getCPUTime();
    if (!cpuTime) {
        m_perActivityStateCPUTime = std::nullopt;
        return;
    }

#if !RELEASE_LOG_DISABLED
    double cpuUsage = cpuTime.value().percentageCPUUsageSince(*m_perActivityStateCPUTime);
    RELEASE_LOG_IF_ALLOWED(PerformanceLogging, "measureCPUUsageInActivityState: Process is using %.1f%% CPU in state: %s", cpuUsage, stringForCPUSamplingActivityState(activityState));
#endif
    m_page.chrome().client().reportProcessCPUTime((cpuTime.value().systemTime + cpuTime.value().userTime) - (m_perActivityStateCPUTime.value().systemTime + m_perActivityStateCPUTime.value().userTime), activityState);

    m_perActivityStateCPUTime = WTFMove(cpuTime);
}

} // namespace WebCore
