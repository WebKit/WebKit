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

#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorClient.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

DisplayRefreshMonitorManager::~DisplayRefreshMonitorManager() = default;

DisplayRefreshMonitorManager& DisplayRefreshMonitorManager::sharedManager()
{
    static NeverDestroyed<DisplayRefreshMonitorManager> manager;
    return manager.get();
}

DisplayRefreshMonitor* DisplayRefreshMonitorManager::monitorForClient(DisplayRefreshMonitorClient& client)
{
    if (!client.hasDisplayID())
        return nullptr;

    PlatformDisplayID clientDisplayID = client.displayID();
    if (auto* existingMonitor = monitorForDisplayID(clientDisplayID)) {
        existingMonitor->addClient(client);
        return existingMonitor;
    }

    auto monitor = DisplayRefreshMonitor::create(client.displayRefreshMonitorFactory(), clientDisplayID);
    if (!monitor)
        return nullptr;

    LOG_WITH_STREAM(DisplayLink, stream << "DisplayRefreshMonitorManager::monitorForClient() - created monitor " << monitor.get() << " for display " << clientDisplayID);
    monitor->addClient(client);
    DisplayRefreshMonitor* result = monitor.get();
    m_monitors.append({ WTFMove(monitor) });
    return result;
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

void DisplayRefreshMonitorManager::setPreferredFramesPerSecond(DisplayRefreshMonitorClient& client, FramesPerSecond preferredFramesPerSecond)
{
    if (auto* monitor = monitorForClient(client))
        monitor->setPreferredFramesPerSecond(preferredFramesPerSecond);
}

bool DisplayRefreshMonitorManager::scheduleAnimation(DisplayRefreshMonitorClient& client)
{
    if (auto* monitor = monitorForClient(client)) {
        client.setIsScheduled(true);
        return monitor->requestRefreshCallback();
    }
    return false;
}

void DisplayRefreshMonitorManager::displayDidRefresh(DisplayRefreshMonitor& monitor)
{
    if (!monitor.shouldBeTerminated())
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "DisplayRefreshMonitorManager::displayDidRefresh() - destroying monitor " << &monitor);

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
