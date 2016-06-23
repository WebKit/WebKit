/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "GraphicsLayerUpdater.h"

#include "DisplayRefreshMonitorManager.h"
#include "GraphicsLayer.h"

namespace WebCore {

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
GraphicsLayerUpdater::GraphicsLayerUpdater(GraphicsLayerUpdaterClient& client, PlatformDisplayID displayID)
    : m_client(client)
{
    DisplayRefreshMonitorManager::sharedManager().registerClient(*this);
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
    DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
}
#else
GraphicsLayerUpdater::GraphicsLayerUpdater(GraphicsLayerUpdaterClient&, PlatformDisplayID)
{
}
#endif

GraphicsLayerUpdater::~GraphicsLayerUpdater()
{
    // ~DisplayRefreshMonitorClient unregisters us as a client.
}

void GraphicsLayerUpdater::scheduleUpdate()
{
    if (m_scheduled)
        return;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
#endif
    m_scheduled = true;
}

void GraphicsLayerUpdater::screenDidChange(PlatformDisplayID displayID)
{
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
#else
    UNUSED_PARAM(displayID);
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
void GraphicsLayerUpdater::displayRefreshFired()
{
    m_scheduled = false;
    m_client.flushLayersSoon(*this);
}
#endif

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> GraphicsLayerUpdater::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    return m_client.createDisplayRefreshMonitor(displayID);
}
#endif

} // namespace WebCore
