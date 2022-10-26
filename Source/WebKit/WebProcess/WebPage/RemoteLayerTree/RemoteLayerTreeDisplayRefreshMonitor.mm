/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerTreeDisplayRefreshMonitor.h"

#import "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

constexpr FramesPerSecond DefaultPreferredFramesPerSecond = 60;

RemoteLayerTreeDisplayRefreshMonitor::RemoteLayerTreeDisplayRefreshMonitor(PlatformDisplayID displayID, RemoteLayerTreeDrawingArea& drawingArea)
    : DisplayRefreshMonitor(displayID)
    , m_drawingArea(drawingArea)
    , m_preferredFramesPerSecond(DefaultPreferredFramesPerSecond)
    , m_currentUpdate({ 0, m_preferredFramesPerSecond })
{
}

RemoteLayerTreeDisplayRefreshMonitor::~RemoteLayerTreeDisplayRefreshMonitor()
{
    if (m_drawingArea)
        m_drawingArea->willDestroyDisplayRefreshMonitor(this);
}

void RemoteLayerTreeDisplayRefreshMonitor::adjustPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    if (preferredFramesPerSecond == m_preferredFramesPerSecond)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "RemoteLayerTreeDisplayRefreshMonitor::adjustMaxPreferredFramesPerSecond to " << preferredFramesPerSecond);

    m_preferredFramesPerSecond = preferredFramesPerSecond;
    m_currentUpdate = { 0, m_preferredFramesPerSecond };

    if (m_drawingArea)
        m_drawingArea->setPreferredFramesPerSecond(preferredFramesPerSecond);
}

bool RemoteLayerTreeDisplayRefreshMonitor::requestRefreshCallback()
{
    if (!m_drawingArea)
        return false;

    Locker locker { lock() };

    if (isScheduled())
        return true;

    LOG_WITH_STREAM(DisplayLink, stream << "[Web] RemoteLayerTreeDisplayRefreshMonitor::requestRefreshCallback - triggering update");
    static_cast<DrawingArea&>(*m_drawingArea.get()).triggerRenderingUpdate();

    setIsScheduled(true);
    return true;
}

void RemoteLayerTreeDisplayRefreshMonitor::triggerDisplayDidRefresh()
{
    {
        Locker locker { lock() };
        setIsScheduled(false);

        if (!isPreviousFrameDone())
            return;

        setIsPreviousFrameDone(false);
    }
    displayDidRefresh(m_currentUpdate);
    m_currentUpdate = m_currentUpdate.nextUpdate();
}

void RemoteLayerTreeDisplayRefreshMonitor::updateDrawingArea(RemoteLayerTreeDrawingArea& drawingArea)
{
    m_drawingArea = drawingArea;
}

std::optional<FramesPerSecond> RemoteLayerTreeDisplayRefreshMonitor::displayNominalFramesPerSecond()
{
    return m_preferredFramesPerSecond;
}

} // namespace WebKit
