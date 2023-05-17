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

#include "LayerTreeContext.h"
#include "RemoteLayerBackingStoreCollection.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/FloatSize.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/LayerPool.h>
#include <WebCore/PlatformCALayer.h>
#include <wtf/Vector.h>

namespace WebKit {

class GraphicsLayerCARemote;
class PlatformCALayerRemote;
class RemoteRenderingBackendProxy;
class WebFrame;
class WebPage;

// FIXME: This class doesn't do much now. Roll into RemoteLayerTreeDrawingArea?
class RemoteLayerTreeContext : public WebCore::GraphicsLayerFactory {
public:
    explicit RemoteLayerTreeContext(WebPage&);
    ~RemoteLayerTreeContext();

    void layerDidEnterContext(PlatformCALayerRemote&, WebCore::PlatformCALayer::LayerType);
#if HAVE(AVKIT)
    void layerDidEnterContext(PlatformCALayerRemote&, WebCore::PlatformCALayer::LayerType, WebCore::HTMLVideoElement&);
#endif
    void layerWillLeaveContext(PlatformCALayerRemote&);

    void graphicsLayerDidEnterContext(GraphicsLayerCARemote&);
    void graphicsLayerWillLeaveContext(GraphicsLayerCARemote&);

    WebCore::LayerPool& layerPool() { return m_layerPool; }

    float deviceScaleFactor() const;

    LayerHostingMode layerHostingMode() const;
    
    std::optional<WebCore::DestinationColorSpace> displayColorSpace() const;

    DrawingAreaIdentifier drawingAreaIdentifier() const;

    void buildTransaction(RemoteLayerTreeTransaction&, WebCore::PlatformCALayer& rootLayer, WebCore::FrameIdentifier);

    void layerPropertyChangedWhileBuildingTransaction(PlatformCALayerRemote&);

    // From the UI process
    void animationDidStart(WebCore::PlatformLayerIdentifier, const String& key, MonotonicTime startTime);
    void animationDidEnd(WebCore::PlatformLayerIdentifier, const String& key);

    void willStartAnimationOnLayer(PlatformCALayerRemote&);

    RemoteLayerBackingStoreCollection& backingStoreCollection() { return *m_backingStoreCollection; }
    
    void setNextRenderingUpdateRequiresSynchronousImageDecoding() { m_nextRenderingUpdateRequiresSynchronousImageDecoding = true; }
    bool nextRenderingUpdateRequiresSynchronousImageDecoding() const { return m_nextRenderingUpdateRequiresSynchronousImageDecoding; }

    void adoptLayersFromContext(RemoteLayerTreeContext&);

    RemoteRenderingBackendProxy& ensureRemoteRenderingBackendProxy();

    bool useCGDisplayListsForDOMRendering() const { return m_useCGDisplayListsForDOMRendering; }
    void setUseCGDisplayListsForDOMRendering(bool useCGDisplayLists) { m_useCGDisplayListsForDOMRendering = useCGDisplayLists; }

    bool useCGDisplayListImageCache() const { return m_useCGDisplayListImageCache; }
    void setUseCGDisplayListImageCache(bool useCGDisplayListImageCache) { m_useCGDisplayListImageCache = useCGDisplayListImageCache; }
    
#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const;
#endif

    WebPage& webPage() { return m_webPage; }

private:
    // WebCore::GraphicsLayerFactory
    Ref<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayer::Type, WebCore::GraphicsLayerClient&) override;

    WebPage& m_webPage;

    HashMap<WebCore::PlatformLayerIdentifier, RemoteLayerTreeTransaction::LayerCreationProperties> m_createdLayers;
    Vector<WebCore::PlatformLayerIdentifier> m_destroyedLayers;

    HashMap<WebCore::PlatformLayerIdentifier, PlatformCALayerRemote*> m_livePlatformLayers;
    HashMap<WebCore::PlatformLayerIdentifier, PlatformCALayerRemote*> m_layersWithAnimations;
#if HAVE(AVKIT)
    HashMap<WebCore::PlatformLayerIdentifier, PlaybackSessionContextIdentifier> m_videoLayers;
#endif

    HashSet<GraphicsLayerCARemote*> m_liveGraphicsLayers;

    std::unique_ptr<RemoteLayerBackingStoreCollection> m_backingStoreCollection;

    WebCore::LayerPool m_layerPool;

    RemoteLayerTreeTransaction* m_currentTransaction { nullptr };

    bool m_nextRenderingUpdateRequiresSynchronousImageDecoding { false };
    bool m_useCGDisplayListsForDOMRendering { false };
    bool m_useCGDisplayListImageCache { false };
};

} // namespace WebKit
