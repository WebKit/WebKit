/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
#include "LegacyDisplayRefreshMonitorMac.h"

#if PLATFORM(MAC)

#include "Logging.h"
#include "RuntimeApplicationChecks.h"
#include <CoreVideo/CVDisplayLink.h>
#include <wtf/RunLoop.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

constexpr unsigned maxUnscheduledFireCount { 20 };

LegacyDisplayRefreshMonitorMac::LegacyDisplayRefreshMonitorMac(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
{
    ASSERT(!isInWebProcess());
    setMaxUnscheduledFireCount(maxUnscheduledFireCount);
}

LegacyDisplayRefreshMonitorMac::~LegacyDisplayRefreshMonitorMac()
{
    ASSERT(!m_displayLink);
}

void LegacyDisplayRefreshMonitorMac::stop()
{
    DisplayRefreshMonitor::stop();
    LOG_WITH_STREAM(DisplayLink, stream << "LegacyDisplayRefreshMonitorMac::stop for dipslay " << displayID() << " destroying display link");
    CVDisplayLinkRelease(m_displayLink);
    m_displayLink = nullptr;
}

static CVReturn displayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    LegacyDisplayRefreshMonitorMac* monitor = static_cast<LegacyDisplayRefreshMonitorMac*>(data);
    monitor->displayLinkCallbackFired();
    return kCVReturnSuccess;
}

void LegacyDisplayRefreshMonitorMac::displayLinkCallbackFired()
{
    displayLinkFired(m_currentUpdate);
    m_currentUpdate = m_currentUpdate.nextUpdate();
}

void LegacyDisplayRefreshMonitorMac::dispatchDisplayDidRefresh(const DisplayUpdate& displayUpdate)
{
    RunLoop::main().dispatch([this, displayUpdate, protectedThis = makeRef(*this)] {
        if (m_displayLink)
            displayDidRefresh(displayUpdate);
    });
}

WebCore::FramesPerSecond LegacyDisplayRefreshMonitorMac::nominalFramesPerSecondFromDisplayLink(CVDisplayLinkRef displayLink)
{
    CVTime refreshPeriod = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink);
    return round((double)refreshPeriod.timeScale / (double)refreshPeriod.timeValue);
}

bool LegacyDisplayRefreshMonitorMac::ensureDisplayLink()
{
    if (m_displayLink)
        return true;

    auto error = CVDisplayLinkCreateWithCGDisplay(displayID(), &m_displayLink);
    if (error)
        return false;

    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
    if (error)
        return false;
        
    return true;
}

bool LegacyDisplayRefreshMonitorMac::startNotificationMechanism()
{
    if (!m_displayLink) {
        if (!ensureDisplayLink())
            return false;
    }

    if (!m_displayLinkIsActive) {
        LOG_WITH_STREAM(DisplayLink, stream << "LegacyDisplayRefreshMonitorMac::startNotificationMechanism for display " << displayID() << " starting display link");

        auto error = CVDisplayLinkStart(m_displayLink);
        if (error)
            return false;
        
        m_displayLinkIsActive = true;
        m_currentUpdate = { 0, nominalFramesPerSecondFromDisplayLink(m_displayLink) };
    }

    return true;
}

void LegacyDisplayRefreshMonitorMac::stopNotificationMechanism()
{
    if (!m_displayLinkIsActive)
        return;

    if (m_displayLink) {
        LOG_WITH_STREAM(DisplayLink, stream << "LegacyDisplayRefreshMonitorMac::stopNotificationMechanism for display " << displayID() << " stopping display link");
        CVDisplayLinkStop(m_displayLink);
    }
        
    m_displayLinkIsActive = false;
}

std::optional<FramesPerSecond> LegacyDisplayRefreshMonitorMac::displayNominalFramesPerSecond()
{
    if (!ensureDisplayLink())
        return std::nullopt;
        
    return nominalFramesPerSecondFromDisplayLink(m_displayLink);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
