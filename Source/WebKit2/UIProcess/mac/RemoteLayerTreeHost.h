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

#ifndef RemoteLayerTreeHost_h
#define RemoteLayerTreeHost_h

#include "LayerRepresentation.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS WKAnimationDelegate;

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;
class WebPageProxy;

class RemoteLayerTreeHost {
public:
    explicit RemoteLayerTreeHost(RemoteLayerTreeDrawingAreaProxy&);
    virtual ~RemoteLayerTreeHost();

    LayerOrView *getLayer(WebCore::GraphicsLayer::PlatformLayerID) const;
    LayerOrView *rootLayer() const { return m_rootLayer; }

    static WebCore::GraphicsLayer::PlatformLayerID layerID(LayerOrView*);

    // Returns true if the root layer changed.
    bool updateLayerTree(const RemoteLayerTreeTransaction&, float indicatorScaleFactor  = 1);

    void setIsDebugLayerTreeHost(bool flag) { m_isDebugLayerTreeHost = flag; }
    bool isDebugLayerTreeHost() const { return m_isDebugLayerTreeHost; }

    typedef HashMap<WebCore::GraphicsLayer::PlatformLayerID, RetainPtr<WKAnimationDelegate>> LayerAnimationDelegateMap;
    LayerAnimationDelegateMap& animationDelegates() { return m_animationDelegates; }

    void animationDidStart(WebCore::GraphicsLayer::PlatformLayerID, double startTime);

private:
    LayerOrView *createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties&, const RemoteLayerTreeTransaction::LayerProperties*);

    void layerWillBeRemoved(WebCore::GraphicsLayer::PlatformLayerID);

    RemoteLayerTreeDrawingAreaProxy& m_drawingArea;
    LayerOrView *m_rootLayer;
    HashMap<WebCore::GraphicsLayer::PlatformLayerID, RetainPtr<LayerOrView>> m_layers;
    HashMap<WebCore::GraphicsLayer::PlatformLayerID, RetainPtr<WKAnimationDelegate>> m_animationDelegates;
    bool m_isDebugLayerTreeHost;
};

} // namespace WebKit

#endif // RemoteLayerTreeHost_h
