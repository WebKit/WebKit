/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "DisplayLink.h"

#if HAVE(DISPLAY_LINK)

#include "Logging.h"
#include <wtf/ProcessPrivilege.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

using namespace WebCore;

void DisplayLink::platformInitialize()
{
    // FIXME: We can get here with displayID == 0 (webkit.org/b/212120), in which case CVDisplayLinkCreateWithCGDisplay()
    // probably defaults to the main screen.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    CVReturn error = CVDisplayLinkCreateWithCGDisplay(m_displayID, &m_displayLink);
    if (error) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a display link for display %u: error %d", m_displayID, error);
        return;
    }

    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
    if (error) {
        RELEASE_LOG_FAULT(DisplayLink, "DisplayLink: Could not set the display link output callback for display %u: error %d", m_displayID, error);
        return;
    }

    m_displayNominalFramesPerSecond = nominalFramesPerSecondFromDisplayLink(m_displayLink);
}

void DisplayLink::platformFinalize()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(m_displayLink);
    if (!m_displayLink)
        return;

    CVDisplayLinkStop(m_displayLink);
    CVDisplayLinkRelease(m_displayLink);
}

FramesPerSecond DisplayLink::nominalFramesPerSecondFromDisplayLink(CVDisplayLinkRef displayLink)
{
    CVTime refreshPeriod = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink);
    if (!refreshPeriod.timeValue)
        return FullSpeedFramesPerSecond;

    FramesPerSecond result = round((double)refreshPeriod.timeScale / (double)refreshPeriod.timeValue);
    return result ?: FullSpeedFramesPerSecond;
}

bool DisplayLink::platformIsRunning() const
{
    return CVDisplayLinkIsRunning(m_displayLink);
}

void DisplayLink::platformStart()
{
    CVReturn error = CVDisplayLinkStart(m_displayLink);
    if (error)
        RELEASE_LOG_FAULT(DisplayLink, "DisplayLink: Could not start the display link: %d", error);
}

void DisplayLink::platformStop()
{
    CVDisplayLinkStop(m_displayLink);
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    static_cast<DisplayLink*>(data)->notifyObserversDisplayDidRefresh();
    return kCVReturnSuccess;
}

} // namespace WebKit

#endif // HAVE(DISPLAY_LINK)
