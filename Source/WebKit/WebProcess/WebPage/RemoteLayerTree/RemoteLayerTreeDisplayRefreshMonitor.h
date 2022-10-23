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

#pragma once

#include "RemoteLayerTreeDrawingArea.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/DisplayRefreshMonitor.h>

namespace WebKit {

class RemoteLayerTreeDisplayRefreshMonitor : public WebCore::DisplayRefreshMonitor {
friend class RemoteLayerTreeDrawingArea;
public:
    static Ref<RemoteLayerTreeDisplayRefreshMonitor> create(WebCore::PlatformDisplayID displayID, RemoteLayerTreeDrawingArea& drawingArea)
    {
        return adoptRef(*new RemoteLayerTreeDisplayRefreshMonitor(displayID, drawingArea));
    }
    
    virtual ~RemoteLayerTreeDisplayRefreshMonitor();

    bool requestRefreshCallback() final;

    void updateDrawingArea(RemoteLayerTreeDrawingArea&);

private:
    explicit RemoteLayerTreeDisplayRefreshMonitor(WebCore::PlatformDisplayID, RemoteLayerTreeDrawingArea&);

    bool startNotificationMechanism() final { return true; }
    void stopNotificationMechanism() final { }
    std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() final;

    void triggerDisplayDidRefresh();

    void adjustPreferredFramesPerSecond(WebCore::FramesPerSecond) final;

    WeakPtr<RemoteLayerTreeDrawingArea> m_drawingArea;

    WebCore::FramesPerSecond m_preferredFramesPerSecond;
    WebCore::DisplayUpdate m_currentUpdate;
};

} // namespace WebKit
