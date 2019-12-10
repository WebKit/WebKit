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
#include "DisplayRefreshMonitorManager.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorClient.h"
#include "Logging.h"

namespace WebCore {

DisplayRefreshMonitorManager::~DisplayRefreshMonitorManager() = default;

DisplayRefreshMonitorManager& DisplayRefreshMonitorManager::sharedManager()
{
    static NeverDestroyed<DisplayRefreshMonitorManager> manager;
    return manager.get();
}

DisplayRefreshMonitor* DisplayRefreshMonitorManager::createMonitorForClient(DisplayRefreshMonitorClient& client)
{
    PlatformDisplayID clientDisplayID = client.displayID();
    if (auto* existingMonitor = monitorForDisplayID(clientDisplayID)) {
        existingMonitor->addClient(client);
        return existingMonitor;
    }

    auto monitor = DisplayRefreshMonitor::create(client);
    if (!monitor)
        return nullptr;

    LOG(RequestAnimationFrame, "DisplayRefreshMonitorManager::createMonitorForClient() - created monitor %p", monitor.get());
    monitor->addClient(client);
    DisplayRefreshMonitor* result = monitor.get();
    m_monitors.append({ WTFMove(monitor) });
    return result;
}

void DisplayRefreshMonitorManager::registerClient(DisplayRefreshMonitorClient& client)
{
    if (!client.hasDisplayID())
        return;

    createMonitorForClient(client);
}

void DisplayRefreshMonitorManager::unregisterClient(DisplayRefreshMonitorClient& client)
{
    if (!client.hasDisplayID())
        return;

    PlatformDisplayID clientDisplayID = client.displayID();
    auto index = findMonitorForDisplayID(clientDisplayID);
    if (index == notFound)
        return;
    RefPtr<DisplayRefreshMonitor> monitor = m_monitors[index].monitor;
    if (monitor->removeClient(client)) {
        if (!monitor->hasClients())
            m_monitors.remove(index);
    }
}

bool DisplayRefreshMonitorManager::scheduleAnimation(DisplayRefreshMonitorClient& client)
{
    if (!client.hasDisplayID())
        return false;

    DisplayRefreshMonitor* monitor = createMonitorForClient(client);
    if (!monitor)
        return false;

    client.setIsScheduled(true);
    return monitor->requestRefreshCallback();
}

void DisplayRefreshMonitorManager::displayDidRefresh(DisplayRefreshMonitor& monitor)
{
    if (!monitor.shouldBeTerminated())
        return;
    LOG(RequestAnimationFrame, "DisplayRefreshMonitorManager::displayDidRefresh() - destroying monitor %p", &monitor);

    m_monitors.removeFirstMatching([&](auto& monitorWrapper) {
        return monitorWrapper.monitor == &monitor;
    });
}

void DisplayRefreshMonitorManager::windowScreenDidChange(PlatformDisplayID displayID, DisplayRefreshMonitorClient& client)
{
    if (client.hasDisplayID() && client.displayID() == displayID)
        return;
    
    unregisterClient(client);
    client.setDisplayID(displayID);
    registerClient(client);
    if (client.isScheduled())
        scheduleAnimation(client);
}

void DisplayRefreshMonitorManager::displayWasUpdated(PlatformDisplayID displayID)
{
    auto* monitor = monitorForDisplayID(displayID);
    if (monitor && monitor->hasRequestedRefreshCallback())
        monitor->displayLinkFired();
}

size_t DisplayRefreshMonitorManager::findMonitorForDisplayID(PlatformDisplayID displayID) const
{
    return m_monitors.findMatching([&](auto& monitorWrapper) {
        return monitorWrapper.monitor->displayID() == displayID;
    });
}

DisplayRefreshMonitor* DisplayRefreshMonitorManager::monitorForDisplayID(PlatformDisplayID displayID) const
{
    auto index = findMonitorForDisplayID(displayID);
    return index == notFound ? nullptr : m_monitors[index].monitor.get();
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
