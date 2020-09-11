/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "EventDispatcherMessages.h"
#include <wtf/ProcessPrivilege.h>

namespace WebKit {
    
DisplayLink::DisplayLink(WebCore::PlatformDisplayID displayID)
    : m_displayID(displayID)
{
    // FIXME: We can get here with displayID == 0 (webkit.org/b/212120), in which case CVDisplayLinkCreateWithCGDisplay()
    // probably defaults to the main screen.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID, &m_displayLink);
    if (error) {
        WTFLogAlways("Could not create a display link for display %u: error %d", displayID, error);
        return;
    }
    
    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
    if (error) {
        WTFLogAlways("Could not set the display link output callback for display %u: error %d", displayID, error);
        return;
    }
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

Optional<unsigned> DisplayLink::nominalFramesPerSecond() const
{
    CVTime refreshPeriod = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(m_displayLink);
    return round((double)refreshPeriod.timeScale / (double)refreshPeriod.timeValue);
}

void DisplayLink::addObserver(IPC::Connection& connection, DisplayLinkObserverID observerID)
{
    ASSERT(RunLoop::isMain());
    bool isRunning = !m_observers.isEmpty();

    {
        LockHolder locker(m_observersLock);
        m_observers.ensure(&connection, [] {
            return Vector<DisplayLinkObserverID> { };
        }).iterator->value.append(observerID);
    }

    if (!isRunning) {
        CVReturn error = CVDisplayLinkStart(m_displayLink);
        if (error)
            WTFLogAlways("Could not start the display link: %d", error);
    }
}

void DisplayLink::removeObserver(IPC::Connection& connection, DisplayLinkObserverID observerID)
{
    ASSERT(RunLoop::isMain());

    {
        LockHolder locker(m_observersLock);

        auto it = m_observers.find(&connection);
        if (it == m_observers.end())
            return;
        bool removed = it->value.removeFirst(observerID);
        ASSERT_UNUSED(removed, removed);
        if (it->value.isEmpty())
            m_observers.remove(it);
    }

    if (m_observers.isEmpty())
        CVDisplayLinkStop(m_displayLink);
}

void DisplayLink::removeObservers(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    {
        LockHolder locker(m_observersLock);
        m_observers.remove(&connection);
    }

    if (m_observers.isEmpty())
        CVDisplayLinkStop(m_displayLink);
}

bool DisplayLink::hasObservers() const
{
    ASSERT(RunLoop::isMain());
    return !m_observers.isEmpty();
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    auto* displayLink = static_cast<DisplayLink*>(data);
    LockHolder locker(displayLink->m_observersLock);
    for (auto& connection : displayLink->m_observers.keys())
        connection->send(Messages::EventDispatcher::DisplayWasRefreshed(displayLink->m_displayID), 0);
    return kCVReturnSuccess;
}

} // namespace WebKit

#endif // ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
