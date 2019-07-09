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
#include "DisplayRefreshMonitorMac.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(MAC)

#include <QuartzCore/QuartzCore.h>
#include <wtf/RunLoop.h>

namespace WebCore {

DisplayRefreshMonitorMac::DisplayRefreshMonitorMac(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
{
}

DisplayRefreshMonitorMac::~DisplayRefreshMonitorMac()
{
    ASSERT(!m_displayLink);
}

void DisplayRefreshMonitorMac::stop()
{
    if (!m_displayLink)
        return;

    CVDisplayLinkStop(m_displayLink);
    CVDisplayLinkRelease(m_displayLink);
    m_displayLink = nullptr;
}

static CVReturn displayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    DisplayRefreshMonitorMac* monitor = static_cast<DisplayRefreshMonitorMac*>(data);
    monitor->displayLinkFired();
    return kCVReturnSuccess;
}

bool DisplayRefreshMonitorMac::requestRefreshCallback()
{
    if (!isActive())
        return false;

    if (!m_displayLink) {
        setIsActive(false);
        CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID(), &m_displayLink);
        if (error)
            return false;

        error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
        if (error)
            return false;

        error = CVDisplayLinkStart(m_displayLink);
        if (error)
            return false;

        setIsActive(true);
    }

    LockHolder lock(mutex());
    setIsScheduled(true);
    return true;
}

void DisplayRefreshMonitorMac::displayLinkFired()
{
    LockHolder lock(mutex());
    if (!isPreviousFrameDone())
        return;

    setIsPreviousFrameDone(false);

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] {
        if (m_displayLink)
            handleDisplayRefreshedNotificationOnMainThread(this);
    });
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(MAC)
