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

DisplayRefreshMonitor* DisplayRefreshMonitorManager::ensureMonitorForDisplayID(PlatformDisplayID displayID, DisplayRefreshMonitorFactory* factory)
{
    if (auto* existingMonitor = monitorForDisplayID(displayID))
        return existingMonitor;

    auto monitor = DisplayRefreshMonitor::create(factory, displayID);
    if (!monitor)
        return nullptr;

    LOG_WITH_STREAM(DisplayLink, stream << "[Web] DisplayRefreshMonitorManager::ensureMonitorForDisplayID() - created monitor " << monitor.get() << " for display " << displayID);
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
    monitor->removeClient(client);
}

void DisplayRefreshMonitorManager::clientPreferredFramesPerSecondChanged(DisplayRefreshMonitorClient& client)
{
    if (auto* monitor = monitorForClient(client))
        monitor->clientPreferredFramesPerSecondChanged(client);
}

bool DisplayRefreshMonitorManager::scheduleAnimation(DisplayRefreshMonitorClient& client)
{
    if (auto* monitor = monitorForClient(client)) {
        client.setIsScheduled(true);
        return monitor->requestRefreshCallback();
    }
    return false;
}

void DisplayRefreshMonitorManager::displayDidRefresh(DisplayRefreshMonitor&)
{
    // Maybe we should remove monitors that haven't been active for some time.
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

std::optional<FramesPerSecond> DisplayRefreshMonitorManager::nominalFramesPerSecondForDisplay(PlatformDisplayID displayID, DisplayRefreshMonitorFactory* factory)
{
    auto* monitor = ensureMonitorForDisplayID(displayID, factory);
    if (monitor)
        monitor->displayNominalFramesPerSecond();

    return std::nullopt;
}

void DisplayRefreshMonitorManager::displayWasUpdated(PlatformDisplayID displayID, const DisplayUpdate& displayUpdate)
{
    auto* monitor = monitorForDisplayID(displayID);
    if (monitor)
        monitor->displayLinkFired(displayUpdate);
}

size_t DisplayRefreshMonitorManager::findMonitorForDisplayID(PlatformDisplayID displayID) const
{
    return m_monitors.findMatching([&](auto& monitorWrapper) {
        return monitorWrapper.monitor->displayID() == displayID;
    });
}

DisplayRefreshMonitor* DisplayRefreshMonitorManager::monitorForClient(DisplayRefreshMonitorClient& client)
{
    if (!client.hasDisplayID())
        return nullptr;

    auto* monitor = ensureMonitorForDisplayID(client.displayID(), client.displayRefreshMonitorFactory());
    if (monitor)
        monitor->addClient(client);

    return monitor;
}

DisplayRefreshMonitor* DisplayRefreshMonitorManager::monitorForDisplayID(PlatformDisplayID displayID) const
{
    auto index = findMonitorForDisplayID(displayID);
    return index == notFound ? nullptr : m_monitors[index].monitor.get();
}

}
