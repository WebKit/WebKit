/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

#include "DrawingAreaMessages.h"
#include "WebProcessProxy.h"
#include <wtf/ProcessPrivilege.h>

namespace WebKit {
    
DisplayLink::DisplayLink(WebCore::PlatformDisplayID displayID, WebProcessProxy& webProcessProxy)
    : m_connection(webProcessProxy.connection())
    , m_displayID(displayID)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID, &m_displayLink);
    if (error) {
        WTFLogAlways("Could not create a display link: %d", error);
        return;
    }
    
    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
    if (error) {
        WTFLogAlways("Could not set the display link output callback: %d", error);
        return;
    }

    error = CVDisplayLinkStart(m_displayLink);
    if (error)
        WTFLogAlways("Could not start the display link: %d", error);
}

DisplayLink::~DisplayLink()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(m_displayLink);
    if (!m_displayLink)
        return;

    CVDisplayLinkStop(m_displayLink);
    CVDisplayLinkRelease(m_displayLink);
}

void DisplayLink::addObserver(unsigned observerID)
{
    m_observers.add(observerID);
}

void DisplayLink::removeObserver(unsigned observerID)
{
    m_observers.remove(observerID);
}

bool DisplayLink::hasObservers() const
{
    return !m_observers.isEmpty();
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    DisplayLink* displayLink = static_cast<DisplayLink*>(data);
    displayLink->m_connection->send(Messages::WebProcess::DisplayWasRefreshed(displayLink->m_displayID), 0);
    return kCVReturnSuccess;
}

}

#endif
