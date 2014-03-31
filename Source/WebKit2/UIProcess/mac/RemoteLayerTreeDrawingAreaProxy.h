/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef RemoteLayerTreeDrawingAreaProxy_h
#define RemoteLayerTreeDrawingAreaProxy_h

#include "DrawingAreaProxy.h"
#include "RemoteLayerTreeHost.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>

namespace WebKit {

class RemoteLayerTreeTransaction;
class RemoteScrollingCoordinatorTransaction;

class RemoteLayerTreeDrawingAreaProxy : public DrawingAreaProxy {
public:
    explicit RemoteLayerTreeDrawingAreaProxy(WebPageProxy*);
    virtual ~RemoteLayerTreeDrawingAreaProxy();

    const RemoteLayerTreeHost& remoteLayerTreeHost() const { return m_remoteLayerTreeHost; }

    void coreAnimationDidCommitLayers();

private:
    virtual void sizeDidChange() override;
    virtual void deviceScaleFactorDidChange() override;
    virtual void didUpdateGeometry() override;

    WebCore::FloatRect scaledExposedRect() const;
    void showDebugIndicator(bool);

#if PLATFORM(MAC)
    virtual void setExposedRect(const WebCore::FloatRect&) override;
#endif

    float indicatorScale(WebCore::IntSize contentsSize) const;
    virtual void updateDebugIndicator() override;
    void updateDebugIndicator(WebCore::IntSize contentsSize, bool rootLayerChanged, float scale);
    void updateDebugIndicatorPosition();
    
    WebCore::FloatPoint indicatorLocation() const;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    // Message handlers
    void commitLayerTree(const RemoteLayerTreeTransaction&, const RemoteScrollingCoordinatorTransaction&);
    
    void sendUpdateGeometry();

    void scheduleCoreAnimationLayerCommitObserver();

    RemoteLayerTreeHost m_remoteLayerTreeHost;
    bool m_isWaitingForDidUpdateGeometry;

    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentLayerPosition;

    std::unique_ptr<RemoteLayerTreeHost> m_debugIndicatorLayerTreeHost;
    RetainPtr<CALayer> m_tileMapHostLayer;
    RetainPtr<CALayer> m_exposedRectIndicatorLayer;

    RetainPtr<CFRunLoopObserverRef> m_layerCommitObserver;
};

DRAWING_AREA_PROXY_TYPE_CASTS(RemoteLayerTreeDrawingAreaProxy, type() == DrawingAreaTypeRemoteLayerTree);

} // namespace WebKit

#endif // RemoteLayerTreeDrawingAreaProxy_h
