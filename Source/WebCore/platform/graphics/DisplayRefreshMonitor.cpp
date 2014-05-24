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

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "DisplayRefreshMonitorClient.h"
#include "DisplayRefreshMonitorIOS.h"
#include "DisplayRefreshMonitorMac.h"
#include "DisplayRefreshMonitorManager.h"

namespace WebCore {

PassRefPtr<DisplayRefreshMonitor> DisplayRefreshMonitor::create(DisplayRefreshMonitorClient* client)
{
    PlatformDisplayID displayID = client->displayID();

    if (RefPtr<DisplayRefreshMonitor> monitor = client->createDisplayRefreshMonitor(displayID))
        return monitor.release();

#if PLATFORM(MAC)
    return DisplayRefreshMonitorMac::create(displayID);
#endif
#if PLATFORM(IOS)
    return DisplayRefreshMonitorIOS::create(displayID);
#endif
}

DisplayRefreshMonitor::DisplayRefreshMonitor(PlatformDisplayID displayID)
    : m_monotonicAnimationStartTime(0)
    , m_active(true)
    , m_scheduled(false)
    , m_previousFrameDone(true)
    , m_unscheduledFireCount(0)
    , m_displayID(displayID)
    , m_clientsToBeNotified(nullptr)
{
}

DisplayRefreshMonitor::~DisplayRefreshMonitor()
{
}

void DisplayRefreshMonitor::handleDisplayRefreshedNotificationOnMainThread(void* data)
{
    DisplayRefreshMonitor* monitor = static_cast<DisplayRefreshMonitor*>(data);
    monitor->displayDidRefresh();
}

void DisplayRefreshMonitor::addClient(DisplayRefreshMonitorClient* client)
{
    m_clients.add(client);
}

bool DisplayRefreshMonitor::removeClient(DisplayRefreshMonitorClient* client)
{
    if (m_clientsToBeNotified)
        m_clientsToBeNotified->remove(client);
    return m_clients.remove(client);
}

void DisplayRefreshMonitor::displayDidRefresh()
{
    double monotonicAnimationStartTime;

    {
        MutexLocker lock(m_mutex);
        if (!m_scheduled)
            ++m_unscheduledFireCount;
        else
            m_unscheduledFireCount = 0;

        m_scheduled = false;
        monotonicAnimationStartTime = m_monotonicAnimationStartTime;
    }

    // The call back can cause all our clients to be unregistered, so we need to protect
    // against deletion until the end of the method.
    Ref<DisplayRefreshMonitor> protect(*this);

    // Copy the hash table and remove clients from it one by one so we don't notify
    // any client twice, but can respond to removal of clients during the delivery process.
    HashSet<DisplayRefreshMonitorClient*> clientsToBeNotified = m_clients;
    m_clientsToBeNotified = &clientsToBeNotified;
    while (!clientsToBeNotified.isEmpty()) {
        DisplayRefreshMonitorClient* client = clientsToBeNotified.takeAny();
        client->fireDisplayRefreshIfNeeded(monotonicAnimationStartTime);

        // This checks if this function was reentered. In that case, stop iterating
        // since it's not safe to use the set any more.
        if (m_clientsToBeNotified != &clientsToBeNotified)
            break;
    }

    if (m_clientsToBeNotified == &clientsToBeNotified)
        m_clientsToBeNotified = nullptr;

    {
        MutexLocker lock(m_mutex);
        m_previousFrameDone = true;
    }
    
    DisplayRefreshMonitorManager::sharedManager().displayDidRefresh(this);
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
