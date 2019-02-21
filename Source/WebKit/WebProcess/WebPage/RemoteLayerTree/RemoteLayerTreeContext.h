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

#include "LayerTreeContext.h"
#include "RemoteLayerBackingStoreCollection.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/LayerPool.h>
#include <WebCore/PlatformCALayer.h>
#include <wtf/Vector.h>

namespace WebKit {

class GraphicsLayerCARemote;
class PlatformCALayerRemote;
class WebPage;

// FIXME: This class doesn't do much now. Roll into RemoteLayerTreeDrawingArea?
class RemoteLayerTreeContext : public WebCore::GraphicsLayerFactory {
public:
    explicit RemoteLayerTreeContext(WebPage&);
    ~RemoteLayerTreeContext();

    void layerDidEnterContext(PlatformCALayerRemote&, WebCore::PlatformCALayer::LayerType);
    void layerWillLeaveContext(PlatformCALayerRemote&);

    void graphicsLayerDidEnterContext(GraphicsLayerCARemote&);
    void graphicsLayerWillLeaveContext(GraphicsLayerCARemote&);

    void backingStoreWasCreated(RemoteLayerBackingStore&);
    void backingStoreWillBeDestroyed(RemoteLayerBackingStore&);
    bool backingStoreWillBeDisplayed(RemoteLayerBackingStore&);

    WebCore::LayerPool& layerPool() { return m_layerPool; }

    float deviceScaleFactor() const;

    LayerHostingMode layerHostingMode() const;

    void buildTransaction(RemoteLayerTreeTransaction&, WebCore::PlatformCALayer& rootLayer);

    void layerPropertyChangedWhileBuildingTransaction(PlatformCALayerRemote&);

    // From the UI process
    void animationDidStart(WebCore::GraphicsLayer::PlatformLayerID, const String& key, MonotonicTime startTime);
    void animationDidEnd(WebCore::GraphicsLayer::PlatformLayerID, const String& key);

    void willStartAnimationOnLayer(PlatformCALayerRemote&);

    RemoteLayerBackingStoreCollection& backingStoreCollection() { return m_backingStoreCollection; }
    
    void setNextFlushIsForImmediatePaint(bool nextFlushIsForImmediatePaint) { m_nextFlushIsForImmediatePaint = nextFlushIsForImmediatePaint; }
    bool nextFlushIsForImmediatePaint() const { return m_nextFlushIsForImmediatePaint; }

    void adoptLayersFromContext(RemoteLayerTreeContext&);

private:
    // WebCore::GraphicsLayerFactory
    Ref<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayer::Type, WebCore::GraphicsLayerClient&) override;

    WebPage& m_webPage;

    HashMap<WebCore::GraphicsLayer::PlatformLayerID, RemoteLayerTreeTransaction::LayerCreationProperties> m_createdLayers;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> m_destroyedLayers;

    HashMap<WebCore::GraphicsLayer::PlatformLayerID, PlatformCALayerRemote*> m_livePlatformLayers;
    HashMap<WebCore::GraphicsLayer::PlatformLayerID, PlatformCALayerRemote*> m_layersWithAnimations;

    HashSet<GraphicsLayerCARemote*> m_liveGraphicsLayers;

    RemoteLayerBackingStoreCollection m_backingStoreCollection;
    
    RemoteLayerTreeTransaction* m_currentTransaction;

    WebCore::LayerPool m_layerPool;
    
    bool m_nextFlushIsForImmediatePaint { false };
};

} // namespace WebKit
