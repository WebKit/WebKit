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

#pragma once

#include "RemoteLayerTreeNode.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CAAnimation;
OBJC_CLASS WKAnimationDelegate;

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;
class WebPageProxy;

class RemoteLayerTreeHost {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteLayerTreeHost(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeHost();

    RemoteLayerTreeNode* nodeForID(WebCore::GraphicsLayer::PlatformLayerID) const;
    RemoteLayerTreeNode* rootNode() const { return m_rootNode.get(); }

    CALayer *layerForID(WebCore::GraphicsLayer::PlatformLayerID) const;
    CALayer *rootLayer() const;

    RemoteLayerTreeDrawingAreaProxy& drawingArea() const { return *m_drawingArea; }

    // Returns true if the root layer changed.
    bool updateLayerTree(const RemoteLayerTreeTransaction&, float indicatorScaleFactor  = 1);
    void asyncSetLayerContents(WebCore::GraphicsLayer::PlatformLayerID, ImageBufferBackendHandle&&);

    void setIsDebugLayerTreeHost(bool flag) { m_isDebugLayerTreeHost = flag; }
    bool isDebugLayerTreeHost() const { return m_isDebugLayerTreeHost; }

    typedef HashMap<WebCore::GraphicsLayer::PlatformLayerID, RetainPtr<WKAnimationDelegate>> LayerAnimationDelegateMap;
    LayerAnimationDelegateMap& animationDelegates() { return m_animationDelegates; }

    void animationDidStart(WebCore::GraphicsLayer::PlatformLayerID, CAAnimation *, MonotonicTime startTime);
    void animationDidEnd(WebCore::GraphicsLayer::PlatformLayerID, CAAnimation *);

    void detachFromDrawingArea();
    void clearLayers();

    // Detach the root layer; it will be reattached upon the next incoming commit.
    void detachRootLayer();

    // Turn all CAMachPort objects in layer contents into actual IOSurfaces.
    // This avoids keeping an outstanding InUse reference when suspended.
    void mapAllIOSurfaceBackingStore();

    CALayer *layerWithIDForTesting(uint64_t) const;

    bool replayCGDisplayListsIntoBackingStore() const;
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    const HashSet<WebCore::GraphicsLayer::PlatformLayerID>& overlayRegionIDs() const { return m_overlayRegionIDs; }
    void updateOverlayRegionIDs(const HashSet<WebCore::GraphicsLayer::PlatformLayerID> &overlayRegionNodes) { m_overlayRegionIDs = overlayRegionNodes; }
#endif

private:
    void createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties&);
    std::unique_ptr<RemoteLayerTreeNode> makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties&);

    void layerWillBeRemoved(WebCore::GraphicsLayer::PlatformLayerID);

    RemoteLayerBackingStore::LayerContentsType layerContentsType() const;

    RemoteLayerTreeDrawingAreaProxy* m_drawingArea { nullptr };
    WeakPtr<RemoteLayerTreeNode> m_rootNode;
    HashMap<WebCore::GraphicsLayer::PlatformLayerID, std::unique_ptr<RemoteLayerTreeNode>> m_nodes;
    HashMap<WebCore::GraphicsLayer::PlatformLayerID, RetainPtr<WKAnimationDelegate>> m_animationDelegates;
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> m_overlayRegionIDs;
#endif
    bool m_isDebugLayerTreeHost { false };
};

} // namespace WebKit
