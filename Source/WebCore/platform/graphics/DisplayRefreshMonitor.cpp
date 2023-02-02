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
#include "DisplayRefreshMonitor.h"

#include "DisplayRefreshMonitorClient.h"
#include "DisplayRefreshMonitorFactory.h"
#include "DisplayRefreshMonitorManager.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

#if PLATFORM(IOS_FAMILY)
#include "DisplayRefreshMonitorIOS.h"
#elif PLATFORM(MAC)
#include "LegacyDisplayRefreshMonitorMac.h"
#elif PLATFORM(GTK)
#include "DisplayRefreshMonitorGtk.h"
#elif PLATFORM(WIN)
#include "DisplayRefreshMonitorWin.h"
#endif

namespace WebCore {

RefPtr<DisplayRefreshMonitor> DisplayRefreshMonitor::createDefaultDisplayRefreshMonitor(PlatformDisplayID displayID)
{
#if PLATFORM(MAC)
    return LegacyDisplayRefreshMonitorMac::create(displayID);
#endif
#if PLATFORM(IOS_FAMILY)
    return DisplayRefreshMonitorIOS::create(displayID);
#endif
#if PLATFORM(GTK) && !USE(GTK4)
    return DisplayRefreshMonitorGtk::create(displayID);
#endif
#if PLATFORM(WIN)
    return DisplayRefreshMonitorWin::create(displayID);
#endif
    UNUSED_PARAM(displayID);
    return nullptr;
}

RefPtr<DisplayRefreshMonitor> DisplayRefreshMonitor::create(DisplayRefreshMonitorFactory* factory, PlatformDisplayID displayID)
{
    if (factory) {
        auto monitor = factory->createDisplayRefreshMonitor(displayID);
        if (monitor)
            return monitor;
    }
    
    return createDefaultDisplayRefreshMonitor(displayID);
}

DisplayRefreshMonitor::DisplayRefreshMonitor(PlatformDisplayID displayID)
    : m_displayID(displayID)
{
}

DisplayRefreshMonitor::~DisplayRefreshMonitor() = default;

void DisplayRefreshMonitor::stop()
{
    stopNotificationMechanism();

    Locker locker { m_lock };
    setIsScheduled(false);
}

void DisplayRefreshMonitor::addClient(DisplayRefreshMonitorClient& client)
{
    auto addResult = m_clients.add(&client);
    if (addResult.isNewEntry) {
        LOG_WITH_STREAM(DisplayLink, stream << "[Web] DisplayRefreshMonitor " << this << " addedClient - displayID " << m_displayID << " client " << &client << " client preferred fps " << client.preferredFramesPerSecond());
        computeMaxPreferredFramesPerSecond();
    }
}

bool DisplayRefreshMonitor::removeClient(DisplayRefreshMonitorClient& client)
{
    if (m_clientsToBeNotified)
        m_clientsToBeNotified->remove(&client);

    bool removed = m_clients.remove(&client);
    if (removed) {
        LOG_WITH_STREAM(DisplayLink, stream << "[Web] DisplayRefreshMonitor " << this << " removedClient " << &client);
        computeMaxPreferredFramesPerSecond();
    }

    return removed;
}

std::optional<FramesPerSecond> DisplayRefreshMonitor::maximumClientPreferredFramesPerSecond() const
{
    std::optional<FramesPerSecond> maxFramesPerSecond;
    for (auto* client : m_clients)
        maxFramesPerSecond = std::max<FramesPerSecond>(maxFramesPerSecond.value_or(0), client->preferredFramesPerSecond());

    return maxFramesPerSecond;
}

void DisplayRefreshMonitor::computeMaxPreferredFramesPerSecond()
{
    auto maxFramesPerSecond = maximumClientPreferredFramesPerSecond();
    LOG_WITH_STREAM(DisplayLink, stream << "[Web] DisplayRefreshMonitor " << this << " computeMaxPreferredFramesPerSecond - displayID " << m_displayID << " adjusting max fps to " << maxFramesPerSecond);
    if (maxFramesPerSecond != m_maxClientPreferredFramesPerSecond) {
        m_maxClientPreferredFramesPerSecond = maxFramesPerSecond;
        if (m_maxClientPreferredFramesPerSecond)
            adjustPreferredFramesPerSecond(*m_maxClientPreferredFramesPerSecond);
    }
}

void DisplayRefreshMonitor::clientPreferredFramesPerSecondChanged(DisplayRefreshMonitorClient&)
{
    computeMaxPreferredFramesPerSecond();
}

bool DisplayRefreshMonitor::requestRefreshCallback()
{
    Locker locker { m_lock };
    
    if (isScheduled())
        return true;

    if (!startNotificationMechanism())
        return false;

    setIsScheduled(true);
    return true;
}

bool DisplayRefreshMonitor::firedAndReachedMaxUnscheduledFireCount()
{
    if (isScheduled()) {
        m_unscheduledFireCount = 0;
        return false;
    }

    ++m_unscheduledFireCount;
    return m_unscheduledFireCount > m_maxUnscheduledFireCount;
}

void DisplayRefreshMonitor::displayLinkFired(const DisplayUpdate& displayUpdate)
{
    {
        Locker locker { m_lock };

        // This may be off the main thread.
        if (!isPreviousFrameDone()) {
            RELEASE_LOG(DisplayLink, "[Web] DisplayRefreshMonitor::displayLinkFired for display %u - previous frame is not complete", displayID());
            return;
        }

        LOG_WITH_STREAM(DisplayLink, stream << "[Web] DisplayRefreshMonitor::displayLinkFired for display " << displayID() << " - scheduled " << isScheduled() << " unscheduledFireCount " << m_unscheduledFireCount << " of " << m_maxUnscheduledFireCount);
        if (firedAndReachedMaxUnscheduledFireCount()) {
            stopNotificationMechanism();
            return;
        }

        setIsScheduled(false);
        setIsPreviousFrameDone(false);
    }
    dispatchDisplayDidRefresh(displayUpdate);
}

void DisplayRefreshMonitor::dispatchDisplayDidRefresh(const DisplayUpdate& displayUpdate)
{
    ASSERT(isMainThread());
    displayDidRefresh(displayUpdate);
}

void DisplayRefreshMonitor::displayDidRefresh(const DisplayUpdate& displayUpdate)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(displayUpdate);
    LOG_WITH_STREAM(DisplayLink, stream << "DisplayRefreshMonitor::displayDidRefresh for display " << displayID() << " update " << displayUpdate);

    // The call back can cause all our clients to be unregistered, so we need to protect
    // against deletion until the end of the method.
    Ref<DisplayRefreshMonitor> protectedThis(*this);

    // Copy the hash table and remove clients from it one by one so we don't notify
    // any client twice, but can respond to removal of clients during the delivery process.
    HashSet<DisplayRefreshMonitorClient*> clientsToBeNotified = m_clients;
    m_clientsToBeNotified = &clientsToBeNotified;
    while (!clientsToBeNotified.isEmpty()) {
        DisplayRefreshMonitorClient* client = clientsToBeNotified.takeAny();
        client->fireDisplayRefreshIfNeeded(displayUpdate);

        // This checks if this function was reentered. In that case, stop iterating
        // since it's not safe to use the set any more.
        if (m_clientsToBeNotified != &clientsToBeNotified)
            break;
    }

    if (m_clientsToBeNotified == &clientsToBeNotified)
        m_clientsToBeNotified = nullptr;

    {
        Locker locker { m_lock };
        setIsPreviousFrameDone(true);
    }

    DisplayRefreshMonitorManager::sharedManager().displayDidRefresh(*this);
}

}
