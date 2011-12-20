/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "DisplayRefreshMonitor.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

DisplayRefreshMonitorClient::DisplayRefreshMonitorClient()
    : m_scheduled(false)
    , m_displayIDIsSet(false)
{
}

DisplayRefreshMonitorClient::~DisplayRefreshMonitorClient()
{
    DisplayRefreshMonitorManager::sharedManager()->unregisterClient(this);
}

void DisplayRefreshMonitorClient::fireDisplayRefreshIfNeeded(double timestamp)
{
    if (m_scheduled) {
        m_scheduled = false;
        displayRefreshFired(timestamp);
    }
}

DisplayRefreshMonitor::DisplayRefreshMonitor(PlatformDisplayID displayID)
    : m_timestamp(0)
    , m_active(true)
    , m_scheduled(false)
    , m_previousFrameDone(true)
    , m_displayID(displayID)
#if PLATFORM(MAC)
    , m_displayLink(0)
#endif
{
}

void DisplayRefreshMonitor::refreshDisplayOnMainThread(void* data)
{
    DisplayRefreshMonitor* monitor = static_cast<DisplayRefreshMonitor*>(data);
    monitor->notifyClients();
}

void DisplayRefreshMonitor::notifyClients()
{
    double timestamp;
    {
        MutexLocker lock(m_mutex);
        m_scheduled = false;
        timestamp = m_timestamp;
    }

    for (size_t i = 0; i < m_clients.size(); ++i)
        m_clients[i]->fireDisplayRefreshIfNeeded(timestamp);

    {
        MutexLocker lock(m_mutex);
        m_previousFrameDone = true;
    }
}

DisplayRefreshMonitorManager* DisplayRefreshMonitorManager::sharedManager()
{
    DEFINE_STATIC_LOCAL(DisplayRefreshMonitorManager, manager, ());
    return &manager;
}

size_t DisplayRefreshMonitorManager::findMonitor(PlatformDisplayID displayID) const
{
    for (size_t i = 0; i < m_monitors.size(); ++i)
        if (m_monitors[i]->displayID() == displayID)
            return i;
            
    return notFound;
}

void DisplayRefreshMonitorManager::registerClient(DisplayRefreshMonitorClient* client)
{
    if (!client->m_displayIDIsSet)
        return;
        
    size_t index = findMonitor(client->m_displayID);
    DisplayRefreshMonitor* monitor;
    
    if (index == notFound) {
        monitor = new DisplayRefreshMonitor(client->m_displayID);
        m_monitors.append(monitor);
    } else
        monitor = m_monitors[index];
        
    monitor->addClient(client);
}

void DisplayRefreshMonitorManager::unregisterClient(DisplayRefreshMonitorClient* client)
{
    if (!client->m_displayIDIsSet)
        return;
        
    size_t index = findMonitor(client->m_displayID);
    if (index == notFound)
        return;
    
    DisplayRefreshMonitor* monitor = m_monitors[index];
    if (monitor->removeClient(client)) {
        if (!monitor->hasClients()) {
            m_monitors.remove(index);
            delete monitor;
        }
    }
}

bool DisplayRefreshMonitorManager::scheduleAnimation(DisplayRefreshMonitorClient* client)
{
    if (!client->m_displayIDIsSet)
        return false;
        
    size_t i = findMonitor(client->m_displayID);
    ASSERT(i != notFound);

    client->m_scheduled = true;
    return m_monitors[i]->requestRefreshCallback();
}

void DisplayRefreshMonitorManager::windowScreenDidChange(PlatformDisplayID displayID, DisplayRefreshMonitorClient* client)
{
    if (client->m_displayIDIsSet && client->m_displayID == displayID)
        return;
    
    unregisterClient(client);
    client->setDisplayID(displayID);
    registerClient(client);
    if (client->m_scheduled)
        scheduleAnimation(client);
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
