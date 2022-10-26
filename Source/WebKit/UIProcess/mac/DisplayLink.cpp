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

#if HAVE(CVDISPLAYLINK)

#include "Logging.h"
#include <WebCore/AnimationFrameRate.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

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

void DisplayLink::addObserver(Client& client, DisplayLinkObserverID observerID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    ASSERT(RunLoop::isMain());

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display display " << m_displayID << " add observer " << observerID << " fps " << preferredFramesPerSecond);

    {
        Locker locker { m_clientsLock };
        m_clients.ensure(client, [] {
            return ClientInfo { };
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

void DisplayLink::removeObserver(Client& client, DisplayLinkObserverID observerID)
{
    ASSERT(RunLoop::isMain());

    Locker locker { m_clientsLock };

    auto it = m_clients.find(client);
    if (it == m_clients.end())
        return;

    auto& clientInfo = it->value;

    bool removed = clientInfo.observers.removeFirstMatching([observerID](const auto& value) {
        return value.observerID == observerID;
    });
    ASSERT_UNUSED(removed, removed);

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display " << m_displayID << " remove observer " << observerID);

    removeInfoForClientIfUnused(client);

    // We do not stop the display link right away when |m_clients| becomes empty. Instead, we
    // let the display link fire up to |maxFireCountWithoutObservers| times without observers to avoid
    // killing & restarting too many threads when observers gets removed & added in quick succession.
}

void DisplayLink::removeClient(Client& client)
{
    ASSERT(RunLoop::isMain());

    Locker locker { m_clientsLock };
    m_clients.remove(client);

    // We do not stop the display link right away when |m_clients| becomes empty. Instead, we
    // let the display link fire up to |maxFireCountWithoutObservers| times without observers to avoid
    // killing & restarting too many threads when observers gets removed & added in quick succession.
}

bool DisplayLink::removeInfoForClientIfUnused(Client& client)
{
    auto it = m_clients.find(client);
    if (it == m_clients.end())
        return false;

    auto& clientInfo = it->value;
    if (clientInfo.observers.isEmpty() && !clientInfo.fullSpeedUpdatesClientCount) {
        m_clients.remove(it);
        return true;
    }
    return false;
}

void DisplayLink::incrementFullSpeedRequestClientCount(Client& client)
{
    Locker locker { m_clientsLock };

    auto& clientInfo = m_clients.ensure(client, [] {
        return ClientInfo { };
    }).iterator->value;

    ++clientInfo.fullSpeedUpdatesClientCount;
}

void DisplayLink::decrementFullSpeedRequestClientCount(Client& client)
{
    Locker locker { m_clientsLock };

    auto it = m_clients.find(client);
    if (it == m_clients.end())
        return;

    auto& clientInfo = it->value;
    ASSERT(clientInfo.fullSpeedUpdatesClientCount);
    --clientInfo.fullSpeedUpdatesClientCount;
    removeInfoForClientIfUnused(client);
}

void DisplayLink::setObserverPreferredFramesPerSecond(Client& client, DisplayLinkObserverID observerID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " setPreferredFramesPerSecond - display " << m_displayID << " observer " << observerID << " fps " << preferredFramesPerSecond);

    Locker locker { m_clientsLock };

    auto it = m_clients.find(client);
    if (it == m_clients.end())
        return;

    auto& clientInfo = it->value;
    auto index = clientInfo.observers.findIf([observerID](const auto& observer) {
        return observer.observerID == observerID;
    });

    if (index != notFound)
        clientInfo.observers[index].preferredFramesPerSecond = preferredFramesPerSecond;
}

CVReturn DisplayLink::displayLinkCallback(CVDisplayLinkRef displayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* data)
{
    static_cast<DisplayLink*>(data)->notifyObserversDisplayWasRefreshed();
    return kCVReturnSuccess;
}

void DisplayLink::notifyObserversDisplayWasRefreshed()
{
    ASSERT(!RunLoop::isMain());

    Locker locker { m_clientsLock };

    auto maxFramesPerSecond = [](const Vector<ObserverInfo>& observers) {
        std::optional<WebCore::FramesPerSecond> observersMaxFramesPerSecond;
        for (const auto& observer : observers)
            observersMaxFramesPerSecond = std::max(observersMaxFramesPerSecond.value_or(0), observer.preferredFramesPerSecond);
        return observersMaxFramesPerSecond;
    };

    bool anyConnectionHadObservers = false;
    for (auto& [client, clientInfo] : m_clients) {
        if (clientInfo.observers.isEmpty())
            continue;

        anyConnectionHadObservers = true;

        auto observersMaxFramesPerSecond = maxFramesPerSecond(clientInfo.observers);
        bool anyObserverWantsCallback = m_currentUpdate.relevantForUpdateFrequency(observersMaxFramesPerSecond.value_or(WebCore::FullSpeedFramesPerSecond));

        LOG_WITH_STREAM(DisplayLink, stream << "[UI ] DisplayLink " << this << " for display " << m_displayID << " (display fps " << m_displayNominalFramesPerSecond << ") update " << m_currentUpdate << " " << clientInfo.observers.size()
            << " observers, maxFramesPerSecond " << observersMaxFramesPerSecond << " full speed client count " << clientInfo.fullSpeedUpdatesClientCount << " relevant " << anyObserverWantsCallback);

        if (clientInfo.fullSpeedUpdatesClientCount || anyObserverWantsCallback)
            client->displayLinkFired(m_displayID, m_currentUpdate, clientInfo.fullSpeedUpdatesClientCount, anyObserverWantsCallback);
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

DisplayLink* DisplayLinkCollection::displayLinkForDisplay(WebCore::PlatformDisplayID displayID) const
{
    for (auto& displayLink : m_displayLinks) {
        if (displayLink->displayID() == displayID)
            return displayLink.get();
    }

    return nullptr;
}

void DisplayLinkCollection::add(std::unique_ptr<DisplayLink>&& displayLink)
{
    ASSERT(!m_displayLinks.containsIf([&](auto &entry) { return entry->displayID() == displayLink->displayID(); }));
    m_displayLinks.append(WTFMove(displayLink));
}

} // namespace WebKit

#endif // HAVE(CVDISPLAYLINK)
