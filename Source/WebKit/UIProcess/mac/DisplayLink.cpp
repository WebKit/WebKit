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

#if HAVE(CVDISPLAYLINK)

#include "EventDispatcherMessages.h"
#include "Logging.h"
#include "WebProcessMessages.h"
#include <WebCore/AnimationFrameRate.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

bool DisplayLink::shouldSendIPCOnBackgroundQueue { false };
constexpr unsigned maxFireCountWithoutObservers { 20 };
    
DisplayLink::DisplayLink(WebCore::PlatformDisplayID displayID)
    : m_displayID(displayID)
{
    // FIXME: We can get here with displayID == 0 (webkit.org/b/212120), in which case CVDisplayLinkCreateWithCGDisplay()
    // probably defaults to the main screen.
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID, &m_displayLink);
    if (error) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a display link for display %u: error %d", displayID, error);
        return;
    }
    
    error = CVDisplayLinkSetOutputCallback(m_displayLink, displayLinkCallback, this);
    if (error) {
        RELEASE_LOG_FAULT(DisplayLink, "DisplayLink: Could not set the display link output callback for display %u: error %d", displayID, error);
        return;
    }

    m_displayNominalFramesPerSecond = nominalFramesPerSecondFromDisplayLink(m_displayLink);

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] Created DisplayLink " << this << " for display " << displayID << " with nominal fps " << m_displayNominalFramesPerSecond);
}

DisplayLink::~DisplayLink()
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] Destroying DisplayLink " << this << " for display " << m_displayID);

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(m_displayLink);
    if (!m_displayLink)
        return;

    CVDisplayLinkStop(m_displayLink);
    CVDisplayLinkRelease(m_displayLink);
}

WebCore::FramesPerSecond DisplayLink::nominalFramesPerSecondFromDisplayLink(CVDisplayLinkRef displayLink)
{
    CVTime refreshPeriod = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink);
    if (!refreshPeriod.timeValue)
        return WebCore::FullSpeedFramesPerSecond;

    WebCore::FramesPerSecond result = round((double)refreshPeriod.timeScale / (double)refreshPeriod.timeValue);
    return result ?: WebCore::FullSpeedFramesPerSecond;
}

void DisplayLink::addObserver(IPC::Connection& connection, DisplayLinkObserverID observerID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    ASSERT(RunLoop::isMain());

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display display " << m_displayID << " add observer " << observerID << " fps " << preferredFramesPerSecond);

    {
        Locker locker { m_observersLock };
        m_observers.ensure(connection.uniqueID(), [] {
            return ConnectionClientInfo { };
        }).iterator->value.observers.append({ observerID, preferredFramesPerSecond });
    }

    if (!CVDisplayLinkIsRunning(m_displayLink)) {
        LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink for display " << m_displayID << " starting CVDisplayLink with fps " << m_displayNominalFramesPerSecond);

        m_currentUpdate = { 0, m_displayNominalFramesPerSecond };

        CVReturn error = CVDisplayLinkStart(m_displayLink);
        if (error)
            RELEASE_LOG_FAULT(DisplayLink, "DisplayLink: Could not start the display link: %d", error);
    }
}

void DisplayLink::removeObserver(IPC::Connection& connection, DisplayLinkObserverID observerID)
{
    ASSERT(RunLoop::isMain());

    Locker locker { m_observersLock };

    auto it = m_observers.find(connection.uniqueID());
    if (it == m_observers.end())
        return;

    auto& connectionInfo = it->value;

    bool removed = connectionInfo.observers.removeFirstMatching([observerID](const auto& value) {
        return value.observerID == observerID;
    });
    ASSERT_UNUSED(removed, removed);

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display " << m_displayID << " remove observer " << observerID);

    removeInfoForConnectionIfPossible(connection);

    // We do not stop the display link right away when |m_observers| becomes empty. Instead, we
    // let the display link fire up to |maxFireCountWithoutObservers| times without observers to avoid
    // killing & restarting too many threads when observers gets removed & added in quick succession.
}

void DisplayLink::removeObservers(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    Locker locker { m_observersLock };
    m_observers.remove(connection.uniqueID());

    // We do not stop the display link right away when |m_observers| becomes empty. Instead, we
    // let the display link fire up to |maxFireCountWithoutObservers| times without observers to avoid
    // killing & restarting too many threads when observers gets removed & added in quick succession.
}

void DisplayLink::removeInfoForConnectionIfPossible(IPC::Connection& connection)
{
    auto it = m_observers.find(connection.uniqueID());
    if (it == m_observers.end())
        return;

    auto& connectionInfo = it->value;
    if (connectionInfo.observers.isEmpty() && !connectionInfo.fullSpeedUpdatesClientCount)
        m_observers.remove(it);
}

void DisplayLink::incrementFullSpeedRequestClientCount(IPC::Connection& connection)
{
    Locker locker { m_observersLock };

    auto& connectionInfo = m_observers.ensure(connection.uniqueID(), [] {
        return ConnectionClientInfo { };
    }).iterator->value;

    ++connectionInfo.fullSpeedUpdatesClientCount;
}

void DisplayLink::decrementFullSpeedRequestClientCount(IPC::Connection& connection)
{
    Locker locker { m_observersLock };

    auto it = m_observers.find(connection.uniqueID());
    if (it == m_observers.end())
        return;

    auto& connectionInfo = it->value;
    ASSERT(connectionInfo.fullSpeedUpdatesClientCount);
    --connectionInfo.fullSpeedUpdatesClientCount;
    removeInfoForConnectionIfPossible(connection);
}

void DisplayLink::setPreferredFramesPerSecond(IPC::Connection& connection, DisplayLinkObserverID observerID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " setPreferredFramesPerSecond - display " << m_displayID << " observer " << observerID << " fps " << preferredFramesPerSecond);

    Locker locker { m_observersLock };

    auto it = m_observers.find(connection.uniqueID());
    if (it == m_observers.end())
        return;

    auto& connectionInfo = it->value;
    auto index = connectionInfo.observers.findMatching([observerID](const auto& observer) {
        return observer.observerID == observerID;
    });

    if (index != notFound)
        connectionInfo.observers[index].preferredFramesPerSecond = preferredFramesPerSecond;
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    static_cast<DisplayLink*>(data)->notifyObserversDisplayWasRefreshed();
    return kCVReturnSuccess;
}

void DisplayLink::notifyObserversDisplayWasRefreshed()
{
    ASSERT(!RunLoop::isMain());

    Locker locker { m_observersLock };

    auto maxFramesPerSecond = [](const Vector<ObserverInfo>& observers) {
        std::optional<WebCore::FramesPerSecond> observersMaxFramesPerSecond;
        for (const auto& observer : observers)
            observersMaxFramesPerSecond = std::max(observersMaxFramesPerSecond.value_or(0), observer.preferredFramesPerSecond);
        return observersMaxFramesPerSecond;
    };

    bool anyConnectionHadObservers = false;
    for (auto& [connectionID, connectionInfo] : m_observers) {
        if (connectionInfo.observers.isEmpty())
            continue;

        anyConnectionHadObservers = true;

        auto observersMaxFramesPerSecond = maxFramesPerSecond(connectionInfo.observers);
        auto mainThreadWantsUpdate = m_currentUpdate.relevantForUpdateFrequency(observersMaxFramesPerSecond.value_or(WebCore::FullSpeedFramesPerSecond));

        LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display " << m_displayID << " (display fps " << m_displayNominalFramesPerSecond << ") update " << m_currentUpdate << " " << connectionInfo.observers.size()
            << " observers, on background queue " << shouldSendIPCOnBackgroundQueue << " maxFramesPerSecond " << observersMaxFramesPerSecond << " full speed clients " << connectionInfo.fullSpeedUpdatesClientCount << " relevant " << mainThreadWantsUpdate);

        if (connectionInfo.fullSpeedUpdatesClientCount) {
            IPC::Connection::send(connectionID, Messages::EventDispatcher::DisplayWasRefreshed(m_displayID, m_currentUpdate, mainThreadWantsUpdate), 0, { }, Thread::QOS::UserInteractive);
        } else if (mainThreadWantsUpdate)
            IPC::Connection::send(connectionID, Messages::WebProcess::DisplayWasRefreshed(m_displayID, m_currentUpdate), 0, { }, Thread::QOS::UserInteractive);
    }

    m_currentUpdate = m_currentUpdate.nextUpdate();

    if (!anyConnectionHadObservers) {
        if (++m_fireCountWithoutObservers >= maxFireCountWithoutObservers) {
            LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink for display " << m_displayID << " fired " << m_fireCountWithoutObservers << " times with no observers; stopping CVDisplayLink");
            CVDisplayLinkStop(m_displayLink);
        }
        return;
    }
    m_fireCountWithoutObservers = 0;
}

} // namespace WebKit

#endif // HAVE(CVDISPLAYLINK)
