/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include "PlaybackSessionContextIdentifier.h"
#include "RemoteLayerTreeNode.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS CAAnimation;
OBJC_CLASS WKAnimationDelegate;

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxy;
class WebPageProxy;

class RemoteLayerTreeHost {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeHost);
public:
    explicit RemoteLayerTreeHost(RemoteLayerTreeDrawingAreaProxy&);
    ~RemoteLayerTreeHost();

    RemoteLayerTreeNode* nodeForID(WebCore::PlatformLayerIdentifier) const;
    RemoteLayerTreeNode* rootNode() const { return m_rootNode.get(); }

    CALayer *layerForID(WebCore::PlatformLayerIdentifier) const;
    CALayer *rootLayer() const;

    RemoteLayerTreeDrawingAreaProxy& drawingArea() const;

    // Returns true if the root layer changed.
    bool updateLayerTree(const IPC::Connection&, const RemoteLayerTreeTransaction&, float indicatorScaleFactor  = 1);
    void asyncSetLayerContents(WebCore::PlatformLayerIdentifier, ImageBufferBackendHandle&&, const WebCore::RenderingResourceIdentifier&);

    void setIsDebugLayerTreeHost(bool flag) { m_isDebugLayerTreeHost = flag; }
    bool isDebugLayerTreeHost() const { return m_isDebugLayerTreeHost; }

    typedef HashMap<WebCore::PlatformLayerIdentifier, RetainPtr<WKAnimationDelegate>> LayerAnimationDelegateMap;
    LayerAnimationDelegateMap& animationDelegates() { return m_animationDelegates; }

    void animationDidStart(WebCore::PlatformLayerIdentifier, CAAnimation *, MonotonicTime startTime);
    void animationDidEnd(WebCore::PlatformLayerIdentifier, CAAnimation *);

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    void animationsWereAddedToNode(RemoteLayerTreeNode&);
    void animationsWereRemovedFromNode(RemoteLayerTreeNode&);
#endif

    void detachFromDrawingArea();
    void clearLayers();

    // Detach the root layer; it will be reattached upon the next incoming commit.
    void detachRootLayer();

    // Turn all CAMachPort objects in layer contents into actual IOSurfaces.
    // This avoids keeping an outstanding InUse reference when suspended.
    void mapAllIOSurfaceBackingStore();

    CALayer *layerWithIDForTesting(WebCore::PlatformLayerIdentifier) const;

    bool replayDynamicContentScalingDisplayListsIntoBackingStore() const;
    bool threadedAnimationResolutionEnabled() const;

    bool cssUnprefixedBackdropFilterEnabled() const;

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    Seconds acceleratedTimelineTimeOrigin() const;
    MonotonicTime animationCurrentTime() const;
#endif

    void remotePageProcessDidTerminate(WebCore::ProcessIdentifier);

private:
    void createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties&);
    std::unique_ptr<RemoteLayerTreeNode> makeNode(const RemoteLayerTreeTransaction::LayerCreationProperties&);

    bool updateBannerLayers(const RemoteLayerTreeTransaction&);

    void layerWillBeRemoved(WebCore::ProcessIdentifier, WebCore::PlatformLayerIdentifier);

    LayerContentsType layerContentsType() const;

    WeakPtr<RemoteLayerTreeDrawingAreaProxy> m_drawingArea;
    WeakPtr<RemoteLayerTreeNode> m_rootNode;
    HashMap<WebCore::PlatformLayerIdentifier, std::unique_ptr<RemoteLayerTreeNode>> m_nodes;
    HashMap<WebCore::LayerHostingContextIdentifier, WebCore::PlatformLayerIdentifier> m_hostingLayers;
    HashMap<WebCore::LayerHostingContextIdentifier, WebCore::PlatformLayerIdentifier> m_hostedLayers;
    HashMap<WebCore::ProcessIdentifier, HashSet<WebCore::PlatformLayerIdentifier>> m_hostedLayersInProcess;
    HashMap<WebCore::PlatformLayerIdentifier, RetainPtr<WKAnimationDelegate>> m_animationDelegates;
#if HAVE(AVKIT)
    HashMap<WebCore::PlatformLayerIdentifier, PlaybackSessionContextIdentifier> m_videoLayers;
#endif
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    HashSet<WebCore::PlatformLayerIdentifier> m_overlayRegionIDs;
#endif
    bool m_isDebugLayerTreeHost { false };
};

} // namespace WebKit
